#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "variant.h"
#include <stdint.h>

// =============================================
// Tropicon2026 — ESP32-S3-WROOM-1-N8
// Primera prueba: solo SX1262 + Bluetooth
// =============================================

// UART
#define TX  43
#define RX  44

// ── I2C Bus 1 — AHT30 Temperature & Humidity Sensor ──────────────────────────
#define SDA I2C_SDA
#define SCL I2C_SCL

// SPI por defecto (SPI0 — sin usar en esta prueba)
#define SS    SX126X_CS
#define MOSI  LORA_MOSI
#define MISO  LORA_MISO
#define SCK   LORA_SCK

// LED integrado (ajusta si tu PCB tiene uno, o deja -1)
#define LED_BUILTIN  48

#endif /* Pins_Arduino_h */