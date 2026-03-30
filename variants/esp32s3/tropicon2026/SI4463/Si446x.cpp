/*
 * Project: Si4463 Radio Library for Meshtastic (Adapted for ESP32-S3)
 * Original Author: Zak Kemble, contact@zakkemble.co.uk
 * Adapted by: Antigravity for ElectronicCats
 */

#include <Arduino.h>
#include <SPI.h>
#include <string.h>
#include <stdint.h>
#include "Si446x.h"
#include "Si446x_config.h"
#include "Si446x_defs.h"
#include "radio_config.h"

// ESP32 specific concurrency
#if defined(ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static portMUX_TYPE _si446x_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

#define IDLE_STATE SI446X_IDLE_MODE
#define MAX_PACKET_LEN SI446X_MAX_PACKET_LEN

#define IRQ_PACKET 0
#define IRQ_MODEM  1
#define IRQ_CHIP   2

#define rssi_dBm(val) ((val / 2) - 134)

static uint8_t _cs_pin;
static uint8_t _sdn_pin;
static int8_t _irq_pin;
static SPIClass* _spi_bus;
static volatile uint8_t isrBusy = 0;
static volatile uint8_t isrState_local = 0;
static volatile uint8_t enabledInterrupts[3];

#define delay_ms(ms) delay(ms)
#define delay_us(us) delayMicroseconds(us)
#define spiSelect()   digitalWrite(_cs_pin, LOW)
#define spiDeselect() digitalWrite(_cs_pin, HIGH)
#define spi_transfer_nr(data) _spi_bus->transfer(data)
#define spi_transfer(data)    _spi_bus->transfer(data)

static volatile bool _irq_enabled = false;

static const uint8_t config_data[] PROGMEM = RADIO_CONFIGURATION_DATA_ARRAY;

// Internal helpers for atomicity
static inline uint8_t interrupt_off(void)
{
    if (!isrBusy)
    {
        taskENTER_CRITICAL(&_si446x_mux);
    }
    return 1;
}

static inline uint8_t interrupt_on(void)
{
    if (!isrBusy)
    {
        taskEXIT_CRITICAL(&_si446x_mux);
    }
    return 0;
}

#define SI446X_ATOMIC() for(uint8_t _cs2 = interrupt_off(); _cs2; _cs2 = interrupt_on())

uint8_t Si446x_irq_off()
{
    _irq_enabled = false;
    isrState_local++;
    return 0;
}

void Si446x_irq_on(uint8_t origVal)
{
    (void)origVal;
    if (isrState_local > 0) isrState_local--;
    if (isrState_local == 0 && _irq_pin != -1) {
        _irq_enabled = true;
    }
}

// Redefinition removed to use the one from Si446x.h

static inline uint8_t cselect(void)
{
    _spi_bus->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    spiSelect();
    return 1;
}

static inline uint8_t cdeselect(void)
{
    spiDeselect();
    _spi_bus->endTransaction();
    return 0;
}

#define CHIPSELECT() for(uint8_t _cs = cselect(); _cs; _cs = cdeselect())

// Callbacks (weakly defined)
void __attribute__((weak)) SI446X_CB_CMDTIMEOUT(void) {}
void __attribute__((weak)) SI446X_CB_RXBEGIN(int16_t rssi) { (void)rssi; }
void __attribute__((weak)) SI446X_CB_RXCOMPLETE(uint8_t length, int16_t rssi) { (void)length; (void)rssi; }
void __attribute__((weak)) SI446X_CB_RXINVALID(int16_t rssi) { (void)rssi; }
void __attribute__((weak)) SI446X_CB_SENT(void) {}
void __attribute__((weak)) SI446X_CB_WUT(void) {}
void __attribute__((weak)) SI446X_CB_LOWBATT(void) {}
#if SI446X_ENABLE_ADDRMATCHING
void __attribute__((weak)) SI446X_CB_ADDRMATCH(void) {}
void __attribute__((weak)) SI446X_CB_ADDRMISS(void) {}
#endif

