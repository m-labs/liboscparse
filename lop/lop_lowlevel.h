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

#ifndef LOP_LOWLEVEL_H
#define LOP_LOWLEVEL_H

#include "lop/lop_osc_types.h"

/**
 * \file lop_lowlevel.h The lop headerfile defining the low-level API
 * functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdint.h>

#include "lop/lop_types.h"
#include "lop/lop_errors.h"

/**
 * \defgroup loplowlevel Low-level OSC API
 *
 * Use these functions if you require more precise control over OSC message
 * contruction or handling that what is provided in the high-level functions
 * described in lop.
 * @{
 */

/**
 * \brief Create a new lop_message object
 */
lop_message lop_message_new(void);

/**
 * \brief Free memory allocated by lop_message_new() and any subsequent
 * \ref lop_message_add_int32 lop_message_add*() calls.
 */
void lop_message_free(lop_message m);

/**
 * \brief Append a number of arguments to a message.
 *
 * The data will be added in OSC byteorder (bigendian).
 *
 * \param m The message to be extended.
 * \param types The types of the data items in the message, types are defined in
 * lop_types_common.h
 * \param ... The data values to be transmitted. The types of the arguments
 * passed here must agree with the types specified in the type parameter.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add(lop_message m, const char *types, ...);

/** \internal \brief the real message_add function (don't call directly) */
int lop_message_add_internal(lop_message m,  const char *file, const int line,
                            const char *types, ...);

/**
 * \brief Append a varargs list to a message.
 *
 * The data will be added in OSC byteorder (bigendian).
 * IMPORTANT: args list must be terminated with LOP_ARGS_END, or this call will
 * fail.  This is used to do simple error checking on the sizes of parameters
 * passed.
 *
 * \param m The message to be extended.
 * \param types The types of the data items in the message, types are defined in
 * lop_types_common.h
 * \param ap The va_list created by a C function declared with an
 * ellipsis (...) argument, and pre-initialised with
 * "va_start(ap)". The types of the arguments passed here must agree
 * with the types specified in the type parameter.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_varargs(lop_message m, const char *types, va_list ap);

/** \internal \brief the real message_add_varargs function (don't call directly) */
int lop_message_add_varargs_internal(lop_message m, const char *types, va_list ap,
                                    const char *file, const int line);

/**
 * \brief Append a data item and typechar of the specified type to a message.
 *
 * The data will be added in OSC byteorder (bigendian).
 *
 * \param m The message to be extended.
 * \param a The data item.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_int32(lop_message m, int32_t a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_float(lop_message m, float a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_string(lop_message m, const char *a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_blob(lop_message m, lop_blob a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_int64(lop_message m, int64_t a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_timetag(lop_message m, lop_timetag a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_double(lop_message m, double a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_symbol(lop_message m, const char *a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_char(lop_message m, char a);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_midi(lop_message m, uint8_t a[4]);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_true(lop_message m);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_false(lop_message m);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_nil(lop_message m);

/**
 * \brief  Append a data item and typechar of the specified type to a message.
 * See lop_message_add_int32() for details.
 *
 * \return Less than 0 on failure, 0 on success.
 */
int lop_message_add_infinitum(lop_message m);

/**
 * \brief Type used to represent numerical values in conversions between OSC
 * types.
 */
typedef long double lop_hires;

/**
 * \brief  Returns the timestamp (lop_timetag *) of a bundled incoming message.
 *
 * Returns LOP_TT_IMMEDIATE if the message is outgoing, or did not arrive
 * contained in a bundle. Do not free the returned timetag.
 */
lop_timetag lop_message_get_timestamp(lop_message m);

/**
 * \brief  Return the message type tag string.
 *
 * The result is valid until further data is added with lop_message_add*().
 */
char *lop_message_get_types(lop_message m);

/**
 * \brief  Return the message argument count.
 *
 * The result is valid until further data is added with lop_message_add*().
 */
int lop_message_get_argc(lop_message m);

/**
 * \brief  Return the message arguments. Do not free the returned data.
 *
 * The result is valid until further data is added with lop_message_add*().
 */
lop_arg **lop_message_get_argv(lop_message m);

/**
 * \brief  Return the length of a message in bytes.
 *
 * \param m The message to be sized
 * \param path The path the message will be sent to
 */
size_t lop_message_length(lop_message m, const char *path);

