#include "Si4463Module.h"
#include "Si446xInterface.h"
#include "MeshService.h"
#include "configuration.h"
#include "main.h"

Si4463Module::Si4463Module(uint8_t cs, uint8_t sdn, int8_t irq, SPIClass& spi)
    : MeshModule("Si4463"), _cs(cs), _sdn(sdn), _irq(irq), _spi(spi) {
    isPromiscuous = true; // See all packets in the mesh
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
    // Return true to receive all packets and forward them via SI4463.
    return true; 
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
