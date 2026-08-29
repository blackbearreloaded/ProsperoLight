#!/usr/bin/env python3
# ps5-native-app-boilerplate - Host tooling regression tests.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Exercises identity initialization and deployment resolution without a console.

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ToolTests(unittest.TestCase):
    def test_stream_dependencies_use_ps5_nonblocking_socket_adapter(self):
        compatibility = (ROOT / "platform/ps5/ps5_compat.h").read_text(
            encoding="utf-8"
        )
        implementation = (ROOT / "platform/ps5/ps5_sockets.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("#define fcntl ps5_socket_fcntl", compatibility)
        self.assertIn("command == F_GETFL", implementation)
        self.assertIn("command == F_SETFL", implementation)
        self.assertIn("SCE_NET_SO_NBIO", implementation)

    def test_stream_threads_skip_unsafe_ps5_thread_rename(self):
        compatibility = (ROOT / "platform/ps5/ps5_compat.h").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "#define pthread_setname_np ps5_pthread_setname_noop", compatibility
        )

    def test_release_build_disables_blocking_lan_telemetry(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("LAN_TELEMETRY ?= 0", makefile)
        self.assertIn(
            "APP_DEFINITIONS += PROSPEROLIGHT_LAN_TELEMETRY=$(LAN_TELEMETRY)",
            makefile,
        )

    def test_stop_refresh_preserves_the_last_application_catalog(self):
        source = (ROOT / "src/moonlight_app.cpp").read_text(encoding="utf-8")
        start = source.index("void MoonlightApp::StopActiveApp()")
        end = source.index("void MoonlightApp::RefreshBackend()", start)
        stop = source[start:end]

        self.assertIn("moonlight_backend_snapshot_t refreshed{};", stop)
        self.assertIn(
            "moonlight_backend_stop_app(SelectedHostAddress(), &refreshed)", stop
        )
        self.assertIn("if (result == 0 || refreshed.app_count)", stop)
        self.assertNotIn(
            "moonlight_backend_stop_app(SelectedHostAddress(), &backend_)", stop
        )

    def test_stream_opens_pad_after_decoder_loading_worker_stops(self):
        source = (ROOT / "src/moonlight_stream.cpp").read_text(encoding="utf-8")
        early_loading = source.index(
            "start_connection_loading(&loading, NULL, 0, mode->hdr, mode->visible_width,"
        )
        early_loading_stop = source.index("stop_connection_loading();", early_loading)
        pad_init = source.index("controller_ready = ps5_controller_init(&controller)")
        self.assertLess(early_loading, early_loading_stop)
        self.assertLess(early_loading_stop, pad_init)

    def test_stream_forwards_only_the_newest_pad_sample(self):
        source = (ROOT / "src/moonlight_stream.cpp").read_text(encoding="utf-8")
        start = source.index("static void ps5_controller_poll(")
        end = source.index("static void ps5_controller_stop(", start)
        poll = source[start:end]

        self.assertIn("ps5_controller_newest_sample(state, count)", poll)
        self.assertNotIn("&state->sample_batch[index]", poll)

    def test_launch_advertises_moonlight_hdr_capabilities_for_main10(self):
        source = (ROOT / "src/gamestream/client.c").read_text(encoding="utf-8")
        self.assertIn("configuration->supportedVideoFormats & VIDEO_FORMAT_MASK_10BIT", source)
        self.assertIn("&hdrMode=1&clientHdrCapVersion=0", source)
        self.assertIn("&clientHdrCapSupportedFlagsInUint32=0", source)
        self.assertIn("&clientHdrCapMetaDataId=NV_STATIC_METADATA_TYPE_1", source)
        self.assertIn("&clientHdrCapDisplayData=0x0x0x0x0x0x0x0x0x0x0", source)

    def test_hdr_frames_wait_for_control_stream_confirmation(self):
        source = (ROOT / "src/moonlight_stream.cpp").read_text(encoding="utf-8")
        start = source.index("static int moonlight_renderer_submit(")
        end = source.index("static void audio_ring_drop(", start)
        submit = source[start:end]

        self.assertIn("(hdr_transitions && !hdr_active)", submit)
        self.assertNotIn("!decode_unit->hdrActive", submit)

    def test_hdr_overlay_uses_a_distinct_relocatable_shader_header(self):
        source = (ROOT / "src/native_agc_present.cpp").read_text(encoding="utf-8")

        self.assertIn("#define HDR_HUD_SHADER_HEADER_OFFSET 0xe000u", source)
        self.assertIn(
            "presenter.shader_memory + HDR_HUD_SHADER_HEADER_OFFSET, 0x1000,",
            source,
        )
        self.assertIn(
            "presenter.shader_memory + HDR_HUD_SHADER_HEADER_OFFSET,\n"
            "                                    presenter.shader_memory + HDR_HUD_SHADER_CODE_OFFSET",
            source,
        )
        self.assertNotIn(
            "&presenter.hud_pixel_shader, presenter.shader_memory + 0x1000,", source
        )

    def test_hdr_overlay_uses_distinct_sdr_conversion_resources(self):
        source = (ROOT / "src/native_agc_present.cpp").read_text(encoding="utf-8")

        self.assertIn("#define HDR_HUD_RESOURCES_OFFSET 0xf000u", source)
        self.assertIn(
            "prepare_resources(presenter.shader_memory + HDR_HUD_RESOURCES_OFFSET, 0)",
            source,
        )
        self.assertIn(
            "uint8_t *hud_resources = hdr ? memory + HDR_HUD_RESOURCES_OFFSET : resources;",
            source,
        )
        self.assertIn("bind_pixel_source(&command, hud_resources,", source)

    def test_4k_videoout_follows_the_hardware_accepted_registration_path(self):
        source = (ROOT / "src/native_agc_present.cpp").read_text(encoding="utf-8")
        initialize = source[
            source.index("static int initialize_presenter(") : source.index(
                "static int present_frame(", source.index("static int initialize_presenter(")
            )
        ]

        self.assertNotIn("sceVideoOutAddBuffer4k2kPrivilege", source)
        self.assertLess(
            initialize.index("sceVideoOutOpen"),
            initialize.index("sceVideoOutSetFlipRate"),
        )
        self.assertLess(
            initialize.index("sceVideoOutSetFlipRate"),
            initialize.index("sceKernelAllocateDirectMemory", initialize.index("sceVideoOutOpen")),
        )
        self.assertLess(
            initialize.index("sceVideoOutSetBufferAttribute2"),
            initialize.index("sceVideoOutRegisterBuffers2"),
        )

    def test_stream_sampler_uses_the_filtered_probe_variant(self):
        header = (ROOT / "include/native_agc_output.hpp").read_text(encoding="utf-8")
        source = (ROOT / "src/native_agc_present.cpp").read_text(encoding="utf-8")

        self.assertIn("kNativeAgcBilinearSamplerWord = 0x09500000u", header)
        self.assertEqual(source.count("kNativeAgcBilinearSamplerWord"), 2)
        self.assertNotIn("descriptor[18] = descriptor[22] = 0x08000000", source)

    def test_loading_labels_match_the_embedded_dimensions(self):
        expected = {
            "loading-prosperolight-alpha.bin": (232, 35),
            "loading-connecting-alpha.bin": (287, 52),
        }
        for name, (width, height) in expected.items():
            self.assertEqual((ROOT / "assets/private" / name).stat().st_size, width * height)

    def test_release_metadata_preserves_hdr_capability(self):
        configured = json.loads(
            (ROOT / "sce_sys/param.json").read_text(encoding="utf-8")
        )
        self.assertEqual(configured["applicationCategoryType"], 0)
        self.assertEqual(configured["attribute"], 0x62000000)
        self.assertEqual(configured["attribute2"], 0)
        self.assertEqual(configured["attribute3"], 0)

    def test_release_workflow_publishes_only_ffpfsc_and_checksum(self):
        workflow = (ROOT / ".github/workflows/tooling.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn('"Jinja2==3.1.6" "jsonschema==4.25.1"', workflow)
        self.assertIn('sha256sum "$TITLE_ID.ffpfsc" > SHA256SUMS', workflow)
        self.assertIn("dist/SHA256SUMS", workflow)
        self.assertIn("Expected exactly one FFPFSC image and SHA256SUMS.", workflow)
        self.assertIn("sha256sum -c SHA256SUMS", workflow)
        self.assertIn('assets=("release/$IMAGE" "release/$CHECKSUM")', workflow)
        self.assertIn("gh release delete-asset", workflow)
        self.assertNotIn(".ffpkg", workflow)

    def test_readme_tracks_identity_and_stream_shortcuts(self):
        configured = json.loads(
            (ROOT / "sce_sys/param.json").read_text(encoding="utf-8")
        )
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn(configured["titleId"], readme)
        self.assertIn(configured["contentVersion"], readme)
        for shortcut in (
            "Select + L1",
            "Select + R1",
            "Select + Square",
            "Select + Triangle",
        ):
            self.assertIn(shortcut, readme)

    def run_init(self, param, **values):
        environment = os.environ.copy()
        environment.update(values)
        return subprocess.run(
            ["bash", str(ROOT / "tools/init-project.sh"), str(param)],
            cwd=ROOT,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )

    def test_init_coordinates_media_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            param.write_text(
                json.dumps(
                    {
                        "contentId": "UP9000-PPSA99999_00-HELLOWORLD000001",
                        "localizedParameters": {
                            "defaultLanguage": "en-US",
                            "en-US": {"titleName": "Old"},
                        },
                        "gameIntent": {"permittedIntents": [{"intentType": "launchActivity"}]},
                    }
                ),
                encoding="utf-8",
            )
            result = self.run_init(
                param,
                TITLE_ID="PPSA12345",
                APP_NAME="Moon Client",
                APP_CATEGORY="media",
                CONTENT_SUFFIX="MOONCLIENT000001",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            configured = json.loads(param.read_text(encoding="utf-8"))
            self.assertEqual(configured["titleId"], "PPSA12345")
            self.assertEqual(configured["conceptId"], "12345")
            self.assertEqual(configured["contentId"], "UP9000-PPSA12345_00-MOONCLIENT000001")
            self.assertEqual(configured["localizedParameters"]["en-US"]["titleName"], "Moon Client")
            self.assertEqual(configured["applicationCategoryType"], 65536)
            self.assertEqual(configured["contentBadgeType"], 2)
            self.assertNotIn("gameIntent", configured)

    def test_init_rejects_invalid_title_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            original = '{"contentId":"UP9000-PPSA99999_00-HELLOWORLD000001"}\n'
            param.write_text(original, encoding="utf-8")
            result = self.run_init(param, TITLE_ID="PPSA12", APP_NAME="Broken")
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(param.read_text(encoding="utf-8"), original)

    def test_init_derives_game_suffix_and_preserves_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            param.write_text(
                json.dumps(
                    {
                        "contentId": "UP9000-PPSA99999_00-HELLOWORLD000001",
                        "localizedParameters": {
                            "defaultLanguage": "en-US",
                            "en-US": {"titleName": "Old"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            param.chmod(0o640)
            result = self.run_init(
                param, TITLE_ID="PPSA54321", APP_NAME="Native Sample", CONTENT_SUFFIX=""
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            configured = json.loads(param.read_text(encoding="utf-8"))
            self.assertEqual(configured["contentId"], "UP9000-PPSA54321_00-NATIVESAMPLE0000")
            self.assertEqual(configured["applicationCategoryType"], 0)
            self.assertEqual(configured["contentBadgeType"], 1)
            self.assertEqual(
                configured["gameIntent"]["permittedIntents"],
                [{"intentType": "launchActivity"}],
            )
            self.assertEqual(param.stat().st_mode & 0o777, 0o640)

    def test_undeploy_dry_run_resolves_only_current_title(self):
        environment = os.environ.copy()
        environment.update(PS5_HOST="192.0.2.1", DEPLOY_DRY_RUN="1")
        result = subprocess.run(
            ["bash", str(ROOT / "tools/deploy.sh"), "undeploy"],
            cwd=ROOT,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        title_id = json.loads(
            (ROOT / "sce_sys/param.json").read_text(encoding="utf-8")
        )["titleId"]
        self.assertIn(f"/data/homebrew/{title_id}/", result.stdout)
        self.assertIn(f"{title_id}.{{ffpkg,ffpfsc}}", result.stdout)
        self.assertIn("no network request was sent", result.stdout)

    def test_deploy_dry_run_uses_mocked_build_and_no_network(self):
        with tempfile.TemporaryDirectory() as directory:
            sandbox = Path(directory)
            (sandbox / "tools").mkdir()
            (sandbox / "sce_sys").mkdir()
            shutil.copy2(ROOT / "tools/deploy.sh", sandbox / "tools/deploy.sh")
            (sandbox / "sce_sys/param.json").write_text(
                '{"titleId":"PPSA12345"}\n', encoding="utf-8"
            )

            mock_bin = sandbox / "mock-bin"
            mock_bin.mkdir()
            mock_make = mock_bin / "make"
            mock_make.write_text(
                "#!/usr/bin/env bash\n"
                "mkdir -p \"$MOCK_ROOT/dist\"\n"
                "printf package > \"$MOCK_ROOT/dist/PPSA12345.ffpkg\"\n",
                encoding="utf-8",
            )
            mock_make.chmod(0o755)

            environment = os.environ.copy()
            environment.update(
                PS5_HOST="192.0.2.1",
                DEPLOY_DRY_RUN="1",
                DEPLOY_FORMAT="ffpkg",
                MOCK_ROOT=str(sandbox),
                PATH=f"{mock_bin}{os.pathsep}{environment['PATH']}",
            )
            result = subprocess.run(
                ["bash", str(sandbox / "tools/deploy.sh")],
                cwd=sandbox,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("/data/homebrew/PPSA12345.ffpkg", result.stdout)
            self.assertIn("no network request was sent", result.stdout)


if __name__ == "__main__":
    unittest.main()
