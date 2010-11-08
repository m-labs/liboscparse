#ifndef LOP_TYPES_H
#define LOP_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_POLL
#include <poll.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#ifdef _MSC_VER
typedef SSIZE_T ssize_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
#endif

#include <pthread.h>

#include "lo/lop_osc_types.h"

typedef void (*lop_err_handler)(int num, const char *msg, const char *path);

struct _lop_method;

typedef struct _lop_address {
	char            *host;
	int              socket;
	char            *port;
	int              protocol;
	struct addrinfo *ai;
	int              errnum;
	const char      *errstr;
	int              ttl;
} *lop_address;

typedef struct _lop_blob {
	uint32_t  size;
	char     *data;
} *lop_blob;

typedef struct _lop_message {
	char      *types;
	size_t     typelen;
	size_t     typesize;
	void      *data;
	size_t     datalen;
	size_t     datasize;
	lop_address source;
        lop_arg   **argv;
        /* timestamp from bundle (LOP_TT_IMMEDIATE for unbundled messages) */
        lop_timetag ts;
} *lop_message;

typedef int (*lop_method_handler)(const char *path, const char *types,
				 lop_arg **argv, int argc, struct _lop_message
				 *msg, void *user_data);

typedef struct _lop_method {
	const char        *path;
	const char        *typespec;
	lop_method_handler  handler;
	char              *user_data;
	struct _lop_method *next;
} *lop_method;

typedef struct _lop_server {
	struct addrinfo         *ai;
	lop_method                first;
	lop_err_handler           err_h;
	int	 	         port;
	char                   	*hostname;
	char                   	*path;
	int            	         protocol;
	void		        *queued;
	struct sockaddr_storage  addr;
	socklen_t 	         addr_len;
	int                  sockets_len;
	int                  sockets_alloc;
#ifdef HAVE_POLL
	struct pollfd        *sockets;
#else
	struct { int fd; }   *sockets;
#endif
} *lop_server;

typedef struct _lop_server_thread {
lop_server    s;
	pthread_t    thread;
	volatile int active;
	volatile int done;
} *lop_server_thread;

typedef struct _lop_bundle {
	size_t      size;
	size_t	    len;
	lop_timetag  ts;
	lop_message *msgs;
	char      **paths;
} *lop_bundle;

typedef struct _lop_strlist {
	char *str;
	struct _lop_strlist *next;
} lop_strlist;

typedef union {
    int32_t  i;
    float    f;
    char     c;
    uint32_t nl;
} lop_pcast32;
    
typedef union {
    int64_t    i;
    double     f;
    uint64_t   nl;
    lop_timetag tt;
} lop_pcast64;

extern struct lop_cs {
	int udp;
	int tcp;
} lop_client_sockets;
	
#endif
