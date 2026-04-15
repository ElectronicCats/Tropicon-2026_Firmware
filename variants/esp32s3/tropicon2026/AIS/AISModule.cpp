#include "AISModule.h"
#include "configuration.h"
#include <Arduino.h>

// ── Static members ──────────────────────────────────────────────────────────
AISDecoder AISModule::_decoder;
uint8_t AISModule::_rxDataPin = 17;
volatile uint32_t AISModule::_frameCount = 0;
volatile uint8_t AISModule::_currentChannel = 0;
std::vector<AISVesselInfo> AISModule::_recentVessels;
SemaphoreHandle_t AISModule::_vesselMutex = nullptr;

static constexpr uint8_t AIS_MAX_VESSELS = 8;
static volatile uint32_t _isrBitCount = 0;
static volatile uint32_t _isrOneCount = 0;

// ── RX_DATA_CLK ISR — called on each demodulated bit ────────────────────────
void IRAM_ATTR AISModule::handleRxBit()
{
    bool bit = digitalRead(_rxDataPin);
    _isrBitCount++;
    if (bit)
        _isrOneCount++;
    _decoder.processBit(bit);
}

// ── Constructor ─────────────────────────────────────────────────────────────
AISModule::AISModule(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t rxClkPin,
                     uint8_t rxDataGpio)
    : MeshModule("AIS"), _cs(cs), _sdn(sdn), _irq(irq), _sck(sck), _miso(miso), _mosi(mosi), _rxClkPin(rxClkPin),
      _rxDataGpio(rxDataGpio)
{
    _rxDataPin = rxDataGpio; // Read demodulated data from GPIO0 pin, not SDO
}

