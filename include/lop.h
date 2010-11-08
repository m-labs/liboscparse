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

#ifndef LOP_H
#define LOP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file lo.h The liblo main headerfile and high-level API functions.
 */

#include "lo/lop_endian.h"
#include "lo/lop_types.h"
#include "lo/lop_osc_types.h"
#include "lo/lop_errors.h"
#include "lo/lop_lowlevel.h"

/**
 * \defgroup liblo High-level OSC API
 *
 * Defines the high-level API functions necessary to implement OSC support.
 * Should be adequate for most applications, but if you require lower level
 * control you can use the functions defined in lop_lowlevel.h
 * @{
 */

/**
 * \brief Declare an OSC destination, given IP address and port number.
 * Same as lop_address_new_with_proto(), but using UDP.
 *
 * \param host An IP address or number, or NULL for the local machine.
 * \param port a decimal port number or service name.
 *
 * The lop_address object may be used as the target of OSC messages.
 *
 * Note: if you wish to receive replies from the target of this address, you
 * must first create a lop_server_thread or lop_server object which will receive
 * the replies. The last lop_server(_thread) object creted will be the receiver.
 */
lop_address lop_address_new(const char *host, const char *port);

/**
 * \brief Declare an OSC destination, given IP address and port number,
 * specifying protocol.
 *
 * \param proto The protocol to use, must be one of LOP_UDP, LOP_TCP or LOP_UNIX.
 * \param host An IP address or number, or NULL for the local machine.
 * \param port a decimal port number or service name.
 *
 * The lop_address object may be used as the target of OSC messages.
 *
 * Note: if you wish to receive replies from the target of this address, you
 * must first create a lop_server_thread or lop_server object which will receive
 * the replies. The last lop_server(_thread) object creted will be the receiver.
 */
lop_address lop_address_new_with_proto(int proto, const char *host, const char *port);

/**
 * \brief Create a lop_address object from an OSC URL.
 *
 * example: \c "osc.udp://localhost:4444/my/path/"
 */
lop_address lop_address_new_from_url(const char *url);

/**
 * \brief Free the memory used by the lop_address object
 */ 
void lop_address_free(lop_address t);

/**
 * \brief Set the Time-to-Live value for a given target address.
 * 
 * This is required for sending multicast UDP messages.  A value of 1
 * (the usual case) keeps the message within the subnet, while 255
 * means a global, unrestricted scope.
 * 
 * \param t An OSC address.
 * \param ttl An integer specifying the scope of a multicast UDP message.
 */ 
void lop_address_set_ttl(lop_address t, int ttl);

/**
 * \brief Get the Time-to-Live value for a given target address.
 * 
 * \param t An OSC address.
 * \return An integer specifying the scope of a multicast UDP message.
 */ 
int lop_address_get_ttl(lop_address t);

/**
 * \brief Send a OSC formatted message to the address specified.
 *
 * \param targ The target OSC address
 * \param path The OSC path the message will be delivered to
 * \param type The types of the data items in the message, types are defined in
 * lop_osc_types.h
 * \param ... The data values to be transmitted. The types of the arguments
 * passed here must agree with the types specified in the type parameter.
 *
 * example:
 * \code
 * lop_send(t, "/foo/bar", "ff", 0.1f, 23.0f);
 * \endcode
 *
 * \return -1 on failure.
 */
int lop_send(lop_address targ, const char *path, const char *type, ...);

/**
 * \brief Send a OSC formatted message to the address specified, 
 * from the same socket as the specificied server.
 *
 * \param targ The target OSC address
 * \param from The server to send message from   (can be NULL to use new socket)
 * \param ts   The OSC timetag timestamp at which the message will be processed 
 * (can be LOP_TT_IMMEDIATE if you don't want to attach a timetag)
 * \param path The OSC path the message will be delivered to
 * \param type The types of the data items in the message, types are defined in
 * lop_osc_types.h
 * \param ... The data values to be transmitted. The types of the arguments
 * passed here must agree with the types specified in the type parameter.
 *
 * example:
 * \code
 * serv = lop_server_new(NULL, err);
 * lop_server_add_method(serv, "/reply", "ss", reply_handler, NULL);
 * lop_send_from(t, serv, LOP_TT_IMMEDIATE, "/foo/bar", "ff", 0.1f, 23.0f);
 * \endcode
 *
 * \return on success, the number of bytes sent, or -1 on failure.
 */
int lop_send_from(lop_address targ, lop_server from, lop_timetag ts, 
	       		const char *path, const char *type, ...);

/**
 * \brief Send a OSC formatted message to the address specified, scheduled to
 * be dispatch at some time in the future.
 *
 * \param targ The target OSC address
 * \param ts The OSC timetag timestamp at which the message will be processed
 * \param path The OSC path the message will be delivered to
 * \param type The types of the data items in the message, types are defined in
 * lop_osc_types.h
 * \param ... The data values to be transmitted. The types of the arguments
 * passed here must agree with the types specified in the type parameter.
 *
 * example:
 * \code
 * lop_timetag now;<br>
 * lop_timetag_now(&now);<br>
 * lop_send_timestamped(t, now, "/foo/bar", "ff", 0.1f, 23.0f);
 * \endcode
 *
 * \return on success, the number of bytes sent, or -1 on failure.
 */
