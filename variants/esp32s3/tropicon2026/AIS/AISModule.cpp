#include "AISModule.h"
#include <Arduino.h>

// ── Static member definitions ────────────────────────────────────────────────
AISDecoder              AISModule::_decoder;
uint8_t                 AISModule::_rxDataPin     = 17;   // SI4463_MISO — overridden in constructor
int8_t                  AISModule::_rxClkPin      = -1;   // No dedicated clock pin (async sampling)
volatile uint32_t       AISModule::_frameCount    = 0;
volatile uint8_t        AISModule::_currentChannel = 0;
std::vector<AISVesselInfo> AISModule::_recentVessels;
SemaphoreHandle_t       AISModule::_vesselMutex   = nullptr;

// Maximum recent-vessel entries kept in RAM
static constexpr uint8_t AIS_MAX_VESSELS = 8;

// Hardware timer handle
static hw_timer_t *aisTimer = nullptr;

// ── Timer ISR — runs every 104 µs (≈ 9600 Hz) ───────────────────────────────
// Reads the SI4463 SDO pin.  The SDO pin is configured as RX_DATA output when
// CS is de-asserted (HIGH).  The sampling ISR only runs when CS is HIGH because
// switchToChannel() disables the timer alarm before asserting CS.
void IRAM_ATTR AISModule::handleRxBit()
{
    bool bit = digitalRead(_rxDataPin);
    _decoder.processBit(bit);
}

// ── Constructor ──────────────────────────────────────────────────────────────
AISModule::AISModule(uint8_t cs, uint8_t sdn, int8_t irq,
                     uint8_t sck, uint8_t miso, uint8_t mosi)
    : MeshModule("AIS"),
      _cs(cs), _sdn(sdn), _irq(irq),
      _sck(sck), _miso(miso), _mosi(mosi)
{
    _rxDataPin = miso;
}

