#pragma once
#if defined(TROPICON2026)
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

namespace graphics {

class TalksRenderer {
public:
    static void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    
private:
    static void drawTalkList(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    static void drawTalkDetail(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    static void drawDaySelection(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    static void drawStageSelection(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
};

} // namespace graphics
#endif
