#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <sys/wait.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "shared.h"
#include "server.h"

int main(int argc, char **argv){
    const char *portstr;          /*  */
    
    struct addrinfo hints;        /*  */
    struct addrinfo *serverInfo;  /*  */
    int listenfd;                 /*  */
    int sockfd;                   /*  */
    int ret;                      /*  */
    
    long n;                /*  */
    char buffer[BUFFER_SIZE]; /*  */
    
    struct sockaddr_storage clientAddr;
    socklen_t               clientAddrSize;
    
    pid_t pid;
    
    /* Not the right amount of arguments. */
    if(argc != 2){
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    /* Register the SIGCHLD handler (to reap zombie processes). */
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR){
        perror("ERROR");
        exit(EXIT_FAILURE);
    }
    
    /* Get the port from the command line. */
    portstr = argv[1];
    
    /* Start up the server. */
    printf("Starting up server on port %s...\n", portstr);
    
    /* Fill out the hints struct. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
    
    /* Get a list of possible sockets to bind to on the local machine. */
    ret = getaddrinfo(NULL, portstr, &hints, &serverInfo);
    if(ret != 0){
        printf("ERROR, getaddrinfo(): %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    
    /* Just bind on the first address. */
    listenfd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if(listenfd == -1){
        perror("ERROR, socket()");
        exit(EXIT_FAILURE);
    }
    
    ret = bind(listenfd, serverInfo->ai_addr, serverInfo->ai_addrlen);
    if(ret != 0){
        perror("ERROR, bind()");
        freeaddrinfo(serverInfo);
        exit(EXIT_FAILURE);
    }
    
    /* free the list given to us by getaddrinfo(). */
    freeaddrinfo(serverInfo);
    
    /* Mark sockfd as a listening socket. */
    ret = listen(listenfd, BACKLOG);
    if(ret != 0){
        perror("ERROR, listen()");
        exit(EXIT_FAILURE);
    }
    
    /* Server is ready to accept connections now. */
    printf("Server succesfully started.\n");
    
    while(1){
        /* Accept a connection. */
        clientAddrSize = sizeof(clientAddr);
        sockfd         = accept(listenfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
        
        /* accept() error. */
        if(sockfd < 0){
            perror("ERROR, accept()");
            exit(EXIT_FAILURE);
        }
        
        printClientDetails((struct sockaddr *)&clientAddr, clientAddrSize, ": " CFLGRN "Connected." C_RST "\n");
        
        pid = fork();
        
        /* fork error. */
        if(pid == -1){
            perror("ERROR");
            exit(EXIT_FAILURE);
        }
        
        /* Child will now handle the client, parent will continue listening. */
        if(pid == 0){
            /* Handle this client until THEY close the connection. */
            while((n=recv(sockfd, buffer, sizeof(buffer), 0)) > 0){
                /* Print out client details. */
                printClientDetails((struct sockaddr *)&clientAddr, clientAddrSize, ": ");
                
                /* Command is not null terminated, exit to prevent segmentation fault. */
                if(buffer[n-1] != '\0'){
                    puts(CFLRED "ERROR:" C_RST " Client sent a non-null terminated command, exiting...");
                    exit(EXIT_FAILURE);
                }
                
                /* Print out the command sent in blue. */
                printf(CFLBLU);
                fwrite(buffer, 1, n, stdout);
                puts(C_RST);
                
                
                /* Execute the command. */
                if(executeCommand(sockfd, buffer) == -1){
                    if(errno != 0){
                        perror(CFLRED "ERROR" C_RST);
                    }
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            }
            
            if(n == -1){
                perror(CFLRED "ERROR" C_RST);
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            
            printClientDetails((struct sockaddr *)&clientAddr, clientAddrSize, ": " CFLRED "Disconnected." C_RST "\n");
            exit(EXIT_SUCCESS);
        }
    }
    
    /* Close the listen socket. */
    if(close(listenfd) != 0){
        perror("ERROR, close() listening socket");
        exit(EXIT_FAILURE);
    }
    
    return EXIT_FAILURE;
}

int executeCommand(int sockfd, const char *command){
    SharedCommandType commandType;
    
    commandType = getSharedCommandType(command);
    
    switch(commandType){
        case command_cd:   { return executeCommandcd(sockfd, command); }
        
        case command_list:
        case command_pwd:
        case command_md5:  { return executeReadOnlyUnixCommand(sockfd, command); }
        
        case command_get: { return executeCommandget(sockfd, command); }
        case command_put: { return executeCommandput(sockfd, command); }
        
        case command_unknown: {
            return -1;
        }
    }
    
    return 0;
}

int executeCommandget(int sockfd, const char *command){
    char buffer[BUFFER_SIZE];
    long n;                   /* Number of bytes read. */
    long size;                /* File size. */
    
    const char *filePath;
    FILE *fp;
    
    const char *errorstr;
    
    long ret;                 /* Return value for various functions. */
    
    /* Skip the initial "get " in the command string. */
    filePath = command + 4;
    
    /* Determine if the file is a regular file. */
    ret = isRegularFile(filePath);
    if(ret == -1){ /* An error occured in isRegularFile() */
        errorstr = strerror(errno);
        ret      = sendGetReplyNo(sockfd, errorstr);
        return 1;
    }
    else if(ret == 0){ /* File is a directory. */
        errorstr = "Can not download directory.";
        ret      = sendGetReplyNo(sockfd, errorstr);
        
        if(ret != 0){
            return -1;
        }
        
        return 1;
    }
    
    /* Open the file. */
    fp = fopen(filePath, "r");
    if(fp == NULL){
        errorstr = strerror(errno);
        ret      = sendGetReplyNo(sockfd, errorstr);
        
        if(ret != 0){
            return -1;
        }
        
        return 1;
    }
    
    /* Send an OK message and the file size. */
    size = fileSize(filePath);
    memcpy(buffer, GET_REPLY_OK, strlen(GET_REPLY_OK));       /* Set the start of the packet to OK */
    memcpy(buffer+strlen(GET_REPLY_OK), &size, sizeof(long)); /* Append the size of the file. */
    ret = write(sockfd, buffer, GET_REPLY_SIZE);              /* Send the packet. */
    if(ret <= 0){
        return -1;
    }
    
    /* Send the file data. */
    while((n=fread(buffer, 1, sizeof(buffer), fp)) > 0){
        n = write(sockfd, buffer, n);
        if(n <= 0){
            return -1;
        }
    }
    
    fclose(fp);
    
    /* Check if an error occured. */
    if(ferror(fp)){
        return -1;
    }
    
    return 0;
}

int executeCommandput(int sockfd, const char *command){
    char buffer[BUFFER_SIZE];
    long numBytesLeft;
    long n;
    
    const char *errorstr;
    
    const char *fileName;
    FILE *fp;
    
    fileName = command + 4; /* Skip the leading "put " */
    
    /* Check if the file already exists. */
    if(fileExists(fileName)){
        errorstr = "File already exists";
        
        /* Send the PUT_REPLY_NO packet. */
        if(write(sockfd, PUT_REPLY_NO, strlen(PUT_REPLY_NO)) <= 0){
            return -1;
        }
        
        /* Send the error message. */
        if(write(sockfd, errorstr, strlen(errorstr)) <= 0){
            return -1;
        }
        
        return 1;
    }
    
    /* Create the file. */
    fp = fopen(fileName, "w");
    
    /* Could not create file, send "NO" error message. */
    if(fp == NULL){
        errorstr = strerror(errno);                        /* Get the error message. */
        
        /* Send the PUT_REPLY_NO packet. */
        if(write(sockfd, PUT_REPLY_NO, strlen(PUT_REPLY_NO)) <= 0){
            return -1;
        }
        
        /* Send the error message. */
        if(write(sockfd, errorstr, strlen(errorstr)) <= 0){
            return -1;
        }
        return 1;
    }
    
    /* Send OK reply. */
    if(write(sockfd, PUT_REPLY_OK, strlen(PUT_REPLY_OK)) <= 0){
        return -1;
    }
    
    /* Get the file size. */
    n = read(sockfd, &numBytesLeft, sizeof(numBytesLeft));
    
    if(n <= 0){
        return -1;
    }
    
    /* Download the file. */
    while(numBytesLeft > 0 && (n=read(sockfd, buffer, sizeof(buffer))) > 0){
        fwrite(buffer, 1, n, fp);
        numBytesLeft -= n;
    }
    
    fclose(fp);
    
    if(n < 0){
        return -1;
    }
    
    return 0;
}

int executeCommandcd(int sockfd, const char *command){
    const char *successstr = "Directory Changed.";
    const char *directory;
    const char *errorstr;
    long       ret;
    
    /* Skip the initial "scd " */
    directory = command + 4;
    
    /* Attempt to change the directory. */
    ret = chdir(directory);
    
    /* An error occured, send an error message. */
    if(ret != 0){
        errorstr = strerror(errno);
        ret      = write(sockfd, errorstr, strlen(errorstr)+1);
    }
    /* Everything went OK, send a success message. */
    else{
        ret = write(sockfd, successstr, strlen(successstr)+1);
    }
    
    if(ret <= 0){
        return -1;
    }
    
    return 0;
}

int executeReadOnlyUnixCommand(int sockfd, const char *command){
    FILE *pipefp;
    char buffer[BUFFER_SIZE];
    long n;
    
    const char *stderrRedirectstr = " 2>&1"; /* So that we can also send the stderr output of the command. */
    long commandLength;
    long stderrRedirectstrLength;
    char *fullcommand;
    
    /* Skip the leading 's', for example: skip the first 's' in 'sls' or 'spwd'. */
    command++;
    
    /* Calculate the lengths of the command and the stderr redirection string. */
    commandLength           = strlen(command);
    stderrRedirectstrLength = strlen(stderrRedirectstr);
    
    /* Allocate memory for the full command. */
    fullcommand = malloc(commandLength + stderrRedirectstrLength + 1);
    if(fullcommand == NULL){
        perror("ERROR");
        return -1;
    }
    
    /* Concatenate the command with the stderr redirection string. */
    memcpy(fullcommand, command, commandLength);
    memcpy(fullcommand+commandLength, stderrRedirectstr, stderrRedirectstrLength+1);
    
    /* Open a pipe to the process. */
    pipefp = popen(fullcommand, "r");
    free(fullcommand);
    
    while((n=fread(buffer, 1, sizeof(buffer), pipefp)) == sizeof(buffer)){
        write(sockfd, buffer, n);
    }
    /* An error occured while reading from the pipe. */
    if(ferror(pipefp)){
        perror("ERROR");
        /*
        errorstr = strerror(errno);
        write(sockfd, errorstr, strlen(errorstr)+1);
        */
        return -1;
    }
    
    if(feof(pipefp)){
        if(n > 0){
            write(sockfd, buffer, n);
        }
        /* Send a packet containing only a null terminating byte to indicate the end of transmission. */
        buffer[0] = '\0';
        write(sockfd, buffer, 1);
    }
    pclose(pipefp);
    
    
    
    return 0;
}

void printUsage(const char *executableName){
    printf("USAGE:   Start up a server on the local machine.\n"
           "         %s <port>\n"
           "\n"
           "EXAMPLE: %s 12345\n", executableName, executableName);
}

int printClientDetails(const struct sockaddr *clientAddress, socklen_t clientAddressSize, const char *str){
    char buffer[INET6_ADDRSTRLEN];
    const char *ret;
    
    /* Determine whether clientAddress is an ipv4 or ipv6 address and convert it to a readable string. */
    switch(clientAddress->sa_family){
        case AF_INET: {
            ret = inet_ntop(AF_INET, &(((struct sockaddr_in *)clientAddress)->sin_addr), buffer, clientAddressSize);
            break;
        }
        case AF_INET6: {
            ret = inet_ntop(AF_INET, &(((struct sockaddr_in *)clientAddress)->sin_addr), buffer, clientAddressSize);
            break;
        }
        default: {
            return -1;
        }
    }
    
    /* inet_ntop() failed. */
    if(ret == NULL){
        return -1;
    }
    
    /* If str is NULL, simply print out the client details, otherwise, print out the client details with the message following it. */
    if(str != NULL){
        printf("%s%s", buffer, str);
    }
    else{
        printf("%s", buffer);
    }
    
    return 0;
}

int sendGetReplyNo(int sockfd, const char *errorstr){
    char buffer[BUFFER_SIZE];
    long ret;
    
    memcpy(buffer, GET_REPLY_NO, strlen(GET_REPLY_NO));     /* Set the start of the packet to NO */
    memset(buffer + strlen(GET_REPLY_NO), 0, sizeof(long)); /* Pad the rest of the packet with zeros. */
    
    ret = write(sockfd, buffer, GET_REPLY_SIZE);            /* Send the packet. */
    if(ret <= 0){
        return -1;
    }
    
    ret = write(sockfd, errorstr, strlen(errorstr));        /* Send the error message. */
    if(ret <= 0){
        return -1;
    }
    
    return 0;
}
