#if defined(TROPICON2026)
#include "TalksModule.h"
#include "FSCommon.h"
#include "main.h"
#include <algorithm>
#include <set>
#include "graphics/Screen.h"
#include "mesh/MeshService.h"
#include <ArduinoJson.h>
#include "gps/RTC.h"

extern graphics::Screen *screen;

TalksModule *talksModule = nullptr;

TalksModule::TalksModule() : MeshModule("Talks"), concurrency::OSThread("TalksModule") {
    talksModule = this;
    // Removed loadSchedule() and loadTalleres() from constructor to save baseline RAM
    loadInterests();

    if (inputBroker) {
        inputObserver.observe(inputBroker);
        LOG_INFO("TalksModule: Input observer registered");
    } else {
        LOG_ERROR("TalksModule: inputBroker not available");
    }

    setIntervalFromNow(1000);
}

TalksModule::~TalksModule() {
    talksModule = nullptr;
}

// ── schedule.json parser ──────────────────────────────────────────────────────
// Each entry has up to two tracks: "Track 1" and "Track Villas".
// Entries with empty title or only whitespace are skipped.
void TalksModule::loadSchedule() {
    File file = FSCom.open("/schedule.json", FILE_O_READ);
    if (!file) {
        LOG_ERROR("TalksModule: Failed to open /schedule.json");
        return;
    }

    // Size the document for a ~25-entry array; each entry has ~8 string fields.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        LOG_ERROR("TalksModule: schedule.json parse error: %s", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject entry : arr) {
        const char* day     = entry["Día"]     | "";
        const char* diaMes  = entry["DiaMes"]  | "";
        const char* horario = entry["Horario"] | "";

        // Track 1
        const char* title1  = entry["Track 1"]        | "";
        const char* ponente1= entry["Ponente"]         | "";
        const char* img1    = entry["ImagenTrack1"]    | "";

        if (title1 && title1[0] != '\0') {
            Talk t;
            t.day     = day;
            t.date    = diaMes;
            t.time    = horario;
            t.stage   = STAGE_TRACK1;
            t.title   = title1;
            t.speaker = ponente1;
            t.image   = img1;
            t.interest = 0;
            t.id      = string(day) + "|" + horario + "|T1";
            talks.push_back(t);
        }

        // Track Villas
        const char* title2  = entry["Track Villas"]   | "";
        const char* ponente2= entry["Ponente.1"]       | "";
        const char* img2    = entry["ImagenVillas"]    | "";

        if (title2 && title2[0] != '\0') {
            Talk t;
            t.day     = day;
            t.date    = diaMes;
            t.time    = horario;
            t.stage   = STAGE_VILLAS;
            t.title   = title2;
            t.speaker = ponente2;
            t.image   = img2;
            t.interest = 0;
            t.id      = string(day) + "|" + horario + "|TV";
            talks.push_back(t);
        }
    }

    LOG_INFO("TalksModule: Loaded %d entries from schedule.json", (int)talks.size());
}

// ── talleres.json parser ──────────────────────────────────────────────────────
// Each entry has 4 workshop slots. Only slots with a non-empty "Taller" field
// are added, using the slot name ("Salón Maya", etc.) as the stage.
void TalksModule::loadTalleres() {
    File file = FSCom.open("/talleres.json", FILE_O_READ);
    if (!file) {
        LOG_ERROR("TalksModule: Failed to open /talleres.json");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        LOG_ERROR("TalksModule: talleres.json parse error: %s", err.c_str());
        return;
    }

    // Map from JSON key → stage display name & Talk id suffix
    struct SlotDef { const char* key; const char* stage; const char* imgKey; const char* idSuffix; };
    static const SlotDef slots[] = {
        { "Taller #1: Salón Maya",    STAGE_TALLER1, "ImgTaller1", "W1" },
        { "Taller #2: Salón la Isla", STAGE_TALLER2, "ImgTaller2", "W2" },
        { "Taller #3: Suite 1",       STAGE_TALLER3, "ImgTaller3", "W3" },
        { "Taller #4: Suite 2",       STAGE_TALLER4, "ImgTaller4", "W4" },
    };

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject entry : arr) {
        const char* day     = entry["Día"]     | "";
        const char* diaMes  = entry["DiaMes"]  | "";
        const char* horario = entry["Horario"] | "";

        for (const auto& slot : slots) {
            JsonObject workshop = entry[slot.key];
            if (workshop.isNull()) continue;

            const char* title  = workshop["Taller"]      | "";
            const char* ponente= workshop["Tallerista"]   | "";
            const char* img    = workshop[slot.imgKey]    | "";

            if (title && title[0] != '\0') {
                Talk t;
                t.day     = day;
                t.date    = diaMes;
                t.time    = horario;
                t.stage   = slot.stage;
                t.title   = title;
                t.speaker = ponente;
                t.image   = img;
                t.interest = 0;
                t.id      = string(day) + "|" + horario + "|" + slot.idSuffix;
                talks.push_back(t);
            }
        }
    }

    LOG_INFO("TalksModule: Loaded %d total entries (schedule + talleres)", (int)talks.size());
}

