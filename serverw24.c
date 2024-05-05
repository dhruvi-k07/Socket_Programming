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
#define BUFSIZE 1024 // The size of buffer
#define SERVER_PORT 16500
int mirror1_port = 17500;
int mirror2_port = 18500;
int connection_count = 0;
char **dirArray = NULL; // Global pointer to array of directory paths
size_t dirCount = 0;    // Number of directories stored in the array
typedef struct DirrectNode
{
   char path[1024]; // Changed from 'name' to 'path' to store full paths
   time_t creation_time;
   struct DirrectNode *next;
} DirrectNode;
void crequest(int csd); // Function prototype for handling client requestsƒ
// Function to determine which server/mirror handles this
int get_target_port_final() {
   if (connection_count < 9) {
       // Initial 9 requests divided among the three (server, mirror1, mirror2)
       int index = (connection_count / 3);
       switch (index) {
           case 0: return SERVER_PORT; // First three requests handled by the server
           case 1: return mirror1_port; // Next three requests handled by mirror1
           case 2: return mirror2_port; // Last three requests handled by mirror2
       }
   } else {
       // After the first 9, cycle through server, mirror1, mirror2 sequentially
       int index = (connection_count - 9) % 3;
       switch (index) {
           case 0: return SERVER_PORT; // Server
           case 1: return mirror1_port; // Mirror1
           case 2: return mirror2_port; // Mirror2
       }
   }
   return 0; // Default to server if nothing else fits
}

// Comparator function for qsort, for case-sensitive sorting
int compare(const void *a, const void *b)
{
    const char *dirA = *(const char **)a;
    const char *dirB = *(const char **)b;
    return strcmp(dirA, dirB);
}

// Function to free the linked list
void freeList(DirrectNode *head)
{
   DirrectNode *temp;
   while (head != NULL)
   {
       temp = head;
       head = head->next;
       free(temp);
   }
}
// Function to forward requests to mirrors, handling each request in a separate process
int forwarding_request(int client_sd, const char *target_ip, int target_port) {
   if (fork() == 0) { // Child process
       int sock = socket(AF_INET, SOCK_STREAM, 0);
       if (sock < 0) {
           perror("Socket creation failed");
           exit(EXIT_FAILURE);
       }
       struct sockaddr_in mirror_addr;
       memset(&mirror_addr, 0, sizeof(mirror_addr));
       mirror_addr.sin_family = AF_INET;
       mirror_addr.sin_port = htons(target_port);
       if (inet_pton(AF_INET, target_ip, &mirror_addr.sin_addr) <= 0) {
           perror("Invalid address/ Address not supported");
           close(sock);
           exit(EXIT_FAILURE);
       }
       if (connect(sock, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr)) < 0) {
           perror("Connection to mirror failed");
           close(sock);
           exit(EXIT_FAILURE);
       }
       char buffer[BUFSIZE];
       int n = read(client_sd, buffer, BUFSIZE); // Read client's request
       if (n > 0) {
           send(sock, buffer, n, 0); // Forward request to mirror
           // Optionally, you can wait for the response from the mirror and send it back to the client
           n = read(sock, buffer, BUFSIZE); // Read response from mirror
           if (n > 0) {
               send(client_sd, buffer, n, 0); // Send response back to client
           }
       }
       close(sock);
       close(client_sd);
       exit(0); // Terminate the child process
   } else {
       // Parent process does not need to handle the client socket
       close(client_sd);
       return 1; // Return success from parent
   }
}
// Function to recursively search for a file in a directory and its subdirectories
// and return its details
int findingFile(const char *basePath, const char *searchFilename, char *fileDetails)
{
   DIR *dir; // Pointer to directory stream
   struct dirent *entry; // Pointer to directory entry
   struct stat fileStatus; // Structure to hold file status information
   char path[1024]; // Buffer to store path of files
   int found = 0; // Flag to indicate if file is found
   // Open the directory specified by basePath
   if (!(dir = opendir(basePath)))
       return 0; // Return 0 if directory cannot be opened
   // Loop through each entry in the directory
   while (!found && (entry = readdir(dir)) != NULL)
   {
       // If the entry is a directory
       if (entry->d_type == DT_DIR)
       {
           // Check if the directory entry is not "." or ".."
           if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
           {
               // Construct full path of the subdirectory
               snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);
               // Recursively call findingFile function for the subdirectory
               found = findingFile(path, searchFilename, fileDetails);
           }
       }
       // If the entry is a file and its name matches the searchFilename
       else if (strcmp(entry->d_name, searchFilename) == 0)
       {
           // Construct full path of the file
           snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);
           // Get file status
           if (stat(path, &fileStatus) == 0)
           {
               char perm[100]; // Buffer to store permissions
               // Get creation time of the file
               strftime(perm, sizeof(perm), "%A %b %d %Y", localtime(&fileStatus.st_ctime));
               // Populate fileDetails with file information
               snprintf(fileDetails, 1024, "Filename: %s\nSize: %ld bytes\nCreated: %s\nPermissions: %o\n",
                        path, fileStatus.st_size, perm, fileStatus.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
               found = 1; // Set found flag to indicate file is found
           }
       }
   }
   closedir(dir); // Close the directory stream
   return found; // Return 1 if file is found, 0 otherwise
}

