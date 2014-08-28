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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define _JKSN_PRIVATE
#include "jksn.h"

typedef struct jksn_value {
    const jksn_t *origin;
    uint8_t control;
    jksn_blobstring data;
    jksn_blobstring buf;
    uint8_t hash;
    struct jksn_value *first_child;
    struct jksn_value *next_child;
} jksn_value;

struct jksn_cache {
    int haslastint;
    int64_t lastint;
    jksn_utf8string texthash[256];
    jksn_blobstring blobhash[256];
};

struct jksn_swap_columns {
    jksn_t *key;
    struct jksn_swap_columns *next;
};

static const char *jksn_error_messages[] = {
    "OK",
    "JKSNError: no enough memory",
    "JKSNDecodeError: JKSN stream may be truncated or corrupted",
    "JKSNEncodeError: cannot encode unrecognizable type of value",
    "JKSNDecodeError: JKSN stream contains an invalid control byte",
    "JKSNDecodeError: this JKSN decoder does not support JSON literals",
    "JKSNDecodeError: this build of JKSN decoder does not support variable length integers",
    "JKSNError: this build of JKSN decoder does not support long double numbers",
    "JKSNDecodeError: JKSN stream requires a non-existing hash",
    "JKSNDecodeError: JKSN stream contains an invalid delta encoded integer",
    "JKSNDecodeError: JKSN row-col swapped array requires an array but not found"
};
typedef enum {
    JKSN_EOK,
    JKSN_ENOMEM,
    JKSN_ETRUNC,
    JKSN_ETYPE,
    JKSN_ECONTROL,
    JKSN_EJSON,
    JKSN_EVARINT,
    JKSN_ELONGDOUBLE,
    JKSN_EHASH,
    JKSN_EDELTA,
    JKSN_ESWAPARRAY
} jksn_error_message_no;

