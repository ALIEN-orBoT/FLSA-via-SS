// server.cxx
// 三个server互相连接，0监听12 1监听2

#include "server.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <string>

#include "net_share.h"
#include "types.h"
#include "utils.h"

#define SERVER0_IP "127.0.0.1"
#define SERVER1_IP "127.0.0.1"
#define SERVER2_IP "127.0.0.1"

#define BUFFER_SIZE 1024

#define INVALID_THRESHOLD 0.5

uint64_t int_sum_max;
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

// Server#0,1,2 connect to each other.
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

std::string get_pk(const int serverfd) {
    char pk_buf[PK_LENGTH];
    recv_in(serverfd, &pk_buf[0], PK_LENGTH);
    std::string pk(pk_buf, pk_buf + PK_LENGTH);
    return pk;
}

returnType bit_sum(const initMsg msg, const int clientfd, const int serverfd0, const int serverfd, const int server_num, uint64_t& ans){

	std::unordered_map<std::string, bool> share_map;
	auto start = clock_start();

    BitShare share;
    const unsigned int total_inputs = msg.num_of_inputs;

    int num_bytes = 0;
    for (unsigned int i = 0; i < total_inputs; i++) {
        num_bytes += recv_in(clientfd, &share, sizeof(BitShare));
        const std::string pk(share.pk, share.pk + PK_LENGTH);
		//std::cout << "Received pk: " << pk << ", share.val: " << share.val << std::endl;
		std::cout <<  "Share[" << i << "] = " << share.val << std::endl;
        if (share_map.find(pk) != share_map.end())
            continue;
        share_map[pk] = share.val;
    }
	
    std::cout << "Received " << total_inputs << " total shares" << std::endl;
    std::cout << "bytes from client: " << num_bytes << std::endl;
    std::cout << "receive time: " << sec_from(start) << std::endl;

	start = clock_start();
 //   auto start2 = clock_start();
	int server_bytes = 0;
	uint64_t result = 0;

// TODO bit_sum
    if (server_num == 1) {

		
		std::cout << "compute time: " << sec_from(start) << std::endl;

		return RET_NO_ANS;

	}
	else if (server_num == 2) {


		std::cout << "compute time: " << sec_from(start) << std::endl;

		return RET_NO_ANS;
	}
	else {

		uint64_t received_data;


		std::cout << "compute time: " << sec_from(start) << std::endl;

		ans = result;
		return RET_ANS;
	}
}

// 将二进制字符串转换为 uint64_t 数据
uint64_t binaryStringToUint64(const std::string& binaryString) {
    std::bitset<64> bits(binaryString);
    return bits.to_ullong();
}

returnType int_sum(const initMsg msg, const int clientfd, const int serverfd0, const int serverfd, const int server_num, uint64_t& ans){

	ans = 0;
	return RET_ANS;
}