// ── Persistence ───────────────────────────────────────────────────────────────

void TalksModule::loadInterests() {
    File file = FSCom.open("/talks_int.dat", FILE_O_READ);
    if (!file) return;

    while (file.available()) {
        String id       = file.readStringUntil('$');
        String stateStr = file.readStringUntil('\n');
        int    state    = stateStr.toInt();

        for (auto& t : talks) {
            if (t.id == id.c_str()) {
                t.interest = state;
                break;
            }
        }
    }
    file.close();
}

void TalksModule::saveInterest(const string& talkId, int state) {
    for (auto& t : talks) {
        if (t.id == talkId) {
            t.interest = state;
            break;
        }
    }
    isDirty = true;
    lastInteractionMs = millis();
}

int32_t TalksModule::runOnce() {
    if (isDirty && millis() - lastInteractionMs > 5000) {
        File file = FSCom.open("/talks_int.dat", FILE_O_WRITE);
        if (file) {
            for (const auto& t : talks) {
                if (t.interest != 0) {
                    file.print(t.id.c_str());
                    file.print("$");
                    file.println(t.interest);
                }
            }
            file.close();
        }
        isDirty = false;
        LOG_INFO("TalksModule: Saved interests to flash");
    }

    // Check for upcoming talks every 30 seconds
    if (millis() - lastNotifCheckMs >= 30000) {
        lastNotifCheckMs = millis();
        // Ensure data is loaded (lazy-load) even if the user hasn't opened the UI yet
        ensureLoaded();
        checkAndSendNotifications();
    }

    return 1000;
}

// ── BLE Notifications ─────────────────────────────────────────────────────────

// Returns the start time in minutes-since-midnight parsed from the Horario field.
// Accepts both "HH:MM hrs" and "HH:MM - HH:MM hrs" (only start time is used).
// Returns -1 on parse failure.
static int parseTalkStartMinutes(const string& timeStr) {
    int h = 0, m = 0;
    if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
        return h * 60 + m;
    }
    return -1;
}

// Parses "DD/Mes/YYYY" (Spanish month name) into day-of-month, 0-based month,
// and full year.  Returns true on success.
static bool parseTalkDate(const string& dateStr, int &outDay, int &outMon, int &outYear) {
    char monthName[16] = {};
    int d = 0, y = 0;
    if (sscanf(dateStr.c_str(), "%d/%15[^/]/%d", &d, monthName, &y) != 3) return false;

    static const char* months[] = {
        "Enero","Febrero","Marzo","Abril","Mayo","Junio",
        "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"
    };
    int m = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(monthName, months[i]) == 0) { m = i; break; }
    }
    if (m < 0) return false;

    outDay  = d;
    outMon  = m;  // 0-based, matches tm_mon
    outYear = y;
    return true;
}

