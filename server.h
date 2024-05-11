#ifndef SERVER_H
#define SERVER_H

#include "net_share.h"

// Return type of different ops
enum returnType {
    RET_INVALID,    // Too many inputs invalid
    RET_ANS,        // Success, Returning ans. For server returning an answer
    RET_NO_ANS,     // Success, no ans. For support server.
};

// daBits
struct Dabits {
	bool bb;
	uint64_t ba;
};
#endif
