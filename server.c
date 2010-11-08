/*
 *  Copyright (C) 2004 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include <sys/types.h>

#include <unistd.h>

#ifdef WIN32
#define geterror() WSAGetLastError()
#else
#define geterror() errno
#endif

#include "lop_types_internal.h"
#include "lop_internal.h"
#include "lop/lop_throw.h"

#define LOP_HOST_SIZE 1024

typedef struct {
    lop_timetag ts;
    char *path;
    lop_message msg;
    void *next;
} queued_msg_list;

struct lop_cs lop_client_sockets = {-1, -1};

static int lop_can_coerce_spec(const char *a, const char *b);
static int lop_can_coerce(char a, char b);
static void dispatch_method(lop_server s, const char *path,
    lop_message msg);
static int dispatch_queued(lop_server s);
static void queue_data(lop_server s, lop_timetag ts, const char *path,
    lop_message msg);
static lop_server lop_server_new_with_proto_internal(const char *group,
                                                   const char *port, int proto,
                                                   lop_err_handler err_h);
static int lop_server_add_socket(lop_server s, int socket);
static void lop_server_del_socket(lop_server s, int index, int socket);
static int lop_server_join_multicast_group(lop_server s, const char *group);

#ifdef WIN32
#ifndef gai_strerror
// Copied from the Win32 SDK 

// WARNING: The gai_strerror inline functions below use static buffers,
// and hence are not thread-safe.  We'll use buffers long enough to hold
// 1k characters.  Any system error messages longer than this will be
// returned as empty strings.  However 1k should work for the error codes
// used by getaddrinfo().
#define GAI_STRERROR_BUFFER_SIZE 1024

char *WSAAPI gai_strerrorA(int ecode)
{
    DWORD dwMsgLen;
    static char buff[GAI_STRERROR_BUFFER_SIZE + 1];

    dwMsgLen = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM
                             |FORMAT_MESSAGE_IGNORE_INSERTS
                             |FORMAT_MESSAGE_MAX_WIDTH_MASK,
                              NULL,
                              ecode,
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPSTR)buff,
                              GAI_STRERROR_BUFFER_SIZE,
                              NULL);
    return buff;
}
#endif

static int stateWSock = -1;

int initWSock()
{
    WORD reqversion;
    WSADATA wsaData;
    if(stateWSock >= 0) return stateWSock;
    /* TODO - which version of Winsock do we actually need? */

    reqversion = MAKEWORD( 2, 2 );
    if(WSAStartup(reqversion,&wsaData) != 0) {
        /* Couldn't initialize Winsock */
        stateWSock = 0;
    }
    else if ( LOBYTE( wsaData.wVersion ) != LOBYTE(reqversion) ||
            HIBYTE( wsaData.wVersion ) != HIBYTE(reqversion) ) {
        /* wrong version */
        WSACleanup();
        stateWSock = 0; 
    }
    else
        stateWSock = 1;

    return stateWSock;
}
#endif

lop_server lop_server_new(const char *port, lop_err_handler err_h)
{
	return lop_server_new_with_proto(port, LOP_DEFAULT, err_h);
}

lop_server lop_server_new_multicast(const char *group, const char *port,
                                  lop_err_handler err_h)
{
    return lop_server_new_with_proto_internal(group, port, LOP_UDP, err_h);
}

lop_server lop_server_new_with_proto(const char *port, int proto,
				   lop_err_handler err_h)
{
    return lop_server_new_with_proto_internal(NULL, port, proto, err_h);
}

lop_server lop_server_new_with_proto_internal(const char *group,
                                            const char *port, int proto,
                                            lop_err_handler err_h)
{
    lop_server s;
    struct addrinfo *ai = NULL, *it, *used;
    struct addrinfo hints;
    int ret = -1;
    int tries = 0;
    char pnum[16];
    const char *service;
    char hostname[LOP_HOST_SIZE];

	// Set real protocol, if Default is requested
	if (proto==LOP_DEFAULT) {
#ifndef WIN32
		if (port && *port == '/') proto = LOP_UNIX;
		else 
#endif
		proto = LOP_UDP;
	}


#ifdef WIN32
    if(!initWSock()) return NULL;
#endif
    
    s = calloc(1, sizeof(struct _lop_server));
    if (!s) return 0;

    s->err_h = err_h;
    s->first = NULL;
    s->ai = NULL;
    s->hostname = NULL;
    s->protocol = proto;
    s->port = 0;
    s->path = NULL;
    s->queued = NULL;
    s->sockets_len = 1;
    s->sockets_alloc = 2;
    s->sockets = calloc(2, sizeof(*(s->sockets)));

    if (!s->sockets) {
        free(s);
        return 0;
    }

    s->sockets[0].fd = -1;

    memset(&hints, 0, sizeof(hints));

    if (proto == LOP_UDP) {
	hints.ai_socktype = SOCK_DGRAM;
    } else if (proto == LOP_TCP) {
	hints.ai_socktype = SOCK_STREAM;
    } 
#ifndef WIN32
    else if (proto == LOP_UNIX) {

	struct sockaddr_un sa;

	s->sockets[0].fd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s->sockets[0].fd == -1) {
        int err = geterror();
	    used = NULL;
	    lop_throw(s, err, strerror(err), "socket()");
	    lop_server_free(s);

	    return NULL;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, port, sizeof(sa.sun_path)-1);

	if ((ret = bind(s->sockets[0].fd,
                    (struct sockaddr *)&sa, sizeof(sa))) < 0) {
        int err = geterror();      
	    lop_throw(s, err, strerror(err), "bind()");

	    lop_server_free(s);
	    return NULL;
	}
	
	s->path = strdup(port);

	return s;
    } 
