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
    struct jksn_t *children;
} jksn_array;

typedef struct {
    struct jksn_t *key;
    struct jksn_t *value;
} jksn_keyvalue;

typedef struct {
    size_t size;
    jksn_keyvalue *children;
} jksn_object;

typedef struct jksn_t {
    enum {
        JKSN_UNDEF,
        JKSN_NULL,
        JKSN_BOOL,
        JKSN_INT,
        JKSN_FLOAT,
        JKSN_DOUBLE,
        JKSN_LONG_DOUBLE,
        JKSN_STRING,
        JKSN_BLOB,
        JKSN_ARRAY,
        JKSN_OBJECT
    } data_type;
    union {
        int data_bool;
        int64_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        jksn_utf8string data_string;
        jksn_blobstring data_blob;
        jksn_array data_array;
        jksn_object data_object;
    };
} jksn_t;

int jksn_dump(jksn_blobstring **result, const jksn_t *object, int header);
int jksn_parse(jksn_t **result, const jksn_blobstring *buffer);
jksn_t *jksn_free(jksn_t *object);
const char *jksn_errcode(int errcode);

#endif