// Compares the current wall-clock time (from RTC/NTP) with each talk that the
// user has flagged as "Asistir" (interest == 1).  When a talk is exactly
// 10 minutes away (within a ±30-second window) a compact string is notified
// over the BLE GATT log characteristic so the paired mobile app can surface a
// system notification.
//
// The date comparison uses the "DiaMes" field ("DD/Mes/YYYY"), which is more
// robust than day-of-week matching across multi-day events.
//
// Notifications are never sent via the LoRa/Mesh radio to avoid traffic
// saturation – only the direct BLE link is used.
//
// Deduplication: once a notification has been sent for a given talk ID it is
// added to notifiedTalkIds and will not be re-sent.  The set is cleared when
// the calendar day changes so that the badge works correctly across multi-day
// events.
void TalksModule::checkAndSendNotifications() {
    RTCQuality quality = getRTCQuality();
    bool bleConnected = false;
#if defined(ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)
    if (nimbleBluetooth && nimbleBluetooth->isConnected()) {
        bleConnected = true;
    }
#endif

    // Match ClockRenderer's time source: local system time (local=true).
    uint32_t nowEpoch = getValidTime(RTCQualityDevice, true);

    LOG_INFO("TalksModule: [Notif Check] RTC:%s BLE:%s Epoch:%lu", 
             RtcName(quality), bleConnected ? "ON" : "OFF", (unsigned long)nowEpoch);

    if (nowEpoch == 0) {
        return;
    }

    // Since MESHTASTIC_EXCLUDE_TZ is set, getTZOffset() is 0 and getTime(true)
    // returns the "local-as-UTC" epoch shown on the badge clock.
    time_t localNow = (time_t)nowEpoch;
    struct tm tmNow;
    gmtime_r(&localNow, &tmNow);

    LOG_INFO("TalksModule: Notif check — Clock %02d/%02d/%04d %02d:%02d (local_epoch=%lu)",
             tmNow.tm_mday, tmNow.tm_mon + 1, tmNow.tm_year + 1900,
             tmNow.tm_hour, tmNow.tm_min, (unsigned long)localNow);

    // Clear the deduplication set once per UTC calendar day
    if (tmNow.tm_yday != lastNotifDay) {
        notifiedTalkIds.clear();
        lastNotifDay = tmNow.tm_yday;
        LOG_INFO("TalksModule: Notification dedup set cleared (UTC yday=%d)", lastNotifDay);
    }

    for (const auto& t : talks) {
        // Only "Asistir" (interest == 1)
        if (t.interest != 1) continue;

        // Skip if already notified today
        if (notifiedTalkIds.count(t.id)) continue;

        // Parse calendar date from "DiaMes" ("DD/Mes/YYYY")
        int talkDay = 0, talkMon = 0, talkYear = 0;
        if (!parseTalkDate(t.date, talkDay, talkMon, talkYear)) {
            LOG_INFO("TalksModule: Could not parse DiaMes '%s' for '%s'",
                     t.date.c_str(), t.title.c_str());
            continue;
        }

        // Match today's UTC date against the talk's date
        if (talkDay  != tmNow.tm_mday        ||
            talkMon  != tmNow.tm_mon          ||
            talkYear != tmNow.tm_year + 1900) {
            continue;
        }

        // Parse start time from "Horario" (UTC, as shown on the badge clock)
        int startMinutes = parseTalkStartMinutes(t.time);
        if (startMinutes < 0) {
            LOG_INFO("TalksModule: Could not parse Horario '%s' for '%s'",
                     t.time.c_str(), t.title.c_str());
            continue;
        }

        // Build the local epoch for today's talk start time.
        // Direct arithmetic ensures consistency with the ClockRenderer display.
        uint32_t todayStart = nowEpoch - (nowEpoch % 86400);
        uint32_t talkEpoch  = todayStart + (startMinutes * 60);

        // Notification window: [9m30s, 10m30s] before the talk — 60 s wide,
        // guaranteed to contain at least one 30-second polling tick
        int64_t delta = (int64_t)talkEpoch - (int64_t)nowEpoch;
        
        // Log all talks marked as 'Asistir' to help diagnose why they aren't triggering
        LOG_INFO("TalksModule:   - Talk '%s' @ %s (Epoch:%lu) -> Delta:%lld s",
                 t.title.c_str(), t.time.c_str(), (unsigned long)talkEpoch, (long long)delta);

        if (delta < 570 || delta > 630) continue;

        // Build the official ClientNotification payload
        meshtastic_ClientNotification *cn = clientNotificationPool.allocZeroed();
        if (cn) {
            cn->level = meshtastic_LogRecord_Level_INFO;
            cn->time = nowEpoch;
            snprintf(cn->message, sizeof(cn->message), "Tropicon: %s empieza en 10 min", t.title.c_str());
            
            if (bleConnected) {
                service->sendClientNotification(cn);
                LOG_INFO("TalksModule: !!! Notification sent to phone for '%s'", t.title.c_str());
            } else {
                // If not connected, we should probably release it or send anyway so it's in the queue
                // The API will handle queueing.
                service->sendClientNotification(cn);
                LOG_INFO("TalksModule: App not connected, notification queued for '%s'", t.title.c_str());
            }
        }

        // Mark notified regardless of delivery to avoid duplicates
        notifiedTalkIds.insert(t.id);
    }
}

