#pragma once
#include <stddef.h>
#include <stdint.h>
namespace uwbtest {
constexpr uint8_t address = 0x42;
constexpr size_t frameSize = 16;
enum class Result { valid, length, pattern, checksum };
inline Result decode(const uint8_t* b, size_t size, uint32_t& sequence) {
  if (size != frameSize) return Result::length;
  const uint8_t head[] = {'2','D','K','I',1,address,0xA5,0x5A};
  for (size_t i=0; i<sizeof head; ++i)
    if (b[i] != head[i]) return Result::pattern;
  if (b[12]!=0x12 || b[13]!=0x34 || b[14]!=0x56) return Result::pattern;
  uint8_t x=0;
  for (size_t i=0; i<15; ++i) x^=b[i];
  if (x!=b[15]) return Result::checksum;
  sequence=uint32_t(b[8]) | (uint32_t(b[9])<<8) |
           (uint32_t(b[10])<<16) | (uint32_t(b[11])<<24);
  return Result::valid;
}
struct Stats {
  uint32_t attempts=0, ok=0, io=0, bad=0, gaps=0;
  uint32_t sequence=0, lastUs=0, maxUs=0;
  bool haveSequence=false;
  void transportError(uint32_t us) {
    ++attempts; ++io; haveSequence=false; timing(us);
  }
  void received(const uint8_t* b, size_t size, uint32_t us) {
    ++attempts; timing(us);
    uint32_t next=0;
    if (decode(b,size,next)!=Result::valid) { ++bad; haveSequence=false; return; }
    if (haveSequence && next != uint32_t(sequence+1)) ++gaps;
    sequence=next; haveSequence=true; ++ok;
  }
  void timing(uint32_t us) { lastUs=us; if(us>maxUs) maxUs=us; }
  bool passes(uint32_t required) const {
    return attempts>=required && ok==attempts && io==0 && bad==0 && gaps==0;
  }
};
}
