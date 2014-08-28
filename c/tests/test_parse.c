#include <stdio.h>
#include "jksn.h"

char buf[4096];

int main(int argc, char *argv[]) {
    int retval;
    jksn_blobstring bufin = { .size = 0, .buf = buf };
    jksn_blobstring *bufout;
    jksn_t *result;
    size_t bytes_parsed = 0xcccccccc;
    bufin.size = fread(bufin.buf, 1, 4096, stdin);
    retval = jksn_parse(&result, &bufin, &bytes_parsed, NULL);
    if(retval != 0) {
        fprintf(stderr, "Parse error %d: %s\n", retval, jksn_errcode(retval));
        return retval;
    }
    retval = jksn_dump(&bufout, result, 0, NULL);
    if(retval != 0) {
        fprintf(stderr, "Dump error %d: %s\n", retval, jksn_errcode(retval));
        return retval;
    }
    fwrite(bufout->buf, 1, bufout->size, stdout);
    bufout = jksn_blobstring_free(bufout);
    return retval;
}
