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
#include <queue>

#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>

#include "ot.h"
#include "net_share.h"
#include "types.h"
#include "utils.h"

#define SERVER0_IP "127.0.0.1"
#define SERVER1_IP "127.0.0.1"
#define SERVER2_IP "127.0.0.1"

#define BUFFER_SIZE 1024

#define INVALID_THRESHOLD 0.5

OT_Wrapper* ot;

std::queue<Dabits> server1Queue;
std::queue<Dabits> server2Queue;

uint32_t num_bits;
unsigned int dabits_num = 200000;
const uint64_t p = 1493925467;

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
    auto start2 = clock_start();

	int server_bytes = 0;

	// bit_sum 
    if (server_num == 0) {
		size_t num_inputs, num_valid = 0;
        recv_size(serverfd0, num_inputs);
        recv_size(serverfd, num_inputs);
        bool* const shares = new bool[num_inputs];
        bool* const valid = new bool[num_inputs];

        for (unsigned int i = 0; i < num_inputs; i++) {
            const std::string pk = get_pk(serverfd0);
            const std::string pk2 = get_pk(serverfd);

            bool is_valid = (share_map.find(pk) != share_map.end()
							and share_map.find(pk) != share_map.end());
            valid[i] = is_valid;
            if (!is_valid)
                continue;
            num_valid++;
            shares[i] = share_map[pk];
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		bool v[num_inputs];
		bool v1[num_inputs];
		bool v2[num_inputs];
		ssize_t recvv = recv(serverfd0, v1, num_inputs, 0);
		if  (recvv < 0) std::cout<<"recv v1 error!"<<std::endl;
		recvv = recv(serverfd, v2, num_inputs, 0);
		if  (recvv < 0) std::cout<<"recv v2 error!"<<std::endl;

        for (unsigned int i = 0; i < num_inputs; i++) {
			v[i] = shares[i] ^ v1[i] ^ v2[i];
		}	

		server_bytes += send_out(serverfd0, v, num_inputs);
		server_bytes += send_out(serverfd, v, num_inputs);

		delete[] shares;
		delete[] valid;

		uint64_t result = 0;
        for (unsigned int i = 0; i < num_inputs; i++) 
			result += v[i];	

		const uint64_t a = result;
        uint64_t b, c;
        recv_uint64(serverfd0, b);
        recv_uint64(serverfd, c);

        std::cout << "Final valid count: " << num_valid << " / " << total_inputs << std::endl;
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;
        if (num_valid < total_inputs * (1 - INVALID_THRESHOLD)) {
            std::cout << "Failing, This is less than the invalid threshold of " << INVALID_THRESHOLD << std::endl;
            return RET_INVALID;
        }

        ans = (a + b + c) % p;
		
		return RET_ANS;
	}
	else if (server_num == 1) {
		const size_t num_inputs = share_map.size();
        server_bytes += send_size(serverfd0, num_inputs);
        bool* const shares = new bool[num_inputs];
        int i = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            shares[i] = share.second;
            i++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		// Doing B2A
		bool v[num_inputs];
		bool bb[num_inputs];
		uint64_t ba[num_inputs];
		Dabits dabits;
		for (unsigned int i = 0; i < num_inputs; i++) {
			dabits = server1Queue.front();
			bb[i] = dabits.bb;
			ba[i] = dabits.ba;
			server1Queue.pop();

			v[i] = shares[i] ^ bb[i]; 
		}	
		// send v to server0
		server_bytes += send_out(serverfd0, v, sizeof(v));
		// recv v from server0
		ssize_t recvv = recv(serverfd0, v, num_inputs, 0);

		std::cout << "Current Dabits: " << server1Queue.size() << std::endl;

		uint64_t result = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			result += (1-2*v[i])*ba[i];
			result = (2*p +result) % p;
		}

        const uint64_t b = result;
        delete[] shares;

        send_uint64(serverfd0, b);
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;

		return RET_NO_ANS;
	}
	else {
		const size_t num_inputs = share_map.size();
        server_bytes += send_size(serverfd0, num_inputs);
        bool* const shares = new bool[num_inputs];
        int i = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            shares[i] = share.second;
            i++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		// Doing B2A
		bool v[num_inputs];
		bool bb[num_inputs];
		uint64_t ba[num_inputs];
		Dabits dabits;
		for (unsigned int i = 0; i < num_inputs; i++) {
			dabits = server2Queue.front();
			bb[i] = dabits.bb;
			ba[i] = dabits.ba;
			server2Queue.pop();

			v[i] = shares[i] ^ bb[i]; 
		}	
		// send v to server0
		server_bytes += send_out(serverfd0, v, sizeof(v));
		// recv v from server0
		ssize_t recvv = recv(serverfd0, v, num_inputs, 0);

		std::cout << "Current Dabits: " << server2Queue.size() << std::endl;

		uint64_t result = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			result += (1-2*v[i])*ba[i];
			result = (2*p +result) % p;
		}

        const uint64_t c = result;
        delete[] shares;

        send_uint64(serverfd0, c);
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;

		return RET_NO_ANS;
	}
}

