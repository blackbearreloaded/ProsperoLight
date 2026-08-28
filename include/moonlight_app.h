/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "moonlight_backend.h"
#include "moonlight_config.h"
#include "radio_input.h"

#include <pthread.h>

namespace Rml {
class ElementDocument;
}

class MoonlightApp {
public:
    enum class Command {
        None,
        StartStream
    };

    bool Initialize(Rml::ElementDocument* document);
    void Poll();
    void HandleInput(const radio_input_event_t& event);
    Command TakeCommand();
    void Shutdown();
    const char* SelectedAppName() const;
    int SelectedAppId() const;
    const char* SelectedHostAddress() const;
    unsigned BitrateKbps() const;
    unsigned DisplayArea() const;
    unsigned VideoCodec() const;
    unsigned StreamResolution() const;
    unsigned HdrEnabled() const;
    void ShowStreamError(const char* message);

private:
    enum class Screen {
        Hosts,
        Games,
        Settings,
        Diagnostics,
        Count
    };

    Rml::ElementDocument* document_ = nullptr;
    Screen screen_ = Screen::Hosts;
    unsigned focus_ = 4;
    unsigned bitrate_mbps_ = 20;
    bool confirm_unpair_ = false;
    bool manual_entry_active_ = false;
    bool pairing_active_ = false;
    moonlight_backend_pair_state_t pairing_state_ = MOONLIGHT_BACKEND_PAIR_IDLE;
    uint64_t pairing_started_ms_ = 0;
    unsigned pairing_remaining_ = MOONLIGHT_BACKEND_PAIR_TIMEOUT_SECONDS + 1;
    Command command_ = Command::None;
    moonlight_backend_snapshot_t backend_{};
    moonlight_config_t config_{};
    unsigned selected_app_ = 0;
    unsigned artwork_page_start_ = MOONLIGHT_BACKEND_MAX_APPS;
    unsigned artwork_next_slot_ = 0;
    bool artwork_attempted_[6]{};
    bool artwork_worker_active_ = false;
    volatile int artwork_worker_state_ = 0;
    pthread_t artwork_thread_{};
    char artwork_host_[MOONLIGHT_CONFIG_ADDRESS_SIZE]{};
    uint16_t artwork_https_port_ = 0;
    int artwork_app_id_ = 0;
    uint64_t artwork_due_ms_ = 0;
    char stream_error_[192]{};

    void SetScreen(Screen screen);
    void MoveFocus(int direction);
    void CycleApp(int direction);
    void MoveGameVertical(int direction);
    void CycleHost(int direction);
    void Activate();
    void StartManualHostEntry();
    static void ManualHostResult(const char* text, void* user_data);
    void AddManualHost(const char* text);
    void DiscoverHosts();
    const moonlight_config_host_t* SelectedHost() const;
    void TogglePairing();
    void PollPairing();
    void PollArtwork();
    void FinishArtworkWorker(bool clear_cache);
    static void* ArtworkWorker(void* argument);
    void StopActiveApp();
    void RefreshBackend();
    void UpdateScreen();
    void UpdateFocus();
    void UpdateHost();
    void UpdateGames();
    void UpdateSettings();
};
