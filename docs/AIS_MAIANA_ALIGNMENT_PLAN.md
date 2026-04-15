# Plan: Align Tropicon AIS Receiver with MAIANA Transponder

## Context

The Tropicon 2026 badge has an SI4463 radio for AIS reception. It's not receiving AIS packets. The MAIANA project (https://github.com/peterantypas/maiana) is a proven, working AIS transponder using the same SI4463 chip. A local clone exists at `/Users/wero1414/Documents/electronicCats/Fw/maiana/`.

**Goal**: Make the Tropicon AIS receiver match MAIANA's proven working configuration as closely as possible.

**Files to modify**:

- `variants/esp32s3/tropicon2026/SI4463/radio_config_ais.h` — Radio register config
- `variants/esp32s3/tropicon2026/AIS/AISDecoder.h` — Decoder header
- `variants/esp32s3/tropicon2026/AIS/AISDecoder.cpp` — Bit-level decoder
- `variants/esp32s3/tropicon2026/AIS/AISModule.cpp` — Module setup & GPIO config

**Reference files (MAIANA, read-only)**:

- `maiana/latest/Firmware/Transponder/Inc/radio_config_si4463.h` — MAIANA's WDS config (two-pass: first pass then `_1` suffix overrides)
- `maiana/latest/Firmware/Transponder/Src/Receiver.cpp` — MAIANA's bit decoder
- `maiana/latest/Firmware/Transponder/Src/RXPacket.cpp` — MAIANA's CRC/byte processing

---

## Step 1: Rewrite radio_config_ais.h to match MAIANA's final register state

MAIANA's config file loads two passes. The second pass (`_1` suffixed defines) overwrites the first. The FINAL register values are what matter. Below is every register that differs between Tropicon's current config and MAIANA's final state.

### 1.1 GLOBAL_CONFIG (property 0x0003)

- **Current**: `0x60` (FIFO_MODE + SEQUENCER_MODE)
- **Target**: `0x20` (FIFO_MODE only, no high-perf sequencer)
- **Why**: SEQUENCER_MODE may interfere with direct-mode RX

### 1.2 GLOBAL_CLK_CFG (property 0x0001)

- **Current**: Not set
- **Target**: Add `0x11, 0x00, 0x01, 0x01, 0x01`
- **Why**: MAIANA enables the 32kHz RC oscillator

### 1.3 INT_CTL_ENABLE (properties 0x0100-0x0103)

- **Current**: `0x11, 0x01, 0x01, 0x00, 0x00` (all interrupts disabled)
- **Target**: `0x11, 0x01, 0x04, 0x00, 0x07, 0x18, 0x01, 0x08` (PH+MODEM+CHIP enabled)
- **Why**: MAIANA enables interrupt groups even in direct mode

### 1.4 FRR_CTL (properties 0x0200-0x0203)

- **Current**: `0x0A, 0x09, 0x00, 0x00` (A=RSSI, B=state)
- **Target**: `0x00, 0x00, 0x00, 0x00` (all disabled)
- **Why**: Match MAIANA. FRR can be queried via commands instead.

### 1.5 Preamble config (properties 0x1000-0x1008)

- **Current**: Only sets PREAMBLE_CONFIG (0x1004) = `0x14`
- **Target**: Full 9-property write: `0x11, 0x10, 0x09, 0x00, 0x08, 0xA0, 0x00, 0x0F, 0x31, 0x00, 0x00, 0x00, 0x00`
  - PREAMBLE_CONFIG = `0x31` (non-standard preamble with extended config)
- **Why**: Different preamble detection mode

### 1.6 Sync word config (properties 0x1100-0x1105)

- **Current**: Not set
- **Target**: `0x11, 0x11, 0x06, 0x00, 0x01, 0xCC, 0xCC, 0x00, 0x00, 0x00`
- **Why**: MAIANA configures sync word (minimal, 1 byte)

### 1.7 Packet handler config (properties 0x1200+)

