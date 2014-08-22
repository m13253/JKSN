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
    "success",
    "no enough memory",
    "JKSN stream may be truncated or corrupted",
    "JKSN stream contains an invalid control byte",
    "this JKSN decoder does not support JSON literals",
    "this build of JKSN decoder does not support variable length integers",
    "this build of JKSN decoder does not support long double numbers",
    "JKSN stream requires a non-existing hash",
    "JKSN stream contains an invalid delta encoded integer",
    "JKSN row-col swapped array requires an array but not found"
};
typedef enum {
    JKSN_EOK,
    JKSN_ENOMEM,
    JKSN_ETRUNC,
    JKSN_ECONTROL,
    JKSN_EJSON,
    JKSN_ELONGDOUBLE,
    JKSN_EHASH,
    JKSN_EDELTA,
    JKSN_ESWAPARRAY
} jksn_error_message_no;

jksn_cache *jksn_cache_new(void) {
    return calloc(1, sizeof (struct jksn_cache));
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

const char *jksn_errcode(int errcode) {
    if(errcode >= 0 && errcode < sizeof jksn_error_messages/sizeof jksn_error_messages[0])
        return jksn_error_messages[errcode];
    else
        return NULL;
}
