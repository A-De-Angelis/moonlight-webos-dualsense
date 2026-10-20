// SPDX-License-Identifier: GPL-3.0-or-later
// webOS adaptation of Nonary/moonlight-qt's Linux Bluetooth DualSense renderer.
// The SAxense packet format is separately licensed: see saxense/PROVENANCE.md.
#include "webos_haptics.h"
#include "saxense/packet.h"
#include <ControllerHaptics.h>
#include <SDL.h>
#include <SDL_version.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MAX_SLOTS 16
#define QUEUE_DEPTH 8
#define MAX_HIDRAW 64
#define STALE_MS 40u
#define IDLE_MS 60u
#define PLAYBACK_US (32000000ULL / SAXENSE_RATE)  // 32 8-bit stereo frames, 10.67 ms

typedef struct {
    Uint32 sequence;
    Uint32 at_ms;
    Uint16 frames;
    Uint8 data[ML_HAPTICS_MAX_FRAMES * ML_HAPTICS_FRAME_BYTES];
} pcm_chunk_t;

typedef struct {
    SDL_mutex *mu;
    SDL_cond *changed;
    SDL_Thread *thread;
    SDL_AudioStream *converter;
    SDL_GameController *controller; // Borrowed. Unbind before SDL closes it.
    int hid_fd;
    Uint16 product;
    Uint8 hid_sequence;
    bool stopping, failed, playing, have_sequence;
    Uint32 expected_sequence, last_input_sequence, last_nonzero_ms;
    bool have_input_sequence;
    unsigned read_pos, used;
    pcm_chunk_t chunks[QUEUE_DEPTH];
} haptic_slot_t;

static SDL_mutex *registry_mu;
static haptic_slot_t *registry[MAX_SLOTS];

static Uint64 clock_us(void) {
    Uint64 count = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    return (count / freq) * 1000000ULL + ((count % freq) * 1000000ULL) / freq;
}

// Deliberately refuse ambiguous controller association. A rooted TV can expose
// several hidraw nodes for one physical pad; absent a stable SDL<->hidraw path,
// never guess if more than one matching Bluetooth HID collection is present.
static int open_unique_hidraw(Uint16 product) {
    int selected_fd = -1;
    unsigned matched = 0;
    for (unsigned i = 0; i < MAX_HIDRAW; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/hidraw%u", i);
        int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        struct hidraw_devinfo info;
        memset(&info, 0, sizeof(info));
        if (ioctl(fd, HIDIOCGRAWINFO, &info) == 0 &&
            info.bustype == BUS_BLUETOOTH && info.vendor == 0x054c &&
            info.product == product) {
            ++matched;
            if (matched == 1) { selected_fd = fd; continue; }
        }
        close(fd);
    }
    if (matched == 1) return selected_fd;
    if (selected_fd >= 0) close(selected_fd);
    SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
        "DualSense PCM unavailable: %u matching Bluetooth hidraw nodes (must be 1)", matched);
    return -1;
}

static bool hid_write(haptic_slot_t *s, const Uint8 pcm[SAXENSE_PCM_BYTES]) {
    Uint8 report[SAXENSE_REPORT_BYTES];
    saxense_packet(report, s->hid_sequence++, pcm);
    ssize_t n = write(s->hid_fd, report, sizeof(report));
    if (n == (ssize_t)sizeof(report)) return true;
    // A nonblocking write error is not a successful submission.
    SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DualSense PCM hidraw write: %s", strerror(errno));
    return false;
}

