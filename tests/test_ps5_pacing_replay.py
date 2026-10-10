# ps5-native-app-boilerplate - PS5 pacing regression.
# Copyright (C) 2026 BlackBearReloaded and contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic host-only PS5 pacing event replay (no hardware timing claims).

Runs a shared source/decode/ready/request/VideoOut-observed timeline across
all 3 profile policies. Simulates jitter, dropped frames, stalls, repeated
scanouts, stale confirmations, and source timestamp discontinuities.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(os.environ.get('PS5_PACING_REPO', Path(__file__).resolve().parents[1]))

SOURCE = r'''
#include "frame_pacing.hpp"
#include "ps5_pacing_feedback.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
using namespace moonlight;

static void display_replay(unsigned source_fps, unsigned profile) {
    VrrRepeatPolicy v;
    v.reset(source_fps);
    v.configure(profile, 12000); // Backend rounded its actual 119.88 Hz mode.
    assert(v.display_floor_us() == 8342);
    assert(v.playout_reserve_us() == 0);
    uint64_t last_submit=0;
    uint64_t pts=100000;
    unsigned repeats=0;
    for (uint32_t frame=1; frame<=420; ++frame) {
        // Alternating encoder jitter, occasional client decode stall, one
        // missing frame and a later source clock break. Monotonic local clock.
        uint64_t period = 1000000u / source_fps;
        if (frame == 210) period += 125000; // Source pause: not network lateness.
        pts += period;
        uint64_t ready = 2000000 + pts + (frame%9 == 0 ? 1700 : 0);
        if (frame%77 == 0) ready += 4800;
        if (last_submit && ready < last_submit) ready = last_submit;
        v.observe_picture(pts, frame);
        v.observe_readiness(pts, ready, frame);
        uint64_t age = (frame%23 == 0) ? 2*period : 0;
        bool successor = age != 0;
        uint64_t due = v.picture_target(ready, successor, age);
        assert(due >= ready);
        if(last_submit) assert(due >= last_submit+v.display_floor_us());
        // Host test assumes requested flip is admitted on time.
        uint64_t observed = due + 2100;
        v.presented(due);
        v.observe_output_feedback(frame,pts,ready,observed,true,due);
        assert(v.playout_reserve_us() <= (profile == 0 ? 0u : profile == 1 ? 8000u : 16000u));
        last_submit=due;
        // When host stops drawing, optional repeat never advances a real-frame
        // timestamp; it still obeys VideoOut ceiling.
        if(frame%95==0) {
            uint64_t repeat_due=v.idle_deadline();
            if(repeat_due < last_submit+v.display_floor_us())
                repeat_due=last_submit+v.display_floor_us();
            v.repeated(repeat_due);
            ++repeats;
            last_submit=repeat_due;
        }
    }
    assert(repeats >= 4);
}

static void feedback_replay() {
    Ps5SpacingFeedback f;
    f.configure(1);
    uint64_t previous=0;
    // Real GPU lateness: source cadence 60, but prepared/requested/observed
    // intervals all slip +1.5 ms per frame. Attack may never exceed 250 us.
    for(uint32_t i=1;i<=35;++i) {
        uint64_t pts=uint64_t(i)*16667;
        uint64_t ready=1000000+pts+uint64_t(i)*1500;
        uint64_t req=ready+500;
        f.observe(i,pts,ready,req+2000,true,req);
        assert(f.reserve_us() <= previous+250);
        previous=f.reserve_us();
    }
    assert(f.reserve_us()>0 && f.reserve_us()<=8000);
    const auto miss=f.misses();
    // Lost flip evidence: old samples must NOT carry into a new epoch.
    f.observe(39,39*16667,1800000,1801000,true,1800500);
    assert(f.reserve_us()==0 && f.epoch_resets()>0);
    assert(f.misses()==miss); // Lifetime metrics survive the reset.

    // Polling jitter without a shift of the actual requested flip must never
    // teach the controller a larger playout reserve. Request time is known.
    f.configure(1);
    for(uint32_t i=1;i<=20;++i) {
        uint64_t pts=uint64_t(i)*16667;
        uint64_t ready=1000000+pts+uint64_t(i)*1500;
        uint64_t req=1500000+pts;
        uint64_t observed=req+uint64_t(i)*2000+5000;
        f.observe(i,pts,ready,observed,true,req);
    }
    assert(f.reserve_us()==0);
    assert(f.ambiguous()>0);

    // Long stall in the local presentation path: invalidate learned state.
    f.configure(2);
    for(uint32_t i=1;i<=20;++i){
        uint64_t pts=uint64_t(i)*16667;
        uint64_t ready=1000000+pts+uint64_t(i)*1500;
        uint64_t req=ready+500;
        f.observe(i,pts,ready,req+2100,true,req);
    }
    assert(f.reserve_us()>0);
    f.observe(21,21*16667,3000000,3010000,true,3000500);
    assert(f.reserve_us()==0);
}

int main() {
    for(unsigned fps: {16u,30u,45u,50u,51u,59u,60u,61u,75u,90u,120u})
        for(unsigned profile=0;profile<3;++profile)display_replay(fps,profile);
    feedback_replay();
    std::puts("PASS: source/decode/ready/request/VideoOut-observed replay; 11 FPS x 3 profiles");
}
'''

class Ps5PacingReplay(unittest.TestCase):
    def test_host_event_replay(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            self.skipTest('No C++17 compiler')
        with tempfile.TemporaryDirectory() as td:
            src=Path(td)/'pacing_replay.cpp'
            binary=Path(td)/'pacing_replay'
            src.write_text(SOURCE)
            subprocess.run([cc,'-std=c++17','-O2','-Wall','-Wextra','-Werror',
                            '-I',str(ROOT/'include'),str(src),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__=='__main__':
    unittest.main()
