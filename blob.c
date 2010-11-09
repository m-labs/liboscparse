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
#include <string.h>

#include "lop_types_internal.h"
#include "lop/lop_lowlevel.h"

lop_blob lop_blob_new(int32_t size, const void *data)
{
    lop_blob b;

    if (size < 1) {
	return NULL;
    }

    b = malloc(sizeof(size) + size);

    b->size = size;

    if (data) {
	memcpy((char*)b + sizeof(uint32_t), data, size);
    }

    return b;
}

void lop_blob_free(lop_blob b)
{
    free(b);
}

uint32_t lop_blob_datasize(lop_blob b)
{
    return b->size;
}

void *lop_blob_dataptr(lop_blob b)
{
    return (char*)b + sizeof(uint32_t);
}

uint32_t lop_blobsize(lop_blob b)
{
    const uint32_t len = sizeof(uint32_t) + b->size;

    return 4 * (len / 4 + 1);
}

/* vi:set ts=8 sts=4 sw=4: */
