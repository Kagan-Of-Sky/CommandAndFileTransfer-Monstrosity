#ifndef CLIENT_H
#define CLIENT_H

#define CLIENT_COMMAND_CD "cd "

#define DISPLAY_GET_PUT_INTERVAL 1000 /* The interval to display upload/download statuses in executeCommandget() and executeCommandput() */

typedef enum{
    client_command_cd,     /* Change directory. */
    client_command_help,   /* Print help screen. */
    client_command_unknown /* Unknown command. */
} ClientCommandType;


/* Short outline of how the client works:
 * 
 * 1. Connect to the server.
 * 2. Fetch a command from the user.
 * 3. Determine the type of command.
 *      + If the command is not a recognized command (such as ls,
 *        pwd, cat, tree, vim, etc.) then fork() and exec() the
 *        command on the users local machine. Then wait for the
 *        command to finish and go back to #1.
 *      + Otherwise, if the command is recognized (sls, spwd, smd5sum
 *        scd, etc.) proceed with the next step.
 * 
 * 4. Call the appropriate function which can handle this command.
 * 5. Repeat.
 */



/* PURPOSE:
 *          Connect this client to the ip and port arguments.
 * 
 * ARGUMENTS:
 *          const char *ip     The address to connect to (does
 *                             not have to be an ip address).
 * 
 *          const char *port   The port to connect on (does not
 *                             have to be numeric).
 * 
 *          int *fd            Please refer to the RETURNS
 *                             section to see the purpose of
 *                             this argument.
 * 
 * RETURNS:
 *          0   Everything went 0K, fd contains an open file
 *              descriptor.
 * 
 *         -1   Error in getaddrinfo(), fd contains a value
 *              which can be used on a call to gai_strerror(fd).
 * 
 *         -2   Could not connect to any entry in the list
 *              returned from getaddrinfo(), ignore the value
 *              in fd.
 */
int connectipport(const char *ip, const char *port, int *fd);




/* PURPOSE:
 *          Prints out the prompt for this program (without newline).
 * 
 * EXAMPLE:
 *          If the cwd of this process is /home/mark
 *          then this function will print out:
 * 
 *          "/home/mark > "
 *          Without the quotes of course.
 * 
 * RETURNS:
 *          0  Success.
 *         -1  Failure.
 */
int printPrompt();

/* PURPOSE:
 *          Simply prints out the help message for this application.
 */
void printUsage(const char *executableName);

/* PURPOSE:
 *          Simply prints out the help screen when the command "help"
 *          is entered by the user.
 */
void printHelpScreen();




/* PURPOSE:
 *          To determine the type of command and take the
 *          appropriate action.
 * 
 * PARAMETERS:
 *          int sockfd:          An open socket to the server.
 *          const char *command: The command the user typed in.
 * 
 * RETURNS:
 *          0  Success.
 *         -1  Failure.
 */
int executeCommand(int sockfd, const char *command);

/* PURPOSE:
 *          Execute a command on the local machine.
 *          Uses the function system() to fork() and
 *          exec() the command.
 * 
 *          The commands executed by this function
 *          are not server commands.
 * 
 * PARAMETERS:
 *          const char *command: The command the user typed in.
 * 
 * RETURNS:
 *          0  Success.
 *         -1  Failure.
 */
int executeLocalCommand(const char *command);

/* Used for simple read only server side commands such as:
 *     sls
 *     spwd
 *     scd
 * 
 * These commands require no synchronization between the
 * server and client.
 */
int executeServerReadOnlyCommand(int sockfd, const char *command);

/* File download/upload commands.
 * RETURNS:
 *     0 - Success.
 *     1 - Non critical error.
 *    -1 - Critical error.
 */
int executeCommandget(int sockfd, const char *command);
int executeCommandput(int sockfd, const char *command);

/* PURPOSE:
 *     To determine what type of command the string passed
 *     to it is. The main use is to save having to write
 *     a lot of if/elseif/else in the executeCommand()
 *     function.
 * 
 * PARAMETERS:
 *     const char *command: A null terminated string
 * 
 * RETURNS:
 *     One of the values in the ClientCommandType enumeration.
 * 
 * EXAMPLE:
 *     getClientCommandType("help");
 *     Would return client_command_cd.
 * 
 */
ClientCommandType getClientCommandType(const char *command);

#endif