// Function to add a directory path to the array
void addingDirectory(const char *path)
{
   // Allocate memory for a temporary array to hold directory paths
   char **temp = realloc(dirArray, (dirCount + 1) * sizeof(char *));
   // Check if memory allocation was successful
   if (temp == NULL)
   {
       // Print an error message if memory allocation failed
       fprintf(stderr, "Memory allocation failed\n");
       return; // Return from the function
   }
   dirArray = temp; // Update dirArray to point to the newly allocated memory

   // Duplicate the path string and store it in the dirArray
   dirArray[dirCount] = strdup(path);
   // Check if memory allocation for path duplication was successful
   if (dirArray[dirCount] == NULL)
   {
       // Print an error message if memory allocation failed for path duplication
       fprintf(stderr, "Memory allocation failed for path duplication\n");
       return; // Return from the function
   }

   dirCount++; // Increment the count of directories stored in the array
}

// Recursive function to collect directories
void collectDirectories(const char *dir_name)
{
   DIR *dir; // Pointer to directory stream
   struct dirent *entry; // Pointer to directory entry
   char subdir_path[1024]; // Buffer to store the path of subdirectories
   // Open the directory specified by dir_name
   if ((dir = opendir(dir_name)) == NULL)
   {
       // Print an error message if the directory cannot be opened
       fprintf(stderr, "Cannot open directory %s\n", dir_name);
       return; // Return from the function
   }
   // Loop through each entry in the directory
   while ((entry = readdir(dir)) != NULL)
   {
       // Ignore directories starting with '.' or '_'
       if (entry->d_type == DT_DIR && (entry->d_name[0] == '.' || entry->d_name[0] == '_'))
       {
           continue; // Skip this directory and continue to the next one
       }
       // If the entry is a directory and not '.' or '..'
       if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
       {
           // Construct the full path of the subdirectory
           snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_name, entry->d_name);
           // Add the directory path to the array
           addingDirectory(subdir_path);
           // Recursively call collectDirectories function to collect subdirectory names
           collectDirectories(subdir_path);
       }
   }
   closedir(dir); // Close the directory stream
}

// Function to insert a new node in sorted order based on creation time
void insertingSorted(DirrectNode **head, DirrectNode *newNode)
{
   DirrectNode *current; // Pointer to traverse the linked list
   // If the linked list is empty or the creation time of the new node is smaller than the creation time of the first node
   if (*head == NULL || (*head)->creation_time > newNode->creation_time)
   {
       // Insert the new node at the beginning of the linked list
       newNode->next = *head;
       *head = newNode;
   }
   else
   {
       current = *head;
       // Traverse the linked list until reaching a node whose next node has creation time greater than or equal to the new node's creation time
       while (current->next != NULL && current->next->creation_time < newNode->creation_time)
       {
           current = current->next;
       }
       // Insert the new node after the current node
       newNode->next = current->next;
       current->next = newNode;
   }
}
// Recursive function to collect directories by creation time, ignoring those starting with '.' and '_'
void collectDirectoriesByTime(const char *base_path, DirrectNode **head)
{
   DIR *dir; // Pointer to directory stream
   struct dirent *entry; // Pointer to directory entry
   struct stat fileStat; // Structure to hold file status information
   char path[1024]; // Buffer to store the path of directories
   // Open the directory specified by base_path
   if ((dir = opendir(base_path)) == NULL)
   {
       // Print an error message if the directory cannot be opened
       fprintf(stderr, "Cannot open directory %s\n", base_path);
       return; // Return from the function
   }
   // Loop through each entry in the directory
   while ((entry = readdir(dir)) != NULL)
   {
       // If the entry is a directory and does not start with '.' or '_'
       if (entry->d_type == DT_DIR && (entry->d_name[0] != '.' && entry->d_name[0] != '_'))
       {
           // Construct the full path of the directory
           snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
           
           // Get the status of the directory
           if (stat(path, &fileStat) < 0)
           {
               // Print an error message if there is an error getting the status of the directory
               fprintf(stderr, "Error getting status of %s: %s\n", path, strerror(errno));
               continue; // Skip this directory and continue to the next one
           }
           // Allocate memory for a new DirrectNode
           DirrectNode *newNode = malloc(sizeof(DirrectNode));
           if (newNode == NULL)
           {
               // Print an error message if memory allocation fails
               fprintf(stderr, "Memory allocation failed\n");
               closedir(dir); // Close the directory stream before returning
               return; // Return from the function
           }
           // Copy the path and set the creation time of the new node
           strcpy(newNode->path, path);
           newNode->creation_time = fileStat.st_ctime;
           newNode->next = NULL;
           
           // Insert the new node into the linked list in sorted order based on creation time
           insertingSorted(head, newNode);
           // Recursively call collectDirectoriesByTime function to add subdirectories
           collectDirectoriesByTime(path, head);
       }
   }
   closedir(dir); // Close the directory stream
}

