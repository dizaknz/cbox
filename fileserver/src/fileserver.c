/* fileserver.c
 *
 * I.T.H. Deyzel for Endace
 * 2015/01/08
 */
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <signal.h>
#include <openssl/md5.h>
#include <errno.h>

#include "fileserver.h"
#include "fileutils.h"

void usage (void) {
    const char *usage = "NAME                                                   \n\
       fileserver                                                               \n\
                                                                                \n\
SYNOPSIS                                                                        \n\
       fileserver [-i <ip> -p <port>] -s <storage directory> [-f <file filter>] \n\
                                                                                \n\
DESCRIPTION                                                                     \n\
       fileserver serves out files in storage directory to clients, by default  \n\
       the server starts on localhost on port 5001 and serves all files unless  \n\
       a file name filter is supplied                                           \n\
\n";

    fprintf (stdout, "%s", usage);
}

/* global server */
FileServer server;

/* initialise global server */
void FileServer_init (const char *ip, int port, const char *storage, const char *filter) {
    server.ip = (char *)ip;
    server.port = port;
    server.storage = (char *)storage;
    if (filter) server.filter = (char *)filter;
    server.run = &FileServer_run;
    server.fileHandler = &FileServer_fileHandler;
    server.close = &FileServer_close;

    /* cache is meant to hold file chunk (4K block) patches for clients based
     * on file name and md5 checksum of file contents 
     * this is not fully implemented
     */
    server.cache = calloc (strlen(server.storage) + 8, sizeof (char));

    if (!server.cache) {
        fprintf (stderr, "ERROR: Out of memory (FileServer_init)\n");
        return;
    }

    /* create cache directory */
    sprintf (server.cache, "%s/.cache", server.storage);
    mkdir (server.cache, 0777);
}

/* cleanup server */
void FileServer_close () {
    fprintf (stdout, "DEBUG: Cleanup server %s:%d and socket %d\n", server.ip, server.port, server.socket);
    if (server.ip) free (server.ip);
    if (server.storage) free (server.storage);
    if (server.filter) free (server.filter);
    if (server.socket) close (server.socket);

    pthread_exit (NULL);

    if (server.cache) {
        rmdir (server.cache);
        free (server.cache);
    }

    return;
}

/** FileServer_run:
 *
 * This is the main function for running the file server.
 *
 * It sets up the server socket, listens and accepts connections from clients
 * Each client request is handled by FileServer_fileHandler
 */
int FileServer_run (void) {
    int clientSocket = -1;
    socklen_t clientLength;
    pthread_t thread;
    int reuse = 1;

    /* socket address */
    struct sockaddr_in clientAddress;
    struct sockaddr_in serverAddress;

    /* handle server cleanup using signals */
    signal (SIGINT, server.close);
    signal (SIGTERM, server.close);

    /* create IPv4 stream socket over TCP */
    server.socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server.socket < 0) {
        fprintf (stderr, "ERROR: Failed to open server socket\n");
        server.close();
        return ERROR;
    }

    /* IPV4 */
    serverAddress.sin_family = AF_INET;
    /* socket address */
    serverAddress.sin_addr.s_addr = inet_addr(server.ip);
    /* socket port using host to network short */
    serverAddress.sin_port = htons (server.port);
    /* zero init the padding */
    memset (serverAddress.sin_zero, 0, 8);

    /* re-use address */
    setsockopt (server.socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));

    /* bind to local address */
    if ((bind (server.socket, (struct sockaddr *) &serverAddress, sizeof (struct sockaddr))) < 0) {
        fprintf (stderr, "ERROR: Failed to bind server socket %s:%d\n", server.ip, server.port);
        server.close();
        return ERROR;
    }

    /* now wait for clients */
    listen (server.socket, CLIENT_QUEUE);

    while (TRUE) {
        fprintf (stdout, "DEBUG: Waiting for clients on %s:%d\n", server.ip, server.port);

        /* start with size of client address structure */
        clientLength = sizeof(clientAddress);

        /* wait to accept connection from client */
        clientSocket = accept (server.socket, (struct sockaddr *) &clientAddress, &clientLength);

        if (clientSocket < 0) {
            /* client failed to connect */
            fprintf (stderr, "ERROR: Client failed to connect\n");
            break;
        }

        /* create thread for client to handle file synchronisation */
        pthread_create (&thread, 0, (void *)&FileServer_fileHandler, (void *) &clientSocket);
    }

    /* close server, only called if run loop was aborted by failed client connection */
    server.close();

    return SUCCESS;
}