/**
 * \brief  Serialise the lop_message object to an area of memory and return a
 * pointer to the serialised form.  Opposite of lop_message_deserialise().
 *
 * \param m The message to be serialised
 * \param path The path the message will be sent to
 * \param to The address to serialise to, memory will be allocated if to is
 * NULL.
 * \param size If this pointer is non-NULL the size of the memory area
 * will be written here
 *
 * The returned form is suitable to be sent over a low level OSC transport,
 * having the correct endianess and bit-packed structure.
 */
void *lop_message_serialise(lop_message m, const char *path, void *to,
    size_t *size);

/**
 * \brief  Deserialise a raw OSC message and return a new lop_message object.
 * Opposite of lop_message_serialise().
 *
 * \param data Pointer to the raw OSC message data in network transmission form
 * (network byte order where appropriate).
 * \param size The size of data in bytes
 * \param result If this pointer is non-NULL, the result or error code will
 * be written here.
 *
 * Returns a new lop_message, or NULL if deserialisation fails.
 * Use lop_message_free() to free the resulting object.
 */
lop_message lop_message_deserialise(void *data, size_t size, int *result);

/**
 * \brief  Dispatch a raw block of memory containing an OSC message.
 *
 * This is useful when a raw block of memory is available that is
 * structured as OSC, and you wish to use lop to dispatch the
 * message to a handler function as if it had been received over the
 * network.
 *
 * \param s The lop_server to use for dispatching.
 * \param data Pointer to the raw OSC message data in network transmission form
 * (network byte order where appropriate).
 * \param size The size of data in bytes
 *
 * Returns the number of bytes used if successful, or less than 0 otherwise.
 */
int lop_server_dispatch_data(lop_server s, void *data, size_t size);

/**
 * \brief Return true if the type specified has a numerical value, such as
 * LOP_INT32, LOP_FLOAT etc.
 *
 * \param a The type to be tested.
 */
int lop_is_numerical_type(lop_type a);

/**
 * \brief Return true if the type specified has a textual value, such as
 * LOP_STRING or LOP_SYMBOL.
 *
 * \param a The type to be tested.
 */
int lop_is_string_type(lop_type a);

/**
 * \brief Attempt to convert one OSC type to another.
 *
 * Numerical types (eg LOP_INT32, LOP_FLOAT etc.) may be converted to other
 * numerical types and string types (LOP_STRING and LOP_SYMBOL) may be converted
 * to the other type. This is done automatically if a received message matches
 * the path, but not the exact types, and is coercible (ie. all numerical
 * types in numerical positions).
 *
 * On failure no translation occurs and false is returned.
 *
 * \param type_to   The type of the destination variable.
 * \param to        A pointer to the destination variable.
 * \param type_from The type of the source variable.
 * \param from      A pointer to the source variable.
 */
int lop_coerce(lop_type type_to, lop_arg *to, lop_type type_from, lop_arg *from);

/**
 * \brief Return the numerical value of the given argument with the
 * maximum native system precision.
 */
lop_hires lop_hires_val(lop_type type, lop_arg *p);

/**
 * \brief Create a new server instance.
 *
 * Using lop_server_recv(), lop_servers block until they receive OSC
 * messages.  If you want non-blocking behaviour see
 * lop_server_recv_noblock() or the \ref lop_server_thread_new
 * "lop_server_thread_*" functions.
 *
 * \param err_h An error callback function that will be called if there is an
 * error in messge reception or server creation. Pass NULL if you do not want
 * error handling.
 */
lop_server lop_server_new(lop_err_handler err_h, lop_send_handler send_h, void *send_h_arg);

/**
 * \brief Free up memory used by the lop_server object
 */
void lop_server_free(lop_server s);

/**
 * \brief Add an OSC method to the specifed server.
 *
 * \param s The server the method is to be added to.
 * \param path The OSC path to register the method to. If NULL is passed the
 * method will match all paths.
 * \param typespec The typespec the method accepts. Incoming messages with
 * similar typespecs (e.g. ones with numerical types in the same position) will
 * be coerced to the typespec given here.
 * \param h The method handler callback function that will be called if a
 * matching message is received
 * \param user_data A value that will be passed to the callback function, h,
 * when its invoked matching from this method.
 */
lop_method lop_server_add_method(lop_server s, const char *path,
                               const char *typespec, lop_method_handler h,
                               void *user_data);