void listOfDIrectoriesByTime(const char *dir_name, char *response)
{
   DirrectNode *head = NULL; // Initialize a pointer to the head of the linked list
   collectDirectoriesByTime(dir_name, &head); // Collect directories sorted by time and populate the linked list
   // Build the response from the sorted list
   DirrectNode *current = head; // Pointer to traverse the linked list
   while (current != NULL)
   {
       // Append the path of the current directory node to the response
       strcat(response, current->path);
       strcat(response, "\n"); // Add a newline character
       current = current->next; // Move to the next node
   }
   // Free the linked list to prevent memory leaks
   freeList(head); // Function to free memory allocated for the linked list
}

void listOfDIrectories(const char *dir_name, char *response)
{
   // Initialize or clear previous data
   free(dirArray); // Free previously allocated memory for dirArray to prevent memory leaks
   dirArray = NULL; // Reset dirArray pointer to NULL
   dirCount = 0; // Reset the directory count
   // Collect all directories
   collectDirectories(dir_name); // Function to collect directories recursively
   // Sort the collected directory paths
   if (dirArray != NULL)
   {
       qsort(dirArray, dirCount, sizeof(char *), compare); // Sort the directory paths alphabetically
   }
   // Build the response from sorted array
   for (size_t i = 0; i < dirCount; i++)
   {
       strcat(response, dirArray[i]); // Append each directory path to the response string
       strcat(response, "\n"); // Add a newline character
       free(dirArray[i]); // Free the memory allocated for the duplicated path string
   }
   // Free the directory array
   free(dirArray); // Free the memory allocated for the directory array
   dirArray = NULL; // Reset dirArray pointer to NULL
   dirCount = 0; // Reset the directory count
}

