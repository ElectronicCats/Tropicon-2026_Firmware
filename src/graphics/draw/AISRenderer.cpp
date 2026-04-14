#if defined(TROPICON2026) && defined(USE_AIS)
#include "AISRenderer.h"
#include "AIS/AISModule.h"
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include <Arduino.h>

namespace graphics {

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants (display is 428 × 142 px in landscape, rotation 1)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int16_t HEADER_H  = 16;
static constexpr int16_t ROW_H     = 26;
static constexpr int16_t COL_MMSI  = 4;
static constexpr int16_t COL_CH    = 100;
static constexpr int16_t COL_AGE   = 130;

void AISRenderer::drawFrame(OLEDDisplay *display, OLEDDisplayUiState * /*state*/,
                             int16_t x, int16_t y)
{
    const int16_t W = display->getWidth();
    const int16_t H = display->getHeight();

    // ── Header bar ──────────────────────────────────────────────────────────
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->fillRect(x, y, W, HEADER_H);
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);

    uint32_t frames  = AISModule::getFrameCount();
    uint8_t  channel = AISModule::getCurrentChannel();
    char header[48];
    snprintf(header, sizeof(header), "AIS  ch %c  %lu frm",
             channel ? 'B' : 'A', (unsigned long)frames);
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

    // Column headers
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + COL_MMSI, curY, "MMSI");
    display->drawString(x + COL_CH,   curY, "Ch");
    display->drawString(x + COL_AGE,  curY, "s ago");
    curY += 12;
    display->drawHorizontalLine(x, curY, W);
    curY += 2;

    // Rows — most recent first
    uint32_t now_ms = (uint32_t)millis();
    int shown = 0;
    for (int i = (int)vessels.size() - 1; i >= 0 && curY + ROW_H <= y + H; i--) {
        const AISVesselInfo &v = vessels[i];

        // Highlight selected row (simple alternating for readability)
        if (shown % 2 == 0) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->drawRect(x, curY, W, ROW_H);
        }

        display->setColor(OLEDDISPLAY_COLOR::WHITE);
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        // MMSI
        display->setFont(FONT_MEDIUM);
        char mmsi_str[12];
        snprintf(mmsi_str, sizeof(mmsi_str), "%09lu", (unsigned long)v.mmsi);
        display->drawString(x + COL_MMSI, curY + 2, mmsi_str);

        // Channel
        display->setFont(FONT_SMALL);
        display->drawString(x + COL_CH, curY + 8, v.channel ? "B" : "A");

        // Age in seconds
        uint32_t age_s = (now_ms - v.seenAt_ms) / 1000;
        char age_str[8];
        if (age_s < 3600)
            snprintf(age_str, sizeof(age_str), "%lu", (unsigned long)age_s);
        else
            snprintf(age_str, sizeof(age_str), ">1h");
        display->drawString(x + COL_AGE, curY + 8, age_str);

        // Message type badge (top-right corner)
        char type_str[6];
        snprintf(type_str, sizeof(type_str), "T%u", v.msgType);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(x + W - 4, curY + 2, type_str);

        curY += ROW_H;
        shown++;
    }
}

} // namespace graphics
#endif
