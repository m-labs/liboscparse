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

#include "lop_types_internal.h"

#include <sys/time.h>
#include <time.h>

#define JAN_1970 0x83aa7e80      /* 2208988800 1970 - 1900 in seconds */

double lop_timetag_diff(lop_timetag a, lop_timetag b)
{
	return (double)a.sec - (double)b.sec +
		((double)a.frac - (double)b.frac) * 0.00000000023283064365;
}

void lop_timetag_now(lop_timetag *t)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	t->sec = tv.tv_sec + JAN_1970;
	t->frac = tv.tv_usec * 4294.967295;
}
