# ps5-native-app-boilerplate - A pull request's build names itself.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BuildLabelTests(unittest.TestCase):
    def test_pull_request_artifact_and_label(self):
        workflow = (ROOT / ".github/workflows/tooling.yml").read_text(encoding="utf-8")
        self.assertIn('echo "artifact=ProsperoLight-PR$PR_NUMBER-$short" >> "$GITHUB_OUTPUT"', workflow)
        self.assertIn('echo "BUILD_LABEL=PR $PR_NUMBER, $short" >> "$GITHUB_ENV"', workflow)
        self.assertIn("name: ${{ steps.label.outputs.artifact }}", workflow)
        # The release job still finds a tag's build under its commit.
        self.assertIn('echo "artifact=prosperolight-release-$GITHUB_SHA" >> "$GITHUB_OUTPUT"', workflow)
        self.assertIn('--name "prosperolight-release-$GITHUB_SHA"', workflow)
        # Event data reaches the shell through the environment, never by substitution.
        self.assertNotIn("${{ github.event.pull_request.number }}\" ", workflow)

    def test_build_writes_and_clears_the_label(self):
        build = (ROOT / "tools/build.sh").read_text(encoding="utf-8")
        self.assertIn('rm -f -- "$app/build-label.txt"', build)
        self.assertIn("printf '%s\\n' \"$BUILD_LABEL\" > \"$app/build-label.txt\"", build)
        self.assertIn("{1,40}$", build)
        launcher = (ROOT / "src/launcher/launcher_ps5.cpp").read_text(encoding="utf-8")
        self.assertIn('"/build-label.txt"', launcher)
        # The version itself comes from param.json alone.
        self.assertNotIn("build_label", (ROOT / "third_party/update-check/update_check.c").read_text())


if __name__ == "__main__":
    unittest.main()
