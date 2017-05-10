#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "shared.h"
#include "client.h"

int main(int argc, char **argv){
    /* Command line arguments. */
    const char *ipstr;   /* The ip address from the command line arguments (argv[1])  */
    const char *portstr; /* The port number from the command line arguments (argv[2]) */
    
    /* Network variables. */
    int sockfd; /* The socket file descriptor. */
    
    /* Prompt variables. */
    char   *line;
    long   lineLength;
    size_t lineBufferSize;
    
    /* Other */
    int ret; /* Hold return value from various functions. */
    
    /* Not enough arguments. */
    if(argc != 3){
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    /* Get the ip address and port from the command line. */
    ipstr   = argv[1];
    portstr = argv[2];
    
    /* Attempt to connect to the server. */
    printf("Attempting to connect to %s on port %s.\n", ipstr, portstr);
    
    ret = connectipport(ipstr, portstr, &sockfd);
    
    /* Could not connect. */
    if(ret != 0){
        printf(CFLRED "ERROR:" C_RST " Could not connect to %s on port %s.\n", ipstr, portstr);
        if(ret == -1){
            printf("REASON: %s\n", gai_strerror(sockfd));
        }
        exit(EXIT_FAILURE);
    }
    
    /* Succesfully connected. */
    printf("Succesfully connected to %s on port %s.\n"
           "Enter 'q' to quit,\n"
           "Enter 'help' to display the help screen.\n", ipstr, portstr);
    
    /* Make getline() allocate the buffer for me. */
    line           = NULL;
    lineBufferSize = 0;
    
    /* Continually loop until the user types 'q'. */
    while((printPrompt() == 0) && (lineLength = getline(&line, &lineBufferSize, stdin)) != -1){
        /* User entered a 'q' and hit ctrl+d. */
        if(lineLength == 1 && line[0] == 'q'){
            puts("\nQuitting...");
            break;
        }
        /* User entered a 'q' and hit the enter key. */
        else if(lineLength == 2 && line[0] == 'q' && line[1] == '\n'){
            puts("Quitting...");
            break;
        }
        /* User just pressed enter. */
        else if(lineLength == 1 && line[0] == '\n'){
            continue;
        }
        
        /* Remove the trailing newline. */
        if(line[lineLength-1] == '\n'){
            line[lineLength-1] = '\0';
        }
        
        /* Make sure the command fits into the buffer. */
        if(lineLength >= BUFFER_SIZE){
            printf(CFLRED "ERROR:" C_RST " command too long.\n");
            continue;
        }
        
        /* Execute the command. */
        if(executeCommand(sockfd, line) == -1){
            if(errno == 0){
                puts(CFLRED "ERROR:" C_RST " Server closed connection.");
            }
            else{
                perror(CFLRED "ERROR" C_RST);
            }
            
            break;
        }
        
        putchar('\n');
    }
    
    free(line);
    
    /* Close the socket. */
    if(close(sockfd) != 0){
        perror("ERROR, close()");
        exit(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}




/**************************************************************************************
 * Execute command functions.
 *************************************************************************************/
int executeCommand(int sockfd, const char *command){
    SharedCommandType commandType;
    
    /* Determine the command type. */
    commandType = getSharedCommandType(command);
    
    /* Call the appropriate function to handle this known command. */
    switch(commandType){
        case command_unknown: {
            return executeLocalCommand(command);
        }
        
        case command_cd:
        case command_list:
        case command_md5:
        case command_pwd: {
            return executeServerReadOnlyCommand(sockfd, command);
        }
        
        case command_get: {
            return executeCommandget(sockfd, command);
        }
        
        case command_put: {
            return executeCommandput(sockfd, command);
        }
    }
    
    return 0;
}

int executeLocalCommand(const char *command){
    ClientCommandType clientCommandType;
    
    clientCommandType = getClientCommandType(command);
    
    switch(clientCommandType){
        case client_command_unknown: {
            if(system(command) == -1){
                return -1;
            }
            break;
        }
        
        case client_command_cd: {
            if(chdir(command + strlen(CLIENT_COMMAND_CD)) != 0){
                perror(CFLRED "ERROR" C_RST);
                return 1;
            }
            break;
        }
        
        case client_command_help: {
            printHelpScreen();
            break;
        }
    }
    
    return 0;
}

int executeServerReadOnlyCommand(int sockfd, const char *command){
    char buffer[BUFFER_SIZE]; 
    long n;                   /* Number of bytes read from the socket into the buffer */
    
    /* Send the command. */
    if(write(sockfd, command, strlen(command)+1) <= 0){
        return -1;
    }
    
    n = read(sockfd, buffer, sizeof(buffer));
    if(n == 0){
        return -1;
    }
    
    while(n > 0 && buffer[n-1] != '\0'){
        fwrite(buffer, 1, n, stdout);
        n = read(sockfd, buffer, sizeof(buffer));
    }
    
    if(n == -1){
        return -1;
    }
    
    /* Write the remaining bytes read. */
    fwrite(buffer, 1, n, stdout);
    
    return 0;
}

int executeCommandget(int sockfd, const char *command){
    char buffer[BUFFER_SIZE]; 
    long totalFileSize;       
    long numBytesLeft;        
    long n;                   
    int  displayNumBytesLeft; 
    
    const char *filePath;     
    const char *fileName;     
    FILE       *fp;           
    
    filePath = command + 3; /* Skip the leading "get" */
    
    /* Skip whitespace. */
    while(*filePath != '\0' && (*filePath == ' ' || *filePath == '\t')){
        filePath++;
    }
    
    if(*filePath == '\0'){ /* No file specified. */
        printf(CFLRED "ERROR:" C_RST " get requires a path to a file.");
        return 1;
    }
    else if(filePath - command > 4){ /* More than one whitespace. */
        printf(CFLRED "ERROR:" C_RST " only one whitespace is permitted between the command and the argument.");
        return 1;
    }
    
    /* Extract the file name from the file path. */
    fileName = extractFileName(filePath);
    if(fileName == NULL){
        printf(CFLRED "ERROR:" C_RST " %s is a directory.", filePath);
        return 1;
    }
    
    /* Check if the file alreay exists. */
    if(fileExists(fileName)){
        printf(CFLRED "ERROR:" C_RST " The file %s already exists.\n", filePath);
        return 1;
    }
    
    /* Create the file. */
    fp = fopen(fileName, "w");
    if(fp == NULL){
        return -1;
    }
    
    /* Send the command. */
    if(write(sockfd, command, strlen(command)+1) < 0){
        return -1;
    }
    
    /* Get the get reply. */
    n = read(sockfd, buffer, GET_REPLY_SIZE);
    if(memcmp(buffer, GET_REPLY_NO, strlen(GET_REPLY_NO)) == 0){ /* An error occured, get the error message. */
        n = read(sockfd, buffer, sizeof(buffer));
        if(n < 0){
            return -1;
        }
        fwrite(buffer, 1, n, stdout);
        putchar('\n');
        
        /* Close and delete the file (fopen creates the file). */
        if(fclose(fp) != 0){
            return -1;
        }
        if(remove(fileName) != 0){
            return -1;
        }
        
        return 1;
    }
    
    /* Extract the size of the file from the get reply. */
    totalFileSize = *((long *)(buffer + strlen(GET_REPLY_NO)));
    numBytesLeft  = totalFileSize;
    
    /* Download the file. */
    displayNumBytesLeft = 0;
    while(numBytesLeft > 0 && (n=read(sockfd, buffer, sizeof(buffer))) > 0){
        fwrite(buffer, 1, n, fp);
        numBytesLeft -= n;
        
        /* Display the download status every DISPLAY_GET_PUT_INTERVAL packets. */
        if(displayNumBytesLeft >= DISPLAY_GET_PUT_INTERVAL){
            printf("%15ld / %ld (%%%2.2f)\r", numBytesLeft, totalFileSize, ((double)(totalFileSize-numBytesLeft) / totalFileSize) * 100.0);
            fflush(stdout);
            displayNumBytesLeft = 0;
        }
        displayNumBytesLeft++;
    }
    
    /* Check if an error occured. */
    if(n <= 0){
        puts(CFLRED "ERROR:" C_RST " Could not download file.");
        fclose(fp);
        if(n == 0){
            errno = 0;
        }
        return -1;
    }
    
    puts("File downloaded, use 'smd5sum' to verify the files checksum on the server,\n"
         "and then 'md5sum' on your computer, if they match, then the file was\n"
         "download without error.");
    
    fclose(fp);
    
    return 0;
}

int executeCommandput(int sockfd, const char *command){
    char buffer[BUFFER_SIZE]; 
    long totalFileSize;       
    long numBytesLeft;        
    long n;                   
    int  displayNumBytesLeft; 
    
    const char *filePath;     
    const char *fileName;     
    FILE       *fp;           
    
    long ret;
    
    filePath = command + 3; /* Skip the leading "get" */
    
    /* Skip whitespace. */
    while(*filePath != '\0' && (*filePath == ' ' || *filePath == '\t')){
        filePath++;
    }
    
    /* No parameters were specified. */
    if(*filePath == '\0'){
        printf(CFLRED "ERROR:" C_RST " put requires a path to a file.");
        return 1;
    }
    
    /* More than one whitespace. */
    if(filePath - command > 4){
        printf(CFLRED "ERROR:" C_RST " only one whitespace is permitted between the command and the argument.");
        return 1;
    }
    
    /* Determine whether this file is a regular file. */
    ret = isRegularFile(filePath);
    if(ret == 0){
        printf("%s is not a regular file.\n", filePath);
        return 1;
    }
    else if(ret == -1){
        perror(CFLRED "ERROR" C_RST);
        return 1;
    }
    
    /* Extract the file name from the path */
    fileName = extractFileName(filePath);
    
    /* Create a new command which contains just "put " and the file name. */
    memcpy(buffer, "put ", 4);
    memcpy(buffer + strlen("put "), fileName, strlen(fileName)+1);
    
    /* Open the file. */
    fp = fopen(filePath, "r");
    if(fp == NULL){
        perror(CFLRED "ERROR" C_RST);
        return 1;
    }
    
    /* Send the command. */
    if(write(sockfd, buffer, strlen(buffer)+1) <= 0){
        return -1;
    }
    
    /* Get the servers response (is it ok to upload this file or not). */
    n = read(sockfd, buffer, PUT_REPLY_SIZE);
    if(n <= 0){
        return -1;
    }
    
    if(memcmp(buffer, PUT_REPLY_NO, strlen(PUT_REPLY_NO)) == 0){
        /* An error occured, get the error message. */
        n = read(sockfd, buffer, sizeof(buffer));
        if(n <= 0){
            return -1;
        }
        fwrite(buffer, 1, n, stdout);
        
        fclose(fp);
        return 1;
    }
    
    /* Send the file size. */
    totalFileSize = fileSize(filePath);
    numBytesLeft  = totalFileSize;
    
    if(write(sockfd, &totalFileSize, sizeof(totalFileSize)) <= 0){
        return -1;
    }
    
    /* Send the file. */
    displayNumBytesLeft = 0;
    while((n=fread(buffer, 1, sizeof(buffer), fp)) > 0){
        if(write(sockfd, buffer, n) <= 0){
            return -1;
        }
        numBytesLeft -= n;
        
        /* Display the upload status every DISPLAY_GET_PUT_INTERVAL packets. */
        if(displayNumBytesLeft >= DISPLAY_GET_PUT_INTERVAL){
            printf("%15ld / %ld (%%%2.2f)\r", numBytesLeft
                                            , totalFileSize
                                            , ((double)(totalFileSize-numBytesLeft)) / totalFileSize * 100.0);
            fflush(stdout);
            displayNumBytesLeft = 0;
        }
        displayNumBytesLeft++;
    }
    
    fclose(fp);
    
    puts("File uploaded, use 'smd5sum' to verify the files checksum on the server,\n"
         "and then 'md5sum' on your computer, if they match, then the file was\n"
         "uploaded without error.");
    
    return 0;
}

ClientCommandType getClientCommandType(const char *command){
    if(strncmp(CLIENT_COMMAND_CD, command, strlen(CLIENT_COMMAND_CD)) == 0){
        return client_command_cd;
    }
    else if(strcmp("help" , command) == 0){
        return client_command_help;
    }
    
    return client_command_unknown;
}




/**************************************************************************************
 * Connect functions.
 *************************************************************************************/
int connectipport(const char *ip, const char *port, int *fd){
    int ret;
    struct addrinfo hints;
    struct addrinfo *serverInfo;
    struct addrinfo *current;
    
    /* Fill out the hints struct. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    /* Either ipv4 or ipv6 */
    hints.ai_socktype = SOCK_STREAM;  /* Reliable TCP sockets. */
    hints.ai_flags    = AI_PASSIVE;   /* Fill in everything for me. */
    
    /* Get a list of potential sockets to connect to. */
    ret = getaddrinfo(ip, port, &hints, &serverInfo);
    if(ret != 0){
        *fd = ret;
        return -1;
    }
    
    /* Iterate through the list and attempt to connect to
     * each element.
     * 
     * Stop at the first succesful connection.
     */
    for(current=serverInfo; current != NULL; current=current->ai_next){
        *fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        
        /* Could not connect, move on to the next link. */
        if (*fd == -1){
            continue;
        }
        
        /* TODO: Hangs up here, set the socket to non-blocking temporarily. */
        if(connect(*fd, current->ai_addr, current->ai_addrlen) != -1){
            break;
        }
        
        close(*fd);
    }
    
    /* Free the linked list. */
    free(serverInfo);
    
    /* Could not connect. */
    if(current == NULL){
        return -2;
    }
    
    return 0;
}




/**************************************************************************************
 * Printing functions.
 *************************************************************************************/
int printPrompt(){
    char *cwd;
    int  cwdSize;
    
    /* Get the cwd. */
    cwdSize = 100;
    cwd = malloc(cwdSize);
    if(cwd == NULL){
        return -1;
    }
    
    /* An error occured getting the cwd. */
    if(getcwd(cwd, cwdSize) == NULL){
        /* Buffer size needs to be increased. */
        while(errno == ERANGE){
            cwdSize *= 2;
            cwd = realloc(cwd, cwdSize);
            if(cwd == NULL){
                return -1;
            }
            
            errno = 0;
            getcwd(cwd, cwdSize);
        }
        
        /* The size of the buffer was not the issue, some other error occured. */
        if(errno != 0){
            free(cwd);
            perror("ERROR, printPrompt()");
            return -1;
        }
    }
    
    /* Print out the cwd. */
    printf(CFLYLW "%s" CFLGRN " > " C_RST, cwd);
    
    free(cwd);
    
    return 0;
}

void printUsage(const char *executableName){
    printf("USAGE:   %s <ip> <port>\n", executableName);
    printf("EXAMPLE: %s 127.0.0.1 12345\n", executableName);
}

void printHelpScreen(){
    /* Legend. */
    puts(CFLBLU "Legend:" C_RST "\n"
         "  []                   - Optional arguments\n");
    
    /* Client commands. */
    puts(CFLBLU "Recognized client side commands:" C_RST "\n"
         "  cd PATH              - Change directory.\n"
         "  help                 - Print this help screen.\n");
    
    /* Server commands. */
    puts(CFLBLU "Recognized server side commands:" C_RST "\n"
         "  scd PATH             - Server change directory.\n"
         "  sls [PATH] [OPTIONS] - Server list files (compatible with all 'ls' arguments).\n"
         "  spwd                 - Server print working directory\n"
         "  smd5sum FILES        - Server compute the md5 for the following set of files.\n");
    
    /* Download/Upload commands. */
    puts(CFLBLU "File transfer commands:" C_RST "\n"
         "  get FILE             - Download a file from the server.\n"
         "  put FILE             - Upload a file to the servers current working directory.\n");
}