static inline void *jksn_malloc(size_t size);
static inline void *jksn_calloc(size_t nmemb, size_t size);
static inline void *jksn_realloc(void *ptr, size_t size);
static jksn_value *jksn_value_new(const jksn_t *origin, uint8_t control, const jksn_blobstring *data, const jksn_blobstring *buf);
static jksn_value *jksn_value_free(jksn_value *object);
static size_t jksn_value_size(const jksn_value *object, int depth);
static char *jksn_value_output(char output[], const jksn_value *object);
static jksn_error_message_no jksn_dump_value(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_int(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_float(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_double(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_longdouble(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_string(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_blob(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_array(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_object(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static void jksn_optimize(jksn_value *object, jksn_cache *cache);
static size_t jksn_encode_int(char result[], uint64_t object, size_t size);
static struct jksn_swap_columns *jksn_swap_columns_free(struct jksn_swap_columns *columns);
static jksn_error_message_no jksn_parse_value(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed, jksn_cache *cache);
static jksn_error_message_no jksn_parse_float(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed);
static jksn_error_message_no jksn_parse_double(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed);
static jksn_error_message_no jksn_parse_longdouble(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed);
static jksn_error_message_no jksn_decode_int(uint64_t *result, const char *buffer, size_t bufsize, size_t size, size_t *bytes_parsed);
static size_t jksn_utf8_to_utf16(uint16_t *utf16str, const char *utf8str, size_t utf8size, int strict);
static size_t jksn_utf16_to_utf8(char *utf8str, const uint16_t *utf16str, size_t utf16size);
static int jksn_compare(const jksn_t *obj1, const jksn_t *obj2);
static jksn_t *jksn_duplicate(const jksn_t *object);
static uint8_t jksn_djbhash(const char *buf, size_t size);
static uint16_t jksn_uint16_to_le(uint16_t n);
static inline uint64_t jksn_int64abs(int64_t x) { return x >= 0 ? x : -x; };

static inline void *jksn_malloc(size_t size) {
    return malloc(size != 0 ? size : 1);
}

static inline void *jksn_calloc(size_t nmemb, size_t size) {
    if(nmemb != 0 && size != 0)
        return calloc(nmemb, size);
    else
        return malloc(1);
}

static inline void* jksn_realloc(void *ptr, size_t size) {
    return realloc(ptr, size != 0 ? size : 1);
}

jksn_cache *jksn_cache_new(void) {
    return jksn_calloc(1, sizeof (struct jksn_cache));
}

jksn_cache *jksn_cache_free(jksn_cache *cache) {
    if(cache) {
        size_t i;
        cache->haslastint = 0;
        for(i = 0; i < 256; i++)
            if(cache->texthash[i].str) {
                cache->texthash[i].size = 0;
                free(cache->texthash[i].str);
                cache->texthash[i].str = NULL;
            }
        for(i = 0; i < 256; i++)
            if(cache->blobhash[i].buf) {
                cache->blobhash[i].size = 0;
                free(cache->blobhash[i].buf);
                cache->blobhash[i].buf = NULL;
            }
        free(cache);
    }
    return NULL;
}

jksn_blobstring *jksn_blobstring_free(jksn_blobstring *blobstring) {
    if(blobstring) {
        blobstring->size = 0;
        free(blobstring->buf);
        blobstring->buf = NULL;
        free(blobstring);
    }
    return NULL;
}

jksn_t *jksn_free(jksn_t *object) {
    if(object) {
        size_t i;
        switch(object->data_type) {
        case JKSN_STRING:
            object->data_string.size = 0;
            free(object->data_string.str);
            object->data_string.str = NULL;
            break;
        case JKSN_BLOB:
            object->data_blob.size = 0;
            free(object->data_blob.buf);
            object->data_blob.buf = NULL;
            break;
        case JKSN_ARRAY:
            for(i = 0; i < object->data_array.size; i++)
                jksn_free(object->data_array.children[i]);
            for(i = 0; i < object->data_array.size; i++)
                object->data_array.children[i] = NULL;
            object->data_array.size = 0;
            free(object->data_array.children);
            object->data_array.children = NULL;
            break;
        case JKSN_OBJECT:
            for(i = 0; i < object->data_object.size; i++) {
                jksn_free(object->data_object.children[i].key);
                jksn_free(object->data_object.children[i].value);
            }
            for(i = 0; i < object->data_object.size; i++)
                object->data_object.children[i].value =
                    object->data_object.children[i].key = NULL;
            object->data_object.size = 0;
            free(object->data_object.children);
            object->data_object.children = NULL;
            break;
        default:
            break;
        }
        free(object);
    }
    return NULL;
}

static jksn_value *jksn_value_new(const jksn_t *origin, uint8_t control, const jksn_blobstring *data, const jksn_blobstring *buf) {
    jksn_value *result = jksn_calloc(1, sizeof (jksn_value));
    if(result) {
        result->origin = origin;
        result->control = control;
        if(data)
            result->data = *data;
        if(buf)
            result->buf = *buf;
    }
    return result;
}

static jksn_value *jksn_value_free(jksn_value *object) {
    while(object) {
        jksn_value *this_object = object;
        object->data.size = 0;
        free(object->data.buf);
        object->data.buf = NULL;
        object->buf.size = 0;
        free(object->buf.buf);
        object->buf.buf = NULL;
        if(object->first_child) {
            jksn_value_free(object->first_child);
            object->first_child = NULL;
        }
        object = object->next_child;
        free(this_object);
    }
    return NULL;
}

static size_t jksn_value_size(const jksn_value *object, int depth) {
    size_t result = 0;
    if(object) {
        result = 1 + object->data.size + object->buf.size;
        if(depth != 1)
            for(object = object->first_child; object; object = object->next_child)
                result += jksn_value_size(object, depth-1);
    }
    return result;
}

static char *jksn_value_output(char output[], const jksn_value *object) {
    while(object) {
        *(output++) = (char) object->control;
        if(object->data.size != 0) {
            memcpy(output, object->data.buf, object->data.size);
            output += object->data.size;
        }
        if(object->buf.size != 0) {
            memcpy(output, object->buf.buf, object->buf.size);
            output += object->buf.size;
        }
        if(object->first_child)
            output = jksn_value_output(output, object->first_child);
        object = object->next_child;
    }
    return output;
}

int jksn_dump(jksn_blobstring **result, const jksn_t *object, /*bool*/ int header, jksn_cache *cache_) {
    *result = NULL;
    if(!object)
        return JKSN_ETYPE;
    else {
        jksn_cache *cache = cache_ ? cache_ : jksn_cache_new();
        if(!cache)
            return JKSN_ENOMEM;
        else {
            jksn_value *result_value = NULL;
            jksn_error_message_no retval = jksn_dump_value(&result_value, object, cache);
            if(retval == JKSN_EOK) {
                jksn_optimize(result_value, cache);
                if(result) {
                    *result = jksn_malloc(sizeof (jksn_blobstring));
                    if(header) {
                        const char *output_end;
                        (*result)->size = jksn_value_size(result_value, 0) + 3;
                        (*result)->buf = jksn_malloc((*result)->size);
                        (*result)->buf[0] = 'j';
                        (*result)->buf[1] = 'k';
                        (*result)->buf[2] = '!';
                        output_end = jksn_value_output((*result)->buf + 3, result_value);
                        assert((size_t) (output_end - (*result)->buf) == (*result)->size);
                    } else {
                        const char *output_end;
                        (*result)->size = jksn_value_size(result_value, 0);
                        (*result)->buf = jksn_malloc((*result)->size);
                        output_end = jksn_value_output((*result)->buf, result_value);
                        assert((size_t) (output_end - (*result)->buf) == (*result)->size);
                    }
                }
            }
            jksn_value_free(result_value);
            if(cache != cache_)
                jksn_cache_free(cache);
            return retval;
        }
    }
}

static jksn_error_message_no jksn_dump_value(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    jksn_error_message_no retval = JKSN_EOK;
    *result = NULL;
    switch(object->data_type) {
    case JKSN_UNDEFINED:
        *result = jksn_value_new(object, 0x00, NULL, NULL);
        retval = *result ? JKSN_EOK : JKSN_ENOMEM;
        break;
    case JKSN_NULL:
        *result = jksn_value_new(object, 0x01, NULL, NULL);
        retval = *result ? JKSN_EOK : JKSN_ENOMEM;
        break;
    case JKSN_BOOL:
        *result = jksn_value_new(object, object->data_bool ? 0x03 : 0x02, NULL, NULL);
        retval = *result ? JKSN_EOK : JKSN_ENOMEM;
        break;
    case JKSN_INT:
        retval = jksn_dump_int(result, object, cache);
        break;
    case JKSN_FLOAT:
        retval = jksn_dump_float(result, object);
        break;
    case JKSN_DOUBLE:
        retval = jksn_dump_double(result, object);
        break;
    case JKSN_LONG_DOUBLE:
        retval = jksn_dump_longdouble(result, object);
        break;
    case JKSN_STRING:
        retval = jksn_dump_string(result, object);
        break;
    case JKSN_BLOB:
        retval = jksn_dump_blob(result, object);
        break;
    case JKSN_ARRAY:
        retval = jksn_dump_array(result, object, cache);
        break;
    case JKSN_OBJECT:
        retval = jksn_dump_object(result, object, cache);
        break;
    case JKSN_UNSPECIFIED:
        *result = jksn_value_new(object, 0xa0, NULL, NULL);
        retval = *result ? JKSN_EOK : JKSN_ENOMEM;
        break;
    default:
        retval = JKSN_ETYPE;
    }
    return retval;
}

static jksn_error_message_no jksn_dump_int(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    cache->lastint = object->data_int;
    if(object->data_int >= 0 && object->data_int <= 0xa)
        *result = jksn_value_new(object, 0x10 | object->data_int, NULL, NULL);
    else if(object->data_int >= -0x80 && object->data_int <= 0x7f) {
        jksn_blobstring data = {1, jksn_malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 1);
        *result = jksn_value_new(object, 0x1d, &data, NULL);
    } else if(object->data_int >= -0x8000 && object->data_int <= 0x7fff) {
        jksn_blobstring data = {2, jksn_malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 2);
        *result = jksn_value_new(object, 0x1c, &data, NULL);
    } else if((object->data_int >= -0x80000000L && object->data_int <= -0x200000L) ||
              (object->data_int >= 0x200000L && object->data_int <= 0x7fffffffL)) {
        jksn_blobstring data = {4, jksn_malloc(4)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 4);
        *result = jksn_value_new(object, 0x1b, &data, NULL);
    } else if(object->data_int >= 0) {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, (uint64_t) object->data_int, 0);
        *result = jksn_value_new(object, 0x1f, &data, NULL);
    } else {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, (uint64_t) -object->data_int, 0);
        *result = jksn_value_new(object, 0x1e, &data, NULL);
    }
    return *result ? JKSN_EOK : JKSN_ENOMEM;
}

static jksn_error_message_no jksn_dump_float(jksn_value **result, const jksn_t *object) {
    if(isnan(object->data_float)) {
        *result = jksn_value_new(object, 0x20, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else if(isinf(object->data_float)) {
        *result = jksn_value_new(object, object->data_float >= 0.0f ? 0x2f : 0x2e, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else {
        union {
            float data_float;
            uint32_t data_int;
        } conv = {object->data_float};
        jksn_blobstring data = {4, jksn_malloc(4)};
        assert(sizeof (float) == 4);
        if(data.buf) {
            jksn_encode_int(data.buf, conv.data_int, 4);
            *result = jksn_value_new(object, 0x2d, &data, NULL);
            return *result ? JKSN_EOK : JKSN_ENOMEM;
        } else
            return JKSN_ENOMEM;
    }
}

static jksn_error_message_no jksn_dump_double(jksn_value **result, const jksn_t *object) {
    if(isnan(object->data_double)) {
        *result = jksn_value_new(object, 0x20, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else if(isinf(object->data_double)) {
        *result = jksn_value_new(object, object->data_double >= 0.0 ? 0x2f : 0x2e, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else {
        union {
            uint32_t data_int[2];
            double data_double;
            uint8_t endiantest;
        } conv = {{1, 0}};
        jksn_blobstring data = {8, jksn_malloc(8)};
        assert(sizeof (double) == 8);
        if(data.buf) {
            int little_endian = (conv.endiantest == 1);
            conv.data_double = object->data_double;
            if(little_endian) {
                jksn_encode_int(data.buf, conv.data_int[1], 4);
                jksn_encode_int(data.buf+4, conv.data_int[0], 4);
            } else {
                jksn_encode_int(data.buf, conv.data_int[0], 4);
                jksn_encode_int(data.buf+4, conv.data_int[1], 4);
            }
            *result = jksn_value_new(object, 0x2c, &data, NULL);
            return *result ? JKSN_EOK : JKSN_ENOMEM;
        } else
            return JKSN_ENOMEM;
    }
}

static jksn_error_message_no jksn_dump_longdouble(jksn_value **result, const jksn_t *object) {
    if(isnan(object->data_long_double)) {
        *result = jksn_value_new(object, 0x20, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else if(isinf(object->data_long_double)) {
        *result = jksn_value_new(object, object->data_long_double >= 0.0L ? 0x2f : 0x2e, NULL, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else if(sizeof (long double) == 12) {
        union {
            uint8_t data_int[12];
            long double data_long_double;
            uint8_t endiantest;
        } conv = {{1, 0}};
        jksn_blobstring data = {10, jksn_malloc(10)};
        int little_endian = (conv.endiantest == 1);
        if(!data.buf)
            return JKSN_ENOMEM;
        conv.data_long_double = object->data_long_double;
        if(little_endian) {
            data.buf[0] = (char) conv.data_int[9];
            data.buf[1] = (char) conv.data_int[8];
            data.buf[2] = (char) conv.data_int[7];
            data.buf[3] = (char) conv.data_int[6];
            data.buf[4] = (char) conv.data_int[5];
            data.buf[5] = (char) conv.data_int[4];
            data.buf[6] = (char) conv.data_int[3];
            data.buf[7] = (char) conv.data_int[2];
            data.buf[8] = (char) conv.data_int[1];
            data.buf[9] = (char) conv.data_int[0];
        } else {
            data.buf[0] = (char) conv.data_int[2];
            data.buf[1] = (char) conv.data_int[3];
            data.buf[2] = (char) conv.data_int[4];
            data.buf[3] = (char) conv.data_int[5];
            data.buf[4] = (char) conv.data_int[6];
            data.buf[5] = (char) conv.data_int[7];
            data.buf[6] = (char) conv.data_int[8];
            data.buf[7] = (char) conv.data_int[9];
            data.buf[8] = (char) conv.data_int[10];
            data.buf[9] = (char) conv.data_int[11];
        }
        *result = jksn_value_new(object, 0x2b, &data, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else if(sizeof (long double) == 16) {
        union {
            uint8_t data_int[16];
            long double data_long_double;
            uint8_t endiantest;
        } conv = {{1, 0}};
        jksn_blobstring data = {10, jksn_malloc(10)};
        int little_endian = (conv.endiantest == 1);
        if(!data.buf)
            return JKSN_ENOMEM;
        conv.data_long_double = object->data_long_double;
        if(little_endian) {
            data.buf[0] = (char) conv.data_int[9];
            data.buf[1] = (char) conv.data_int[8];
            data.buf[2] = (char) conv.data_int[7];
            data.buf[3] = (char) conv.data_int[6];
            data.buf[4] = (char) conv.data_int[5];
            data.buf[5] = (char) conv.data_int[4];
            data.buf[6] = (char) conv.data_int[3];
            data.buf[7] = (char) conv.data_int[2];
            data.buf[8] = (char) conv.data_int[1];
            data.buf[9] = (char) conv.data_int[0];
        } else {
            data.buf[0] = (char) conv.data_int[6];
            data.buf[1] = (char) conv.data_int[7];
            data.buf[2] = (char) conv.data_int[8];
            data.buf[3] = (char) conv.data_int[9];
            data.buf[4] = (char) conv.data_int[10];
            data.buf[5] = (char) conv.data_int[11];
            data.buf[6] = (char) conv.data_int[12];
            data.buf[7] = (char) conv.data_int[13];
            data.buf[8] = (char) conv.data_int[14];
            data.buf[9] = (char) conv.data_int[15];
        }
        *result = jksn_value_new(object, 0x2b, &data, NULL);
        return *result ? JKSN_EOK : JKSN_ENOMEM;
    } else
        return JKSN_ELONGDOUBLE;
}

static jksn_error_message_no jksn_dump_string(jksn_value **result, const jksn_t *object) {
    size_t utf16size = jksn_utf8_to_utf16(NULL, object->data_string.str, object->data_string.size, 1);
    if(utf16size != (size_t) (ptrdiff_t) -1 && utf16size*2 < object->data_string.size) {
        uint16_t *utf16str = jksn_malloc(utf16size*2);
        jksn_blobstring buf = {0, (char *) utf16str};
        size_t i;
        if(!utf16str)
            return JKSN_ENOMEM;
        buf.size = jksn_utf8_to_utf16(utf16str, object->data_string.str, object->data_string.size, 1) * 2;
        assert(buf.size == utf16size * 2);
        for(i = 0; i < utf16size; i++)
            utf16str[i] = jksn_uint16_to_le(utf16str[i]);
        if(utf16size <= 0xb)
            *result = jksn_value_new(object, 0x30 | utf16size, NULL, &buf);
        else if(utf16size <= 0xff) {
            jksn_blobstring data = {1, jksn_malloc(1)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            jksn_encode_int(data.buf, utf16size, 1);
            *result = jksn_value_new(object, 0x3e, &data, &buf);
        } else if(utf16size <= 0xffff) {
            jksn_blobstring data = {2, jksn_malloc(2)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            jksn_encode_int(data.buf, utf16size, 2);
            *result = jksn_value_new(object, 0x3d, &data, &buf);
        } else {
            jksn_blobstring data = {0, jksn_malloc(10)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            data.size = jksn_encode_int(data.buf, utf16size, 0);
            *result = jksn_value_new(object, 0x3f, &data, &buf);
        }
    } else {
        jksn_blobstring buf = {object->data_string.size, jksn_malloc(object->data_string.size)};
        if(!buf.buf)
            return JKSN_ENOMEM;
        memcpy(buf.buf, object->data_string.str, object->data_string.size);
        if(buf.size <= 0xc)
            *result = jksn_value_new(object, 0x40 | buf.size, NULL, &buf);
        else if(buf.size <= 0xff) {
            jksn_blobstring data = {1, jksn_malloc(1)};
            if(!data.buf)
                return JKSN_ENOMEM;
            jksn_encode_int(data.buf, buf.size, 1);
            *result = jksn_value_new(object, 0x4e, &data, &buf);
        } else if(buf.size <= 0xffff) {
            jksn_blobstring data = {2, jksn_malloc(2)};
            if(!data.buf)
                return JKSN_ENOMEM;
            jksn_encode_int(data.buf, buf.size, 2);
            *result = jksn_value_new(object, 0x4d, &data, &buf);
        } else {
            jksn_blobstring data = {0, jksn_malloc(10)};
            if(!data.buf)
                return JKSN_ENOMEM;
            data.size = jksn_encode_int(data.buf, buf.size, 0);
            *result = jksn_value_new(object, 0x4f, &data, &buf);
        }
    }
    if(*result) {
        (*result)->hash = jksn_djbhash((*result)->buf.buf, (*result)->buf.size);
        return JKSN_EOK;
    } else
        return JKSN_ENOMEM;
}

static jksn_error_message_no jksn_dump_blob(jksn_value **result, const jksn_t *object) {
    jksn_blobstring buf = {object->data_blob.size, jksn_malloc(object->data_blob.size)};
    if(!buf.buf)
        return JKSN_ENOMEM;
    memcpy(buf.buf, object->data_blob.buf, object->data_blob.size);
    if(object->data_blob.size <= 0xb)
        *result = jksn_value_new(object, 0x50 | object->data_blob.size, NULL, &object->data_blob);
    else if(object->data_blob.size <= 0xff) {
        jksn_blobstring data = {1, jksn_malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_blob.size, 1);
        *result = jksn_value_new(object, 0x5e, &data, &object->data_blob);
    } else if(object->data_blob.size <= 0xffff) {
        jksn_blobstring data = {2, jksn_malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_blob.size, 2);
        *result = jksn_value_new(object, 0x5d, &data, &object->data_blob);
    } else {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, object->data_blob.size, 0);
        *result = jksn_value_new(object, 0x5f, &data, &object->data_blob);
    }
    if(*result) {
        (*result)->hash = jksn_djbhash((*result)->buf.buf, (*result)->buf.size);
        return JKSN_EOK;
    } else
        return JKSN_ENOMEM;
}

static int jksn_test_swap_availability(const jksn_t *object) {
    int columns = 0;
    size_t row;
    for(row = 0; row < object->data_array.size; row++)
        if(object->data_array.children[row]->data_type != JKSN_OBJECT)
            return 0;
        else if(object->data_array.children[row]->data_object.size != 0)
            columns = 1;
    return columns;
}

static jksn_error_message_no jksn_encode_straight_array(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    if(object->data_array.size <= 0xc)
        *result = jksn_value_new(object, 0x80 | object->data_array.size, NULL, NULL);
    else if(object->data_array.size <= 0xff) {
        jksn_blobstring data = {1, jksn_malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_array.size, 1);
        *result = jksn_value_new(object, 0x8e, &data, NULL);
    } else if(object->data_array.size <= 0xffff) {
        jksn_blobstring data = {2, jksn_malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_array.size, 2);
        *result = jksn_value_new(object, 0x8d, &data, NULL);
    } else {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, object->data_array.size, 0);
        *result = jksn_value_new(object, 0x8f, &data, NULL);
    }
    if(!*result)
        return JKSN_ENOMEM;
    else {
        size_t i;
        jksn_value **next_child = &(*result)->first_child;
        for(i = 0; i < object->data_array.size; i++) {
            jksn_error_message_no retval = jksn_dump_value(next_child, object->data_array.children[i], cache);
            if(retval != JKSN_EOK)
                return retval;
            next_child = &(*next_child)->next_child;
        }
        return JKSN_EOK;
    }
}

static jksn_error_message_no jksn_encode_swapped_array(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    struct jksn_swap_columns *columns = NULL;
    size_t columns_size = 0;
    size_t row;
    for(row = 0; row < object->data_array.size; row++) {
        size_t column;
        for(column = 0; column < object->data_array.children[row]->data_object.size; column++) {
            struct jksn_swap_columns **next_column = &columns;
            while(*next_column) {
                if(!jksn_compare((*next_column)->key, object->data_array.children[row]->data_object.children[column].key))
                    break;
                next_column = &(*next_column)->next;
            }
            if(!*next_column) {
                *next_column = jksn_calloc(1, sizeof (struct jksn_swap_columns));
                if(!*next_column) {
                    columns = jksn_swap_columns_free(columns);
                    return JKSN_ENOMEM;
                }
                (*next_column)->key = object->data_array.children[row]->data_object.children[column].key;
                columns_size++;
            }
        }
    }
    if(columns_size <= 0xc)
        *result = jksn_value_new(object, 0xa0 | columns_size, NULL, NULL);
    else if(columns_size <= 0xff) {
        jksn_blobstring data = {1, jksn_malloc(1)};
        if(!data.buf) {
            columns = jksn_swap_columns_free(columns);
            return JKSN_ENOMEM;
        }
        jksn_encode_int(data.buf, columns_size, 1);
        *result = jksn_value_new(object, 0xae, &data, NULL);
    } else if(columns_size <= 0xffff) {
        jksn_blobstring data = {2, jksn_malloc(2)};
        if(!data.buf) {
            columns = jksn_swap_columns_free(columns);
            return JKSN_ENOMEM;
        }
        jksn_encode_int(data.buf, columns_size, 2);
        *result = jksn_value_new(object, 0xad, &data, NULL);
    } else {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf) {
            columns = jksn_swap_columns_free(columns);
            return JKSN_ENOMEM;
        }
        data.size = jksn_encode_int(data.buf, columns_size, 0);
        *result = jksn_value_new(object, 0xaf, &data, NULL);
    }
    if(!*result) {
        columns = jksn_swap_columns_free(columns);
        return JKSN_ENOMEM;
    } else {
        jksn_value **next_child = &(*result)->first_child;
        struct jksn_swap_columns *next_column = columns;
        while(next_column) {
            jksn_error_message_no retval;
            size_t row;
            jksn_t *row_array = jksn_malloc(sizeof (jksn_t));
            if(!row_array) {
                columns = jksn_swap_columns_free(columns);
                return JKSN_ENOMEM;
            }
            row_array->data_type = JKSN_ARRAY;
            row_array->data_array.size = object->data_array.size;
            row_array->data_array.children = jksn_malloc(object->data_array.size * sizeof (jksn_t *));
            if(!row_array->data_array.children) {
                free(row_array);
                columns = jksn_swap_columns_free(columns);
                return JKSN_ENOMEM;
            }
            for(row = 0; row < object->data_array.size; row++) {
                static jksn_t unspecified_value = { .data_type = JKSN_UNSPECIFIED };
                size_t i;
                row_array->data_array.children[row] = &unspecified_value;
                for(i = object->data_array.children[row]->data_object.size; i--; )
                    if(!jksn_compare(next_column->key, object->data_array.children[row]->data_object.children[i].key)) {
                        row_array->data_array.children[row] = object->data_array.children[row]->data_object.children[i].value;
                        break;
                    }
            }
            retval = jksn_dump_value(next_child, next_column->key, cache);
            if(retval != JKSN_EOK) {
                free(row_array->data_array.children);
                free(row_array);
                columns = jksn_swap_columns_free(columns);
                return retval;
            }
            next_child = &(*next_child)->next_child;
            retval = jksn_dump_array(next_child, row_array, cache);
            free(row_array->data_array.children);
            free(row_array);
            if(retval != JKSN_EOK) {
                columns = jksn_swap_columns_free(columns);
                return retval;
            }
            next_child = &(*next_child)->next_child;
            next_column = next_column->next;
            columns_size--;
        }
        assert(columns_size == 0);
        columns = jksn_swap_columns_free(columns);
        return JKSN_EOK;
    }
}

static jksn_error_message_no jksn_dump_array(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    jksn_error_message_no retval = jksn_encode_straight_array(result, object, cache);
    if(retval != JKSN_EOK)
        return retval;
    if(jksn_test_swap_availability(object)) {
        jksn_value *result_swapped = NULL;
        retval = jksn_encode_swapped_array(&result_swapped, object, cache);
        assert(retval == JKSN_EOK);
        if(retval == JKSN_EOK && jksn_value_size(result_swapped, 3) < jksn_value_size(*result, 3)) {
            jksn_value_free(*result);
            *result = result_swapped;
        } else
            result_swapped = jksn_value_free(result_swapped);
    }
    return JKSN_EOK;
}

static jksn_error_message_no jksn_dump_object(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    if(object->data_object.size <= 0xc)
        *result = jksn_value_new(object, 0x90 | object->data_object.size, NULL, NULL);
    else if(object->data_object.size <= 0xff) {
        jksn_blobstring data = {1, jksn_malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_object.size, 1);
        *result = jksn_value_new(object, 0x9e, &data, NULL);
    } else if(object->data_object.size <= 0xffff) {
        jksn_blobstring data = {2, jksn_malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_object.size, 2);
        *result = jksn_value_new(object, 0x9d, &data, NULL);
    } else {
        jksn_blobstring data = {0, jksn_malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, object->data_object.size, 0);
        *result = jksn_value_new(object, 0x9f, &data, NULL);
    }
    if(!*result)
        return JKSN_ENOMEM;
    else {
        size_t i;
        jksn_value **next_child = &(*result)->first_child;
        for(i = 0; i < object->data_object.size; i++) {
            jksn_error_message_no retval = jksn_dump_value(next_child, object->data_object.children[i].key, cache);
            if(retval != JKSN_EOK) {
                *result = jksn_value_free(*result);
                return retval;
            }
            next_child = &(*next_child)->next_child;
            retval = jksn_dump_value(next_child, object->data_object.children[i].value, cache);
            if(retval != JKSN_EOK) {
                *result = jksn_value_free(*result);
                return retval;
            }
            next_child = &(*next_child)->next_child;
        }
        return JKSN_EOK;
    }
}

static void jksn_optimize(jksn_value *object, jksn_cache *cache) {
    while(object) {
        uint8_t control = object->control & 0xf0;
        switch(control) {
        case 0x10:
            if(cache->haslastint) {
                int64_t delta = object->origin->data_int - cache->lastint;
                if(jksn_int64abs(delta) < jksn_int64abs(object->origin->data_int)) {
                    uint8_t new_control = 0;
                    jksn_blobstring new_data = {0, NULL};
                    if(delta >= 0 && delta <= 0x5)
                        new_control = 0xb0 | delta;
                    else if(delta >= -0x5 && delta <= -0x1)
                        new_control = 0xb0 | (delta + 11);
                    else if(delta >= -0x80 && delta <= 0x7f) {
                        new_data.size = 1;
                        new_data.buf = jksn_malloc(1);
                        if(new_data.buf) {
                            new_control = 0xbd;
                            jksn_encode_int(new_data.buf, (uint64_t) delta, 1);
                        }
                    } else if(delta >= -0x8000 && delta <= 0x7fff) {
                        new_data.size = 2;
                        new_data.buf = jksn_malloc(2);
                        if(new_data.buf) {
                            new_control = 0xbc;
                            jksn_encode_int(new_data.buf, (uint64_t) delta, 2);
                        }
                    } else if((delta >= -0x80000000L && delta <= -0x200000L) ||
                              (delta >= 0x200000L && delta <= 0x7fffffffL)) {
                        new_data.size = 4;
                        new_data.buf = jksn_malloc(4);
                        if(new_data.buf) {
                            new_control = 0xbb;
                            jksn_encode_int(new_data.buf, (uint64_t) delta, 4);
                        }
                    } else if(delta >= 0) {
                        new_data.buf = jksn_malloc(10);
                        if(new_data.buf) {
                            new_control = 0xbf;
                            new_data.size = jksn_encode_int(new_data.buf, (uint64_t) delta, 0);
                        }
                    } else {
                        new_data.buf = jksn_malloc(10);
                        if(new_data.buf) {
                            new_control = 0xbe;
                            new_data.size = jksn_encode_int(new_data.buf, (uint64_t) -delta, 0);
                        }
                    }
                    if(new_control != 0) {
                        if(new_data.size < object->data.size) {
                            object->control = new_control;
                            free(object->data.buf);
                            object->data = new_data;
                        } else
                            free(new_data.buf);
                    }
                }
            } else
                cache->haslastint = 1;
            cache->lastint = object->origin->data_int;
            break;
        case 0x30:
        case 0x40:
            if(object->buf.size > 1) {
                if(cache->texthash[object->hash].size == object->origin->data_string.size &&
                   !memcmp(cache->texthash[object->hash].str, object->origin->data_string.str, object->origin->data_string.size)) {
                    jksn_blobstring new_data = {1, jksn_malloc(1)};
                    if(new_data.buf) {
                        new_data.buf[0] = (char) object->hash;
                        object->control = 0x3c;
                        object->data = new_data;
                        object->buf.size = 0;
                        free(object->buf.buf);
                        object->buf.buf = NULL;
                    }
                } else {
                    free(cache->texthash[object->hash].str);
                    cache->texthash[object->hash].str = jksn_malloc(object->origin->data_string.size);
                    if(cache->texthash[object->hash].str) {
                        cache->texthash[object->hash].size = object->origin->data_string.size;
                        memcpy(cache->texthash[object->hash].str, object->origin->data_string.str, object->origin->data_string.size);
                    } else
                        cache->texthash[object->hash].size = 0;
                }
            }
            break;
        case 0x50:
            if(object->buf.size > 1) {
                if(cache->blobhash[object->hash].size == object->origin->data_blob.size &&
                   !memcmp(cache->blobhash[object->hash].buf, object->origin->data_blob.buf, object->origin->data_blob.size)) {
                    jksn_blobstring new_data = {1, jksn_malloc(1)};
                    if(new_data.buf) {
                        new_data.buf[0] = (char) object->hash;
                        object->control = 0x5c;
                        object->data = new_data;
                        object->buf.size = 0;
                        free(object->buf.buf);
                        object->buf.buf = NULL;
                    }
                } else {
                    free(cache->blobhash[object->hash].buf);
                    cache->blobhash[object->hash].buf = jksn_malloc(object->origin->data_blob.size);
                    if(cache->blobhash[object->hash].buf) {
                        cache->blobhash[object->hash].size = object->origin->data_blob.size;
                        memcpy(cache->blobhash[object->hash].buf, object->origin->data_blob.buf, object->origin->data_blob.size);
                    } else
                        cache->blobhash[object->hash].size = 0;
                }
            }
            break;
        default:
            jksn_optimize(object->first_child, cache);
        }
        object = object->next_child;
    }
}

static size_t jksn_encode_int(char result[], uint64_t number, size_t size) {
    switch(size) {
    case 1:
        result[0] = (char) (uint8_t) number;
        break;
    case 2:
        result[0] = (char) (uint8_t) (number >> 8);
        result[1] = (char) (uint8_t) number;
        break;
    case 4:
        result[0] = (char) (uint8_t) (number >> 24);
        result[1] = (char) (uint8_t) (number >> 16);
        result[2] = (char) (uint8_t) (number >> 8);
        result[3] = (char) (uint8_t) number;
        break;
    case 0: /* buffer size should be 10 */
        result[(size = 9)] = (char) (((uint8_t) number) & 0x7f);
        number >>= 7;
        while(number != 0) {
            result[--size] = (char) (((uint8_t) number) | 0x80);
            number >>= 7;
        }
        assert(result <= result+size);
        memmove(result, result+size, 10-size);
        size = 10-size;
        break;
    default:
        assert(size == 1 || size == 2 || size == 4 || size == 0);
    }
    return size;
}

static struct jksn_swap_columns *jksn_swap_columns_free(struct jksn_swap_columns *columns) {
    struct jksn_swap_columns *column = columns;
    while(column) {
        struct jksn_swap_columns *next_column = column->next;
        free(column);
        column = next_column;
    }
    return NULL;
}

int jksn_parse(jksn_t **result, const jksn_blobstring *buffer, size_t *bytes_parsed, jksn_cache *cache_) {
    *result = NULL;
    if(bytes_parsed)
        *bytes_parsed = 0;
    if(!buffer)
        return JKSN_ETRUNC;
    else {
        jksn_cache *cache = cache_ ? cache_ : jksn_cache_new();
        if(!cache)
            return JKSN_ENOMEM;
        else {
            jksn_error_message_no retval;
            if(buffer->size >= 3 && buffer->buf[0] == 'j' && buffer->buf[1] == 'k' && buffer->buf[2] == '!') {
                if(bytes_parsed)
                    *bytes_parsed += 3;
                retval = jksn_parse_value(result, buffer->buf + 3, buffer->size - 3, bytes_parsed, cache);
            } else
                retval = jksn_parse_value(result, buffer->buf, buffer->size, bytes_parsed, cache);
            if(cache != cache_)
                jksn_cache_free(cache);
            return retval;
        }
    }
}

static jksn_error_message_no jksn_parse_value(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed, jksn_cache *cache) {
    *result = NULL;
    if(!size)
        return JKSN_ETRUNC;
    else {
        uint8_t control = (uint8_t) buffer[0];
        uint8_t ctrlhi = control & 0xf0;
        buffer++;
        size--;
        if(bytes_parsed)
            (*bytes_parsed)++;
        switch(ctrlhi) {
        case 0x00:
            switch(control) {
            case 0x00:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_UNDEFINED;
                return JKSN_EOK;
            case 0x01:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_NULL;
                return JKSN_EOK;
            case 0x02:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_BOOL;
                (*result)->data_bool = 0;
                return JKSN_EOK;
            case 0x03:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_BOOL;
                (*result)->data_bool = 1;
                return JKSN_EOK;
            case 0x0f:
                return JKSN_EJSON;
            }
            break;
        case 0x10:
            {
                jksn_error_message_no retval;
                uint64_t uresint;
                int64_t resint;
                switch(control) {
                case 0x1b:
                    retval = jksn_decode_int(&uresint, buffer, size, 4, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    resint = (int64_t) (int32_t) uresint;
                    break;
                case 0x1c:
                    retval = jksn_decode_int(&uresint, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    resint = (int64_t) (int16_t) uresint;
                    break;
                case 0x1d:
                    retval = jksn_decode_int(&uresint, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    resint = (int64_t) (int8_t) uresint;
                    break;
                case 0x1e:
                    retval = jksn_decode_int(&uresint, buffer, size, 0, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if((int64_t) uresint < 0)
                        return JKSN_EVARINT;
                    resint = -(int64_t) uresint;
                    break;
                case 0x1f:
                    retval = jksn_decode_int(&uresint, buffer, size, 0, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if((int64_t) uresint < 0)
                        return JKSN_EVARINT;
                    resint = (int64_t) uresint;
                    break;
                default:
                    resint = (int64_t) (control & 0xf);
                }
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_INT;
                (*result)->data_int = resint;
                cache->lastint = resint;
                cache->haslastint = 1;
                return JKSN_EOK;
            }
        case 0x20:
            switch(control) {
            case 0x20:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_DOUBLE;
                (*result)->data_double = NAN;
                return JKSN_EOK;
            case 0x2b:
                return jksn_parse_longdouble(result, buffer, size, bytes_parsed);
            case 0x2c:
                return jksn_parse_double(result, buffer, size, bytes_parsed);
            case 0x2d:
                return jksn_parse_float(result, buffer, size, bytes_parsed);
            case 0x2e:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_DOUBLE;
                (*result)->data_double = -INFINITY;
                return JKSN_EOK;
            case 0x2f:
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_DOUBLE;
                (*result)->data_double = INFINITY;
                return JKSN_EOK;
            }
        case 0x30:
            {
                jksn_error_message_no retval;
                uint64_t str_size;
                size_t varint_size = 0;
                uint16_t *utf16str;
                size_t i;
                uint8_t hashvalue;
                switch(control) {
                case 0x3c:
                    if(!size--)
                        return JKSN_ETRUNC;
                    hashvalue = (uint8_t) (buffer++)[0];
                    if(bytes_parsed)
                        (*bytes_parsed)++;
                    if(cache->texthash[hashvalue].size == 0)
                        return JKSN_EHASH;
                    *result = jksn_malloc(sizeof (jksn_t));
                    if(!*result)
                        return JKSN_ENOMEM;
                    (*result)->data_type = JKSN_STRING;
                    (*result)->data_string.str = jksn_malloc(cache->texthash[hashvalue].size + 1);
                    (*result)->data_string.size = cache->texthash[hashvalue].size;
                    if(!(*result)->data_string.str) {
                        free(*result);
                        *result = NULL;
                        return JKSN_ENOMEM;
                    }
                    memcpy((*result)->data_string.str, cache->texthash[hashvalue].str, cache->texthash[hashvalue].size);
                    (*result)->data_string.str[cache->texthash[hashvalue].size] = '\0';
                    return JKSN_EOK;
                case 0x3d:
                    retval = jksn_decode_int(&str_size, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x3e:
                    retval = jksn_decode_int(&str_size, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x3f:
                    retval = jksn_decode_int(&str_size, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        *bytes_parsed += varint_size;
                    break;
                default:
                    str_size = control & 0xf;
                }
                if(size < str_size*2)
                    return JKSN_ETRUNC;
                utf16str = jksn_malloc(str_size*2);
                if(!utf16str)
                    return JKSN_ENOMEM;
                memcpy((char *) utf16str, buffer, str_size*2);
                hashvalue = jksn_djbhash((char *) utf16str, str_size*2);
                for(i = 0; i < str_size; i++)
                    utf16str[i] = jksn_uint16_to_le(utf16str[i]);
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_STRING;
                (*result)->data_string.size = jksn_utf16_to_utf8(NULL, utf16str, str_size);
                (*result)->data_string.str = jksn_malloc((*result)->data_string.size + 1);
                if(!(*result)->data_string.str) {
                    free(utf16str);
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                jksn_utf16_to_utf8((*result)->data_string.str, utf16str, str_size);
                free(utf16str);
                utf16str = NULL;
                (*result)->data_string.str[(*result)->data_string.size] = '\0';
                buffer += str_size*2;
                size -= str_size*2;
                if(bytes_parsed)
                    *bytes_parsed += str_size*2;
                free(cache->texthash[hashvalue].str);
                cache->texthash[hashvalue].str = jksn_malloc((*result)->data_string.size);
                if(cache->texthash[hashvalue].str) {
                    cache->texthash[hashvalue].size = (*result)->data_string.size;
                    memcpy(cache->texthash[hashvalue].str, (*result)->data_string.str, (*result)->data_string.size);
                } else {
                    cache->texthash[hashvalue].size = 0;
                    free((*result)->data_string.str);
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                return JKSN_EOK;
            }
        case 0x40:
            {
                jksn_error_message_no retval;
                uint64_t str_size;
                size_t varint_size = 0;
                uint8_t hashvalue;
                switch(control) {
                case 0x4d:
                    retval = jksn_decode_int(&str_size, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x4e:
                    retval = jksn_decode_int(&str_size, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x4f:
                    retval = jksn_decode_int(&str_size, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        bytes_parsed += varint_size;
                    break;
                default:
                    str_size = control & 0xf;
                }
                if(size < str_size)
                    return JKSN_ETRUNC;
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_STRING;
                (*result)->data_string.size = str_size;
                (*result)->data_string.str = jksn_malloc(str_size + 1);
                if(!(*result)->data_string.str) {
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                memcpy((*result)->data_string.str, buffer, str_size);
                (*result)->data_string.str[str_size] = '\0';
                buffer += str_size;
                size -= str_size;
                if(bytes_parsed)
                    *bytes_parsed += str_size;
                hashvalue = jksn_djbhash((*result)->data_string.str, str_size);
                free(cache->texthash[hashvalue].str);
                cache->texthash[hashvalue].str = jksn_malloc(str_size);
                if(cache->texthash[hashvalue].str) {
                    cache->texthash[hashvalue].size = str_size;
                    memcpy(cache->texthash[hashvalue].str, (*result)->data_string.str, str_size);
                } else {
                    cache->texthash[hashvalue].size = 0;
                    free((*result)->data_string.str);
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                return JKSN_EOK;
            }
        case 0x50:
            {
                jksn_error_message_no retval;
                uint64_t blob_size;
                size_t varint_size = 0;
                uint8_t hashvalue;
                switch(control) {
                case 0x5d:
                    retval = jksn_decode_int(&blob_size, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x5e:
                    retval = jksn_decode_int(&blob_size, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x5f:
                    retval = jksn_decode_int(&blob_size, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        bytes_parsed += varint_size;
                    break;
                default:
                    blob_size = control & 0xf;
                }
                if(size < blob_size)
                    return JKSN_ETRUNC;
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_BLOB;
                (*result)->data_blob.size = blob_size;
                (*result)->data_blob.buf = jksn_malloc(blob_size + 1);
                if(!(*result)->data_blob.buf) {
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                memcpy((*result)->data_blob.buf, buffer, blob_size);
                (*result)->data_blob.buf[blob_size] = '\0';
                buffer += blob_size;
                size -= blob_size;
                if(bytes_parsed)
                    *bytes_parsed += blob_size;
                hashvalue = jksn_djbhash((*result)->data_blob.buf, blob_size);
                free(cache->blobhash[hashvalue].buf);
                cache->blobhash[hashvalue].buf = jksn_malloc(blob_size);
                if(cache->blobhash[hashvalue].buf) {
                    cache->blobhash[hashvalue].size = blob_size;
                    memcpy(cache->blobhash[hashvalue].buf, (*result)->data_blob.buf, blob_size);
                } else {
                    cache->blobhash[hashvalue].size = 0;
                    free((*result)->data_blob.buf);
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                return JKSN_EOK;
            }
        case 0x70:
            {
                jksn_error_message_no retval;
                uint64_t value_len;
                size_t varint_size = 0;
                size_t i;
                switch(control) {
                case 0x70:
                    for(i = 0; i < 256; i++) {
                        cache->texthash[i].size = 0;
                        free(cache->texthash[i].str);
                        cache->texthash[i].str = NULL;
                    }
                    for(i = 0; i < 256; i++) {
                        cache->blobhash[i].size = 0;
                        free(cache->blobhash[i].buf);
                        cache->blobhash[i].buf = NULL;
                    }
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0x7d:
                    retval = jksn_decode_int(&value_len, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x7e:
                    retval = jksn_decode_int(&value_len, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x7f:
                    retval = jksn_decode_int(&value_len, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        *bytes_parsed += varint_size;
                    break;
                default:
                    value_len = control & 0xf;
                }
                while(value_len--) {
                    jksn_t *tmp = NULL;
                    size_t bytes_skipped = 0;
                    retval = jksn_parse_value(&tmp, buffer, size, &bytes_skipped, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    jksn_free(tmp);
                    buffer += bytes_skipped;
                    size -= bytes_skipped;
                    if(bytes_parsed)
                        *bytes_parsed += bytes_skipped;
                }
                return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
            }
        case 0x80:
            {
                jksn_error_message_no retval;
                uint64_t array_len;
                size_t varint_size = 0;
                size_t i;
                switch(control) {
                case 0x8d:
                    retval = jksn_decode_int(&array_len, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x8e:
                    retval = jksn_decode_int(&array_len, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x8f:
                    retval = jksn_decode_int(&array_len, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        *bytes_parsed += varint_size;
                    break;
                default:
                    array_len = control & 0xf;
                }
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_ARRAY;
                (*result)->data_array.size = array_len;
                (*result)->data_array.children = jksn_calloc(array_len, sizeof (jksn_t *));
                if(!(*result)->data_array.children) {
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                for(i = 0; i < array_len; i++) {
                    size_t child_size = 0;
                    retval = jksn_parse_value(&(*result)->data_array.children[i], buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK) {
                        *result = jksn_free(*result);
                        return retval;
                    }
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                }
                return JKSN_EOK;
            }
        case 0x90:
            {
                jksn_error_message_no retval;
                uint64_t object_len;
                size_t varint_size = 0;
                size_t i;
                switch(control) {
                case 0x9d:
                    retval = jksn_decode_int(&object_len, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0x9e:
                    retval = jksn_decode_int(&object_len, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0x9f:
                    retval = jksn_decode_int(&object_len, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        *bytes_parsed += varint_size;
                    break;
                default:
                    object_len = control & 0xf;
                }
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_OBJECT;
                (*result)->data_object.size = object_len;
                (*result)->data_object.children = jksn_calloc(object_len, sizeof (jksn_keyvalue));
                if(!(*result)->data_object.children) {
                    free(*result);
                    *result = NULL;
                    return JKSN_ENOMEM;
                }
                for(i = 0; i < object_len; i++) {
                    size_t child_size = 0;
                    retval = jksn_parse_value(&(*result)->data_object.children[i].key, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK) {
                        *result = jksn_free(*result);
                        return retval;
                    }
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    child_size = 0;
                    retval = jksn_parse_value(&(*result)->data_object.children[i].value, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK) {
                        *result = jksn_free(*result);
                        return retval;
                    }
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                }
                return JKSN_EOK;
            }
        case 0xa0:
            {
                jksn_error_message_no retval;
                uint64_t column_len;
                size_t varint_size = 0;
                size_t column_id;
                switch(control) {
                case 0xa0:
                    *result = jksn_malloc(sizeof (jksn_t));
                    if(!*result)
                        return JKSN_ENOMEM;
                    (*result)->data_type = JKSN_UNSPECIFIED;
                    return JKSN_EOK;
                case 0xad:
                    retval = jksn_decode_int(&column_len, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += 2;
                    size -= 2;
                    break;
                case 0xa3:
                    retval = jksn_decode_int(&column_len, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer++;
                    size--;
                    break;
                case 0xa4:
                    retval = jksn_decode_int(&column_len, buffer, size, 0, &varint_size);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += varint_size;
                    size -= varint_size;
                    if(bytes_parsed)
                        *bytes_parsed += varint_size;
                    break;
                default:
                    column_len = control & 0xf;
                }
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_ARRAY;
                (*result)->data_array.size = 0;
                (*result)->data_array.children = NULL;
                for(column_id = 0; column_id < column_len; column_id++) {
                    jksn_t *column_name = NULL;
                    jksn_t *column_values = NULL;
                    size_t child_size = 0;
                    size_t row;
                    retval = jksn_parse_value(&column_name, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK) {
                        *result = jksn_free(*result);
                        return retval;
                    }
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    child_size = 0;
                    retval = jksn_parse_value(&column_values, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK) {
                        column_name = jksn_free(column_name);
                        *result = jksn_free(*result);
                        return retval;
                    }
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(column_values->data_type != JKSN_ARRAY) {
                        column_name = jksn_free(column_name);
                        column_values = jksn_free(column_values);
                        *result = jksn_free(*result);
                        return JKSN_ESWAPARRAY;
                    }
                    if((*result)->data_array.size < column_values->data_array.size) {
                        jksn_t **tmpptr = jksn_realloc((*result)->data_array.children, column_values->data_array.size * sizeof (jksn_t *));
                        if(tmpptr) {
                            size_t i;
                            size_t oldsize = (*result)->data_array.size;
                            (*result)->data_array.size = column_values->data_array.size;
                            (*result)->data_array.children = tmpptr;
                            for(i = oldsize; i < column_values->data_array.size; i++)
                                tmpptr[i] = NULL;
                            for(i = oldsize; i < column_values->data_array.size; i++) {
                                tmpptr[i] = jksn_malloc(sizeof (jksn_t));
                                if(!tmpptr[i]) {
                                    column_name = jksn_free(column_name);
                                    column_values = jksn_free(column_values);
                                    *result = jksn_free(*result);
                                    return JKSN_ENOMEM;
                                }
                                tmpptr[i]->data_type = JKSN_OBJECT;
                                tmpptr[i]->data_object.size = 0;
                                tmpptr[i]->data_object.children = NULL;
                            }
                        } else {
                            *result = jksn_free(*result);
                            return JKSN_ENOMEM;
                        }
                    }
                    for(row = 0; row < column_values->data_array.size; row++)
                        if(column_values->data_array.children[row]->data_type != JKSN_UNSPECIFIED) {
                            size_t oldsize = (*result)->data_array.children[row]->data_object.size;
                            jksn_keyvalue *tmpptr = jksn_realloc((*result)->data_array.children[row]->data_object.children, (oldsize + 1) * sizeof (jksn_keyvalue));
                            if(!tmpptr) {
                                column_name = jksn_free(column_name);
                                column_values = jksn_free(column_values);
                                *result = jksn_free(*result);
                                return JKSN_ENOMEM;
                            }
                            tmpptr[oldsize].key = jksn_duplicate(column_name);
                            if(!tmpptr[oldsize].key) {
                                tmpptr[oldsize].value = NULL;
                                column_name = jksn_free(column_name);
                                column_values = jksn_free(column_values);
                                *result = jksn_free(*result);
                                return JKSN_ENOMEM;
                            }
                            tmpptr[oldsize].value = column_values->data_array.children[row];
                            column_values->data_array.children[row] = NULL;
                            (*result)->data_array.children[row]->data_object.children = tmpptr;
                            (*result)->data_array.children[row]->data_object.size++;
                        }
                    jksn_free(column_name);
                    jksn_free(column_values);
                }
                return JKSN_EOK;
            }
        case 0xb0:
            {
                jksn_error_message_no retval;
                uint64_t delta;
                switch(control) {
                case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5:
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += (int64_t) (control & 0xf);
                    break;
                case 0xb6: case 0xb7: case 0xb8: case 0xb9: case 0xba:
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += ((int64_t) (control & 0xf)) - 11;
                    break;
                case 0xbb:
                    retval = jksn_decode_int(&delta, buffer, size, 4, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += (int32_t) delta;
                    break;
                case 0xbc:
                    retval = jksn_decode_int(&delta, buffer, size, 2, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += (int16_t) delta;
                    break;
                case 0xbd:
                    retval = jksn_decode_int(&delta, buffer, size, 1, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += (int8_t) delta;
                    break;
                case 0xbe:
                    retval = jksn_decode_int(&delta, buffer, size, 0, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if((int64_t) delta < 0)
                        return JKSN_EVARINT;
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint -= delta;
                    break;
                case 0xbf:
                    retval = jksn_decode_int(&delta, buffer, size, 0, bytes_parsed);
                    if(retval != JKSN_EOK)
                        return retval;
                    if((int64_t) delta < 0)
                        return JKSN_EVARINT;
                    if(!cache->haslastint)
                        return JKSN_EDELTA;
                    cache->lastint += delta;
                    break;
                }
                cache->haslastint = 1;
                *result = jksn_malloc(sizeof (jksn_t));
                if(!*result)
                    return JKSN_ENOMEM;
                (*result)->data_type = JKSN_INT;
                (*result)->data_int = cache->lastint;
                return JKSN_EOK;
            }
        case 0xf0:
            {
                jksn_error_message_no retval;
                jksn_t *tmp;
                size_t child_size = 0;
                switch(control) {
                case 0xf0:
                    if(size < 4)
                        return JKSN_ETRUNC;
                    buffer += 4;
                    size -= 4;
                    if(bytes_parsed)
                        *bytes_parsed += 4;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0xf1:
                    if(size < 16)
                        return JKSN_ETRUNC;
                    buffer += 16;
                    size -= 16;
                    if(bytes_parsed)
                        *bytes_parsed += 16;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0xf2:
                    if(size < 20)
                        return JKSN_ETRUNC;
                    buffer += 20;
                    size -= 20;
                    if(bytes_parsed)
                        *bytes_parsed += 20;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0xf3:
                    if(size < 32)
                        return JKSN_ETRUNC;
                    buffer += 32;
                    size -= 32;
                    if(bytes_parsed)
                        *bytes_parsed += 32;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0xf4:
                    if(size < 64)
                        return JKSN_ETRUNC;
                    buffer += 64;
                    size -= 64;
                    if(bytes_parsed)
                        *bytes_parsed += 64;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                case 0xf8:
                    retval = jksn_parse_value(result, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(size < 4)
                        return JKSN_ETRUNC;
                    buffer += 4;
                    size -= 4;
                    if(bytes_parsed)
                        *bytes_parsed += 4;
                    return JKSN_EOK;
                case 0xf9:
                    retval = jksn_parse_value(result, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(size < 16)
                        return JKSN_ETRUNC;
                    buffer += 16;
                    size -= 16;
                    if(bytes_parsed)
                        *bytes_parsed += 16;
                    return JKSN_EOK;
                case 0xfa:
                    retval = jksn_parse_value(result, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(size < 20)
                        return JKSN_ETRUNC;
                    buffer += 20;
                    size -= 20;
                    if(bytes_parsed)
                        *bytes_parsed += 20;
                    return JKSN_EOK;
                case 0xfb:
                    retval = jksn_parse_value(result, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(size < 32)
                        return JKSN_ETRUNC;
                    buffer += 32;
                    size -= 32;
                    if(bytes_parsed)
                        *bytes_parsed += 32;
                    return JKSN_EOK;
                case 0xfc:
                    retval = jksn_parse_value(result, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    if(size < 64)
                        return JKSN_ETRUNC;
                    buffer += 64;
                    size -= 64;
                    if(bytes_parsed)
                        *bytes_parsed += 64;
                    return JKSN_EOK;
                case 0xff:
                    retval = jksn_parse_value(&tmp, buffer, size, &child_size, cache);
                    if(retval != JKSN_EOK)
                        return retval;
                    tmp = jksn_free(tmp);
                    buffer += child_size;
                    size -= child_size;
                    if(bytes_parsed)
                        *bytes_parsed += child_size;
                    return jksn_parse_value(result, buffer, size, bytes_parsed, cache);
                }
            }
        }
        return JKSN_ECONTROL;
    }
}

static jksn_error_message_no jksn_parse_float(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed) {
    assert(sizeof (float) == 4);
    if(size < 4)
        return JKSN_ETRUNC;
    else {
        union {
            uint32_t data_int;
            float data_float;
        } conv = {
            ((uint32_t) buffer[0]) << 24 |
            ((uint32_t) buffer[1]) << 16 |
            ((uint32_t) buffer[2]) << 8 |
            ((uint32_t) buffer[3])
        };
        *result = jksn_malloc(sizeof (jksn_t));
        if(!*result)
            return JKSN_ENOMEM;
        (*result)->data_type = JKSN_FLOAT;
        (*result)->data_float = conv.data_float;
        if(bytes_parsed)
            *bytes_parsed += 4;
        return JKSN_EOK;
    }
}

static jksn_error_message_no jksn_parse_double(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed) {
    assert(sizeof (double) == 8);
    if(size < 8)
        return JKSN_ETRUNC;
    else {
        union {
            uint64_t data_int;
            double data_double;
        } conv = {
            ((uint64_t) buffer[0]) << 56 |
            ((uint64_t) buffer[1]) << 48 |
            ((uint64_t) buffer[2]) << 40 |
            ((uint64_t) buffer[3]) << 32 |
            ((uint64_t) buffer[4]) << 24 |
            ((uint64_t) buffer[5]) << 16 |
            ((uint64_t) buffer[6]) << 8 |
            ((uint64_t) buffer[7])
        };
        *result = jksn_malloc(sizeof (jksn_t));
        if(!*result)
            return JKSN_ENOMEM;
        (*result)->data_type = JKSN_DOUBLE;
        (*result)->data_double = conv.data_double;
        if(bytes_parsed)
            *bytes_parsed += 8;
        return JKSN_EOK;
    }
}

static jksn_error_message_no jksn_parse_longdouble(jksn_t **result, const char *buffer, size_t size, size_t *bytes_parsed) {
    if(size < 10)
        return JKSN_ETRUNC;
    else if(sizeof (long double) == 12) {
        union {
            uint8_t data_int[12];
            long double data_long_double;
            uint8_t endiantest;
        } conv = {{1, 0}};
        int little_endian = (conv.endiantest == 1);
        *result = jksn_malloc(sizeof (jksn_t));
        if(!*result)
            return JKSN_ENOMEM;
        if(little_endian) {
            conv.data_int[0] = (uint8_t) buffer[9];
            conv.data_int[1] = (uint8_t) buffer[8];
            conv.data_int[2] = (uint8_t) buffer[7];
            conv.data_int[3] = (uint8_t) buffer[6];
            conv.data_int[4] = (uint8_t) buffer[5];
            conv.data_int[5] = (uint8_t) buffer[4];
            conv.data_int[6] = (uint8_t) buffer[3];
            conv.data_int[7] = (uint8_t) buffer[2];
            conv.data_int[8] = (uint8_t) buffer[1];
            conv.data_int[9] = (uint8_t) buffer[0];
            conv.data_int[10] = 0;
            conv.data_int[11] = 0;
        } else {
            conv.data_int[0] = 0;
            conv.data_int[1] = 0;
            conv.data_int[2] = (uint8_t) buffer[0];
            conv.data_int[3] = (uint8_t) buffer[1];
            conv.data_int[4] = (uint8_t) buffer[2];
            conv.data_int[5] = (uint8_t) buffer[3];
            conv.data_int[6] = (uint8_t) buffer[4];
            conv.data_int[7] = (uint8_t) buffer[5];
            conv.data_int[8] = (uint8_t) buffer[6];
            conv.data_int[9] = (uint8_t) buffer[7];
            conv.data_int[10] = (uint8_t) buffer[8];
            conv.data_int[11] = (uint8_t) buffer[9];
        }
        (*result)->data_type = JKSN_LONG_DOUBLE;
        (*result)->data_long_double = conv.data_long_double;
        if(bytes_parsed)
            *bytes_parsed += 10;
        return JKSN_EOK;
    } else if(sizeof (long double) == 16) {
        union {
            uint8_t data_int[16];
            long double data_long_double;
            uint8_t endiantest;
        } conv = {{1, 0}};
        int little_endian = (conv.endiantest == 1);
        *result = jksn_malloc(sizeof (jksn_t));
        if(!*result)
            return JKSN_ENOMEM;
        if(little_endian) {
            conv.data_int[0] = (uint8_t) buffer[9];
            conv.data_int[1] = (uint8_t) buffer[8];
            conv.data_int[2] = (uint8_t) buffer[7];
            conv.data_int[3] = (uint8_t) buffer[6];
            conv.data_int[4] = (uint8_t) buffer[5];
            conv.data_int[5] = (uint8_t) buffer[4];
            conv.data_int[6] = (uint8_t) buffer[3];
            conv.data_int[7] = (uint8_t) buffer[2];
            conv.data_int[8] = (uint8_t) buffer[1];
            conv.data_int[9] = (uint8_t) buffer[0];
            conv.data_int[10] = 0;
            conv.data_int[11] = 0;
            conv.data_int[12] = 0;
            conv.data_int[13] = 0;
            conv.data_int[14] = 0;
            conv.data_int[15] = 0;
        } else {
            conv.data_int[0] = 0;
            conv.data_int[1] = 0;
            conv.data_int[2] = 0;
            conv.data_int[3] = 0;
            conv.data_int[4] = 0;
            conv.data_int[5] = 0;
            conv.data_int[6] = (uint8_t) buffer[0];
            conv.data_int[7] = (uint8_t) buffer[1];
            conv.data_int[8] = (uint8_t) buffer[2];
            conv.data_int[9] = (uint8_t) buffer[3];
            conv.data_int[10] = (uint8_t) buffer[4];
            conv.data_int[11] = (uint8_t) buffer[5];
            conv.data_int[12] = (uint8_t) buffer[6];
            conv.data_int[13] = (uint8_t) buffer[7];
            conv.data_int[14] = (uint8_t) buffer[8];
            conv.data_int[15] = (uint8_t) buffer[9];
        }
        (*result)->data_type = JKSN_LONG_DOUBLE;
        (*result)->data_long_double = conv.data_long_double;
        if(bytes_parsed)
            *bytes_parsed += 10;
        return JKSN_EOK;
    } else
        return JKSN_ELONGDOUBLE;
}

static jksn_error_message_no jksn_decode_int(uint64_t *result, const char *buffer, size_t bufsize, size_t size, size_t *bytes_parsed) {
    switch(size) {
    case 1:
        if(bufsize < 1)
            return JKSN_ETRUNC;
        *result = (uint64_t) (uint8_t) buffer[0];
        if(bytes_parsed)
            (*bytes_parsed)++;
        break;
    case 2:
        if(bufsize < 2)
            return JKSN_ETRUNC;
        *result = ((uint64_t) (uint8_t) buffer[0]) << 8 |
                  ((uint64_t) (uint8_t) buffer[1]);
        if(bytes_parsed)
            *bytes_parsed += 2;
        break;
    case 4:
        if(bufsize < 4)
            return JKSN_ETRUNC;
        *result = ((uint64_t) (uint8_t) buffer[0]) << 24 |
                  ((uint64_t) (uint8_t) buffer[1]) << 16 |
                  ((uint64_t) (uint8_t) buffer[2]) << 8 |
                  ((uint64_t) (uint8_t) buffer[3]);
        if(bytes_parsed)
            *bytes_parsed += 4;
        break;
    case 0:
        {
            uint8_t thisbyte;
            *result = 0;
            do {
                if(bufsize-- == 0)
                    return JKSN_ETRUNC;
                if(*result & 0xfe00000000000000LL)
                    return JKSN_EVARINT;
                thisbyte = (buffer++)[0];
                *result = (*result << 7) | (thisbyte & 0x7f);
                if(bytes_parsed)
                    (*bytes_parsed)++;
            } while(thisbyte & 0x80);
        }
        break;
    default:
        assert(size == 1 || size == 2 || size == 4 || size == 0);
    }
    return JKSN_EOK;
}

static int jksn_utf8_check_continuation(const char *utf8str, size_t length, size_t check_length) {
    if(check_length < length) {
        while(check_length--)
            if((*++utf8str & 0xc0) != 0x80)
                return 0;
        return 1;
    } else
        return 0;
}

static size_t jksn_utf8_to_utf16(uint16_t *utf16str, const char *utf8str, size_t utf8size, int strict) {
    size_t reslen = 0;
    while(utf8size) {
        if((uint8_t) utf8str[0] < 0x80) {
            if(utf16str)
                utf16str[reslen] = utf8str[0];
            reslen++;
            utf8str++;
            utf8size--;
            continue;
        } else if((uint8_t) utf8str[0] < 0xc0) {
        } else if((uint8_t) utf8str[0] < 0xe0) {
            if(jksn_utf8_check_continuation(utf8str, utf8size, 1)) {
                uint32_t ucs4 = (utf8str[0] & 0x1f) << 6 | (utf8str[1] & 0x3f);
                if(ucs4 >= 0x80) {
                    if(utf16str)
                        utf16str[reslen] = ucs4;
                    reslen++;
                    utf8str += 2;
                    utf8size -= 2;
                    continue;
                }
            }
        } else if((uint8_t) utf8str[0] < 0xf0) {
            if(jksn_utf8_check_continuation(utf8str, utf8size, 2)) {
                uint32_t ucs4 = (utf8str[0] & 0xf) << 12 | (utf8str[1] & 0x3f) << 6 | (utf8str[2] & 0x3f);
                if(ucs4 >= 0x800 && (ucs4 & 0xf800) != 0xd800) {
                    if(utf16str)
                        utf16str[reslen] = ucs4;
                    reslen++;
                    utf8str += 3;
                    utf8size -= 3;
                    continue;
                }
            }
        } else if((uint8_t) utf8str[0] < 0xf8) {
            if(jksn_utf8_check_continuation(utf8str, utf8size, 3)) {
                uint32_t ucs4 = (utf8str[0] & 0x7) << 18 | (utf8str[1] & 0x3f) << 12 | (utf8str[2] & 0x3f) << 6 | (utf8str[3] & 0x3f);
                if(ucs4 >= 0x10000 && ucs4 < 0x110000) {
                    if(utf16str) {
                        ucs4 -= 0x10000;
                        utf16str[reslen] = ucs4 >> 10 | 0xd800;
                        utf16str[reslen+1] = (ucs4 & 0x3ff) | 0xdc00;
                    }
                    reslen += 2;
                    utf8str += 4;
                    utf8size -= 4;
                    continue;
                }
            }
        }
        if(strict)
            return (size_t) (ptrdiff_t) -1;
        else {
            if(utf16str)
                utf16str[reslen] = 0xfffd;
            reslen++;
            utf8str++;
            utf8size--;
        }
    }
    return reslen;
}

static size_t jksn_utf16_to_utf8(char *utf8str, const uint16_t *utf16str, size_t utf16size) {
    size_t reslen = 0;
    while(utf16size) {
        if(utf16str[0] < 0x80) {
            if(utf8str)
                utf8str[reslen] = utf16str[0];
            reslen++;
            utf16str++;
            utf16size--;
        } else if(utf16str[0] < 0x800) {
            if(utf8str) {
                utf8str[reslen] = utf16str[0] >> 6 | 0xc0;
                utf8str[reslen+1] = (utf16str[0] & 0x3f) | 0x80;
            }
            reslen += 2;
            utf16str++;
            utf16size--;
        } else if((utf16str[0] & 0xf800) != 0xd800) {
            if(utf8str) {
                utf8str[reslen] = utf16str[0] >> 12 | 0xe0;
                utf8str[reslen+1] = ((utf16str[0] >> 6) & 0x3f) | 0x80;
                utf8str[reslen+2] = (utf16str[0] & 0x3f) | 0x80;
            }
            reslen += 3;
            utf16str++;
            utf16size--;
        } else if(utf16size != 1 && (utf16str[0] & 0xfc00) == 0xd800 && (utf16str[1] & 0xfc00) == 0xdc00) {
            uint32_t ucs4 = ((utf16str[0] & 0x3ff) << 10 | (utf16str[1] & 0x3ff)) + 0x10000;
            if(utf8str) {
                utf8str[reslen] = ucs4 >> 18 | 0xf0;
                utf8str[reslen+1] = ((ucs4 >> 12) & 0x3f) | 0x80;
                utf8str[reslen+2] = ((ucs4 >> 6) & 0x3f) | 0x80;
                utf8str[reslen+3] = (ucs4 & 0x3f) | 0x80;
            }
            reslen += 4;
            utf16str += 2;
            utf16size -= 2;
        } else {
            if(utf8str) {
                utf8str[reslen] = 0xef;
                utf8str[reslen+1] = 0xbf;
                utf8str[reslen+2] = 0xbd;
            }
            reslen += 3;
            utf16str++;
            utf16size--;
        }
    }
    return reslen;
}

static int jksn_compare(const jksn_t *obj1, const jksn_t *obj2) {
    if(obj1 == obj2)
        return 0;
    else if(obj1->data_type != obj2->data_type)
        return 1;
    else switch(obj1->data_type) {
        case JKSN_UNDEFINED:
        case JKSN_NULL:
            return 0;
        case JKSN_BOOL:
            return obj1->data_bool != obj2->data_bool;
        case JKSN_INT:
            return obj1->data_int != obj2->data_int;
        case JKSN_FLOAT:
            return obj1->data_float != obj2->data_float;
        case JKSN_DOUBLE:
            return obj1->data_double != obj2->data_double;
        case JKSN_LONG_DOUBLE:
            return obj1->data_long_double != obj2->data_long_double;
        case JKSN_STRING:
            return obj1->data_string.size != obj2->data_string.size ||
                   memcmp(obj1->data_string.str, obj2->data_string.str, obj1->data_string.size);
        case JKSN_BLOB:
            return obj1->data_blob.size != obj2->data_blob.size ||
                   memcmp(obj1->data_blob.buf, obj2->data_blob.buf, obj1->data_blob.size);
        case JKSN_UNSPECIFIED:
            return 0;
        default:
            return 1;
    }
}

static jksn_t *jksn_duplicate(const jksn_t *object) {
    jksn_t *result = object ? jksn_malloc(sizeof (jksn_t)) : NULL;
    if(result) {
        size_t i;
        *result = *object;
        switch(object->data_type) {
        case JKSN_STRING:
            result->data_string.str = jksn_malloc(object->data_string.size + 1);
            if(!result->data_string.str) {
                free(result);
                result = NULL;
            }
            memcpy(result->data_string.str, object->data_string.str, object->data_string.size);
            result->data_string.str[object->data_string.size] = '\0';
            break;
        case JKSN_BLOB:
            result->data_blob.buf = jksn_malloc(object->data_blob.size + 1);
            if(!result->data_blob.buf) {
                free(result);
                result = NULL;
            }
            memcpy(result->data_blob.buf, object->data_blob.buf, object->data_blob.size);
            result->data_blob.buf[object->data_blob.size] = '\0';
            break;
        case JKSN_ARRAY:
            result->data_array.children = jksn_calloc(object->data_array.size, sizeof (jksn_t *));
            if(!result->data_array.children) {
                free(result);
                result = NULL;
            }
            for(i = 0; i < object->data_array.size; i++)
                if(!(result->data_array.children[i] = jksn_duplicate(object->data_array.children[i]))) {
                    result = jksn_free(result);
                    break;
                }
            break;
        case JKSN_OBJECT:
            result->data_object.children = jksn_calloc(object->data_object.size, sizeof (jksn_keyvalue));
            if(!result->data_object.children) {
                free(result);
                result = NULL;
            }
            for(i = 0; i < object->data_object.size; i++) {
                if(!(result->data_object.children[i].key = jksn_duplicate(object->data_object.children[i].key))) {
                    result = jksn_free(result);
                    break;
                }
                if(!(result->data_object.children[i].value = jksn_duplicate(object->data_object.children[i].value))) {
                    result = jksn_free(result);
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
    return result;
}

static uint8_t jksn_djbhash(const char *buf, size_t size) {
    unsigned int result = 0;
    size_t i;
    for(i = 0; i < size; i++)
        result += (result << 5) + (uint8_t) buf[i];
    return (uint8_t) result;
}

static uint16_t jksn_uint16_to_le(uint16_t n) {
    union {
        uint16_t word;
        uint8_t byte;
    } endiantest = {1};
    if(endiantest.byte == 1)
        return n;
    else
        return (n << 8) | (n >> 8);
}

const char *jksn_errcode(int errcode) {
    if(errcode >= 0 && (size_t) errcode < sizeof jksn_error_messages/sizeof jksn_error_messages[0])
        return jksn_error_messages[errcode];
    else
        return NULL;
}
