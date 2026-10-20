import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('bootstrap', ROOT / 'scripts/bootstrap.py')
bootstrap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bootstrap)


class OverlayTests(unittest.TestCase):
    def test_patch_exactly_once(self):
        self.assertEqual(bootstrap.patch_text('one-two', 'one', 'three', 'test'), 'three-two')
        for source in ('no match', 'repeated repeated'):
            with self.assertRaises(RuntimeError):
                bootstrap.patch_text(source, 'repeated', 'patched', 'test')

    def test_pins_protocol_to_release_not_master(self):
        self.assertEqual(bootstrap.COMMON_SHA, 'd6a11bc685b41037b352a96f29d08276fe5359ba')
        self.assertEqual(bootstrap.BASE_SHA, 'cf96ecc72cdab3aae7ba0698f463b89065ae7e3b')
        self.assertNotEqual(bootstrap.APP_ID, 'com.limelight.webos')

    def test_overlay_patches_expected_upstream_layout(self):
        fixture = {
            'core/moonlight-common-c/CMakeLists.txt': '    CHECK_CXX_SOURCE_COMPILES(\n',
            'CMakeLists.txt': (
                'execute_process(COMMAND git describe --tags --abbrev=0 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}\n'
                '        OUTPUT_VARIABLE MOONLIGHT_VERSION ERROR_QUIET)\n'
                'set(WEBOS_APPINFO_ID "com.limelight.webos")\n'
            ),
            'src/app/CMakeLists.txt': 'add_subdirectory(input)\nadd_subdirectory(stream)\n',
            'src/app/input/input_gamepad.c': (
                '#include "app_input.h"\n'
                '    SDL_GameControllerClose(state->controller);\n'
                '    app_gamepad_state_t *state = &input->gamepads[controller_id];\n'
            ),
            'src/app/input/app_input.c': (
                '#include "app_input.h"\n'
                '    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);\n'
                '    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);\n'
            ),
            'src/app/stream/input/session_gamepad.c': (
                '#include "input/input_gamepad.h"\n'
                '    LiSendControllerArrivalEvent(gamepad->gs_id, input->input->activeGamepadMask, type,\n'
            ),
            'src/app/stream/connection/session_connnection.c': (
                '#include "input/input_gamepad.h"\n'
                '        .setHdrMode = connection_set_hdr\n'
                'void session_connection_callbacks_reset(session_t *session) {\n'
            ),
            'deploy/webos/appinfo.json': (
                '"title": "Moonlight"\n'
                '"vendor": "Mariotaku Lee"\n'
                '"appDescription": "Open Source NVIDIA GameStream Client"\n'
            ),
        }
        with tempfile.TemporaryDirectory() as temp:
            base = pathlib.Path(temp)
            for name, content in fixture.items():
                dest = base / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_text(content)
            bootstrap.apply(base)
            cmake = (base/'src/app/CMakeLists.txt').read_text()
            self.assertIn('webos_dualsense/webos_haptics.c', cmake)
            self.assertIn('LI_CCAP_HAPTICS_PCM', (base/'src/app/stream/input/session_gamepad.c').read_text())
            self.assertIn('controllerHaptics = webos_haptics_receive',
                          (base/'src/app/stream/connection/session_connnection.c').read_text())
            self.assertEqual((base/'src/app/webos_dualsense/saxense/packet.h').read_bytes(),
                             (ROOT/'haptics/saxense/packet.h').read_bytes())
            self.assertIn('com.ray.moonlight.dualsense', (base/'CMakeLists.txt').read_text())
            self.assertIn('v1.6.36', (base/'CMakeLists.txt').read_text())
            with self.assertRaises(RuntimeError):
                bootstrap.apply(base)


if __name__ == '__main__':
    unittest.main()