static uint8_t getResponse(void* buff, uint8_t len)
{
    uint8_t cts = 0;
    SI446X_ATOMIC() {
        CHIPSELECT() {
            spi_transfer_nr(SI446X_CMD_READ_CMD_BUFF);
            cts = (spi_transfer(0xFF) == 0xFF);
            if (cts)
            {
                for (uint8_t i = 0; i < len; i++)
                {
                    ((uint8_t*)buff)[i] = spi_transfer(0xFF);
                }
            }
        }
    }
    return cts;
}

static uint8_t waitForResponse(void* out, uint8_t outLen, uint8_t useTimeout) 
{
    uint16_t timeout = 4000;
    while (!getResponse(out, outLen))
    {
        delay_us(10);
		if(useTimeout && !--timeout)
		{
            SI446X_CB_CMDTIMEOUT();
            return 0;
        }
    }
    return 1;
}

static void doAPI(void* data, uint8_t len, void* out, uint8_t outLen)
{
    SI446X_NO_INTERRUPT() {
        if (waitForResponse(NULL, 0, 1)) {
            SI446X_ATOMIC()
            {
                CHIPSELECT()
                {
                    for (uint8_t i = 0; i < len; i++)
                    {
                        spi_transfer_nr(((uint8_t*)data)[i]);
                    }
                }
            }
            if (((uint8_t*)data)[0] == SI446X_CMD_IRCAL)
                waitForResponse(NULL, 0, 0);
            else if (out != NULL)
                waitForResponse(out, outLen, 1);
        }
    }
}

static void setProperties(uint16_t prop, void* values, uint8_t len)
{
    uint8_t data[16] = { SI446X_CMD_SET_PROPERTY, (uint8_t)(prop >> 8), len, (uint8_t)prop };
    memcpy(data + 4, values, len);
    doAPI(data, len + 4, NULL, 0);
}

static inline void setProperty(uint16_t prop, uint8_t value)
{
    setProperties(prop, &value, 1);
}

static void getProperties(uint16_t prop, void* values, uint8_t len)
{
    uint8_t data[] = { SI446X_CMD_GET_PROPERTY, (uint8_t)(prop >> 8), len, (uint8_t)prop };
    doAPI(data, sizeof(data), values, len);
}

static inline uint8_t getProperty(uint16_t prop)
{
    uint8_t val;
    getProperties(prop, &val, 1);
    return val;
}

static uint16_t getADC(uint8_t adc_en, uint8_t adc_cfg, uint8_t part)
{
    uint8_t data[6] = { SI446X_CMD_GET_ADC_READING, adc_en, adc_cfg };
    doAPI(data, 3, data, 6);
    return (data[part] << 8 | data[part + 1]);
}

// Read a fast response register
static uint8_t getFRR(uint8_t reg)
{
    uint8_t frr = 0;
	SI446X_ATOMIC()
	{
		CHIPSELECT()
		{
            spi_transfer_nr(reg);
            frr = spi_transfer(0xFF);
        }
    }
    return frr;
}

// Ge the patched RSSI from the beginning of the packet
static int16_t getLatchedRSSI(void)
{
    uint8_t frr = getFRR(SI446X_CMD_READ_FRR_A);
    return rssi_dBm(frr);
}

// Get current radio state
static si446x_state_t getState(void)
{
    uint8_t state = getFRR(SI446X_CMD_READ_FRR_B);
    if (state == SI446X_STATE_TX_TUNE) state = SI446X_STATE_TX;
    else if (state == SI446X_STATE_RX_TUNE) state = SI446X_STATE_RX;
    else if (state == SI446X_STATE_READY2) state = SI446X_STATE_READY;
    return (si446x_state_t)state;
}

static void setState(si446x_state_t newState)
{
    uint8_t data[] = 
    { 
        SI446X_CMD_CHANGE_STATE, 
        (uint8_t)newState 
    };
    doAPI(data, sizeof(data), NULL, 0);
}

static void clearFIFO(void)
{
    static const uint8_t clearFifo[] = 
    { 
        SI446X_CMD_FIFO_INFO, 
        SI446X_FIFO_CLEAR_RX | SI446X_FIFO_CLEAR_TX 
    };
    doAPI((uint8_t*)clearFifo, sizeof(clearFifo), NULL, 0);
}

