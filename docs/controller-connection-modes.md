# DualSense connection modes: LG webOS vs. Windows PC

## Controller paired to LG TV

```
DualSense --Bluetooth--> LG webOS Moonlight DS5 --gamepad input--> Vibepollo on PC
         <--hidraw/SDL--                         <--feedback / PCM--
```

The Windows host must advertise a **virtual USB DualSense with a 4-channel controller
waveform audio endpoint** for games to emit native haptics. Stock Apollo or
stock Sunshine cannot be assumed to implement Nonary's 0x5601 PCM transport.
Install Vibepollo with its optional **DualSense USB audio and haptics** component;
the host feature still requires an actual Bluetooth renderer on the TV.

The TV implementation has separate feedback paths:

- SDL PS5 HIDAPI: ordinary rumble, trigger motors, LED and the host's adaptive
  trigger effect blocks (0x5503).
- Raw Bluetooth HID (`/dev/hidraw`): actuator PCM encoded into SAxense 0x32
  output reports, using Nonary's 0x5601 controller haptics extension.

If the app cannot write the controller's `/dev/hidraw*`, it advertises no PCM
capability. Adaptive triggers can still work via SDL PS5 HIDAPI. This is an
intentional safe fallback, not proof of full native haptics.

### Installation validation

1. Install the CI-built IPK on the **rooted TV** and pair one DualSense by
   Bluetooth. Avoid simultaneously pairing the controller to Windows.
2. Run the read-only `scripts/diagnose-webos-dualsense.sh` probe on the TV;
   collect the output (it omits HID serials/MACs).
3. Confirm the physical DualSense HID device is visible and writable **by the
   app**, not just by a root shell. Don't make `/dev/hidraw*` world-writable.
4. Start a compatible game through Vibepollo and observe whether the TV client
   logs `Bluetooth DualSense waveform ready for slot N`.
5. Verify buttons, both analog triggers, gyro, touchpad, lighting, individual
   adaptive trigger effects, actuator left/right separation, disconnect/reconnect.

**Limitations:** USB-paired TV DualSense waveform, speaker, microphone, headset
jack, and simultaneous same-model controllers need additional work.

## Controller paired to Windows PC

```
DualSense --Bluetooth--> Windows PC --game input--> Windows game
LG TV <--Moonlight A/V stream-- PC
```

No special Moonlight client or Vibepollo waveform emulation is needed: the
actual game reads the physical PC controller. Avoid forwarding that controller
again as a Moonlight virtual pad. Native game compatibility depends on Steam
Input, the game, and Windows' Bluetooth handling.

Native waveform haptics typically require a **USB audio interface**, which
ordinary Windows Bluetooth pairing does not supply. For full game-native
features use a direct **USB-C cable** or a **Raspberry Pi Pico 2 W running
DS5Dongle**: the controller connects wirelessly to the Pico; Windows sees a
wired USB DualSense, including its controller audio endpoint. Evaluate the
particular DS5Dongle fork/firmware and game's compatibility before relying on
mic/speaker features.

References:
- Sony: https://www.playstation.com/en-us/support/playstation-gaming-on-pc/
- DS5Dongle: https://github.com/awalol/DS5Dongle
- Vibepollo native waveform design: https://github.com/Nonary/Vibepollo/blob/master/docs/windows-dualsense-haptics.md
