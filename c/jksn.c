#include "jksn.h"

typedef struct jksn_value {
    const jksn_t *origin;
    char control;
    jksn_blobstring data;
    jksn_blobstring buf;
} jksn_value;

static const char *jksn_error_messages[] = {
    "success",
    "JKSN stream may be truncated or corrupted",
    "this JKSN decoder does not support JSON literals",
    "this build of JKSN decoder does not support long double numbers",
    "JKSN stream requires a non-existing hash",
    "JKSN stream contains an invalid delta encoded integer",
    "JKSN stream contains an invalid control byte",
    "JKSN row-col swapped array requires an array but not found"
};

const char *jksn_errcode(int errcode) {
    if(errcode >= 0 && errcode < sizeof jksn_error_messages/sizeof jksn_error_messages[0])
        return jksn_error_messages[errcode];
    else
        return NULL;
}