static int playback_main(void *userdata) {
    haptic_slot_t *s = (haptic_slot_t *)userdata;
    Uint64 next_us = clock_us();
    bool active = false;
    SDL_LockMutex(s->mu);
    while (!s->stopping && !s->failed) {
        if (!active && s->used == 0) {
            SDL_CondWait(s->changed, s->mu);
            continue;
        }
        if (!active) {
            active = true;
            s->playing = true;
            next_us = clock_us();
            SDL_UnlockMutex(s->mu);
            // Prevent SDL's cached rumble from overwriting actuator packets.
            SDL_GameControllerRumble(s->controller, 0, 0, 0);
            SDL_LockMutex(s->mu);
        }
        Uint32 now_ms = SDL_GetTicks();
        while (s->used != 0) {
            pcm_chunk_t *c = &s->chunks[s->read_pos];
            bool stale = SDL_TICKS_PASSED(now_ms, c->at_ms + STALE_MS);
            bool late = s->have_sequence && (Sint32)(c->sequence - s->expected_sequence) < 0;
            if (stale || (s->have_sequence && !late && c->sequence != s->expected_sequence)) {
                SDL_AudioStreamClear(s->converter);
                s->have_sequence = false;
            }
            if (!stale && !late) {
                s->expected_sequence = c->sequence + 1;
                s->have_sequence = true;
                if (SDL_AudioStreamAvailable(s->converter) > 4 * SAXENSE_PCM_BYTES)
                    SDL_AudioStreamClear(s->converter);
                if (SDL_AudioStreamPut(s->converter, c->data,
                                       c->frames * ML_HAPTICS_FRAME_BYTES) < 0)
                    s->failed = true;
            }
            s->read_pos = (s->read_pos + 1) % QUEUE_DEPTH;
            --s->used;
        }
        if (s->failed || s->stopping) break;
        Uint64 now_us = clock_us();
        if (now_us < next_us) {
            // Do not block the receiving thread; it only queues bounded copies.
            Uint32 wait_ms = (Uint32)((next_us - now_us + 999) / 1000);
            SDL_CondWaitTimeout(s->changed, s->mu, wait_ms > 10 ? 10 : wait_ms);
            continue;
        }
        Uint8 pcm[SAXENSE_PCM_BYTES] = {0};
        if (SDL_AudioStreamAvailable(s->converter) >= SAXENSE_PCM_BYTES &&
            SDL_AudioStreamGet(s->converter, pcm, sizeof(pcm)) != (int)sizeof(pcm)) {
            s->failed = true;
            break;
        }
        bool idle = SDL_TICKS_PASSED(now_ms, s->last_nonzero_ms + IDLE_MS);
        if (idle) {
            SDL_AudioStreamClear(s->converter);
            memset(pcm, 0, sizeof(pcm));
            s->have_sequence = false;
            s->used = 0;
        }
        SDL_UnlockMutex(s->mu);
        bool ok = hid_write(s, pcm);
        SDL_LockMutex(s->mu);
        if (!ok) s->failed = true;
        if (idle) { active = false; s->playing = false; }
        next_us += PLAYBACK_US;
        if (next_us < clock_us()) next_us = clock_us(); // No catch-up burst.
    }
    s->playing = false;
    s->used = 0;
    SDL_UnlockMutex(s->mu);
    // Write a final silence frame on normal teardown, never after failed I/O.
    if (!s->failed && active) {
        Uint8 silence[SAXENSE_PCM_BYTES] = {0};
        (void)hid_write(s, silence);
    }
    return 0;
}

void webos_haptics_init(void) {
    if (!registry_mu) registry_mu = SDL_CreateMutex();
}

static void destroy_slot(haptic_slot_t *s) {
    if (!s) return;
    SDL_LockMutex(s->mu);
    s->stopping = true;
    SDL_CondSignal(s->changed);
    SDL_UnlockMutex(s->mu);
    if (s->thread) SDL_WaitThread(s->thread, NULL);
    if (s->hid_fd >= 0) close(s->hid_fd);
    if (s->converter) SDL_FreeAudioStream(s->converter);
    SDL_DestroyCond(s->changed);
    SDL_DestroyMutex(s->mu);
    SDL_free(s);
}

bool webos_haptics_bind(unsigned slot, SDL_GameController *controller) {
    if (!registry_mu || !controller || slot >= MAX_SLOTS) return false;
#if SDL_VERSION_ATLEAST(2, 0, 6)
    Uint16 vendor = SDL_GameControllerGetVendor(controller);
    Uint16 product = SDL_GameControllerGetProduct(controller);
    if (vendor != 0x054c || (product != 0x0ce6 && product != 0x0df2)) return false;
    // SDL must identify the *same bus* as hidraw, not a USB adapter or virtual pad.
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller));
    if ((Uint16)(guid.data[0] | (guid.data[1] << 8)) != BUS_BLUETOOTH) return false;
#else
    return false;
