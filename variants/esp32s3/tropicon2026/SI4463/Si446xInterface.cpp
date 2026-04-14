#include "Si446xInterface.h"
#include "Si446x.h"
#include "mesh/MeshService.h"
#include "mesh/MeshRadio.h"
#include "mesh/MeshTypes.h"
#include "PointerQueue.h"
#include "configuration.h"
#include "main.h"
#include "gps/RTC.h"

// Static instance pointer for C-style callbacks
static Si446xInterface* _instance = nullptr;

#ifdef TROPICON2026
Si446xInterface::Si446xInterface(uint8_t cs, uint8_t sdn, int8_t irq, uint8_t sck, uint8_t miso, uint8_t mosi)
    : _cs(cs), _sdn(sdn), _irq(irq), _sck(sck), _miso(miso), _mosi(mosi) {
    _instance = this;
}
#else
Si446xInterface::Si446xInterface(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi)
    : _cs(cs), _sdn(sdn), _irq(irq), _spi(spi) {
    _instance = this;
}
#endif

Si446xInterface::~Si446xInterface()
{
    if (_instance == this) _instance = nullptr;
}

bool Si446xInterface::init()
{
    LOG_INFO("Si446x: Initializing radio");
#ifdef TROPICON2026
    // Pass soft SPI pins as uint8_t[3] = { sck, miso, mosi }
    const uint8_t softPins[3] = { _sck, _miso, _mosi };
    Si446x_init(_cs, _sdn, _irq, (void*)softPins);
#else
    Si446x_init(_cs, _sdn, _irq, &_spi);
#endif
    
    si446x_info_t info;
    Si446x_getInfo(&info);
    
    if (info.part == 0x4463)
    {
        LOG_INFO("Si446x: Detected SI4463 Rev: %X Part: %04X Rom: %X", info.chipRev, info.part, info.romId);
        
        // Success, start RX
        Si446x_RX(_channel);
        return true;
    }
    else
    {
        LOG_ERROR("Si446x: Radio NOT detected (Part: %04X)", info.part);
        return false;
    }
}

bool Si446xInterface::reconfigure()
{
    LOG_INFO("Si446x: Reconfiguring radio");
    // For now, keep it simple. Meshtastic's LoRa settings don't apply directly.
    return true;
}

ErrorCode Si446xInterface::send(meshtastic_MeshPacket *p)
{
    if (disabled) return ERRNO_DISABLED;

    size_t len = beginSending(p);
    if (len == 0) return ERRNO_UNKNOWN;
    
    // Meshtastic packets include a header. 
    // The Si4463 library handles the length and the data.
    if (Si446x_TX(&radioBuffer, len, _channel, SI446X_STATE_RX))
    {
        LOG_DEBUG("Si446x: Sending packet (len=%d)", len);
        return ERRNO_OK;
    }
    else
    {
        LOG_ERROR("Si446x: Failed to start transmission");
        return ERRNO_UNKNOWN;
    }
}

uint32_t Si446xInterface::getPacketTime(uint32_t totalPacketLen, bool received)
{
    // Si4463 at 50kbps (default radio_config.h usually):
    // (len * 8 bits) / 50000 bps = time in seconds.
    // Plus preamble (~8 bytes) and sync (~4 bytes).
    uint32_t bits = (totalPacketLen + 12) * 8;
    return (bits * 1000) / 50000; // time in ms
}

bool Si446xInterface::sleep()
{
    Si446x_sleep();
    return true;
}

void Si446xInterface::handleRxBegin(int16_t rssi)
{
    this->lastTxStart = 0; // Not sending
}

void Si446xInterface::handleRxComplete(uint8_t length, int16_t rssi)
{
    LOG_DEBUG("Si446x: Packet received (len=%d, rssi=%d)", length, rssi);
    
    // length is already uint8_t (max 255), and radioBuffer is 256 bytes, 
    // so it will always fit.
    Si446x_read(&radioBuffer, length);
    
    // Convert to mesh packet
    meshtastic_MeshPacket *p = packetPool.allocZeroed();
    if (p)
    {
        p->rx_rssi = rssi;
        p->rx_time = getTime();
        
        // In Meshtastic, the radioBuffer already contains the PacketHeader + Payload
        // due to how beginSending() works on the tx side.
        // We need to decode it back into the protobuf MeshPacket.
        if (length >= sizeof(PacketHeader))
        {
            memcpy(&p->from, &radioBuffer.header.from, 4);
            memcpy(&p->to, &radioBuffer.header.to, 4);
            p->id           = radioBuffer.header.id;
            p->hop_limit    = radioBuffer.header.flags & PACKET_FLAGS_HOP_LIMIT_MASK;
            p->want_ack     = (radioBuffer.header.flags & PACKET_FLAGS_WANT_ACK_MASK) != 0;
            p->channel      = radioBuffer.header.channel;
            
            size_t payloadLen = length - sizeof(PacketHeader);
            p->which_payload_variant = meshtastic_MeshPacket_encrypted_tag;
            p->encrypted.size = payloadLen;
            memcpy(p->encrypted.bytes, radioBuffer.payload, payloadLen);
            
            deliverToReceiver(p);
        }
        else
        {
            packetPool.release(p);
        }
    }
}

void Si446xInterface::handleRxInvalid(int16_t rssi)
{
    LOG_DEBUG("Si446x: Invalid packet (CRC error)");
}

void Si446xInterface::handleSent()
{
    LOG_DEBUG("Si446x: Packet sent successfully");
    if (sendingPacket)
    {
        packetPool.release(sendingPacket);
        sendingPacket = nullptr;
    }
    // Return to RX
    Si446x_RX(_channel);
}

// Map the weak C callbacks to the class instance
extern "C"
{
    void SI446X_CB_RXBEGIN(int16_t rssi)
    {
        if (_instance) _instance->handleRxBegin(rssi);
    }
    void SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi)
    {
        if (_instance) _instance->handleRxComplete(length, rssi);
    }
    void SI446X_CB_RXINVALID(int16_t rssi)
    {
        if (_instance) _instance->handleRxInvalid(rssi);
    }
    void SI446X_CB_SENT()
    {
        if (_instance) _instance->handleSent();
    }
}
