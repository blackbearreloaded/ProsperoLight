# ps5-native-app-boilerplate - PS5 pacing regression.
# Copyright (C) 2026 BlackBearReloaded and contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-only C++ tests: received RTP deadlines, decode readiness and feedback."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Ps5PacingFeedbackTests(unittest.TestCase):
    def test_feedback(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler missing')
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory)/'test.cpp'
            exe = pathlib.Path(directory)/'test'
            source.write_text(r'''
#include "ps5_pacing_feedback.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace moonlight;
int main() {
    Ps5ReceiveDeadline dl;
    assert(dl.lookup(90000,2000000)==0);
    dl.publish(90000,5000000,4990000,9000000,2500,true);
    // 10ms ahead, less 3ms readiness lead, in the common-c clock domain.
    assert(dl.lookup(90000,9000000)==9007000);
    assert(dl.lookup(91500,9000000)==9023666);
    assert(dl.lookup(90000-90000,9000000)==0);
    assert(dl.lookup(91500,9100000)==0);  // stale epoch: not immortal
    dl.publish(0,5000000,4990000,9000000,2500,true);
    assert(dl.lookup(0,9000000)==9007000); // RTP zero is valid.
    dl.publish(90000,5000000,4990000,9000000,2500,false);
    assert(dl.lookup(90000,9000000)==0); // Unpaced may never force expiry.
    dl.publish(0xfffffff0u,5000000,4990000,9000000,2500,true);
    assert(dl.lookup(0x00000010u,9000000)>9007000); // RTP rollover.
    Ps5ReadinessEstimator ready;
    assert(ready.percentile_us()==3500);
    for (int i=0;i<80;i++) ready.observe(1000000,1000000+((i%20==0)?6000:3000));
    assert(ready.percentile_us()>=3000 && ready.percentile_us()<=6000);
    assert(ready.samples()==64);
    Ps5SpacingFeedback feedback;
    feedback.configure(1);
    for (uint32_t i=1;i<=30;i++) {
        const uint64_t pts=uint64_t(i)*16667;
        // Client CPU/GPU preparation is systematically late compared to PTS.
        const uint64_t lateness=uint64_t(i)*1500;
        feedback.observe(i,pts,pts+1000000+lateness,pts+1000000+lateness,true);
    }
    assert(feedback.reserve_us()>0 && feedback.reserve_us()<=8000);
    assert(feedback.misses()>=3);
    for (uint32_t i=31;i<301;i++) {
        const uint64_t pts=uint64_t(i)*16667;
        feedback.observe(i,pts,pts+1000000+45000,pts+1000000+45000,true);
    }
    assert(feedback.reserve_us()==0);
    feedback.configure(1);
    for (uint32_t i=1;i<=5;i++) {
        const uint64_t pts=uint64_t(i)*16667;
        feedback.observe(i,pts,pts+1000000+uint64_t(i)*5000,
                         pts+1000000+uint64_t(i)*5000,true);
        assert(feedback.reserve_us()<=uint64_t(i)*250);
    }
    feedback.configure(0);
    for (uint32_t i=1;i<=30;i++) {
        const uint64_t pts=uint64_t(i)*16667;
        feedback.observe(i,pts,pts+1000000+uint64_t(i)*1500,pts+1000000+uint64_t(i)*1500,true);
    }
    assert(feedback.reserve_us()==0);
    assert(feedback.epoch_resets()==0); // Disabled Low Latency is not a discontinuity.
    feedback.configure(1);
    feedback.observe(1,16667,1016667,1017167,true,1017000);
    feedback.observe(2,33334,1033334,1033834,false,1033500);
    assert(feedback.epoch_resets()==1);
    feedback.observe(3,50001,1050001,1050501,false,1050300);
    assert(feedback.epoch_resets()==1); // Count only the actual eligibility transition.
    std::cout << "PASS: receive deadlines, readiness p95, attributed spacing reserve\n";
}
''')
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),str(source),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