returnType int_sum_split(const initMsg msg, const int clientfd, const int serverfd0, const int serverfd, const int server_num, uint64_t& ans){

    std::unordered_map<std::string, std::string> share_map;
    auto start = clock_start();

    IntSumShare share;
    const uint64_t max_val = 1ULL << num_bits;
    const unsigned int total_inputs = msg.num_of_inputs;

    int num_bytes = 0;
    for (unsigned int i = 0; i < total_inputs; i++) {
        num_bytes += recv_in(clientfd, &share, sizeof(IntSumShare));
        const std::string pk(share.pk, share.pk + PK_LENGTH);

        if (share_map.find(pk) != share_map.end())
            continue;
	//	share_map[pk] = std::string(share.val, sizeof(share.val));
        share_map[pk] = share.val;

//		std::cout << "strlen of share:" << strlen(share.val) << std::endl;
//		std::cout << "sizeof of share:" << sizeof(share.val) << std::endl;
        std::cout << "share[" << i << "] = " << share.val << std::endl;
    }

    std::cout << "Received " << total_inputs << " total shares" << std::endl;
    std::cout << "bytes from client: " << num_bytes << std::endl;
    std::cout << "receive time: " << sec_from(start) << std::endl;

	int length0 = num_bits / 3;
	int length1 = num_bits / 3;
	int remainder = num_bits - length0 - length1;

	start = clock_start(); //for compute time
	auto start2 = clock_start(); //for verify time

	uint64_t result = 0;
	uint64_t data;
	int server_bytes = 0;

	if (server_num == 0){
		size_t num_inputs, num_valid = 0;
        recv_size(serverfd0, num_inputs);
        recv_size(serverfd, num_inputs);
		uint64_t** const shares = new uint64_t*[num_inputs];
        bool* const valid = new bool[num_inputs];

        for (unsigned int i = 0; i < num_inputs; i++) {
            const std::string pk = get_pk(serverfd0);
            const std::string pk2 = get_pk(serverfd);

            bool is_valid = (share_map.find(pk) != share_map.end()
							and share_map.find(pk2) != share_map.end() );
            valid[i] = is_valid;
			shares[i] = new uint64_t[1];
            if (!is_valid)
                continue;
            num_valid++;
            shares[i][0] = binaryStringToUint64(share_map[pk]);
        }

		std::cout << "verify time: " << sec_from(start2) << std::endl;

		start2 = clock_start(); //for convert time

		for (auto it = share_map.begin(); it != share_map.end(); ++it) {
			result += binaryStringToUint64(it->second);
		}
		result = result << (length1+remainder);
		std::cout << "result0 = " << result << std::endl;
		ans = result;

		int bytes_received = recv(serverfd0, &data, sizeof(uint64_t), 0);
		if (bytes_received != sizeof(uint64_t)) {
			error_exit("receive error from #1.");
		}
		ans += data;
		memset(&data, 0, sizeof(uint64_t));
		bytes_received = recv(serverfd, &data, sizeof(uint64_t), 0);
		if (bytes_received != sizeof(uint64_t)) {
			error_exit("receive error from #2.");
		}

		std::cout << "Final valid count: " << num_valid << " / " << total_inputs << std::endl;
		std::cout << "convert time: " << sec_from(start2) << std::endl;		
		std::cout << "compute time: " << sec_from(start) << std::endl;		

		ans += data;
		return RET_ANS;
	}

	else if (server_num == 1){
		const size_t num_inputs = share_map.size();
		server_bytes += send_size(serverfd0, num_inputs);
		uint64_t** const shares = new uint64_t*[num_inputs];
        int i = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            shares[i] = new uint64_t[1];
            shares[i][0] = binaryStringToUint64(share.second);
            i++;
        }
		std::cout << "verify time: " << sec_from(start2) << std::endl;

		start2 = clock_start(); //for convert time

		for (auto it = share_map.begin(); it != share_map.end(); ++it) {
			result += binaryStringToUint64(it->second);
		}
		result = result << remainder;	
		std::cout << "result1 = " << result << std::endl; 

		int bytes_sent = send(serverfd0, &result, sizeof(uint64_t), 0);
		if (bytes_sent != sizeof(uint64_t)) 
			error_exit("send error to #0");

		std::cout << "convert time: " << sec_from(start2) << std::endl;		
		std::cout << "compute time: " << sec_from(start) << std::endl;		

		return RET_NO_ANS;	
	}

	else {
		const size_t num_inputs = share_map.size();
		server_bytes += send_size(serverfd0, num_inputs);
		uint64_t** const shares = new uint64_t*[num_inputs];
        int i = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            shares[i] = new uint64_t[1];
            shares[i][0] = binaryStringToUint64(share.second);
            i++;
        }

		std::cout << "verify time: " << sec_from(start2) << std::endl;

		start2 = clock_start(); //for convert time

		for (auto it = share_map.begin(); it != share_map.end(); ++it) {
			result += binaryStringToUint64(it->second);
		}
		std::cout << "result2 = " << result << std::endl; 

		int bytes_sent = send(serverfd0, &result, sizeof(uint64_t), 0);
		if (bytes_sent != sizeof(uint64_t)) 
			error_exit("Send error to #0");

		std::cout << "convert time: " << sec_from(start2) << std::endl;		
		std::cout << "compute time: " << sec_from(start) << std::endl;		

		return RET_NO_ANS;	
	}
}

int send_bool_batch(const int sockfd, const bool* const x, const size_t n) {
    const size_t len = (n+7) / 8;  // Number of bytes to hold n, aka ceil(n/8)
    char* buf = new char[len];

    memset(buf, 0, sizeof(char) * len);

    for (unsigned int i = 0; i < n; i++)
        if (x[i])
            buf[i / 8] ^= (1 << (i % 8));

    int ret = send(sockfd, buf, len, 0);

    delete[] buf;

    return ret;
}

