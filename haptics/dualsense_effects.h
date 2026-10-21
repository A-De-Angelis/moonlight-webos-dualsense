// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <string.h>

// Matches the 47-byte effect payload consumed by SDL_GameControllerSendEffect()
// for DualSense (see Moonlight Qt's DualSenseOutputReport and SDL's PS5 HIDAPI).
// The host's 11-byte effect blocks are transferred opaquely; no invented
// resistance profiles or unsupported modes are synthesized on the TV.
#define DS5_EFFECT_BLOCK_SIZE 10u
#define DS5_TRIGGER_RIGHT 0x04u
#define DS5_TRIGGER_LEFT 0x08u

typedef struct {
    uint8_t valid_flag0;
    uint8_t valid_flag1;
    uint8_t motor_right;
    uint8_t motor_left;
    uint8_t reserved_audio[4];
    uint8_t mute_button_led;
    uint8_t power_save_control;
    uint8_t right_effect_type;
    uint8_t right_effect[DS5_EFFECT_BLOCK_SIZE];
    uint8_t left_effect_type;
    uint8_t left_effect[DS5_EFFECT_BLOCK_SIZE];
    uint8_t reserved2[6];
    uint8_t valid_flag2;
    uint8_t reserved3[2];
    uint8_t lightbar_setup;
    uint8_t led_brightness;
    uint8_t player_leds;
    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
} ds5_output_effect_t;

_Static_assert(sizeof(ds5_output_effect_t) == 47, "DualSense SDL effect length changed");

// Preserve only the two trigger update flags. A zero type is a valid effect
// update (release resistance); don't skip it when host explicitly requests it.
static inline ds5_output_effect_t ds5_make_trigger_effect(uint8_t flags,
        uint8_t left_type, uint8_t right_type,
        const uint8_t left[DS5_EFFECT_BLOCK_SIZE],
        const uint8_t right[DS5_EFFECT_BLOCK_SIZE]) {
    ds5_output_effect_t report = {0};
    report.valid_flag0 = (uint8_t)(flags & (DS5_TRIGGER_LEFT | DS5_TRIGGER_RIGHT));
    if (report.valid_flag0 & DS5_TRIGGER_RIGHT) {
        report.right_effect_type = right_type;
        memcpy(report.right_effect, right, DS5_EFFECT_BLOCK_SIZE);
    }
    if (report.valid_flag0 & DS5_TRIGGER_LEFT) {
        report.left_effect_type = left_type;
        memcpy(report.left_effect, left, DS5_EFFECT_BLOCK_SIZE);
    }
    return report;
}
