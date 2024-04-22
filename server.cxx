// server.cxx
// 三个server互相连接，0监听12 1监听2

//#include "server.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <string>

#include "utils.h"

#define SERVER0_IP "127.0.0.1"
#define SERVER1_IP "127.0.0.1"
#define SERVER2_IP "127.0.0.1"

#define BUFFER_SIZE 1024


uint32_t num_bits;

void bind_and_listen(sockaddr_in& addr, int& sockfd, const int port, const int reuse = 1) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) error_exit("Socket creation failed");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
        error_exit("Sockopt failed");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)))
        error_exit("Sockopt failed");

    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port: " << port << std::endl;
        error_exit("Bind to port failed");
    }

    if (listen(sockfd, 2) < 0)
        error_exit("Listen failed");
}

// Symmetric: 0,1,2 connect to each other.
void start_server(int& sockfd, int& newsockfd, const int port, const int reuse = 0) {
    sockaddr_in addr;
    bind_and_listen(addr, sockfd, port, reuse);

    socklen_t addrlen = sizeof(addr);
    std::cout << "  Waiting to accept\n";
	
    newsockfd = accept(sockfd, (sockaddr*)&addr, &addrlen);
    if (newsockfd < 0) error_exit("Accept failure");
    std::cout << "  Accepted\n";
}

void server_connect(int& sockfd, const int port, const int reuse = 0) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error_exit("Socket creation failed");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
        error_exit("Sockopt failed");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)))
        error_exit("Sockopt failed");

    sockaddr_in addr;
    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, SERVER0_IP, &addr.sin_addr);

    std::cout << "  Trying to connect...\n";
    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
        error_exit("Can't connect to other server");
    std::cout << "  Connected\n";
}

int main(int argc, char** argv) {

	if (argc < 2) {
		std::cout << "argument: server_num(0/1/2) " << std::endl;
		return 1;
	}

	const int server_num = atoi(argv[1]);

	const int server0_1port = 5000; 
	const int server0_2port = 5001; 
	const int server1_2port = 5002; 
	
	const int client_port = 8000 + server_num;	//port of this server,for client
		
//	const int client_port = atoi(argv[2]);	//port of this server,for client

	std::cout << "This server is server #" << server_num << std::endl;
	std::cout << " Listening for client on " << client_port << std::endl;
//	std::cout << " Listening for server on" << server_port << std::endl;

	num_bits = 8;
	char buffer[BUFFER_SIZE];

    // Server 0 listens 1,2
    // Server 1 connects to 0, server 1 listens to 2
	// Server 2 connects to 0,1
    int sockfd_server, newsockfd_server, serverfd = 0;
    int sockfd_server0, newsockfd_server0, serverfd0 = 0;
    if (server_num == 0) {
        start_server(sockfd_server0, newsockfd_server0, server0_1port, 1);
		serverfd0 = newsockfd_server0;
        start_server(sockfd_server, newsockfd_server, server0_2port, 1);
        serverfd = newsockfd_server;
    } else if (server_num == 1) {
        server_connect(sockfd_server0, server0_1port, 1);
        serverfd0 = sockfd_server0;
        start_server(sockfd_server, newsockfd_server, server1_2port, 1);
        serverfd = newsockfd_server;
    } else if (server_num == 2) {
        server_connect(sockfd_server0, server0_2port, 1);
        server_connect(sockfd_server, server1_2port, 1);
        serverfd0 = sockfd_server0;
        serverfd = sockfd_server;
    } else {
        error_exit("Can only handle servers #0 and #1 and #2");
    }

// test for server communication
/*
	if (server_num == 0) {

		// 要发送的消息
	    const char *message = "Hello, server!";

   		// 发送数据to server#1
    	ssize_t bytes_sent = send(serverfd0, message, strlen(message), 0);
    	if (bytes_sent < 0)
        	error_exit("ERROR sending message");

   		// 发送数据to server#2
    	bytes_sent = send(serverfd, message, strlen(message), 0);
    	if (bytes_sent < 0)
        	error_exit("ERROR sending message");

    	std::cout << "Sent " << bytes_sent << " bytes: " << message << std::endl;

	} else if (server_num == 1) {

		// 接收数据from server#0
    	ssize_t bytes_received = recv(serverfd0, buffer, BUFFER_SIZE, 0);
    	if (bytes_received < 0)
        	error_exit("ERROR receiving message");
    	else if (bytes_received == 0)
        	std::cout << "Connection closed by remote peer." << std::endl;
    	else
        	std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;

		// 要发送的消息
	    const char *message = "I'm server #1";

   		// 发送数据to server#2
    	ssize_t bytes_sent = send(serverfd, message, strlen(message), 0);
    	if (bytes_sent < 0)
        	error_exit("ERROR sending message");

    	std::cout << "Sent " << bytes_sent << " bytes: " << message << std::endl;

	} else if (server_num == 2) {

		// 接收数据from server#0         
    	ssize_t bytes_received = recv(serverfd0, buffer, BUFFER_SIZE, 0);
    	if (bytes_received < 0)
        	error_exit("ERROR receiving message");
    	else if (bytes_received == 0)
        	std::cout << "Connection closed by remote peer." << std::endl;
    	else
        	std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;
		
		// 清空缓存区
		memset(buffer, 0, BUFFER_SIZE);

		// 接收数据from server#1         
    	bytes_received = recv(serverfd, buffer, BUFFER_SIZE, 0);
    	if (bytes_received < 0)
        	error_exit("ERROR receiving message");
    	else if (bytes_received == 0)
        	std::cout << "Connection closed by remote peer." << std::endl;
    	else
        	std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;
	}
*/

	//TODO: precomputation ..

	int sockfd, newsockfd;
    sockaddr_in addr;

    bind_and_listen(addr, sockfd, client_port, 1);

	while (1) {
		socklen_t addrlen = sizeof(addr);

		std::cout << "waiting for connection..." << std::endl;

		newsockfd = accept(sockfd, (struct sockaddr*)&addr, &addrlen);
        if (newsockfd < 0) error_exit("Connection creation failure");
	
		// test for connect client
/*		int bytes_received = recv(newsockfd, buffer, BUFFER_SIZE, 0);
    	if (bytes_received < 0) error_exit("Error receiving data from client");
    	std::cout << "Received message from client: " << std::string(buffer, bytes_received) << std::endl;
*/

		break;
	}


	close(sockfd_server);
	close(newsockfd_server);
	close(sockfd_server0);
	close(newsockfd_server0);
	std::cout << "socket closed" << std::endl;
	
	return 0;
}

