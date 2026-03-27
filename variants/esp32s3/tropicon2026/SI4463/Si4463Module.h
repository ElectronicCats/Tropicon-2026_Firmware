#pragma once

#include "mesh/MeshModule.h"
#include "Si446xInterface.h"
#include <SPI.h>

/**
 * @brief Meshtastic module for the SI4463 secondary radio.
 * 
 * This module manages the lifecycle of the SI4463 radio module and integrates it
 * into the Meshtastic service as an internal module.
 */
class Si4463Module : public MeshModule
{
public:
    Si4463Module(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi);
    virtual ~Si4463Module();

    // MeshModule implementation
    virtual void setup() override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

private:
    uint8_t _cs, _sdn;
    int8_t _irq;
    SPIClass& _spi;
    Si446xInterface* _radioIf = nullptr;
};
