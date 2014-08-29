#include <stdio.h>
#include "jksn.h"

jksn_t key[] = {
    { JKSN_STRING, { .data_string = { 3, "key" } } },
    { JKSN_STRING, { .data_string = { 3, "键" } } }
};

jksn_t value[] = {
    { JKSN_STRING, { .data_string = { 5, "value" } } },
    { JKSN_STRING, { .data_string = { 3, "值" } } }
};

jksn_keyvalue object_children[] = {
    { &key[0], &value[0] }, { &key[1], &value[1] }
};

jksn_t object = {
    JKSN_OBJECT,
    {
        .data_object = {
            .size = 2,
            .children = object_children
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
