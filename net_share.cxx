#include "net_share.h"
#include "utils.h"

#include <sys/socket.h>
#include <netinet/in.h>

// Ensure this is defined, as it's architecture dependent
#if !defined(htonll) && !defined(ntohll)
#if __BIG_ENDIAN__
# define htonll(x) (x)
# define ntohll(x) (x)
#else
# define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
# define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif
#endif


size_t send_out(const int sockfd, const void* const buf, const size_t len) {
    size_t ret = send(sockfd, buf, len, 0);
    if (ret <= 0) error_exit("Failed to send");
    return ret;
}

int recv_in(const int sockfd, void* const buf, const size_t len) {
    unsigned int bytes_read = 0, tmp;
    char* bufptr = (char*) buf;
    while (bytes_read < len) {
        tmp = recv(sockfd, bufptr + bytes_read, len - bytes_read, 0);
        if (tmp <= 0) return tmp; else bytes_read += tmp;
    }
    return bytes_read;
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

int send_size(const int sockfd, const size_t x) {
    size_t x_conv = htonl(x);
    const char* data = (const char*) &x_conv;
    return send(sockfd, data, sizeof(size_t), 0);
}

int recv_size(const int sockfd, size_t& x) {
    int ret = recv_in(sockfd, &x, sizeof(size_t));
    x = ntohl(x);
    return ret;
}

int send_uint64(const int sockfd, const uint64_t x) {
    uint64_t x_conv = htonll(x);
    const char* data = (const char*) &x_conv;
    return send(sockfd, data, sizeof(uint64_t), 0);
}

int recv_uint64(const int sockfd, uint64_t& x) {
    int ret = recv_in(sockfd, &x, sizeof(uint64_t));
    x = ntohll(x);
    return ret;
}

