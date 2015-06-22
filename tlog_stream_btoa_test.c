#include <stdio.h>
#include "tlog_stream.c"

int
main(void)
{
    uint8_t i = 0;
    uint8_t btoa_buf[4] = {0,};
    size_t btoa_rc;
    uint8_t snprintf_buf[4] = {0,};
    int snprintf_rc;
    do {
        snprintf_rc = snprintf((char *)snprintf_buf, sizeof(snprintf_buf),
                               "%hhu", i);
        btoa_rc = tlog_stream_btoa(btoa_buf, sizeof(btoa_buf), i);
        if (btoa_rc != (size_t)snprintf_rc) {
            fprintf(stderr, "%hhu: rc mismatch: %zu != %d\n",
                    i, btoa_rc, snprintf_rc);
            return 1;
        }
        if (memcmp(btoa_buf, snprintf_buf, 4) != 0) {
            fprintf(stderr,
                    "%hhu: buffer mismatch: "
                    "%02hhx %02hhx %02hhx %02hhx != "
                    "%02hhx %02hhx %02hhx %02hhx\n",
                    i,
                    btoa_buf[0], btoa_buf[1],
                    btoa_buf[2], btoa_buf[3], 
                    snprintf_buf[0], snprintf_buf[1],
                    snprintf_buf[2], snprintf_buf[3]);
            return 1;
        }
        printf("%hhu: %zu == %d, %s == %s\n",
               i, btoa_rc, snprintf_rc, btoa_buf, snprintf_buf);
    } while (i++ != 255);
    return 0;
}
