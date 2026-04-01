#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "variant.h"
#include <stdint.h>

// =============================================
// Tropicon2026 — ESP32-S3-WROOM-1-N8
// Third test: SX1262 + AHT10 + SI4463
// =============================================

// UART
#define TX  43
#define RX  44

// ── I2C Bus 1 — AHT30 Temperature & Humidity Sensor ──────────────────────────
#define SDA I2C_SDA
#define SCL I2C_SCL

// ── SPI Bus 0 — SX1262 LoRa ──────────────────────────────────────────────────
#define SS    SX126X_CS
#define MOSI  LORA_MOSI
#define MISO  LORA_MISO
#define SCK   LORA_SCK

#endif /* Pins_Arduino_h */