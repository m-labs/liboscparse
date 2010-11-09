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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lop_types_internal.h"
#include "lop_internal.h"
#include "lop/lop_lowlevel.h"
#include "lop/lop_macros.h"
#include "lop/lop_endian.h"
#include "lop/lop_osc_types.h"

#define LOP_DEF_TYPE_SIZE 8
#define LOP_DEF_DATA_SIZE 8

static char lop_numerical_types[] = {
    LOP_INT32,
    LOP_FLOAT,
    LOP_INT64,
    LOP_DOUBLE,
    '\0'
};

static char lop_string_types[] = {
    LOP_STRING,
    LOP_SYMBOL,
    '\0'
};

static int lop_message_add_typechar(lop_message m, char t);
static void *lop_message_add_data(lop_message m, size_t s);
void lop_arg_pp_internal(lop_type type, void *data, int bigendian);

// Used for calculating new sizes when expanding message data buffers.
// Note that log(x)/0.69315 = log2(x): this simply finds the next
// highest power of 2.
#if 1
#define lop_pow2_over(a,b) \
    a = ((b > a) ? (a << ((int)((log(((double)b/(double)a))/0.69315)+1))) : a);
#else
#define lop_pow2_over(a,b) \
    while (b > a) {a *= 2;}
#endif

lop_message lop_message_new()
{
    lop_message m = malloc(sizeof(struct _lop_message));
    if (!m) {
	return m;
    }

    m->types = calloc(LOP_DEF_TYPE_SIZE, sizeof(char));
    m->types[0] = ',';
    m->types[1] = '\0';
    m->typelen = 1;
    m->typesize = LOP_DEF_TYPE_SIZE;
    m->data = NULL;
    m->datalen = 0;
    m->datasize = 0;
    m->argv = NULL;
    m->ts = LOP_TT_IMMEDIATE;

    return m;
}

void lop_message_free(lop_message m)
{
    if (m) {
	free(m->types);
	free(m->data);
        free(m->argv);
    }
    free(m);
}

/* Don't call lop_message_add_varargs_internal directly, use
 * lop_message_add_varargs, a macro wrapping this function with
 * appropriate values for file and line */

