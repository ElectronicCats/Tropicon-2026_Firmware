#if defined(TROPICON2026)
#include "TalksModule.h"
#include "FSCommon.h"
#include "main.h"
#include <algorithm>
#include <set>
#include "graphics/Screen.h"
#include <iostream>

using namespace std;

extern graphics::Screen *screen;

TalksModule *talksModule = nullptr;

TalksModule::TalksModule() : MeshModule("Talks") {
    talksModule = this;
    loadSchedule();
    loadInterests();
    inputObserver.observe(inputBroker);
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
    
    // Simple persistence: rewrite the file
    File file = FSCom.open("/talks_int.dat", FILE_O_WRITE);
    if (!file) return;
    
    for (const auto& t : talks) {
        if (t.interest != 0) {
            file.print(t.id.c_str());
            file.print("$");
            file.println(t.interest);
        }
    }
    file.close();
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

int TalksModule::handleInputEvent(const InputEvent *event) {
    if (!screen->isScreenOn() || screen->isOverlayBannerShowing())
        return 0;

    // Only handle input if our frame is focused
    if (!isRequestingFocus())
        return 0;

    if (event->inputEvent == INPUT_BROKER_UP) {
        if (inDetailView) {
            // Scroll description maybe?
        } else {
            if (currentTalkIndex > 0) currentTalkIndex--;
        }
        screen->runNow();
        return 1;
    } else if (event->inputEvent == INPUT_BROKER_DOWN) {
        if (inDetailView) {
            // Scroll description
        } else {
            auto days = getDays();
            if (currentDayIndex < (int)days.size()) {
                auto stages = getStages(days[currentDayIndex]);
                if (currentStageIndex < (int)stages.size()) {
                    auto talkIndices = getFilteredTalkIndices(days[currentDayIndex], stages[currentStageIndex]);
                    if (currentTalkIndex < (int)talkIndices.size() - 1) currentTalkIndex++;
                }
            }
        }
        screen->runNow();
        return 1;
    } else if (event->inputEvent == INPUT_BROKER_SELECT) {
        if (inDetailView) {
            // Change interest
            auto days = getDays();
            auto stages = getStages(days[currentDayIndex]);
            auto talkIndices = getFilteredTalkIndices(days[currentDayIndex], stages[currentStageIndex]);
            if (currentTalkIndex < (int)talkIndices.size()) {
                int idx = talkIndices[currentTalkIndex];
                int nextInterest = (talks[idx].interest + 1) % 4;
                saveInterest(talks[idx].id, nextInterest);
            }
        } else {
            inDetailView = true;
        }
        screen->runNow();
        return 1;
    } else if (event->inputEvent == INPUT_BROKER_BACK) {
        if (inDetailView) {
            inDetailView = false;
        } else {
            // Exit talks module
            screen->setFrames(graphics::Screen::FOCUS_DEFAULT);
        }
        screen->runNow();
        return 1;
    }
    return 0;
}

#endif