static void interrupt(void* buff)
{
    uint8_t data = SI446X_CMD_GET_INT_STATUS;
    doAPI(&data, sizeof(data), buff, 8);
}

static void interrupt2(void* buff, uint8_t clearPH, uint8_t clearMODEM, uint8_t clearCHIP)
{
    uint8_t data[] = 
    { 
        SI446X_CMD_GET_INT_STATUS, 
        clearPH, 
        clearMODEM, 
        clearCHIP 
    };
    doAPI(data, sizeof(data), buff, 8);
}

static void resetDevice(void)
{
    digitalWrite(_sdn_pin, HIGH);
    delay_ms(50);
    digitalWrite(_sdn_pin, LOW);
    delay_ms(50);
}

static void applyStartupConfig(void)
{
    uint8_t buff[17];
    for (uint16_t i = 0; i < sizeof(config_data); )
    {
        memcpy_P(buff, &config_data[i], sizeof(buff));
        doAPI(&buff[1], buff[0], NULL, 0);
        i += (buff[0] + 1);
    }
}

void Si446x_init(uint8_t cs, uint8_t sdn, int8_t irq, void* spi)
{
    _cs_pin = cs;
    _sdn_pin = sdn;
    _irq_pin = irq;
    _spi_bus = (SPIClass*)spi;

    spiDeselect();
    pinMode(_cs_pin, OUTPUT);
    pinMode(_sdn_pin, OUTPUT);
    if (_irq_pin != -1) pinMode(_irq_pin, INPUT_PULLUP);

    resetDevice();
    applyStartupConfig();
    interrupt(NULL);
    Si446x_sleep();

    enabledInterrupts[IRQ_PACKET] = (1 << SI446X_PACKET_RX_PEND) | (1 << SI446X_CRC_ERROR_PEND);
    
    // Attach interrupt permanently once at boot, then state is toggled via _irq_enabled
    if (_irq_pin != -1) {
        attachInterrupt(digitalPinToInterrupt(_irq_pin), Si446x_SERVICE, FALLING);
    }
    Si446x_irq_on(1);
}

void Si446x_getInfo(si446x_info_t* info)
{
    uint8_t data[8]     = { SI446X_CMD_PART_INFO };
    doAPI(data, 1, data, 8);
    info->chipRev       = data[0];
    info->part          = (data[1] << 8) | data[2];
    info->partBuild     = data[3];
    info->id            = (data[4] << 8) | data[5];
    info->customer      = data[6];
    info->romId         = data[7];
    data[0]             = SI446X_CMD_FUNC_INFO;
    doAPI(data, 1, data, 6);
    info->revExternal   = data[0];
    info->revBranch     = data[1];
    info->revInternal   = data[2];
    info->patch         = (data[3] << 8) | data[4];
    info->func          = data[5];
}

int16_t Si446x_getRSSI()
{
    uint8_t data[3] = { SI446X_CMD_GET_MODEM_STATUS, 0xFF };
    doAPI(data, 2, data, 3);
    return rssi_dBm(data[2]);
}

si446x_state_t Si446x_getState()
{
    return getState();
}

void Si446x_setTxPower(uint8_t pwr)
{
    setProperty(SI446X_PA_PWR_LVL, pwr);
}

void Si446x_setLowBatt(uint16_t voltage)
{
    uint8_t batt = (voltage / 50) - 30;
    setProperty(SI446X_GLOBAL_LOW_BATT_THRESH, batt);
}