int lop_message_add_varargs_internal(lop_message msg, const char *types,
                                    va_list ap, const char *file, int line)
{
    int count = 0;
    int i;
    int64_t i64;
    float f;
    char *s;
    lop_blob b;
    uint8_t *m;
    lop_timetag tt;
    double d;
	int ret = 0;

    while (types && *types) {
	count++;
    i=0;
    i64=0;
    f=0;
    s=0;
    b=0;
    m=0;
    d=0;
	switch (*types++) {

	case LOP_INT32:
	    i = va_arg(ap, int32_t);
	    lop_message_add_int32(msg, i);
	    break;

	case LOP_FLOAT:
	    f = (float)va_arg(ap, double);
	    lop_message_add_float(msg, f);
	    break;

	case LOP_STRING:
	    s = va_arg(ap, char *);
#ifdef __GNUC__
	    if (s == (char *)LOP_MARKER_A) {
		fprintf(stderr, "liblo error: lop_message_add called with "
			"invalid string pointer for arg %d, probably arg mismatch\n"
		        "at %s:%d, exiting.\n", count, file, line);
	    }
#endif
	    lop_message_add_string(msg, s);
	    break;
	case LOP_BLOB:
	    b = va_arg(ap, lop_blob);
	    lop_message_add_blob(msg, b);
	    break;
	case LOP_INT64:
	    i64 = va_arg(ap, int64_t);
	    lop_message_add_int64(msg, i64);
	    break;

	case LOP_TIMETAG:
	    tt = va_arg(ap, lop_timetag);
	    lop_message_add_timetag(msg, tt);
	    break;

	case LOP_DOUBLE:
	    d = va_arg(ap, double);
	    lop_message_add_double(msg, d);
	    break;

	case LOP_SYMBOL:
	    s = va_arg(ap, char *);
#ifdef __GNUC__
	    if (s == (char *)LOP_MARKER_A) {
		fprintf(stderr, "liblo error: lop_message_add called with "
			"invalid symbol pointer for arg %d, probably arg mismatch\n"
		        "at %s:%d, exiting.\n", count, file, line);
        va_end(ap);
        return -2;
	    }
#endif
	    lop_message_add_symbol(msg, s);
	    break;

	case LOP_CHAR:
	    i = va_arg(ap, int);
	    lop_message_add_char(msg, i);
	    break;

	case LOP_MIDI:
	    m = va_arg(ap, uint8_t *);
	    lop_message_add_midi(msg, m);
	    break;

	case LOP_TRUE:
	    lop_message_add_true(msg);
	    break;

	case LOP_FALSE:
	    lop_message_add_false(msg);
	    break;

	case LOP_NIL:
	    lop_message_add_nil(msg);
	    break;

	case LOP_INFINITUM:
	    lop_message_add_infinitum(msg);
	    break;

	default:
		ret = -1; // unknown type
	    fprintf(stderr, "liblo warning: unknown type '%c' at %s:%d\n",
		    *(types-1), file, line);
	    break;
	}
    }
#ifdef __GNUC__
    i = va_arg(ap, uint32_t);
    if (i != LOP_MARKER_A) {
	ret = -2; // bad format/args
	fprintf(stderr, "liblo error: lop_message_add or lop_message_add_varargs called with "
			"mismatching types and data at\n%s:%d, exiting.\n", file, line);
    va_end(ap);
    return ret;
    }
    i = va_arg(ap, uint32_t);
    if (i != LOP_MARKER_B) {
	ret = -2; // bad format/args
	fprintf(stderr, "liblo error: lop_message_add or lop_message_add_varargs called with "
			"mismatching types and data at\n%s:%d, exiting.\n", file, line);
    }
#endif
    va_end(ap);

	return ret;
}

/* Don't call lop_message_add_internal directly, use lop_message_add,
 * a macro wrapping this function with appropriate values for file and line */

#ifdef __GNUC__
int lop_message_add_internal(lop_message msg, const char *file, const int line,
                            const char *types, ...)
#else
int lop_message_add(lop_message msg, const char *types, ...)
#endif
{
    va_list ap;
    int ret = 0;

#ifndef __GNUC__
    const char *file = "";
    const int line = 0;
#endif

    va_start(ap, types);
    ret = lop_message_add_varargs_internal(msg, types, ap, file, line);

	return ret;
}
	
int lop_message_add_int32(lop_message m, int32_t a)
{
    lop_pcast32 b;
    int32_t *nptr = lop_message_add_data(m, sizeof(a));
    if (!nptr) return -1;
    b.i = a;

    if (lop_message_add_typechar(m, LOP_INT32))
        return -1;
    *nptr = b.nl;
    return 0;
}
    
int lop_message_add_float(lop_message m, float a)
{
    lop_pcast32 b;
    int32_t *nptr = lop_message_add_data(m, sizeof(a));
    if (!nptr) return -1;
    b.f = a;

    if (lop_message_add_typechar(m, LOP_FLOAT))
        return -1;
    *nptr = b.nl;
    return 0;
}

int lop_message_add_string(lop_message m, const char *a)
{
    const int size = lop_strsize(a);
    char *nptr = lop_message_add_data(m, size);
    if (!nptr) return -1;

    if (lop_message_add_typechar(m, LOP_STRING))
        return -1;
    strncpy(nptr, a, size);
    return 0;
}

int lop_message_add_blob(lop_message m, lop_blob a)
{
    const uint32_t size = lop_blobsize(a);
    const uint32_t dsize = lop_blob_datasize(a);
    char *nptr = lop_message_add_data(m, size);
    if (!nptr) return -1;

    if (lop_message_add_typechar(m, LOP_BLOB))
        return -1;
    memset(nptr + size - 4, 0, 4);

    memcpy(nptr, &dsize, sizeof(dsize));
    memcpy(nptr + sizeof(int32_t), lop_blob_dataptr(a), lop_blob_datasize(a));
    return 0;
}

