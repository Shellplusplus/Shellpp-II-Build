#ifdef SHELLPP_TEST_036
#include "../targets/xiaomi-band-10-pro-3.101.036/settings_template.h"
#else
#include "../targets/xiaomi-band-10-pro-3.101.043/settings_template.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
#ifdef SHELLPP_TEST_036
    const uint32_t original[10] = {0, 0x0c544875, 0x0c545039,
        0x0c544941, 0, 0x0c5448c1, 0x0c544b09, 0x0c544b19, 0, 0};
#else
    const uint32_t original[10] = {0, 0x0c56634d, 0x0c566b75,
        0x0c566429, 0, 0x0c5663a9, 0x0c5665f5, 0x0c566605, 0, 0};
#endif
    uint32_t replacement[10], table[10], before[10];
    struct shellpp_settings_template_state state;
    memcpy(replacement, original, sizeof(original));
    replacement[1] = 0x101; replacement[2] = 0x201;
    replacement[3] = 0x301; replacement[5] = 0x501;
    for (unsigned i = 0; i < 10; ++i) {
        memset(&state, 0, sizeof(state));
        memcpy(table, original, sizeof(table));
        table[i] ^= 2;
        memcpy(before, table, sizeof(table));
        assert(shellpp_settings_template_install(&state, table, original, replacement) == -301);
        assert(memcmp(table, before, sizeof(table)) == 0);
        assert(!state.resident && !state.installed);
    }
    memset(&state, 0, sizeof(state));
    memcpy(table, original, sizeof(table));
    replacement[6] ^= 2;
    assert(shellpp_settings_template_install(&state, table, original, replacement) == -22);
    assert(memcmp(table, original, sizeof(table)) == 0);
    replacement[6] ^= 2;
    replacement[1] &= ~1u;
    assert(shellpp_settings_template_install(&state, table, original, replacement) == -22);
    assert(!state.resident);
    replacement[1] |= 1u;
    assert(shellpp_settings_template_install(&state, table, original, replacement) == 0);
    assert(state.resident && state.installed);
    assert(memcmp(table, replacement, sizeof(table)) == 0);
    assert(shellpp_settings_template_install(&state, table, original, replacement) == 0);
    table[2] ^= 2;
    memcpy(before, table, sizeof(table));
    assert(shellpp_settings_template_install(&state, table, original, replacement) == -301);
    assert(state.resident && memcmp(table, before, sizeof(table)) == 0);
    state.installed = 0;
    assert(shellpp_settings_template_install(&state, table, original, replacement) == -302);
    assert(state.resident && memcmp(table, before, sizeof(table)) == 0);
    puts("Settings template guard tests passed (host only)");
    return 0;
}