#endif
    else {
	lop_throw(s, LOP_UNKNOWNPROTO, "Unknown protocol", NULL);
	lop_server_free(s);

	return NULL;
    }

#ifdef ENABLE_IPV6
    hints.ai_family = PF_UNSPEC;
#else
    hints.ai_family = PF_INET;
#endif
    hints.ai_flags = AI_PASSIVE;

    if (!port) {
	service = pnum;
    } else {
	service = port;
    }
    do {
	if (!port) {
	    /* not a good way to get random numbers, but its not critical */
	    snprintf(pnum, 15, "%ld", 10000 + ((unsigned int)rand() +
		     time(NULL)) % 10000);
	}

	if ((ret = getaddrinfo(NULL, service, &hints, &ai))) {
	    lop_throw(s, ret, gai_strerror(ret), NULL);
	    freeaddrinfo(ai);

	    return NULL;
	}

	used = NULL;
	s->ai = ai;
	s->sockets[0].fd = -1;
	s->port = 0;

	for (it = ai; it && s->sockets[0].fd == -1; it = it->ai_next) {
	    used = it;
	    s->sockets[0].fd = socket(it->ai_family, hints.ai_socktype, 0);
	}
	if (s->sockets[0].fd == -1) {
        int err = geterror();
	    used = NULL;
	    lop_throw(s, err, strerror(err), "socket()");

	    lop_server_free(s);
	    return NULL;
	}

    /* Join multicast group if specified. */
    /* This must be done before bind() on POSIX, but after bind() Windows. */
#ifndef WIN32
    if (group != NULL)
        if (lop_server_join_multicast_group(s, group))
            return NULL;
#endif

	if ((ret = bind(s->sockets[0].fd, used->ai_addr, used->ai_addrlen)) < 0) {
        int err = geterror();
	    if (err == EINVAL || err == EADDRINUSE) {
		used = NULL;

		continue;
	    }
	    lop_throw(s, err, strerror(err), "bind()");

	    lop_server_free(s);

	    return NULL;
	}
    } while (!used && tries++ < 16);

    /* Join multicast group if specified (see above). */
#ifdef WIN32
    if (group != NULL)
        if (lop_server_join_multicast_group(s, group))
            return NULL;
#endif

    if (proto == LOP_TCP) {
        listen(s->sockets[0].fd, 8);
    }

    if (!used) {
	lop_throw(s, LOP_NOPORT, "cannot find free port", NULL);

	lop_server_free(s);
	return NULL;
    }

    if (proto == LOP_UDP) {
        lop_client_sockets.udp = s->sockets[0].fd;
    } else if (proto == LOP_TCP) {
        lop_client_sockets.tcp = s->sockets[0].fd;
    }

	/* Set hostname to empty string */
    hostname[0] = '\0';
	
#ifdef ENABLE_IPV6
    /* Try it the IPV6 friendly way first */
	for (it = ai; it; it = it->ai_next) {
	if (getnameinfo(it->ai_addr, it->ai_addrlen, hostname,
			sizeof(hostname), NULL, 0, NI_NAMEREQD) == 0) {
	    break;
	}
    }

    /* check to make sure getnameinfo() didn't just set the hostname to "::".
       Needed on Darwin. */
    if (hostname[0] == ':') {
	hostname[0] = '\0';
    }
