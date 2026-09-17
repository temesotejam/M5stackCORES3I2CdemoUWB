/* Type2DK I2C diagnostic v5: QN9090 USART0 -> onboard FT230X -> USB.
 * Own code; requires the user's QN9090 SDK headers to build.
 * I2C test format is unchanged from v1. No UWB, BLE or flash writes.
 */
#include "QN9090.h"
#include "i2c_response.h"
#include "i2c_pins.h"
static uint32_t tick_ms, beat;
static int uart_ready, i2c_ready;
static int observe_lines;
static uint32_t line_samples, scl_low, sda_low, scl_changes, sda_changes, last_pins;

static void sample_lines(void) {
    if(!observe_lines) return;
    const uint32_t pins=GPIO->PIN[0];
    if(line_samples) {
        if((pins^last_pins)&(1u<<12)) ++scl_changes;
        if((pins^last_pins)&(1u<<13)) ++sda_changes;
    }
    if(!(pins&(1u<<12))) ++scl_low;
    if(!(pins&(1u<<13))) ++sda_low;
    last_pins=pins;
    ++line_samples;
}

static void poll_tick(void) {
    if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) ++tick_ms;
    sample_lines();
}
static void wait_ms(uint32_t delay) {
    uint32_t start=tick_ms;
    while((uint32_t)(tick_ms-start)<delay) poll_tick();
}
static void putch(char c) {
    /* Bound the wait so a UART fault never permanently prevents I2C startup.
       I2C interrupts remain enabled during ordinary log transmission. */
    for(unsigned n=0;n<100000u;n++) {
        poll_tick();
        if(USART0->FIFOSTAT & USART_FIFOSTAT_TXNOTFULL_MASK) {
            USART0->FIFOWR=(uint8_t)c; return;
        }
    }
}
static void puts_uart(const char *s) { while(*s) putch(*s++); }
static void dec(uint32_t n) {
    char b[10]; unsigned k=0;
    do { b[k++]=(char)('0'+n%10u); n/=10u; } while(n);
    while(k) putch(b[--k]);
}
static void hex(uint32_t n) {
    puts_uart("0x");
    for(int shift=28;shift>=0;shift-=4) putch("0123456789ABCDEF"[(n>>shift)&15u]);
}
static void uart_init(void) {
    SYSCON->USARTCLKSEL=0; /* OSC32CLKSEL below selects 32 MHz FRO. */
    SYSCON->AHBCLKCTRLSET[1]=SYSCON_AHBCLKCTRL1_USART0_MASK;
    SYSCON->PRESETCTRLSET[1]=SYSCON_PRESETCTRL1_USART0_RST_MASK;
    while(!(SYSCON->PRESETCTRL[1]&SYSCON_PRESETCTRL1_USART0_RST_MASK)) {}
    SYSCON->PRESETCTRLCLR[1]=SYSCON_PRESETCTRL1_USART0_RST_MASK;
    while(SYSCON->PRESETCTRL[1]&SYSCON_PRESETCTRL1_USART0_RST_MASK) {}
    const uint32_t config=IOCON_PIO_FUNC(2)|IOCON_PIO_MODE(0)
        |IOCON_PIO_DIGIMODE(1)|IOCON_PIO_FILTEROFF(1);
    IOCON->PIO[0][8]=config; /* USART0 TX to FT230X RX */
    IOCON->PIO[0][9]=config; /* USART0 RX; no commands required */
    FLEXCOMM0->PSELID=FLEXCOMM_PSELID_PERSEL(1);
    USART0->CFG=0;
    USART0->CTL=0;
    USART0->FIFOCFG=USART_FIFOCFG_EMPTYTX_MASK|USART_FIFOCFG_ENABLETX_MASK;
    /* Same integer baud search as the SDK, FRO32M nominal. */
    uint32_t best_diff=0xffffffffu,best_osr=15,best_brg=0;
    for(int osr=15;osr>=8;--osr) {
        uint32_t brg=(((32000000u*10u)/((uint32_t)(osr+1)*115200u))-5u)/10u;
        uint32_t baud=32000000u/((uint32_t)(osr+1)*(brg+1));
        uint32_t diff=baud>115200u?baud-115200u:115200u-baud;
        if(diff<best_diff) {best_diff=diff;best_osr=(uint32_t)osr;best_brg=brg;}
    }
    USART0->OSR=best_osr; USART0->BRG=best_brg;
    USART0->CFG=USART_CFG_DATALEN(1)|USART_CFG_ENABLE_MASK; /* 8N1; no CTS */
    uart_ready=1;
}
static void registers(void) {
    puts_uart("REG,pio12=");hex(IOCON->PIO[0][12]);
    puts_uart(",pio13=");hex(IOCON->PIO[0][13]);
    puts_uart(",pselid=");hex(FLEXCOMM3->PSELID);
    puts_uart(",cfg=");hex(I2C1->CFG);
    puts_uart(",addr0=");hex(I2C1->SLVADR[0]);
    puts_uart(",id=");hex(I2C1->ID);
    puts_uart(",inten=");hex(I2C1->INTENSET);
    puts_uart(",intstat=");hex(I2C1->INTSTAT);
    puts_uart(",i2c_clksel=");hex(SYSCON->I2CCLKSEL);
    puts_uart(",osc32_clksel=");hex(SYSCON->OSC32CLKSEL);
    puts_uart(",gpio_clock_gate=");hex(SYSCON->AHBCLKCTRL[0]);
    puts_uart(",gpio_reset=");hex(SYSCON->PRESETCTRL[0]);
    puts_uart(",gpio_dir=");hex(GPIO->DIR[0]);
    puts_uart(",clkdiv=");dec(I2C1->CLKDIV);
    puts_uart(",clock_gate=");hex(SYSCON->AHBCLKCTRL[1]);
    puts_uart(",reset=");hex(SYSCON->PRESETCTRL[1]);
    puts_uart(",retention=");hex(SYSCON->RETENTIONCTRL);
    puts_uart(",async_bridge=");hex(SYSCON->ASYNCAPBCTRL);
    puts_uart(",async_clock=");hex(ASYNC_SYSCON->ASYNCAPBCLKSELA);
    puts_uart(",uart_cfg=");hex(USART0->CFG);
    puts_uart(",uart_fifo=");hex(USART0->FIFOSTAT);
    puts_uart(",vtor=");hex(SCB->VTOR);
    puts_uart(",primask=");dec(__get_PRIMASK());
    puts_uart(",basepri=");dec(__get_BASEPRI());
    puts_uart(",faultmask=");dec(__get_FAULTMASK());
    puts_uart("\r\n");
}
static void line_report(const char *mode) {
    /* Snapshot before printing: logging itself calls poll_tick. Counts are
       sampled observations, NOT complete edge counts or clock measurements. */
    const uint32_t samples=line_samples, cl=scl_low, dl=sda_low;
    const uint32_t cc=scl_changes, dc=sda_changes, pins=GPIO->PIN[0];
    line_samples=scl_low=sda_low=scl_changes=sda_changes=0;
    puts_uart("LINES,mode=");puts_uart(mode);
    puts_uart(",scl=");dec((pins>>12)&1u);
    puts_uart(",sda=");dec((pins>>13)&1u);
    puts_uart(",samples=");dec(samples);
    puts_uart(",scl_low=");dec(cl);puts_uart(",sda_low=");dec(dl);
    puts_uart(",scl_changes=");dec(cc);puts_uart(",sda_changes=");dec(dc);
    puts_uart("\r\n");
}
static void checks(void) {
    const uint32_t config=type2dk_i2c_pin_config();
    const uint32_t mask=I2C_INTENSET_SLVPENDINGEN_MASK|I2C_INTENSET_SLVDESELEN_MASK;
    puts_uart("CHECK,pins=");dec(IOCON->PIO[0][12]==config && IOCON->PIO[0][13]==config);
    puts_uart(",slave=");dec(I2C1->CFG==I2C_CFG_SLVEN_MASK);
    puts_uart(",address=");dec(I2C1->SLVADR[0]==(ADDRESS<<1));
    puts_uart(",clock_gate=");dec((SYSCON->AHBCLKCTRL[1]&SYSCON_AHBCLKCTRL1_I2C1_MASK)!=0);
    puts_uart(",reset_released=");dec((SYSCON->PRESETCTRL[1]&SYSCON_PRESETCTRL1_I2C1_RST_MASK)==0);
    puts_uart(",irq_enable=");dec((I2C1->INTENSET&mask)==mask);
    puts_uart(",nvic_enable=");dec((NVIC->ISER[0]&(1u<<FLEXCOMM3_IRQn))!=0);
    puts_uart(",unmasked=");dec(!__get_PRIMASK() && !__get_BASEPRI() && !__get_FAULTMASK());
    puts_uart(",psel_match=");dec((FLEXCOMM3->PSELID&FLEXCOMM_PSELID_PERSEL_MASK)==3);
    puts_uart("\r\n");
}
static void heartbeat(void) {
    /* Aligned 32-bit reads are atomic, but counters are independently sampled;
       this log is for activity diagnosis, not a transactional frame audit. */
    puts_uart("STATE,v=5,beat=");dec(++beat);
    puts_uart(",ready=");dec((uint32_t)i2c_ready);
    puts_uart(",irq=");dec(irq_count);
    puts_uart(",read_addr=");dec(reads);
    puts_uart(",write_addr=");dec(writes);
    puts_uart(",tx_bytes=");dec(tx_bytes);
    puts_uart(",write_bytes=");dec(write_bytes);
    puts_uart(",deselect=");dec(deselects);
    puts_uart(",stat=");hex(I2C1->STAT);
    puts_uart(",last_irq=");hex(last_irq_stat);
    puts_uart(",irq_pending=");dec(NVIC_GetPendingIRQ(FLEXCOMM3_IRQn));
    puts_uart("\r\n");
}
void app_fault(void) {
    if(uart_ready) {
        puts_uart("FAULT,exception=");dec(__get_IPSR());
        puts_uart(",cfsr=");hex(SCB->CFSR);puts_uart(",hfsr=");hex(SCB->HFSR);
        puts_uart("\r\n");
    }
    for(;;) __NOP();
}
void app_main(void) {
    /* Match SDK CLOCK_EnableClock(kCLOCK_Fro32M) before selecting FRO32M. */
    PMC->FRO192M |= PMC_FRO192M_DIVSEL(1u << 1);
    SYSCON->MAINCLKSEL=3;SYSCON->AHBCLKDIV=0;
    SYSCON->OSC32CLKSEL&=~SYSCON_OSC32CLKSEL_SEL32MHZ_MASK;
    /* Retain the SDK-style APB bridge setup from v4. This is not evidence
       that USART0 is on this bridge or that v3 fixed the earlier silence. */
    SYSCON->ASYNCAPBCTRL |= SYSCON_ASYNCAPBCTRL_ENABLE_MASK;
    ASYNC_SYSCON->ASYNCAPBCLKSELA=ASYNC_SYSCON_ASYNCAPBCLKSELA_SEL(0);
    __DSB();
    SYSCON->AHBCLKCTRLSET[0]=SYSCON_AHBCLKCTRL0_IOCON_MASK|SYSCON_AHBCLKCTRL0_GPIO_MASK;
    SYSCON->SYSTICKCLKDIV=0;
    SysTick->LOAD=31999;SysTick->VAL=0;
    SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_ENABLE_Msk; /* no IRQ */
    uart_init();
    puts_uart("BOOT,2DK_I2C_DIAG_V5,baud=115200,format=8N1,reset_cause=");hex(PMC->RESETCAUSE);puts_uart("\r\n");
    puts_uart("BOOT,waiting_before_SWD_to_I2C\r\n");
    wait_ms(2000);
    /* First observe the physical pins as GPIO inputs. No pin drives Low or
       High in this phase, and there is no I2C slave to ACK until INIT below.
       With CoreS3 already running at 100 kHz this separates signal delivery
       from the FUNC5 peripheral route. */
    type2dk_i2c_observe_pins();
    observe_lines=1;
    puts_uart("OBSERVE,GPIO_INPUT,seconds=3,no_ACK_expected\r\n");
    for(unsigned n=0;n<3;n++) {wait_ms(1000);line_report("GPIO_INPUT");}
    observe_lines=0;
    puts_uart("INIT,I2C1,addr=0x42,SCL=PIO12,SDA=PIO13,FUNC=5\r\n");
    SYSCON->I2CCLKSEL=0;
    SYSCON->AHBCLKCTRLSET[1]=SYSCON_AHBCLKCTRL1_I2C1_MASK;
    SYSCON->PRESETCTRLSET[1]=SYSCON_PRESETCTRL1_I2C1_RST_MASK;
    while(!(SYSCON->PRESETCTRL[1]&SYSCON_PRESETCTRL1_I2C1_RST_MASK)) {}
    SYSCON->PRESETCTRLCLR[1]=SYSCON_PRESETCTRL1_I2C1_RST_MASK;
    while(SYSCON->PRESETCTRL[1]&SYSCON_PRESETCTRL1_I2C1_RST_MASK) {}
    const uint32_t config=type2dk_i2c_pin_config();
    type2dk_i2c_configure_pins();
    FLEXCOMM3->PSELID=FLEXCOMM_PSELID_PERSEL(3);
    I2C1->CFG=0;I2C1->CLKDIV=8;I2C1->SLVADR[0]=ADDRESS<<1;
    for(unsigned n=1;n<4;n++) I2C1->SLVADR[n]=1;
    I2C1->SLVQUAL0=0;I2C1->STAT=I2C_STAT_SLVDESEL_MASK;
    I2C1->INTENSET=I2C_INTENSET_SLVPENDINGEN_MASK|I2C_INTENSET_SLVDESELEN_MASK;
    NVIC_SetPriority(FLEXCOMM3_IRQn,1);NVIC_ClearPendingIRQ(FLEXCOMM3_IRQn);
    NVIC_EnableIRQ(FLEXCOMM3_IRQn);I2C1->CFG=I2C_CFG_SLVEN_MASK;
    i2c_ready=(IOCON->PIO[0][12]==config && IOCON->PIO[0][13]==config
        && (FLEXCOMM3->PSELID&FLEXCOMM_PSELID_PERSEL_MASK)==3
        && (I2C1->CFG&I2C_CFG_SLVEN_MASK)!=0 && I2C1->SLVADR[0]==(ADDRESS<<1));
    __DSB();__enable_irq();
    /* Preserve v4's ready calculation for comparison. A PSELID mismatch
       alone is NOT established proof that the I2C slave is disabled:
       QN9090.h leaves 0xFF8 reserved in I2C_Type, while the generic
       FLEXCOMM driver accesses it. Do not silently turn ready into 1. */
    puts_uart("INFO,ready_uses_v4_checks,PSELID_expectation_unconfirmed\r\n");
    checks();registers();
    line_samples=scl_low=sda_low=scl_changes=sda_changes=0;
    observe_lines=1;
    for(;;) {
        heartbeat();wait_ms(1000);line_report("I2C1_FUNC5");
        if(beat%5u==0) {checks();registers();}
    }
}
