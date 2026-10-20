# Moonlight TV — Vibepollo DualSense Waveform Port

**Status: EXPERIMENTAL source port; testing webOS cross-build end-to-end TV/controller support on LG WEBOS.**

This is a reproducible *overlay project* which creates a separate LG webOS IPK
from [mariotaku/moonlight-tv](https://github.com/mariotaku/moonlight-tv),
with the waveform protocol from [Nonary/moonlight-common-c](https://github.com/Nonary/moonlight-common-c)
and a Linux Bluetooth DualSense SAxense renderer adapted from
[Nonary/moonlight-qt](https://github.com/Nonary/moonlight-qt).
It does **not** pretend Qt's desktop streaming/video implementation runs on webOS.

The distinct app ID `com.ray.moonlight.dualsense` leaves regular Moonlight TV installed.
The display name is **Moonlight DS5**.

## What was implemented

- Pinned original Moonlight TV at `cf96ecc72cdab3aae7ba0698f463b89065ae7e3b`.
- Replaces `core/moonlight-common-c` with Nonary's vrr18 protocol revision
  `d6a11bc685b41037b352a96f29d08276fe5359ba`.
- Advertises `ML_FF_HAPTICS_PCM` at connection level and `LI_CCAP_HAPTICS_PCM`
  only after the physical Bluetooth DualSense renderer binds successfully.
- Validates and receives 0x5601 encrypted-control PCM from Vibepollo using
  Nonary's common-c and queues it to the bounded SDL/hidraw renderer.
- Converts 48 kHz S16LE stereo actuator samples to 3 kHz stereo signed 8-bit
  samples with SDL_AudioStream, emitting SAxense Bluetooth packets with CRC.
- Limits stale packets, rejects duplicate/late sequences, clears resampler on
  gaps, emits silence on normal teardown, handles controller removal, and does
  not let ordinary rumble interrupt active waveform output.
- Retains the original Moonlight TV streaming pipeline and IPK packaging.

### Current restrictions

- **Bluetooth only**: DualSense (054c:0ce6) and DualSense Edge (054c:0df2).
  USB DualSense native audio playback is **not** implemented.
- The controller must be paired directly to the TV, visible to SDL as a Sony
  device on the Bluetooth bus, and have **one and only one** writable matching
  `/dev/hidraw*` interface. Multiple indistinguishable devices are rejected
  to avoid sending haptics to the wrong controller.
- Rooting the TV does not automatically grant sandboxed native apps access to
  the raw HID device. A TV-specific privilege solution might be necessary.
- This does not add support for Nonary's PyroWave codec, Windows WASAPI, or Qt
  presentation features. The original webOS client still handles A/V.
- Adaptive trigger *effects* are not yet handled by the webOS port. Existing
  ordinary trigger rumble/touchpad support is inherited from Moonlight TV.
- TVs that require an x86 or 64-bit ARM native binary need an additional target
  toolchain; the included build is specifically for 32-bit ARM webOS.
- Rendering correctness, Bluetooth timing, audio drift, multiple devices,
  hardware hotplug and TV-specific ARM ABI compatibility remain unverified.
- Standard Moonlight-TV hosts should continue to stream normally, but this
  fork of common-c needs a real regression build/test.

## Requirements to build

- Linux x86-64 environment with Python 3, Git, C/C++ build toolchain and CMake.
- LG webOS buildroot **32-bit ARM** NDK `openlgtv/buildroot-nc4`, the same toolchain used
  by the upstream Moonlight TV GitHub Actions.
- `ares-package` and webosbrew toolbox (also installed by upstream's workflow).
- Internet access to clone the GPL Moonlight repositories and their submodules.
- A rooted LG webOS TV with Bluetooth DualSense and writable hidraw interface
  to test waveform output.

## Build using GitHub Actions (recommended)

1. Extract the ZIP and create a **new GitHub repository**. From the extracted
   `moonlight-webos-dualsense` folder, run (requires Git and GitHub CLI):

   ```sh
   git init -b main
   git add .
   git commit -m "Initial webOS DualSense waveform port"
   gh repo create moonlight-webos-dualsense --public --source=. --remote=origin --push
   ```

2. Enable GitHub Actions in that repository.
3. Run **Build Moonlight DualSense webOS IPK** manually from the Actions tab.
4. If the cross-build succeeds, download `moonlight-webos-dualsense-ipk` from
   the workflow's Artifacts. This will contain a real native `.ipk`.
   The workflow also uploads a corresponding-source archive for redistribution.
5. A `v*` tag produces a **pre-release** with the IPK, for testing before any
   public stable release or Homebrew Channel submission.

The workflow downloads the same unofficial ARM NDK and packager used by the
existing Moonlight TV project. It is not necessary to build or ship Qt.

## Local build on Linux

```bash
./scripts/test.sh
export TOOLCHAIN_FILE=/opt/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
./scripts/build-ipk.sh
ls moonlight-tv-worktree/dist/*.ipk
```

The bootstrap refuses unknown upstream revisions and missing patch anchors.
It avoids building a silently incompatible IPK. A dirty pinned `moonlight-tv-worktree`
should be deleted before regenerating it; **do not remove it if it contains your work**.

## Install and inspect on rooted LG TV

Use a previously paired TV in webOS Developer Mode or a tool which has root
installation privileges. Example with an ares device configured as `webos`:

```bash
ares-install -d webos moonlight-tv-worktree/dist/com.ray.moonlight.dualsense_*.ipk
ares-launch -d webos com.ray.moonlight.dualsense
```

Check actual device info on the TV over an authorized root shell:

```sh
uname -m
ls -l /dev/hidraw* 2>/dev/null
for p in /sys/class/hidraw/hidraw*/device/uevent; do
  echo "$p"; grep -E 'HID_ID|HID_NAME|HID_UNIQ' "$p" 2>/dev/null || :
done
```

Only one `BUS_BLUETOOTH` Sony DualSense HID collection can be associated
safely with this initial port. If no writable hidraw device is found, the
code intentionally **does not advertise native waveform capability** for
that controller and falls back to existing behaviour. Avoid world-writable
`chmod 666 /dev/hidraw*`; narrow privileges to the Moonlight app instead.

## Enable the Windows host

1. Install current [Vibepollo](https://github.com/Nonary/Vibepollo) on the
   Windows streaming host and select **DualSense USB audio and haptics** in
   its installer.
2. Enable `usbip_ds5` or per-app DualSense waveform mode.
3. Pair the physical DualSense directly with the TV (not a generic USB dongle).
4. Launch **Moonlight DS5**, start a new stream and use a Windows game with
   genuine native wired DualSense haptics. Disable game-specific Steam Input
   remapping if it prevents native gamepad detection.
5. Look for an SDL log line `Bluetooth DualSense waveform ready for slot N`.
   Its presence proves only device association, **not** that game haptics work.

## Test criteria before publication as stable

- IPK cross-compiles, installs and launches on the intended webOS/CPU model.
- Regular gameplay and A/V streaming still work.
- Game recognizes composite DualSense *and* the 4-channel audio endpoint.
- Both actuator channels produce distinct native feedback over Bluetooth.
- Rumble, touchpad, sensors and any adaptive-trigger effects under test remain correct.
- Mid-stream disconnect/reconnect and app close leave no active HID worker.
- No stale PCM is delivered to a newly attached controller or wrong slot.
- Performance at 48 kHz input remains stable without haptic latency spikes.

## Source and credits

- Moonlight TV by mariotaku and contributors: GPL-3.0.
- Nonary common-c waveform extension: GPL-3.0-or-later.
- SAxense output packet adaptation by Nonary, based on work by Sdore/Egor
  Vorontsov: MPL-2.0 and GPL-3.0-or-later (see `haptics/saxense/`).
- All changes in this overlay: GPL-3.0-or-later. Preserve license notices.

This repository contains the additions and deterministic integration script,
not a vendored snapshot of third-party projects. The CI workflow bundles all
submodule contents used in the build as a corresponding-source archive and
attaches it to tagged pre-releases. Keep that source available whenever you
publish a binary; review every upstream project's license before distribution.