int lop_message_add_int64(lop_message m, int64_t a)
{
    lop_pcast64 b;
    uint64_t *nptr = lop_message_add_data(m, sizeof(a));
    if (!nptr) return -1;
    b.i = a;

    if (lop_message_add_typechar(m, LOP_INT64))
        return -1;
    *nptr = b.nl;
    return 0;
}

int lop_message_add_timetag(lop_message m, lop_timetag a)
{
    lop_pcast64 b;
    uint64_t *nptr = lop_message_add_data(m, sizeof(a));
    if (!nptr) return -1;
    b.tt = a;

    if (lop_message_add_typechar(m, LOP_TIMETAG))
        return -1;
    *nptr = b.nl;
    return 0;
}

int lop_message_add_double(lop_message m, double a)
{
    lop_pcast64 b;
    uint64_t *nptr = lop_message_add_data(m, sizeof(a));
    if (!nptr) return -1;
    b.f = a;

    if (lop_message_add_typechar(m, LOP_DOUBLE))
        return -1;
    *nptr = b.nl;
    return 0;
}

int lop_message_add_symbol(lop_message m, const char *a)
{
    const int size = lop_strsize(a);
    char *nptr = lop_message_add_data(m, size);
    if (!nptr) return -1;

    if (lop_message_add_typechar(m, LOP_SYMBOL))
        return -1;
    strncpy(nptr, a, size);
    return 0;
}

int lop_message_add_char(lop_message m, char a)
{
    lop_pcast32 b;
    int32_t *nptr = lop_message_add_data(m, sizeof(int32_t));
    if (!nptr) return -1;

    b.c = a;

    if (lop_message_add_typechar(m, LOP_CHAR))
        return -1;
    *nptr = b.nl;
    return 0;
}

int lop_message_add_midi(lop_message m, uint8_t a[4])
{
    char *nptr = lop_message_add_data(m, 4);
    if (!nptr) return -1;

    if (lop_message_add_typechar(m, LOP_MIDI))
        return -1;

    memcpy(nptr, a, sizeof(a));
    return 0;
}

int lop_message_add_true(lop_message m)
{
    return lop_message_add_typechar(m, LOP_TRUE);
}

int lop_message_add_false(lop_message m)
{
    return lop_message_add_typechar(m, LOP_FALSE);
}

int lop_message_add_nil(lop_message m)
{
    return lop_message_add_typechar(m, LOP_NIL);
}

int lop_message_add_infinitum(lop_message m)
{
    return lop_message_add_typechar(m, LOP_INFINITUM);
}

static int lop_message_add_typechar(lop_message m, char t)
{
    if (m->typelen + 1 >= m->typesize) {
        int new_typesize = m->typesize * 2;
        char *new_types = 0;
        if (!new_typesize)
            new_typesize = LOP_DEF_TYPE_SIZE;
        new_types = realloc(m->types, new_typesize);
        if (!new_types) return -1;
        m->types = new_types;
        m->typesize = new_typesize;
    }
    m->types[m->typelen] = t;
    m->typelen++;
    m->types[m->typelen] = '\0';
    if (m->argv) {
        free(m->argv);
        m->argv = NULL;
    }
    return 0;
}

static void *lop_message_add_data(lop_message m, size_t s)
{
    uint32_t old_dlen = m->datalen;
    int new_datasize = m->datasize;
    int new_datalen = m->datalen + s;
    void *new_data = 0;

	if (!new_datasize)
	    new_datasize = LOP_DEF_DATA_SIZE;

    lop_pow2_over(new_datasize, new_datalen);
	new_data = realloc(m->data, new_datasize);
    if (!new_data)
        return 0;

    m->datalen = new_datalen;
    m->datasize = new_datasize;
    m->data = new_data;

    if (m->argv) {
        free(m->argv);
        m->argv = NULL;
    }

    return (void*)((char*)m->data + old_dlen);
}

int lop_strsize(const char *s)
{
    return 4 * (strlen(s) / 4 + 1);
}

