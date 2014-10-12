/*
  Copyright (c) 2014 StarBrilliant <m13253@hotmail.com>
  All rights reserved.

  Redistribution and use in source and binary forms are permitted
  provided that the above copyright notice and this paragraph are
  duplicated in all such forms and that any documentation,
  advertising materials, and other materials related to such
  distribution and use acknowledge that the software was developed by
  StarBrilliant.
  The name of StarBrilliant may not be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef _JKSN_H_INCLUDED
#define _JKSN_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t size;
    char *str;
} jksn_utf8string;

typedef struct {
    size_t size;
    char *buf;
} jksn_blobstring;

typedef struct {
    size_t size;
    struct jksn_t **children;
} jksn_array;

typedef struct {
    struct jksn_t *key;
    struct jksn_t *value;
} jksn_keyvalue;

typedef struct {
    size_t size;
    jksn_keyvalue *children;
} jksn_object;

typedef enum {
    JKSN_UNDEFINED,
    JKSN_NULL,
    JKSN_BOOL,
    JKSN_INT,
    JKSN_FLOAT,
    JKSN_DOUBLE,
    JKSN_LONG_DOUBLE,
    JKSN_STRING,
    JKSN_BLOB,
    JKSN_ARRAY,
    JKSN_OBJECT,
    JKSN_UNSPECIFIED
} jksn_data_type;

typedef struct jksn_t {
    jksn_data_type data_type;
    union {
        int data_bool;
        intmax_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        jksn_utf8string data_string;
        jksn_blobstring data_blob;
        jksn_array data_array;
        jksn_object data_object;
    };
} jksn_t;

typedef struct jksn_cache jksn_cache;

#ifdef __cplusplus
extern "C" {
#endif

jksn_cache *jksn_cache_new(void);
jksn_cache *jksn_cache_free(jksn_cache *cache);
int jksn_dump(jksn_blobstring **result, const jksn_t *object, /*bool*/ int header, jksn_cache *cache);
int jksn_parse(jksn_t **result, const jksn_blobstring *buffer, size_t *bytes_parsed, jksn_cache *cache);
jksn_t *jksn_free(jksn_t *object);
jksn_blobstring *jksn_blobstring_free(jksn_blobstring *blobstring);
const char *jksn_errcode(int errcode);

#ifdef __cplusplus
}
#endif

#endif
