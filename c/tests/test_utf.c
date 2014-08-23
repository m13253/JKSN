#include <stdio.h>
#include "../jksn.c"

int to_utf8 = 0;

int main(int argc, char *argv[]) {
    int argi;
    for(argi = 1; argi < argc; argi++)
        if(argv[argi][0] == '-' && argv[argi][1] == 'd' && argv[argi][2] == '\0') {
            to_utf8 = 1;
            break;
        }
    if(to_utf8) {
        uint16_t buf[1024];
        if(fgets((char *) buf, 2048, stdin)) {
            size_t len;
            for(len = 0; buf[len]; len++) {
            }
            fprintf(stderr, "Input length = %zu\n", len);
            char *out = malloc(jksn_utf16_to_utf8(NULL, buf, len));
            fwrite(out, 1, jksn_utf16_to_utf8(out, buf, len), stdout);
            free(out);
        }
    } else {
        char buf[1024];
        if(fgets((char *) buf, 1024, stdin)) {
            size_t len;
            for(len = 0; buf[len]; len++) {
            }
            fprintf(stderr, "Input length = %zu\n", len);
            uint16_t *out = malloc(jksn_utf8_to_utf16(NULL, buf, len) * sizeof (uint16_t));
            fwrite(out, sizeof (uint16_t), jksn_utf8_to_utf16(out, buf, len), stdout);
            free(out);
        }
    }
    return 0;
}
