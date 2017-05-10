#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shared.h"




SharedCommandType getSharedCommandType(const char *command){
    if      (strncmp("sls" , command, 3) == 0)     { return command_list; }
    else if (strncmp("scd " , command, 4) == 0)    { return command_cd;   }
    else if (strncmp("get ", command, 4) == 0)     { return command_get;  }
    else if (strncmp("put ", command, 4) == 0)     { return command_put;  }
    else if (strncmp("spwd", command, 4) == 0)     { return command_pwd;  }
    else if (strncmp("smd5sum ", command, 8) == 0) { return command_md5;  }
    
    return command_unknown;
}




/*********************************************************************************
 * File functions below.
 ********************************************************************************/
long fileSize(const char *filePath) {
    struct stat s;
    
    if(stat(filePath, &s) == 0){
        return s.st_size;
    }
    
    return -1;
}

int isRegularFile(const char *filePath){
    struct stat s;
    if(stat(filePath, &s) != 0){
        return -1;
    }
    return S_ISREG(s.st_mode);
}

int fileExists(const char *filePath){
    return access(filePath, F_OK) == 0;
}

const char *extractFileName(const char *filePath){
    const char *endFilePath;
    
    endFilePath = filePath + strlen(filePath)-1;
    
    if(*endFilePath == '/'){
        return NULL;
    }
    
    while(endFilePath > filePath && *endFilePath != '/'){
        endFilePath--;
    }
    
    if(*endFilePath == '/'){
        endFilePath++;
    }
    
    return endFilePath;
}