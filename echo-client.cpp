#include <arpa/inet.h>  // socket(), connect(), inet_pton()
#include <unistd.h>     // close(), read(), write()
#include <cstring>      // strlen()
#include <iostream>

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];

    // Create the client socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(client_socket);
        return 1;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(client_socket);
        return 1;
    }

    // Send a message to the server
    std::string message = "Hello, Echo Server!";
    send(client_socket, message.c_str(), message.length(), 0);

    // Receive the echoed message from the server
    int bytes_received = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("Read failed");
    } else {
        std::cout << "Echo from server: " << std::string(buffer, bytes_received) << std::endl;
    }

    // Close the socket
    close(client_socket);

    return 0;
}
