#include <stdio.h>
#include "jksn.h"

jksn_t key[] = {
    { JKSN_STRING, { .data_string = { 4, "name" } } },
    { JKSN_STRING, { .data_string = { 5, "email" } } },
    { JKSN_STRING, { .data_string = { 5, "phone" } } },

    { JKSN_STRING, { .data_string = { 4, "name" } } },
    { JKSN_STRING, { .data_string = { 3, "age" } } },
    { JKSN_STRING, { .data_string = { 5, "email" } } },
    { JKSN_STRING, { .data_string = { 4, "phone" } } },
};

jksn_t value[] = {
    { JKSN_STRING, { .data_string = { 5, "Jason" } } },
    { JKSN_STRING, { .data_string = { 17, "jason@example.com" } } },
    { JKSN_STRING, { .data_string = { 12, "777-777-7777" } } },

    { JKSN_STRING, { .data_string = { 7, "Jackson" } } },
    { JKSN_INT, { .data_int = 17 } },
    { JKSN_STRING, { .data_string = { 19, "jackson@example.com" } } },
    { JKSN_STRING, { .data_string = { 12, "888-888-8888" } } },
};

jksn_keyvalue object_jason_kv[] = {
    { &key[0], &value[0] }, { &key[1], &value[1] }, { &key[2], &value[2] }
};

jksn_keyvalue object_jackson_kv[] = {
    { &key[3], &value[3] }, { &key[4], &value[4] }, { &key[5], &value[5] }, { &key[6], &value[6] }
};

jksn_t object_jason_jackson[] = {
    { JKSN_OBJECT, { .data_object = { .size = 3, .children = object_jason_kv } } },
    { JKSN_OBJECT, { .data_object = { .size = 4, .children = object_jackson_kv } } },
};

jksn_t *object_array[] = {
    &object_jason_jackson[0], &object_jason_jackson[1]
};

jksn_t object = {
    JKSN_ARRAY,
    {
        .data_array = {
            .size = 2,
            .children = object_array
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
