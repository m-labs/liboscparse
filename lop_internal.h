#ifndef LOP_INTERNAL_H
#define LOP_INTERNAL_H

#include <lop/lop_osc_types.h>

/**
 * \brief Validate raw OSC string data. Where applicable, data should be
 * in network byte order.
 *
 * This function is used internally to parse and validate raw OSC data.
 *
 * Returns length of string or < 0 if data is invalid.
 *
 * \param data      A pointer to the data.
 * \param size      The size of data in bytes (total bytes remaining).
 */
ssize_t lop_validate_string(void *data, ssize_t size);

/**
 * \brief Validate raw OSC blob data. Where applicable, data should be
 * in network byte order.
 *
 * This function is used internally to parse and validate raw OSC data.
 *
 * Returns length of blob or < 0 if data is invalid.
 *
 * \param data      A pointer to the data.
 * \param size      The size of data in bytes (total bytes remaining).
 */
ssize_t lop_validate_blob(void *data, ssize_t size);

/**
 * \brief Validate raw OSC bundle data. Where applicable, data should be
 * in network byte order.
 *
 * This function is used internally to parse and validate raw OSC data.
 *
 * Returns length of bundle or < 0 if data is invalid.
 *
 * \param data      A pointer to the data.
 * \param size      The size of data in bytes (total bytes remaining).
 */
ssize_t lop_validate_bundle(void *data, ssize_t size);

/**
 * \brief Validate raw OSC argument data. Where applicable, data should be
 * in network byte order.
 *
 * This function is used internally to parse and validate raw OSC data.
 *
 * Returns length of argument data or < 0 if data is invalid.
 *
 * \param type      The OSC type of the data item (eg. LOP_FLOAT).
 * \param data      A pointer to the data.
 * \param size      The size of data in bytes (total bytes remaining).
 */
ssize_t lop_validate_arg(lop_type type, void *data, ssize_t size);

#endif
