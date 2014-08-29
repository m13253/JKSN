#include <stdio.h>
#include "jksn.h"

jksn_t object = {
    JKSN_DOUBLE,
    { .data_double = 4.2e100 }
};

int main(void) {
    jksn_blobstring *result;
    int retval = jksn_dump(&result, &object, 1, NULL);
    fprintf(stderr, "retval = %d (%s)\n", retval, jksn_errcode(retval));
    fwrite(result->buf, 1, result->size, stdout);
    result = jksn_blobstring_free(result);
    return retval;
}
