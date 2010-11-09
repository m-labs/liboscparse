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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <float.h>
#include <sys/types.h>

#include <unistd.h>

#define geterror() errno

#include "lop_types_internal.h"
#include "lop_internal.h"
#include "lop/lop_throw.h"
#include "lop/lop_lowlevel.h"
#include "lop/lop_endian.h"

static void dispatch_method(lop_server s, const char *path,
    lop_message msg);
static void dispatch_queued(lop_server s);
static void queue_data(lop_server s, lop_timetag ts, const char *path,
    lop_message msg);
static int lop_can_coerce(char a, char b);
static int lop_can_coerce_spec(const char *a, const char *b);

typedef struct {
    lop_timetag ts;
    char *path;
    lop_message msg;
    void *next;
} queued_msg_list;

lop_server lop_server_new(lop_err_handler err_h, lop_send_handler send_h, void *send_h_arg)
{
    lop_server s;
    
    s = calloc(1, sizeof(struct _lop_server));
    if (!s) return 0;
    
    s->err_h = err_h;
    s->send_h = send_h;
    s->send_h_arg = send_h_arg;
    
    return s;
}

void lop_server_free(lop_server s)
{
    lop_method it;
    lop_method next;
    
#warning free s->queued ?
    for (it = s->first; it; it = next) {
        next = it->next;
        free((char *)it->path);
        free((char *)it->typespec);
        free(it);
    }
    free(s);
}

int lop_server_dispatch_data(lop_server s, void *data, size_t size)
{
    int result;
    char *path;
    ssize_t len;
    
    dispatch_queued(s);
    if (size == 0)
        return 0;
    
    result = 0;
    path = data;
    len = lop_validate_string(data, size);
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

static void lop_send_message(lop_server s, const char *path, lop_message msg)
{
    const size_t data_len = lop_message_length(msg, path);
    char *data = lop_message_serialise(msg, path, NULL, NULL);

    s->send_h(data, data_len, s->send_h_arg);

    free(data);
}

static void dispatch_method(lop_server s, const char *path,
    lop_message msg)
{
    char *types = msg->types + 1;
    int argc = strlen(types);
    lop_arg **argv = msg->argv;
    lop_method it;
    int ret = 1;
    int pattern = strpbrk(path, " #*,?[]{}") != NULL;
    const char *pptr;
    
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
    if (ret == 1) {
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
		    sec = index(tmp, '/');
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
	    lop_send_message(s, "#reply", reply);
	    lop_message_free(reply);
	}
    }
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

static void dispatch_queued(lop_server s)
{
    queued_msg_list *head = s->queued;
    queued_msg_list *tailhead;
    lop_timetag disp_time;

    lop_timetag_now(&disp_time);
    while (head && lop_timetag_diff(head->ts, disp_time) < FLT_EPSILON) {
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
    }
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

void lop_server_pp(lop_server s)
{
    lop_method it;

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
