#include "ot.h"

OT_Wrapper::OT_Wrapper(const char* address, const int port)
: io(new emp::NetIO(address, port, true))
, ot(new emp::OTNP<emp::NetIO>(io))
{}

OT_Wrapper::~OT_Wrapper() {
    delete ot;
    delete io;
}

void OT_Wrapper::send(const uint64_t* const data0, const uint64_t* const data1,
        const size_t length) {
    emp::block* const block0 = new emp::block[length];
    emp::block* const block1 = new emp::block[length];

    for (unsigned int i = 0; i < length; i++) {
        block0[i] = emp::makeBlock(0, data0[i]);
        block1[i] = emp::makeBlock(0, data1[i]);
        // std::cout << "Send[" << i << "] = (" << data0[i] << ", " << data1[i] << ")\n";
    }

    io->sync();
    ot->send(block0, block1, length);
    io->flush();

    delete[] block0;
    delete[] block1;
}

void OT_Wrapper::recv(uint64_t* const data, const bool* b, const size_t length) {
    emp::block* const block = new emp::block[length];
    io->sync();
    ot->recv(block, b, length);
    io->flush();

    for (unsigned int i = 0; i < length; i++) {
        data[i] = *(uint64_t*)&block[i];
        // std::cout << "Recv[" << i << "][" << b[i] << "] = " << data[i] << std::endl;
    }

    delete[] block;
}
