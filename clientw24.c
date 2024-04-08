#include <netinet/in.h> // Structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // For socket APIs
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h> // For inet_pton
#include <unistd.h> // For read/write

int main(int argc, char *argv[])
{ 
    char message[100];
    int server;
    int portNumber = 8001; // Correct data type and value assignment
    socklen_t len;
    struct sockaddr_in servAdd;

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { 
        fprintf(stderr, "Cannot create socket\n");
        exit(1);
    }

    servAdd.sin_family = AF_INET; // Internet
    servAdd.sin_port = htons(portNumber); // Corrected conversion to network byte order

    if (inet_pton(AF_INET, "10.60.8.51", &servAdd.sin_addr) < 0)
    {
        fprintf(stderr, " inet_pton() has failed\n");
        exit(2);
    }

    if (connect(server, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0)
    {
        fprintf(stderr, "connect() failed, exiting\n");
        exit(3);
    }

    if (read(server, message, 100) < 0)
    {
        fprintf(stderr, "read() error\n");
        exit(3);
    }

    fprintf(stderr, "%s\n", message);
    char buff[50];
    printf("\nEnter the message to be sent to the server\n");
    scanf("%49s", buff); // Corrected to avoid potential buffer overflow

    write(server, buff, strlen(buff)); // Send only the actual message length

    exit(0);
}