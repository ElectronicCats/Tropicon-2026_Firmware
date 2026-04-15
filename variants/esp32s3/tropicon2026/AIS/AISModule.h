#pragma once

#include "AISDecoder.h"
#include "Si446x.h"
#include "mesh/MeshModule.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include <vector>

struct AISVesselInfo {
    uint32_t mmsi;
    uint8_t msgType;
    uint8_t channel; // 0=A, 1=B
    uint32_t seenAt_ms;
};

class AISModule : public MeshModule
{
  public:
    AISModule(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t rxClkPin = 5,
              uint8_t rxDataGpio = 6);
    virtual void setup() override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }

    static void IRAM_ATTR handleRxBit();

    void switchToChannel(uint8_t channel);

    // Thread-safe accessors for the renderer
    static uint32_t getFrameCount();
    static uint8_t getCurrentChannel();
    static std::vector<AISVesselInfo> getRecentVessels();

  private:
    uint8_t _cs, _sdn;
    int8_t _irq;
    uint8_t _sck, _miso, _mosi, _rxClkPin, _rxDataGpio;

    static AISDecoder _decoder;
    static uint8_t _rxDataPin;

    static volatile uint32_t _frameCount;
    static volatile uint8_t _currentChannel;
    static std::vector<AISVesselInfo> _recentVessels;
    static SemaphoreHandle_t _vesselMutex;

    static uint8_t _extractMsgType(const uint8_t *payload, uint8_t len);
    static uint32_t _extractMMSI(const uint8_t *payload, uint8_t len);

    friend void aisTask(void *pvParameters);
};
