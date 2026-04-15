#include "AISDecoder.h"

AISDecoder::AISDecoder()
{
    resetDecoder();
}

void AISDecoder::resetDecoder()
{
    _lastBit = true;
    _consecutiveOnes = 0;
    _inPacket = false;
    _bitCount = 0;
    _currentByte = 0;
    _bitInByte = 0;
    _msgLen = 0;
    _messageReady = false;
    _crc = 0xFFFF;
    _shiftReg = 0xFF;
    dbgFlags = 0;
    dbgFrameStart = 0;
    dbgAborts = 0;
    dbgCrcFail = 0;
    dbgCrcPass = 0;
    dbgMaxLen = 0;
    dbgAbortLen = 0;
    dbgAbortReady = false;
    dbgLastCrc = 0;
    dbgLastCrcLen = 0;
    dbgFrameLen = 0;
    dbgFrameReady = false;
}

void IRAM_ATTR AISDecoder::processBit(bool bit)
{
    // NRZI decode: same level = 1, transition = 0
    bool decodedBit = (bit == _lastBit);
    _lastBit = bit;

    // Shift into 8-bit flag detector
    _shiftReg = (_shiftReg << 1) | (decodedBit ? 1 : 0);

    // Check for HDLC flag 0x7E = 01111110
    if (_shiftReg == 0x7E) {
        dbgFlags++;
        if (!_inPacket) {
            // Start of frame
            _inPacket = true;
            _msgLen = 0;
            _bitCount = 0;
            _currentByte = 0;
            _bitInByte = 0;
            _consecutiveOnes = 0;
            _crc = 0xFFFF;
            dbgFrameStart++;
        } else {
            // End of frame
            if (_msgLen > 2 && checkCRC()) {
                _msgLen -= 2; // strip CRC bytes
                _messageReady = true;
                dbgCrcPass++;
            } else if (_msgLen > 0) {
                dbgCrcFail++;
                dbgLastCrc = _crc;
                dbgLastCrcLen = _msgLen;
                // Capture frames for inspection (any length)
                if (_msgLen >= 5 && !dbgFrameReady) {
                    uint8_t copyLen = _msgLen > 48 ? 48 : _msgLen;
                    for (uint8_t i = 0; i < copyLen; i++)
                        dbgFrameBuf[i] = _msgBuf[i];
                    dbgFrameLen = copyLen;
                    dbgFrameReady = true;
                }
            }
            _inPacket = false;
        }
        return;
    }

    if (!_inPacket)
        return;

    // Bit-unstuffing
    if (decodedBit) {
        _consecutiveOnes++;
        if (_consecutiveOnes >= 7) {
            // Abort — 7+ consecutive ones = HDLC abort (6 ones is part of flag 0x7E)
            dbgAborts++;
            if (_msgLen > dbgMaxLen)
                dbgMaxLen = _msgLen;
            // Capture first 32 bytes for debug
            if (_msgLen >= 10 && !dbgAbortReady) {
                uint8_t copyLen = _msgLen > 32 ? 32 : _msgLen;
                for (uint8_t i = 0; i < copyLen; i++)
                    dbgAbortBuf[i] = _msgBuf[i];
                dbgAbortLen = copyLen;
                dbgAbortReady = true;
            }
            _inPacket = false;
            return;
        }
    } else {
        if (_consecutiveOnes == 5) {
            // Stuffed bit — discard
            _consecutiveOnes = 0;
            return;
        }
        _consecutiveOnes = 0;
    }

    // Accumulate bits LSB-first
    if (decodedBit)
        _currentByte |= (1 << _bitInByte);

    _bitInByte++;
    if (_bitInByte == 8) {
        if (_msgLen < AIS_MAX_MSG_LEN) {
            _msgBuf[_msgLen++] = _currentByte;
            updateCRC(_currentByte);
        } else {
            // Frame too long — abort
            _inPacket = false;
        }
        _currentByte = 0;
        _bitInByte = 0;
    }
}

// CRC-16-CCITT reflected (polynomial 0x8408, init 0xFFFF)
// Bits are already LSB-first from the accumulator, matching reflected convention
void IRAM_ATTR AISDecoder::updateCRC(uint8_t byte)
{
    _crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (_crc & 1)
            _crc = (_crc >> 1) ^ 0x8408;
        else
            _crc >>= 1;
    }
}

bool AISDecoder::checkCRC()
{
    // Try both common HDLC CRC-CCITT residues
    return (_crc == 0xF0B8 || _crc == 0x0F47);
}

bool AISDecoder::hasMessage() const
{
    return _messageReady;
}

uint8_t AISDecoder::getMessageLen() const
{
    return _msgLen;
}

const uint8_t *AISDecoder::getMessagePtr() const
{
    return _msgBuf;
}

void AISDecoder::clearMessage()
{
    _messageReady = false;
    _msgLen = 0;
}
