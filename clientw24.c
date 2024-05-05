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
    char message[620000];
    int server;
    int portNumber = 16500; // Correct data type and value assignment
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
        fprintf(stderr, "inet_pton() has failed\n");
        exit(2);
    }

    if (connect(server, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0)
    {
        fprintf(stderr, "connect() failed, exiting\n");
        exit(3);
    }

    while (1) // Loop to allow multiple commands and responses
    {
        printf("client24$ ");
        fgets(message, sizeof(message), stdin); // Read input including spaces
        message[strcspn(message, "\n")] = '\0'; // Remove trailing newline if present

        if (strlen(message) == 0) {
            continue; // Skip empty messages
        }

        if (strcmp(message, "exit") == 0) {
            printf("Exiting...\n");
            break; // Exit loop if user types "exit"
        }

        // Check if the user wants to quit and send PID to server
        if (strcmp(message, "quitc") == 0) {
            sprintf(message, "quitc %d", getpid()); // Include PID in the message
        }

        write(server, message, strlen(message)); // Send the message to the server

        int n = read(server, message, sizeof(message)-1); // Read response from server
        if (n < 0) {
            fprintf(stderr, "read() error\n");
            break;
        } else {
            message[n] = '\0'; // Null-terminate the received data
            printf("Server's response: %s\n", message); // Print server's response
        }
    }

    close(server); // Close the connection
    exit(0);
}
