#include "protocol.h"
#include <cassert>
#include <cstdio>
#include <cstring>
void packet(uint8_t* b,uint32_t seq) {
  const uint8_t initial[16]={'2','D','K','I',1,0x42,0xA5,0x5A,0,0,0,0,0x12,0x34,0x56,0};
  memcpy(b,initial,16);
  for(int i=0;i<4;i++) b[8+i]=seq>>(8*i);
  for(int i=0;i<15;i++) b[15]^=b[i];
}
int main() {
  uint8_t b[16]; uint32_t seq=0;
  packet(b,0x87654321);
  assert(uwbtest::decode(b,16,seq)==uwbtest::Result::valid && seq==0x87654321);
  assert(uwbtest::decode(b,15,seq)==uwbtest::Result::length);
  for(int i=0;i<16;i++) {b[i]^=1;assert(uwbtest::decode(b,16,seq)!=uwbtest::Result::valid);b[i]^=1;}
  // Wrong magic with a repaired XOR is still not this protocol.
  b[0]^=1;b[15]^=1;assert(uwbtest::decode(b,16,seq)==uwbtest::Result::pattern);
  uwbtest::Stats s;
  packet(b,0xffffffff);s.received(b,16,400);
  packet(b,0);s.received(b,16,420);assert(s.gaps==0 && s.passes(2));
  s.received(b,16,380);assert(s.gaps==1 && !s.passes(2));
  s.transportError(20000);packet(b,10);s.received(b,16,410);
  assert(s.gaps==1 && s.io==1 && s.maxUs==20000);
  s={};assert(!s.passes(1));
  for(unsigned i=0;i<500;i++){packet(b,i);s.received(b,16,400);}
  assert(s.passes(500));
  puts("PASS: framing, corruption, sequence wrap/gaps, failure recovery, pass criteria");
}
