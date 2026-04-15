#include "variant.h"
#include <Arduino.h>
#ifdef USE_AIS
#include "AIS/AISModule.h"
AISModule *aisModule = nullptr;
#else
#include "SI4463/Si4463Module.h"
Si4463Module *si4463Module = nullptr;
#endif

// SI4463 is driven via software (bit-bang) SPI because FSPI is used by LoRa
// and HSPI is used by the NV3007 display.  No SPIClass instance is needed.

void earlyInitVariant()
{
    // Hold SI4463 in shutdown during early boot; lateInitVariant will release it.
    pinMode(SI4463_SDN, OUTPUT);
    digitalWrite(SI4463_SDN, HIGH);
}

void lateInitVariant()
{
#ifdef USE_AIS
    aisModule = new AISModule(SI4463_CS,    // Chip Select  — IO14
                              SI4463_SDN,   // Shutdown     — IO04
                              SI4463_IRQ,   // IRQ          — IO08
                              SI4463_SCK,   // Soft SCK     — IO15
                              SI4463_MISO,  // Soft MISO    — IO17
                              SI4463_MOSI,  // Soft MOSI    — IO16
                              SI4463_GPIO2, // RX_DATA_CLK  — IO05
                              SI4463_GPIO0  // RX_DATA      — IO06
    );
    aisModule->setup();
#else
    si4463Module = new Si4463Module(SI4463_CS,   // Chip Select  — IO14
                                    SI4463_SDN,  // Shutdown     — IO04
                                    SI4463_IRQ,  // IRQ          — IO08
                                    SI4463_SCK,  // Soft SCK     — IO15
                                    SI4463_MISO, // Soft MISO    — IO17
                                    SI4463_MOSI  // Soft MOSI    — IO16
    );
#endif
}
