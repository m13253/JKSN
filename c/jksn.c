#include <assert.h>
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
static char *jksn_value_output(char *output, const jksn_value *object);
static jksn_error_message_no jksn_dump_value(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_int(jksn_value **result, const jksn_t *object, jksn_cache *cache);
static jksn_error_message_no jksn_dump_float(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_double(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_dump_longdouble(jksn_value **result, const jksn_t *object);
static jksn_error_message_no jksn_optimize(jksn_value *object, jksn_cache *cache);
static size_t jksn_encode_int(char (*result)[10], uint64_t object, size_t size);

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

static char *jksn_value_output(char *output, const jksn_value *object) {
    while(object) {
        *(output++) = (char) object->control;
        if(object->data.size != 0) {
            memcpy(output, object->data.buf, object->data.size);
            output += object->data.size;
        }
        if(object->data.buf != 0) {
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
                    (*result)->size = jksn_value_size(result_value, 0);
                    (*result)->buf = malloc((*result)->size);
                    jksn_value_output((*result)->buf, result_value);
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
        if(sizeof (long double) == 16)
            retval = jksn_dump_longdouble(result, object);
        else
            retval = JKSN_ELONGDOUBLE;
    }
    return retval;
}

static jksn_error_message_no jksn_dump_int(jksn_value **result, const jksn_t *object, jksn_cache *cache) {
    jksn_blobstring buf;
    char bufbuf[10];
    if(object->data_int >= 0 && object->data_int <= 0xa)
        *result = jksn_value_new(object, 0x10 | object->data_int, NULL, NULL);
    else if(object->data_int >= -0x80 && object->data_int <= 0x7f) {
        buf.size = jksn_encode_int(&bufbuf, object->data_int, 1);
        buf.buf = bufbuf;
        *result = jksn_value_new(object, 0x1d, &buf, NULL);
    } else if(object->data_int >= -0x8000 && object->data_int <= 0x7fff) {
        buf.size = jksn_encode_int(&bufbuf, object->data_int, 2);
        buf.buf = bufbuf;
        *result = jksn_value_new(object, 0x1c, &buf, NULL);
    } else if((object->data_int >= -0x80000000L && object->data_int <= -0x200000L) || (object->data_int >= 0x200000L && object->data_int <= 0x7fffffffL)) {
        buf.size = jksn_encode_int(&bufbuf, object->data_int, 4);
        buf.buf = bufbuf;
        *result = jksn_value_new(object, 0x1b, &buf, NULL);
    } else if(object->data_int >= 0) {
        buf.size = jksn_encode_int(&bufbuf, object->data_int, 0);
        buf.buf = bufbuf;
        *result = jksn_value_new(object, 0x1e, &buf, NULL);
    } else {
        buf.size = jksn_encode_int(&bufbuf, -object->data_int, 0);
        buf.buf = bufbuf;
        *result = jksn_value_new(object, 0x1f, &buf, NULL);
    }
    return *result ? JKSN_EOK : JKSN_ENOMEM;
}

static jksn_error_message_no jksn_optimize(jksn_value *result, jksn_cache *cache) {
    return JKSN_EOK;
}

static size_t jksn_encode_int(char (*result)[10], uint64_t number, size_t size) {
    switch(size) {
    case 1:
        (*result)[0] = (char) (uint8_t) number;
        break;
    case 2:
        (*result)[0] = (char) (uint8_t) (number >> 8);
        (*result)[1] = (char) (uint8_t) number;
        break;
    case 4:
        (*result)[0] = (char) (uint8_t) (number >> 24);
        (*result)[1] = (char) (uint8_t) (number >> 16);
        (*result)[2] = (char) (uint8_t) (number >> 8);
        (*result)[3] = (char) (uint8_t) number;
        break;
    case 0:
        assert(number >= 0);
        (*result)[(size = 9)] = (char) (((uint8_t) number) & 0x7f);
        number >>= 7;
        while(number != 0) {
            (*result)[--size] = (char) (((uint8_t) number) | 0x80);
            number >>= 7;
        }
        assert((*result) <= (*result)+size);
        memmove((*result), (*result)+size, 10-size);
        size = 10-size;
        break;
    default:
        assert(size == 1 || size == 2 || size == 4 || size == 0);
    }
    return size;
}

const char *jksn_errcode(int errcode) {
    if(errcode >= 0 && errcode < sizeof jksn_error_messages/sizeof jksn_error_messages[0])
        return jksn_error_messages[errcode];
    else
        return NULL;
}
