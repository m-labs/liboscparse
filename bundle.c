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

#include "lop_types_internal.h"
#include "lo/lo.h"

lop_bundle lop_bundle_new(lop_timetag tt)
{
    lop_bundle b = calloc(1, sizeof(struct _lop_bundle));

    b->size = 4;
    b->len = 0;
    b->ts = tt;
    b->msgs = calloc(b->size, sizeof(lop_message));
    b->paths = calloc(b->size, sizeof(char *));

    return b;
}

int lop_bundle_add_message(lop_bundle b, const char *path, lop_message m)
{
    if (!m)
    	return 0;

    if (b->len >= b->size) {
	b->size *= 2;
	b->msgs = realloc(b->msgs, b->size * sizeof(lop_message));
	b->paths = realloc(b->paths, b->size * sizeof(char *));
    if (!b->msgs || !b->paths)
        return -1;
    }

    b->msgs[b->len] = m;
    b->paths[b->len] = (char *)path;

    (b->len)++;
    return 0;
}

size_t lop_bundle_length(lop_bundle b)
{
    size_t size = 16; /* "#bundle" and the timetag */
    int i;

    if (!b) {
	return 0;
    }

    size += b->len * 4; /* sizes */
    for (i = 0; i < b->len; i++) {
	size += lop_message_length(b->msgs[i], b->paths[i]);
    }

    return size;
}

void *lop_bundle_serialise(lop_bundle b, void *to, size_t *size)
{
    size_t s, skip;
    int32_t *bes;
    int i;
    char *pos;
    lop_pcast32 be;

    if (!b) {
	return NULL;
    }

    s = lop_bundle_length(b);
    if (size) {
	*size = s;
    }

    if (!to) {
	to = calloc(1, s);
    }

    pos = to;
    strcpy(pos, "#bundle");
    pos += 8;
	
    be.nl = lop_htoo32(b->ts.sec);
    memcpy(pos, &be, 4);
    pos += 4;
    be.nl = lop_htoo32(b->ts.frac);
    memcpy(pos, &be, 4);
    pos += 4;

    for (i = 0; i < b->len; i++) {
	lop_message_serialise(b->msgs[i], b->paths[i], pos + 4, &skip);
	bes = (int32_t *)pos;
	*bes = lop_htoo32(skip);
	pos += skip + 4;

	if (pos > (char *)to + s) {
	    fprintf(stderr, "liblo: data integrity error at message %d\n", i);

	    return NULL;
	}
    }
    if (pos != (char*)to + s) {
	fprintf(stderr, "liblo: data integrity error\n");

	return NULL;
    }

    return to;
}

void lop_bundle_free(lop_bundle b)
{
    if (!b) {
	return;
    }

    free(b->msgs);
    free(b->paths);
    free(b);
}

static int _lop_internal_compare_ptrs( const void* a, const void* b )
{
    if (*(void**)a <  *(void**)b) return -1;
    if (*(void**)a == *(void**)b) return 0;
    return 1;
}

void lop_bundle_free_messages(lop_bundle b)
{
    int i;
    lop_message tmp = 0;

    if (!b)
        return;

    /* avoid freeing the same message twice */
    if (b->len > 2)
        qsort(b->msgs, b->len, sizeof(lop_message*), _lop_internal_compare_ptrs);

    for(i = 0; i < b->len; i++) {
        if (b->msgs[i] != tmp) {
            tmp = b->msgs[i];
            lop_message_free(b->msgs[i]);
        }
    }
    free(b->msgs);
    free(b->paths);
    free(b);
}

void lop_bundle_pp(lop_bundle b)
{
    int i;

    if (!b) return;

    printf("bundle(%f):\n", (double)b->ts.sec + b->ts.frac / 4294967296.0);
    for (i = 0; i < b->len; i++) {
	lop_message_pp(b->msgs[i]);
    }
    printf("\n");
}

/* vi:set ts=8 sts=4 sw=4: */
