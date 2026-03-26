#define USE_SX1262

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