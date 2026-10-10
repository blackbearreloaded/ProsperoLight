# ps5-native-app-boilerplate - PS5 pacing regression.
# Copyright (C) 2026 BlackBearReloaded and contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile PS5 shared pacing headers across Unpaced, Paced and Paced+VRR."""
from pathlib import Path
import shutil
import os
import subprocess
import tempfile
import unittest

PACKAGE=Path(__file__).resolve().parents[1]

class PS5PolicyModes(unittest.TestCase):
    def test_shared_policy_modes(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc: self.skipTest('no C++17 compiler')
        source=r'''
#include "frame_pacing.hpp"
#include "ps5_pacing_feedback.hpp"
#include <cassert>
#include <cstdio>
using namespace moonlight;
int main() {
    // Unpaced: reassembly deadline disabled, no intentional source buffering.
    Ps5ReceiveDeadline rx;
    rx.publish(0,1001000,1000000,2000000,1000,false);
    assert(rx.lookup(0,2000000)==0);
    VrrRepeatPolicy latency;
    latency.reset(60); latency.configure(0,11988);
    assert(latency.playout_reserve_us()==0);
    for(uint32_t frame=1;frame<45;frame++) {
        uint64_t pts=uint64_t(frame)*16667;
        uint64_t ready=1000000+pts+uint64_t(frame)*2000;
        latency.observe_readiness(pts,ready,frame);
        latency.observe_output_feedback(frame,pts,ready,ready,true);
    }
    assert(latency.playout_reserve_us()==0);

    /* FIXED_MODE_PACING_TEST */

    // Paced+VRR: lateness-driven reserve is capped and only client-caused.
    VrrRepeatPolicy adaptive;
    adaptive.reset(60);adaptive.configure(1,11988);
    for(uint32_t frame=1;frame<=45;frame++) {
        uint64_t pts=uint64_t(frame)*16667;
        uint64_t late=uint64_t(frame)*2000;
        uint64_t ready=3000000+pts+late;
        adaptive.observe_readiness(pts,ready,frame);
        adaptive.observe_output_feedback(frame,pts,ready,ready,true);
    }
    assert(adaptive.playout_reserve_us()>0);
    assert(adaptive.playout_reserve_us()<=8000);
    assert(adaptive.feedback_misses()>0);

    // Source-side slowdown advances PTS itself. No invented client penalty.
    Ps5SpacingFeedback game_jitter;
    game_jitter.configure(2);
    uint64_t pts=1,local=1000000;
    for(uint32_t frame=1;frame<200;frame++) {
        const uint64_t period=frame%3==0?30000:16667;
        pts+=period;local+=period;
        game_jitter.observe(frame,pts,local,local,true);
    }
    assert(game_jitter.misses()==0);
    assert(game_jitter.reserve_us()==0);
    std::puts("PASS: Unpaced/Fixed Paced/Paced+VRR shared policies");
}
'''
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'modes.cpp';dst=Path(d)/'modes';src.write_text(source)
            # Prefer a full generated fixture for local package testing; after
            # application inside ProsperoLight the installed header is used.
            root=Path(os.environ['PS5_PACING_REPO']) if 'PS5_PACING_REPO' in os.environ else ((PACKAGE/'_policy_fixture') if (PACKAGE/'_policy_fixture/include/frame_pacing.hpp').is_file() else PACKAGE)
            # The source-only local fixture contains only changed anchors,
            # whereas the real checkout includes FramePacing. Test fixed
            # scheduling when its complete class is available.
            if 'class FramePacing' in (root/'include/frame_pacing.hpp').read_text():
                source = source.replace('    /* FIXED_MODE_PACING_TEST */', '''    // Paced with fixed 90 FPS on a 119.88 Hz link: timestamps progress, fixed
    // refresh is the clock, renderer lead cannot create an early submission.
    FramePacing fixed;
    fixed.reset(90);
    uint64_t prev=0;
    for(int frame=1;frame<=200;frame++) {
        uint64_t pts=uint64_t(frame)*11111;
        uint64_t ready=2000000+pts;
        uint64_t target=fixed.target(frame,pts,ready,11988,prev?prev+250:0,0,2000);
        assert(target>=ready);
        fixed.submitted(target,target,target-ready);
        prev=target;
    }
    assert(fixed.stats.submissions==200);

''')
            else:
                source = source.replace('    /* FIXED_MODE_PACING_TEST */',
                                        '// fixed scheduling tested on complete checkout')
            src.write_text(source)
            subprocess.run([cc,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(root/'include'),str(src),'-o',str(dst)],check=True)
            subprocess.run([str(dst)],check=True)
