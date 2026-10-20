#!/usr/bin/env python3
"""Build a reproducible Moonlight TV worktree with Nonary's waveform protocol.

No private SDK or binary dependency is vendored. Fail closed on upstream drift.
"""
from __future__ import annotations
import argparse
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_REPO = 'https://github.com/mariotaku/moonlight-tv.git'
BASE_SHA = 'cf96ecc72cdab3aae7ba0698f463b89065ae7e3b'
COMMON_REPO = 'https://github.com/Nonary/moonlight-common-c.git'
COMMON_SHA = 'd6a11bc685b41037b352a96f29d08276fe5359ba'
APP_ID = 'com.ray.moonlight.dualsense'


def run(*args: str, cwd: pathlib.Path | None = None) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def patch_text(text: str, old: str, new: str, description: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{description}: expected one anchor, found {count} — upstream changed; refusing to patch')
    return text.replace(old, new, 1)


def patch_file(dest: pathlib.Path, old: str, new: str, description: str) -> None:
    text = dest.read_text()
    dest.write_text(patch_text(text, old, new, description))


def clone_and_pin(dest: pathlib.Path) -> None:
    if not (dest / '.git').exists():
        if dest.exists() and any(dest.iterdir()):
            raise RuntimeError(f'{dest} exists and is not a git checkout')
        dest.mkdir(parents=True, exist_ok=True)
        run('git', 'init', str(dest))
        run('git', 'remote', 'add', 'origin', BASE_REPO, cwd=dest)
        run('git', 'fetch', '--depth', '1', 'origin', BASE_SHA, cwd=dest)
        run('git', 'checkout', '--detach', BASE_SHA, cwd=dest)
    assert run('git', 'rev-parse', 'HEAD', cwd=dest) == BASE_SHA, (
        'Moonlight TV revision changed. Use the pinned base revision or explicitly review and update patches.'
    )
    # Change only the common-c submodule URL; other submodules remain upstream.
    run('git', 'config', '-f', '.gitmodules', 'submodule.core/moonlight-common-c.url', COMMON_REPO, cwd=dest)
    run('git', 'submodule', 'sync', '--recursive', cwd=dest)
    run('git', 'submodule', 'update', '--init', '--recursive', '--depth', '1', cwd=dest)
    common = dest / 'core/moonlight-common-c'
    # Gitlink points at upstream; use the version actually built by Nonary's vrr18.
    run('git', 'fetch', '--depth', '1', 'origin', COMMON_SHA, cwd=common)
    run('git', 'checkout', '--detach', COMMON_SHA, cwd=common)
    run('git', 'submodule', 'update', '--init', '--recursive', '--depth', '1', cwd=common)
    assert run('git', 'rev-parse', 'HEAD', cwd=common) == COMMON_SHA
    header = (common / 'src/ControllerHaptics.h').read_text()
    for symbol in ['ML_HAPTICS_PACKET_TYPE 0x5601', 'LI_CCAP_HAPTICS_PCM 0x8000', 'ML_FF_HAPTICS_PCM 0x04']:
        if symbol not in header:
            raise RuntimeError(f'Incorrect Nonary extension: {symbol} missing')


def apply(dest: pathlib.Path) -> None:
    # The top-level root project is C-only. Nonary common-c runs a C++ CMake
    # compile probe even though its source probe is C; make it C-only too.
    common_cmake = dest / 'core/moonlight-common-c/CMakeLists.txt'
    patch_file(common_cmake, '    CHECK_CXX_SOURCE_COMPILES(',
               '    include(CheckCSourceCompiles)\n    CHECK_C_SOURCE_COMPILES(',
               'fix C-only common-c clock probe')

    root_cmake = dest / 'CMakeLists.txt'
    patch_file(root_cmake,
               'execute_process(COMMAND git describe --tags --abbrev=0 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}\n'
               '        OUTPUT_VARIABLE MOONLIGHT_VERSION ERROR_QUIET)',
               'set(MOONLIGHT_VERSION "v1.6.36")',
               'avoid live version lookups and pin 1.6.36 package')
    patch_file(root_cmake, 'set(WEBOS_APPINFO_ID "com.limelight.webos")',
               f'set(WEBOS_APPINFO_ID "{APP_ID}")',
               'independent webOS package ID')
    patch_file(dest / 'src/app/CMakeLists.txt', 'add_subdirectory(input)\nadd_subdirectory(stream)',
               'add_subdirectory(input)\nadd_subdirectory(stream)\n'
               'if (TARGET_WEBOS)\n'
               '    target_sources(moonlight-lib PRIVATE webos_dualsense/webos_haptics.c)\n'
               '    target_include_directories(moonlight-lib PRIVATE webos_dualsense)\n'
               'endif ()', 'build DualSense renderer')

    hooks = '#ifdef TARGET_WEBOS\n#include "webos_haptics.h"\n#endif\n'
    gamepad_c = dest / 'src/app/input/input_gamepad.c'
    patch_file(gamepad_c, '#include "app_input.h"', '#include "app_input.h"\n'+hooks, 'gamepad renderer includes')
    patch_file(gamepad_c, '    SDL_GameControllerClose(state->controller);',
               '#ifdef TARGET_WEBOS\n    webos_haptics_unbind((unsigned)state->gs_id);\n#endif\n'
               '    SDL_GameControllerClose(state->controller);', 'controller disconnection cleanup')
    patch_file(gamepad_c, '    app_gamepad_state_t *state = &input->gamepads[controller_id];',
               '    app_gamepad_state_t *state = &input->gamepads[controller_id];\n'
               '#ifdef TARGET_WEBOS\n    if (webos_haptics_playing(controller_id)) return;\n#endif',
               'prevent legacy rumble overriding waveform')

    input_c = dest / 'src/app/input/app_input.c'
    patch_file(input_c, '#include "app_input.h"', '#include "app_input.h"\n'+hooks, 'input lifecycle includes')
    patch_file(input_c, '    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);',
               '    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);\n'
               '#ifdef TARGET_WEBOS\n    webos_haptics_init();\n#endif', 'initialize waveform registry')
    patch_file(input_c, '    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);',
               '#ifdef TARGET_WEBOS\n    webos_haptics_shutdown();\n#endif\n'
               '    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);', 'destroy waveform registry')

    session_gamepad = dest / 'src/app/stream/input/session_gamepad.c'
    patch_file(session_gamepad, '#include "input/input_gamepad.h"',
               '#include "input/input_gamepad.h"\n'+hooks, 'stream controller includes')
    patch_file(session_gamepad,
               '    LiSendControllerArrivalEvent(gamepad->gs_id, input->input->activeGamepadMask, type,',
               '#ifdef TARGET_WEBOS\n'
               '    // Advertise native waveform only if the exact Bluetooth DualSense\n'
               '    // has a writable, unambiguous hidraw output path.\n'
               '    if (webos_haptics_bind((unsigned)gamepad->gs_id, gamepad->controller)) {\n'
               '        type = LI_CTYPE_PS;\n'
               '        capabilities |= LI_CCAP_HAPTICS_PCM;\n'
               '    }\n#endif\n'
               '    LiSendControllerArrivalEvent(gamepad->gs_id, input->input->activeGamepadMask, type,',
               'advertise real renderer only')

    connection = dest / 'src/app/stream/connection/session_connnection.c'
    patch_file(connection, '#include "input/input_gamepad.h"',
               '#include "input/input_gamepad.h"\n'+hooks,
               'connection callback includes')
    patch_file(connection, '        .setHdrMode = connection_set_hdr',
               '        .setHdrMode = connection_set_hdr\n'
               '#ifdef TARGET_WEBOS\n'
               '        , .controllerHaptics = webos_haptics_receive\n'
               '#endif', 'connection receives haptics packets')
    patch_file(connection, 'void session_connection_callbacks_reset(session_t *session) {\n',
               'void session_connection_callbacks_reset(session_t *session) {\n'
               '#ifdef TARGET_WEBOS\n'
               '    // After LiStopConnection(), drain and close HID workers.\n'
               '    webos_haptics_unbind_all();\n'
               '#endif\n', 'stream shutdown and cleanup')

    out = dest / 'src/app/webos_dualsense'
    out.mkdir(parents=True, exist_ok=True)
    for path in (ROOT / 'haptics').iterdir():
        if path.name in ('webos_haptics.c', 'webos_haptics.h'):
            shutil.copy2(path, out / path.name)
    (out / 'saxense').mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'haptics/saxense/packet.h', out / 'saxense/packet.h')

    metadata = dest / 'deploy/webos/appinfo.json'
    patch_file(metadata, '"title": "Moonlight"', '"title": "Moonlight DS5"', 'app display name')
    patch_file(metadata, '"vendor": "Mariotaku Lee"',
               '"vendor": "Moonlight TV contributors; DualSense adaptation"', 'fork attribution')
    patch_file(metadata, '"appDescription": "Open Source NVIDIA GameStream Client"',
               '"appDescription": "Experimental Moonlight TV with Vibepollo DualSense PCM"', 'app description')

    assets = dest / 'deploy/webos/assets/dualsense-licenses'
    assets.mkdir(parents=True, exist_ok=True)
    for name in ('LICENSE', 'PROVENANCE.md', 'packet.h'):
        shutil.copy2(ROOT / 'haptics/saxense' / name, assets / name)
    (assets / 'SOURCE.txt').write_text(
        'Complete corresponding source and build script must accompany distributions.\n'
        'Base: https://github.com/mariotaku/moonlight-tv\n'
        'Protocol: https://github.com/Nonary/moonlight-common-c\n'
        'SAxense: https://github.com/Nonary/moonlight-qt/tree/v6.1.0-vrr18/third-party/saxense\n'
    )
    print('Patched worktree:', dest)
    print('WARNING: not hardware-validated; check rooted TV hidraw access before publishing stable builds')


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dest', type=pathlib.Path, default=ROOT/'moonlight-tv-worktree')
    p.add_argument('--apply-only', action='store_true', help='Patch existing pinned, initialized worktree (for development)')
    args = p.parse_args()
    dest = args.dest.resolve()
    if not args.apply_only:
        clone_and_pin(dest)
    apply(dest)

if __name__ == '__main__':
    main()
