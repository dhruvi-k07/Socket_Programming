#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for fork, getpid
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h> // for sockaddr_in
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h> // for uint16_t
#include <sys/stat.h>
#include <time.h>
#include <netdb.h>
#include <sys/signal.h>
#include <pwd.h>
#include <ctype.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define MIRROR1_PORT 17500
#define WNOHANG 1

char **dirArray = NULL; // Global pointer to array of directory paths
size_t dirCount = 0;    // Number of directories stored in the array

typedef struct DirectoryNode
{
    char path[1024]; // Changed from 'name' to 'path' to store full paths
    time_t creation_time;
    struct DirectoryNode *next;
} DirectoryNode;

// Function to free the linked list
void freeList(DirectoryNode *head)
{
    DirectoryNode *temp;
    while (head != NULL)
    {
        temp = head;
        head = head->next;
        free(temp);
    }
}

// Function to find a file and return its details
int findFile(const char *basePath, const char *searchFilename, char *fileDetails)
{
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char path[1024];
    int found = 0;

    if (!(dir = opendir(basePath)))
        return 0;

    while (!found && (entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);
                found = findFile(path, searchFilename, fileDetails);
            }
        }
        else if (strcmp(entry->d_name, searchFilename) == 0)
        {
            snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);
            if (stat(path, &fileStat) == 0)
            {
                char perm[100];
                strftime(perm, sizeof(perm), "%A %b %d %Y", localtime(&fileStat.st_ctime)); // Get creation time
                snprintf(fileDetails, 1024, "Filename: %s\nSize: %ld bytes\nCreated: %s\nPermissions: %o\n",
                         path, fileStat.st_size, perm, fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
                found = 1;
            }
        }
    }
    closedir(dir);
    return found;
}
// Function to add directory to the array
void addDirectory(const char *path)
{
    char **temp = realloc(dirArray, (dirCount + 1) * sizeof(char *));
    if (temp == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    dirArray = temp;
    dirArray[dirCount] = strdup(path); // Duplicate the path string
    if (dirArray[dirCount] == NULL)
    {
        fprintf(stderr, "Memory allocation failed for path duplication\n");
        return;
    }
    dirCount++;
}

// Comparator function for qsort, for case-sensitive sorting
int compare(const void *a, const void *b)
{
    const char *dirA = *(const char **)a;
    const char *dirB = *(const char **)b;
    return strcmp(dirA, dirB);
}

// Recursive function to collect directories
void collectDirectories(const char *dir_name)
{
    DIR *dir;
    struct dirent *entry;
    char subdir_path[1024];

    if ((dir = opendir(dir_name)) == NULL)
    {
        fprintf(stderr, "Cannot open directory %s\n", dir_name);
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && (entry->d_name[0] == '.' || entry->d_name[0] == '_'))
        {
            continue; // Ignore directories starting with '.' or '_'
        }

        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_name, entry->d_name);
            addDirectory(subdir_path); // Add directory path to the array

            // Recursively collect subdirectory names
            collectDirectories(subdir_path);
        }
    }

    closedir(dir);
}

