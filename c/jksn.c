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

static jksn_value *jksn_value_new(const jksn_t *origin, uint8_t control, const jksn_blobstring *data, const jksn_blobstring *buf);
static jksn_value *jksn_value_free(jksn_value *object);
static size_t jksn_value_size(const jksn_value *object, int depth);
static char *jksn_value_output(char output[], const jksn_value *object);
static jksn_error_message_no jksn_dump_value(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_int(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_float(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_double(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_longdouble(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_string(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_blob(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_array(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_object(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_optimize(jksn_value *object, jksn_cache *cache);
static size_t jksn_encode_int(char result[], uint64_t object, size_t size);
static size_t jksn_utf8_to_utf16(uint16_t *utf16str, const char *utf8str, size_t utf8size, int strict);
static size_t jksn_utf16_to_utf8(char *utf8str, const uint16_t *utf16str, size_t utf16size);
static uint8_t jksn_djbhash(jksn_blobstring *buf);
static uint16_t jksn_htons(uint16_t n);

jksn_cache *jksn_cache_new(void) {
    return calloc(1, sizeof (struct jksn_cache));
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
    }
    return NULL;
}

static jksn_value *jksn_value_new(const jksn_t *origin, uint8_t control, const jksn_blobstring *data, const jksn_blobstring *buf) {
    jksn_value *result = calloc(1, sizeof (jksn_value));
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
            jksn_value_output(output, object->first_child);
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
                retval = jksn_optimize(result_value, cache);
                if(retval == JKSN_EOK && result) {
                    *result = malloc(sizeof (jksn_blobstring));
                    if(header) {
                        (*result)->size = jksn_value_size(result_value, 0) + 3;
                        (*result)->buf = malloc((*result)->size);
                        (*result)->buf[0] = 'j';
                        (*result)->buf[1] = 'k';
                        (*result)->buf[2] = '!';
                        jksn_value_output((*result)->buf + 3, result_value);
                    } else {
                        (*result)->size = jksn_value_size(result_value, 0);
                        (*result)->buf = malloc((*result)->size);
                        jksn_value_output((*result)->buf, result_value);
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
        retval = jksn_dump_string(result, object, cache);
        break;
    case JKSN_BLOB:
        retval = jksn_dump_blob(result, object, cache);
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
        jksn_blobstring data = {1, malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 1);
        *result = jksn_value_new(object, 0x1d, &data, NULL);
    } else if(object->data_int >= -0x8000 && object->data_int <= 0x7fff) {
        jksn_blobstring data = {2, malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 2);
        *result = jksn_value_new(object, 0x1c, &data, NULL);
    } else if((object->data_int >= -0x80000000L && object->data_int <= -0x200000L) ||
              (object->data_int >= 0x200000L && object->data_int <= 0x7fffffffL)) {
        jksn_blobstring data = {4, malloc(4)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, (uint64_t) object->data_int, 4);
        *result = jksn_value_new(object, 0x1b, &data, NULL);
    } else if(object->data_int >= 0) {
        jksn_blobstring data = {0, malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, (uint64_t) object->data_int, 0);
        *result = jksn_value_new(object, 0x1e, &data, NULL);
    } else {
        jksn_blobstring data = {0, malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, (uint64_t) -object->data_int, 0);
        *result = jksn_value_new(object, 0x1f, &data, NULL);
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
        jksn_blobstring data = {4, malloc(4)};
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
        jksn_blobstring data = {8, malloc(8)};
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
        jksn_blobstring data = {10, malloc(10)};
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
        jksn_blobstring data = {10, malloc(10)};
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

static jksn_error_message_no jksn_dump_string(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    size_t utf16size = jksn_utf8_to_utf16(NULL, object->data_string.str, object->data_string.size, 1);
    if(utf16size != (size_t) (ptrdiff_t) -1 && utf16size*2 < object->data_string.size) {
        uint16_t *utf16str = malloc(utf16size*2);
        jksn_blobstring buf = {0, (char *) utf16str};
        size_t i;
        if(!utf16str)
            return JKSN_ENOMEM;
        buf.size = jksn_utf8_to_utf16(utf16str, object->data_string.str, object->data_string.size, 1) * 2;
        assert(buf.size == utf16size * 2);
        for(i = 0; i < utf16size; i++)
            utf16str[i] = jksn_htons(utf16str[i]);
        if(utf16size <= 0xb)
            *result = jksn_value_new(object, 0x30 | utf16size, NULL, &buf);
        else if(utf16size <= 0xff) {
            jksn_blobstring data = {1, malloc(1)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            jksn_encode_int(data.buf, utf16size, 1);
            *result = jksn_value_new(object, 0x3e, &data, &buf);
        } else if(utf16size <= 0xffff) {
            jksn_blobstring data = {2, malloc(2)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            jksn_encode_int(data.buf, utf16size, 2);
            *result = jksn_value_new(object, 0x3d, &data, &buf);
        } else {
            jksn_blobstring data = {0, malloc(10)};
            if(!data.buf) {
                free(utf16str);
                return JKSN_ENOMEM;
            }
            data.size = jksn_encode_int(data.buf, utf16size, 0);
            *result = jksn_value_new(object, 0x3f, &data, &buf);
        }
    } else {
        jksn_blobstring buf = {object->data_string.size, malloc(object->data_string.size)};
        if(!buf.buf)
            return JKSN_ENOMEM;
        memcpy(buf.buf, object->data_string.str, object->data_string.size);
        if(buf.size <= 0xc)
            *result = jksn_value_new(object, 0x40 | buf.size, NULL, &buf);
        else if(buf.size <= 0xff) {
            jksn_blobstring data = {1, malloc(1)};
            if(!data.buf)
                return JKSN_ENOMEM;
            jksn_encode_int(data.buf, buf.size, 1);
            *result = jksn_value_new(object, 0x4e, &data, &buf);
        } else if(buf.size <= 0xffff) {
            jksn_blobstring data = {2, malloc(2)};
            if(!data.buf)
                return JKSN_ENOMEM;
            jksn_encode_int(data.buf, buf.size, 2);
            *result = jksn_value_new(object, 0x4d, &data, &buf);
        } else {
            jksn_blobstring data = {0, malloc(10)};
            if(!data.buf)
                return JKSN_ENOMEM;
            data.size = jksn_encode_int(data.buf, buf.size, 0);
            *result = jksn_value_new(object, 0x4f, &data, &buf);
        }
    }
    if(*result) {
        (*result)->hash = jksn_djbhash(&(*result)->buf);
        return JKSN_EOK;
    } else
        return JKSN_ENOMEM;
}

static jksn_error_message_no jksn_dump_blob(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    jksn_blobstring buf = {object->data_blob.size, malloc(object->data_blob.size)};
    if(!buf.buf)
        return JKSN_ENOMEM;
    memcpy(buf.buf, object->data_blob.buf, object->data_blob.size);
    if(object->data_blob.size <= 0xb)
        *result = jksn_value_new(object, 0x50 | object->data_blob.size, NULL, &object->data_blob);
    else if(object->data_blob.size <= 0xff) {
        jksn_blobstring data = {1, malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_blob.size, 1);
        *result = jksn_value_new(object, 0x5e, &data, &object->data_blob);
    } else if(object->data_blob.size <= 0xffff) {
        jksn_blobstring data = {2, malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_blob.size, 2);
        *result = jksn_value_new(object, 0x5d, &data, &object->data_blob);
    } else {
        jksn_blobstring data = {0, malloc(10)};
        if(!data.buf)
            return JKSN_ENOMEM;
        data.size = jksn_encode_int(data.buf, object->data_blob.size, 0);
        *result = jksn_value_new(object, 0x5f, &data, &object->data_blob);
    }
    if(*result) {
        (*result)->hash = jksn_djbhash(&(*result)->buf);
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
        jksn_blobstring data = {1, malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_array.size, 1);
        *result = jksn_value_new(object, 0x8e, &data, NULL);
    } else if(object->data_array.size <= 0xffff) {
        jksn_blobstring data = {2, malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_array.size, 2);
        *result = jksn_value_new(object, 0x8d, &data, NULL);
    } else {
        jksn_blobstring data = {0, malloc(10)};
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
        }
        return JKSN_EOK;
    }
}

static jksn_error_message_no jksn_dump_array(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    jksn_error_message_no retval = jksn_encode_straight_array(result, object, cache);
    if(retval != JKSN_EOK)
        return retval;
    return JKSN_EOK;
}

static jksn_error_message_no jksn_dump_object(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    if(object->data_object.size <= 0xc)
        *result = jksn_value_new(object, 0x90 | object->data_object.size, NULL, NULL);
    else if(object->data_object.size <= 0xff) {
        jksn_blobstring data = {1, malloc(1)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_object.size, 1);
        *result = jksn_value_new(object, 0x9e, &data, NULL);
    } else if(object->data_object.size <= 0xffff) {
        jksn_blobstring data = {2, malloc(2)};
        if(!data.buf)
            return JKSN_ENOMEM;
        jksn_encode_int(data.buf, object->data_object.size, 2);
        *result = jksn_value_new(object, 0x9d, &data, NULL);
    } else {
        jksn_blobstring data = {0, malloc(10)};
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

static jksn_error_message_no jksn_optimize(jksn_value *result, jksn_cache *cache) {
    return JKSN_EOK;
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

static uint8_t jksn_djbhash(jksn_blobstring *buf) {
    unsigned int result = 0;
    size_t i;
    for(i = 0; i < buf->size; i++)
        result += (result << 5) + (uint8_t) buf->buf[i];
    return (uint8_t) result;
}

static uint16_t jksn_htons(uint16_t n) {
    union {
        uint16_t word;
        uint8_t byte;
    } endiantest = {1};
    if(endiantest.byte == 1)
        return (n << 8) | (n >> 8);
    else
        return n;
}

const char *jksn_errcode(int errcode) {
    if(errcode >= 0 && errcode < sizeof jksn_error_messages/sizeof jksn_error_messages[0])
        return jksn_error_messages[errcode];
    else
        return NULL;
}
