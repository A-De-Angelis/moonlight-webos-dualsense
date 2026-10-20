#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "saxense/packet.h"
#include "ControllerHaptics.h"

int main(void) {
    // Correct 5 ms (240 stereo frames, 16-bit LE) message from Vibepollo.
    uint8_t p[ML_HAPTICS_MAX_PAYLOAD] = {0};
    p[0] = 1; p[1] = 1;
    MlHapticsWrite16(p + 2, 3);
    MlHapticsWrite32(p + 4, 0x12345678);
    MlHapticsWrite16(p + 8, ML_HAPTICS_MAX_FRAMES);
    for (unsigned i=0;i<ML_HAPTICS_MAX_FRAMES*4;++i) p[12+i] = (uint8_t)i;
    assert(MlHapticsValidate(p, sizeof(p)));
    assert(MlHapticsRead16(p + 2) == 3);
    assert(MlHapticsRead32(p + 4) == 0x12345678);
    assert(!MlHapticsValidate(p, sizeof(p)-1));
    p[1] = 2; assert(!MlHapticsValidate(p, sizeof(p)));
    p[1] = 1; MlHapticsWrite16(p + 2, 16);
    assert(!MlHapticsValidate(p, sizeof(p)));

    uint8_t pcm[SAXENSE_PCM_BYTES];
    for (unsigned i=0;i<sizeof(pcm);++i) pcm[i] = (uint8_t)i;
    uint8_t a[SAXENSE_REPORT_BYTES], b[SAXENSE_REPORT_BYTES];
    saxense_packet(a, 0xA5, pcm);
    saxense_packet(b, 0xA5, pcm);
    assert(!memcmp(a,b,sizeof(a)));
    assert(a[0] == 0x32 && a[2] == 0x91 && a[10] == 0xA5);
    assert(a[11] == 0x92 && a[12] == SAXENSE_PCM_BYTES);
    assert(!memcmp(a+13,pcm,sizeof(pcm)));
    // Independent reflected CRC32 with the Bluetooth HID 0xA2 prefix.
    uint8_t crc_input[1 + SAXENSE_REPORT_BYTES-4];
    crc_input[0] = 0xA2;
    memcpy(crc_input + 1, a, sizeof(a)-4);
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned i=0;i<sizeof(crc_input);++i) {
        crc ^= crc_input[i];
        for (unsigned j=0;j<8;++j)
            crc = (crc>>1) ^ ((crc & 1) ? 0xEDB88320u : 0u);
    }
    crc = ~crc;
    assert(a[138] == (uint8_t)crc);
    assert(a[139] == (uint8_t)(crc>>8));
    assert(a[140] == (uint8_t)(crc>>16));
    assert(a[141] == (uint8_t)(crc>>24));
    puts("PASS: waveform payload, limits, sequence and SAxense CRC");
    return 0;
}
