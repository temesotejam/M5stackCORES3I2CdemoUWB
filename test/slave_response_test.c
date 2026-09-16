#include <stdint.h>
typedef struct { uint32_t STAT,SLVDAT,SLVCTL; } MockI2C;
extern MockI2C mock;
#define I2C1 (&mock)
#define I2C_STAT_SLVDESEL_MASK 0x8000u
#define I2C_STAT_SLVPENDING_MASK 0x100u
#define I2C_STAT_SLVSTATE_MASK 0x600u
#define I2C_STAT_SLVSTATE_SHIFT 9u
#define I2C_SLVCTL_SLVCONTINUE_MASK 1u
#define I2C_SLVCTL_SLVNACK_MASK 2u
#define __DSB() ((void)0)

#include "../type2dk/src/i2c_response.h"
#include <assert.h>
#include <stdio.h>
MockI2C mock;
static void event(unsigned state, unsigned data) {
 mock.STAT=0x100|(state<<9);mock.SLVDAT=data;I2C1_Handler();
}
int main(void) {
 for(unsigned trial=1;trial<=10000;trial++) {
  event(0,0x85);assert(mock.SLVCTL==1);
  uint8_t b[16],x=0;
  for(unsigned n=0;n<16;n++) {event(2,0);b[n]=mock.SLVDAT;x^=b[n];}
  assert(b[0]=='2'&&b[1]=='D'&&b[2]=='K'&&b[3]=='I');
  assert(b[4]==1&&b[5]==0x42&&b[6]==0xA5&&b[7]==0x5A);
  assert(b[12]==0x12&&b[13]==0x34&&b[14]==0x56&&x==0);
  uint32_t seq=b[8]|(b[9]<<8)|(b[10]<<16)|((uint32_t)b[11]<<24);
  assert(seq==trial);
  event(2,0);assert(mock.SLVDAT==0xff);
 }
 event(0,0x84);assert(mock.SLVCTL==1); // scan ACK, no counter increment
 event(1,0x00);assert(mock.SLVCTL==2); // unsupported write NACK
 event(0,0x85);event(2,0);assert(mock.SLVDAT=='2'); // partial read
 event(0,0x85);event(2,0);assert(mock.SLVDAT=='2'); // restart from zero
 assert(reads==10002);
 assert(writes==1 && write_bytes==1 && tx_bytes==170002);
 mock.STAT=I2C_STAT_SLVDESEL_MASK;I2C1_Handler();assert(deselects==1);
 puts("PASS: 10000 frames, XOR, counter, overread, scan, write NACK, restart");
}