size_t lop_arg_size(lop_type type, void *data)
{
    switch (type) {
    case LOP_TRUE:
    case LOP_FALSE:
    case LOP_NIL:
    case LOP_INFINITUM:
	return 0;

    case LOP_INT32:
    case LOP_FLOAT:
    case LOP_MIDI:
    case LOP_CHAR:
	return 4;

    case LOP_INT64:
    case LOP_TIMETAG:
    case LOP_DOUBLE:
	return 8;

    case LOP_STRING:
    case LOP_SYMBOL:
	return lop_strsize((char *)data);

    case LOP_BLOB:
	return lop_blobsize((lop_blob)data);

    default:
	fprintf(stderr, "liblo warning: unhandled OSC type '%c' at %s:%d\n", type, __FILE__, __LINE__);
	return 0;
    }

    return 0;
}

char *lop_get_path(void *data, ssize_t size)
{
    ssize_t result = lop_validate_string(data, size);
    return (result >= 4) ? (char *)data : NULL;
}

ssize_t lop_validate_string(void *data, ssize_t size)
{
    ssize_t i = 0, len = 0;
    char *pos = data;

    if (size < 0) {
        return -LOP_ESIZE;      // invalid size
    }
    for (i = 0; i < size; ++i) {
        if (pos[i] == '\0') {
            len = 4 * (i / 4 + 1);
            break;
        }
    }
    if (0 == len) {
        return -LOP_ETERM;      // string not terminated
    }
    if (len > size) {
        return -LOP_ESIZE;      // would overflow buffer
    }
    for (; i < len; ++i) {
        if (pos[i] != '\0') {
            return -LOP_EPAD;  // non-zero char found in pad area
        }
    }
    return len;
}


ssize_t lop_validate_blob(void *data, ssize_t size)
{
    ssize_t i, end, len;
    uint32_t dsize;
    char *pos = (char *)data;

    if (size < 0) {
        return -LOP_ESIZE;      // invalid size
    }
    dsize = lop_otoh32(*(uint32_t*)data);
    if (dsize > LOP_MAX_MSG_SIZE) { // avoid int overflow in next step
        return -LOP_ESIZE;
    }
    end = sizeof(uint32_t) + dsize; // end of data
    len = 4 * (end / 4 + 1); // full padded size
    if (len > size) {
        return -LOP_ESIZE;      // would overflow buffer
    }
    for (i = end; i < len; ++i) {
        if (pos[i] != '\0') {
            return -LOP_EPAD;  // non-zero char found in pad area
        }
    }
    return len;
}


ssize_t lop_validate_bundle(void *data, ssize_t size)
{
    ssize_t len = 0, remain = size;
    char *pos = data;
    uint32_t elem_len;

    len = lop_validate_string(data, size);
    if (len < 0) {
        return -LOP_ESIZE; // invalid size
    }
    if (0 != strcmp(data, "#bundle")) {
        return -LOP_EINVALIDBUND; // not a bundle
    }
    pos += len;
    remain -= len;

    // time tag
    if (remain < 8) {
        return -LOP_ESIZE;
    }
    pos += 8;
    remain -= 8;

    while (remain >= 4) {
        elem_len = lop_otoh32(*((uint32_t *)pos));
        pos += 4;
        remain -= 4;
        if (elem_len > remain) {
            return -LOP_ESIZE;
        }
        pos += elem_len;
        remain -= elem_len;
    }
    if (0 != remain) {
        return -LOP_ESIZE;
    }
    return size;
}


ssize_t lop_validate_arg(lop_type type, void *data, ssize_t size)
{
    if (size < 0) {
        return -1;
    }
    switch (type) {
    case LOP_TRUE:
    case LOP_FALSE:
    case LOP_NIL:
    case LOP_INFINITUM:
        return 0;

    case LOP_INT32:
    case LOP_FLOAT:
    case LOP_MIDI:
    case LOP_CHAR:
        return size >= 4 ? 4 : -LOP_ESIZE;

    case LOP_INT64:
    case LOP_TIMETAG:
    case LOP_DOUBLE:
        return size >= 8 ? 8 : -LOP_ESIZE;

    case LOP_STRING:
    case LOP_SYMBOL:
        return lop_validate_string((char *)data, size);

    case LOP_BLOB:
        return lop_validate_blob((lop_blob)data, size);

    default:
        return -LOP_EINVALIDTYPE;
    }
    return -LOP_INT_ERR;
}

