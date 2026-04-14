#pragma once
#if defined(TROPICON2026)
#include "MeshModule.h"
#include "FSCommon.h"
#include "input/InputBroker.h"
#include "concurrency/OSThread.h"
#include <vector>
#include <string>
#include <set>

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
    string date;  // "DiaMes" field: "DD/Mes/YYYY" (e.g. "17/Abril/2026")
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

    void ensureLoaded();
    const vector<Talk>& getTalks() { ensureLoaded(); return talks; }

    void loadSchedule();
    void loadTalleres();
    void loadInterests();
    void saveInterest(const string& talkId, int state);

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
    bool     isLoaded          = false;
    bool     isDirty           = false;
    uint32_t lastInteractionMs = 0;

    // ── BLE notification state ────────────────────────────────────────────────
    // Talk IDs for which a 10-minute alert has already been sent this day.
    // Cleared automatically when the calendar day changes.
    set<string>  notifiedTalkIds;
    uint32_t     lastNotifCheckMs = 0;
    int          lastNotifDay     = -1; // tm_yday of the last check, used to reset the set daily

    // Sends BLE push alerts for talks marked "Asistir" that start in ~10 minutes.
    void checkAndSendNotifications();

    // Observer for input
    CallbackObserver<TalksModule, const InputEvent *> inputObserver =
        CallbackObserver<TalksModule, const InputEvent *>(this, &TalksModule::handleInputEvent);
};

extern TalksModule *talksModule;

#endif