#endif
    SDL_LockMutex(registry_mu);
    if (registry[slot]) { SDL_UnlockMutex(registry_mu); return true; }
    for (unsigned i = 0; i < MAX_SLOTS; ++i) {
        if (registry[i] && registry[i]->product == product) {
            SDL_UnlockMutex(registry_mu);
            return false;
        }
    }
    int fd = open_unique_hidraw(product);
    if (fd < 0) { SDL_UnlockMutex(registry_mu); return false; }
    haptic_slot_t *s = SDL_calloc(1, sizeof(*s));
    if (!s) { close(fd); SDL_UnlockMutex(registry_mu); return false; }
    s->hid_fd = fd;
    s->product = product;
    s->controller = controller;
    s->mu = SDL_CreateMutex();
    s->changed = SDL_CreateCond();
    s->converter = SDL_NewAudioStream(AUDIO_S16LSB, 2, 48000,
                                     AUDIO_S8, 2, SAXENSE_RATE);
    if (!s->mu || !s->changed || !s->converter) {
        // Avoid calling destroy_slot until a mutex exists.
        if (s->converter) SDL_FreeAudioStream(s->converter);
        if (s->changed) SDL_DestroyCond(s->changed);
        if (s->mu) SDL_DestroyMutex(s->mu);
        close(fd); SDL_free(s); SDL_UnlockMutex(registry_mu);
        return false;
    }
    s->thread = SDL_CreateThread(playback_main, "ds5-waveform", s);
    if (!s->thread) {
        destroy_slot(s); SDL_UnlockMutex(registry_mu); return false;
    }
    registry[slot] = s;
    SDL_UnlockMutex(registry_mu);
    SDL_LogInfo(SDL_LOG_CATEGORY_INPUT, "Bluetooth DualSense waveform ready for slot %u", slot);
    return true;
}

void webos_haptics_unbind(unsigned slot) {
    if (!registry_mu || slot >= MAX_SLOTS) return;
    SDL_LockMutex(registry_mu);
    haptic_slot_t *s = registry[slot];
    registry[slot] = NULL;
    SDL_UnlockMutex(registry_mu);
    destroy_slot(s);
}

void webos_haptics_unbind_all(void) {
    for (unsigned i = 0; i < MAX_SLOTS; ++i) webos_haptics_unbind(i);
}

void webos_haptics_shutdown(void) {
    if (!registry_mu) return;
    webos_haptics_unbind_all();
    SDL_DestroyMutex(registry_mu);
    registry_mu = NULL;
}

bool webos_haptics_playing(unsigned slot) {
    if (!registry_mu || slot >= MAX_SLOTS) return false;
    SDL_LockMutex(registry_mu);
    haptic_slot_t *s = registry[slot];
    bool value = false;
    if (s) {
        SDL_LockMutex(s->mu);
        value = s->playing && !s->failed;
        SDL_UnlockMutex(s->mu);
    }
    SDL_UnlockMutex(registry_mu);
    return value;
}

void webos_haptics_receive(uint16_t slot, uint32_t sequence,
                          const uint8_t *pcm, uint16_t frames) {
    if (!registry_mu || !pcm || slot >= MAX_SLOTS ||
        frames == 0 || frames > ML_HAPTICS_MAX_FRAMES) return;
    SDL_LockMutex(registry_mu);
    haptic_slot_t *s = registry[slot];
    if (s) {
        SDL_LockMutex(s->mu);
        if (!s->stopping && !s->failed &&
            (!s->have_input_sequence || (Sint32)(sequence - s->last_input_sequence) > 0)) {
            s->last_input_sequence = sequence;
            s->have_input_sequence = true;
            bool nonzero = false;
            for (unsigned i = 0; i < frames * ML_HAPTICS_FRAME_BYTES; ++i) {
                if (pcm[i] != 0) { nonzero = true; break; }
            }
            Uint32 now = SDL_GetTicks();
            if (nonzero) s->last_nonzero_ms = now;
            if (nonzero || s->playing || s->used) {
                if (s->used == QUEUE_DEPTH) { s->used = 0; s->read_pos = 0; }
                pcm_chunk_t *c = &s->chunks[(s->read_pos + s->used) % QUEUE_DEPTH];
                c->sequence = sequence;
                c->frames = frames;
                c->at_ms = now;
                memcpy(c->data, pcm, frames * ML_HAPTICS_FRAME_BYTES);
                ++s->used;
                SDL_CondSignal(s->changed);
            }
        }
        SDL_UnlockMutex(s->mu);
    }
    SDL_UnlockMutex(registry_mu);
}
