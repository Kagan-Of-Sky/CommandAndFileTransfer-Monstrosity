#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>

#define BACKLOG  10

/* Server outline:
 * 1. Server starts up on the port specifed by argv[1]
 * 
 * 2. Server listens for connections.
 * 
 * 3. Once the server gets a connection, it fork()s. The child server
 *    now handles the client (step 4 and on), and the parent server
 *    continues listening (back to step 1).
 * 
 * 4. Child server sits and waits for a command from the client to
 *    execute.
 * 
 * 5. Once it gets a command it calls executeCommand(), which will
 *    determine the correct function executeCommandXXXXX() to handle
 *    that command.
 * 
 * 6. Back to 4.
 */




/* Simple prints out the help message for this application. */
void printUsage(const char *executableName);

/* Print out the details for a client. */
int printClientDetails(const struct sockaddr *clientAddress, socklen_t clientAddressSize, const char *str);




/* Determine the type of command and call the appropriate executeCommandXXXXX() function. */
int executeCommand(int sockfd, const char *command);
    /* PURPOSE:
     *     Execute a 'simple read only' command, one which does not change
     *     the processes state like the 'scd' command does, and does not
     *     require any synchronization between the client and server like
     *     the 'get' or 'put' commands do.
     * 
     *     Uses popen() to open a pipe to the process.
     * 
     * RETURNS:
     *     0 - Everything went 0K.
     *     1 - A non-critical error occured.
     *    -1 - Critical error.
     * 
     * Examples of simple commands:
     *     sls, spwd, smd5sum
     */
    int executeReadOnlyUnixCommand(int sockfd, const char *command);
    
    
    /* PURPOSE:
     *     Change the current working directory of the server.
     * 
     * RETURNS:
     *     0 - Everything went OK.
     *     1 - Non critical error.
     *    -1 - A critical error occured.
     */
    int executeCommandcd(int sockfd, const char *command);
    
    
    /* File download/upload commands.
     * 
     * RETURNS:
     *     0 - Success
     *     1 - Non-critical error.
     *    -1 - Critical error.
     */
    int executeCommandget(int sockfd, const char *command);
    int sendGetReplyNo(int sockfd, const char *errorstr);
    int executeCommandput(int sockfd, const char *command);
    
#endif