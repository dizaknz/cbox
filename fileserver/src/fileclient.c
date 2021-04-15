/* fileclient.c
 *
 * I.T.H. Deyzel for Endace
 * 2015/01/08
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "fileutils.h"

void usage (void) {
    const char *usage = "NAME                                      \n\
       fileclient                                                  \n\
                                                                   \n\
SYNOPSIS                                                           \n\
       fileclient [-i <ip> -p <port>] -s <storage directory>       \n\
                                                                   \n\
DESCRIPTION                                                        \n\
       fileclient connects to file server and receives updates to  \n\
       files in its storage directory                              \n\
\n";

    fprintf (stdout, "%s", usage);
}

int main (int argc, char **argv) {
    char *ip = NULL;
    int port = 0;
    char *storage = NULL;
    char c = 0;
    struct sockaddr_in serverAddress;
    int clientSocket = -1;
    FileMetaDataList *mdlist = NULL;
    int i = 0;
    int j = 0;
    int n = 0;
    unsigned char *mddata = NULL;
    unsigned char *mditer = NULL;
    int mdsize = 0, mdlistsize = 0;
    /* read buffer */
    unsigned char buffer[BLOCK_SIZE];
    int in = 0;
    char infile[FILENAME_LEN] = { '\0' };
    md5digest inmd5 = { '\0' };
    unsigned char *reader = 0;
    char outfile[PATH_MAX + FILENAME_LEN + 1] = { '\0' };
    FILE *out = NULL;
    int offsetlen = sizeof (unsigned long);
    unsigned long chunkoffset = 0;
    md5digest chunkmd5;
    md5digest checkmd5;
    char chunkstr[MD5_DIGEST_LENGTH] = { '\0' };
    char checkstr[MD5_DIGEST_LENGTH] = { '\0' };

    /* parse command line */
    while ((c = getopt (argc, argv, "i:p:s:")) != -1) {
        switch (c) {
            case 'i':
                ip = strdup (optarg);
                break;
            case 'p':
                port = atoi (optarg); 
                break;
            case 's':
                storage = strdup (optarg);
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

    if (!ip) {
        fprintf (stderr, "ERROR: Provide IP address of server");
        return ERROR;
    }

    if (!port) {
        fprintf (stderr, "ERROR: Provide port of server");
        return ERROR;
    }

    /* setup local TCP socket to communicate with server */
    clientSocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket < 0) {
        fprintf (stderr, "ERROR: Failed to open client socket\n");
        return ERROR;
    }

    /* IPV4 */
    serverAddress.sin_family = AF_INET;
    /* socket address */
    serverAddress.sin_addr.s_addr = inet_addr(ip);
    /* socket port using host to network short */
    serverAddress.sin_port = htons (port);
    /* zero init the padding */
    memset (serverAddress.sin_zero, 0, 8);

    if (0) {
        /* debug code, ignore */
        mdlist = FileMetaDataList_readFromDir (storage, NULL);
        for (i = 0; i < mdlist->size; i++) {
            fprintf (stdout, "DEBUG: %s ", mdlist->metadata[i].filename);

            for (j = 0; j < MD5_DIGEST_LENGTH; j++) {
                fprintf (stdout, "%02x", mdlist->metadata[i].md5sum[j]);
            }
            fprintf (stdout, "\n");
        }
        FileMetaDataList_destroy (&mdlist);
    }

    if ((connect (clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress))) < 0) {
        fprintf (stderr, "ERROR: Could not connect to %s:%d\n", ip, port);
    }
    else {
        /* send local storage meta data */
        mdlist = FileMetaDataList_readFromDir (storage, NULL);

        /* always start with 4 bytes containing number of entries */
        mdsize = sizeof (int);

        /* mdlist is populated if client has files, else it's NULL */
        if (mdlist) {
            mdlistsize = mdlist->size;
            /* calc size of data to send */
            mdsize += mdlist->size * (sizeof (struct FileMetaData));
        }

        mddata = calloc (mdsize, sizeof (unsigned char));

        if (!mddata) {
            fprintf (stderr, "ERROR: Out of memory (mddata)\n");
            FileMetaDataList_destroy (&mdlist);
            if (ip) free (ip);
            if (storage) free (storage);
            return ERROR;
        }

        fprintf (stdout, "DEBUG: size=%d\n", mdsize);

        /* convert list into contiguous memory block, this include a lot of wasted space
         * for this demo we stick with a fixed block of data */
        mditer = mddata;
        memcpy (mditer, &mdlistsize, sizeof (int));
        mditer += sizeof (int);

        if (mdlist) {
            for (i = 0; i < mdlist->size; i++) {
                memcpy (mditer, mdlist->metadata[i].filename, FILENAME_LEN);
                mditer += FILENAME_LEN;
                memcpy (mditer, mdlist->metadata[i].md5sum, MD5_DIGEST_LENGTH);
                mditer += MD5_DIGEST_LENGTH;
            }
        }

        fprintf (stdout, "DEBUG: Sending meta data list of size: %d to %s:%d\n", mdsize, ip, port);

        if ((send (clientSocket, mddata, mdsize, 0)) < 0) {
            /* failed to send */    
            fprintf (stderr, "ERROR: Failed to send meta data list of size: %d to %s:%d\n", mdsize, ip, port);
        }
        else {
            /* continue */
            fprintf (stdout, "DEBUG: server's got it\n");

            memset (buffer, '\0', BLOCK_SIZE);

            /* read number of files incoming, only 4 bytes */
            n = recv (clientSocket, buffer, BLOCK_SIZE, 0);

            if (n == sizeof (int)) {
                in = *((int *)((char *)buffer));

                fprintf (stdout, "DEBUG: Server is sending %d files\n", in);

                /* loop and receive incoming */
                for (i = 0; i < in; i++) {
                    memset (buffer, '\0', BLOCK_SIZE);
                    /* get file header */
                    n = recv (clientSocket, buffer, BLOCK_SIZE, 0);

                    reader = buffer;
                    memset (infile, '\0', FILENAME_LEN);
                    memset (inmd5, '\0', MD5_DIGEST_LENGTH);

                    memcpy (infile, reader, FILENAME_LEN);
                    reader += FILENAME_LEN;
                    memcpy (inmd5, reader, MD5_DIGEST_LENGTH);
                    reader += MD5_DIGEST_LENGTH;

                    fprintf (stdout, "DEBUG: Receiving file %s\n", infile);
                    FileUtils_printMD5 (inmd5);

                    // @todo: validation
                    
                    memset (outfile, '\0', PATH_MAX + FILENAME_LEN + 1);
                    sprintf (outfile, "%s/%s", storage, infile);

                    /* open file with read/write/append in binary mode */
                    out = fopen (outfile, "w+b");

                    if (!out) {
                        fprintf (stderr, "ERROR: Could not open file: %s for appending\n", outfile);
                        break;
                    }

                    /* keep reading from server */
                    memset (buffer, '\0', BLOCK_SIZE);
                    while (recv (clientSocket, buffer, BLOCK_SIZE, 0)) {
                        reader = buffer;
                        /* read chunk data */
                        memcpy (&chunkoffset, reader, offsetlen);
                        reader += offsetlen;
                        memcpy (chunkmd5, reader, MD5_DIGEST_LENGTH);
                        reader += MD5_DIGEST_LENGTH;
                        FileUtils_calcDataMD5 (reader, CHUNK_SIZE, &checkmd5);

                        if (0) {
                            /* debug */
                            FileUtils_MD5toString (chunkmd5, chunkstr);
                            FileUtils_MD5toString (checkmd5, checkstr);

                            fprintf (stdout, "DEBUG: chunk MD5 %s check MD5 %s\n", chunkstr, checkstr);

                            if (FileUtils_compMD5 (chunkmd5, checkmd5) != 0) {
                                fprintf (stderr, "ERROR: mismatch in MD5 for chunk offset %lu\n", chunkoffset);
                            }
                        }

                        // @todo validation needed on chunk offset for writing into existing data files
                        fseek (out, chunkoffset, SEEK_SET);
                        fwrite (reader, 1, CHUNK_SIZE, out);

                        memset (buffer, '\0', BLOCK_SIZE);
                    }

                    fclose (out);
                }
            }
            else {
                fprintf (stderr, "ERROR: Server did not send the expected number of files incoming\n");
            }
        
        }

        FileMetaDataList_destroy (&mdlist);
        free (mddata);
    }

    if (ip) free (ip);
    if (storage) free (storage);

    return SUCCESS;
}