// ── Queries ───────────────────────────────────────────────────────────────────

void TalksModule::ensureLoaded() {
    if (!isLoaded) {
        LOG_INFO("TalksModule: Lazy loading data...");
        loadSchedule();
        loadTalleres();
        loadInterests();
        isLoaded = true;
    }
}

vector<string> TalksModule::getDays() {
    ensureLoaded();
    vector<string> days;
    set<string>    seen;
    for (const auto& t : talks) {
        if (seen.insert(t.day).second)
            days.push_back(t.day);
    }
    return days;
}

vector<string> TalksModule::getStages(const string& day) {
    ensureLoaded();
    vector<string> stages;
    set<string>    seen;
    for (const auto& t : talks) {
        if (t.day == day && seen.insert(t.stage).second)
            stages.push_back(t.stage);
    }
    return stages;
}

vector<int> TalksModule::getFilteredTalkIndices(const string& day, const string& stage) {
    ensureLoaded();
    vector<int> indices;
    for (size_t i = 0; i < talks.size(); ++i) {
        if (talks[i].day == day && talks[i].stage == stage)
            indices.push_back((int)i);
    }
    return indices;
}

// ── Input handler ─────────────────────────────────────────────────────────────
//
// Esquema de navegación con 4 botones físicos:
//
//   Botón UP (IO40) — pulsación corta → INPUT_BROKER_USER_PRESS
//   Botón UP (IO40) — pulsación larga → INPUT_BROKER_SELECT
//   Botón DOWN (IO43)                 → INPUT_BROKER_DOWN
//   Botón LEFT (IO01)                 → INPUT_BROKER_LEFT
//   Botón RIGHT (IO18)                → INPUT_BROKER_RIGHT
//
// Vista lista (inDetailView = false):
//   UP corto  → plática anterior ↑ (no se sale de la lista)
//   DOWN      → plática siguiente ↓
//   LEFT      → día anterior (cíclico)
//   RIGHT     → siguiente stage; si es el último → siguiente día
//   UP largo  → entrar a vista detalle
//
// Vista detalle (inDetailView = true):
//   UP corto  → salir a lista
//   DOWN      → siguiente plática (permanece en detalle)
//   LEFT      → plática anterior (permanece en detalle; cambia stage/día si es necesario)
//   RIGHT     → siguiente plática (permanece en detalle; cambia stage/día si es necesario)
//   UP largo  → ciclar interés (like / estrella / saltar)