int main() {
   int sd, csd;
   struct sockaddr_in servAdd;
   int opt = 1;
   if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       perror("Cannot create socket");
       exit(1);
   }
   setsockopt(sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
   servAdd.sin_family = AF_INET;
   servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
   servAdd.sin_port = htons((uint16_t) SERVER_PORT);
   if (bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0) {
       perror("Bind failed");
       exit(1);
   }
   listen(sd, 10);
   printf("Server is listening on port %d\n", SERVER_PORT);
   while (1) {
       csd = accept(sd, (struct sockaddr *)NULL, NULL);
       if (csd < 0) {
           perror("Accept failed");
           continue;
       }
       int target_port = get_target_port_final();
       printf("Client connected. Forwarding to port %d\n", target_port);
       if (target_port == SERVER_PORT) {
           if (!fork()) {
               close(sd); // Close server socket in the child process
               crequest(csd);
               exit(0);
           }
       } else {
           forwarding_request(csd, "127.0.0.1", target_port);
       }
       close(csd); // Close client socket
       connection_count++;
   }
   return 0;
}
// for directory operations
void crequest(int csd)
{
   char message[255];
   int n;
   // write(csd, "Welcome to the server!\n", 23); // Send welcome message to client
   while (1)
   {
       char command_npsd[1024] = {0};           // Command buffer
       if ((n = read(csd, message, 255)) > 0) // Read client command
       {
           message[n] = '\0'; // Null terminate message
           printf("Received command from client: %s\n", message);
           if (strcasecmp(message, "dirlist -a") == 0) // Check if client wants directory listing
           {
               char *base_dir = "/home/kunvara";
               char response[1024] = ""; // Buffer to store directory listing
               // Recursively traverse directories
               listOfDIrectories(base_dir, response);
               // Send directory listing back to the client
               write(csd, response, strlen(response));
           }
           else if (strcasecmp(message, "dirlist -t") == 0) // Check if client wants directory listing by creation time
           {
               char *base_dir = "/home/kunvara";
               char response[620000] = ""; // Buffer to store directory listing
               // List directories in the order of creation time
               listOfDIrectoriesByTime(base_dir, response);
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
                   if (token == NULL || strlen(token) != 10 || token[4] != '-' || token[7] != '-')
                   {
                       sprintf(command_npsd, "Incorrect date format. Please enter the date in yyyy-mm-dd format.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                   }
                   else
                   {
                       // Execute find command to get files created after or on specified date
                       char find_command[BUFSIZE];
                       sprintf(find_command, "find ~ -type f -newermt '%s' -exec tar -cvf - {} + | gzip > temp.tar.gz", token);
                       FILE *fp = popen(find_command, "r");
                       if (fp == NULL)
                       {
                           perror("Error executing find command");
                           sprintf(command_npsd, "Error executing find command.\n");
                           write(csd, command_npsd, strlen(command_npsd));
                       }
                       else
                       {
                           // Read the output of the find command
                           char find_output[BUFSIZE];
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
                               sprintf(command_npsd, "Error creating archive or no files found.\n");
                               write(csd, command_npsd, strlen(command_npsd));
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
                   if (token == NULL || strlen(token) != 10 || token[4] != '-' || token[7] != '-')
                   {
                       sprintf(command_npsd, "Incorrect date format. Please enter the date in yyyy-mm-dd format.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                   }
                   else
                   {
                       // Execute find command to get files created before or on specified date
                       char find_command[BUFSIZE];
                       sprintf(find_command, "find ~ -type f ! -newermt '%s' -print0 | tar --null -T - -cvzf temp.tar.gz", token);
                       FILE *fp = popen(find_command, "r");
                       if (fp == NULL)
                       {
                           perror("Error executing find command");
                           sprintf(command_npsd, "Error executing find command.\n");
                           write(csd, command_npsd, strlen(command_npsd));
                       }
                       else
                       {
                           // Read the output of the find command
                           char find_output[BUFSIZE];
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
                               sprintf(command_npsd, "Error creating archive or no files found.\n");
                               write(csd, command_npsd, strlen(command_npsd));
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
                       sprintf(command_npsd, "Size1 argument is missing. Please enter the size1.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                       continue;
                   }
                   int size1 = atoi(token);
                   token = strtok(NULL, " ");
                   if (token == NULL)
                   {
                       sprintf(command_npsd, "Size2 argument is missing. Please enter the size2.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                       continue;
                   }
                   int size2 = atoi(token);
                   // Check if size1 <= size2
                   if (size1 > size2)
                   {
                       sprintf(command_npsd, "Invalid size range. Size1 should be less than or equal to size2.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                       continue;
                   }
                   // Execute find command to get files within the specified size range
                   char find_command[BUFSIZE];
                   sprintf(find_command, "find ~ -type f -size +%d -a -size -%d -exec tar -rvf temp.tar {} +", size1, size2);
                   FILE *fp = popen(find_command, "r");
                   if (fp == NULL)
                   {
                       perror("Error executing find command");
                       sprintf(command_npsd, "Error executing find command.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                   }
                   else
                   {
                       // Read the output of the find command
                       char find_output[BUFSIZE];
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
                           sprintf(command_npsd, "Error creating archive or no files found.\n");
                           write(csd, command_npsd, strlen(command_npsd));
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
                   char ext_list[3][BUFSIZE];
                   int ext_count = 0;
                   // Extract file extensions from the command
                   while ((token = strtok(NULL, " ")) != NULL && ext_count < 3)
                   {
                       strcpy(ext_list[ext_count++], token);
                   }
                   // Check if at least one file extension is provided
                   if (ext_count == 0)
                   {
                       sprintf(command_npsd, "File extension(s) missing. Please enter at least one file extension.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                       continue;
                   }
                   // Print the file extensions
                   printf("Extension list:\n");
                   for (int i = 0; i < ext_count; i++)
                   {
                       printf("%s\n", ext_list[i]);
                   }
                   // Execute find command to get files with specified extensions
                   char find_command[BUFSIZE];
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
                       sprintf(command_npsd, "Error executing find command.\n");
                       write(csd, command_npsd, strlen(command_npsd));
                   }
                   else
                   {
                       // Read the output of the find command
                       char find_output[BUFSIZE];
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
                           sprintf(command_npsd, "Error creating archive or no files found.\n");
                           write(csd, command_npsd, strlen(command_npsd));
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
                   if (findingFile(homedir, filename, response))
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
                   sprintf(command_npsd, "Unknown command.\n");
                   write(csd, command_npsd, strlen(command_npsd));
               }
           }
       }
   }
   close(csd); // Close client socket
   exit(0);
}