/* convert endianness of arg pointed to by data from network to host */
void lop_arg_host_endian(lop_type type, void *data)
{
    switch (type) {
    case LOP_INT32:
    case LOP_FLOAT:
    case LOP_BLOB:
    case LOP_CHAR:
	*(int32_t *)data = lop_otoh32(*(int32_t *)data);
	break;

    case LOP_INT64:
    case LOP_TIMETAG:
    case LOP_DOUBLE:
	*(int64_t *)data = lop_otoh64(*(int64_t *)data);
	break;

    case LOP_STRING:
    case LOP_SYMBOL:
    case LOP_MIDI:
    case LOP_TRUE:
    case LOP_FALSE:
    case LOP_NIL:
    case LOP_INFINITUM:
	/* these are fine */
	break;

    default:
	fprintf(stderr, "liblo warning: unhandled OSC type '%c' at %s:%d\n",
		type, __FILE__, __LINE__);
	break;
    }
}

/* convert endianness of arg pointed to by data from host to network */
void lop_arg_network_endian(lop_type type, void *data)
{
    switch (type) {
    case LOP_INT32:
    case LOP_FLOAT:
    case LOP_BLOB:
    case LOP_CHAR:
        *(int32_t *)data = lop_htoo32(*(int32_t *)data);
        break;

    case LOP_INT64:
    case LOP_TIMETAG:
    case LOP_DOUBLE:
        *(int64_t *)data = lop_htoo64(*(int64_t *)data);
        break;

    case LOP_STRING:
    case LOP_SYMBOL:
    case LOP_MIDI:
    case LOP_TRUE:
    case LOP_FALSE:
    case LOP_NIL:
    case LOP_INFINITUM:
        /* these are fine */
        break;

    default:
        fprintf(stderr, "liblo warning: unhandled OSC type '%c' at %s:%d\n",
                type, __FILE__, __LINE__);
        break;
    }
}

lop_timetag lop_message_get_timestamp(lop_message m)
{
    return m->ts;
}

size_t lop_message_length(lop_message m, const char *path)
{
    return lop_strsize(path) + lop_strsize(m->types) + m->datalen;
}

int lop_message_get_argc(lop_message m)
{
    return m->typelen - 1;
}

lop_arg **lop_message_get_argv(lop_message m)
{
    int i, argc;
    char *types, *ptr;
    lop_arg **argv;

    if (NULL != m->argv) { return m->argv; }

    i = 0;
    argc = m->typelen - 1;
    types = m->types + 1;
    ptr = m->data;

    argv = calloc(argc, sizeof(lop_arg *));
    for (i = 0; i < argc; ++i) {
        size_t len = lop_arg_size(types[i], ptr);
        argv[i] = len ? (lop_arg*)ptr : NULL;
        ptr += len;
    }
    m->argv = argv;
    return argv;
}

char *lop_message_get_types(lop_message m)
{
    return m->types + 1;
}

void *lop_message_serialise(lop_message m, const char *path, void *to,
			   size_t *size)
{
    int i, argc;
    char *types, *ptr;
    size_t s = lop_message_length(m, path);

    if (size) {
	*size = s;
    }

    if (!to) {
	to = calloc(1, s);
    }
    memset((char*)to + lop_strsize(path) - 4, 0, 4); // ensure zero-padding
    strcpy(to, path);
    memset((char*)to + lop_strsize(path) + lop_strsize(m->types) - 4, 0, 4);
    strcpy((char*)to + lop_strsize(path), m->types);

    types = m->types + 1;
    ptr = (char*)to + lop_strsize(path) + lop_strsize(m->types);
    memcpy(ptr, m->data, m->datalen);

    i = 0;
    argc = m->typelen - 1;
    for (i = 0; i < argc; ++i) {
        size_t len = lop_arg_size(types[i], ptr);
        lop_arg_network_endian(types[i], ptr);
        ptr += len;
    }
    return to;
}


