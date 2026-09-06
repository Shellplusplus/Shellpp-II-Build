#ifndef SHELLPP_SETTINGS_TEMPLATE_H
#define SHELLPP_SETTINGS_TEMPLATE_H

#include <stdint.h>

/* Caller must serialize with Settings creation on the firmware UI thread.
 * This helper does not make a four-pointer publication atomic. */
struct shellpp_settings_template_state {
    uint32_t resident;
    uint32_t installed;
};

static int shellpp_settings_template_matches(const volatile uint32_t *table,
        const uint32_t *expected) {
    uint32_t i;
    if (!table || !expected) return 0;
    for (i = 0; i < 10u; ++i)
        if (table[i] != expected[i]) return 0;
    return 1;
}

static int shellpp_settings_template_install(
        struct shellpp_settings_template_state *state,
        volatile uint32_t *table, const uint32_t *original,
        const uint32_t *replacement) {
    static const uint8_t slots[] = { 1u, 2u, 3u, 5u };
    uint32_t i;
    if (!state || !table || !original || !replacement) return -22;
    for (i = 0; i < 10u; ++i) {
        if (i == 1u || i == 2u || i == 3u || i == 5u) {
            if (!(replacement[i] & 1u)) return -22;
        } else if (replacement[i] != original[i]) {
            return -22;
        }
    }
    if (state->installed)
        return shellpp_settings_template_matches(table, replacement) ? 0 : -301;
    if (state->resident) return -302;
    if (!shellpp_settings_template_matches(table, original)) return -301;
    /* Never permit unload after even a partial publication. */
    state->resident = 1u;
    for (i = 0; i < sizeof(slots); ++i)
        table[slots[i]] = replacement[slots[i]];
    if (!shellpp_settings_template_matches(table, replacement)) return -302;
    state->installed = 1u;
    return 0;
}

#endif
