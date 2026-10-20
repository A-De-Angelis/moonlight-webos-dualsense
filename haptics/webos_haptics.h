// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <ControllerHaptics.h>
#include <SDL_gamecontroller.h>

// Set up before any gamepads or stream callbacks; destroy after stream ends.
void webos_haptics_init(void);
void webos_haptics_shutdown(void);

// Valid only for a physical Bluetooth Sony DualSense that we can exclusively
// associate with a writable Linux hidraw output. Must not be used for adapters.
bool webos_haptics_bind(unsigned slot, SDL_GameController *controller);
void webos_haptics_unbind(unsigned slot);
void webos_haptics_unbind_all(void);
bool webos_haptics_playing(unsigned slot);

// Invoked directly on Moonlight's control receive thread. Never perform HID I/O here.
void webos_haptics_receive(uint16_t slot, uint32_t sequence,
                          const uint8_t *pcm, uint16_t frames);
