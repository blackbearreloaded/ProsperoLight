# ps5-native-app-boilerplate - PS5 pacing regression.
# Copyright (C) 2026 BlackBearReloaded and contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-compiled smoke/regression test for PS5 source-clock VRR playout.

Run from the checkout root using python -m unittest discover -s tests.
"""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class Ps5VrrTimelineRegression(unittest.TestCase):
    def test_source_mapping_jitter_and_discontinuity(self):
        cc = shutil.which('clang++') or shutil.which('g++') or shutil.which('c++')
        if cc is None:
            self.skipTest('A C++17 compiler is unavailable')
        src = r'''
#include "ps5_vrr_timeline.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
int main() {
    moonlight::VrrSourceTimeline t;
    const uint64_t base = 10000000;
    for (unsigned i=1; i<=240; ++i) {
        uint64_t pts = 1000000 + uint64_t(i)*16667;
        uint64_t jitter = i%6==0 ? 2500 : 0;
        uint64_t ready = base + uint64_t(i)*16667 + jitter;
        t.observe(pts,ready);
        const uint64_t target = t.target(ready, 3000);
        assert(target >= ready);
        assert(target <= ready+5000);
        if (i>20 && jitter == 0) {
            // Early frames go to the source slot, not ready+reserve.
            assert(target >= ready+1000);
        }
    }
    t.observe(40000000,55000000); // discontinuity: re-anchor
    assert(t.target(55000000,3000)>=55000000);
    t.reset();
    assert(!t.anchored());
    assert(t.target(100000,2000)==100000);
    std::puts("PASS: source-referenced readiness timeline");
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder)
            (path/'case.cpp').write_text(src)
            cmd = [cc, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-I', str(ROOT/'include'), str(path/'case.cpp'), '-o', str(path/'case')]
            subprocess.run(cmd, check=True)
            subprocess.run([str(path/'case')], check=True)


if __name__ == '__main__':
    unittest.main()
