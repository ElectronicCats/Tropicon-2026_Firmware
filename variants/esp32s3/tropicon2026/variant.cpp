#include "variant.h"
#include <Arduino.h>
#include "SI4463/Si4463Module.h"

// SI4463 is driven via software (bit-bang) SPI because FSPI is used by LoRa
// and HSPI is used by the NV3007 display.  No SPIClass instance is needed.
Si4463Module* si4463Module = nullptr;

extern "C" void earlyInitVariant()
{
    // Hold SI4463 in shutdown during early boot; lateInitVariant will release it.
    pinMode(SI4463_SDN, OUTPUT);
    digitalWrite(SI4463_SDN, HIGH);
}

extern "C" void lateInitVariant()
{
    si4463Module = new Si4463Module(
        SI4463_CS,   // Chip Select  — IO14
        SI4463_SDN,  // Shutdown     — IO04
        SI4463_IRQ,  // IRQ          — IO08
        SI4463_SCK,  // Soft SCK     — IO15
        SI4463_MISO, // Soft MISO    — IO17
        SI4463_MOSI  // Soft MOSI    — IO16
    );
}
