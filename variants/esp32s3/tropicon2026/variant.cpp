#include "variant.h"
#include <Arduino.h>
#include <SPI.h>
#include "SI4463/Si4463Module.h"

// Secondary SPI for SI4463 (disabled for display testing)
SPIClass* spi_si4463 = nullptr;
Si4463Module* si4463Module = nullptr;

extern "C" void earlyInitVariant()
{
    // SI4463 disabled: HSPI bus is reserved for the NV3007 display.
    // To re-enable SI4463, move it to software SPI or redesign pin assignments.
    //
    // spi_si4463 = new SPIClass(HSPI);
    // spi_si4463->begin(SI4463_SCK, SI4463_MISO, SI4463_MOSI, SI4463_CS);

    // SDN held high → SI4463 stays in shutdown/reset
    pinMode(SI4463_SDN, OUTPUT);
    digitalWrite(SI4463_SDN, HIGH);
}

extern "C" void lateInitVariant()
{
    // SI4463 disabled — spi_si4463 is nullptr, so module is not created.
    if (spi_si4463)
    {
        si4463Module = new Si4463Module(SI4463_CS, SI4463_SDN, SI4463_IRQ, *spi_si4463);
    }
}