/** prepare the file to transfer to the client */
FileChunkList *FileServer_prepareFileChunks (FileMetaDataTransfer *mdtransfer) {
    /* the master file in chunks */
    FileChunkList *master = NULL;
    /* chunk list to send to client */
    FileChunkList *send = NULL;
    /* not implement */
    FileChunkList *patch = NULL;
    char filename[MDKEY_LEN + 7] = { '\0' };
    /* pointer into above filename used for writing */
    char *nameptr = NULL;
    int i = 0;
    
    /* create list of file chunks for server file to transfer to client
     * a file chunk consist of a 8 byte file offset, md5 digest for
     * validation and a 4K chunk of file contents. the aim is to keep track
     * of changes to file at the chunk level and ideally to patch the
     * chunks, although that is not implement.
     */
    master = FileChunkList_readFile (server.storage, mdtransfer->master);

    if (!master) {
        return NULL;
    }

    memset (filename, '\0', MDKEY_LEN + 7);

    if (0) {
        /* debug */
        for (i = 0; i <master->size; i++) {
            fprintf (stdout, "DEBUG: chunk %d offset %lu\n", i, master->chunks[i].offset);
        }
    }

    if (mdtransfer->action == FILE_ADD) {
        /* send the master, no need to compare, just add the file to client's catalog */
        send = master;
    }
    else if (mdtransfer->action == FILE_UPDATE) {
        if (!mdtransfer->client) {
            fprintf (stderr, "ERROR: Invalid FileMetaDataTransfer with no client meta data provided (FileServer_prepareFileChunks)\n");
            if (master) FileChunkList_destroy (&master);
            return NULL;
        }

        /* if client md5sum exist in cache we have a prepared chunk file
         * that is if a .cache/file_md5sum.patch file exist we don't have to do the
         * delta comparison again
         */
        nameptr = filename;
        sprintf (nameptr, "%s", mdtransfer->client->filename);
        nameptr += strlen (mdtransfer->client->filename);

        for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sprintf (nameptr, "%02x", mdtransfer->client->md5sum[i]);
            nameptr++;
        }

        sprintf (nameptr, ".patch");

        fprintf (stdout, "DEBUG: patch file %s\n", filename);

        //
        // @todo
        // implement patching
        //
        // if patch exist read it FileChunkList_readChunkFile (const char *chunkfile)
        // else create patch
        // send = patch;
        //

        if (1) {
            /* patching is not implemented, therefore return the master as the patch
             */
            patch = master;
            send = patch;
        }
        if (0) {
            /* if patching was implemented we would do the following */
            send = patch;
            FileChunkList_destroy (&master);
        }
    }

    return send;
}

