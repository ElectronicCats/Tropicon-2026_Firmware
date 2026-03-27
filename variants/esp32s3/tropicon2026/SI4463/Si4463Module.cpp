#include "Si4463Module.h"
#include "Si446xInterface.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"

Si4463Module::Si4463Module(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi)
    : MeshModule("Si4463"), _cs(cs), _sdn(sdn), _irq(irq), _spi(spi) {
}

Si4463Module::~Si4463Module()
{
    if (_radioIf)
    {
        delete _radioIf;
    }
}

void Si4463Module::setup()
{
    MeshModule::setup();
    
    LOG_INFO("Si4463Module: Setting up radio interface");
    
    // Create the RadioInterface wrapper
    _radioIf = new Si446xInterface(_cs, _sdn, _irq, _spi);
    
    // Initialize the physical radio
    if (_radioIf->init())
    {
        LOG_INFO("Si4463Module: Radio initialized successfully");
    }
    else
    {
        LOG_ERROR("Si4463Module: Radio initialization FAILED");
    }
}

bool Si4463Module::wantPacket(const meshtastic_MeshPacket *p)
{
    // Return true if you want to forward specific packets via SI4463.
    // E.g., if (p->decoded.portnum == meshtastic_PortNum_TELEMETRY_APP) return true;
    return false; // Set to true to test outgoing packets
}

ProcessMessage Si4463Module::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Forward mesh packets to this radio
    if (_radioIf)
    {
        _radioIf->send(const_cast<meshtastic_MeshPacket *>(&mp));
    }
    return ProcessMessage::CONTINUE;
}
