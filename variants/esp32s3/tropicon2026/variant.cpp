#include "variant.h"
#include <Arduino.h>
#include <SPI.h>
#include "SI4463/Si4463Module.h"

// Secondary SPI for SI4463
SPIClass* spi_si4463 = nullptr;
Si4463Module* si4463Module = nullptr;

extern "C" void earlyInitVariant()
{
    // Initialize SI4463 SPI bus
    spi_si4463 = new SPIClass(HSPI); // Using HSPI for SI4463
    spi_si4463->begin(SI4463_SCK, SI4463_MISO, SI4463_MOSI, SI4463_CS);
    
    // SDN pin must be held high then low to reset the radio
    pinMode(SI4463_SDN, OUTPUT);
    digitalWrite(SI4463_SDN, HIGH);
}

extern "C" void lateInitVariant()
{
    // Instantiate the SI4463 module
    // This will register itself in the MeshModule chain
    if (spi_si4463)
    {
        si4463Module = new Si4463Module(SI4463_CS, SI4463_SDN, SI4463_IRQ, *spi_si4463);
    }
}
