#if defined(TROPICON2026)
#include "TalksRenderer.h"
#include "modules/TalksModule.h"
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include "graphics/TFTDisplay.h"
#include "main.h"

namespace graphics {

void TalksRenderer::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    if (!::talksModule) return;

    if (::talksModule->inDetailView) {
        drawTalkDetail(display, state, x, y);
    } else {
        drawTalkList(display, state, x, y);
    }
}

void TalksRenderer::drawTalkList(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    // Remove any PNG overlay when showing the list
    TFTDisplay::clearPngOverlay();

    auto days = ::talksModule->getDays();
    if (days.empty()) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(FONT_MEDIUM);
        display->drawString(x + display->getWidth() / 2, y + 20, "No talks found");
        return;
    }

    std::string currentDay = days[::talksModule->currentDayIndex];
    auto stages = ::talksModule->getStages(currentDay);
    if (stages.empty()) return;
    
    std::string currentStage = stages[::talksModule->currentStageIndex];
    auto talkIndices = ::talksModule->getFilteredTalkIndices(currentDay, currentStage);
    
    // Header
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->fillRect(x, y, display->getWidth(), 16);
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
    char header[64];
    snprintf(header, sizeof(header), "%s - %s", currentDay.c_str(), currentStage.c_str());
    display->drawString(x + display->getWidth() / 2, y, header);
    display->setColor(OLEDDISPLAY_COLOR::WHITE);

    // List
    int startY = y + 20;
    int rowHeight = 40;
    int visibleRows = (display->getHeight() - 20) / rowHeight;
    
    int scrollOffset = 0;
    if (::talksModule->currentTalkIndex >= visibleRows) {
        scrollOffset = ::talksModule->currentTalkIndex - visibleRows + 1;
    }

    for (int i = 0; i < visibleRows && (i + scrollOffset) < talkIndices.size(); ++i) {
        int idx = talkIndices[i + scrollOffset];
        const auto& talk = ::talksModule->getTalks()[idx];
        
        int itemY = startY + i * rowHeight;
        
        if (i + scrollOffset == ::talksModule->currentTalkIndex) {
            display->fillRect(x, itemY, display->getWidth(), rowHeight);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
        } else {
            display->drawRect(x, itemY, display->getWidth(), rowHeight);
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
        }
        
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(FONT_SMALL);
        display->drawString(x + 4, itemY + 2, talk.time.c_str());
        
        display->setFont(FONT_MEDIUM);
        // Truncate title if too long
        String title = talk.title.c_str();
        if (display->getStringWidth(title) > display->getWidth() - 10) {
            title = title.substring(0, 20) + "...";
        }
        display->drawString(x + 4, itemY + 12, title);
        
        display->setFont(FONT_SMALL);
        display->drawString(x + 4, itemY + 26, talk.speaker.c_str());

        // Interest marker
        if (talk.interest > 0) {
            const char* markers[] = {"", "[A]", "[T]", "[S]"};
            display->setTextAlignment(TEXT_ALIGN_RIGHT);
            display->drawString(x + display->getWidth() - 4, itemY + 2, markers[talk.interest]);
        }
        
        display->setColor(OLEDDISPLAY_COLOR::WHITE);
    }
}

void TalksRenderer::drawTalkDetail(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
    auto days = ::talksModule->getDays();
    std::string currentDay = days[::talksModule->currentDayIndex];
    auto stages = ::talksModule->getStages(currentDay);
    std::string currentStage = stages[::talksModule->currentStageIndex];
    auto talkIndices = ::talksModule->getFilteredTalkIndices(currentDay, currentStage);

    if (::talksModule->currentTalkIndex >= talkIndices.size()) return;

    int idx = talkIndices[::talksModule->currentTalkIndex];
    const auto& talk = ::talksModule->getTalks()[idx];

    // ── Image area constants ─────────────────────────────────────────────────
    // Reserve y+48 … y+189 (142 px tall) for the speaker image.
    // The OLEDDisplay buffer stays 0 (black) there so display() never overwrites
    // the PNG that TFTDisplay draws directly onto the TFT each changed frame.
    const int IMG_TOP    = y + 48;
    const int IMG_HEIGHT = 142;  // fits 130 px tall portrait + small margin
    const int IMG_BOTTOM = IMG_TOP + IMG_HEIGHT;
    const int IMG_CENTER_X = x + display->getWidth() / 2;  // 71 px

    // ── Header ───────────────────────────────────────────────────────────────
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    display->setFont(FONT_MEDIUM);
    display->drawString(x + 4, y + 4, talk.title.c_str());

    display->setFont(FONT_SMALL);
    display->drawString(x + 4, y + 20, talk.speaker.c_str());
    display->drawString(x + 4, y + 32, (talk.time + " @ " + talk.stage).c_str());
    display->drawHorizontalLine(x, y + 44, display->getWidth());

    // ── Speaker image ────────────────────────────────────────────────────────
    // Request PNG overlay; TFTDisplay handles decode, cache and blit.
    // If the talk has no image the overlay is cleared (no image shown).
    if (!talk.image.empty()) {
        TFTDisplay::setPngOverlay(talk.image.c_str(), IMG_CENTER_X, IMG_TOP,
                                  display->getWidth(), IMG_HEIGHT);
    } else {
        TFTDisplay::clearPngOverlay();
    }
    // Removed placeholder drawRect as it interferes with the "Draw-Once-and-Skip" strategy.
    // The image area is kept at 0 (black) in the OLED buffer naturally.

    // ── Description (word-wrapped title) ─────────────────────────────────────
    display->drawHorizontalLine(x, IMG_BOTTOM + 2, display->getWidth());
    display->setFont(FONT_SMALL);
    int descY = IMG_BOTTOM + 6;
    String title = talk.title.c_str();
    int lineStart = 0;
    while (lineStart < (int)title.length() && descY < display->getHeight() - 36) {
        int lineEnd = lineStart + 26;
        if (lineEnd >= (int)title.length()) {
            lineEnd = title.length();
        } else {
            int lastSpace = title.lastIndexOf(' ', lineEnd);
            if (lastSpace > lineStart) lineEnd = lastSpace;
        }
        display->drawString(x + 4, descY, title.substring(lineStart, lineEnd));
        lineStart = lineEnd + 1;
        descY += 12;
    }

    // ── Footer – interest selection ───────────────────────────────────────────
    int footerY = display->getHeight() - 30;
    display->drawHorizontalLine(x, footerY - 2, display->getWidth());
    const char *interestLabels[] = {"None", "Asistir", "Tal vez", "Saltar"};
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + display->getWidth() / 2, footerY,
                        (String("Status: ") + interestLabels[talk.interest]).c_str());
    display->drawString(x + display->getWidth() / 2, footerY + 12, "Select to change");
}

} // namespace graphics
#endif
