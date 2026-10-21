#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "dualsense_effects.h"
int main(void) {
    unsigned char l[10], r[10];
    for (unsigned i = 0; i < 10; ++i) { l[i] = i + 1; r[i] = i + 20; }
    ds5_output_effect_t both = ds5_make_trigger_effect(0xff, 0x21, 0x26, l, r);
    assert(sizeof(both) == 47);
    assert(offsetof(ds5_output_effect_t, right_effect_type) == 10);
    assert(offsetof(ds5_output_effect_t, left_effect_type) == 21);
    assert(both.valid_flag0 == 0x0c);
    assert(both.right_effect_type == 0x26 && both.left_effect_type == 0x21);
    assert(memcmp(both.left_effect, l, 10) == 0);
    assert(memcmp(both.right_effect, r, 10) == 0);
    ds5_output_effect_t left_only = ds5_make_trigger_effect(DS5_TRIGGER_LEFT, 0, 0xff, l, r);
    assert(left_only.valid_flag0 == DS5_TRIGGER_LEFT);
    assert(left_only.left_effect_type == 0); // valid release request
    assert(left_only.right_effect_type == 0);
    for (unsigned i = 0; i < 10; ++i) assert(left_only.right_effect[i] == 0);
    ds5_output_effect_t none = ds5_make_trigger_effect(0, 0xff, 0xff, l, r);
    ds5_output_effect_t zero = {0};
    assert(memcmp(&none, &zero, sizeof(none)) == 0);
    puts("PASS: DualSense adaptive-trigger payload, selective flags and zeroed fields");
    return 0;
}
