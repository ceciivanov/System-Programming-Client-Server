#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>


#define BUFFSIZE 4096   // max length of path for given file/directory


// --------------- Used Functions  --------------

// create dir with given name
int create_dir(char *name);

// create file with given name and return FILE* pointer to it
FILE* create_file(char *name);

// copy contents of file that are sent from socket, to local file with FILE pointer fp
void copy_file(FILE* fp, int socket);

// ---------------------------------------------


int main(int argc, char *argv[]) {

    // arguments
    char serverIP[40];
    char *directory = calloc(BUFFSIZE, sizeof(char));
    int port;

    for (int i=1; i<argc; i++) {

        if (strcmp(argv[i-1], "-p") == 0) {
            port = atoi(argv[i]);
        }
        if (strcmp(argv[i-1], "-i") == 0) {
            strcpy(serverIP, argv[i]);
        }
        if (strcmp(argv[i-1], "-d") == 0) {
            strcpy(directory, argv[i]);
        }
    }
    
    printf("Client's parameters are:\n");
    // ----------------------------------
    printf("Server's IP: %s\n", serverIP);
    printf("Port: %d\n", port);
    printf("Directory: %s\n", directory);

    // create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // this is for binding to the port so it can be used again
    int val=1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(serverIP);

    // connect to socket
    int connection = connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr));

    if (connection == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    printf("Connecting to %s on port %d\n", serverIP, port);
    // ------------------------------------------------------
    
    // 1. Send to server how many bytes he will read for the directory name
    int bytes_to_write = htons(strlen(directory));
    write(sock, &bytes_to_write, sizeof(bytes_to_write));

    // 2. Send the desired directory to copy
    write(sock, directory, strlen(directory));

    // 3. Read the number of files that are gonna be copied
    int no_files = 0;
    read(sock, &no_files, sizeof(no_files));
    no_files = ntohs(no_files);

    // for each file that is gonna be copied
    for (int i=0; i<no_files; i++) {

        // 1. Read the number of bytes for the filename
        int bytes_to_read = 0;
        read(sock, &bytes_to_read, sizeof(bytes_to_read));
        bytes_to_read = ntohs(bytes_to_read);

        // 2. Read the filename (relative path)
        char* filename = calloc(bytes_to_read, sizeof(char));
        read(sock, filename, bytes_to_read);

        printf("Received file: %s\n", filename);
        // ---------------------------------------

        // 3. Create needed directory hierarchy
        char* temp = strdup(filename);
        char* token = strtok(temp, "/");
        char* dir = malloc(strlen(token));
        char* last = NULL;

        // tokenize with delimeter '/' to get each subdirectory
        while (token != NULL) {
            
            if (last != NULL) {
                
                // concatenate last subdirectory to the new one to construct the relative path for the file
                // ** if client asks for test/fold2/fold3 (meaning only fold3) we, the result will be also the fold3 directory but within the test/fold2
                dir = realloc(dir, strlen(dir) + strlen(last) + 1);
                strcat(dir, last);
                strcat(dir, "/");
                
                create_dir(dir);
            }

            last = token;
            token = strtok(NULL, "/");
        }

        // 4. Create file
        FILE* fp = create_file(filename);
        
        // 5. Copy contents to the file
        copy_file(fp, sock);

        free(temp);
    }

    close(sock);
    free(directory);

    return 0;
}


// copy contents of file that are passed from server through the socket to local file with FILE pointer fp
// -------------------------------------------------------------------------------------------------------
void copy_file(FILE* fp, int socket) {

    // 1. Read metadata of file (file_size in bytes)
    int file_sz = 0;
    read(socket, &file_sz, sizeof(file_sz));
    file_sz = ntohl(file_sz);

    // copy untill all bytes from file are copied, client knows exactly how much he will read and doesn't block on any read call
    int total_bytes_copied = 0;

    while (total_bytes_copied < file_sz) {         

        // 2. Read number of bytes per block to read
        int block_bytes = 0;
        read(socket, &block_bytes, sizeof(block_bytes));
        block_bytes = ntohs(block_bytes);

        // 3. Read contents and store them in block variable
        char* block = calloc(block_bytes, sizeof(char));
        read(socket, block, block_bytes);

        // 4. Write all the bytes of block into the file
        fwrite(block, sizeof(char), block_bytes, fp);

        total_bytes_copied += block_bytes;
    }
}


// create directory with given name
// --------------------------------
int create_dir(char *name) {

    int bytes = strlen(name);

    char *dir = malloc(bytes*sizeof(char));
    if (dir == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    memset(dir, 0, bytes);

    // remove the last '/' character from the name  e.x  bar/foo/foobar/ 
    memcpy(dir, name, bytes-1);
    dir[bytes-1] = '\0';

    if (mkdir(dir, S_IRWXU) != 0 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    return 0;
}


// return 1 if given file exists or 0
// ----------------------------------
int file_exists(char *filename) {
    
    struct stat   buffer;   
    return (stat (filename, &buffer) == 0);
}


// create/open file with given name and return FILE pointer
// -------------------------------------------------------
FILE* create_file(char *name) {

    if (file_exists(name)) {        // if it exists delete it and create new one
        unlink(name);
    }

    FILE* fp = fopen(name, "w");    // creates and open file with write mode
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    return fp;                      // return file pointer
}