lop_message lop_message_deserialise(void *data, size_t size, int *result)
{
    lop_message msg = NULL;
    char *types = NULL, *ptr = NULL;
    int i = 0, argc = 0, remain = size, res = 0, len;

    if (remain <= 0) { res = LOP_ESIZE; goto fail; }

    msg = malloc(sizeof(struct _lop_message));
    if (!msg) { res = LOP_EALLOC; goto fail; }

    msg->types = NULL;
    msg->typelen = 0;
    msg->typesize = 0;
    msg->data = NULL;
    msg->datalen = 0;
    msg->datasize = 0;
    msg->argv = NULL;
    msg->ts = LOP_TT_IMMEDIATE;

    // path
    len = lop_validate_string(data, remain);
    if (len < 0) {
        res = LOP_EINVALIDPATH; // invalid path string
        goto fail;
    }
    remain -= len;

    // types
    if (remain <= 0) {
        res = LOP_ENOTYPE; // no type tag string
        goto fail;
    }
    types = (char*)data + len;
    len = lop_validate_string(types, remain);
    if (len < 0) {
        res = LOP_EINVALIDTYPE; // invalid type tag string
        goto fail;
    }
    if (types[0] != ',') {
        res = LOP_EBADTYPE; // type tag string missing initial comma
        goto fail;
    }
    remain -= len;

    msg->typelen = strlen(types);
    msg->typesize = len;
    msg->types = malloc(msg->typesize);
    if (NULL == msg->types) { res = LOP_EALLOC; goto fail; }
    memcpy(msg->types, types, msg->typesize);

    // args
    msg->data = malloc(remain);
    if (NULL == msg->data) { res = LOP_EALLOC; goto fail; }
    memcpy(msg->data, types + len, remain);
    msg->datalen = msg->datasize = remain;
    ptr = msg->data;

    ++types;
    argc = msg->typelen - 1;
    if (argc) {
        msg->argv = calloc(argc, sizeof(lop_arg *));
        if (NULL == msg->argv) { res = LOP_EALLOC; goto fail; }
    }

    for (i = 0; remain >= 0 && i < argc; ++i) {
        len = lop_validate_arg((lop_type)types[i], ptr, remain);
        if (len < 0) {
            res = LOP_EINVALIDARG; // invalid argument
            goto fail;
        }
        lop_arg_host_endian((lop_type)types[i], ptr);
        msg->argv[i] = len ? (lop_arg*)ptr : NULL;
        remain -= len;
        ptr += len;
    }
    if (0 != remain || i != argc) {
        res = LOP_ESIZE; // size/argument mismatch
        goto fail;
    }

    if (result) { *result = res; }
    return msg;

fail:
    if (msg) { lop_message_free(msg); }
    if (result) { *result = res; }
    return NULL;
}

void lop_message_pp(lop_message m)
{
    void *d = m->data;
    void *end = (char*)m->data + m->datalen;
    int i;

    printf("%s ", m->types);
    for (i = 1; m->types[i]; i++) {
	if (i > 1) {
	    printf(" ");
	}

	lop_arg_pp_internal(m->types[i], d, 1);
	d = (char*)d + lop_arg_size(m->types[i], d);
    }
    putchar('\n');
    if (d != end) {
	fprintf(stderr, "liblo warning: type and data do not match (off by %d) in message %p\n",
            abs((char*)d - (char*)end), m);
    }
}

void lop_arg_pp(lop_type type, void *data)
{
    lop_arg_pp_internal(type, data, 0);
}

