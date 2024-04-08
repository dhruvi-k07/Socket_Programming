# Winter 2024 Client-Server File Request Project

## Overview
This project for COMP-8567, Winter 2024, demonstrates a client-server architecture where clients request files from a server. It showcases socket communication, supporting multiple clients from different machines. The system includes a main server (`serverw24`), two mirrors (`mirror1` and `mirror2`), and client applications.

## Key Features
- **Immediate Feedback**: Rapid response to client requests, highlighting the system's efficiency.
- **File Operations**: Supports commands for directory listings, file details, and file retrieval based on size, type, or creation date.
- **Error Handling**: Client-side command syntax verification.
- **Load Distribution**: Alternating connection handling between the server and mirrors for balanced load management.

## Commands
- `dirlist`: List directories alphabetically or by creation time.
- `w24fn`: Fetch details of a specific file.
- `w24fz`: Retrieve files within a specified size range.
- `w24ft`: Download files of specified types.
- `w24fdb`/`w24fda`: Fetch files created before or after a specified date.
- `quitc`: Terminate the client connection.

## Setup
1. Prepare separate environments for `serverw24`, `mirror1`, `mirror2`, and the client.
2. Compile: `gcc serverw24.c -o serverw24`, `gcc clientw24.c -o clientw24`, `gcc mirror1.c -o mirror1`, `gcc mirror2.c -o mirror2`.
3. Run servers and mirrors before initiating client connections.
4. Connect using the client application and utilize the available commands for file interaction.

## Contribution
This project represents collaborative work under COMP-8567, showcasing practical application of network programming principles.

## Acknowledgments
Thanks to the course instructors and all project contributors for their support and hard work.