#endif


    /* Fallback to the oldschool (i.e. more reliable) way */
    if (!hostname[0]) {
	struct hostent *he;

	gethostname(hostname, sizeof(hostname));
	he = gethostbyname(hostname);
	if (he) {
	    strncpy(hostname, he->h_name, sizeof(hostname));
	}
    }

    /* soethings gone really wrong, just hope its local only */
    if (!hostname[0]) {
	strcpy(hostname, "localhost");
    }
    s->hostname = strdup(hostname);

    if (used->ai_family == PF_INET6) {
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)used->ai_addr;

	s->port = htons(addr->sin6_port);
    } else if (used->ai_family == PF_INET) {
	struct sockaddr_in *addr = (struct sockaddr_in *)used->ai_addr;

	s->port = htons(addr->sin_port);
    } else {
	lop_throw(s, LOP_UNKNOWNPROTO, "unknown protocol family", NULL);
	s->port = atoi(port);
    }

    return s;
}

int lop_server_join_multicast_group(lop_server s, const char *group)
{
    struct ip_mreq mreq;
    unsigned int yes = 1;
    memset(&mreq, 0, sizeof(mreq));
#ifdef HAVE_INET_ATON
    if (inet_aton(group, &mreq.imr_multiaddr)==0) {
        int err = geterror();
        lop_throw(s, err, strerror(err), "inet_aton()");
        lop_server_free(s);
        return err;
    }
#else
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    if (mreq.imr_multiaddr.s_addr == INADDR_ANY
        || mreq.imr_multiaddr.s_addr == INADDR_NONE)
    {
        int err = geterror();
        lop_throw(s, err, strerror(err), "inet_addr()");
        lop_server_free(s);
        return err;
    }
#endif
    mreq.imr_interface.s_addr=htonl(INADDR_ANY);

    if (setsockopt(s->sockets[0].fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,
            &mreq,sizeof(mreq)) < 0)
    {
        int err = geterror();
        lop_throw(s, err, strerror(err), "setsockopt(IP_ADD_MEMBERSHIP)");
        lop_server_free(s);
        return err;
    }

    if (setsockopt(s->sockets[0].fd,SOL_SOCKET,SO_REUSEADDR,
            &yes,sizeof(yes)) < 0)
    {
        int err = geterror();
        lop_throw(s, err, strerror(err), "setsockopt(SO_REUSEADDR)");
        lop_server_free(s);
        return err;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(s->sockets[0].fd,SOL_SOCKET,SO_REUSEPORT,
            &yes,sizeof(yes)) < 0)
    {
        int err = geterror();
        lop_throw(s, err, strerror(err), "setsockopt(SO_REUSEPORT)");
        lop_server_free(s);
        return err;
    }
#endif

    return 0;
}

void lop_server_free(lop_server s)
{
    if (s) {
	lop_method it;
	lop_method next;
    int i;

    for (i=s->sockets_len-1; i >= 0; i--)
    {
        if (s->sockets[i].fd != -1) {
            if (s->protocol == LOP_UDP
                && s->sockets[i].fd == lop_client_sockets.udp)
            {
                lop_client_sockets.udp = -1;
            }
            else if (s->protocol == LOP_TCP
                     && s->sockets[0].fd == lop_client_sockets.tcp)
            {
                lop_client_sockets.tcp = -1;
            }

            close(s->sockets[i].fd);
            s->sockets[i].fd = -1;
        }
    }
	if (s->ai) {
		freeaddrinfo(s->ai);
		s->ai=NULL;
	}
	if (s->hostname) {
		free(s->hostname);
		s->hostname = NULL;
	}
	if (s->path) {
		if (s->protocol == LOP_UNIX) unlink( s->path );
		free(s->path);
		s->path = NULL;
	}
	for (it = s->first; it; it = next) {
	    next = it->next;
	    free((char *)it->path);
	    free((char *)it->typespec);
	    free(it);
	}
	free(s->sockets);
	free(s);
    }
}

void *lop_server_recv_raw(lop_server s, size_t *size)
{
    char buffer[LOP_MAX_MSG_SIZE];
    int ret;
    void *data = NULL;

#ifdef WIN32
    if(!initWSock()) return NULL;
#endif

    s->addr_len = sizeof(s->addr);

    ret = recvfrom(s->sockets[0].fd, buffer, LOP_MAX_MSG_SIZE, 0,
		   (struct sockaddr *)&s->addr, &s->addr_len);
    if (ret <= 0) {
	return NULL;
    }
    data = malloc(ret);
    memcpy(data, buffer, ret);

    if (size) *size = ret;

    return data;
}

void *lop_server_recv_raw_stream(lop_server s, size_t *size)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    char buffer[LOP_MAX_MSG_SIZE];
    int32_t read_size;
    int ret=0, i;
    void *data = NULL;
    int sock = -1;
    int repeat = 1;
#ifdef HAVE_SELECT
#ifndef HAVE_POLL
    fd_set ps;
    int nfds=0;
