#include <arpa/inet.h>  // socket(), bind(), accept(), inet_ntop(), listen()
#include <unistd.h>     // close(), read()
#include <semaphore.h>  // sem_t
#include <pthread.h>    // pthread
#include <sys/sendfile.h> // sendfile
#include <fcntl.h>      // O_CREAT, O_WRONLY, O_RDONLY
#include <vector>
#include <sys/stat.h>
#include "server.h"     // Include your server-specific headers here

#define CLIENT_MESSAGE_SIZE 1024
#define PORT 8080

// Semaphore for synchronizing access to shared resources
sem_t mutex;
int thread_count = 0;

// Vector to store server data
std::vector<std::string> serverData;

// Function to extract a substring until a specified character
std::string getStr(std::string sql, char end) {
    int counter = 0;
    std::string retStr = "";
    while (sql[counter] != '\0') {
        if (sql[counter] == end) {
            break;
        }
        retStr += sql[counter];
        counter++;
    }
    return retStr;
}

// Function to send HTTP headers and files over a socket
void send_message(int fd, std::string filePath, std::string headerFile) {
    // Prepare the HTTP header
    std::string header = Messages[HTTP_HEADER] + headerFile;
    filePath = "./public" + filePath;
    struct stat stat_buf; // Hold information about the file to send

    // Send the HTTP header
    if (send(fd, header.c_str(), header.length(), 0) == -1) {
        perror("send failed");
        return;
    }

    // Open the file to be sent
    int fdimg = open(filePath.c_str(), O_RDONLY);
    if (fdimg < 0) {
        printf("cannot open file path: %s\n", filePath.c_str());
        return;
    }

    // Get file statistics
    fstat(fdimg, &stat_buf);
    int img_total_size = stat_buf.st_size;
    int block_size = stat_buf.st_blksize;

    // Send the file in blocks
    if (fdimg >= 0) {
        while (img_total_size > 0) {
            int send_bytes = (img_total_size < block_size) ? img_total_size : block_size;
            int done_bytes = sendfile(fd, fdimg, NULL, send_bytes);
            if (done_bytes == -1) {
                perror("sendfile failed");
                close(fdimg);
                return;
            }
            img_total_size -= done_bytes;
        }
        printf("sent file: %s\n", filePath.c_str());
        close(fdimg);
    }
}

// Function to find the file extension and return the corresponding content type
std::string findFileExt(std::string fileEx) {
    for (int i = 0; i <= sizeof(fileExtension); i++) {
        if (fileExtension[i] == fileEx) {
            return ContentType[i];
        }
    }
    printf("serving .%s as html\n", fileEx.c_str());
    return "Content-Type: text/html\r\n\r\n";
}

// Function to extract data from the client's request
void getData(std::string requestType, std::string client_message) {
    std::string extract;
    std::string data = client_message;

    if (requestType == "GET") {
        data.erase(0, getStr(data, ' ').length() + 1);
        data = getStr(data, ' ');
        data.erase(0, getStr(data, '?').length() + 1);
    } else if (requestType == "POST") {
        int counter = data.length() - 1;
        while (counter > 0) {
            if (data[counter] == ' ' || data[counter] == '\n') {
                break;
            }
            counter--;
        }
        data.erase(0, counter + 1);
        int found = data.find("=");
        if (found == std::string::npos) {
            data = "";
        }
    }

    int found = client_message.find("cookie");
    if (found != std::string::npos) {
        client_message.erase(0, found + 8);
        client_message = getStr(client_message, ' ');
        data = data + "&" + getStr(client_message, '\n');
    }
    while (data.length() > 0) {
        extract = getStr(data, '&');
        serverData.push_back(extract);
        data.erase(0, getStr(data, '&').length() + 1);
    }
}

// Thread handler function to process client connections
void *connection_handler(void *socket_desc) {
    int newSock = *((int *)socket_desc);
    char client_message[CLIENT_MESSAGE_SIZE];
    int request = read(newSock, client_message, CLIENT_MESSAGE_SIZE);
    std::string message = client_message;
    
    // Update and print the thread counter
    sem_wait(&mutex);
    thread_count++;
    printf("thread counter %d\n", thread_count);
    if (thread_count > 20) {
        send(newSock, Messages[BAD_REQUEST].c_str(), Messages[BAD_REQUEST].length(), 0);
        thread_count--;
        close(newSock);
        sem_post(&mutex);
        pthread_exit(NULL);
    }
    sem_post(&mutex);

    if (request < 0) {
        puts("Receive failed");
    } else if (request == 0) {
        puts("Client disconnected unexpectedly");
    } else {
        // Process the client's request
        std::string requestType = getStr(message, ' ');
        message.erase(0, requestType.length() + 1);
        std::string requestFile = getStr(message, ' ');

        std::string requestF = requestFile;
        std::string fileExt = requestF.erase(0, getStr(requestF, '.').length() + 1);
        std::string fileEx = getStr(getStr(fileExt, '/'), '?');
        requestFile = getStr(requestFile, '.') + "." + fileEx;

        if (requestType == "GET" || requestType == "POST") {
            if (requestFile.length() <= 1) {
                requestFile = "/index.html";
            }
            if (fileEx == "php") {
                // Do nothing for PHP files (not handled in this server)
                getData(requestType, client_message);
            } 
            sem_wait(&mutex);
            send_message(newSock, requestFile, findFileExt(fileEx));
            sem_post(&mutex);           
        }
    }
    printf("\n -----exiting server--------");
    close(newSock);
    sem_wait(&mutex);
    thread_count--;
    sem_post(&mutex);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    sem_init(&mutex, 0, 1);
    int server_socket, client_socket, *thread_sock;
    int randomPORT = PORT;
    struct sockaddr_in server_address, client_address;
    char ip4[INET_ADDRSTRLEN];

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket <= 0) {
        perror("error in socket:");
        exit(EXIT_FAILURE);
    }

    // Bind the server socket to a random port
    randomPORT = 8080 + (rand() % 10);
    memset(&server_address, 0, sizeof server_address); 
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(randomPORT);

    while (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        randomPORT = 8080 + (rand() % 10);
        server_address.sin_port = htons(randomPORT);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("error in listen:");
        exit(EXIT_FAILURE);
    }

    while (1) {
        socklen_t len = sizeof(client_address);
        printf("listening port: %d \n", randomPORT);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &len);
        if (client_socket < 0) {
            perror("unable to accept connection:");
            return 0;
        } else {
            inet_ntop(AF_INET, &(client_address.sin_addr), ip4, INET_ADDRSTRLEN);
            printf("connected to %s \n", ip4);
        }

        // Create a new thread for each client connection
        pthread_t multi_thread;
        thread_sock = new int();
        *thread_sock = client_socket;

        if (pthread_create(&multi_thread, NULL, connection_handler, (void*)thread_sock) > 0) {
            perror("could not create thread");
            return 0;
        }
    }
}