- **Current**: Only `RF_PKT_CONFIG = 0x11, 0x12, 0x01, 0x06, 0x40`
- **Target**: Add full packet handler setup matching MAIANA (PKT_CRC_CONFIG, PKT_LEN, PKT_FIELD configs, PKT_CRC_SEED). Copy these defines verbatim from MAIANA's `_1` suffix section:
  - `RF_PKT_CRC_CONFIG_7`: `0x11, 0x12, 0x07, 0x00, 0x00, 0x01, 0x08, 0xFF, 0xFF, 0x20, 0x02`
  - `RF_PKT_LEN_12`: `0x11, 0x12, 0x0C, 0x08, 0x00, 0x00, 0x00, 0x30, 0x40, 0x00, 0x30, 0x04, 0x00, 0x00, 0x00, 0x00`
  - `RF_PKT_FIELD_2_CRC_CONFIG_12`: all zeros
  - `RF_PKT_FIELD_5_CRC_CONFIG_12`: all zeros
  - `RF_PKT_RX_FIELD_3_CRC_CONFIG_9`: all zeros
  - `RF_PKT_CRC_SEED_31_24_4`: all zeros

### 1.8 MODEM_MOD_TYPE (property 0x2000)

- **Current**: `0x0B` (MOD_SOURCE=01 direct mode + MOD_TYPE=011 GMSK)
- **Target**: `0x03` (MOD_SOURCE=00 packet handler + MOD_TYPE=011 GMSK)
- **Why**: MOD_SOURCE controls TX source. For RX-only, MAIANA uses 0x03. The demodulated RX_DATA/RX_DATA_CLK are available on GPIO pins regardless.

### 1.9 MODEM_IF_CTRL block (properties 0x2018-0x2023)

- **Current** (MODEM_IF_CTRL): starts at 0x2019, misses 0x2018 and 0x2020-0x2023
  - `0x80, 0x08, 0x02, 0x80, 0x00, 0x70, 0x20`
- **Target**: Full 12-property write starting at 0x2018:
  - `0x01, 0x80, 0x08, 0x02, 0x80, 0x00, 0x70, 0x20, 0x00, 0xE8, 0x00, 0x62`
  - Adds TX_RAMP_DELAY=0x01, DECIMATION_CFG2=0x00, IFPKD_THRESHOLDS=0xE8, BCR_OSR=0x0062
- **Why**: Missing IFPKD_THRESHOLDS (0xE8) may affect IF peak detector behavior

### 1.10 AFC_WAIT (property 0x202D)

- **Current**: `0x36`
- **Target**: `0x62`
- **Why**: MAIANA waits longer before AFC engagement. Too-aggressive AFC on noisy channels can cause lock-on to noise.

### 1.11 AFC_LIMITER (properties 0x2030-0x2031)

- **Current**: `0x02, 0x4E` (590 decimal)
- **Target**: `0x02, 0x13` (531 decimal)
- **Why**: Tighter AFC limiter prevents the AFC from pulling too far off frequency

### 1.12 AGC_CONTROL (property 0x2035)

- **Current**: `0xE2`
- **Target**: `0xE0`
- **Why**: Bit 1 difference. MAIANA uses 0xE0.

### 1.13 FSK4_GAIN1 (property 0x203B)

- **Current**: `0x00`
- **Target**: `0x80`
- **Why**: Enables secondary branch ISI suppression

### 1.14 OOK_BLOPK (property 0x2041)

- **Current**: Not explicitly set
- **Target**: `0x0C`
- **Why**: MAIANA sets this as part of its AGC window block

### 1.15 OOK_MISC (property 0x2043)

- **Current**: `0x03`
- **Target**: `0x23`
- **Why**: Different detector selection for (G)FSK demodulation. Bit 5 enables an additional detector.

### 1.16 RAW_EYE (properties 0x2046-0x2047)

- **Current**: `0x00, 0x62`
- **Target**: `0x00, 0x6A`
- **Why**: Slightly higher eye-open threshold

### 1.17 RSSI_THRESH (property 0x204A)

- **Current**: `0x80` (-6 dBm — way too high, will ignore weak signals!)
- **Target**: `0x46` (-61 dBm)
- **Why**: This is a CRITICAL bug. 0x80 = RSSI threshold of -6 dBm. Real AIS signals from distant ships are typically -90 to -110 dBm. This threshold may be causing the radio to ignore ALL real AIS signals.

### 1.18 RSSI_JUMP_THRESH + RSSI_CONTROL (properties 0x204B-0x204D)

- **Current**: `0x0C, 0x03` (only 2 properties from the MODEM_RAW block)
- **Target**: Full MODEM_RAW block (8 properties starting at 0x2045):
  `0x8F, 0x00, 0x6A, 0x02, 0x00, 0x46, 0x06, 0x23`
  Plus RSSI_CONTROL continuation:
  `0x09, 0x1C, 0x40` (properties 0x204C-0x204E)

