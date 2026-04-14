
#ifndef RADIO_CONFIG_AIS_H_
#define RADIO_CONFIG_AIS_H_

// AIS radio configuration for SI4463
// Crystal: 30.0 MHz  |  Band: VHF (144–175 MHz)  |  OUTDIV = 10
// Modulation: GMSK 9600 bps  |  Channel A: 161.975 MHz, Channel B: 162.025 MHz
//
// FREQ_CONTROL derivation (verified math):
//   Formula:  f_RF = (FC_INTE + 1 + FC_FRAC/2^19) × 2 × f_XO / OUTDIV
//                  = (FC_INTE + 1 + FC_FRAC/524288) × 6 MHz
//   161.975 MHz → N = 161975000/6000000 = 26.99583...
//     FC_INTE = 25  (0x19)
//     FC_FRAC = round(0.99583 × 524288) = 522103  (0x07F777)
//
//   Channel step (50 kHz separation):
//     CHANNEL_STEP = round(50000 × OUTDIV / (2 × f_XO) × 2^19)
//                  = round(50000 × 10 / 60000000 × 524288) = 4369  (0x1111)
//
// NOTE: MODEM filter coefficients should be regenerated with Silicon Labs WDS3
// for optimal GMSK-9600 reception with a 30 MHz XTAL.

#define RF_POWER_UP     0x02, 0x01, 0x00, 0x01, 0xC9, 0xC3, 0x80
#define RF_GPIO_PIN_CFG 0x13, 0x41, 0x41, 0x21, 0x20, 0x67, 0x4B, 0x00
#define GLOBAL_2_0      0x11, 0x00, 0x04, 0x00, 0x52, 0x00, 0x18, 0x30

// MODEM: MOD_TYPE=GMSK(0x03), DATA_RATE≈9600 bps @ 30 MHz
// DATA_RATE = round(9600/30e6 × 2^24) = 0x027525
#define MODEM_2_0 0x11, 0x20, 0x0C, 0x00, \
    0x03,       /* MOD_TYPE = 2GFSK/GMSK     */ \
    0x00,       /* MAP_CONTROL               */ \
    0x07,       /* DSM_CTRL                  */ \
    0x02, 0x75, 0x25, /* DATA_RATE = 0x027525 */ \
    0x00, 0x05,  /* TX_NCO_MODE (MSB..LSB)   */ \
    0xC9, 0xC3, 0x80, 0x00

// FREQ_CONTROL for 161.975 MHz (channel 0 = AIS-A) with 50 kHz channel step
//   Byte layout (SET_PROPERTY 0x40 group, 8 bytes from offset 0x00):
//   [INTE] [FRAC_H] [FRAC_M] [FRAC_L] [STEP_H] [STEP_L] [W_SIZE] [VCOCAL_RX]
#define FREQ_CONTROL_2_0 0x11, 0x40, 0x08, 0x00, \
    0x19,       /* FC_INTE  = 25               */ \
    0x07, 0xF7, 0x77, /* FC_FRAC = 0x07F777    */ \
    0x11, 0x11, /* CHANNEL_STEP = 0x1111 (50 kHz) */ \
    0x20, 0xFE  /* W_SIZE, VCOCAL_RX            */

#define RF_START_RX 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define RF_IRCAL 0x17, 0x56, 0x10, 0xCA, 0xF0
#define RF_IRCAL_1 0x17, 0x13, 0x10, 0xCA, 0xF0

#define RADIO_CONFIGURATION_DATA_ARRAY { \
0x07, RF_POWER_UP, \
0x08, RF_GPIO_PIN_CFG, \
0x08, GLOBAL_2_0, \
0x10, MODEM_2_0, \
0x0C, FREQ_CONTROL_2_0, \
0x08, RF_START_RX, \
0x05, RF_IRCAL, \
0x05, RF_IRCAL_1, \
0x00 \
}

#endif
