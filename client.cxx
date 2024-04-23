// client.cxx
// TEST: simulate 10 clients to computation

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "types.h"
#include "utils.h"

#define SERVER0_IP "127.0.0.1"
#define SERVER1_IP "127.0.0.1"
#define SERVER2_IP "127.0.0.1"

#define BUFFER_SIZE 1024

uint32_t num_bits;

int sockfd0, sockfd1, sockfd2;

int send_to_server(const int server, const void* const buffer, const size_t n, const int flags = 0) {
	const int socket = (server == 0) ? sockfd0 : ((server == 1) ? sockfd1 : sockfd2);
//    const int socket = (server == 0 ? sockfd0 : sockfd1);
    int ret = send(socket, buffer, n, flags);
    if (ret < 0) error_exit("Failed to send to server ");
    return ret;
}

void bit_sum(const std::string protocol, const size_t numreqs) {
	std::cout << "test: use protocol:" << protocol << std::endl;
	std::cout << "test: numreqs:" << numreqs << std::endl;

}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cout << "argument: client_num OPERATION" <<std::endl;
		return 1;
	}

	const int numreqs = atoi(argv[1]);  // Number of simulated clients
    const int port0 = 8000;
    const int port1 = 8001;
    const int port2 = 8002;
	const std::string protocol(argv[2]);

	num_bits = 8;
	char buffer[BUFFER_SIZE];
	std::cout << "num_bits:" << num_bits << std::endl;

	// Set up server connections

    struct sockaddr_in server2, server1, server0;

    sockfd0 = socket(AF_INET, SOCK_STREAM, 0);
    sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
    sockfd2 = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd0 < 0 or sockfd1 < 0 or sockfd2 < 0) error_exit("Socket creation failed!");
	
	server2.sin_port = htons(port2);
	server1.sin_port = htons(port1);
    server0.sin_port = htons(port0);

    server0.sin_family = AF_INET;
    server1.sin_family = AF_INET;
    server2.sin_family = AF_INET;

    inet_pton(AF_INET, SERVER0_IP, &server0.sin_addr);
    inet_pton(AF_INET, SERVER1_IP, &server1.sin_addr);
    inet_pton(AF_INET, SERVER2_IP, &server2.sin_addr);

	std::cout << "Connecting to server 0" << std::endl;
    if (connect(sockfd0, (sockaddr*)&server0, sizeof(server0)) < 0)
        error_exit("Can't connect to server0");
    std::cout << "Connecting to server 1" << std::endl;
    if (connect(sockfd1, (sockaddr*)&server1, sizeof(server1)) < 0)
        error_exit("Can't connect to server1");
    std::cout << "Connecting to server 2" << std::endl;
    if (connect(sockfd2, (sockaddr*)&server2, sizeof(server2)) < 0)
        error_exit("Can't connect to server2");

	// test for send to server
/*	const char *message0 = "Helloserver#0";	
	ssize_t bytes_sent = send(sockfd0, message0, strlen(message0), 0);
    if (bytes_sent < 0)
        error_exit("Error sending data to server");
	const char *message1 = "Helloserver#1";	
	bytes_sent = send(sockfd1, message1, strlen(message1), 0);
    if (bytes_sent < 0)
        error_exit("Error sending data to server");
	const char *message2 = "Helloserver#2";	
	bytes_sent = send(sockfd2, message2, strlen(message2), 0);
    if (bytes_sent < 0)
	bytes_sent = send(sockfd2, buffer, strlen(buffer), 0);
    if (bytes_sent < 0)
        error_exit("Error sending data to server");
*/
	
	// TODO client initialize 
	std::cout << "Init constants: " << std::endl;

	std::cout << "testing initMsg,,,, " << std::endl;

    initMsg msg;
    msg.num_of_inputs = numreqs;
/*
    msg.type = BIT_SUM;
	send_to_server(0, &msg, sizeof(initMsg));
	send_to_server(1, &msg, sizeof(initMsg));
	send_to_server(2, &msg, sizeof(initMsg));
*/

	auto start = clock_start();
	if (protocol == "BITSUM") {
    	msg.type = BIT_SUM;

		// TODO
		bit_sum(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "INTSUM") {
    	msg.type = INT_SUM;

		// TODO  int_sum

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "ANDOP") {
    	msg.type = AND_OP;

		// TODO and 

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "OROP") {
    	msg.type = OR_OP;

		// TODO or 

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 

	else {
		std::cout << "Unrecognized protocol" << std::endl;
	}

	std::cout << "Total time:\t" << sec_from(start) << std::endl;

    close(sockfd0);
    close(sockfd1);
    close(sockfd2);
	std::cout << "socket closed" << std::endl;

	return 0;
}