void Si446x_setupWUT(uint8_t r, uint16_t m, uint8_t ldc, uint8_t config)
{
    if (!(config & (SI446X_WUT_RUN | SI446X_WUT_BATT | SI446X_WUT_RX))) return;
    SI446X_NO_INTERRUPT()
    {
        setProperty(SI446X_GLOBAL_WUT_CONFIG, 0);
        uint8_t doRun = !!(config & SI446X_WUT_RUN);
        uint8_t doBatt = !!(config & SI446X_WUT_BATT);
        uint8_t doRx = (config & SI446X_WUT_RX);
        uint8_t intChip = (doBatt << SI446X_INT_CTL_CHIP_LOW_BATT_EN) | (doRun << SI446X_INT_CTL_CHIP_WUT_EN);
        enabledInterrupts[IRQ_CHIP] = intChip;
        setProperty(SI446X_INT_CTL_CHIP_ENABLE, intChip);
        if (getProperty(SI446X_GLOBAL_CLK_CFG) != SI446X_DIVIDED_CLK_32K_SEL_RC)
        {
            setProperty(SI446X_GLOBAL_CLK_CFG, SI446X_DIVIDED_CLK_32K_SEL_RC);
            delay_us(300);
        }
        uint8_t props[5] = 
        {
            (uint8_t)(doRx ? SI446X_GLOBAL_WUT_CONFIG_WUT_LDC_EN_RX : 0 | doBatt << SI446X_GLOBAL_WUT_CONFIG_WUT_LBD_EN | (1 << SI446X_GLOBAL_WUT_CONFIG_WUT_EN)),
            (uint8_t)(m >> 8), 
            (uint8_t)m, 
            (uint8_t)(r | SI446X_LDC_MAX_PERIODS_TWO | (1 << SI446X_WUT_SLEEP)), 
            ldc
        };
        setProperties(SI446X_GLOBAL_WUT_CONFIG, props, 5);
    }
}

void Si446x_disableWUT()
{
    SI446X_NO_INTERRUPT()
    {
        setProperty(SI446X_GLOBAL_WUT_CONFIG, 0);
        setProperty(SI446X_GLOBAL_CLK_CFG, 0);
    }
}

void Si446x_setupCallback(uint16_t callbacks, uint8_t state)
{
    SI446X_NO_INTERRUPT()
    {
        uint8_t data[2];
        getProperties(SI446X_INT_CTL_PH_ENABLE, data, 2);
        if (state)
        {
            data[0] |= (callbacks >> 8);
            data[1] |= (uint8_t)callbacks;
        }
        else
        {
            data[0] &= ~(callbacks >> 8);
            data[1] &= ~(uint8_t)callbacks;
        }
        enabledInterrupts[IRQ_PACKET] = data[0];
        enabledInterrupts[IRQ_MODEM] = data[1];
        setProperties(SI446X_INT_CTL_PH_ENABLE, data, 2);
    }
}

uint8_t Si446x_sleep()
{
    if (getState() == SI446X_STATE_TX) return 0;
    setState(SI446X_STATE_SLEEP);
    return 1;
}

void Si446x_read(void* buff, uint8_t len)
{
    SI446X_ATOMIC()
    {
        CHIPSELECT()
        {
            spi_transfer_nr(SI446X_CMD_READ_RX_FIFO);
            for (uint8_t i = 0; i < len; i++)
            {
                ((uint8_t*)buff)[i] = spi_transfer(0xFF);
            }
        }
    }
}

uint8_t Si446x_TX(void* packet, uint8_t len, uint8_t channel, si446x_state_t onTxFinish)
{
    SI446X_NO_INTERRUPT()
    {
        if (getState() == SI446X_STATE_TX)
        {
            return 0;
        }
        setState(IDLE_STATE);
        clearFIFO();
        interrupt2(NULL, 0, 0, 0xFF);
        SI446X_ATOMIC()
        {
            CHIPSELECT()
            {
                spi_transfer_nr(SI446X_CMD_WRITE_TX_FIFO);
#if !SI446X_FIXED_LENGTH
                spi_transfer_nr(len);
                for (uint8_t i = 0; i < len; i++)
                {
                    spi_transfer_nr(((uint8_t*)packet)[i]);
                }
#else
                for (uint8_t i = 0; i < SI446X_FIXED_LENGTH; i++)
                {
                    spi_transfer_nr(((uint8_t*)packet)[i]);
                }
#endif
            }
        }
#if !SI446X_FIXED_LENGTH
        setProperty(SI446X_PKT_FIELD_2_LENGTH_LOW, len);
#endif
        uint8_t data[] =
        { 
            SI446X_CMD_START_TX,
            channel,
            (uint8_t)(onTxFinish << 4),
            0,
            SI446X_FIXED_LENGTH,
            0,
            0
        };
        doAPI(data, sizeof(data), NULL, 0);
#if !SI446X_FIXED_LENGTH
        setProperty(SI446X_PKT_FIELD_2_LENGTH_LOW, MAX_PACKET_LEN);
#endif
    }
    return 1;
}