### 1.19 DSA (Signal Arrival Detection) (properties 0x205B-0x205F)

- **Current**: Not configured at all
- **Target**: `0x11, 0x20, 0x05, 0x5B, 0x42, 0x04, 0x04, 0x78, 0x20`
- **Why**: MAIANA uses Signal Arrival Detection to improve packet timing

### 1.20 CHANNEL FILTER COEFFICIENTS (properties 0x2100-0x2123) — MOST CRITICAL

- **Current**: dAISy-derived coefficients:
  ```
  0xFF, 0xC4, 0x30, 0x7F, 0xF5, 0xB5, 0xB8, 0xDE, 0x05, 0x17, 0x16, 0x0C
  0x03, 0x00, 0x15, 0xFF, 0x00, 0x00, 0xFF, 0xC4, 0x30, 0x7F, 0xF5, 0xB5
  0xB8, 0xDE, 0x05, 0x17, 0x16, 0x0C, 0x03, 0x00, 0x15, 0xFF, 0x00, 0x00
  ```
- **Target**: MAIANA's final (second-pass) coefficients:
  ```
  0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1, 0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11
  0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00, 0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1
  0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11, 0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00
  ```
- **Why**: Channel filter coefficients shape the IF passband. Wrong coefficients can completely attenuate the AIS signal before the demodulator sees it. This is likely the #1 reason for no reception.

### 1.21 PA_RAMP_DOWN_DELAY (property 0x2205)

- **Current**: Not set
- **Target**: `0x11, 0x22, 0x01, 0x05, 0x28`
- **Why**: MAIANA manually adds this

### 1.22 Address match (properties 0x3000-0x300B)

- **Current**: Not set
- **Target**: All zeros (explicitly disable): `0x11, 0x30, 0x0C, 0x00, 0x00...`

### 1.23 FREQ_CONTROL_VCOCNT_RX_ADJ (property 0x4007)

- **Current**: `0xFE`
- **Target**: `0xFA`
- **Why**: VCO calibration RX adjustment. MAIANA uses 0xFA.

### 1.24 Update RADIO_CONFIGURATION_DATA_ARRAY

Rebuild the configuration array to include all the new defines in the correct order. Follow MAIANA's ordering: power up, GPIO, global, interrupts, FRR, preamble, sync, packet handler, modem, channel filter, PA, synth, match, freq control, start RX, IRCAL.

---

## Step 2: Update GPIO configuration in AISModule::setup()

In `AISModule.cpp`, the `setup()` method configures GPIOs at runtime after the radio config is loaded.

### 2.1 Match MAIANA's GPIO assignment for RX

MAIANA's `Receiver::configureGPIOsForRX()` uses:

```
GPIO0 = 0x00  (no change / default)
GPIO1 = 0x14  (RX_DATA — demodulated bits)
GPIO2 = 0x00  (no change / default)
GPIO3 = 0x11  (RX_DATA_CLK — bit clock)
NIRQ  = 0x00
SDO   = 0x00
```

Current Tropicon uses GPIO0=RX_DATA, GPIO2=RX_DATA_CLK. This is fine if the hardware is wired that way (GPIO0→pin6, GPIO2→pin5). **Do NOT change these unless the PCB traces go to different pins.** The GPIO assignment must match hardware routing.

### 2.2 Remove the runtime XO_TUNE/FC_FRAC sweep (or make it optional)

The current `setup()` spends ~30+ seconds doing a two-pass frequency sweep. While clever, this should be done AFTER confirming basic reception works. For initial debugging:

- Comment out the XO_TUNE override to 0x7F
- Comment out the FC_FRAC coarse and fine sweep
- Leave XO_TUNE at the config default (0x52)
- This removes a variable and lets you confirm the radio config itself is working

### 2.3 Simplify the GPIO0 connectivity test

Keep the test, but if GPIO0 is connected, just set it to RX_DATA (0x14) without the sweep. If not, fall back to SDO.

---

## Step 3: Upgrade HDLC preamble detection in AISDecoder.cpp

### 3.1 Replace 8-bit shift register with 16-bit sliding window

MAIANA uses a 16-bit `mBitWindow` and checks the bottom 12 bits for preamble+flag pattern. This requires seeing AIS preamble bits before the HDLC flag, which dramatically reduces false frame starts from noise.

**Current** (AISDecoder.cpp):

