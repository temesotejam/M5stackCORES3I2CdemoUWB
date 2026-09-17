#pragma once

/* QN9090 PIO12/13 are I2C1 on FUNC5, NOT FUNC4 (PWM0/PWM2).
 * Source: NXP JN-AN-1252 rev 1V2, p.2, section 3.1 pin-function table:
 * https://www.nxp.com/docs/en/application-note/JN-AN-1252.pdf
 * The data sheet's descriptive list omits unused slots; its list order is
 * not a function number. Include QN9090.h before this header on target.
 */
static inline uint32_t type2dk_i2c_pin_config(void) {
    return IOCON_PIO_FUNC(5)|IOCON_PIO_MODE(2)|IOCON_PIO_DIGIMODE(1)
        |IOCON_PIO_FILTEROFF(1)|IOCON_PIO_OD(1);
}
static inline void type2dk_i2c_configure_pins(void) {
    const uint32_t config=type2dk_i2c_pin_config();
    IOCON->PIO[0][12]=config;
    IOCON->PIO[0][13]=config;
}
static inline void type2dk_i2c_observe_pins(void) {
    /* Disable both GPIO outputs before selecting GPIO; no test pulses. */
    GPIO->DIRCLR[0]=(1u<<12)|(1u<<13);
    const uint32_t input=IOCON_PIO_FUNC(0)|IOCON_PIO_MODE(2)
        |IOCON_PIO_DIGIMODE(1)|IOCON_PIO_FILTEROFF(1);
    IOCON->PIO[0][12]=input;
    IOCON->PIO[0][13]=input;
}
