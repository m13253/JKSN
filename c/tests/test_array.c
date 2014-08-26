#include <stdio.h>
#include "jksn.h"

jksn_t children[] = {
    { JKSN_STRING, { .data_string = { 7, "element" } } },
    { JKSN_STRING, { .data_string = { 6, "元素" } } },
    { JKSN_STRING, { .data_string = { 7, "element" } } },
    /* { JKSN_STRING, { .data_string = { 6, "元素" } } } */
};

jksn_t *children_array[] = {
    &children[0], &children[1], &children[2], &children[1]
};

jksn_t object = {
    JKSN_ARRAY,
    {
        .data_array = {
            .size = 4,
            .children = children_array
        }
    }
};

int main(int argc, char *argv[]) {
    jksn_blobstring *result;
    int retval = jksn_dump(&result, &object, 1, NULL);
    fprintf(stderr, "retval = %d (%s)\n", retval, jksn_errcode(retval));
    fwrite(result->buf, 1, result->size, stdout);
    result = jksn_blobstring_free(result);
    return retval;
}
