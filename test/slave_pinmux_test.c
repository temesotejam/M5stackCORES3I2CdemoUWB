#include <stdint.h>
#include <assert.h>
#include <stdio.h>

/* Host register shim. Test the same pin-write helper used by app_main. */
static struct { uint32_t PIO[1][22]; } mock_iocon;
#define IOCON (&mock_iocon)
#define IOCON_PIO_FUNC(x) ((uint32_t)(x)&7u)
#define IOCON_PIO_MODE(x) (((uint32_t)(x)&3u)<<3)
#define IOCON_PIO_DIGIMODE(x) (((uint32_t)(x)&1u)<<7)
#define IOCON_PIO_FILTEROFF(x) (((uint32_t)(x)&1u)<<8)
#define IOCON_PIO_OD(x) (((uint32_t)(x)&1u)<<10)
#include "../type2dk/src/i2c_pins.h"

int main(void) {
    for(unsigned i=0;i<22;i++) mock_iocon.PIO[0][i]=0xdeadbeefu;
    type2dk_i2c_configure_pins();
    /* Expected routing is independently fixed by NXP JN-AN-1252 p.2:
       FUNC2=SWD, FUNC4=PWM, FUNC5=I2C1. This catches the v1-v3 defect. */
    assert((mock_iocon.PIO[0][12]&7u)==5u);
    assert((mock_iocon.PIO[0][13]&7u)==5u);
    assert(mock_iocon.PIO[0][12]==0x595u);
    assert(mock_iocon.PIO[0][13]==0x595u);
    for(unsigned i=0;i<22;i++)
        if(i!=12 && i!=13) assert(mock_iocon.PIO[0][i]==0xdeadbeefu);
    puts("PASS: PIO12/13 use FUNC5 I2C1; UART and other pins untouched");
}
