#include <arpa/inet.h>  // socket(), bind(), accept(), inet_ntop(), listen()
#include <unistd.h>     // close(), read()
#include <semaphore.h>  // sem_t
#include <pthread.h>    // pthread
#include <vector>
#include <string>

#define CLIENT_MESSAGE_SIZE 1024
#define PORT 8080

// Semaphore for synchronizing access to shared resources
sem_t mutex;
int thread_count = 0;

// Thread handler function to process client connections
void *connection_handler(void *socket_desc) {
    int newSock = *((int *)socket_desc);
    char client_message[CLIENT_MESSAGE_SIZE];
    int request = read(newSock, client_message, CLIENT_MESSAGE_SIZE);

    // Update and print the thread counter
    sem_wait(&mutex);
    thread_count++;
    printf("thread counter %d\n", thread_count);
    if (thread_count > 20) {
        const char* bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(newSock, bad_request, strlen(bad_request), 0);
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
        // Echo the received message back to the client
        send(newSock, client_message, request, 0);
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
