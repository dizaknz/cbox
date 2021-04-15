#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <openssl/md5.h>

#define BLOCK_SIZE 2 << 12
#define CHUNK_SIZE 2 << 11
#define FILENAME_LEN NAME_MAX + 1
#define MDKEY_LEN FILENAME_LEN + MD5_DIGEST_LENGTH

typedef unsigned char md5digest[MD5_DIGEST_LENGTH];

typedef unsigned char bool;

#define TRUE 1
#define FALSE 0

#define SUCCESS 0
#define ERROR 1

/** FileMetaData:
 *
 *  Holds meta data about files to synchronise
 */
typedef struct FileMetaData {
    char filename[FILENAME_LEN];
    md5digest md5sum;
} FileMetaData;

void FileMetaData_makeKey (FileMetaData *metadata, const char *keyOut);

/** FileMetaDataList:
 *
 *  A list of file meta data that represent the so called catalog on
 *  server or client
 */
typedef struct FileMetaDataList FileMetaDataList;

struct FileMetaDataList {
    int size;
    FileMetaData *metadata;
};

FileMetaDataList *FileMetaDataList_new (int size);
void FileMetaDataList_destroy (FileMetaDataList **list);
FileMetaDataList *FileMetaDataList_readFromDir (char *dirname, char *filter);

/** FileTransferAction
 *
 *  Define the actions that can be performed with a chunk during data transfer
 */
typedef enum FileTransferAction {
    FILE_ADD,
    FILE_UPDATE
} FileTransferAction;

typedef struct FileMetaDataTransfer {
    FileMetaData *master;
    FileMetaData *client;
    FileTransferAction action;
} FileMetaDataTransfer;

/** FileChunk: 
 *  
 *  A file chunk has a file offset, md5 check sum for validation and fixed data chunk read/stored on disk
 */
typedef struct FileChunk {
    unsigned long offset;
    md5digest md5sum;
    unsigned char data[CHUNK_SIZE];
} FileChunk;

/** FileChunkList:
 *
 *  List of file chunks to send
 */
typedef struct FileChunkList {
    int size;
    FileChunk *chunks;
} FileChunkList;

FileChunkList *FileChunkList_new (int size);
void FileChunkList_destroy (FileChunkList **list);
FileChunkList *FileChunkList_readFile (const char *storage, FileMetaData *metadata);
FileChunk *FileChunkList_searchChunk (unsigned long offset);
void FileChunkList_writePatchToDisk (FileChunkList *list, const char *chunkfile);

/** FileChunkPatch:
 *
 *  Individual chunk data patch
 *
 *  @todo - not implemented
 *
 *  the idea was that to submit fine granular patches on an indivial file chunk
 *  as identified by chunkOffset. patchOffset is an offset into chunk from start
 *  of file chunk, size the length of the patch and the data patch, this could
 *  be a single value or a contiguous range. alas not enough time to implement
 *  it for this code demo
 */
typedef struct FileChunkPatch {
    unsigned long chunkOffset;
    unsigned long patchOffset;
    size_t size;
    void *patch;
} FileChunkPatch;

/* utility function for fetching filenames from a directory */
char * FileUtils_getFileNames (const char *dirname, int *sizeOut, char *filter);

int FileUtils_calcFileMD5 (const char *filename, md5digest *md5sum);
int FileUtils_calcDataMD5 (const void *data, int size, md5digest *md5);
int FileUtils_compMD5 (md5digest a, md5digest b);
void FileUtils_MD5toString (md5digest md5, char *str);
void FileUtils_printMD5 (md5digest md5);
int FileUtils_copyFile (const char *source, const char *target);
bool FileUtils_fileExists (const char *filename);
