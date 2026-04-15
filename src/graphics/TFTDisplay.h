#pragma once

#include <GpioLogic.h>
#include <OLEDDisplay.h>

/**
 * An adapter class that allows using the LovyanGFX library as if it was an OLEDDisplay implementation.
 *
 * Remaining TODO:
 * optimize display() to only draw changed pixels (see other OLED subclasses for examples)
 * Use the fast NRF52 SPI API rather than the slow standard arduino version
 *
 * turn radio back on - currently with both on spi bus is fucked? or are we leaving chip select asserted?
 */
class TFTDisplay : public OLEDDisplay
{
  public:
    /* constructor
    FIXME - the parameters are not used, just a temporary hack to keep working like the old displays
    */
    TFTDisplay(uint8_t, int, int, OLEDDISPLAY_GEOMETRY, HW_I2C);

    // Destructor to clean up allocated memory
    ~TFTDisplay();

    // Write the buffer to the display memory
    virtual void display() override { display(false); };
    virtual void display(bool fromBlank);
    void sdlLoop();

    // Turn the display upside down
    virtual void flipScreenVertically();

    // Touch screen (static handlers)
    static bool hasTouch(void);
    static bool getTouch(int16_t *x, int16_t *y);

    // Functions for changing display brightness
    void setDisplayBrightness(uint8_t);

    /**
     * shim to make the abstraction happy
     *
     */
    void setDetected(uint8_t detected);

    /**
     * This is normally managed entirely by TFTDisplay, but some rare applications (heltec tracker) might need to replace the
     * default GPIO behavior with something a bit more complex.
     *
     * We (cruftily) make it static so that variant.cpp can access it without needing a ptr to the TFTDisplay instance.
     */
    static GpioPin *backlightEnable;

#if defined(TROPICON2026)
    /**
     * Read an uncompressed 24-bit BMP from the filesystem and push it as an
     * RGB565 overlay directly into the TFT's GRAM.  The image is center-cropped
     * (not scaled) to fit within maxW × maxH.  Subsequent display() calls skip
     * those coordinates so the image persists without a repaint.
     * Call clearPngOverlay() to remove it.
     *
     * @param path     Filesystem path, e.g. "data/img/Foo.bmp" or "/img/Foo.bmp"
     * @param centerX  Horizontal center of the target area (TFT absolute coords)
     * @param topY     Top-left Y of the target area (TFT absolute coords)
     * @param maxW     Maximum rendered width in pixels
     * @param maxH     Maximum rendered height in pixels
     */
    static void setPngOverlay(const char *path, int16_t centerX, int16_t topY, int16_t maxW, int16_t maxH, bool drawBorder = true);
    static void clearPngOverlay();
#endif

  protected:
    // the header size of the buffer used, e.g. for the SPI command header
    virtual int getBufferOffset(void) override { return 0; }

    // Send a command to the display (low level function)
    virtual void sendCommand(uint8_t com) override;

    // Connect to the display
    virtual bool connect() override;

    uint16_t *linePixelBuffer = nullptr;
};