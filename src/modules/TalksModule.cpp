#if defined(TROPICON2026)
#include "TalksModule.h"
#include "FSCommon.h"
#include "main.h"
#include <algorithm>
#include <set>
#include "graphics/Screen.h"

extern graphics::Screen *screen;

TalksModule *talksModule = nullptr;

TalksModule::TalksModule() : MeshModule("Talks"), concurrency::OSThread("TalksModule") {
    talksModule = this;
    loadSchedule();
    loadInterests();
    
    // Registrar observer de input CORRECTAMENTE
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

void TalksModule::loadSchedule() {
    // Meshtastic filesystem is usually mounted at /
    File file = FSCom.open("/schedule.csv", FILE_O_READ);
    if (!file) {
        LOG_ERROR("Failed to open /schedule.csv");
        return;
    }
    
    talks.clear();
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            parseCSVLine(line.c_str());
        }
    }
    file.close();
    LOG_INFO("Loaded %d talks from schedule.csv", (int)talks.size());
}

void TalksModule::parseCSVLine(const string& line) {
    Talk talk;
    vector<string> parts;
    size_t start = 0;
    size_t end = line.find('$');
    while (end != string::npos) {
        parts.push_back(line.substr(start, end - start));
        start = end + 1;
        end = line.find('$', start);
    }
    parts.push_back(line.substr(start));
    
    if (parts.size() < 7) return;
    
    talk.day = parts[0];
    talk.time = parts[1];
    talk.stage = parts[2];
    talk.title = parts[3];
    talk.speaker = parts[4];
    talk.image = parts[5];
    talk.description = parts[6];
    talk.interest = 0;
    
    // Create a simple ID for persistence
    talk.id = talk.title; // For simplicity, title is the ID if unique
    
    talks.push_back(talk);
}

void TalksModule::loadInterests() {
    File file = FSCom.open("/talks_int.dat", FILE_O_READ);
    if (!file) return;
    
    while (file.available()) {
        String id = file.readStringUntil('$');
        String stateStr = file.readStringUntil('\n');
        int state = stateStr.toInt();
        
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
    // Find and update in memory
    for (auto& t : talks) {
        if (t.id == talkId) {
            t.interest = state;
            break;
        }
    }
    
    // Mark as dirty and schedule flash write
    isDirty = true;
    lastInteractionMs = millis();
}

int32_t TalksModule::runOnce() {
    if (isDirty && millis() - lastInteractionMs > 5000) {
        // Simple persistence: rewrite the file
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
    return 1000; // Run again in 1 second
}

vector<string> TalksModule::getDays() {
    set<string> days;
    for (const auto& t : talks) {
        days.insert(t.day);
    }
    return vector<string>(days.begin(), days.end());
}

vector<string> TalksModule::getStages(const string& day) {
    set<string> stages;
    for (const auto& t : talks) {
        if (t.day == day) {
            stages.insert(t.stage);
        }
    }
    return vector<string>(stages.begin(), stages.end());
}

vector<int> TalksModule::getFilteredTalkIndices(const string& day, const string& stage) {
    vector<int> indices;
    for (size_t i = 0; i < talks.size(); ++i) {
        if (talks[i].day == day && talks[i].stage == stage) {
            indices.push_back((int)i);
        }
    }
    return indices;
}

int TalksModule::handleInputEvent(const InputEvent *event)
{
    // Guards
    if (!screen || !screen->isScreenOn()) return 0;
    if (screen->getCurrentFrame() != graphics::Screen::FOCUS_TALKS) return 0;
    if (screen->isOverlayBannerShowing()) return 0;

    LOG_DEBUG("TalksModule input: %d", event->inputEvent);

    // ── Pre-compute context defensively ──────────────────────────────────────
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

    // ── UP: charla anterior ───────────────────────────────────────────────────
    if (event->inputEvent == INPUT_BROKER_UP) {
        if (!inDetailView) {
            if (currentTalkIndex > 0) currentTalkIndex--;
        }
        screen->runNow();
        return 1;
    }

    // ── DOWN: charla siguiente ────────────────────────────────────────────────
    if (event->inputEvent == INPUT_BROKER_DOWN) {
        if (!inDetailView) {
            if (currentTalkIndex < numTalks - 1) currentTalkIndex++;
        }
        screen->runNow();
        return 1;
    }

    // ── LEFT: día anterior (cicla) ────────────────────────────────────────────
    if (event->inputEvent == INPUT_BROKER_LEFT) {
        if (!inDetailView) {
            currentDayIndex = (currentDayIndex - 1 + numDays) % numDays;
            auto newStages = getStages(days[currentDayIndex]);
            if (currentStageIndex >= (int)newStages.size()) currentStageIndex = 0;
            currentTalkIndex = 0;
        }
        screen->runNow();
        return 1;
    }

    // ── RIGHT: día siguiente / stage siguiente ────────────────────────────────
    if (event->inputEvent == INPUT_BROKER_RIGHT) {
        if (!inDetailView) {
            // Primero intenta avanzar el stage; si ya es el último, avanza el día
            if (numStages > 1 && currentStageIndex < numStages - 1) {
                currentStageIndex++;
            } else {
                currentDayIndex  = (currentDayIndex + 1) % numDays;
                currentStageIndex = 0;
            }
            currentTalkIndex = 0;
        }
        screen->runNow();
        return 1;
    }

    // ── SELECT: entrar al detalle / ciclar interés ────────────────────────────
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

    // ── BACK / CANCEL: salir del detalle o del módulo ─────────────────────────
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

    // ── SELECT_LONG: salir al frame principal desde cualquier nivel ───────────
    if (event->inputEvent == INPUT_BROKER_SELECT_LONG) {
        inDetailView = false;
        screen->setFrames(graphics::Screen::FOCUS_DEFAULT);
        screen->runNow();
        return 1;
    }

    // Cualquier otro evento (USER_PRESS, etc.) se pasa al sistema estándar
    return 0;
}

#endif
