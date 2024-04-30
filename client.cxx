// client.cxx
// TEST: simulate 10 clients to computation

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <bitset>

#include "types.h"
#include "utils.h"

#define SERVER0_IP "127.0.0.1"
#define SERVER1_IP "127.0.0.1"
#define SERVER2_IP "127.0.0.1"

#define BUFFER_SIZE 1024

uint32_t num_bits;
uint64_t max_int;

int sockfd0, sockfd1, sockfd2;

std::string pub_key_to_hex(const uint64_t* const key) {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(16) << std::hex << key[0];
    ss << std::setfill('0') << std::setw(16) << std::hex << key[1];
    return ss.str();
}

std::string make_pk(emp::PRG prg) {
    emp::block b;
    prg.random_block(&b, 1);
    return pub_key_to_hex((uint64_t*)&b);
}

int send_to_server(const int server, const void* const buffer, const size_t n, const int flags = 0) {
	const int socket = (server == 0) ? sockfd0 : ((server == 1) ? sockfd1 : sockfd2);
    int ret = send(socket, buffer, n, flags);
    if (ret < 0) error_exit("Failed to send to server ");
    return ret;
}

int bit_sum_helper(const std::string protocol, const size_t numreqs, unsigned int &ans, const initMsg* const msg_ptr = nullptr) {
    auto start = clock_start();
    int num_bytes = 0;

    bool real_val, share0, share1, share2;

    // Can't use a fixed key, or serial will have the same key every time
    emp::PRG prg;

    BitShare* const bitshare0 = new BitShare[numreqs];
    BitShare* const bitshare1 = new BitShare[numreqs];
    BitShare* const bitshare2 = new BitShare[numreqs];
    for (unsigned int i = 0; i < numreqs; i++) {
        prg.random_bool(&real_val, 1);
        prg.random_bool(&share0, 1);
        prg.random_bool(&share1, 1);
        share2 = share1 ^ share0 ^ real_val;
        ans += real_val;

        const std::string pk_s = make_pk(prg);
        const char* const pk = pk_s.c_str();

        memcpy(bitshare0[i].pk, &pk[0], PK_LENGTH);
        bitshare0[i].val = share0;

        memcpy(bitshare1[i].pk, &pk[0], PK_LENGTH);
        bitshare1[i].val = share1;

        memcpy(bitshare2[i].pk, &pk[0], PK_LENGTH);
        bitshare2[i].val = share2;

       // std::cout << pk << ": " << real_val << " = " << bitshare0[i].val << " ^ " << bitshare1[i].val << " ^ " << bitshare2[i].val << std::endl;
        std::cout << "client" << i << ": " << real_val << " = " << bitshare0[i].val << " ^ " << bitshare1[i].val << " ^ " << bitshare2[i].val << std::endl;
    }

    if (numreqs > 1)
        std::cout << "batch make:\t" << sec_from(start) << std::endl;

    start = clock_start();
    if (msg_ptr != nullptr) {
        num_bytes += send_to_server(0, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(1, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(2, msg_ptr, sizeof(initMsg));
	}
    for (unsigned int i = 0; i < numreqs; i++) {
        num_bytes += send_to_server(0, &bitshare0[i], sizeof(BitShare));
        num_bytes += send_to_server(1, &bitshare1[i], sizeof(BitShare));
        num_bytes += send_to_server(2, &bitshare2[i], sizeof(BitShare));
    }

    delete[] bitshare0;
    delete[] bitshare1;
    delete[] bitshare2;

    if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void bit_sum(const std::string protocol, const size_t numreqs) {
	unsigned int ans = 0;
	int num_bytes = 0;
	initMsg msg;
	msg.num_of_inputs = numreqs;
	msg.type = BIT_SUM;

	num_bytes += bit_sum_helper(protocol, numreqs, ans, &msg);

	std::cout << "Ans: " << ans << std::endl;
	std::cout << "Total sent bytes: " << num_bytes << std::endl;
}

// 将 uint64_t 存储的数据的后n位转换为二进制字符串
std::string uint64LastNToBinaryString(uint64_t num, int n) {
    // 获取后n位数据
    uint64_t lastN = num & ((1ULL << n) - 1);

    // 将后n位数据转换为二进制字符串
    return std::bitset<64>(lastN).to_string().substr(64 - n);
}

// 将二进制字符串分成三部分
void splitBinaryString(const std::string& binaryString, std::string& part1, std::string& part2, std::string& part3) {
    int length = binaryString.length();
    int partLength = length / 3;

    part1 = binaryString.substr(0, partLength);
    part2 = binaryString.substr(partLength, partLength);
    part3 = binaryString.substr(2 * partLength, length - 2 * partLength);
}

int int_sum_helper(const std::string protocol, const size_t numreqs, unsigned int &ans, const initMsg* const msg_ptr = nullptr) {
    auto start = clock_start();
    int num_bytes = 0;

    uint64_t real_val; 
//	share0, share1, share2;
	std::string shar0, shar1, shar2;

    emp::PRG prg;

	IntSumShare* const intshare0 = new IntSumShare[numreqs];
	IntSumShare* const intshare1 = new IntSumShare[numreqs];
	IntSumShare* const intshare2 = new IntSumShare[numreqs];

	for (unsigned int i = 0; i < numreqs; i++) {
		prg.random_data(&real_val, sizeof(uint64_t));
        real_val = real_val % max_int;
		std::string binaryString = uint64LastNToBinaryString(real_val, num_bits); // 获取后num_bits位并转换为二进制字符串
        ans += real_val;
		splitBinaryString(binaryString, shar0, shar1, shar2);

		const std::string pk_s = make_pk(prg);
		const char* const pk = pk_s.c_str();
		const char* const share0 = shar0.c_str();
		const char* const share1 = shar1.c_str();
		const char* const share2 = shar2.c_str();

		memcpy(intshare0[i].pk, &pk[0], PK_LENGTH);
		memcpy(intshare0[i].val, &share0[0], num_bits);

		memcpy(intshare1[i].pk, &pk[0], PK_LENGTH);
		memcpy(intshare1[i].val, &share1[0], num_bits);

		memcpy(intshare2[i].pk, &pk[0], PK_LENGTH);
		memcpy(intshare2[i].val, &share2[0], num_bits);

        std::cout << "client" << i << ": " << real_val << " = " << intshare0[i].val << " | " << intshare1[i].val << " | " << intshare2[i].val << std::endl;
	}

    if (numreqs > 1)
        std::cout << "batch make:\t" << sec_from(start) << std::endl;

    start = clock_start();
    if (msg_ptr != nullptr) {
        num_bytes += send_to_server(0, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(1, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(2, msg_ptr, sizeof(initMsg));
	}
    for (unsigned int i = 0; i < numreqs; i++) {
        num_bytes += send_to_server(0, &intshare0[i], sizeof(IntSumShare));
        num_bytes += send_to_server(1, &intshare1[i], sizeof(IntSumShare));
        num_bytes += send_to_server(2, &intshare2[i], sizeof(IntSumShare));
    }

    delete[] intshare0;
    delete[] intshare1;
    delete[] intshare2;

    if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void int_sum(const std::string protocol, const size_t numreqs) {
	unsigned int ans = 0;
	int num_bytes = 0;
	initMsg msg;
	msg.num_of_inputs = numreqs;
	msg.type = INT_SUM;

	num_bytes += int_sum_helper(protocol, numreqs, ans, &msg);

	std::cout << "Ans: " << ans << std::endl;
	std::cout << "Total sent bytes: " << num_bytes << std::endl;
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
	std::cout << "num bits:" << num_bits << std::endl;
	max_int = 1ULL << num_bits;
	std::cout << "max int: " << max_int << std::endl;

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
/*	
	const char *message0 = "Helloserver#0";	
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
        error_exit("Error sending data to server");
*/
	
	// TODO client initialize 
	std::cout << "Init constants: " << std::endl;


    initMsg msg;
    msg.num_of_inputs = numreqs;

	auto start = clock_start();
	if (protocol == "BITSUM") {
		std::cout << "Uploading all BITSUM shares: " << numreqs << std::endl;

		// bit_sum
		bit_sum(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "INTSUM") {
		std::cout << "Uploading all INTSUM shares: " << numreqs << std::endl;

		// int_sum
		int_sum(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "ANDOP") {
		std::cout << "Uploading all ANDOP shares: " << numreqs << std::endl;

		// todo and 

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "OROP") {
		std::cout << "Uploading all OROP shares: " << numreqs << std::endl;

		// todo or 

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 

	else {
		std::cout << "Unrecognized protocol" << std::endl;
	}

    close(sockfd0);
    close(sockfd1);
    close(sockfd2);
	std::cout << "socket closed" << std::endl;

	return 0;
}

