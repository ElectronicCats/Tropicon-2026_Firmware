#pragma once
#if defined(TROPICON2026) && defined(USE_AIS)
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

namespace graphics {

class AISRenderer {
public:
    static void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
};

} // namespace graphics
#endif
