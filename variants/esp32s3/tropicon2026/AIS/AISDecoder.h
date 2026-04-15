#pragma once

#include <esp_attr.h>
#include <stdint.h>

// Max AIS frame payload before CRC: 168 bytes covers all message types
#define AIS_MAX_MSG_LEN 168

class AISDecoder
{
  public:
    AISDecoder();

    // Called from ISR — must be IRAM-safe, no heap allocation
    void IRAM_ATTR processBit(bool bit);

    // Called from task context
    bool hasMessage() const;
    uint8_t getMessageLen() const;
    const uint8_t *getMessagePtr() const;
    void clearMessage();

  private:
    bool _lastBit;
    uint8_t _consecutiveOnes;
    bool _inPacket;
    uint16_t _bitCount;
    uint8_t _currentByte;
    uint8_t _bitInByte;

    uint8_t _msgBuf[AIS_MAX_MSG_LEN];
    uint8_t _msgLen;
    volatile bool _messageReady;

    uint16_t _crc;
    uint8_t _shiftReg;

    void resetDecoder();
    bool checkCRC();
    void IRAM_ATTR updateCRC(uint8_t byte);

  public:
    // Debug counters (volatile for ISR safety)
    volatile uint32_t dbgFlags;      // HDLC flags (0x7E) detected
    volatile uint32_t dbgFrameStart; // Frames started
    volatile uint32_t dbgAborts;     // Frames aborted (6+ ones)
    volatile uint32_t dbgCrcFail;    // Frames with CRC failure
    volatile uint32_t dbgCrcPass;    // Frames with CRC pass
    volatile uint16_t dbgMaxLen;     // Max bytes before abort/end

    // Debug: last aborted frame data
    uint8_t dbgAbortBuf[32];      // First 32 bytes of last aborted frame
    volatile uint8_t dbgAbortLen; // Length of last aborted frame (capped at 32)
    volatile bool dbgAbortReady;  // New abort data available

    // Debug: CRC failure info
    volatile uint16_t dbgLastCrc;   // CRC value at last failure
    volatile uint8_t dbgLastCrcLen; // Frame length at last CRC failure

    // Debug: dump long frame (20+ bytes) on CRC fail
    uint8_t dbgFrameBuf[48];
    volatile uint8_t dbgFrameLen;
    volatile bool dbgFrameReady;
};
