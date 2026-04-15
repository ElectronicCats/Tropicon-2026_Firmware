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
        String title = talk.title.c_str();
        if (display->getStringWidth(title) > display->getWidth() - 10) {
            while (title.length() > 1 &&
                   display->getStringWidth(title + "...") > display->getWidth() - 10) {
                title = title.substring(0, title.length() - 1);
            }
            title = title + "...";
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

    // ── Header ───────────────────────────────────────────────────────────────
    // The title may wrap to a second line; yOff tracks extra vertical space used.
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_MEDIUM);

    String titleStr  = talk.title.c_str();
    int    titleMaxW = display->getWidth() - 8;  // 4 px margin each side
    int    yOff      = 0;  // extra offset when title needs 2 lines

    if (display->getStringWidth(titleStr) <= titleMaxW) {
        display->drawString(x + 4, y + 4, titleStr);
    } else {
        // Word-wrap: build the longest line 1 that fits
        String line1 = titleStr;
        while (line1.length() > 1 && display->getStringWidth(line1) > titleMaxW) {
            int sp = line1.lastIndexOf(' ');
            if (sp > 0) line1 = line1.substring(0, sp);
            else        line1 = line1.substring(0, line1.length() - 1);
        }
        String line2 = titleStr.substring(line1.length() + 1);
        // Truncate line2 with "..." if it still overflows
        if (display->getStringWidth(line2) > titleMaxW) {
            while (line2.length() > 1 &&
                   display->getStringWidth(line2 + "...") > titleMaxW) {
                int sp = line2.lastIndexOf(' ');
                if (sp > 0) line2 = line2.substring(0, sp);
                else        line2 = line2.substring(0, line2.length() - 1);
            }
            line2 = line2 + "...";
        }
        display->drawString(x + 4, y + 4,  line1);
        display->drawString(x + 4, y + 17, line2);
        yOff = 14;  // push the rest of the header + image down 14 px
    }

    display->setFont(FONT_SMALL);
    display->drawString(x + 4, y + 20 + yOff, talk.speaker.c_str());
    display->drawString(x + 4, y + 32 + yOff, (talk.time + " @ " + talk.stage).c_str());
    display->drawHorizontalLine(x, y + 44 + yOff, display->getWidth());

    // ── Image area constants ─────────────────────────────────────────────────
    // Reserve space below the header for the speaker image.
    // yOff shifts the image down when the title used 2 lines; IMG_HEIGHT shrinks
    // by the same amount so IMG_BOTTOM (and the description area) stays fixed.
    // The OLEDDisplay buffer stays 0 (black) there so display() never overwrites
    // the PNG that TFTDisplay draws directly onto the TFT each changed frame.
    int IMG_TOP    = y + 48 + yOff;
    int IMG_HEIGHT = 142  - yOff;  // fits 130 px tall portrait + small margin
    int IMG_BOTTOM = IMG_TOP + IMG_HEIGHT;
    const int IMG_CENTER_X = x + display->getWidth() / 2;  // 71 px

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