// ── FreeRTOS background task ─────────────────────────────────────────────────
void aisTask(void *pvParameters)
{
    AISModule *module = static_cast<AISModule *>(pvParameters);

    static uint32_t lastHop = 0;
    static bool     alt     = false;

    while (true) {
        // ── Process decoded frames ──────────────────────────────────────────
        if (AISModule::getDecoder().hasMessage()) {
            std::vector<uint8_t> raw = AISModule::getDecoder().getMessage();
            AISModule::getDecoder().clearMessage();

            uint8_t  msgType = AISModule::_extractMsgType(raw);
            uint32_t mmsi    = AISModule::_extractMMSI(raw);

            Serial.printf("AIS Frame  type=%u  MMSI=%09lu  ch=%c  bytes=%u\n",
                          msgType, (unsigned long)mmsi,
                          AISModule::getCurrentChannel() ? 'B' : 'A',
                          (unsigned)raw.size());

            // Store vessel info (protected by mutex)
            AISVesselInfo info{mmsi, msgType,
                               AISModule::getCurrentChannel(),
                               (uint32_t)millis()};

            if (xSemaphoreTake(AISModule::_vesselMutex, portMAX_DELAY) == pdTRUE) {
                // Update existing entry or append
                bool updated = false;
                for (auto &v : AISModule::_recentVessels) {
                    if (v.mmsi == mmsi) { v = info; updated = true; break; }
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

        // ── Frequency hopping every 10 s ───────────────────────────────────
        if (millis() - lastHop >= 10000) {
            alt = !alt;
            module->switchToChannel(alt ? 1 : 0);
            lastHop = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── setup() ─────────────────────────────────────────────────────────────────
void AISModule::setup()
{
    MeshModule::setup();

    _vesselMutex = xSemaphoreCreateMutex();

    // Soft-SPI pin array expected by Si446x_init on TROPICON2026
    static uint8_t softSpiPins[3];
    softSpiPins[0] = _sck;
    softSpiPins[1] = _miso;
    softSpiPins[2] = _mosi;

    Si446x_init(_cs, _sdn, _irq, softSpiPins);

    // Configure SDO pin as RX_DATA output.
    // When CS is HIGH the SI4463 drives SDO with the demodulated bit stream;
    // when CS is LOW the pin reverts to normal SPI MISO behaviour.
    // switchToChannel() disables the timer alarm *before* asserting CS so the
    // ISR never samples the pin during an SPI transaction.
    Si446x_writeGPIO(SI446X_SDO, SI446X_GPIO_MODE_RX_DATA);

    // Enter RX on channel A (161.975 MHz)
    switchToChannel(0);

    // Timer: prescaler 80 → 1 MHz tick, alarm at 104 → ~9615 Hz ≈ 9600 bps
    aisTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(aisTimer, &AISModule::handleRxBit, true);
    timerAlarmWrite(aisTimer, 104, true);
    timerAlarmEnable(aisTimer);

    xTaskCreate(aisTask, "AisTask", 4096, this, 1, nullptr);
}

// ── switchToChannel() ────────────────────────────────────────────────────────
// Pauses the sampling timer so that no ISR fires while CS is asserted (LOW)
// and the SDO/MISO pin carries SPI response bytes instead of RX data.
void AISModule::switchToChannel(uint8_t channel)
{
    // Stop sampling before touching SPI
    if (aisTimer) timerAlarmDisable(aisTimer);

    _currentChannel = channel;
    Si446x_RX(channel);   // SPI transaction — CS is asserted/de-asserted inside

    // Re-enable sampling after SPI is done
    if (aisTimer) timerAlarmEnable(aisTimer);
}

// ── Thread-safe accessors ────────────────────────────────────────────────────
uint32_t AISModule::getFrameCount()    { return _frameCount; }
uint8_t  AISModule::getCurrentChannel(){ return _currentChannel; }

std::vector<AISVesselInfo> AISModule::getRecentVessels()
{
    std::vector<AISVesselInfo> copy;
    if (_vesselMutex && xSemaphoreTake(_vesselMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = _recentVessels;
        xSemaphoreGive(_vesselMutex);
    }
    return copy;
}

// ── AIS payload bit-extraction helpers ──────────────────────────────────────
// The SI4463 delivers bits LSB-first over the air (HDLC convention), so every
// byte stored in the decoder buffer has bit-0 = first received = AIS MSB.
// To recover the numerical value of a field we bit-reverse each byte.

uint8_t AISModule::_bitReverse8(uint8_t b)
{
    b = (b & 0xF0u) >> 4 | (b & 0x0Fu) << 4;
    b = (b & 0xCCu) >> 2 | (b & 0x33u) << 2;
    b = (b & 0xAAu) >> 1 | (b & 0x55u) << 1;
    return b;
}

uint8_t AISModule::_bitReverse6(uint8_t b)
{
    // Reverse the 6 significant bits (bits 5..0) and return in bits 5..0
    uint8_t r = 0;
    for (int i = 0; i < 6; i++)
        r |= ((b >> i) & 1) << (5 - i);
    return r;
}

// AIS message type — bits 0-5, stored LSB-first → reverse the lower 6 bits
uint8_t AISModule::_extractMsgType(const std::vector<uint8_t>& payload)
{
    if (payload.empty()) return 0;
    return _bitReverse6(payload[0] & 0x3Fu);
}

// AIS MMSI — bits 8-37 (30 bits), stored LSB-first across bytes 1-4
uint32_t AISModule::_extractMMSI(const std::vector<uint8_t>& payload)
{
    if (payload.size() < 5) return 0;
    uint32_t mmsi = 0;
    mmsi |= (uint32_t)_bitReverse8(payload[1]) << 22;
    mmsi |= (uint32_t)_bitReverse8(payload[2]) << 14;
    mmsi |= (uint32_t)_bitReverse8(payload[3]) << 6;
    mmsi |= (uint32_t)_bitReverse6(payload[4] & 0x3Fu);
    return mmsi;
}