int TalksModule::handleInputEvent(const InputEvent *event)
{
    if (!screen || !screen->isScreenOn()) return 0;
    if (screen->getCurrentFrame() != graphics::Screen::FOCUS_TALKS) return 0;
    if (screen->isOverlayBannerShowing()) return 0;

    LOG_DEBUG("TalksModule input: %d", event->inputEvent);

    auto days    = getDays();
    int  numDays = (int)days.size();
    if (numDays == 0) return 0;
    if (currentDayIndex >= numDays) currentDayIndex = 0;

    auto stages    = getStages(days[currentDayIndex]);
    int  numStages = (int)stages.size();
    if (numStages == 0) return 0;
    if (currentStageIndex >= numStages) currentStageIndex = 0;

    auto talkIndices = getFilteredTalkIndices(days[currentDayIndex], stages[currentStageIndex]);
    int  numTalks    = (int)talkIndices.size();

    // UP corto (USER_PRESS):
    //   Lista   → plática anterior ↑; si ya estamos en la primera → menú principal
    //   Detalle → salir a lista
    if (event->inputEvent == INPUT_BROKER_USER_PRESS) {
        if (inDetailView) {
            inDetailView = false;
        } else {
            if (currentTalkIndex > 0) {
                currentTalkIndex--;
            } else {
                // Ya estamos en la primera plática: UP sale al menú principal
                screen->setFrames(graphics::Screen::FOCUS_DEFAULT);
            }
        }
        screen->runNow();
        return 1;
    }

    // DOWN:
    //   Lista   → plática siguiente ↓
    //   Detalle → plática siguiente (permanece en detalle)
    if (event->inputEvent == INPUT_BROKER_DOWN) {
        if (currentTalkIndex < numTalks - 1) {
            currentTalkIndex++;
        }
        screen->runNow();
        return 1;
    }

    // LEFT:
    //   Lista   → día anterior (cíclico), resetea stage y plática
    //   Detalle → plática anterior; si es la primera del stage → stage/día anterior
    if (event->inputEvent == INPUT_BROKER_LEFT) {
        if (inDetailView) {
            if (currentTalkIndex > 0) {
                currentTalkIndex--;
            } else if (currentStageIndex > 0) {
                currentStageIndex--;
                auto prevIndices = getFilteredTalkIndices(days[currentDayIndex], stages[currentStageIndex]);
                currentTalkIndex = (int)prevIndices.size() - 1;
                if (currentTalkIndex < 0) currentTalkIndex = 0;
            } else {
                currentDayIndex = (currentDayIndex - 1 + numDays) % numDays;
                auto newStages  = getStages(days[currentDayIndex]);
                currentStageIndex = (int)newStages.size() - 1;
                if (currentStageIndex < 0) currentStageIndex = 0;
                if (!newStages.empty()) {
                    auto prevIndices = getFilteredTalkIndices(days[currentDayIndex], newStages[currentStageIndex]);
                    currentTalkIndex = (int)prevIndices.size() - 1;
                    if (currentTalkIndex < 0) currentTalkIndex = 0;
                } else {
                    currentTalkIndex = 0;
                }
            }
        } else {
            currentDayIndex = (currentDayIndex - 1 + numDays) % numDays;
            auto newStages = getStages(days[currentDayIndex]);
            if (currentStageIndex >= (int)newStages.size()) currentStageIndex = 0;
            currentTalkIndex = 0;
        }
        screen->runNow();
        return 1;
    }

    // RIGHT:
    //   Lista   → siguiente stage; si es el último → siguiente día
    //   Detalle → plática siguiente; si es la última del stage → stage/día siguiente
    if (event->inputEvent == INPUT_BROKER_RIGHT) {
        if (inDetailView) {
            if (currentTalkIndex < numTalks - 1) {
                currentTalkIndex++;
            } else if (currentStageIndex < numStages - 1) {
                currentStageIndex++;
                currentTalkIndex = 0;
            } else {
                currentDayIndex   = (currentDayIndex + 1) % numDays;
                currentStageIndex = 0;
                currentTalkIndex  = 0;
            }
        } else {
            if (numStages > 1 && currentStageIndex < numStages - 1) {
                currentStageIndex++;
            } else {
                currentDayIndex   = (currentDayIndex + 1) % numDays;
                currentStageIndex = 0;
            }
            currentTalkIndex = 0;
        }
        screen->runNow();
        return 1;
    }

    // UP largo (SELECT):
    //   Lista   → entrar a vista detalle
    //   Detalle → ciclar interés (0→1→2→3→0)
    if (event->inputEvent == INPUT_BROKER_SELECT) {
        if (inDetailView) {
            if (numTalks > 0 && currentTalkIndex < numTalks) {
                int idx = talkIndices[currentTalkIndex];
                saveInterest(talks[idx].id, (talks[idx].interest + 1) % 4);
            }
        } else {
            if (numTalks > 0) inDetailView = true;
        }
        screen->runNow();
        return 1;
    }

    if (event->inputEvent == INPUT_BROKER_BACK ||
        event->inputEvent == INPUT_BROKER_CANCEL) {
        if (inDetailView) {
            inDetailView = false;
        } else {
            screen->setFrames(graphics::Screen::FOCUS_DEFAULT);
        }
        screen->runNow();
        return 1;
    }

    // SELECT_LONG: salir al menú principal desde cualquier nivel
    if (event->inputEvent == INPUT_BROKER_SELECT_LONG) {
        inDetailView = false;
        screen->setFrames(graphics::Screen::FOCUS_DEFAULT);
        screen->runNow();
        return 1;
    }

    return 0;
}

#endif
