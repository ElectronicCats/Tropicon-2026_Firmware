
#ifndef RADIO_CONFIG_AIS_H_
#define RADIO_CONFIG_AIS_H_

// AIS radio configuration for SI4463 — aligned with MAIANA transponder
// Reference: https://github.com/peterantypas/maiana (proven working AIS)
// Crystal: 30.0 MHz  |  Frequency: 161.975 / 162.025 MHz
// Modulation: 2GFSK (GMSK) 9600 bps  |  Direct mode RX on GPIO0/GPIO2
//
// MAIANA uses a two-pass WDS config. Values below are the FINAL state
// (second-pass overrides applied).

// Power-up: 30 MHz TCXO, no patch
#define RF_POWER_UP 0x02, 0x01, 0x00, 0x01, 0xC9, 0xC3, 0x80

// GPIO0=RX_DATA(0x14), GPIO1=tristate(0x00), GPIO2=RX_DATA_CLK(0x11),
// GPIO3=tristate(0x00), NIRQ=0x00, SDO=0x00
// Keeps SPI free while receiving (RX_DATA on GPIO0 not SDO)
#define RF_GPIO_PIN_CFG 0x13, 0x14, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00

// GLOBAL: XO_TUNE=0x52 (default, tunable at runtime)
#define RF_GLOBAL_XO_TUNE 0x11, 0x00, 0x01, 0x00, 0x52

// GLOBAL_CLK_CFG: enable 32kHz RC oscillator (MAIANA)
#define RF_GLOBAL_CLK_CFG 0x11, 0x00, 0x01, 0x01, 0x01

// GLOBAL_CONFIG = 0x20 (FIFO mode only, NO sequencer mode)
// MAIANA uses 0x20; our old 0x60 had SEQUENCER_MODE which may interfere with direct RX
#define RF_GLOBAL_CONFIG 0x11, 0x00, 0x01, 0x03, 0x20

// Interrupts: PH+MODEM+CHIP enabled (MAIANA)
#define RF_INT_CTL 0x11, 0x01, 0x04, 0x00, 0x07, 0x18, 0x01, 0x08

// FRR: A=RSSI, B=state (needed for diagnostics on Tropicon)
#define RF_FRR_CTL 0x11, 0x02, 0x04, 0x00, 0x0A, 0x09, 0x00, 0x00

// Preamble: full 9-property config (MAIANA)
#define RF_PREAMBLE 0x11, 0x10, 0x09, 0x00, 0x08, 0xA0, 0x00, 0x0F, 0x31, 0x00, 0x00, 0x00, 0x00

// Sync word config (MAIANA)
#define RF_SYNC_CONFIG 0x11, 0x11, 0x06, 0x00, 0x01, 0xCC, 0xCC, 0x00, 0x00, 0x00

// Packet handler (MAIANA) — not used in direct mode but configures internal state
#define RF_PKT_CRC_CONFIG 0x11, 0x12, 0x07, 0x00, 0x00, 0x01, 0x08, 0xFF, 0xFF, 0x20, 0x02
#define RF_PKT_LEN 0x11, 0x12, 0x0C, 0x08, 0x00, 0x00, 0x00, 0x30, 0x40, 0x00, 0x30, 0x04, 0x00, 0x00, 0x00, 0x00
#define RF_PKT_FIELD_2_CRC 0x11, 0x12, 0x0C, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define RF_PKT_FIELD_5_CRC 0x11, 0x12, 0x0C, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define RF_PKT_RX_FIELD_3 0x11, 0x12, 0x09, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define RF_PKT_CRC_SEED 0x11, 0x12, 0x04, 0x36, 0x00, 0x00, 0x00, 0x00

// ── MODEM (MAIANA second-pass final values) ────────────────────────────────
// MOD_TYPE=0x03: MOD_SOURCE=00 (packet handler), MOD_TYPE=011 (2GFSK)
// DATA_RATE=0x05DC00, TX_NCO_MODE: TXOSR=20x, NCOMOD=30 MHz
#define MODEM_MOD_TYPE                                                                                                           \
    0x11, 0x20, 0x0C, 0x00, 0x03, /* MOD_TYPE                          */                                                        \
        0x00,                     /* MAP_CONTROL                       */                                                        \
        0x07,                     /* DSM_CTRL                          */                                                        \
        0x05, 0xDC, 0x00,         /* DATA_RATE                         */                                                        \
        0x05,                     /* TX_NCO_MODE                       */                                                        \
        0xC9, 0xC3, 0x80,         /* NCOMOD = 30 MHz                   */                                                        \
        0x00, 0x01                /* FREQ_DEV[16:8]                    */

