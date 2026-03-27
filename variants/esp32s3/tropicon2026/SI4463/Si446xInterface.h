#pragma once

#include "RadioInterface.h"
#include "Si446x.h"
#include <SPI.h>

/**
 * @brief RadioInterface implementation for the Si446x radio module.
 * 
 * This class wraps the Si446x library to provide a Meshtastic-compatible interface.
 * While Si4463 is not a LoRa radio, we can still use this interface to send and receive
 * packets through the Meshtastic radio stack or as a secondary telemetry/data link.
 */
class Si446xInterface : public RadioInterface
{
public:
    Si446xInterface(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi);
    virtual ~Si446xInterface();

    // RadioInterface implementation
    virtual bool init() override;
    virtual bool reconfigure() override;
    virtual ErrorCode send(meshtastic_MeshPacket *p) override;
    virtual uint32_t getPacketTime(uint32_t totalPacketLen, bool received = false) override;
    virtual bool sleep() override;
    virtual bool canSleep() override { return true; }

    // Callbacks from the C library
    void handleRxBegin(int16_t rssi);
    void handleRxComplete(uint8_t length, int16_t rssi);
    void handleRxInvalid(int16_t rssi);
    void handleSent();

private:
    uint8_t _cs, _sdn;
    int8_t _irq;
    SPIClass& _spi;
    uint8_t _channel = 0;

    void applySettings();
};
