#ifndef SHARED_H
#define SHARED_H




#define BUFFER_SIZE 500




/* Get and put macros. */
#define GET_REPLY_SIZE (2 + sizeof(long)) /* Big enough to hold the leading "OK" or "NO", and the size of the file. */
#define GET_REPLY_OK   "OK"
#define GET_REPLY_NO   "NO"

#define PUT_REPLY_SIZE  2   /* Big enough to hold the leading "OK" or "NO". */
#define PUT_REPLY_OK   "OK"
#define PUT_REPLY_NO   "NO"




/* Terminal Colors. */
#define C_RST  "\x1B[0m"  /* Reset color. */
#define CFLRED "\x1b[91m" /* Color Foreground Light Red */
#define CFLGRN "\x1b[92m" /* Color Foreground Light Green */
#define CFLBLU "\x1b[94m" /* Color Foreground Light Blue */
#define CFLCYN "\x1b[96m" /* Color Foreground Light Cyan */
#define CFLYLW "\x1b[93m" /* Color Foreground Light Yellow */




/* An enumeration representing all of the commands which
 * are recognized by both the client AND the server.
 */
typedef enum{
    command_list,   /* List the files in a directory. */
    command_cd,     /* Change directory. */
    command_pwd,    /* Print current working directory. */
    command_md5,    /* Compute the md5 for a file. */
    command_get,    /* get (download) a file. */
    command_put,    /* put (upload) a file. */
    command_unknown /* Unknown command. */
} SharedCommandType;

/* PURPOSE:
 *     To determine what type of command the string passed
 *     to it is. The main use is to save having to write
 *     a lot of redundant if/elseif/else in the executeCommand()
 *     functions.
 * 
 * PARAMETERS:
 *     const char *command: A null terminated string
 * 
 * RETURNS:
 *     One of the values in the SharedCommandType enumeration.
 * 
 * EXAMPLE:
 *     getSharedCommandType("scd /bin");
 *     Would return command_cd.
 * 
 */
SharedCommandType getSharedCommandType(const char *command);




/* PURPOSE:
 *     To get the size of a file.
 * 
 * PARAMETERS: 
 *     const char *filePath: The path to the file.
 * 
 * RETURNS:
 *     SUCCESS: The size of the file in bytes.
 *     FAILURE: -1, errno set by stat().
 */
long fileSize(const char *filePath);

/* PURPOSE:
 *     Get the file name from a file path.
 * 
 * PARAMETERS:
 *     const char *filePath: A path to the file.
 * 
 * RETURNS:
 *     A pointer to the file name, see EXAMPLES.
 *     The pointer points to the start of the file
 *     name.
 * 
 * EXAMPLES:
 *     extractFileName("/home/user/Desktop/text.txt")
 *         Returns a pointer to "text.txt"
 * 
 *     extractFileName("/text.txt")
 *         Returns a pointer to "text.txt"
 * 
 *     extractFileName("/")
 *         Returns NULL
 */
const char *extractFileName(const char *filePath);

/* PURPOSE:
 *     To determine if the file is a regular file.
 *     ie. Not a directory, block device, char device, etc.
 * 
 * RETURNS:
 *     1 - File is a regular file.
 *     0 - File is NOT a regular file.
 *    -1 - An error occured (most likely file doesnt exist,
 *         or dont have permission to access it).
 */
int isRegularFile(const char *filePath);

/* PURPOSE:
 *     To check if a file exists.
 * 
 * RETURNS:
 *     1 - File exists
 *     0 - File does not exist
 */
int fileExists(const char *filePath);

#endif