#if defined(TROPICON2026) && defined(USE_AIS)
#include "AISRenderer.h"
#include "AIS/AISModule.h"
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include <Arduino.h>

namespace graphics
{

static constexpr int16_t HEADER_H = 16;
static constexpr int16_t ROW_H = 28;

void AISRenderer::drawFrame(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    const int16_t W = display->getWidth();
    const int16_t H = display->getHeight();

    // ── Header bar ──────────────────────────────────────────────────────────
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->fillRect(x, y, W, HEADER_H);
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);

    uint32_t frames = AISModule::getFrameCount();
    uint8_t channel = AISModule::getCurrentChannel();
    char header[48];
    snprintf(header, sizeof(header), "AIS  ch %c  %lu frm", (channel == 21) ? 'B' : 'A', (unsigned long)frames);
    display->drawString(x + W / 2, y + 1, header);

    display->setColor(OLEDDISPLAY_COLOR::WHITE);

    // ── Vessel list ──────────────────────────────────────────────────────────
    auto vessels = AISModule::getRecentVessels();

    if (vessels.empty()) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(FONT_MEDIUM);
        display->drawString(x + W / 2, y + H / 2 - 8, "Listening...");
        return;
    }

    int16_t curY = y + HEADER_H + 2;

    // Rows — most recent first
    int shown = 0;
    for (int i = (int)vessels.size() - 1; i >= 0 && curY + ROW_H <= y + H; i--) {
        const AISVesselInfo &v = vessels[i];

        // Alternating row highlight
        if (shown % 2 == 0) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->drawRect(x, curY, W, ROW_H);
        }

        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        // Line 1: MMSI + RSSI + age
        display->setFont(FONT_MEDIUM);
        uint32_t age_s = ((uint32_t)millis() - v.seenAt_ms) / 1000;
        char line1[64];
        if (age_s < 3600)
            snprintf(line1, sizeof(line1), "%09lu  %ddBm  %lus", (unsigned long)v.mmsi, v.rssi, (unsigned long)age_s);
        else
            snprintf(line1, sizeof(line1), "%09lu  %ddBm  >1h", (unsigned long)v.mmsi, v.rssi);
        display->drawString(x + 4, curY, line1);

        // Message type badge (top-right)
        char type_str[6];
        snprintf(type_str, sizeof(type_str), "T%u", v.msgType);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->setFont(FONT_SMALL);
        display->drawString(x + W - 4, curY + 1, type_str);

        // Line 2: NMEA sentence (truncated to fit)
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(FONT_SMALL);
        display->drawString(x + 4, curY + 15, v.nmea);

        curY += ROW_H;
        shown++;
    }
}

} // namespace graphics
#endif