// FREQ_DEV[7:0] = 0xF7 → 2400 Hz GMSK deviation
#define MODEM_FREQ_DEV 0x11, 0x20, 0x01, 0x0C, 0xF7

// TX_RAMP_DELAY + IF control + decimation + IFPKD + BCR_OSR (0x2018-0x2023)
// MAIANA final: includes TX_RAMP_DELAY=0x01, MDM_CTRL=0x80, IF_CONTROL=0x08,
// IFPKD_THRESHOLDS=0xE8, BCR_OSR=0x0062
#define MODEM_TX_RAMP_IF_BCR                                                                                                     \
    0x11, 0x20, 0x0C, 0x18, 0x01, /* TX_RAMP_DELAY                     */                                                        \
        0x80,                     /* MDM_CTRL                          */                                                        \
        0x08,                     /* IF_CONTROL                        */                                                        \
        0x02, 0x80, 0x00,         /* IF_FREQ                           */                                                        \
        0x70, 0x20,               /* DECIMATION_CFG1, CFG0             */                                                        \
        0x00,                     /* DECIMATION_CFG2                   */                                                        \
        0xE8,                     /* IFPKD_THRESHOLDS (was missing!)   */                                                        \
        0x00, 0x62                /* BCR_OSR                           */

// BCR NCO + GAIN + GEAR + MISC + AFC (0x2024-0x202F)
#define MODEM_BCR_AFC                                                                                                            \
    0x11, 0x20, 0x0C, 0x24, 0x05, 0x3E, 0x2D, /* BCR_NCO_OFFSET                    */                                            \
        0x02, 0x9D,                           /* BCR_GAIN                          */                                            \
        0x00, 0xC2,                           /* BCR_GEAR, BCR_MISC1               */                                            \
        0x00,                                 /* BCR_MISC0                         */                                            \
        0x54,                                 /* AFC_GEAR                          */                                            \
        0x62,                                 /* AFC_WAIT (MAIANA: 0x62, was 0x36) */                                            \
        0x81, 0x01                            /* AFC_GAIN                          */

// AFC_LIMITER + AFC_MISC (0x2030-0x2032)
#define MODEM_AFC_LIMITER 0x11, 0x20, 0x03, 0x30, 0x02, 0x13, 0x80

// AGC_CONTROL (0x2035) — MAIANA uses 0xE0
#define MODEM_AGC_CONTROL 0x11, 0x20, 0x01, 0x35, 0xE0

// AGC_WINDOW + FSK4 + OOK (0x2038-0x2043) — MAIANA final
// FSK4_GAIN1=0x80 (enables secondary ISI suppression)
// OOK_BLOPK=0x0C, OOK_CNT1=0x84, OOK_MISC=0x23 (extra detector)
#define MODEM_AGC_WINDOW                                                                                                         \
    0x11, 0x20, 0x0C, 0x38, 0x11, 0x15, 0x15, /* AGC_WINDOW, RFPD_DECAY, IFPD_DECAY */                                           \
        0x80,                                 /* FSK4_GAIN1 (MAIANA: 0x80)         */                                            \
        0x1A, 0x20, 0x00, 0x00,               /* FSK4_GAIN0, TH1, TH0, MAP        */                                             \
        0x28,                                 /* OOK_PDTC                          */                                            \
        0x0C,                                 /* OOK_BLOPK (MAIANA)                */                                            \
        0x84,                                 /* OOK_CNT1                          */                                            \
        0x23                                  /* OOK_MISC (MAIANA: 0x23)           */

// RAW_CONTROL + RAW_EYE + ANT_DIV + RSSI_THRESH (0x2045-0x204C)
// RSSI_THRESH=0x46 (~-61 dBm) — CRITICAL FIX! Was 0x80 (-6 dBm)
#define MODEM_RAW                                                                                                                \
    0x11, 0x20, 0x08, 0x45, 0x8F, /* RAW_CONTROL                       */                                                        \
        0x00, 0x6A,               /* RAW_EYE (MAIANA: 0x006A)          */                                                        \
        0x02, 0x00,               /* ANT_DIV_MODE, CONTROL             */                                                        \
        0x46,                     /* RSSI_THRESH (-61 dBm, was 0x80!)  */                                                        \
        0x06,                     /* RSSI_JUMP_THRESH                  */                                                        \
        0x23                      /* RSSI_CONTROL                      */

// RSSI_CONTROL continuation (0x204C-0x204E)
#define MODEM_RSSI_CTL 0x11, 0x20, 0x03, 0x4C, 0x09, 0x1C, 0x40

// RAW_SEARCH2 + CLKGEN_BAND (0x2050-0x2051)
#define MODEM_RAW_SEARCH 0x11, 0x20, 0x02, 0x50, 0x94, 0x0D

