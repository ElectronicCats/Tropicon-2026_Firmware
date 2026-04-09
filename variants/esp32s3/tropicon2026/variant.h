// ══════════════════════════════════════════════════════════════════════════════
// variant.h — Custom hardware definition for ESP32-S3-WROOM-1-N8
// Board: Tropicon 2026
// Firmware: Meshtastic / PlatformIO
// ══════════════════════════════════════════════════════════════════════════════

#ifndef TROPICON2026
#define TROPICON2026
#endif

// ── SX1262 LoRa — SPI Bus 1 ──────────────────────────────────────────────────
#ifndef USE_SX1262
#define USE_SX1262
#endif

// SPI Bus Pins
#define LORA_SCK       37   // IO37
#define LORA_MISO      36   // IO36
#define LORA_MOSI      35   // IO35
#define LORA_CS        38   // IO38 = NSS

// Control Pins
#define LORA_RESET     47   // IO47 = RST
#define LORA_DIO1      48   // IO48 = IRQ
#define LORA_DIO2      39   // IO39 = BUSY

// Meshtastic Driver Mapping
#define SX126X_CS       LORA_CS
#define SX126X_DIO1     LORA_DIO1
#define SX126X_BUSY     LORA_DIO2
#define SX126X_RESET    LORA_RESET

#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

// ── I2C Bus 1 — AHT30 Temperature & Humidity Sensor ──────────────────────────
#define I2C_SDA        42   // IO42 = SDA
#define I2C_SCL        41   // IO41 = SCL

// ── SI4463 Radio — SPI ───────────────────────────────────────────────────────

#define SI4463_CS            14   // IO14 = CS2
#define SI4463_SDN            4   // IO04 = SDN
#define SI4463_IRQ            8   // IO08 = IRQ
#define SI4463_SCK           15   // IO15 = SCK2
#define SI4463_MOSI          16   // IO16 = MOSI2
#define SI4463_MISO          17   // IO17 = MISO2


// ── Display ER-TFT2.79-1 (controller: NV3007) — SPI Bus 2 (HSPI) ─────────────
//
//   NV3007 pin  │ KiCad label │ ESP32-S3 pin │ SPI function
//   ────────────┼─────────────┼──────────────┼──────────────
//   SDA         │ SDA         │ IO11         │ MOSI (serial data in)
//   SCL         │ SCL         │ IO12         │ SCLK (serial clock)
//   D/CX        │ RS          │ IO09         │ Data/Command select
//   CSX         │ CS1         │ IO10         │ Chip Select (active low)
//   RESX        │ RSTB        │ IO21         │ Reset (active low)
//   TE          │ TE          │ IO13         │ Tearing Effect (frame sync)
//   LED         │ LED         │ IO02         │ Backlight (PWM output)
//
// ── Display ER-TFT2.79-1 (controller: NV3007) — SPI Bus 2 (HSPI) ─────────────
// Using Arduino_GFX with Arduino_NV3007 driver

// === Pines ======================= //
// SPI bus configuration
#define TFT_MOSI                11   // IO11 = SDA (SPI MOSI)
#define TFT_SCLK                12   // IO12 = SCL (SPI SCLK)

// SPI pins for display
#define TFT_DC                  9    // IO09 = D/CX (Data/Command select)
#define TFT_RST                 21   // IO21 = RESX (Reset)
#define TFT_CS                  10   // IO10 = CSX (Chip Select)

// Brightness control
#define TFT_BL                  2    // IO02 = LCD_BRIGHT (PWM output)

#define TFT_MISO                -1 // No usado

//================================== //

#define SPI_FREQUENCY           2000000
#define SPI_READ_FREQUENCY      16000000
#define TFT_HEIGHT              428  // Physical rows  (NV3007_TFTHEIGHT)
#define TFT_WIDTH               168  // Physical columns (NV3007_TFTWIDTH) — confirmed by test code
#define TFT_OFFSET_X1           12
#define TFT_OFFSET_Y1           0
#define TFT_OFFSET_X2           0
#define TFT_OFFSET_Y2           0
#define TFT_ROTATION            1
#define SCREEN_TRANSITION_FRAMERATE 5
#define HAS_SCREEN              1
#define TFT_BLACK               0
#define BRIGHTNESS_DEFAULT      130  // Medium Low Brightness
#define USE_TFTDISPLAY          1

#define USE_POWERSAVE
#define SLEEP_TIME              120

// ── Navigation Buttons ────────────────────────────────────────────────────────
// Primary button (IO40): single-press = advance frame, long-press = open menu,
// long-long press = shutdown. Handled automatically by Meshtastic ButtonThread.
#define BUTTON_PIN              40   // IO40 = UP button (primary Meshtastic button)
#define BUTTON_ACTIVE_LOW       true
#define BUTTON_ACTIVE_PULLUP    true

// Directional pad — 4 buttons routed through TrackballInterruptImpl1.
// This reuses the existing Meshtastic trackball driver, which emits:
//   INPUT_BROKER_UP / DOWN / LEFT / RIGHT / SELECT / SELECT_LONG
// directly into InputBroker — no custom code needed.
//
// ⚠ IO43 is UART0 TX. It floats HIGH during boot (bootloader uses it).
//   Do not use INPUT_PULLUP on this pin; add a 10 kΩ pull-down on the PCB.
#define HAS_TRACKBALL 1
#define TB_UP      40   // IO40 — same pin as BUTTON_PIN → UP / advance
#define TB_DOWN    43   // IO43 — UART TX (pull-down on PCB required)
#define TB_LEFT     1   // IO01 — LEFT
#define TB_RIGHT   18   // IO18 — RIGHT
#define TB_PRESS   40   // IO40 long-press → SELECT (handled by ButtonThread)

// Trackball interrupt edge: directional buttons trigger on FALLING (active-low)
#define TB_DIRECTION FALLING
