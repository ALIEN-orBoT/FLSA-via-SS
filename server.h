#ifndef SERVER_H
#define SERVER_H


// Return type of different ops
enum returnType {
    RET_INVALID,    // Too many inputs invalid
    RET_ANS,        // Success, Returning ans. For server returning an answer
    RET_NO_ANS,     // Success, no ans. For support server.
};

#endif
