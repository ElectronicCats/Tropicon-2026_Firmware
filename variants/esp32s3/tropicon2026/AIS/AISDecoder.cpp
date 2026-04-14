#include "AISDecoder.h"
#include <Arduino.h>

AISDecoder::AISDecoder() {
    resetDecoder();
}

void AISDecoder::resetDecoder() {
    _lastBit = true;
    _consecutiveOnes = 0;
    _inPacket = false;
    _bitCount = 0;
    _currentByte = 0;
    _bitInByte = 0;
    _messageBuffer.clear();
    _messageReady = false;
    _crc = 0xFFFF;
    _shiftReg = 0xFF;  // Reset HDLC flag shift register
}

void AISDecoder::processBit(bool bit) {
    // NRZI Decoding: 0 = change, 1 = no change
    bool decodedBit = (bit == _lastBit);
    _lastBit = bit;

    // Shift the decoded bit into the 8-bit flag detector register.
    // Using the member variable _shiftReg (not a static local) so that
    // resetDecoder() can properly clear it between packets.
    _shiftReg = (_shiftReg << 1) | (decodedBit ? 1 : 0);

    if (_shiftReg == 0x7E) {
        if (!_inPacket) {
            // Potential Start of Frame
            _inPacket = true;
            _messageBuffer.clear();
            _bitCount = 0;
            _currentByte = 0;
            _bitInByte = 0;
            _consecutiveOnes = 0;
            _crc = 0xFFFF;
        } else {
            // Potential End of Frame
            if (_messageBuffer.size() > 2) {
                if (checkCRC()) {
                    // Valid AIS message! Remove CRC bytes before signaling ready
                    _messageBuffer.pop_back();
                    _messageBuffer.pop_back();
                    _messageReady = true;
                }
            }
            _inPacket = false;
        }
        return;
    }

    if (!_inPacket) return;

    // Bit Unstuffing: after five consecutive '1's, the sender inserts a '0'.
    // Six or more '1's without a stuffed '0' is a protocol violation — abort frame.
    if (decodedBit) {
        _consecutiveOnes++;
        if (_consecutiveOnes >= 6) {
            // Abort sequence (≥6 ones inside a packet). Reset decoder.
            _inPacket = false;
            return;
        }
    } else {
        if (_consecutiveOnes == 5) {
            // Stuffed bit detected, ignore this '0'
            _consecutiveOnes = 0;
            return;
        }
        _consecutiveOnes = 0;
    }

    // Accumulate bits into bytes (LSB first for AIS/HDLC)
    if (decodedBit) _currentByte |= (1 << _bitInByte);
    
    _bitInByte++;
    if (_bitInByte == 8) {
        _messageBuffer.push_back(_currentByte);
        updateCRC(_currentByte);
        _currentByte = 0;
        _bitInByte = 0;
    }
}

bool AISDecoder::hasMessage() {
    return _messageReady;
}

std::vector<uint8_t> AISDecoder::getMessage() {
    return _messageBuffer;
}

void AISDecoder::clearMessage() {
    _messageReady = false;
    _messageBuffer.clear();
}

void AISDecoder::updateCRC(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        bool bit = (byte >> i) & 1;
        bool c15 = (_crc >> 15) & 1;
        _crc <<= 1;
        if (c15 ^ bit) _crc ^= 0x1021;
    }
}

bool AISDecoder::checkCRC() {
    // Standard HDLC CRC-16 (X.25)
    // The result of the division after receiving all bits (including FCS)
    // should be 0x1D0F for CCITT if using this implementation
    // Or we can just check if the calculated CRC matches the last two bytes.
    // For simplicity, let's assume updateCRC was called on all bytes except final FCS.
    // Actually, it's better to just check if the state is final.
    return (_crc == 0xF0B8); // Standard final value for CCITT CRC-16 if inverted
}