// 将二进制字符串转换为 uint64_t 数据
uint64_t binaryStringToUint64(const std::string& binaryString) {
    std::bitset<64> bits(binaryString);
    return bits.to_ullong();
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

	// int_sum_split
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
		if (bytes_received != sizeof(uint64_t)) 
			error_exit("receive error from #1.");
		
		ans += data;
		memset(&data, 0, sizeof(uint64_t));
		bytes_received = recv(serverfd, &data, sizeof(uint64_t), 0);
		if (bytes_received != sizeof(uint64_t)) 
			error_exit("receive error from #2.");

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

		ssize_t bytes_sent = send_out(serverfd0, &result, sizeof(uint64_t));

		std::cout << "convert time: " << sec_from(start2) << std::endl;		
		std::cout << "compute time: " << sec_from(start) << std::endl;		
        std::cout << "sent server bytes: " << server_bytes << std::endl;

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

		ssize_t bytes_sent = send_out(serverfd0, &result, sizeof(uint64_t));

		std::cout << "convert time: " << sec_from(start2) << std::endl;		
		std::cout << "compute time: " << sec_from(start) << std::endl;		
        std::cout << "sent server bytes: " << server_bytes << std::endl;

		return RET_NO_ANS;	
	}
}

// int_sum
returnType int_sum(const initMsg msg, const int clientfd, const int serverfd0, const int serverfd, const int server_num, uint64_t& ans){
	std::unordered_map<std::string, uint64_t> share_map;
    auto start = clock_start();

    IntShare share;
    const uint64_t max_val = 1ULL << num_bits;
    const unsigned int total_inputs = msg.num_of_inputs;
    const size_t nbits[1] = {num_bits};

    int num_bytes = 0;
	uint32_t share_[total_inputs];
    for (unsigned int i = 0; i < total_inputs; i++) {
        num_bytes += recv_in(clientfd, &share, sizeof(IntShare));
        const std::string pk(share.pk, share.pk + PK_LENGTH);

        if (share_map.find(pk) != share_map.end()
            or share.val >= max_val)
            continue;
        share_map[pk] = share.val;

		share_[i] = share.val;
        std::cout << "share[" << i << "] = " << share.val << std::endl;
    }

    std::cout << "Received " << total_inputs << " total shares" << std::endl;
    std::cout << "bytes from client: " << num_bytes << std::endl;
    std::cout << "receive time: " << sec_from(start) << std::endl;
    start = clock_start();
    auto start2 = clock_start();

    int server_bytes = 0;

	// int_sum
	if (server_num == 0) {
		size_t num_inputs, num_valid = 0;
        recv_size(serverfd0, num_inputs);
        recv_size(serverfd, num_inputs);
        uint64_t** const shares = new uint64_t*[num_inputs];
        bool* const valid = new bool[num_inputs];

        for (unsigned int i = 0; i < num_inputs; i++) {
            const std::string pk = get_pk(serverfd0);
            const std::string pk2 = get_pk(serverfd);

            bool is_valid = (share_map.find(pk) != share_map.end()
							and share_map.find(pk2) != share_map.end());
            valid[i] = is_valid;
            shares[i] = new uint64_t[1];
            if (!is_valid)
                continue;
            num_valid++;
            shares[i][0] = share_map[pk];
			
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		bool shares0[num_inputs][num_bits];
		for (unsigned int i = 0; i < num_inputs; ++i) {
			//std::cout << "share[" << i << "] = ";
			for (unsigned int j = 0; j < num_bits; ++j) {
				shares0[i][j] = (share_[i] >> j) & 1; 
				//std::cout << shares0[i][j];
			}
			//std::cout << std::endl;
		}

		bool v[num_inputs][num_bits];
		bool v1[num_bits*num_inputs];
		bool v2[num_bits*num_inputs];
		bool v_[num_inputs*num_bits];

		ssize_t recvv = recv(serverfd0, v1, num_inputs*num_bits, 0);
		if  (recvv < 0) std::cout<<"recv v1 error!"<<std::endl;
		recvv = recv(serverfd, v2, num_inputs*num_bits, 0);
		if  (recvv < 0) std::cout<<"recv v2 error!"<<std::endl;
	
		int cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
        	for (unsigned int j = 0; j < num_bits; j++) {
				v_[cnt] = shares0[i][j] ^ v1[cnt] ^ v2[cnt];
				cnt++;
			}
		}

		server_bytes += send_out(serverfd0, v_, num_inputs*num_bits);
		server_bytes += send_out(serverfd, v_, num_inputs*num_bits);

        delete[] valid;

        uint64_t a = 0;
		cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
        	for (unsigned int j = 0; j < num_bits; j++) {
				v[i][j] = v_[cnt];
				cnt++;
			}
		}
		for (unsigned int i = 0; i < num_inputs; i++) {
        	for (unsigned int j = 0; j < num_bits; j++) {
				a += pow(2,j)*v[i][j];
			}
			a = a % p;
		}

        delete[] shares;

        uint64_t b, c;
        recv_uint64(serverfd0, b);
        recv_uint64(serverfd, c);
        std::cout << "Final valid count: " << num_valid << " / " << total_inputs << std::endl;
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        if (num_valid < total_inputs * (1 - INVALID_THRESHOLD)) {
            std::cout << "Failing, This is less than the invalid threshold of " << INVALID_THRESHOLD << std::endl;
            return RET_INVALID;
        }

        ans = a + b + c;
		ans = ans % p;

		return RET_ANS;
	}
	else if (server_num == 1) {
		const size_t num_inputs = share_map.size();
        server_bytes += send_size(serverfd0, num_inputs);
        uint64_t** const shares = new uint64_t*[num_inputs];
        int i = 0;
        for (const auto& share : share_map) {
            server_bytes += send_out(serverfd0, &share.first[0], PK_LENGTH);
            shares[i] = new uint64_t[1];
            shares[i][0] = share.second;
            i++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		bool shares1[num_inputs][num_bits];
		for (unsigned int i = 0; i < num_inputs; ++i) {
			//std::cout << "share[" << i << "] = ";
			for (unsigned int j = 0; j < num_bits; ++j) {
				shares1[i][j] = (share_[i] >> j) & 1; 
				//std::cout << shares1[i][j];
			}
			//std::cout << std::endl;
		}

		// Doing B2A
		bool v[num_inputs][num_bits];
		bool bb[num_inputs][num_bits];
		uint64_t ba[num_inputs][num_bits];
		Dabits dabits;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				dabits = server1Queue.front();
				bb[i][j] = dabits.bb;
				ba[i][j] = dabits.ba;
				server1Queue.pop();

				v[i][j] = shares1[i][j] ^ bb[i][j]; 
			}
		}
		// convert 2 dimension to 1 dimention
		bool v_[num_bits*num_inputs];
		int cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				v_[cnt] = v[i][j];
				cnt++;
			}
		}	
		// send v to server0
		server_bytes += send_out(serverfd0, v_, num_inputs*num_bits);
		// recv v from server0
		ssize_t recvv = recv(serverfd0, v_, num_inputs*num_bits, 0);

		// convert 1 dimention to 2 dimension
		cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				v[i][j] = v_[cnt];
				cnt++;
			}
		}	

		std::cout << "Current Dabits: " << server1Queue.size() << std::endl;

		uint64_t result[num_bits] = {0};
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				result[j] += (1-2*v[i][j])*ba[i][j];
				result[j] = (2*p +result[j]) % p;
			}
		}

        uint64_t b = 0;
		for (unsigned int i = 0; i < num_bits; i++) {
			b += pow(2,i)*result[i];
			b = b % p;
		}

        delete[] shares;

        send_uint64(serverfd0, b);
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;

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
            shares[i][0] = share.second;
            i++;
        }
        std::cout << "verify time: " << sec_from(start2) << std::endl;
        start2 = clock_start();

		bool shares2[num_inputs][num_bits];
		for (unsigned int i = 0; i < num_inputs; ++i) {
			//std::cout << "share[" << i << "] = ";
			for (unsigned int j = 0; j < num_bits; ++j) {
				shares2[i][j] = (share_[i] >> j) & 1; 
				//std::cout << shares2[i][j];
			}
			//std::cout << std::endl;
		}

		// Doing B2A
		bool v[num_inputs][num_bits];
		bool bb[num_inputs][num_bits];
		uint64_t ba[num_inputs][num_bits];
		Dabits dabits;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				dabits = server2Queue.front();
				bb[i][j] = dabits.bb;
				ba[i][j] = dabits.ba;
				server2Queue.pop();

				v[i][j] = shares2[i][j] ^ bb[i][j]; 
			}
		}	
		// convert 2 dimension to 1 dimention
		bool v_[num_bits*num_inputs];
		int cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				v_[cnt] = v[i][j];
				cnt++;
			}
		}	
		// send v to server0
		server_bytes += send_out(serverfd0, v_, num_inputs*num_bits);
		// recv v from server0
		ssize_t recvv = recv(serverfd0, v_, num_inputs*num_bits, 0);

		// convert 1 dimention to 2 dimension
		cnt = 0;
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				v[i][j] = v_[cnt];
				cnt++;
			}
		}	

		std::cout << "Current Dabits: " << server2Queue.size() << std::endl;

		uint64_t result[num_bits] = {0};
		for (unsigned int i = 0; i < num_inputs; i++) {
			for (unsigned int j = 0; j < num_bits; j++) {
				result[j] += (1-2*v[i][j])*ba[i][j];
				result[j] = (2*p +result[j]) % p;
			}
		}

        uint64_t c = 0;
		for (unsigned int i = 0; i < num_bits; i++) {
			c += pow(2,i)*result[i];
			c = c % p;
		}

        delete[] shares;

        send_uint64(serverfd0, c);
        std::cout << "convert time: " << sec_from(start2) << std::endl;
        std::cout << "compute time: " << sec_from(start) << std::endl;
        std::cout << "sent server bytes: " << server_bytes << std::endl;


		return RET_NO_ANS;
	}
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

	// xor_op
	if (server_num == 0) {
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

	if (argc < 3) {
		std::cout << "argument: server_num(0/1/2) num_bits" << std::endl;
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

	num_bits = atoi(argv[2];
//	num_bits = 8;

//	std::cout << "init_constants: " << std::endl;

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
	char buffer[BUFFER_SIZE];
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

	// precomputation ..
	unsigned int need = dabits_num;

	if (server_num == 1)
		ot = new OT_Wrapper(nullptr, 60050);
	else if (server_num == 2)
		ot = new OT_Wrapper(SERVER1_IP, 60050);
	std::cout << "Created a OT" << std::endl;

	auto start = clock_start();
	std::cout << "Doing precomputation..." << std::endl;
	emp::PRG prg;
//	const uint64_t p = 41381;

	// Generate dabits
	if (server_num == 1) {
		Dabits dabits1;
		uint64_t x[need], data0[need], data1[need];
		bool r[need];
		uint64_t y1[need];
		for (unsigned int i = 0; i < need; i++) {
			prg.random_data(&x[i], sizeof(int));
			x[i] = x[i] % p;
			prg.random_bool(&r[i], 1);
			// for OT sending
			data0[i] = x[i];
			data1[i] = x[i] + r[i];
			// set y1
			y1[i] = p - x[i]; // -x[i] mod p
		//	std::cout << "y1=" << y1[i] << std::endl;
			dabits1.bb = r[i];
			dabits1.ba = (3*p + r[i] - 2*y1[i]) % p;
		//	std::cout << "bb=" << dabits1.bb << "\tba=" << dabits1.ba << std::endl;
			server1Queue.push(dabits1);	
		}
		ot->send(data0, data1, need);
	//	std::cout << "Queue size: " << server1Queue.size() << std::endl;

	} else if (server_num == 2) {
		Dabits dabits2;
		bool r[need];
		uint64_t y2[need];
		const bool* choice[need];
		for (unsigned int i = 0; i < need; i++) {
			prg.random_bool(&r[i], 1);
		}
		// OT receive, set y2
		ot->recv(y2, r, need);
		for (unsigned int i = 0; i < need; i++) {
			dabits2.bb = r[i];
			dabits2.ba = (3*p + r[i] - 2*y2[i]) % p;
		//	std::cout << "y2=" << y2[i] << std::endl;
		//	std::cout << "bb=" << dabits2.bb << "\tba=" << dabits2.ba << std::endl;
			server2Queue.push(dabits2);	
		}
	//	std::cout << "Queue size: " << server2Queue.size() << std::endl;
	}
	std::cout << "adding Dabits: " << need << std::endl;
	std::cout << "adding Dabits timing: " << sec_from(start) << std::endl;

/*
	// OT test
	if (server_num == 1) {
		uint64_t x;	
		prg.random_data(&x, sizeof(int));
		x = x % p;
		bool r;
		prg.random_bool(&r, 1);

		// OT
		uint64_t data0[] = {x};
		uint64_t data1[] = {x+r};
		ot1->send(data0, data1, 1);
		
		int y1 = -x;
		std::cout << "x = " << x << "\t x+r = " << x+r << std::endl;

	} else if (server_num == 2) {
		bool r;
		prg.random_bool(&r, 1);

		// OT
		uint64_t received[1];
		const bool * choice = &r;
		ot1->recv(received, choice, 1);

		uint64_t *y2 = received;
		std::cout << "random bits:" << r <<"\treceived y2 = " << *y2 << std::endl;
	}
*/

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
		close(newsockfd);
	//	break;
	}

	close(sockfd_server);
	close(newsockfd_server);
	close(sockfd_server0);
	close(newsockfd_server0);

	delete ot;
	std::cout << "socket closed" << std::endl;

	return 0;
}

