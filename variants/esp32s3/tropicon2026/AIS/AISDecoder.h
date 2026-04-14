#pragma once

#include <stdint.h>
#include <vector>

class AISDecoder {
public:
    AISDecoder();
    void processBit(bool bit);
    bool hasMessage();
    std::vector<uint8_t> getMessage();
    void clearMessage();

private:
    bool _lastBit;
    uint8_t _consecutiveOnes;
    bool _inPacket;
    uint16_t _bitCount;
    uint8_t _currentByte;
    uint8_t _bitInByte;
    std::vector<uint8_t> _messageBuffer;
    bool _messageReady;

    uint16_t _crc;
    uint8_t  _shiftReg;  // HDLC flag detector — must be a member so resetDecoder() can clear it

    void resetDecoder();
    bool checkCRC();
    void updateCRC(uint8_t byte);
};