#endif
#endif

    /* check sockets in reverse order so that already-open sockets
     * have priority.  this allows checking for closed sockets even
     * when new connections are being requested.  it also allows to
     * continue looping through the list of sockets after closing and
     * deleting a socket, since deleting sockets doesn't affect the
     * order of the array to the left of the index. */

#ifdef HAVE_POLL
    for (i=0; i < s->sockets_len; i++) {
        s->sockets[i].events = POLLIN | POLLPRI;
        s->sockets[i].revents = 0;
    }

    poll(s->sockets, s->sockets_len, -1);

    for (i=(s->sockets_len-1); i >= 0; --i) {
        if (s->sockets[i].revents == POLLERR
            || s->sockets[i].revents == POLLHUP)
        {
            if (i>0) {
                close(s->sockets[i].fd);
                lop_server_del_socket(s, i, s->sockets[i].fd);
                continue;
            }
            else
                return NULL;
        }
        if (s->sockets[i].revents) {
            sock = s->sockets[i].fd;

#else
#ifdef HAVE_SELECT
    if(!initWSock()) return NULL;

    FD_ZERO(&ps);
    for (i=(s->sockets_len-1); i >= 0; --i) {
        FD_SET(s->sockets[i].fd, &ps);
        if (s->sockets[i].fd > nfds)
            nfds = s->sockets[i].fd;
    }

    if (select(nfds+1,&ps,NULL,NULL,NULL) == SOCKET_ERROR)
        return NULL;

    for (i=0; i < s->sockets_len; i++) {
        if (FD_ISSET(s->sockets[i].fd, &ps)) {
            sock = s->sockets[i].fd;

#endif
#endif

    if (sock == -1 || !repeat)
        return NULL;

    /* zeroeth socket is listening for new connections */
    if (sock == s->sockets[0].fd) {
        sock = accept(sock, (struct sockaddr *)&addr, &addr_len);
        i = lop_server_add_socket(s, sock);

        /* only repeat this loop for sockets other than the listening
         * socket,  (otherwise i will be wrong next time around) */
        repeat = 0;
    }

    if (i<0) {
        close(sock);
        return NULL;
    }

    ret = recv(sock, &read_size, sizeof(read_size), 0);
    read_size = ntohl(read_size);
    if (read_size > LOP_MAX_MSG_SIZE || ret <= 0) {
        close(sock);
        lop_server_del_socket(s, i, sock);
        if (ret > 0)
            lop_throw(s, LOP_TOOBIG, "Message too large", "recv()");
        continue;
    }
    ret = recv(sock, buffer, read_size, 0);
    if (ret <= 0) {
        close(sock);
        lop_server_del_socket(s, i, sock);
        continue;
    }
 
    /* end of loop over sockets: successfully read data */
    break;
        }
    }

    data = malloc(ret);
    memcpy(data, buffer, ret);

    if (size) *size = ret;

    return data;
}

int lop_server_wait(lop_server s, int timeout)
{
    int sched_timeout = lop_server_next_event_delay(s) * 1000;
    int i;
#ifdef HAVE_SELECT
#ifndef HAVE_POLL
    fd_set ps;
    struct timeval stimeout;
#endif
#endif

#ifdef HAVE_POLL
    for (i=0; i < s->sockets_len; i++) {
        s->sockets[i].events = POLLIN | POLLPRI | POLLERR | POLLHUP;
        s->sockets[i].revents = 0;
    }

    poll(s->sockets, s->sockets_len,
         timeout > sched_timeout ? sched_timeout : timeout);

    if (lop_server_next_event_delay(s) < 0.01)
        return 1;

    for (i=0; i < s->sockets_len; i++) {
        if (s->sockets[i].revents == POLLERR
            || s->sockets[i].revents == POLLHUP)
            return 0;
        if (s->sockets[i].revents)
            return 1;
    }
#else
#ifdef HAVE_SELECT
    int res,to,nfds=0;

    if(!initWSock()) return 0;

    to = timeout > sched_timeout ? sched_timeout : timeout;
    stimeout.tv_sec = to/1000;
    stimeout.tv_usec = (to%1000)*1000;

    FD_ZERO(&ps);
    for (i=0; i < s->sockets_len; i++) {
        FD_SET(s->sockets[i].fd,&ps);
        if (s->sockets[i].fd > nfds)
            nfds = s->sockets[i].fd;
    }

    res = select(nfds+1,&ps,NULL,NULL,&stimeout);

    if(res == SOCKET_ERROR)
        return 0;

    if (res || lop_server_next_event_delay(s) < 0.01)
	    return 1;
#endif
#endif

    return 0;
}

int lop_server_recv_noblock(lop_server s, int timeout)
{
    int result = lop_server_wait(s,timeout);
    if (result>0) {
      return lop_server_recv(s);
    } else {
      return 0;
    }
}     

int lop_server_recv(lop_server s)
{
    void *data;
    size_t size;
    double sched_time = lop_server_next_event_delay(s);
    int i;
#ifdef HAVE_SELECT
#ifndef HAVE_POLL
    fd_set ps;
    struct timeval stimeout;
    int res,nfds=0;
#endif
#endif

again:
    if (sched_time > 0.01) {
	if (sched_time > 10.0) {
	    sched_time = 10.0;
	}

#ifdef HAVE_POLL
    for (i=0; i < s->sockets_len; i++) {
        s->sockets[i].events = POLLIN | POLLPRI | POLLERR | POLLHUP;
        s->sockets[i].revents = 0;
    }

	poll(s->sockets, s->sockets_len, (int)(sched_time * 1000.0));

    for (i=0; i < s->sockets_len; i++)
    {
        if (   s->sockets[i].revents == POLLERR
            || s->sockets[i].revents == POLLHUP)
            return 0;

        if (s->sockets[i].revents)
            break;
    }

    if (i >= s->sockets_len)
    {
        sched_time = lop_server_next_event_delay(s);

        if (sched_time > 0.01)
            goto again;

        return dispatch_queued(s);
    }
#else
#ifdef HAVE_SELECT
    if(!initWSock()) return 0;

    FD_ZERO(&ps);
    for (i=0; i < s->sockets_len; i++) {
        FD_SET(s->sockets[i].fd,&ps);
        if (s->sockets[i].fd > nfds)
            nfds = s->sockets[i].fd;
    }

    stimeout.tv_sec = sched_time;
    stimeout.tv_usec = (sched_time-stimeout.tv_sec)*1.e6;
	res = select(nfds+1,&ps,NULL,NULL,&stimeout);
	if(res == SOCKET_ERROR) {
	    return 0;
	}

	if(!res) {
	    sched_time = lop_server_next_event_delay(s);

	    if (sched_time > 0.01)
            goto again;

	    return dispatch_queued(s);
	}
#endif
#endif
    } else {
	return dispatch_queued(s);
    }
    if (s->protocol == LOP_TCP) {
	data = lop_server_recv_raw_stream(s, &size);
    } else {
	data = lop_server_recv_raw(s, &size);
    }

    if (!data) {
        return 0;
    }
    if (lop_server_dispatch_data(s, data, size) < 0) {
        free(data);
        return -1;
    }
    free(data);
    return size;
}

/** \internal \brief Add a socket to this server's list of sockets.
 *  \param s The lop_server
 *  \param socket The socket number to add.
 *  \return The index number of the added socket, or -1 on failure.
 */
int lop_server_add_socket(lop_server s, int socket)
{
    if ((s->sockets_len+1) > s->sockets_alloc) {
        void *sp = realloc(s->sockets,
                           sizeof(*(s->sockets))*(s->sockets_alloc*2));
        if (!sp)
            return -1;
        s->sockets = sp;
        s->sockets_alloc *= 2;
    }

    s->sockets[s->sockets_len].fd = socket;
    s->sockets_len ++;

    return s->sockets_len-1;
}

/** \internal \brief Delete a socket from this server's list of sockets.
 *  \param s The lop_server
 *  \param index The index of the socket to delete, -1 if socket is provided.
 *  \param socket The socket number to delete, -1 if index is provided.
 *  \return The index number of the added socket.
 */
void lop_server_del_socket(lop_server s, int index, int socket)
{
    int i;

    if (index < 0 && socket != -1) {
        for (index=0; index < s->sockets_len; index++)
            if (s->sockets[index].fd == socket)
                break;
    }

    if (index < 0 || index >= s->sockets_len)
        return;

    for (i=index+1; i < s->sockets_len; i++)
        s->sockets[i-1] = s->sockets[i];
    s->sockets_len --;
}

int lop_server_dispatch_data(lop_server s, void *data, size_t size)
{
    int result = 0;
    char *path = data;
    ssize_t len = lop_validate_string(data, size);
    if (len < 0) {
        lop_throw(s, -len, "Invalid message path", NULL);
        return len;
    }

    if (!strcmp(data, "#bundle")) {
        char *pos;
        int remain;
        uint32_t elem_len;
        lop_timetag ts, now;

        ssize_t bundle_result = lop_validate_bundle(data, size);
        if (bundle_result < 0) {
            lop_throw(s, -bundle_result, "Invalid bundle", NULL);
            return bundle_result;
        }
        pos = (char *)data + len;
        remain = size - len;

        lop_timetag_now(&now);
        ts.sec = lop_otoh32(*((uint32_t *)pos));
        pos += 4;
        ts.frac = lop_otoh32(*((uint32_t *)pos));
        pos += 4;
        remain -= 8;

        while (remain >= 4) {
            lop_message msg;
            elem_len = lop_otoh32(*((uint32_t *)pos));
            pos += 4;
            remain -= 4;
            msg = lop_message_deserialise(pos, elem_len, &result);
            if (!msg) {
                lop_throw(s, result, "Invalid bundle element received", path);
                return -result;
            }

	    // set timetag from bundle
	    msg->ts = ts;

            // test for immediate dispatch
            if ((ts.sec == LOP_TT_IMMEDIATE.sec
                 && ts.frac == LOP_TT_IMMEDIATE.frac) ||
                                lop_timetag_diff(ts, now) <= 0.0) {
                dispatch_method(s, pos, msg);
                lop_message_free(msg);
            } else {
                queue_data(s, ts, pos, msg);
            }
            pos += elem_len;
            remain -= elem_len;
        }
    } else {
        lop_message msg = lop_message_deserialise(data, size, &result);
        if (NULL == msg) {
            lop_throw(s, result, "Invalid message received", path);
            return -result;
        }
        dispatch_method(s, data, msg);
        lop_message_free(msg);
    }
    return size;
}

/* returns the time in seconds until the next scheduled event */
double lop_server_next_event_delay(lop_server s)
{
    if (s->queued) {
	lop_timetag now;
	double delay;

	lop_timetag_now(&now);
	delay = lop_timetag_diff(((queued_msg_list *)s->queued)->ts, now);

	delay = delay > 100.0 ? 100.0 : delay;
	delay = delay < 0.0 ? 0.0 : delay;

	return delay;
    }

    return 100.0;
}

static void dispatch_method(lop_server s, const char *path,
    lop_message msg)
{
    char *types = msg->types + 1;
    int argc = strlen(types);
    lop_arg **argv = msg->argv;
    lop_method it;
    int ret = 1;
    int err;
    int pattern = strpbrk(path, " #*,?[]{}") != NULL;
    lop_address src = lop_address_new(NULL, NULL);
    char hostname[LOP_HOST_SIZE];
    char portname[32];
    const char *pptr;

    msg->source = src;

    //inet_ntop(s->addr.ss_family, &s->addr.padding, hostname, sizeof(hostname));
    if (s->protocol == LOP_UDP && s->addr_len>0) {
	err = getnameinfo((struct sockaddr *)&s->addr, sizeof(s->addr),
	    hostname, sizeof(hostname), portname, sizeof(portname),
	    NI_NUMERICHOST | NI_NUMERICSERV);
	if (err) {
	    switch (err) {
	    case EAI_AGAIN:
		lop_throw(s, err, "Try again", path);
		break;
	    case EAI_BADFLAGS:
		lop_throw(s, err, "Bad flags", path);
		break;
	    case EAI_FAIL:
		lop_throw(s, err, "Failed", path);
		break;
	    case EAI_FAMILY:
		lop_throw(s, err, "Cannot resolve address family", path);
		break;
	    case EAI_MEMORY:
		lop_throw(s, err, "Out of memory", path);
		break;
	    case EAI_NONAME:
		lop_throw(s, err, "Cannot resolve", path);
		break;
#ifndef WIN32
	    case EAI_SYSTEM:
		lop_throw(s, err, strerror(err), path);
		break;
#endif
	    default:
		lop_throw(s, err, "Unknown error", path);
		break;
	    }

	    return;
	}
    } else {
	hostname[0] = '\0';
	portname[0] = '\0';
    }
    
    
    // Store the source information in the lop_address
    if (src->host) free(src->host);
    if (src->host) free(src->port);
    src->host = strdup(hostname);
    src->port = strdup(portname);
    src->protocol = s->protocol;

    for (it = s->first; it; it = it->next) {
	/* If paths match or handler is wildcard */
	if (!it->path || !strcmp(path, it->path) ||
	    (pattern && lop_pattern_match(it->path, path))) {
	    /* If types match or handler is wildcard */
	    if (!it->typespec || !strcmp(types, it->typespec)) {
		/* Send wildcard path to generic handler, expanded path
		  to others.
		*/
		pptr = path;
		if (it->path) pptr = it->path;
		ret = it->handler(pptr, types, argv, argc, msg,
				      it->user_data);

	    } else if (lop_can_coerce_spec(types, it->typespec)) {
		int i;
		int opsize = 0;
		char *ptr = msg->data;
		char *data_co, *data_co_ptr;

		argv = calloc(argc, sizeof(lop_arg *));
		for (i=0; i<argc; i++) {
		    opsize += lop_arg_size(it->typespec[i], ptr);
		    ptr += lop_arg_size(types[i], ptr);
		}

		data_co = malloc(opsize);
		data_co_ptr = data_co;
		ptr = msg->data;
		for (i=0; i<argc; i++) {
		    argv[i] = (lop_arg *)data_co_ptr;
		    lop_coerce(it->typespec[i], (lop_arg *)data_co_ptr,
			      types[i], (lop_arg *)ptr);
		    data_co_ptr += lop_arg_size(it->typespec[i], data_co_ptr);
		    ptr += lop_arg_size(types[i], ptr);
		}

		/* Send wildcard path to generic handler, expanded path
		  to others.
		*/
		pptr = path;
		if (it->path) pptr = it->path;
		ret = it->handler(pptr, it->typespec, argv, argc, msg,
				      it->user_data);
		free(argv);
		free(data_co);
		argv = NULL;
	    }

	    if (ret == 0 && !pattern) {
		break;
	    }
	}
    }

    /* If we find no matching methods, check for protocol level stuff */
    if (ret == 1 && s->protocol == LOP_UDP) {
	char *pos = strrchr(path, '/');

	/* if its a method enumeration call */
	if (pos && *(pos+1) == '\0') {
	    lop_message reply = lop_message_new();
	    int len = strlen(path);
	    lop_strlist *sl = NULL, *slit, *slnew, *slend;

	    if (!strcmp(types, "i")) {
		lop_message_add_int32(reply, argv[0]->i);
	    }
	    lop_message_add_string(reply, path);

	    for (it = s->first; it; it = it->next) {
		/* If paths match */
		if (it->path && !strncmp(path, it->path, len)) {
		    char *tmp;
		    char *sec;

		    tmp = malloc(strlen(it->path + len) + 1);
		    strcpy(tmp, it->path + len);
#ifdef WIN32
            sec = strchr(tmp,'/');
#else
		    sec = index(tmp, '/');
#endif
		    if (sec) *sec = '\0';
		    slend = sl;
		    for (slit = sl; slit; slend = slit, slit = slit->next) {
			if (!strcmp(slit->str, tmp)) {
			    free(tmp);
			    tmp = NULL;
			    break;
			}
		    }
		    if (tmp) {
			slnew = calloc(1, sizeof(lop_strlist));
			slnew->str = tmp;
			slnew->next = NULL;
			if (!slend) {
			    sl = slnew;
			} else {
			    slend->next = slnew;
			}
		    }
		}
	    }

	    slit = sl;
	    while(slit) {
		lop_message_add_string(reply, slit->str);
		slnew = slit;
		slit = slit->next;
		free(slnew->str);
		free(slnew);
	    }
	    lop_send_message(src, "#reply", reply);
	    lop_message_free(reply);
	}
    }

    lop_address_free(src);
    msg->source = NULL;
}

int lop_server_events_pending(lop_server s)
{
    return s->queued != 0;
}

static void queue_data(lop_server s, lop_timetag ts, const char *path,
    lop_message msg)
{
    /* insert blob into future dispatch queue */
    queued_msg_list *it = s->queued;
    queued_msg_list *prev = NULL;
    queued_msg_list *ins = calloc(1, sizeof(queued_msg_list));

    ins->ts = ts;
    ins->path = strdup(path);
    ins->msg = msg;

    while (it) {
	if (lop_timetag_diff(it->ts, ts) > 0.0) {
	    if (prev) {
		prev->next = ins;
	    } else {
		s->queued = ins;
		ins->next = NULL;
	    }
	    ins->next = it;

	    return;
	}
	prev = it;
	it = it->next;
    }

    /* fell through, so this event is last */
    if (prev) {
	prev->next = ins;
    } else {
	s->queued = ins;
    }
    ins->next = NULL;
}

static int dispatch_queued(lop_server s)
{
    queued_msg_list *head = s->queued;
    queued_msg_list *tailhead;
    lop_timetag disp_time;

    if (!head) {
	lop_throw(s, LOP_INT_ERR, "attempted to dispatch with empty queue",
		 "timeout");
	return 1;
    }

    disp_time = head->ts;

    do {
        char *path;
        lop_message msg;
	tailhead = head->next;
        path = ((queued_msg_list *)s->queued)->path;
        msg = ((queued_msg_list *)s->queued)->msg;
	dispatch_method(s, path, msg);
        free(path);
        lop_message_free(msg);
	free((queued_msg_list *)s->queued);

	s->queued = tailhead;
	head = tailhead;
    } while (head && lop_timetag_diff(head->ts, disp_time) < FLT_EPSILON);

    return 0;
}

lop_method lop_server_add_method(lop_server s, const char *path,
			       const char *typespec, lop_method_handler h,
			       void *user_data)
{
    lop_method m = calloc(1, sizeof(struct _lop_method));
    lop_method it;

    if (path && strpbrk(path, " #*,?[]{}")) {
	return NULL;
    }

    if (path) {
	m->path = strdup(path);
    } else {
	m->path = NULL;
    }

    if (typespec) {
	m->typespec = strdup(typespec);
    } else {
	m->typespec = NULL;
    }

    m->handler = h;
    m->user_data = user_data;
    m->next = NULL;

    /* append the new method to the list */
    if (!s->first) {
	s->first = m;
    } else {
	/* get to the last member of the list */
	for (it=s->first; it->next; it=it->next);
	it->next = m;
    }

    return m;
}

void lop_server_del_method(lop_server s, const char *path,
			  const char *typespec)
{
    lop_method it, prev, next;
    int pattern = 0;

    if (!s->first) return;
    if (path) pattern = strpbrk(path, " #*,?[]{}") != NULL;

    it = s->first;
    prev = it;
    while (it) {
	/* incase we free it */
	next = it->next;

	/* If paths match or handler is wildcard */
	if ((it->path == path) ||
	    (path && it->path && !strcmp(path, it->path)) ||
	    (pattern && it->path && lop_pattern_match(it->path, path))) {
	    /* If types match or handler is wildcard */
	    if ((it->typespec == typespec) ||
		(typespec && it->typespec && !strcmp(typespec, it->typespec))
	        ) {
		/* Take care when removing the head. */
		if (it == s->first) {
		    s->first = it->next;
		} else {
		    prev->next = it->next;
		}
		next = it->next;
		free((void *)it->path);
		free((void *)it->typespec);
		free(it);
		it = prev;
	    }
	}
	prev = it;
	if (it) it = next;
    }
}

int lop_server_get_socket_fd(lop_server s)
{
    if (s->protocol != LOP_UDP &&
        s->protocol != LOP_TCP 
#ifndef WIN32
        && s->protocol != LOP_UNIX
#endif
    ) {
        return -1;  /* assume it is not supported */
    }
    return s->sockets[0].fd;
}

int lop_server_get_port(lop_server s)
{
    if (!s) {
	return 0;
    }

    return s->port;
}

int lop_server_get_protocol(lop_server s)
{
    if (!s) {
	return -1;
    }

    return s->protocol;
}


char *lop_server_get_url(lop_server s)
{
    int ret=0;
    char *buf;

    if (!s) {
	return NULL;
    }

    if (s->protocol == LOP_UDP || s->protocol == LOP_TCP) {
	char *proto = s->protocol == LOP_UDP ? "udp" : "tcp";

#ifndef _MSC_VER
	ret = snprintf(NULL, 0, "osc.%s://%s:%d/", proto, s->hostname, s->port);
#endif
	if (ret <= 0) {
	    /* this libc is not C99 compliant, guess a size */
	    ret = 1023;
	}
	buf = malloc((ret + 2) * sizeof(char));
	snprintf(buf, ret+1, "osc.%s://%s:%d/", proto, s->hostname, s->port);

	return buf;
    } 
#ifndef WIN32
    else if (s->protocol == LOP_UNIX) {
	ret = snprintf(NULL, 0, "osc.unix:///%s", s->path);
	if (ret <= 0) {
	    /* this libc is not C99 compliant, guess a size */
	    ret = 1023;
	}
	buf = malloc((ret + 2) * sizeof(char));
	snprintf(buf, ret+1, "osc.unix:///%s", s->path);

	return buf;
    }
#endif
    return NULL;
}

void lop_server_pp(lop_server s)
{
    lop_method it;

    printf("socket: %d\n\n", s->sockets[0].fd);
    printf("Methods\n");
    for (it = s->first; it; it = it->next) {
	printf("\n");
	lop_method_pp_prefix(it, "   ");
    }
}

static int lop_can_coerce_spec(const char *a, const char *b)
{
    unsigned int i;

    if (strlen(a) != strlen(b)) {
	return 0;
    }

    for (i=0; a[i]; i++) {
	if (!lop_can_coerce(a[i], b[i])) {
	    return 0;
	}
    }

    return 1;
}

static int lop_can_coerce(char a, char b)
{
    return ((a == b) ||
           (lop_is_numerical_type(a) && lop_is_numerical_type(b)) ||
           (lop_is_string_type(a) && lop_is_string_type (b)));
}

void lop_throw(lop_server s, int errnum, const char *message, const char *path)
{
    if (s->err_h) {
	(*s->err_h)(errnum, message, path);
    }
}

/* vi:set ts=8 sts=4 sw=4: */