```cpp
_shiftReg = (_shiftReg << 1) | (decodedBit ? 1 : 0);
if (_shiftReg == 0x7E) { ... }  // bare flag only
```

**Target** (match MAIANA's Receiver.cpp:192-198):

```cpp
_bitWindow = (_bitWindow << 1) | (decodedBit ? 1 : 0);

// Require preamble bits before flag (reduces false starts)
if (!_inPacket) {
    if ((_bitWindow & 0x0FFF) == 0b101001111110 ||
        (_bitWindow & 0x0FFF) == 0b010101111110) {
        // Start of frame
    }
} else {
    if ((_bitWindow & 0x00FF) == 0x7E) {
        // End of frame
    }
}
```

### 3.2 Change `_shiftReg` from `uint8_t` to `uint16_t _bitWindow` in AISDecoder.h

### 3.3 On abort (7+ ones) or packet too large, the decoder should fully reset

MAIANA returns `RESTART_RX` which calls `resetBitScanner()` AND re-enters RX mode on the radio. In Tropicon, just reset the decoder state (`_inPacket = false` is already done, but also reset `_bitWindow`, `_consecutiveOnes`, `_currentByte`, `_bitInByte`).

---

## Step 4: Match byte accumulation order (verify correctness)

### 4.1 Verify CRC is correct with current LSB-first accumulation

MAIANA accumulates bits MSB-first (`mRXByte <<= 1; mRXByte |= bit`), then does CRC bit-by-bit in MSB order on the byte.

Tropicon accumulates LSB-first (`_currentByte |= (1 << _bitInByte)`), then does CRC byte-at-a-time reflected.

**Both are mathematically equivalent** — both process bits in reception order. The CRC residue (0xF0B8) should match. No change needed here.

### 4.2 Verify field extraction handles LSB-first storage

The `_extractMsgType()` and `_extractMMSI()` functions do manual bit-reversal which is correct for LSB-first storage. No change needed.

---

## Step 5: Add radio re-entry to RX on abort

### 5.1 After abort or frame end, re-enter RX mode

MAIANA calls `startReceiving()` (which sends START_RX command) after every frame end or abort. This ensures the radio's internal state machine is reset.

In `AISModule.cpp`'s `aisTask()`, after `_decoder.clearMessage()` or when the decoder aborts, call:

```cpp
Si446x_RX(0);  // Re-enter RX on channel 0
```

This may require making Si446x_RX accessible from the task, which it already is (it's an extern C function).

### 5.2 Add a periodic RX re-entry as a safety net

MAIANA reboots hourly because "RX performance goes downhill." A lighter approach: every 60 seconds, if no frames have been received, re-enter RX mode:

```cpp
if (millis() - lastFrameTime > 60000) {
    Si446x_RX(0);
    lastFrameTime = millis();
}
```

---

## Step 6: Improve diagnostic output

### 6.1 Add RSSI to diagnostic prints

After setup, periodically read and print RSSI to verify the radio is seeing RF energy. In a port environment, RSSI should be around -100 to -110 dBm with occasional spikes to -80 dBm when ships transmit. If RSSI is stuck at -134 dBm (minimum), the radio isn't receiving.

### 6.2 Print radio state after setup

After all configuration, print:

- Radio state (should be 0x08 = RX)
- Current RSSI
- All modem status fields
- Verify GPIO pin configuration readback matches expectations

---

## Execution Order

1. **Step 1** first (radio config) — this is 90% likely to fix reception
2. **Step 2** next (GPIO/setup simplification) — removes variables
3. **Step 5** (RX re-entry) — ensures radio stays in RX
4. **Step 3** (preamble detection) — improves noise rejection
5. **Step 4** (verify only, no changes expected)
6. **Step 6** (diagnostics) — helps debug if still not working

## Testing

After each step, build and flash (`pio run -e tropicon2026 -t upload`), then monitor serial (`pio device monitor`). Look for:

- `[AIS] 5s: XXXX bits` — should show ~48000 bits per 5 seconds (9600 bps)
- `flags=` counter increasing — HDLC flags being detected
- `crcOK=` counter increasing — successfully decoded AIS frames
- `rssi=` — should show values above -130 (noise floor), ideally -90 to -110 near a port

If bits are arriving but no flags: radio config issue (filter coefficients, frequency)
If flags arrive but all CRC fail: decoder issue (bit ordering, NRZI, CRC)  
If no bits at all: GPIO routing issue (wrong pin for RX_DATA_CLK or RX_DATA)
