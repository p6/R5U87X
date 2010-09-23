/*
 * fw-extract.c
 * Produces Intel HEX files from Ricoh webcam microcode.
 * 
 * Code by Marcin Cieslak <saper@saper.info>, 2009
 * Released into the public domain.
 * 
 * Modifications:
 * uint_8 -> unsigned int so we can compile without kernel headers.
 * Added usage with no args.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

struct fwchunk {
    int len;
    unsigned int addr[2];
    unsigned int bytes[2];
};

int
main(int argc, const char *argv[])
{
#define MAXFWSIZE 32768
    unsigned int fwbuf[MAXFWSIZE];
    unsigned int *ptr, *endptr;
    struct fwchunk *chkptr;
    ssize_t fwsize, chksiz;
    int firmware = 0;
    int i;
    const char *fwname;
    unsigned int cksum;

    if (argc >= 2) {
            fwname = argv[1];
    
            if ((firmware = open(fwname, O_RDONLY)) < 0) {
                perror("open firmware");
                return 3;
            }
    } else {
        printf("usage: fw-extract <firmware>\n");
        return 2;
    }
    fwsize = read(firmware, &fwbuf, MAXFWSIZE);
    if (fwsize == 0) {
        perror("read firmware");
        return 4;
    }
    close(firmware);
    if (fwsize == MAXFWSIZE)    
        return 1;
    ptr = fwbuf;
    endptr = fwbuf + fwsize;
    for (chkptr = (struct fwchunk *)fwbuf;
        ptr < endptr; ) {
        chksiz = chkptr->len;
        printf(":%02X%02X%02X%02X", (unsigned int)chksiz, 
            chkptr->addr[1], 
            chkptr->addr[0], 
            0);
        cksum = chksiz + chkptr->addr[1] + chkptr->addr[2];
        for (i = 0; i < chksiz; i ++)  {
            printf("%02X", chkptr->bytes[i]); 
            cksum += chkptr->bytes[i];
        }
        printf("%02X\n", ((cksum ^ 0xff) + 1) & 0xff);
        chkptr = (struct fwchunk *)(ptr += (chksiz + 3));
    }
    printf(":00000001FF\n");
    return 0;
}
