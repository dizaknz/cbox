#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "crcsearch.h"

void usage (void) {
    const char *usage = "NAME                                           \n\
       crcsearch                                                        \n\
                                                                        \n\
SYNOPSIS                                                                \n\
       crcsearch -i file -q crc checksum                                \n\
                                                                        \n\
DESCRIPTION                                                             \n\
       Searches file for a given checksum and report it's range in file \n\
\n";

    fprintf (stdout, "%s", usage);
}

CRCSearch * CRCSearch_new (uint64 poly) {
    CRCSearch *this = (CRCSearch *) calloc (1, sizeof (CRCSearch));
    if (!this) {
        fprintf (stderr, "Out of memory (table)\n");
        return NULL;
    }
    this->init = &CRCSearch_init;
    this->search = &CRCSearch_search;
    this->close = &CRCSearch_close;
    this->found = 0;
    this->length = 0ULL;

    this->init(this, poly);

    return this;
}

void CRCSearch_init(CRCSearch *this, uint64 poly) {
    int i = 0, j = 0;
    uint64 part = 0ULL;

    for (i = 0; i < TABSIZE; i++) {
        part = (uint64)i;
        /* perform calc, xor poly same times as number of bits shifted off */
        for (j = 0; j < NUMBITS; j++) {
            if (part & 1) {
                /* first bit set, shift LSB off and xor with poly */
                part = (part >> 1) ^ poly;
            }
            else {
                /* shift LSB off, half the value */
                part >>= 1;
            }
        }
        this->table[i] = part;
    }

    return;
}

uint64 CRCSearch_search(CRCSearch *this, const char *fname, uint64 query) {
    uint64 crc = 0ULL;
    unsigned char buff[BUFSIZE] = { '\0' };
    int i = 0, numread = 0;
    int totread = 0; 

    /* open file */
    FILE *fh = fopen (fname, "rb");

    if (!fh) {
        fprintf (stderr, "ERROR: could not open file %s\n", fname);
        return (uint64)NULL;
    }

    while ((numread = (int)fread(&buff, sizeof(unsigned char), BUFSIZE, fh))) {
        for (i = 0; i < numread; i++) {
            /* xor running CRC with value in buffer retaining 
             * first byte only as index into lookup table
             * xor running CRC, ignoring bottom byte, with lookup CRC 
             */
            crc = this->table[(crc ^ buff[i]) &  0xff] ^ (crc >> 8);
            totread++;

            if (0) {
                /* debug */
                fprintf (stderr, "[%d]: 0x%llx\n", totread, crc);
            }

            if (crc == query) {
                this->found = (byte)1;
                this->length = totread;
                break;
            }
        }
    }
        
    /* close file */
    fclose (fh);

    return crc;
}

void CRCSearch_close (CRCSearch **this) {
    if (*this) free(*this);
}

#define CRC_64_ECMA_182 0x42f0e1eba9ea3693ULL

int main (int argc, char **argv) {
    char *fname = NULL;
    uint64 crc = 0ULL;
    uint64 poly = CRC_64_ECMA_182;
    char c = 0;
    CRCSearch *cs;

    /* parse command line */
    while ((c = getopt (argc, argv, "i:q:p:")) != -1) {
        switch (c) {
            case 'i':
                fname = strdup (optarg);
                break;
            case 'q':
                crc = strtoll (optarg, NULL, 16); 
                break;
            case 'p':
                fprintf (stdout, "INFO: Using user provided polynomial: %s\n", optarg);
                poly = strtoll (optarg, NULL, 16);
                break;
            case '?':
                fprintf (stderr, "Invalid option: %c\n", c);
                return ERROR;
            default:
                usage();
                return SUCCESS;
        }
    }

    if (argc == 1) {
        usage();
        return SUCCESS;
    }

    /* setup search and lookup table */
    cs = CRCSearch_new(poly);
    fprintf (stdout, "INFO: Searching for checksum: %llu in file: %s\n", crc, fname);
    if (cs->search(cs, fname, crc)) {
        if (cs->found) {
            fprintf (stdout, "INFO: Checksum: %llu valid for first %llu bytes of file: %s\n", crc, cs->length, fname);
        }
        else {
            fprintf (stdout, "INFO: Checksum: %llu is not valid for contents of file: %s\n", crc, fname);
        }
    }
    cs->close(&cs);

    if (fname) {
        free (fname);
    }

    return SUCCESS;
}
