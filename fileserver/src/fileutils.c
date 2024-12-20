/** 
 * provide useful file utilities for synching files between client and server
 */
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/conf.h>

#include "fileutils.h"

/* create structure to hold file meta data */
FileMetaDataList *FileMetaDataList_new (int size) {
    FileMetaDataList *list = NULL;
    
    if (size <= 0) {
        fprintf (stderr, "ERROR: Invalid size: %d provided for creating meta data list\n", size);
        return NULL;
    }

    list = calloc (1, sizeof (struct FileMetaDataList));

    if (!list) {
        fprintf (stderr, "ERROR: Out of memory (FileMetaDataList_new:list)\n");
        return NULL;
    }

    list->metadata = calloc (size, sizeof (struct FileMetaData));

    if (!list->metadata) {
        fprintf (stderr, "ERROR: Out of memory (FileMetaDataList_new:list->metadata)\n");
        if (list) free (list);
        return NULL;
    }

    list->size = size;

    return list;
}

/** cleanup meta data list structure */
void FileMetaDataList_destroy (FileMetaDataList **list) {
    if (*list) {
        if ((*list)->metadata) {
            free ((*list)->metadata);
            (*list)->metadata = NULL;
            (*list)->size = 0;
        }
        free (*list);
        *list = NULL;
    }
    return;
}

/** Converts contents of a directory to a meta data structure to be used to catalog
 *  contents of directory
 */
FileMetaDataList *FileMetaDataList_readFromDir (char *dirname, char *filter) {
    char *filenames = NULL;
    int size = 0;
    int i = 0;
    char *filename = NULL;
    FileMetaDataList *list = NULL;
    FileMetaData *metadata = NULL;
    char fullpath[PATH_MAX + NAME_MAX + 1] = { '\0' };

    filenames = FileUtils_getFileNames (dirname, &size, filter);

    if (!filenames || !size) return NULL;

    list = FileMetaDataList_new (size);

    /* start of filenames */
    filename = filenames;

    /* start of meta data */
    metadata = list->metadata;

    for (i = 0; i < size; i++) {
        memset (fullpath, '\0', sizeof (fullpath));
        sprintf (fullpath, "%s/%s", dirname, filename);
        snprintf (metadata->filename, FILENAME_LEN, "%s", filename);
        FileUtils_calcFileMD5 ((const char *)fullpath, &metadata->md5sum);

        /* next file name */
        filename += FILENAME_LEN;
        
        /* next meta data */
        metadata++;
    }

    free (filenames);

    return list;
}

/** retrieve names of regular files in a directory, returns pointer to start of filename block
 *  and writes number of names returned to sizeOut, optionally returns files that matches
 *  filter (for now just a strncmp)
 */
char * FileUtils_getFileNames (const char *dirname, int *sizeOut, char *filter) {
    DIR *dir;
    struct dirent *entry;
    struct stat entryStats;
    int count = 0;
    int i = 0;
    char *copy = NULL;
    char fullpath[PATH_MAX + NAME_MAX + 1] = { '\0' };
    char *filesOut;

    *sizeOut = 0;

    /* list contents of directory */
    dir = opendir (dirname);

    if (!dir) {
        fprintf (stderr, "ERROR: Failed to read directory: %s\n", dirname);
        return NULL;
    }

    while ((entry = readdir (dir)) != NULL) {
        memset (fullpath, '\0', sizeof (fullpath));
        sprintf (fullpath, "%s/%s", dirname, entry->d_name);

        if ((stat (fullpath, &entryStats)) != -1 && S_ISREG(entryStats.st_mode)) {
            /* apply filename filter, if required */
            if ((filter && strncmp (entry->d_name, filter, strlen(filter)) == 0) || !filter) {
                /* count the regular files */
                count++;
            } 
        }
    }

    if (!count) {
        closedir (dir);
        return NULL;
    }

    fprintf (stdout, "DEBUG: %s - %d files\n", dirname, count);

    /* allocate filesOut */
    filesOut = calloc (count * FILENAME_LEN, sizeof(char));

    if (!filesOut) {
        fprintf (stderr, "ERROR: Out of memory (FileUtils_getFileNames:filesOut)\n");
        return NULL;
    }

    closedir (dir);
    dir = opendir (dirname);

    /* start of files names */
    copy = filesOut;
    while ((entry = readdir (dir)) != NULL) {
        memset (fullpath, '\0', sizeof (fullpath));
        sprintf (fullpath, "%s/%s", dirname, entry->d_name);

        if ((stat (fullpath, &entryStats)) != -1 && S_ISREG(entryStats.st_mode)) {
            if ((filter && strncmp (entry->d_name, filter, strlen(filter)) == 0) || !filter) {
                if (i > count /*|| copy > filesOut + (FILENAME_LEN * count)*/) {
                    fprintf (stderr, "ERROR: overflowing %d > %d\n", i, count);
                    free (filesOut);
                    return NULL;
                }

                fprintf (stdout, "DEBUG: Adding file: %s\n", entry->d_name);
                strcpy (copy, entry->d_name);

                /* advance to next file name slot */
                copy += FILENAME_LEN;

                /* for validation */
                i++;
            }
        }
    }

    *sizeOut = count;

    closedir (dir);

    return filesOut;
}

