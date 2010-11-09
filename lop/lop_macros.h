/*
 *  Copyright (C) 2004 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#ifndef LOP_MACROS_H
#define LOP_MACROS_H

/* macros that have to be defined after function signatures */

#ifdef __cplusplus
extern "C" {
#endif

/* \brief Maximum length of UDP messages in bytes
 */
#define LOP_MAX_MSG_SIZE 32768

/* \brief A set of macros to represent different communications transports
 */
#define LOP_DEFAULT 0x0
#define LOP_UDP     0x1
#define LOP_UNIX    0x2
#define LOP_TCP     0x4

/* an internal value, ignored in transmission but check against LOP_MARKER in the
 * argument list. Used to do primitive bounds checking */
#define LOP_MARKER_A 0xdeadbeef
#define LOP_MARKER_B 0xf00baa23
#define LOP_ARGS_END LOP_MARKER_A, LOP_MARKER_B

#define lop_message_add_varargs(msg, types, list) \
    lop_message_add_varargs_internal(msg, types, list, __FILE__, __LINE__)

#ifdef __GNUC__

#define lop_message_add(msg, types...)                         \
    lop_message_add_internal(msg, __FILE__, __LINE__, types,   \
                            LOP_MARKER_A, LOP_MARKER_B)

#else

/* In non-GCC compilers, there is no support for variable-argument
 * macros, so provide "internal" vararg functions directly instead. */

int lop_message_add(lop_message msg, const char *types, ...);

#endif

#ifdef __cplusplus
}
#endif

#endif