int recv_bool_batch(const int sockfd, bool* const x, const size_t n) {
    const size_t len = (n+7) / 8;
    char* buf = new char[len];

    int ret = recv_in(sockfd, buf, len);

    for (unsigned int i = 0; i < n; i++)
        x[i] = (buf[i/8] & (1 << (i % 8)));

    delete[] buf;

    return ret;
}

// AND OR 
returnType xor_op(const initMsg msg, const int clientfd, const int serverfd0, const int serverfd, const int server_num, bool& ans) {
    std::unordered_map<std::string, uint64_t> share_map;
    auto start = clock_start();

    IntShare share;
    const unsigned int total_inputs = msg.num_of_inputs;

    int num_bytes = 0;
    for (unsigned int i = 0; i < total_inputs; i++) {
        num_bytes += recv_in(clientfd, &share, sizeof(IntShare));
        const std::string pk(share.pk, share.pk + PK_LENGTH);

        if (share_map.find(pk) != share_map.end())
            continue;
        share_map[pk] = share.val;
        std::cout << "share[" << i << "] = " << share.val << std::endl;
    }

	std::cout << "Received " << total_inputs << " total shares" << std::endl;
    std::cout << "bytes from client: " << num_bytes << std::endl;
    std::cout << "receive time: " << sec_from(start) << std::endl;
    start = clock_start();
    auto start2 = clock_start();

    int server_bytes = 0;

	if (server_num == 0) {
		// todo verify
		size_t num_inputs, num_valid = 0;
        recv_size(serverfd0, num_inputs);
        recv_size(serverfd, num_inputs);
        uint64_t a = 0;
        bool* const valid = new bool[num_inputs];

        for (unsigned int i = 0; i < num_inputs; i++) {
            const std::string pk = get_pk(serverfd0);
            const std::string pk2 = get_pk(serverfd);
            valid[i] = (share_map.find(pk) != share_map.end()
						and share_map.find(pk2) != share_map.end());
            if (!valid[i])
                continue;
            num_valid++;
            a ^= share_map[pk];
        }

        std::cout << "verify + convert time: " << sec_from(start2) << std::endl;
		start2 = clock_start();

		server_bytes += send_bool_batch(serverfd0, valid, num_inputs);
		server_bytes += send_bool_batch(serverfd, valid, num_inputs);

		delete[] valid;
		
		uint64_t b, c;
		recv_uint64(serverfd0, b);
		recv_uint64(serverfd, c);	
		const uint64_t aggr = a ^ b ^ c;
	
		std::cout << "Final valid count: " << num_valid << " / " << total_inputs << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;
        if (num_valid < total_inputs * (1 - INVALID_THRESHOLD)) {
            std::cout << "Failing, This is less than the invalid threshold of " << INVALID_THRESHOLD << std::endl;
            return RET_INVALID;
        }

		if (msg.type == AND_OP) {
            ans = (aggr == 0);
        } else if (msg.type == OR_OP) {
            ans = (aggr != 0);
        } else {
            error_exit("Message type incorrect for xor_op");
        }
		return RET_ANS;
	}

	else if (server_num == 1) {
		// verify
		const size_t num_inputs = share_map.size();
        server_bytes += send_size(serverfd0, num_inputs);
        uint64_t b = 0;
        std::string* const pk_list = new std::string[num_inputs];
        size_t idx = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            pk_list[idx] = share.first;
            idx++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
		start2 = clock_start();

        bool* const other_valid = new bool[num_inputs];
        recv_bool_batch(serverfd0, other_valid, num_inputs);
        for (unsigned int i = 0; i < num_inputs; i++) {
            if (!other_valid[i])
                continue;
            b ^= share_map[pk_list[i]];
        }
        delete[] other_valid;

        send_uint64(serverfd0, b);
        delete[] pk_list;
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;

		return RET_NO_ANS;
	}

	else {
		// verify 
		const size_t num_inputs = share_map.size();
        server_bytes += send_size(serverfd0, num_inputs);
        uint64_t c = 0;
        std::string* const pk_list = new std::string[num_inputs];
        size_t idx = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            pk_list[idx] = share.first;
            idx++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
		start2 = clock_start();

        bool* const other_valid = new bool[num_inputs];
        recv_bool_batch(serverfd0, other_valid, num_inputs);
        for (unsigned int i = 0; i < num_inputs; i++) {
            if (!other_valid[i])
                continue;
            c ^= share_map[pk_list[i]];
        }
        delete[] other_valid;

        send_uint64(serverfd0, c);
        delete[] pk_list;
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;

		return RET_NO_ANS;
	}
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

		std::cout << std::endl << "waiting for connection..." << std::endl;

		newsockfd = accept(sockfd, (struct sockaddr*)&addr, &addrlen);
        if (newsockfd < 0) error_exit("Connection creation failure");
	
		// test for connect client
/*		int bytes_received = recv(newsockfd, buffer, BUFFER_SIZE, 0);
    	if (bytes_received < 0) error_exit("Error receiving data from client");
    	std::cout << "Received message from client: " << std::string(buffer, bytes_received) << std::endl;
*/
		// Get an initMsg
        initMsg msg;
        recv_in(newsockfd, &msg, sizeof(initMsg));
//		std::cout << "Received from client. msg is:msg.type:" << msg.type << " and msg.num_of_inputs:" << msg.num_of_inputs << std::endl;

	    if (msg.type == BIT_SUM) {
            std::cout << "BIT_SUM" << std::endl;
            auto start = clock_start();

			//  bit_sum
			uint64_t ans;
			returnType ret = bit_sum(msg, newsockfd, serverfd0, serverfd, server_num, ans);
			if (ret == RET_ANS)
                std::cout << "Ans: " << ans << std::endl;

            std::cout << "Total time  : " << sec_from(start) << std::endl;
        }
		else if (msg.type == INT_SUM_SPLIT) {
            std::cout << "INT_SUM_SPLIT" << std::endl;
            auto start = clock_start();

			//  int_sum_split
			uint64_t ans;
			returnType ret = int_sum_split(msg, newsockfd, serverfd0, serverfd, server_num, ans);
			if (ret == RET_ANS)
                std::cout << "Ans: " << ans << std::endl;

            std::cout << "Total time  : " << sec_from(start) << std::endl;
		}
		else if (msg.type == INT_SUM) {
            std::cout << "INT_SUM" << std::endl;
            auto start = clock_start();

			//  int_sum
			uint64_t ans;
			returnType ret = int_sum(msg, newsockfd, serverfd0, serverfd, server_num, ans);
			if (ret == RET_ANS)
                std::cout << "Ans: " << ans << std::endl;

            std::cout << "Total time  : " << sec_from(start) << std::endl;
		}
		else if (msg.type == AND_OP) {
            std::cout << "AND_OP" << std::endl;
            auto start = clock_start();

			//  and 
			bool ans;
			returnType ret = xor_op(msg, newsockfd, serverfd0, serverfd, server_num, ans);
			if (ret == RET_ANS)
                std::cout << "Ans: " << std::boolalpha << ans << std::endl;

/*
			// for verify the ans
			ssize_t bytes_sent = send(newsockfd, &ans, sizeof(ans), 0);
			if (bytes_sent < 0) 
        		std::cerr << "Error sending boolean value" << std::endl;
*/
            std::cout << "Total time  : " << sec_from(start) << std::endl;
		}

		else if (msg.type == OR_OP) {
            std::cout << "OR_OP" << std::endl;
            auto start = clock_start();

			//  or
			bool ans;
			returnType ret = xor_op(msg, newsockfd, serverfd0, serverfd, server_num, ans);
			if (ret == RET_ANS)
                std::cout << "Ans: " << std::boolalpha << ans << std::endl;

/*
			// for verify the ans
			ssize_t bytes_sent = send(newsockfd, &ans, sizeof(ans), 0);
			if (bytes_sent < 0) 
        		std::cerr << "Error sending boolean value" << std::endl;
*/
            std::cout << "Total time  : " << sec_from(start) << std::endl;
		}

		else if (msg.type == NONE_OP) {
            std::cout << "Empty client message" << std::endl;
        } 

		else {
            std::cout << "Unrecognized message type: " << msg.type << std::endl;
        }

	//	break;
	}


	close(sockfd_server);
	close(newsockfd_server);
	close(sockfd_server0);
	close(newsockfd_server0);
	std::cout << "socket closed" << std::endl;
	
	return 0;
}