/** calculate MD5 check sum on file contents */
int FileUtils_calcFileMD5 (const char *filename, md5digest *md5sum) {
    FILE *file = NULL;
    MD5_CTX context;
    int bytes;
    unsigned char data[BLOCK_SIZE];

    file = fopen (filename, "rb");

    if (!file) {
        fprintf (stderr, "ERROR: Could not open file %s for calculating digest\n", filename);
        return ERROR;
    }

    // TODO
    // EVP_Q_digest(NULL, "MD5", NULL, target.c_str(), target.size(), digest, NULL);

    MD5_Init (&context);

    while ((bytes = fread (data, 1, BLOCK_SIZE, file)) != 0) {
        MD5_Update (&context, data, bytes);
    }

    MD5_Final (*md5sum, &context);

    /* valgrind still report unreachable memory */
    CONF_modules_free();

    fclose (file);

    return SUCCESS;
}

/** calculate MD5 on small data blocks or strings, not files */
int FileUtils_calcDataMD5 (const void *data, int size, md5digest *md5) {
    MD5_CTX context;

    MD5_Init (&context);
    MD5_Update (&context, data, size);
    MD5_Final (*md5, &context);
    CONF_modules_free();

    return SUCCESS;
}

/** compare two MD5 digests verbatim */
int FileUtils_compMD5 (md5digest a, md5digest b) {
    int i = 0;

    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}

/** convert MD5 digest to plain string */
void FileUtils_MD5toString (md5digest md5, char *str) {
    int i = 0;

    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf (str, "%02x", md5[i]);
        str++;
    }

    return;
}

/* debug only */
void FileUtils_printMD5 (md5digest md5) {
    char str[MD5_DIGEST_LENGTH] = { '\0' };

    FileUtils_MD5toString (md5, str);

    fprintf (stdout, "DEBUG: MD5 %s\n", str);

    return;
}

/** check if file exists */
bool FileUtils_fileExists (const char *filename) {
    struct stat st;
    int rc = 0;

    rc = stat (filename, &st);

    return rc == 0 ? TRUE : FALSE;
}

/** copy file
 *
 *  FIXME not thread safe, no locking on file
 */
int FileUtils_copyFile (const char *source, const char *target) {
    FILE *in = NULL, *out = NULL;
    int bytes = 0;
    unsigned char buffer[BLOCK_SIZE];

    in = fopen (source, "rb");

    if (!in) {
        fprintf (stderr, "ERROR: Failed to open file %s for copying to %s\n", source, target);
        return ERROR;
    }

    out = fopen (target, "wb");

    if (!out) {
        fprintf (stderr, "ERROR: Failed to open file %s for writing from %s\n", target, source);
        return ERROR;
    }

    while ((bytes = fread (buffer, 1, BLOCK_SIZE, in)) != 0) {
        fwrite (buffer, 1, BLOCK_SIZE, out);
    }

    fclose (in);
    fclose (out);

    return SUCCESS;
}

/* combine metadata file name and md5 as key */
void FileMetaData_makeKey (FileMetaData *metadata, const char *keyOut) {
    int i = 0;
    char *keyptr = NULL;

    keyptr = (char *)keyOut;
    memset (keyptr, '\0', MDKEY_LEN);

    sprintf(keyptr, "%s", metadata->filename);
    keyptr += strlen (metadata->filename);

    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf (keyptr, "%02x", metadata->md5sum[i]);
        keyptr++;
    }

    return;
}