/** send the file chunks to client */
void FileServer_sendFileAsChunks (FileMetaData *master, FileMetaData *client, int socket) {
    FileMetaDataTransfer mdtransfer;
    char masterKey[MDKEY_LEN] = { '\0' };
    char clientKey[MDKEY_LEN] = { '\0' };
    FileChunkList *sendlist = NULL;
    unsigned char data[BLOCK_SIZE] = { '\0' };
    unsigned char *writer = NULL;
    int i = 0;
    FileChunk *chunk = NULL;
    int offsetlen = sizeof (unsigned long);

    if (!master) {
        return;
    }

    if (!client) {
        mdtransfer.action = FILE_ADD;
    }
    else {
        /* sanity check */
        FileMetaData_makeKey (master, masterKey);
        FileMetaData_makeKey (client, clientKey);

        if (strcmp (masterKey, clientKey) != 0) {
            fprintf (stdout, "WARN: master file %s same as client file, nothing to do\n", master->filename);
            return;
        }

        mdtransfer.action = FILE_UPDATE;
    }

    mdtransfer.master = master;
    mdtransfer.client = client;

    sendlist = FileServer_prepareFileChunks (&mdtransfer);

    if (!sendlist) {
        return;
    }

    /* send file name and md5 first */
    memset (data, '\0', BLOCK_SIZE);
    writer = data;

    memcpy (writer, mdtransfer.master->filename, FILENAME_LEN);
    writer += FILENAME_LEN;
    memcpy (writer, mdtransfer.master->md5sum, MD5_DIGEST_LENGTH);
    writer += MD5_DIGEST_LENGTH;

    fprintf (stdout, "DEBUG: Sending file %s to client on socket %d\n", mdtransfer.master->filename, socket);

    if ((send (socket, data, BLOCK_SIZE, 0)) < 0) {
        fprintf (stderr, "ERROR: Failed to send file header to client on socket %d\n", socket);
        FileChunkList_destroy (&sendlist);
        return;
    }

    /* send it to client */
    chunk = sendlist->chunks;
    for (i = 0; i < sendlist->size; i++) {
        /* chunk is only 4K + 8 byte offset + 32 byte MD5, so lots smaller than 8K block - lazy and wasteful */
        memset (data, '\0', BLOCK_SIZE);
        writer = data;

        memcpy (writer, &chunk->offset, offsetlen);
        writer += offsetlen;
        memcpy (writer, chunk->md5sum, MD5_DIGEST_LENGTH);
        writer += MD5_DIGEST_LENGTH;
        memcpy (writer, chunk->data, CHUNK_SIZE);
        writer += CHUNK_SIZE;

        if ((send (socket, data, BLOCK_SIZE, 0)) < 0) {
            fprintf (stderr, "ERROR: Failed to send chunk %d to client on socket %d\n", i, socket);
            if (strerror(errno)) fprintf (stderr, "ERROR: %s\n", strerror (errno));
            break;
        }

        chunk++;
    }

    FileChunkList_destroy (&sendlist);
}

/** FileServer_fileHandler:
 *
 *  This function handles the actual client request, which synchronises the client's
 *  files with the ones on the server
 */
