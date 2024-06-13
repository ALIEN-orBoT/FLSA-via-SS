#ifndef NET_SHARE_H
#define NET_SHARE_H

#include <string>

size_t send_out(const int sockfd, const void* const buf, const size_t len);
int recv_in(const int sockfd, void* const buf, const size_t len);

int send_bool_batch(const int sockfd, const bool* const x, const size_t n);
int recv_bool_batch(const int sockfd, bool* const x, const size_t n);

int send_size(const int sockfd, const size_t x);
int recv_size(const int sockfd, size_t& x);

int send_uint64(const int sockfd, const uint64_t x);
int recv_uint64(const int sockfd, uint64_t& x);

int send_uint64_batch(const int sockfd, const uint64_t* const x, const size_t n);

int recv_uint64_batch(const int sockfd, uint64_t* const x, const size_t n);

#endif