void Si446x_RX(uint8_t channel)
{
    SI446X_NO_INTERRUPT() {
        setState(IDLE_STATE);
        clearFIFO();
        interrupt2(NULL, 0, 0, 0xFF);
        uint8_t data[] = { SI446X_CMD_START_RX, channel, 0, 0, SI446X_FIXED_LENGTH, SI446X_STATE_NOCHANGE, (uint8_t)IDLE_STATE, (uint8_t)SI446X_STATE_SLEEP };
        doAPI(data, sizeof(data), NULL, 0);
    }
}

uint16_t Si446x_adc_gpio(uint8_t pin)
{
    return getADC(SI446X_ADC_CONV_GPIO | pin, (SI446X_ADC_SPEED << 4) | SI446X_ADC_RANGE_3P6, 0);
}

uint16_t Si446x_adc_battery()
{
    uint16_t res = getADC(SI446X_ADC_CONV_BATT, (SI446X_ADC_SPEED << 4), 2);
    return ((uint32_t)res * 75) / 32;
}

float Si446x_adc_temperature()
{
    float res = getADC(SI446X_ADC_CONV_TEMP, (SI446X_ADC_SPEED << 4), 4);
    return (899 / 4096.0) * res - 293;
}

void Si446x_writeGPIO(si446x_gpio_t pin, uint8_t value)
{
    uint8_t data[] =
    {
        SI446X_CMD_GPIO_PIN_CFG,
        SI446X_GPIO_MODE_DONOTHING,
        SI446X_GPIO_MODE_DONOTHING,
        SI446X_GPIO_MODE_DONOTHING,
        SI446X_GPIO_MODE_DONOTHING,
        SI446X_NIRQ_MODE_DONOTHING,
        SI446X_SDO_MODE_DONOTHING,
        SI446X_GPIO_DRV_HIGH
    };
    data[pin + 1] = value;
    doAPI(data, sizeof(data), NULL, 0);
}

uint8_t Si446x_readGPIO()
{
    uint8_t data[4] = { SI446X_CMD_GPIO_PIN_CFG };
    doAPI(data, 1, data, 4);
    return (data[0] >> 7) | ((data[1] & 0x80) >> 6) | ((data[2] & 0x80) >> 5) | ((data[3] & 0x80) >> 4);
}

void Si446x_SERVICE()
{
    if (!_irq_enabled) return;
    isrBusy = 1;
    uint8_t ints[8];
    interrupt(ints);
    ints[2] &= enabledInterrupts[IRQ_PACKET];
    ints[4] &= enabledInterrupts[IRQ_MODEM];
    ints[6] &= enabledInterrupts[IRQ_CHIP];

    if (ints[4] & (1 << SI446X_SYNC_DETECT_PEND))
    {
        SI446X_CB_RXBEGIN(getLatchedRSSI());
    }
    if (ints[2] & (1 << SI446X_PACKET_RX_PEND))
    {
        uint8_t len = SI446X_FIXED_LENGTH ? SI446X_FIXED_LENGTH : 0;
        if (!len)
        {
            Si446x_read(&len, 1);
        }
        SI446X_CB_RXCOMPLETE(len, getLatchedRSSI());
    }
    if (ints[2] & (1 << SI446X_CRC_ERROR_PEND))
    {
        if (getState() == SI446X_STATE_SPI_ACTIVE) setState(IDLE_STATE);
        SI446X_CB_RXINVALID(getLatchedRSSI());
    }
    if (ints[2] & (1 << SI446X_PACKET_SENT_PEND))
    {
        SI446X_CB_SENT();
    }
    if (ints[6] & (1 << SI446X_LOW_BATT_PEND))
    {
        SI446X_CB_LOWBATT();
    }
    if (ints[6] & (1 << SI446X_WUT_PEND))
    {
        SI446X_CB_WUT();
    }
    isrBusy = 0;
}
