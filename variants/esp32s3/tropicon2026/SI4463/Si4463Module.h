#ifndef USE_AIS
#pragma once

#include "mesh/MeshModule.h"
#include "Si446xInterface.h"
#include <SPI.h>

/**
 * @brief Meshtastic module for the SI4463 secondary radio.
 *
 * On TROPICON2026 the constructor accepts explicit SPI pin numbers because
 * both hardware SPI buses are already in use (LoRa on FSPI, display on HSPI).
 * On other targets a hardware SPIClass reference is used.
 */
class Si4463Module : public MeshModule
{
public:
#ifdef TROPICON2026
    Si4463Module(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi);
#else
    Si4463Module(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi);
#endif
    virtual ~Si4463Module();

    // MeshModule implementation
    virtual void setup() override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

private:
    uint8_t _cs, _sdn;
    int8_t _irq;
#ifdef TROPICON2026
    uint8_t _sck, _miso, _mosi;  // soft SPI pins
#else
    SPIClass& _spi;
#endif
    Si446xInterface* _radioIf = nullptr;
};
#endif
