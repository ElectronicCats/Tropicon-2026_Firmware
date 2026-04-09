#pragma once
#if defined(TROPICON2026)
#include "MeshModule.h"
#include "FSCommon.h"
#include "input/InputBroker.h"
#include "concurrency/OSThread.h"
#include <vector>
#include <string>

using namespace std;

// "stage" values for schedule.json tracks
#define STAGE_TRACK1   "Track 1"
#define STAGE_VILLAS   "Track Villas"

// "stage" values for talleres.json workshops
#define STAGE_TALLER1  "Salón Maya"
#define STAGE_TALLER2  "Salón la Isla"
#define STAGE_TALLER3  "Suite 1"
#define STAGE_TALLER4  "Suite 2"

struct Talk {
    string id;
    string day;
    string time;
    string stage;
    string title;
    string speaker;
    string image;
    int interest; // 0: None, 1: Asistir, 2: Tal vez, 3: Saltar
};

class TalksModule : public MeshModule, private concurrency::OSThread {
public:
    TalksModule();
    virtual ~TalksModule();

    void loadSchedule();
    void loadTalleres();
    void loadInterests();
    void saveInterest(const string& talkId, int state);

    const vector<Talk>& getTalks() const { return talks; }

    // UI state
    int currentDayIndex   = 0;
    int currentStageIndex = 0;
    int currentTalkIndex  = 0;
    bool inDetailView     = false;

    vector<string> getDays();
    vector<string> getStages(const string& day);
    vector<int>    getFilteredTalkIndices(const string& day, const string& stage);

    // Input handling
    int handleInputEvent(const InputEvent *event);

    // Periodic tasks
    virtual int32_t runOnce() override;

    // MeshModule requirement
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }

private:
    vector<Talk> talks;
    bool     isDirty           = false;
    uint32_t lastInteractionMs = 0;

    // Observer for input
    CallbackObserver<TalksModule, const InputEvent *> inputObserver =
        CallbackObserver<TalksModule, const InputEvent *>(this, &TalksModule::handleInputEvent);
};

extern TalksModule *talksModule;

#endif
