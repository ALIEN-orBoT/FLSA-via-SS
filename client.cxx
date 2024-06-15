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
#include "net_share.h"

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

int send_maxshare(const int server_num, const MaxShare& maxshare, const unsigned int B) {
	const int sock = (server_num == 0) ? sockfd0 : ((server_num == 1) ? sockfd1 : sockfd2);

    int ret = send(sock, (void*)&(maxshare.pk[0]), PK_LENGTH, 0);
    ret += send_uint64_batch(sock, maxshare.arr, B+1);

    return ret;
}

int send_freqshare(const int server_num, const FreqShare& freqshare, const uint64_t n) {
	const int sock = (server_num == 0) ? sockfd0 : ((server_num == 1) ? sockfd1 : sockfd2);

    int ret = send(sock, (void*)&(freqshare.pk[0]), PK_LENGTH, 0);
    ret += send_bool_batch(sock, freqshare.arr, n);
    return ret;
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
        std::cout << "client[" << i << "]: " << real_val << " = " << bitshare0[i].val << " ^ " << bitshare1[i].val << " ^ " << bitshare2[i].val << std::endl;
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

int int_sum_sp_helper(const std::string protocol, const size_t numreqs, unsigned int &ans, const initMsg* const msg_ptr = nullptr) {
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

        std::cout << "client[" << i << "]: " << real_val << " = " << intshare0[i].val << " | " << intshare1[i].val << " | " << intshare2[i].val << std::endl;
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

void int_sum_split(const std::string protocol, const size_t numreqs) {
	unsigned int ans = 0;
	int num_bytes = 0;
	initMsg msg;
	msg.num_of_inputs = numreqs;
	msg.type = INT_SUM_SPLIT;

	num_bytes += int_sum_sp_helper(protocol, numreqs, ans, &msg);

	std::cout << "Ans: " << ans << std::endl;
	std::cout << "Total sent bytes: " << num_bytes << std::endl;
}

int int_sum_helper(const std::string protocol, const size_t numreqs,
                   uint64_t &ans, const initMsg* const msg_ptr = nullptr) {
    auto start = clock_start();
    int num_bytes = 0;

    uint64_t real_val, share0, share1, share2;

    emp::PRG prg;

    IntShare* const intshare0 = new IntShare[numreqs];
    IntShare* const intshare1 = new IntShare[numreqs];
    IntShare* const intshare2 = new IntShare[numreqs];

    for (unsigned int i = 0; i < numreqs; i++) {
        prg.random_data(&real_val, sizeof(uint64_t));
        prg.random_data(&share0, sizeof(uint64_t));
        prg.random_data(&share1, sizeof(uint64_t));
        real_val = real_val % max_int;
        share0 = share0 % max_int;
        share1 = share1 % max_int;
        share2 = share1 ^ share0 ^ real_val;
        ans += real_val;

        const std::string pk_s = make_pk(prg);
        const char* const pk = pk_s.c_str();

        memcpy(intshare0[i].pk, &pk[0], PK_LENGTH);
        intshare0[i].val = share0;

        memcpy(intshare1[i].pk, &pk[0], PK_LENGTH);
        intshare1[i].val = share1;

        memcpy(intshare2[i].pk, &pk[0], PK_LENGTH);
        intshare2[i].val = share2;
		
		std::cout << "client[" << i << "]: " << real_val << " = " << intshare0[i].val << " ^ " << intshare1[i].val << " ^ " << intshare2[i].val << std::endl;
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
        num_bytes += send_to_server(0, &intshare0[i], sizeof(IntShare));
        num_bytes += send_to_server(1, &intshare1[i], sizeof(IntShare));
        num_bytes += send_to_server(2, &intshare2[i], sizeof(IntShare));
    }
    delete[] intshare0;
    delete[] intshare1;
    delete[] intshare2;

    if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void int_sum(const std::string protocol, const size_t numreqs) {
	uint64_t ans = 0;
    int num_bytes = 0;
    initMsg msg;
    msg.num_of_inputs = numreqs;
    msg.type = INT_SUM;

	num_bytes += int_sum_helper(protocol, numreqs, ans, &msg);

    std::cout << "Ans : " << ans << std::endl;
    std::cout << "Total sent bytes: " << num_bytes << std::endl;
}

// and or
int xor_op_helper(const std::string protocol, const size_t numreqs,
                  bool &ans, const initMsg* const msg_ptr = nullptr) {
	auto start = clock_start();
	int num_bytes = 0;
	
	bool value;
	uint64_t encoded, share0, share1, share2;

	emp::PRG prg;
	
	IntShare* const intshare0 = new IntShare[numreqs];
	IntShare* const intshare1 = new IntShare[numreqs];
	IntShare* const intshare2 = new IntShare[numreqs];

	for (unsigned int i = 0; i < numreqs; i++) {
		prg.random_bool(&value, 1);
//		value = 1;
		if (protocol == "ANDOP") {
			ans &=value;
			if (value)
				encoded = 0;
			else	
				prg.random_data(&encoded, sizeof(uint64_t));
		} else if (protocol == "OROP") {
			ans |= value;
			if (not value)
				encoded = 0;
			else
				prg.random_data(&encoded, sizeof(uint64_t));
		}
		prg.random_data(&share0, sizeof(uint64_t));
		prg.random_data(&share1, sizeof(uint64_t));
		share2 = share0 ^ share1 ^ encoded;	
	
        const std::string pk_s = make_pk(prg);
        const char* const pk = pk_s.c_str();

        memcpy(intshare0[i].pk, &pk[0], PK_LENGTH);
        intshare0[i].val = share0;

        memcpy(intshare1[i].pk, &pk[0], PK_LENGTH);
        intshare1[i].val = share1;

        memcpy(intshare2[i].pk, &pk[0], PK_LENGTH);
        intshare2[i].val = share2;

        std::cout << "client" << i << ": " << encoded << " = " << intshare0[i].val << " ^ " << intshare1[i].val << " ^ " << intshare2[i].val << std::endl;
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
        num_bytes += send_to_server(0, &intshare0[i], sizeof(IntShare));
        num_bytes += send_to_server(1, &intshare1[i], sizeof(IntShare));
        num_bytes += send_to_server(2, &intshare2[i], sizeof(IntShare));
    }

    delete[] intshare0;
    delete[] intshare1;
    delete[] intshare2;

	if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void xor_op(const std::string protocol, const size_t numreqs) {
	bool ans;
	int num_bytes = 0;
	initMsg msg;
	msg.num_of_inputs = numreqs;
	if (protocol == "ANDOP") {
		msg.type = AND_OP;
		ans = true;
	} else if (protocol == "OROP") {
		msg.type = OR_OP;
		ans = false;
	} else {
		return;
	}

	num_bytes += xor_op_helper(protocol, numreqs, ans, &msg);

	std::cout << "Ans : " << std::boolalpha << ans << std::endl;
	std::cout << "Total sent bytes: " << num_bytes << std::endl;

/*
	// for verify the ans
	bool ans2;
	ssize_t bytes_received = recv(sockfd0, &ans2, sizeof(ans2), 0);
	if (bytes_received < 0) {
        std::cerr << "Error receiving boolean value" << std::endl;
    } else if (bytes_received == 0) {
        std::cerr << "Connection closed by peer" << std::endl;
    }
	std::cout << "Server Ans: " << ans2 << std::endl;
	if (ans2 != ans) 
		std::cout << "Ans error!!!!!!!!!!!!!!!!!!" << std::endl;
	else std::cout << "Ans right" << std::endl;
*/
}

// max
int max_op_helper(const std::string protocol, const size_t numreqs,
                  const unsigned int B, uint64_t &ans,
                  const initMsg* const msg_ptr = nullptr) {
    auto start = clock_start();
    int num_bytes = 0;

    start = clock_start();

    uint64_t value;
    uint64_t* const or_encoded_array = new uint64_t[B+1];
    uint64_t* const share0 = new uint64_t[B+1];
    uint64_t* const share1 = new uint64_t[B+1];
    uint64_t* const share2 = new uint64_t[B+1];

    emp::PRG prg;

    MaxShare* const maxshare0 = new MaxShare[numreqs];
    MaxShare* const maxshare1 = new MaxShare[numreqs];
    MaxShare* const maxshare2 = new MaxShare[numreqs];
    for (unsigned int i = 0; i < numreqs; i++) {
        prg.random_data(&value, sizeof(uint64_t));
        value = value % (B + 1);

        if (protocol == "MAXOP")
            ans = (value > ans ? value : ans);
        if (protocol == "MINOP")
            ans = (value < ans ? value : ans);

        prg.random_data(or_encoded_array, (B+1)*sizeof(uint64_t));
        prg.random_data(share1, (B+1)*sizeof(uint64_t));
        prg.random_data(share2, (B+1)*sizeof(uint64_t));

        uint64_t v = 0;
        if (protocol == "MAXOP")
            v = value;
        if (protocol == "MINOP")
            v = B - value;

        for (unsigned int j = v + 1; j <= B ; j++)
            or_encoded_array[j] = 0;

        for (unsigned int j = 0; j <= B; j++)
            share0[j] = share1[j] ^ share2[j] ^ or_encoded_array[j];

        const std::string pk_s = make_pk(prg);
        const char* const pk = pk_s.c_str();

        memcpy(maxshare0[i].pk, &pk[0], PK_LENGTH);
        maxshare0[i].arr = new uint64_t[B+1];
        memcpy(maxshare0[i].arr, share0, (B+1)*sizeof(uint64_t));

        memcpy(maxshare1[i].pk, &pk[0], PK_LENGTH);
        maxshare1[i].arr = new uint64_t[B+1];
        memcpy(maxshare1[i].arr, share1, (B+1)*sizeof(uint64_t));

        memcpy(maxshare2[i].pk, &pk[0], PK_LENGTH);
        maxshare2[i].arr = new uint64_t[B+1];
        memcpy(maxshare2[i].arr, share2, (B+1)*sizeof(uint64_t));

//        std::cout << "client" << i << ": " << or_encoded_array[i] << " = " << maxshare0[i].arr << " ^ " << maxshare1[i].arr << " ^ " << maxshare2[i].arr << std::endl;
		std::cout << "client" << i << ": " << value << "; " ;
		for (int i = 0; i < max_int; ++i)  
			std::cout << or_encoded_array[i] << " ";
		std::cout << " = ";
		for (int j = 0; j < max_int; ++j) 
			std::cout << maxshare0[i].arr[j] << " ";
		std::cout << " ^ "; 
		for (int j = 0; j < max_int; ++j) 
			std::cout << maxshare1[i].arr[j] << " ";
		std::cout << " ^ "; 
		for (int j = 0; j < max_int; ++j) 
			std::cout << maxshare2[i].arr[j] << " ";
		std::cout << std::endl;
    }
    delete[] or_encoded_array;
    delete[] share0;
    delete[] share1;
    delete[] share2;
    if (numreqs > 1)
        std::cout << "batch make:\t" << sec_from(start) << std::endl;

    start = clock_start();
    if (msg_ptr != nullptr) {
        num_bytes += send_to_server(0, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(1, msg_ptr, sizeof(initMsg));
        num_bytes += send_to_server(2, msg_ptr, sizeof(initMsg));
    }
    for (unsigned int i = 0; i < numreqs; i++) {
        num_bytes += send_maxshare(0, maxshare0[i], B);
        num_bytes += send_maxshare(1, maxshare1[i], B);
        num_bytes += send_maxshare(2, maxshare2[i], B);

        delete[] maxshare0[i].arr;
        delete[] maxshare1[i].arr;
        delete[] maxshare2[i].arr;
    }

    delete[] maxshare0;
    delete[] maxshare1;
    delete[] maxshare2;

    if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void max_op(const std::string protocol, const size_t numreqs) {
    const uint64_t B = max_int;

    uint64_t ans;
    int num_bytes = 0;
    initMsg msg;
    msg.num_of_inputs = numreqs;
    msg.max_inp = B;
    if (protocol == "MAXOP") {
        msg.type = MAX_OP;
        ans = 0;
    } else if (protocol == "MINOP") {
        msg.type = MIN_OP;
        ans = B;
    } else {
        return;
    }

	num_bytes += max_op_helper(protocol, numreqs, B, ans, &msg);

    std::cout << "Ans : " << ans << std::endl;
    std::cout << "Total sent bytes: " << num_bytes << std::endl;
}

int freq_helper(const std::string protocol, const size_t numreqs,
                uint64_t* counts, const initMsg* const msg_ptr = nullptr) {
    auto start = clock_start();
    int num_bytes = 0;

    uint64_t real_val;
	bool* real_arr = new bool[max_int];

    emp::PRG prg;

    FreqShare* const freqshare0 = new FreqShare[numreqs];
    FreqShare* const freqshare1 = new FreqShare[numreqs];
    FreqShare* const freqshare2 = new FreqShare[numreqs];
    for (unsigned int i = 0; i < numreqs; i++) {
        prg.random_data(&real_val, sizeof(uint64_t));
        real_val %= max_int;
        counts[real_val] += 1;

		memset(real_arr, 0, max_int * sizeof(bool));
		real_arr[real_val] = 1;
//		for (int i = 0; i < max_int; ++i) 
//			std::cout << real_arr[i] << "";

        // std::cout << "Value " << i << " = " << real_val << std::endl;

        // Same everywhere exept at real_val
        freqshare0[i].arr = new bool[max_int];
        prg.random_bool(freqshare0[i].arr, max_int);
        freqshare1[i].arr = new bool[max_int];
        prg.random_bool(freqshare1[i].arr, max_int);
        freqshare2[i].arr = new bool[max_int];
		for (unsigned int j = 0; j < max_int; j++) {
            freqshare2[i].arr[j] = freqshare0[i].arr[j] ^ freqshare1[i].arr[j] ^ real_arr[j];
        }	

        const std::string pk_s = make_pk(prg);
        const char* const pk = pk_s.c_str();
        memcpy(freqshare0[i].pk, &pk[0], PK_LENGTH);
        memcpy(freqshare1[i].pk, &pk[0], PK_LENGTH);
        memcpy(freqshare2[i].pk, &pk[0], PK_LENGTH);

		std::cout << "client" << i << ": " << real_val << "; " ;
		for (int i = 0; i < max_int; ++i) 
			std::cout << real_arr[i] << "";
		std::cout << " = ";
		for (int j = 0; j < max_int; ++j) 
			std::cout << freqshare0[i].arr[j];
		std::cout << " ^ "; 
		for (int j = 0; j < max_int; ++j) 
			std::cout << freqshare1[i].arr[j];
		std::cout << " ^ "; 
		for (int j = 0; j < max_int; ++j) 
			std::cout << freqshare2[i].arr[j];
		std::cout << std::endl;
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
        num_bytes += send_freqshare(0, freqshare0[i], max_int);
        num_bytes += send_freqshare(1, freqshare1[i], max_int);
        num_bytes += send_freqshare(2, freqshare2[i], max_int);

        delete[] freqshare0[i].arr;
        delete[] freqshare1[i].arr;
        delete[] freqshare2[i].arr;
    }

	delete[] real_arr;
    delete[] freqshare0;
    delete[] freqshare1;
    delete[] freqshare2;

    if (numreqs > 1)
        std::cout << "batch send:\t" << sec_from(start) << std::endl;

    return num_bytes;
}

void freq_op(const std::string protocol, const size_t numreqs) {
    uint64_t* count = new uint64_t[max_int];
    memset(count, 0, max_int * sizeof(uint64_t));
    int num_bytes = 0;
    initMsg msg;
    msg.num_of_inputs = numreqs;
    msg.max_inp = max_int;
    msg.type = FREQ_OP;
	if (protocol == "MEDOP") msg.type = MED_OP;

	num_bytes += freq_helper(protocol, numreqs, count, &msg);

    for (unsigned int j = 0; j < max_int; j++)
        std::cout << " Freq(" << j << ") = " << count[j] << std::endl;

	if (protocol == "MEDOP") {
		uint64_t mid1 = (numreqs + 1) / 2;
		uint64_t mid2 = (numreqs + 2) / 2;
		int cnt = 0;
		uint64_t median1=0, median2=0;
		
		for (unsigned int i = 0; i < max_int; i++) {
			cnt += count[i];
			if (cnt >= mid1) median1 = i;
			if (cnt >= mid2) median2 = i;
			if (median1 && median2) break;
		}

		if (numreqs % 2 == 0) 
			std::cout << "Median: " << (median1 +median2)/2 << std::endl;
		else
			std::cout << "Median: " << median1 << std::endl;
	}
    delete[] count;

    std::cout << "Total sent bytes: " << num_bytes << std::endl;
}

int main(int argc, char** argv) {
	if (argc < 4) {
		std::cout << "argument: client_num OPERATION num_bits" <<std::endl;
		return 1;
	}

	const int numreqs = atoi(argv[1]);  // Number of simulated clients
    const int port0 = 8000;
    const int port1 = 8001;
    const int port2 = 8002;
	const std::string protocol(argv[2]);

	num_bits = atoi(argv[3]);
//	num_bits = 8;
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
//	std::cout << "Init constants: " << std::endl;


    initMsg msg;
    msg.num_of_inputs = numreqs;

	auto start = clock_start();
	if (protocol == "BITSUM") {
		std::cout << "Uploading all BITSUM shares: " << numreqs << std::endl;

		// bit_sum
		bit_sum(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "INTSUMSP") {
		std::cout << "Uploading all INTSUM_Split shares: " << numreqs << std::endl;

		// int_sum_split
		int_sum_split(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "INTSUM") {
		std::cout << "Uploading all INTSUM shares: " << numreqs << std::endl;

		// int_sum
		int_sum(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "ANDOP") {
		std::cout << "Uploading all AND shares: " << numreqs << std::endl;

		// and
		xor_op(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "OROP") {
		std::cout << "Uploading all OR shares: " << numreqs << std::endl;

		// or 
		xor_op(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "MAXOP") {
		std::cout << "Uploading all MAX shares: " << numreqs << std::endl;

		// max 
		max_op(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	} 
	else if (protocol == "MINOP") {
		std::cout << "Uploading all MIN shares: " << numreqs << std::endl;

		// min 
		max_op(protocol, numreqs);

		std::cout << "Total time:\t" << sec_from(start) << std::endl;
	}
	else if(protocol == "FREQOP") {
        std::cout << "Uploading all FREQ shares: " << numreqs << std::endl;

		// freq
        freq_op(protocol, numreqs);

        std::cout << "Total time:\t" << sec_from(start) << std::endl;
    }
	else if(protocol == "MEDOP") {
        std::cout << "Uploading all MEDIAN shares: " << numreqs << std::endl;

		// median
        freq_op(protocol, numreqs);

        std::cout << "Total time:\t" << sec_from(start) << std::endl;
	}

	else {
		std::cout << "Unrecognized protocol" << std::endl;
		initMsg msg;
        msg.type = NONE_OP;
        send_to_server(0, &msg, sizeof(initMsg));
        send_to_server(1, &msg, sizeof(initMsg));
        send_to_server(2, &msg, sizeof(initMsg));
	}

    close(sockfd0);
    close(sockfd1);
    close(sockfd2);
	std::cout << "socket closed" << std::endl;

	return 0;
}