void lop_arg_pp_internal(lop_type type, void *data, int bigendian)
{
    lop_pcast32 val32;
    lop_pcast64 val64;
    int size;
    int i;

    size = lop_arg_size(type, data);
    if (size == 4 || type == LOP_BLOB) {
	if (bigendian) {
	    val32.nl = lop_otoh32(*(int32_t *)data);
	} else {
	    val32.nl = *(int32_t *)data;
	}
    } else if (size == 8) {
	if (bigendian) {
	    val64.nl = lop_otoh64(*(int64_t *)data);
	} else {
	    val64.nl = *(int64_t *)data;
	}
    }

    switch (type) {
    case LOP_INT32:
	printf("%d", val32.i);
	break;
    
    case LOP_FLOAT:
	printf("%f", val32.f);
	break;

    case LOP_STRING:
	printf("\"%s\"", (char *)data);
	break;

    case LOP_BLOB:
	printf("[");
	if (val32.i > 12) {
	    printf("%d byte blob", val32.i);
	} else {
	    printf("%db ", val32.i);
	    for (i=0; i<val32.i; i++) {
		printf("0x%02x", *((char *)(data) + 4 + i));
		if (i+1 < val32.i) printf(" ");
	    }
	}
	printf("]");
	break;

    case LOP_INT64:
	printf("%lld", (long long int)val64.i);
	break;
    
    case LOP_TIMETAG:
	printf("%08x.%08x", val64.tt.sec, val64.tt.frac);
	break;
    
    case LOP_DOUBLE:
	printf("%f", val64.f);
	break;
    
    case LOP_SYMBOL:
	printf("'%s", (char *)data);
	break;

    case LOP_CHAR:
	printf("'%c'", (char)val32.c);
	break;

    case LOP_MIDI:
	printf("MIDI [");
	for (i=0; i<4; i++) {
	    printf("0x%02x", *((uint8_t *)(data) + i));
	    if (i+1 < 4) printf(" ");
	}
	printf("]");
	break;

    case LOP_TRUE:
	printf("#T");
	break;

    case LOP_FALSE:
	printf("#F");
	break;

    case LOP_NIL:
	printf("Nil");
	break;

    case LOP_INFINITUM:
	printf("Infinitum");
	break;

    default:
	fprintf(stderr, "liblo warning: unhandled type: %c\n", type);
	break;
    }
}

int lop_is_numerical_type(lop_type a)
{
    return strchr(lop_numerical_types, a) != 0;
}

int lop_is_string_type(lop_type a)
{
    return strchr(lop_string_types, a) != 0;
}

int lop_coerce(lop_type type_to, lop_arg *to, lop_type type_from, lop_arg *from)
{
    if (type_to == type_from) {
	memcpy(to, from, lop_arg_size(type_from, from));

	return 1;
    }

    if (lop_is_string_type(type_to) && lop_is_string_type(type_from)) {
	strcpy((char *)to, (char *)from);

	return 1;
    }

    if (lop_is_numerical_type(type_to) && lop_is_numerical_type(type_from)) {
	switch (type_to) {
	case LOP_INT32:
	    to->i = (uint32_t)lop_hires_val(type_from, from);
	    break;

	case LOP_INT64:
	    to->i64 = (uint64_t)lop_hires_val(type_from, from);
	    break;

	case LOP_FLOAT:
	    to->f = (float)lop_hires_val(type_from, from);
	    break;

	case LOP_DOUBLE:
	    to->d = (double)lop_hires_val(type_from, from);
	    break;

	default:
	    fprintf(stderr, "liblo: bad coercion: %c -> %c\n", type_from,
		    type_to);
	    return 0;
	}
	return 1;
    }

    return 0;
}

lop_hires lop_hires_val(lop_type type, lop_arg *p)
{
    switch (type) {
    case LOP_INT32:
	return p->i;
    case LOP_INT64:
	return p->h;
    case LOP_FLOAT:
	return p->f;
    case LOP_DOUBLE:
	return p->d;
    default:
	fprintf(stderr, "liblo: hires val requested of non numerical type '%c' at %s:%d\n", type, __FILE__, __LINE__);
	break;
    }

    return 0.0l;
}



/* vi:set ts=8 sts=4 sw=4: */