int lop_send_timestamped(lop_address targ, lop_timetag ts, const char *path,
	       		const char *type, ...);

/**
 * \brief Return the error number from the last failed lop_send() or
 * lop_address_new() call
 */
int lop_address_errno(lop_address a);

/**
 * \brief Return the error string from the last failed lop_send() or
 * lop_address_new() call
 */
const char *lop_address_errstr(lop_address a);

/**
 * \brief Create a new server thread to handle incoming OSC
 * messages.
 *
 * Server threads take care of the message reception and dispatch by
 * transparently creating a system thread to handle incoming messages.
 * Use this if you do not want to handle the threading yourself.
 *
 * \param port If NULL is passed then an unused port will be chosen by the
 * system, its number may be retrieved with lop_server_thread_get_port()
 * so it can be passed to clients. Otherwise a decimal port number, service
 * name or UNIX domain socket path may be passed.
 * \param err_h A function that will be called in the event of an error being
 * raised. The function prototype is defined in lop_types.h
 */
lop_server_thread lop_server_thread_new(const char *port, lop_err_handler err_h);

/**
 * \brief Create a new server thread to handle incoming OSC
 * messages, and join a UDP multicast group.
 *
 * Server threads take care of the message reception and dispatch by
 * transparently creating a system thread to handle incoming messages.
 * Use this if you do not want to handle the threading yourself.
 *
 * \param group The multicast group to join.  See documentation on IP
 * multicast for the acceptable address range; e.g., http://tldp.org/HOWTO/Multicast-HOWTO-2.html
 * \param port If NULL is passed then an unused port will be chosen by the
 * system, its number may be retrieved with lop_server_thread_get_port()
 * so it can be passed to clients. Otherwise a decimal port number, service
 * name or UNIX domain socket path may be passed.
 * \param err_h A function that will be called in the event of an error being
 * raised. The function prototype is defined in lop_types.h
 */
lop_server_thread lop_server_thread_new_multicast(const char *group, const char *port,
                                                lop_err_handler err_h);

/**
 * \brief Create a new server thread to handle incoming OSC
 * messages, specifying protocol.
 *
 * Server threads take care of the message reception and dispatch by
 * transparently creating a system thread to handle incoming messages.
 * Use this if you do not want to handle the threading yourself.
 *
 * \param port If NULL is passed then an unused port will be chosen by the
 * system, its number may be retrieved with lop_server_thread_get_port()
 * so it can be passed to clients. Otherwise a decimal port number, service
 * name or UNIX domain socket path may be passed.
 * \param proto The protocol to use, should be one of LOP_UDP, LOP_TCP or LOP_UNIX.
 * \param err_h A function that will be called in the event of an error being
 * raised. The function prototype is defined in lop_types.h
 */
lop_server_thread lop_server_thread_new_with_proto(const char *port, int proto,
				   lop_err_handler err_h);

/**
 * \brief Free memory taken by a server thread
 *
 * Frees the memory, and, if currently running will stop the associated thread.
 */
void lop_server_thread_free(lop_server_thread st);

/**
 * \brief Add an OSC method to the specifed server thread.
 *
 * \param st The server thread the method is to be added to.
 * \param path The OSC path to register the method to. If NULL is passed the
 * method will match all paths.
 * \param typespec The typespec the method accepts. Incoming messages with
 * similar typespecs (e.g. ones with numerical types in the same position) will
 * be coerced to the typespec given here.
 * \param h The method handler callback function that will be called it a
 * matching message is received
 * \param user_data A value that will be passed to the callback function, h,
 * when its invoked matching from this method.
 */
lop_method lop_server_thread_add_method(lop_server_thread st, const char *path,
                               const char *typespec, lop_method_handler h,
                               void *user_data);
/**
 * \brief Delete an OSC method from the specifed server thread.
 *
 * \param st The server thread the method is to be removed from.
 * \param path The OSC path of the method to delete. If NULL is passed the
 * method will match the generic handler.
 * \param typespec The typespec the method accepts.
 */
void lop_server_thread_del_method(lop_server_thread st, const char *path,
				 const char *typespec);

/**
 * \brief Start the server thread
 *
 * \param st the server thread to start.
 * \return Less than 0 on failure, 0 on success.
 */
int lop_server_thread_start(lop_server_thread st);

/**
 * \brief Stop the server thread
 *
 * \param st the server thread to start.
 * \return Less than 0 on failure, 0 on success.
 */
int lop_server_thread_stop(lop_server_thread st);

/**
 * \brief Return the port number that the server thread has bound to.
 */
int lop_server_thread_get_port(lop_server_thread st);

/**
 * \brief Return a URL describing the address of the server thread.
 *
 * Return value must be free()'d to reclaim memory.
 */
char *lop_server_thread_get_url(lop_server_thread st);

/**
 * \brief Return the lop_server for a lop_server_thread
 *
 * This function is useful for passing a thread's lop_server 
 * to lop_send_from().
 */
lop_server lop_server_thread_get_server(lop_server_thread st);

/** \brief Return true if there are scheduled events (eg. from bundles) waiting
 * to be dispatched by the thread */
int lop_server_thread_events_pending(lop_server_thread st);

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

#include "lo/lop_macros.h"

#ifdef __cplusplus
}
#endif

#endif
