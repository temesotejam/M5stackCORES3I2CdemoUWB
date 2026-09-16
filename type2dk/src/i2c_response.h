#pragma once
/* Shared by target build and host protocol tests; requires QN9090 register names. */
#define ADDRESS 0x42u
static uint8_t frame[16];
static unsigned pos;
static volatile uint32_t reads, writes, irq_count, tx_bytes, deselects, write_bytes;
static volatile uint32_t last_irq_stat;

static void snapshot(void) {
    ++reads;
    frame[0]='2';frame[1]='D';frame[2]='K';frame[3]='I';
    frame[4]=1;frame[5]=ADDRESS;frame[6]=0xA5;frame[7]=0x5A;
    for(unsigned n=0;n<4;n++) frame[8+n]=(uint8_t)(reads>>(8*n));
    frame[12]=0x12;frame[13]=0x34;frame[14]=0x56;
    uint8_t sum=0;for(unsigned n=0;n<15;n++) sum^=frame[n];frame[15]=sum;
}
void I2C1_Handler(void) {
    ++irq_count;
    uint32_t s=I2C1->STAT;last_irq_stat=s;
    if(s&I2C_STAT_SLVDESEL_MASK) {++deselects;I2C1->STAT=I2C_STAT_SLVDESEL_MASK;}
    if(!(s&I2C_STAT_SLVPENDING_MASK)) return;
    unsigned state=(s&I2C_STAT_SLVSTATE_MASK)>>I2C_STAT_SLVSTATE_SHIFT;
    if(state==0) {
        pos=0;
        if(I2C1->SLVDAT&1u) snapshot();else ++writes;
        I2C1->SLVCTL=I2C_SLVCTL_SLVCONTINUE_MASK;
    } else if(state==2) {
        I2C1->SLVDAT=(pos<sizeof frame)?frame[pos++]:0xFF;
        ++tx_bytes;
        I2C1->SLVCTL=I2C_SLVCTL_SLVCONTINUE_MASK;
    } else {
        ++write_bytes;(void)I2C1->SLVDAT;
        I2C1->SLVCTL=I2C_SLVCTL_SLVNACK_MASK;
    }
    __DSB();
}
