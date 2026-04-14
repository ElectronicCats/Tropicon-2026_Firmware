#pragma once

#include "RadioInterface.h"
#include "Si446x.h"
#include <SPI.h>

/**
 * @brief RadioInterface implementation for the Si446x radio module.
 *
 * On TROPICON2026 both hardware SPI buses are occupied (FSPI → LoRa, HSPI → display),
 * so the SI4463 is driven via software (bit-bang) SPI using its dedicated GPIO pins.
 * The constructor accepts explicit SCK/MISO/MOSI pin numbers in that variant.
 * On all other targets a hardware SPIClass reference is used as before.
 */
class Si446xInterface : public RadioInterface
{
public:
#ifdef TROPICON2026
    Si446xInterface(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi);
#else
    Si446xInterface(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi);
#endif
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
#ifdef TROPICON2026
    uint8_t _sck, _miso, _mosi;  // soft SPI pins
#else
    SPIClass& _spi;
#endif
    uint8_t _channel = 0;

    void applySettings();
};