// Function to insert a new node in sorted order based on creation time
void insertSorted(DirectoryNode **head, DirectoryNode *newNode)
{
    DirectoryNode *current;
    if (*head == NULL || (*head)->creation_time > newNode->creation_time)
    {
        newNode->next = *head;
        *head = newNode;
    }
    else
    {
        current = *head;
        while (current->next != NULL && current->next->creation_time < newNode->creation_time)
        {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
}

// Recursive function to collect directories, ignoring those starting with . and _
void collectDirectoriesByTime(const char *base_path, DirectoryNode **head)
{
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char path[1024];

    if ((dir = opendir(base_path)) == NULL)
    {
        fprintf(stderr, "Cannot open directory %s\n", base_path);
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && (entry->d_name[0] != '.' && entry->d_name[0] != '_'))
        {
            snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
            if (stat(path, &fileStat) < 0)
            {
                fprintf(stderr, "Error getting status of %s: %s\n", path, strerror(errno));
                continue;
            }

            DirectoryNode *newNode = malloc(sizeof(DirectoryNode));
            if (newNode == NULL)
            {
                fprintf(stderr, "Memory allocation failed\n");
                closedir(dir);
                return;
            }

            strcpy(newNode->path, path);
            newNode->creation_time = fileStat.st_ctime;
            newNode->next = NULL;
            insertSorted(head, newNode);

            // Recursively add subdirectories
            collectDirectoriesByTime(path, head);
        }
    }
    closedir(dir);
}

void listDirectoriesByTime(const char *dir_name, char *response)
{
    DirectoryNode *head = NULL;
    collectDirectoriesByTime(dir_name, &head);

    // Build the response from the sorted list
    DirectoryNode *current = head;
    while (current != NULL)
    {
        strcat(response, current->path);
        strcat(response, "\n");
        current = current->next;
    }

    // Free the linked list
    freeList(head);
}

void listDirectories(const char *dir_name, char *response)
{
    // Initialize or clear previous data
    free(dirArray);
    dirArray = NULL;
    dirCount = 0;

    // Collect all directories
    collectDirectories(dir_name);

    // Sort the collected directory paths
    if (dirArray != NULL)
    {
        qsort(dirArray, dirCount, sizeof(char *), compare);
    }

    // Build the response from sorted array
    for (size_t i = 0; i < dirCount; i++)
    {
        strcat(response, dirArray[i]);
        strcat(response, "\n");
        free(dirArray[i]); // Free the duplicated path string
    }

    // Free the directory array
    free(dirArray);
    dirArray = NULL;
    dirCount = 0;
}

// for directory operations
void crequest(int csd)
{
    char message[255];
    int n;

    // write(csd, "Welcome to the server!\n", 23); // Send welcome message to client

    while (1)
    {
        char npsd_commd[1024] = {0};           // Command buffer
        if ((n = read(csd, message, 255)) > 0) // Read client command
        {
            message[n] = '\0'; // Null terminate message
            printf("Received command from client: %s\n", message);
            if (strcasecmp(message, "dirlist -a") == 0) // Check if client wants directory listing
            {
                char *base_dir = "/home/kunvara";
                char response[6300000] = ""; // Buffer to store directory listing

                // Recursively traverse directories
                listDirectories(base_dir, response);

                // Send directory listing back to the client
                write(csd, response, strlen(response));
            }
            else if (strcasecmp(message, "dirlist -t") == 0) // Check if client wants directory listing by creation time
            {
                char *base_dir = "/home/kunvara";
                char response[620000] = ""; // Buffer to store directory listing

                // List directories in the order of creation time
                listDirectoriesByTime(base_dir, response);

                // Send directory listing back to the client
                write(csd, response, strlen(response));
            }
            else
            {
                // Tokenize the message by space
                char *token = strtok(message, " ");
                if (token == NULL)
                {
                    // Handle empty command
                    printf("Empty command received.\n");
                    continue;
                }
                printf("%s\n", token);

                // Compare the first token with known commands
                if (strcasecmp(token, "quitc") == 0)
                {
                    token = strtok(NULL, " "); // Move to next token which should be the date
                    printf("Client requested to quit.\n");
                    if (token != NULL)
                    {
                        // Convert the PID string to an integer
                        pid_t pid = atoi(token);

                        // Check if the conversion was successful
                        if (pid > 0)
                        {
                            printf("Client requested to quit.\n");
                            // Kill the client process
                            if (kill(pid, SIGKILL) == 0)
                            {
                                printf("Client process with PID %d killed successfully.\n", pid);
                            }
                            else
                            {
                                perror("kill");
                                printf("Failed to kill client process with PID %d.\n", pid);
                            }
                        }
                        else
                        {
                            printf("Invalid PID provided: %s\n", token);
                        }
                    }
                    else
                    {
                        printf("No PID provided for quitting.\n");
                    }

                    exit(0);

                    // break;
                }
                else if (strcasecmp(token, "w24fda") == 0)
                {
                    token = strtok(NULL, " "); // Move to next token which should be the date
                    if (token == NULL)
                    {
                        sprintf(npsd_commd, "Date argument is missing. Please enter the date.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                    }
                    else
                    {
                        // Execute find command to get files created after or on specified date
                        char find_command[BUFFER_SIZE];
                        sprintf(find_command, "find ~ -type f -newermt '%s' -exec tar -cvf - {} + | gzip > temp.tar.gz", token);

                        FILE *fp = popen(find_command, "r");
                        if (fp == NULL)
                        {
                            perror("Error executing find command");
                            sprintf(npsd_commd, "Error executing find command.\n");
                            write(csd, npsd_commd, strlen(npsd_commd));
                        }
                        else
                        {
                            // Read the output of the find command
                            char find_output[BUFFER_SIZE];
                            while (fgets(find_output, sizeof(find_output), fp) != NULL)
                            {
                                // Do nothing, just read the output
                            }
                            pclose(fp);

                            // Send the tar.gz file back to the client
                            FILE *tar_fp = fopen("temp.tar.gz", "r");
                            if (tar_fp == NULL)
                            {
                                perror("Error opening temp.tar.gz file");
                                sprintf(npsd_commd, "Error creating archive or no files found.\n");
                                write(csd, npsd_commd, strlen(npsd_commd));
                            }
                            else
                            {
                                // Send file contents
                                while ((n = fread(message, 1, sizeof(message), tar_fp)) > 0)
                                {
                                    if (write(csd, message, n) != n)
                                    {
                                        perror("Error sending archive to client");
                                        break;
                                    }
                                }
                                fclose(tar_fp);
                            }
                        }
                    }
                }
                else if (strcasecmp(token, "w24fdb") == 0)
                {
                    token = strtok(NULL, " "); // Move to next token which should be the date
                    if (token == NULL)
                    {
                        sprintf(npsd_commd, "Date argument is missing. Please enter the date.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                    }
                    else
                    {
                        // Execute find command to get files created before or on specified date
                        char find_command[BUFFER_SIZE];
                        sprintf(find_command, "find ~ -type f ! -newermt '%s' -print0 | tar --null -T - -cvzf temp.tar.gz", token);

                        FILE *fp = popen(find_command, "r");
                        if (fp == NULL)
                        {
                            perror("Error executing find command");
                            sprintf(npsd_commd, "Error executing find command.\n");
                            write(csd, npsd_commd, strlen(npsd_commd));
                        }
                        else
                        {
                            // Read the output of the find command
                            char find_output[BUFFER_SIZE];
                            while (fgets(find_output, sizeof(find_output), fp) != NULL)
                            {
                                // Do nothing, just read the output
                            }
                            pclose(fp);

                            // Send the tar.gz file back to the client
                            FILE *tar_fp = fopen("temp.tar.gz", "r");
                            if (tar_fp == NULL)
                            {
                                perror("Error opening temp.tar.gz file");
                                sprintf(npsd_commd, "Error creating archive or no files found.\n");
                                write(csd, npsd_commd, strlen(npsd_commd));
                            }
                            else
                            {
                                // Send file contents
                                while ((n = fread(message, 1, sizeof(message), tar_fp)) > 0)
                                {
                                    if (write(csd, message, n) != n)
                                    {
                                        perror("Error sending archive to client");
                                        break;
                                    }
                                }
                                fclose(tar_fp);
                            }
                        }
                    }
                }
                else if (strcasecmp(token, "w24fz") == 0)
                {
                    // Extract size1 and size2 from the command
                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sprintf(npsd_commd, "Size1 argument is missing. Please enter the size1.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                        continue;
                    }
                    int size1 = atoi(token);

                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sprintf(npsd_commd, "Size2 argument is missing. Please enter the size2.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                        continue;
                    }
                    int size2 = atoi(token);

                    // Check if size1 <= size2
                    if (size1 > size2)
                    {
                        sprintf(npsd_commd, "Invalid size range. Size1 should be less than or equal to size2.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                        continue;
                    }

                    // Execute find command to get files within the specified size range
                    char find_command[BUFFER_SIZE];
                    sprintf(find_command, "find ~ -type f -size +%d -a -size -%d -exec tar -rvf temp.tar {} +", size1, size2);

                    FILE *fp = popen(find_command, "r");
                    if (fp == NULL)
                    {
                        perror("Error executing find command");
                        sprintf(npsd_commd, "Error executing find command.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                    }
                    else
                    {
                        // Read the output of the find command
                        char find_output[BUFFER_SIZE];
                        while (fgets(find_output, sizeof(find_output), fp) != NULL)
                        {
                            // Do nothing, just read the output
                        }
                        pclose(fp);

                        // Send the tar.gz file back to the client
                        FILE *tar_fp = fopen("temp.tar.gz", "r");
                        if (tar_fp == NULL)
                        {
                            perror("Error opening temp.tar.gz file");
                            sprintf(npsd_commd, "Error creating archive or no files found.\n");
                            write(csd, npsd_commd, strlen(npsd_commd));
                        }
                        else
                        {
                            // Send file contents
                            while ((n = fread(message, 1, sizeof(message), tar_fp)) > 0)
                            {
                                if (write(csd, message, n) != n)
                                {
                                    perror("Error sending archive to client");
                                    break;
                                }
                            }
                            fclose(tar_fp);
                        }
                    }
                }
                else if (strcasecmp(token, "w24ft") == 0)
                {
                    // Initialize an array to store the file extensions
                    char ext_list[3][BUFFER_SIZE];
                    int ext_count = 0;

                    // Extract file extensions from the command
                    while ((token = strtok(NULL, " ")) != NULL && ext_count < 3)
                    {
                        strcpy(ext_list[ext_count++], token);
                    }

                    // Check if at least one file extension is provided
                    if (ext_count == 0)
                    {
                        sprintf(npsd_commd, "File extension(s) missing. Please enter at least one file extension.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                        continue;
                    }

                    // Print the file extensions
                    printf("Extension list:\n");
                    for (int i = 0; i < ext_count; i++)
                    {
                        printf("%s\n", ext_list[i]);
                    }

                    // Execute find command to get files with specified extensions
                    char find_command[BUFFER_SIZE];

                    // Construct the find command to search for files with specified extensions
                    sprintf(find_command, "find ~ -type f \\( ");
                    for (int i = 0; i < ext_count; i++)
                    {
                        if (i > 0)
                        {
                            strcat(find_command, " -o ");
                        }
                        strcat(find_command, "-iname \"*.");
                        strcat(find_command, ext_list[i]);
                        strcat(find_command, "\"");
                    }
                    strcat(find_command, " \\) -exec tar -rvf temp.tar {} + && gzip temp.tar");

                    // Execute the find command and compress the tarball
                    FILE *fp = popen(find_command, "r");
                    if (fp == NULL)
                    {
                        perror("Error executing find command");
                        sprintf(npsd_commd, "Error executing find command.\n");
                        write(csd, npsd_commd, strlen(npsd_commd));
                    }
                    else
                    {
                        // Read the output of the find command
                        char find_output[BUFFER_SIZE];
                        while (fgets(find_output, sizeof(find_output), fp) != NULL)
                        {
                            // Do nothing, just read the output
                        }
                        pclose(fp);

                        // Send the tar.gz file back to the client
                        FILE *tar_fp = fopen("temp.tar.gz", "r");
                        if (tar_fp == NULL)
                        {
                            perror("Error opening temp.tar.gz file");
                            sprintf(npsd_commd, "Error creating archive or no files found.\n");
                            write(csd, npsd_commd, strlen(npsd_commd));
                        }
                        else
                        {
                            // Send file contents
                            while ((n = fread(message, 1, sizeof(message), tar_fp)) > 0)
                            {
                                if (write(csd, message, n) != n)
                                {
                                    perror("Error sending archive to client");
                                    break;
                                }
                            }
                            fclose(tar_fp);
                        }
                    }
                }
                if (token != NULL && strcmp(token, "w24fs") == 0)
                {
                    char *filename = message + 6;
                    struct passwd *pw = getpwuid(getuid());
                    const char *homedir = pw->pw_dir;
                    char response[620000] = "";

                    if (findFile(homedir, filename, response))
                    {
                        write(csd, response, strlen(response));
                    }
                    else
                    {
                        strcpy(response, "File not found\n");
                        write(csd, response, strlen(response));
                    }
                }

                else
                {
                    // Unknown command
                    printf("Unknown command: %s\n", token);
                    sprintf(npsd_commd, "Unknown command.\n");
                    write(csd, npsd_commd, strlen(npsd_commd));
                }
            }
        }
    }

    close(csd); // Close client socket
    exit(0);
}

int main() {
    int sockfd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MIRROR1_PORT);

    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(2);
    }

    listen(sockfd, 10);
    printf("Mirror is listening on port %d\n", MIRROR1_PORT);

    while (1) {
        new_socket = accept(sockfd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Connection established with main server\n");

        if (!fork()) { // Child process
            close(sockfd); // Close listening socket in child
            crequest(new_socket);
            exit(0);
        } else { // Parent process
            close(new_socket); // Close connected socket in parent
            while (waitpid(-1, NULL, WNOHANG) > 0); // Clean up zombie processes
        }
    }

    return 0;
}