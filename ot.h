/*
Oblivious Transfer code
For share conversion
*/

#ifndef PROTO_H
#define PROTO_H

#include <emp-ot/emp-ot.h>
#include <emp-tool/emp-tool.h>
#include <iostream>

struct OT_Wrapper {
  emp::NetIO* const io;
  emp::OTNP<emp::NetIO>* const ot;

  OT_Wrapper(const char* address, const int port);
  ~OT_Wrapper();

  void send(const uint64_t* const data0, const uint64_t* const data1,
            const size_t length);
  void recv(uint64_t* const data, const bool* b, const size_t length);
};


#endif