/** create and initialise new file chunk list data structure */
FileChunkList *FileChunkList_new (int size) {
    FileChunkList *list = NULL;
    
    if (size <= 0) {
        fprintf (stderr, "ERROR: Invalid size: %d provided for creating chunk data list\n", size);
        return NULL;
    }

    list = calloc (1, sizeof (struct FileChunkList));

    if (!list) {
        fprintf (stderr, "ERROR: Out of memory (FileChunkList_new:list)\n");
        return NULL;
    }

    list->chunks = calloc (size, sizeof (struct FileChunk));

    if (!list->chunks) {
        fprintf (stderr, "ERROR: Out of memory (FileChunkList_new:list->chunks)\n");
        if (list) free (list);
        return NULL;
    }

    list->size = size;

    return list;
}

/** cleanup list and chunks */
void FileChunkList_destroy (FileChunkList **list) {
    if (*list) {
        if ((*list)->chunks) {
            free ((*list)->chunks);
            (*list)->chunks = NULL;
            (*list)->size = 0;
        }
        free (*list);
        *list = NULL;
    }
    return;
}

/** read a file in storage into a chunk list data structure to allow comparing lists */
FileChunkList *FileChunkList_readFile (const char *storage, FileMetaData *metadata) {
    FILE *file = NULL;
    char filename[PATH_MAX + NAME_MAX + 2] = { '\0' };
    int bytes = 0;
    unsigned long offset = 0;
    FileChunkList *list = NULL;
    int count = 0;
    unsigned char data[CHUNK_SIZE];
    FileChunk *chunk = NULL;
    char filecopy[PATH_MAX + MDKEY_LEN + 2] = { '\0' };
    char *nameptr = NULL;
    int i = 0;
 
    sprintf (filename, "%s/%s", storage, metadata->filename);

    file = fopen (filename, "rb");

    if (!file) {
        fprintf (stderr, "ERROR: Unable to open file: %s for creating chunk list\n", filename);
        return NULL;
    }

    /* count number of chunks */
    while ((bytes = fread (data, 1, CHUNK_SIZE, file)) != 0) count++;

    if (!count) {
        /* no files in server storage */
        return NULL;
    }

    /* init the chunk list */
    list = FileChunkList_new (count);

    if (!list) {
        return NULL;
    }

    offset = 0;
    fseek (file, 0L, SEEK_SET);
    chunk = list->chunks;

    /* read in chunks of CHUNK_SIZE */
    while ((bytes = fread (data, 1, CHUNK_SIZE, file)) != 0) {
        /* create chunk with file offset, data and md5 for validation */
        chunk->offset = offset;
        memcpy (chunk->data, data, CHUNK_SIZE);
        FileUtils_calcDataMD5 (chunk->data, CHUNK_SIZE, &chunk->md5sum);
        offset += bytes;
        chunk++;
    }

    fclose (file);

    /* lastly we create a copy of source file in .cache directory for next time,
     * it is not ideal, but in lieu of proper versioning we'll archive it as
     * a file identified by meta data key
     */
    memset (filecopy, '\0', PATH_MAX + MDKEY_LEN + 1);
    nameptr = filecopy;
    sprintf (nameptr, "%s/.cache/%s", storage, metadata->filename);
    nameptr += (strlen (storage) + strlen (metadata->filename) + 8);

    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf (nameptr, "%02x", metadata->md5sum[i]);
        nameptr++;
    }

    fprintf (stdout, "DEBUG: copy file %s\n", filecopy);

    if (FileUtils_fileExists (filecopy) == FALSE) {
        FileUtils_copyFile (filename, filecopy);
    }

    return list;
}

/** @todo - not implemented
 *
 *  needed for performing chunk comparisons to create file chunk patches
 *  to send to client, eg. only send updates
 *  file offsets are always incremental, use a binary search
 */
FileChunk *FileChunkList_searchChunk (unsigned long offset) {
    /* stub */
    return NULL;
}

/** @todo - not implemented
 *
 *  meant to write a patch chunk file to disk for later re-use by other clients
 */
void FileChunkList_writePatchToDisk (FileChunkList *list, const char *chunkfile) {
    /* stub */
    return;
}
