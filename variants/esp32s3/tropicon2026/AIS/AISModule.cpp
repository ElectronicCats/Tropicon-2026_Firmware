#include "AISModule.h"
#include "configuration.h"
#include <Arduino.h>

// ── Static members ──────────────────────────────────────────────────────────
AISDecoder AISModule::_decoder;
uint8_t AISModule::_rxDataPin = 17;
volatile uint32_t AISModule::_frameCount = 0;
volatile uint8_t AISModule::_currentChannel = 19; // MAIANA ch19 = AIS-A 161.975 MHz
std::vector<AISVesselInfo> AISModule::_recentVessels;
SemaphoreHandle_t AISModule::_vesselMutex = nullptr;

static constexpr uint8_t AIS_MAX_VESSELS = 8;

// ── Ring buffer for ISR → task bit transfer ─────────────────────────────────
static constexpr uint16_t BIT_RING_SIZE = 16384;
static volatile uint8_t _bitRing[BIT_RING_SIZE / 8];
static volatile uint16_t _bitRingHead = 0;
static volatile uint16_t _bitRingTail = 0;

static volatile uint32_t _isrBitCount = 0;
static volatile uint32_t _isrOneCount = 0;
static volatile uint32_t _isrOverflows = 0;

// ── RX_DATA_CLK ISR — stores bits in ring buffer ───────────────────────────
void IRAM_ATTR AISModule::handleRxBit()
{
    bool bit = digitalRead(_rxDataPin);
    _isrBitCount++;
    if (bit)
        _isrOneCount++;

    // Store bit in ring buffer (packed, 8 bits per byte)
    uint16_t head = _bitRingHead;
    uint16_t nextHead = (head + 1) & (BIT_RING_SIZE - 1);
    if (nextHead == _bitRingTail) {
        _isrOverflows++;
        return;
    }
    uint16_t byteIdx = head >> 3;
    uint8_t bitIdx = head & 7;
    if (bit)
        _bitRing[byteIdx] |= (1 << bitIdx);
    else
        _bitRing[byteIdx] &= ~(1 << bitIdx);
    _bitRingHead = nextHead;
}

// ── Constructor ─────────────────────────────────────────────────────────────
AISModule::AISModule(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t rxClkPin,
                     uint8_t rxDataGpio)
    : MeshModule("AIS"), _cs(cs), _sdn(sdn), _irq(irq), _sck(sck), _miso(miso), _mosi(mosi), _rxClkPin(rxClkPin),
      _rxDataGpio(rxDataGpio)
{
    _rxDataPin = rxDataGpio;
}

// ── checkAlive helper ──────────────────────────────────────────────────────
static bool checkAlive(const char *label)
{
    si446x_info_t info;
    Si446x_getInfo(&info);
    bool ok = (info.part == 0x4463);
    Serial.printf("  [%s] part=0x%04X %s\n", label, info.part, ok ? "OK" : "DEAD!");
    return ok;
}

