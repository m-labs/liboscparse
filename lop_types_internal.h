#ifndef LOP_TYPES_H
#define LOP_TYPES_H

#include <sys/types.h>

#include "lop/lop_osc_types.h"

typedef void (*lop_err_handler)(int num, const char *msg, const char *path);

struct _lop_method;

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
	lop_method                first;
	lop_err_handler           err_h;
	int	 	         port;
	char                   	*hostname;
	char                   	*path;
	int            	         protocol;
	void		        *queued;
} *lop_server;

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

#endif