// SPIKE_DET + ONE_SHOT_AFC (0x2054-0x2055)
#define MODEM_SPIKE_DET 0x11, 0x20, 0x02, 0x54, 0x03, 0x07

// RSSI_MUTE (0x2057)
#define MODEM_RSSI_MUTE 0x11, 0x20, 0x01, 0x57, 0x00

// DSA (Signal Arrival Detection) (0x205B-0x205F) — MAIANA
#define MODEM_DSA 0x11, 0x20, 0x05, 0x5B, 0x42, 0x04, 0x04, 0x78, 0x20

// ── CHANNEL FILTER (MAIANA second-pass final coefficients) ─────────────────
#define MODEM_CHFLT_1 0x11, 0x21, 0x0C, 0x00, 0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1, 0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11

#define MODEM_CHFLT_2 0x11, 0x21, 0x0C, 0x0C, 0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00, 0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1

#define MODEM_CHFLT_3 0x11, 0x21, 0x0C, 0x18, 0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11, 0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00

// ── PA ─────────────────────────────────────────────────────────────────────
#define RF_PA_TC 0x11, 0x22, 0x01, 0x03, 0x1F
#define RF_PA_RAMP_DOWN 0x11, 0x22, 0x01, 0x05, 0x28

// ── SYNTH ──────────────────────────────────────────────────────────────────
#define SYNTH_PFDCP 0x11, 0x23, 0x07, 0x00, 0x2C, 0x0E, 0x0B, 0x04, 0x0C, 0x73, 0x03

// ── ADDRESS MATCH (all disabled) ───────────────────────────────────────────
#define RF_MATCH_DISABLE 0x11, 0x30, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

// ── FREQ CONTROL ───────────────────────────────────────────────────────────
// 161.975 MHz base, 50 kHz channel step, VCOCNT_RX_ADJ=0xFA (MAIANA)
#define FREQ_CONTROL_AIS                                                                                                         \
    0x11, 0x40, 0x08, 0x00, 0x40, /* FC_INTE  = 64                     */                                                        \
        0x06, 0x51, 0xEC,         /* FC_FRAC  = 0x0651EC = 414188      */                                                        \
        0x28, 0xF6,               /* CHANNEL_STEP = 50 kHz             */                                                        \
        0x20, 0xFA                /* W_SIZE, VCOCNT_RX_ADJ (MAIANA)    */

#define RF_START_RX 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#define RF_IRCAL 0x17, 0x56, 0x10, 0xCA, 0xF0
#define RF_IRCAL_1 0x17, 0x13, 0x10, 0xCA, 0xF0

#define RADIO_CONFIGURATION_DATA_ARRAY                                                                                           \
    {                                                                                                                            \
        0x07, RF_POWER_UP, 0x08, RF_GPIO_PIN_CFG, 0x05, RF_GLOBAL_XO_TUNE, 0x05, RF_GLOBAL_CLK_CFG, 0x05, RF_GLOBAL_CONFIG,      \
            0x08, RF_INT_CTL, 0x08, RF_FRR_CTL, 0x0D, RF_PREAMBLE, 0x0A, RF_SYNC_CONFIG, 0x0B, RF_PKT_CRC_CONFIG, 0x10,          \
            RF_PKT_LEN, 0x10, RF_PKT_FIELD_2_CRC, 0x10, RF_PKT_FIELD_5_CRC, 0x0D, RF_PKT_RX_FIELD_3, 0x08, RF_PKT_CRC_SEED,      \
            0x10, MODEM_MOD_TYPE, 0x05, MODEM_FREQ_DEV, 0x10, MODEM_TX_RAMP_IF_BCR, 0x10, MODEM_BCR_AFC, 0x07,                   \
            MODEM_AFC_LIMITER, 0x05, MODEM_AGC_CONTROL, 0x10, MODEM_AGC_WINDOW, 0x0C, MODEM_RAW, 0x07, MODEM_RSSI_CTL, 0x06,     \
            MODEM_RAW_SEARCH, 0x06, MODEM_SPIKE_DET, 0x05, MODEM_RSSI_MUTE, 0x09, MODEM_DSA, 0x10, MODEM_CHFLT_1, 0x10,          \
            MODEM_CHFLT_2, 0x10, MODEM_CHFLT_3, 0x05, RF_PA_TC, 0x05, RF_PA_RAMP_DOWN, 0x0B, SYNTH_PFDCP, 0x10,                  \
            RF_MATCH_DISABLE, 0x0C, FREQ_CONTROL_AIS, 0x08, RF_START_RX, 0x05, RF_IRCAL, 0x05, RF_IRCAL_1, 0x00                  \
    }

#endif