/**
 * \brief Delete an OSC method from the specifed server.
 *
 * \param s The server the method is to be removed from.
 * \param path The OSC path of the method to delete. If NULL is passed the
 * method will match the generic handler.
 * \param typespec The typespec the method accepts.
 */
void lop_server_del_method(lop_server s, const char *path,
                               const char *typespec);
/** 
 * \brief Return true if there are scheduled events (eg. from bundles) 
 * waiting to be dispatched by the server
 */
int lop_server_events_pending(lop_server s);

/** 
 * \brief Return the time in seconds until the next scheduled event.
 *
 * If the delay is greater than 100 seconds then it will return 100.0.
 */
double lop_server_next_event_delay(lop_server s);

/* utility functions */

/**
 * \brief A function to calculate the amount of OSC message space required by a
 * C char *.
 *
 * Returns the storage size in bytes, which will always be a multiple of four.
 */
int lop_strsize(const char *s);

/**
 * \brief A function to calculate the amount of OSC message space required by a
 * lop_blob object.
 *
 * Returns the storage size in bytes, which will always be a multiple of four.
 */
uint32_t lop_blobsize(lop_blob b);

/**
 * \brief Test a string against an OSC pattern glob
 *
 * \param str The string to test
 * \param p   The pattern to test against
 */
int lop_pattern_match(const char *str, const char *p);


/** \brief Find the time difference between two timetags
 *
 * Returns a - b in seconds.
 */
double lop_timetag_diff(lop_timetag a, lop_timetag b);

/** \brief Return a timetag for the current time
 *
 * On exit the timetag pointed to by t is filled with the OSC
 * representation of this instant in time.
 */
void lop_timetag_now(lop_timetag *t);

/**
 * \brief Return the storage size, in bytes, of the given argument.
 */
size_t lop_arg_size(lop_type type, void *data);

/**
 * \brief Given a raw OSC message, return the message path.
 *
 * \param data      A pointer to the raw OSC message data.
 * \param size      The size of data in bytes (total buffer bytes).
 *
 * Returns the message path or NULL if an error occurs.
 * Do not free() the returned pointer.
 */
char *lop_get_path(void *data, ssize_t size);

/**
 * \brief Convert the specified argument to host byte order where necessary.
 *
 * \param type The OSC type of the data item (eg. LOP_FLOAT).
 * \param data A pointer to the data item to be converted. It is changed
 * in-place.
 */
void lop_arg_host_endian(lop_type type, void *data);

/**
 * \brief Convert the specified argument to network byte order where necessary.
 *
 * \param type The OSC type of the data item (eg. LOP_FLOAT).
 * \param data A pointer to the data item to be converted. It is changed
 * in-place.
 */
void lop_arg_network_endian(lop_type type, void *data);

/** @} */

/* prettyprinters */

/**
 * \defgroup pp Prettyprinting functions
 *
 * These functions all print an ASCII representation of their argument to
 * stdout. Useful for debugging.
 * @{
 */

/** \brief Pretty-print a lop_message object. */
void lop_message_pp(lop_message m);

/** \brief Pretty-print a set of typed arguments.
 * \param type A type string.
 * \param data An OSC data pointer, like that provided in the
 * lop_method_handler.
 */
void lop_arg_pp(lop_type type, void *data);

/** \brief Pretty-print a lop_server object. */
void lop_server_pp(lop_server s);

/** \brief Pretty-print a lop_method object. */
void lop_method_pp(lop_method m);

/** \brief Pretty-print a lop_method object, but prepend a given prefix
 * to all field names. */
void lop_method_pp_prefix(lop_method m, const char *p);

/**
 * \brief Create a new OSC blob type.
 *
 * \param size The amount of space to allocate in the blob structure.
 * \param data The data that will be used to initialise the blob, should be
 * size bytes long.
 */
lop_blob lop_blob_new(int32_t size, const void *data);

/**
 * \brief Free the memory taken by a blob
 */
void lop_blob_free(lop_blob b);

/**
 * \brief Return the amount of valid data in a lop_blob object.
 *
 * If you want to know the storage size, use lop_arg_size().
 */
uint32_t lop_blob_datasize(lop_blob b);

/**
 * \brief Return a pointer to the start of the blob data to allow contents to
 * be changed.
 */
void *lop_blob_dataptr(lop_blob b);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