// ── Background task ─────────────────────────────────────────────────────────
void aisTask(void *pvParameters)
{
    uint32_t lastDiag = millis();

    while (true) {
        // Diagnostic: print ISR stats every 5 seconds
        if (millis() - lastDiag >= 5000) {
            uint32_t bits = _isrBitCount;
            uint32_t ones = _isrOneCount;
            _isrBitCount = 0;
            _isrOneCount = 0;
            // SPI still works — read radio state, RSSI, modem status, AFC
            uint8_t radioState = Si446x_getState();
            uint8_t modemStat = 0;
            uint8_t currRssiRaw = 0;
            int16_t afcOffset = 0;
            Si446x_getModemStatus(&modemStat, &currRssiRaw, &afcOffset);
            int16_t rssi = (currRssiRaw / 2) - 134;
            Serial.printf("[AIS] 5s: %lu bits, %lu%% ones | flags=%lu aborts=%lu crcFail=%lu crcOK=%lu maxLen=%u | st=%u rssi=%d "
                          "modem=0x%02X afc=%d | lastCRC=0x%04X len=%u\n",
                          (unsigned long)bits, bits ? (unsigned long)(ones * 100 / bits) : 0UL,
                          (unsigned long)AISModule::_decoder.dbgFlags, (unsigned long)AISModule::_decoder.dbgAborts,
                          (unsigned long)AISModule::_decoder.dbgCrcFail, (unsigned long)AISModule::_decoder.dbgCrcPass,
                          (unsigned)AISModule::_decoder.dbgMaxLen, radioState, rssi, modemStat, afcOffset,
                          AISModule::_decoder.dbgLastCrc, AISModule::_decoder.dbgLastCrcLen);
            lastDiag = millis();
        }

        // Print hex dump of aborted frames (for debugging)
        if (AISModule::_decoder.dbgAbortReady) {
            uint8_t len = AISModule::_decoder.dbgAbortLen;
            Serial.printf("[AIS] Abort hex (%u bytes): ", len);
            for (uint8_t i = 0; i < len; i++)
                Serial.printf("%02X ", AISModule::_decoder.dbgAbortBuf[i]);
            Serial.println();
            AISModule::_decoder.dbgAbortReady = false;
        }

        // Print hex dump of long CRC-fail frames (possible AIS)
        if (AISModule::_decoder.dbgFrameReady) {
            uint8_t len = AISModule::_decoder.dbgFrameLen;
            uint16_t crc = AISModule::_decoder.dbgLastCrc;
            Serial.printf("[AIS] CRC-fail frame (%u bytes, crc=0x%04X): ", len, crc);
            for (uint8_t i = 0; i < len; i++)
                Serial.printf("%02X ", AISModule::_decoder.dbgFrameBuf[i]);
            Serial.println();
            AISModule::_decoder.dbgFrameReady = false;
        }

        // Check for decoded frames
        if (AISModule::_decoder.hasMessage()) {
            uint8_t len = AISModule::_decoder.getMessageLen();
            uint8_t frameBuf[AIS_MAX_MSG_LEN];
            if (len > 0 && len <= AIS_MAX_MSG_LEN) {
                memcpy(frameBuf, AISModule::_decoder.getMessagePtr(), len);
            }
            AISModule::_decoder.clearMessage();

            if (len > 0) {
                uint8_t msgType = AISModule::_extractMsgType(frameBuf, len);
                uint32_t mmsi = AISModule::_extractMMSI(frameBuf, len);

                LOG_INFO("AIS Frame  type=%u  MMSI=%09lu  ch=%c  bytes=%u", msgType, (unsigned long)mmsi,
                         AISModule::_currentChannel ? 'B' : 'A', (unsigned)len);

                AISVesselInfo info{mmsi, msgType, AISModule::_currentChannel, (uint32_t)millis()};

                if (xSemaphoreTake(AISModule::_vesselMutex, portMAX_DELAY) == pdTRUE) {
                    bool updated = false;
                    for (auto &v : AISModule::_recentVessels) {
                        if (v.mmsi == mmsi) {
                            v = info;
                            updated = true;
                            break;
                        }
                    }
                    if (!updated) {
                        if (AISModule::_recentVessels.size() >= AIS_MAX_VESSELS)
                            AISModule::_recentVessels.erase(AISModule::_recentVessels.begin());
                        AISModule::_recentVessels.push_back(info);
                    }
                    xSemaphoreGive(AISModule::_vesselMutex);
                }

                AISModule::_frameCount++;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── setup() ─────────────────────────────────────────────────────────────────
void AISModule::setup()
{
    MeshModule::setup();
    _vesselMutex = xSemaphoreCreateMutex();

    // Init SI4463 with soft-SPI pins
    static uint8_t softSpiPins[3];
    softSpiPins[0] = _sck;
    softSpiPins[1] = _miso;
    softSpiPins[2] = _mosi;
    Si446x_init(_cs, _sdn, _irq, softSpiPins);

    // Si446x_init ends with Si446x_sleep() — wake radio before querying
    Si446x_RX(0);

    // Verify chip is present
    si446x_info_t info;
    Si446x_getInfo(&info);
    Serial.printf("[AIS] part=0x%04X rev=0x%02X rom=0x%02X\n", info.part, info.chipRev, info.romId);
    if (info.part == 0x4463) {
        LOG_INFO("AIS: SI4463 detected (rev=%X rom=%X)", info.chipRev, info.romId);
    } else {
        LOG_ERROR("AIS: SI4463 NOT detected (part=%04X)", info.part);
        return;
    }

    // ── GPIO0 connectivity test ──
    pinMode(_rxDataGpio, INPUT);
    Si446x_writeGPIO(SI446X_GPIO0, 0x03); // DRIVE1 (force high)
    delay(2);
    bool g0_hi = digitalRead(_rxDataGpio);
    Si446x_writeGPIO(SI446X_GPIO0, 0x02); // DRIVE0 (force low)
    delay(2);
    bool g0_lo = digitalRead(_rxDataGpio);
    Serial.printf("[AIS] GPIO0(pin%d): HIGH=%d LOW=%d\n", _rxDataGpio, g0_hi, g0_lo);
    bool gpio0_ok = (g0_hi == 1 && g0_lo == 0);

    if (gpio0_ok) {
        Si446x_writeGPIO(SI446X_GPIO0, 0x14); // GPIO0 = RX_DATA
        _rxDataPin = _rxDataGpio;
        Serial.println("[AIS] GPIO0 connected — using GPIO0 for RX_DATA, SPI stays up");
    } else {
        Serial.println("[AIS] GPIO0 NOT connected — using SDO for RX_DATA (SPI disabled)");
        Si446x_writeGPIO(SI446X_SDO, 0x14); // SDO = RX_DATA
        _rxDataPin = _miso;
    }

    // Enter RX mode
    Si446x_RX(0);
    _currentChannel = 0;
    delay(10);

    // Attach bit-clock interrupt on FALLING edge of RX_DATA_CLK
    pinMode(_rxClkPin, INPUT);
    pinMode(_rxDataGpio, INPUT);
    attachInterrupt(digitalPinToInterrupt(_rxClkPin), &AISModule::handleRxBit, FALLING);
    delay(100);
    Serial.printf("[AIS] CLK test: %lu bits in 100ms\n", (unsigned long)_isrBitCount);

    // Quick ones% check (no frequency override — using default XO_TUNE from radio config)
    _isrBitCount = 0;
    _isrOneCount = 0;
    delay(2000);
    uint32_t chkBits = _isrBitCount;
    uint32_t chkOnes = _isrOneCount;
    int chkPct = chkBits ? (int)(chkOnes * 100 / chkBits) : 0;
    Serial.printf("[AIS] Default config: XO=0x52, ones=%d%%\n", chkPct);

    LOG_INFO("AIS: RX_DATA on pin%d, CLK on pin%d, listening 161.975 MHz", (int)_rxDataPin, _rxClkPin);

    xTaskCreate(aisTask, "AisTask", 4096, this, 1, nullptr);
}

// ── switchToChannel() ───────────────────────────────────────────────────────
// NOTE: Channel hopping is disabled because SPI doesn't work after SDO
// is set to RX_DATA mode. To re-enable, SDO must be temporarily switched
// back to SPI mode, or RX_DATA must be routed through a GPIO pin instead.
void AISModule::switchToChannel(uint8_t channel)
{
    _currentChannel = channel;
}

// ── Accessors ───────────────────────────────────────────────────────────────
uint32_t AISModule::getFrameCount()
{
    return _frameCount;
}
uint8_t AISModule::getCurrentChannel()
{
    return _currentChannel;
}

std::vector<AISVesselInfo> AISModule::getRecentVessels()
{
    std::vector<AISVesselInfo> copy;
    if (_vesselMutex && xSemaphoreTake(_vesselMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = _recentVessels;
        xSemaphoreGive(_vesselMutex);
    }
    return copy;
}

// ── AIS field extraction ────────────────────────────────────────────────────
uint8_t AISModule::_extractMsgType(const uint8_t *payload, uint8_t len)
{
    if (len < 1)
        return 0;
    uint8_t b = payload[0] & 0x3F;
    uint8_t r = 0;
    for (int i = 0; i < 6; i++)
        r |= ((b >> i) & 1) << (5 - i);
    return r;
}

uint32_t AISModule::_extractMMSI(const uint8_t *payload, uint8_t len)
{
    if (len < 5)
        return 0;
    auto rev8 = [](uint8_t b) -> uint8_t {
        b = (b & 0xF0u) >> 4 | (b & 0x0Fu) << 4;
        b = (b & 0xCCu) >> 2 | (b & 0x33u) << 2;
        b = (b & 0xAAu) >> 1 | (b & 0x55u) << 1;
        return b;
    };

    uint32_t mmsi = 0;
    mmsi |= (uint32_t)rev8(payload[1]) << 22;
    mmsi |= (uint32_t)rev8(payload[2]) << 14;
    mmsi |= (uint32_t)rev8(payload[3]) << 6;
    uint8_t b4 = payload[4] & 0x3F;
    uint8_t r4 = 0;
    for (int i = 0; i < 6; i++)
        r4 |= ((b4 >> i) & 1) << (5 - i);
    mmsi |= r4;

    return mmsi & 0x3FFFFFFF;
}
