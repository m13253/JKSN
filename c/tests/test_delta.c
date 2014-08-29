#include <stdio.h>
#include "jksn.h"

jksn_t children[] = {
    { JKSN_INT, { .data_int = 100 } },
    { JKSN_INT, { .data_int = 101 } },
    { JKSN_INT, { .data_int = 99 } },
    { JKSN_INT, { .data_int = 130 } },
    { JKSN_INT, { .data_int = 1000 } }
};

jksn_t *children_array[] = {
    &children[0], &children[1], &children[2], &children[3], &children[4]
};

jksn_t object = {
    JKSN_ARRAY,
    {
        .data_array = {
            .size = 5,
            .children = children_array
        }
    }
};

int main(void) {
    jksn_blobstring *result;
    int retval = jksn_dump(&result, &object, 1, NULL);
    fprintf(stderr, "retval = %d (%s)\n", retval, jksn_errcode(retval));
    fwrite(result->buf, 1, result->size, stdout);
    result = jksn_blobstring_free(result);
    return retval;
}
