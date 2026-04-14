#pragma once

#include "AISDecoder.h"
#include "mesh/MeshModule.h"
#include "Si446x.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include <string>

// Minimal decoded vessel info extracted from a Type-1/2/3 AIS frame
struct AISVesselInfo {
    uint32_t mmsi;       // 30-bit Maritime Mobile Service Identity
    uint8_t  msgType;    // AIS message type (1-27)
    uint8_t  channel;    // 0 = A (161.975 MHz), 1 = B (162.025 MHz)
    uint32_t seenAt_ms;  // millis() timestamp
};

class AISModule : public MeshModule {
public:
    AISModule(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi);
    virtual void setup() override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }

    // Timer ISR callback — reads raw RX bit from the SI4463 SDO/MISO pin
    static void IRAM_ATTR handleRxBit();

    static AISDecoder& getDecoder() { return _decoder; }

    // Frequency hop: pauses the sampling timer to avoid SDO/SPI conflicts
    void switchToChannel(uint8_t channel);

    // Thread-safe accessors for the display renderer
    static uint32_t            getFrameCount();
    static uint8_t             getCurrentChannel();
    static std::vector<AISVesselInfo> getRecentVessels();

private:
    uint8_t _cs, _sdn;
    int8_t  _irq;
    uint8_t _sck, _miso, _mosi;

    static AISDecoder  _decoder;
    static uint8_t     _rxDataPin;
    static int8_t      _rxClkPin;   // Unused (async sampling); int8_t so -1 is valid

    static volatile uint32_t         _frameCount;
    static volatile uint8_t          _currentChannel;
    static std::vector<AISVesselInfo> _recentVessels;
    static SemaphoreHandle_t          _vesselMutex;

    // Bit-reverse helpers for AIS payload extraction (HDLC transmits LSB first)
    static uint8_t  _bitReverse8(uint8_t b);
    static uint8_t  _bitReverse6(uint8_t b);
    static uint8_t  _extractMsgType(const std::vector<uint8_t>& payload);
    static uint32_t _extractMMSI(const std::vector<uint8_t>& payload);

    // Background FreeRTOS task needs access to private members
    friend void aisTask(void *pvParameters);
};