void FileServer_fileHandler (void *arg) {
    int socket = *((int *)arg);

    /* number of bytes read from client */
    int n = 0;
    /* read buffer */
    unsigned char buffer[BLOCK_SIZE];
    /* raw meta data buffer received from client */
    unsigned char *mddata = NULL;
    /* pointer to allow writing into mddata */
    unsigned char *mdwriter = NULL;
    /* pointer to allow iteration over mddata */
    unsigned char *mditer = NULL;
    /* incoming md list size from client */
    int size = 0;
    /* incoming meta data list from client */
    FileMetaDataList *mdlist = NULL;
    /* master list of files in server catalog */
    FileMetaDataList *mastermdlist = NULL;
    /* size of meta data block received from client */
    int mdsize = 0;
    /* counter */
    int i = 0;
    int j = 0;
    /* number of files to send */
    int tosend = 0;
    int f = 0;

    fprintf (stdout, "DEBUG: sending files to socket: %d\n", socket);

    memset (buffer, '\0', BLOCK_SIZE);

    /* read file catalog from client, serialise into objects */
    n = recv (socket, buffer, BLOCK_SIZE, 0);
    size = *(int *)((char *)buffer);

    /* determine meta data master list for files */
    mastermdlist = FileMetaDataList_readFromDir (server.storage, server.filter);

    if (size) {
        /* client has files, continue to check and process them */
        mdsize = size * (sizeof (struct FileMetaData)) + sizeof (int);

        /* raw data */
        mddata = calloc (mdsize, sizeof (unsigned char));

        /* next convert raw data to meta data list, first allocate MD list */   
        mdlist = FileMetaDataList_new (size);

        fprintf (stdout, "DEBUG: received %d bytes, block size %d\n", n, BLOCK_SIZE);
        fprintf (stdout, "DEBUG: expecting %d of mdlist entries\n", size);

        /* pointer into mddata for writing */
        mdwriter = mddata;

        memcpy (mdwriter, buffer, n);
        mdwriter += n;
        
        /* keep writing to raw data while there is more incoming data */
        while (n == BLOCK_SIZE) {
            n = recv (socket, buffer, BLOCK_SIZE, 0);

            if (mdwriter - mddata > mdsize) {
                /* check for overflow */
                fprintf (stderr, "ERROR: mddata overflow");
                break;
            }

            memcpy (mdwriter, buffer, n);
            mdwriter += n;
        }

        /* skip the first 4 bytes of the size of mdlist */
        mditer = mddata + sizeof (int);

        /* reconstruct mdlist from raw data */
        for (i = 0; i < mdlist->size; i++) {
            memcpy (mdlist->metadata[i].filename, mditer, FILENAME_LEN);
            mditer += FILENAME_LEN;
            memcpy (mdlist->metadata[i].md5sum, mditer, MD5_DIGEST_LENGTH);
            mditer += MD5_DIGEST_LENGTH;
        }

        /* the client sends the server it's file catalog so that the server can
         * compare it to it's own catalog and decide which files to send and/or
         * which parts (4K chunks) of it to send 
         */
        fprintf (stderr, "Server got the client's file catalog\n");

        for (i = 0; i < mdlist->size; i++) {
            fprintf (stdout, "DEBUG: [%d] %s ", i, mdlist->metadata[i].filename);

            for (j = 0; j < MD5_DIGEST_LENGTH; j++) {
                fprintf (stdout, "%02x", mdlist->metadata[i].md5sum[j]);
            }
            fprintf (stdout, "\n");
        }

        /* check if master have files to possible send to client */
        if (mastermdlist) {
            /* compare the files from client with server's and decide which ones to send
             * and which chunks to send */

            // @todo
            // compare two file meta data lists
            // create new list to send
            // pass it to FileServer_sendFileAsChunks

            // dummy
            tosend = mastermdlist->size;

            if ((send (socket, &tosend, sizeof (int), 0)) < 0) {
                fprintf (stderr, "ERROR: Failed to send number of files in payload to client on socket %d\n", socket);
            }
            /* now send it all */
            for (f = 0; f < mastermdlist->size; f++) {
                /* prepare the chunks to send */
                FileServer_sendFileAsChunks (&mastermdlist->metadata[f], NULL, socket);
            }
        }
        else {
            fprintf (stdout, "WARN: No files in server catalog to send to client\n");
            /* send zero */
            tosend = 0;
            if ((send (socket, &tosend, sizeof (int), 0)) < 0) {
                fprintf (stderr, "ERROR: Failed to send number of files in payload to client on socket %d\n", socket);
            }
        }

        /* cleanup */ 
        free (mddata);
        FileMetaDataList_destroy (&mdlist);
        if (mastermdlist) FileMetaDataList_destroy (&mastermdlist);
    }
    else {
        /* the client has no files, send what we have */
        if (mastermdlist) {
            /* send size */
            tosend = mastermdlist->size;
        }
        else {
            fprintf (stdout, "WARN: No files in server catalog to send to client\n");
            /* send zero */
            tosend = 0;
        }

        if ((send (socket, &tosend, sizeof (int), 0)) < 0) {
            fprintf (stderr, "ERROR: Failed to send number of files in payload to client on socket %d\n", socket);
        }

        if (mastermdlist) {
            /* now send it all */
            for (f = 0; f < mastermdlist->size; f++) {
                /* prepare the chunks to send */
                FileServer_sendFileAsChunks (&mastermdlist->metadata[f], NULL, socket);
            }

            FileMetaDataList_destroy (&mastermdlist);
        }
    }

    fprintf (stdout, "DEBUG: Closing child socket: %d\n", socket);
    close(socket);
    shutdown(socket, SHUT_WR);

    return;
}

int main (int argc, char **argv) {
    char *ip = NULL;
    int port = 0;
    char *storage = NULL;
    char *filter  = NULL;
    char c = 0;

    /* parse command line, skip command line validation */
    while ((c = getopt (argc, argv, "i:p:s:f:")) != -1) {
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
            case 'f':
                filter = strdup (optarg);
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
        ip = strdup (DEFAULT_SERVER);
    }

    if (!port) {
        port = DEFAULT_PORT;
    }

    /* init file server */
    FileServer_init (ip, port, storage, filter);

    /* run the file server */
    return server.run();
}