// ── MAIANA Pass 2 modem config (applied at runtime after Pass 1 in radio_config) ──
static void applyAISModemConfig()
{
    // Global settings
    uint8_t g1[] = {0x11, 0x00, 0x01, 0x01, 0x00}; // GLOBAL_CLK_CFG
    Si446x_sendCmd(g1, sizeof(g1));
    uint8_t g2[] = {0x11, 0x00, 0x01, 0x03, 0x20}; // GLOBAL_CONFIG
    Si446x_sendCmd(g2, sizeof(g2));
    uint8_t g3[] = {0x11, 0x01, 0x01, 0x00, 0x00}; // INT_CTL disabled
    Si446x_sendCmd(g3, sizeof(g3));
    uint8_t g4[] = {0x11, 0x02, 0x04, 0x00, 0x0A, 0x09, 0x00, 0x00}; // FRR: A=RSSI, B=STATE
    Si446x_sendCmd(g4, sizeof(g4));

    // Preamble
    uint8_t p1[] = {0x11, 0x10, 0x09, 0x00, 0x08, 0x94, 0x00, 0x1F, 0x31, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(p1, sizeof(p1));

    // Sync
    uint8_t s1[] = {0x11, 0x11, 0x06, 0x00, 0x01, 0xCC, 0xCC, 0x00, 0x00, 0x00};
    Si446x_sendCmd(s1, sizeof(s1));

    // Packet handler
    uint8_t pk1[] = {0x11, 0x12, 0x0C, 0x00, 0x00, 0x01, 0x08, 0xFF, 0xFF, 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x40};
    Si446x_sendCmd(pk1, sizeof(pk1));
    uint8_t pk2[] = {0x11, 0x12, 0x0C, 0x0C, 0x40, 0x00, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(pk2, sizeof(pk2));
    uint8_t pk3[] = {0x11, 0x12, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(pk3, sizeof(pk3));
    uint8_t pk4[] = {0x11, 0x12, 0x0C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(pk4, sizeof(pk4));
    uint8_t pk5[] = {0x11, 0x12, 0x05, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(pk5, sizeof(pk5));
    uint8_t pk6[] = {0x11, 0x12, 0x04, 0x36, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(pk6, sizeof(pk6));
    checkAlive("PKT");

    // Modem Pass 2
    uint8_t m1[] = {0x11, 0x20, 0x0C, 0x00, 0x03, 0x00, 0x07, 0x05, 0xDC, 0x00, 0x05, 0xC9, 0xC3, 0x80, 0x00, 0x01};
    Si446x_sendCmd(m1, sizeof(m1));
    uint8_t m2[] = {0x11, 0x20, 0x01, 0x0C, 0xF7}; // FREQ_DEV
    Si446x_sendCmd(m2, sizeof(m2));
    uint8_t m3[] = {0x11, 0x20, 0x0C, 0x18, 0x01, 0x80, 0x08, 0x02, 0x80, 0x00, 0x70, 0x20, 0x00, 0xE8, 0x00, 0x62};
    Si446x_sendCmd(m3, sizeof(m3));
    uint8_t m4[] = {0x11, 0x20, 0x0C, 0x24, 0x05, 0x3E, 0x2D, 0x02, 0x9D, 0x00, 0xC2, 0x00, 0x54, 0x23, 0x81, 0x01};
    Si446x_sendCmd(m4, sizeof(m4));
    // AFC_LIMITER widened for 20pF crystal offset
    uint8_t m5[] = {0x11, 0x20, 0x03, 0x30, 0x08, 0x00, 0x80};
    Si446x_sendCmd(m5, sizeof(m5));
    uint8_t m6[] = {0x11, 0x20, 0x01, 0x35, 0xE0}; // AGC_CONTROL
    Si446x_sendCmd(m6, sizeof(m6));
    uint8_t m7[] = {0x11, 0x20, 0x0C, 0x38, 0x11, 0x15, 0x15, 0x80, 0x1A, 0x20, 0x00, 0x00, 0x28, 0x0C, 0x84, 0x23};
    Si446x_sendCmd(m7, sizeof(m7));
    checkAlive("MODEM");

    // RAW_CONTROL
    uint8_t m8[] = {0x11, 0x20, 0x0A, 0x45, 0x8F, 0x00, 0x6A, 0x01, 0x00, 0xFF, 0x06, 0x00, 0x18, 0x40};
    Si446x_sendCmd(m8, sizeof(m8));
    uint8_t m9[] = {0x11, 0x20, 0x02, 0x50, 0x94, 0x0D}; // RAW_SEARCH2 + CLKGEN_BAND
    Si446x_sendCmd(m9, sizeof(m9));
    uint8_t m10[] = {0x11, 0x20, 0x02, 0x54, 0x03, 0x07}; // SPIKE_DET
    Si446x_sendCmd(m10, sizeof(m10));
    uint8_t m11[] = {0x11, 0x20, 0x01, 0x57, 0x00}; // RSSI_MUTE
    Si446x_sendCmd(m11, sizeof(m11));
    uint8_t m12[] = {0x11, 0x20, 0x05, 0x5B, 0x40, 0x04, 0x04, 0x78, 0x20}; // DSA
    Si446x_sendCmd(m12, sizeof(m12));

    // Channel filters Pass 2
    uint8_t f1[] = {0x11, 0x21, 0x0C, 0x00, 0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1, 0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11};
    Si446x_sendCmd(f1, sizeof(f1));
    uint8_t f2[] = {0x11, 0x21, 0x0C, 0x0C, 0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00, 0xCC, 0xA1, 0x30, 0xA0, 0x21, 0xD1};
    Si446x_sendCmd(f2, sizeof(f2));
    uint8_t f3[] = {0x11, 0x21, 0x0C, 0x18, 0xB9, 0xC9, 0xEA, 0x05, 0x12, 0x11, 0x0A, 0x04, 0x15, 0xFC, 0x03, 0x00};
    Si446x_sendCmd(f3, sizeof(f3));
    checkAlive("CHFLT");

    // PA + Synth + Match + Freq Control
    uint8_t pa[] = {0x11, 0x22, 0x04, 0x00, 0x08, 0x7F, 0x00, 0x1D};
    Si446x_sendCmd(pa, sizeof(pa));
    uint8_t sy[] = {0x11, 0x23, 0x07, 0x00, 0x2C, 0x0E, 0x0B, 0x04, 0x0C, 0x73, 0x03};
    Si446x_sendCmd(sy, sizeof(sy));
    uint8_t mv[] = {0x11, 0x30, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si446x_sendCmd(mv, sizeof(mv));
    // FREQ_CONTROL: MAIANA base ~150.9 MHz, step 25 kHz. Ch19=AIS-A, Ch21=AIS-B
    uint8_t fc[] = {0x11, 0x40, 0x08, 0x00, 0x3F, 0x0C, 0xCC, 0xCC, 0x14, 0x7B, 0x20, 0xFA};
    Si446x_sendCmd(fc, sizeof(fc));
    checkAlive("FREQ_CTRL");
}

// ── Background task ─────────────────────────────────────────────────────────
void aisTask(void *pvParameters)
{
    uint32_t lastDiag = millis();

    while (true) {
        // Drain ring buffer → decoder
        while (_bitRingTail != _bitRingHead) {
            uint16_t tail = _bitRingTail;
            uint16_t byteIdx = tail >> 3;
            uint8_t bitIdx = tail & 7;
            bool bit = (_bitRing[byteIdx] >> bitIdx) & 1;
            _bitRingTail = (tail + 1) & (BIT_RING_SIZE - 1);
            AISModule::_decoder.processBit(bit);
        }

        // Diagnostic: print ISR stats every 5 seconds
        if (millis() - lastDiag >= 5000) {
            uint32_t bits = _isrBitCount;
            uint32_t ones = _isrOneCount;
            _isrBitCount = 0;
            _isrOneCount = 0;
            uint8_t radioState = Si446x_getState();
            uint8_t modemStat = 0;
            uint8_t currRssiRaw = 0;
            int16_t afcOffset = 0;
            Si446x_getModemStatus(&modemStat, &currRssiRaw, &afcOffset);
            int16_t rssi = (currRssiRaw / 2) - 134;
            Serial.printf("[AIS] 5s: %lu bits, %lu%% ones | flags=%lu aborts=%lu crcFail=%lu crcOK=%lu maxLen=%u | st=%u rssi=%d "
                          "modem=0x%02X afc=%d | ovf=%lu\n",
                          (unsigned long)bits, bits ? (unsigned long)(ones * 100 / bits) : 0UL,
                          (unsigned long)AISModule::_decoder.dbgFlags, (unsigned long)AISModule::_decoder.dbgAborts,
                          (unsigned long)AISModule::_decoder.dbgCrcFail, (unsigned long)AISModule::_decoder.dbgCrcPass,
                          (unsigned)AISModule::_decoder.dbgMaxLen, radioState, rssi, modemStat, afcOffset,
                          (unsigned long)_isrOverflows);
            lastDiag = millis();
        }

        // Silently consume debug frames to avoid serial flood blocking ring buffer drain
        if (AISModule::_decoder.dbgAbortReady)
            AISModule::_decoder.dbgAbortReady = false;
        if (AISModule::_decoder.dbgFrameReady)
            AISModule::_decoder.dbgFrameReady = false;

        // Check for decoded frames
        if (AISModule::_decoder.hasMessage()) {
            uint8_t len = AISModule::_decoder.getMessageLen();
            uint8_t frameBuf[AIS_MAX_MSG_LEN];
            if (len > 0 && len <= AIS_MAX_MSG_LEN) {
                memcpy(frameBuf, AISModule::_decoder.getMessagePtr(), len);
            }
            AISModule::_decoder.clearMessage();

            if (len > 0) {
                // Read RSSI at decode time (approximate — from FRR latch)
                uint8_t rssiRaw = 0;
                Si446x_getModemStatus(nullptr, &rssiRaw, nullptr);
                int16_t rssiDbm = (rssiRaw / 2) - 134;

                // Output NMEA !AIVDM sentence
                char channel = (AISModule::_currentChannel == 19) ? 'A' : 'B';
                uint16_t totalBits = len * 8;
                uint8_t fillBits = (6 - (totalBits % 6)) % 6;
                uint16_t numChars = (totalBits + fillBits) / 6;

                char nmea[256];
                int pos = snprintf(nmea, sizeof(nmea), "!AIVDM,1,1,,%c,", channel);

                // Encode payload: extract bits in AIS standard order, group into 6-bit NMEA chars
                // HDLC sends bytes LSB-first, so wire bit 0 = byte LSB = AIS bit 7.
                // AIS bit N is stored at byte[N/8] bit (7 - N%8) (MSB-first within each byte).
                for (uint16_t c = 0; c < numChars && pos < (int)sizeof(nmea) - 10; c++) {
                    uint8_t val = 0;
                    for (int b = 0; b < 6; b++) {
                        uint16_t bitIdx = c * 6 + b;
                        bool bit = false;
                        if (bitIdx < totalBits)
                            bit = (frameBuf[bitIdx / 8] >> (7 - (bitIdx % 8))) & 1;
                        val = (val << 1) | (bit ? 1 : 0);
                    }
                    // AIS 6-bit ASCII armor: add 48, if >87 add 8 more
                    char ch = val + 48;
                    if (ch > 87)
                        ch += 8;
                    nmea[pos++] = ch;
                }

                pos += snprintf(nmea + pos, sizeof(nmea) - pos, ",%u", fillBits);

                // Compute NMEA checksum (XOR of everything between ! and *)
                uint8_t cksum = 0;
                for (int i = 1; i < pos; i++)
                    cksum ^= nmea[i];
                snprintf(nmea + pos, sizeof(nmea) - pos, "*%02X", cksum);

                Serial.println(nmea);
                Serial.printf("[AIS] rssi=%d dBm, %u bytes\n", rssiDbm, len);

                // Extract fields for vessel tracking
                uint8_t msgType = AISModule::_extractMsgType(frameBuf, len);
                uint32_t mmsi = AISModule::_extractMMSI(frameBuf, len);

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

        vTaskDelay(pdMS_TO_TICKS(1)); // Must drain ring buffer faster than 9600 bps fill rate
    }
}

// ── setup() ─────────────────────────────────────────────────────────────────
void AISModule::setup()
{
    MeshModule::setup();
    _vesselMutex = xSemaphoreCreateMutex();

    // Init SI4463 with soft-SPI pins — loads Pass 1 config + firmware patch
    static uint8_t softSpiPins[3];
    softSpiPins[0] = _sck;
    softSpiPins[1] = _miso;
    softSpiPins[2] = _mosi;
    Si446x_init(_cs, _sdn, _irq, softSpiPins);

    // Wake radio
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
    Si446x_writeGPIO(SI446X_GPIO0, 0x03); // DRIVE1
    delay(2);
    bool g0_hi = digitalRead(_rxDataGpio);
    Si446x_writeGPIO(SI446X_GPIO0, 0x02); // DRIVE0
    delay(2);
    bool g0_lo = digitalRead(_rxDataGpio);
    Serial.printf("[AIS] GPIO0(pin%d): HIGH=%d LOW=%d\n", _rxDataGpio, g0_hi, g0_lo);

    // Configure GPIOs for AIS RX (matching Sabas/MAIANA)
    uint8_t gpioCmd[] = {0x13, 0x14, 0x00, 0x11, 0x14, 0x1B, 0x00, 0x60};
    Si446x_sendCmd(gpioCmd, sizeof(gpioCmd));
    _rxDataPin = _rxDataGpio;
    Serial.println("[AIS] GPIO cfg: GPIO0=RX_DATA, GPIO2=RX_DATA_CLK");
    checkAlive("GPIO_CFG");

    // Apply MAIANA Pass 2 modem config
    Serial.println("[AIS] Applying MAIANA Pass 2 modem config...");
    applyAISModemConfig();
    Serial.println("[AIS] Pass 2 config applied");

    // Enter RX on channel 19 (AIS-A = 161.975 MHz)
    {
        uint8_t cmd[] = {0x32, 19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        Si446x_sendCmd(cmd, sizeof(cmd));
    }
    _currentChannel = 19;
    delay(10);

    // Attach bit-clock interrupt on RISING edge (matching dAISy/Sabas)
    pinMode(_rxClkPin, INPUT);
    pinMode(_rxDataGpio, INPUT);
    attachInterrupt(digitalPinToInterrupt(_rxClkPin), &AISModule::handleRxBit, RISING);
    delay(100);
    Serial.printf("[AIS] CLK test: %lu bits in 100ms\n", (unsigned long)_isrBitCount);

    // Frequency fine-tune: crystal offset compensation via FC_FRAC
    // MAIANA nominal FRAC = 0x0CCCCC. Adjust to center ones% near 50%.
    // Each LSB ≈ 4.77 Hz at RF. Negative offset pulls LO down.
    const uint32_t nominalFrac = 0x0CCCCC;
    const int32_t fracOffset = -1500; // ~-7.2 kHz compensation for crystal
    uint32_t frac = nominalFrac + fracOffset;
    uint8_t fracCmd[] = {0x11, 0x40, 0x03, 0x01, (uint8_t)(frac >> 16), (uint8_t)(frac >> 8), (uint8_t)frac};
    Si446x_sendCmd(fracCmd, sizeof(fracCmd));
    // Re-enter RX on ch19 after frequency change
    {
        uint8_t cmd[] = {0x32, 19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        Si446x_sendCmd(cmd, sizeof(cmd));
    }
    delay(50);

    // Quick ones% check
    _isrBitCount = 0;
    _isrOneCount = 0;
    delay(2000);
    uint32_t chkBits = _isrBitCount;
    uint32_t chkOnes = _isrOneCount;
    int chkPct = chkBits ? (int)(chkOnes * 100 / chkBits) : 0;
    Serial.printf("[AIS] ones=%d%% (ch19, XO=0x7F, FC offset=%+d)\n", chkPct, (int)fracOffset);

    LOG_INFO("AIS: RX on pin%d, CLK pin%d, ch19 (161.975 MHz)", (int)_rxDataPin, _rxClkPin);

    xTaskCreate(aisTask, "AisTask", 4096, this, 1, nullptr);
}

// ── switchToChannel() ───────────────────────────────────────────────────────
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
// HDLC LSB-first means: byte[0] MSB = AIS bit 0, byte[0] LSB = AIS bit 7
// Type = AIS bits 0-5 = byte[0] bits 7-2
uint8_t AISModule::_extractMsgType(const uint8_t *payload, uint8_t len)
{
    if (len < 1)
        return 0;
    return (payload[0] >> 2) & 0x3F;
}

// MMSI = AIS bits 8-37 = byte[1] full + byte[2] full + byte[3] full + byte[4] bits 7-2
uint32_t AISModule::_extractMMSI(const uint8_t *payload, uint8_t len)
{
    if (len < 5)
        return 0;
    uint32_t mmsi = 0;
    mmsi |= (uint32_t)payload[1] << 22;
    mmsi |= (uint32_t)payload[2] << 14;
    mmsi |= (uint32_t)payload[3] << 6;
    mmsi |= (uint32_t)(payload[4] >> 2);
    return mmsi;
}
