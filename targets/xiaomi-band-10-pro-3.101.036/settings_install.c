#include <stdint.h>
#include "shellpp_firmware_abi.h"
#include "shellpp_native_app.h"
#include "settings_template.h"

#if SHELLPP_ABI_FIRMWARE_CODE != 3101036u
#error Settings installation requires firmware 3.101.036
#endif

uint32_t shellpp_settings_count(void *, void *);
uint32_t shellpp_settings_type(void *, uint32_t, void *);
void *shellpp_settings_create(void *, uint32_t, void *);
void shellpp_settings_bind(void *, void *, uint32_t, void *);

static struct shellpp_settings_template_state settings_state;

/* NuttX modlib does not resolve compiler runtime imports. Volatile stores
 * keep this implementation from being lowered to a call to itself. */
void __aeabi_memclr4(void *address, uint32_t length) {
    volatile uint8_t *bytes = (volatile uint8_t *)address;
    while (length--) *bytes++ = 0;
}

uint32_t shellpp_settings_resident(void) { return settings_state.resident; }
uint32_t shellpp_settings_installed(void) { return settings_state.installed; }

/* Invoke synchronously from a LuaLVGL callback, serialized with Settings UI.
 * No list pointer is retained beyond this call. */
int shellpp_settings_start(void) {
    const uint32_t original[10] = {
        0, SHELLPP_ABI_SETTINGS_TYPE_ADDR, SHELLPP_ABI_SETTINGS_CREATE_ADDR,
        SHELLPP_ABI_SETTINGS_BIND_ADDR, 0, SHELLPP_ABI_SETTINGS_COUNT_ADDR,
        0x0c544b09u, 0x0c544b19u, 0, 0
    };
    uint32_t replacement[10];
    volatile uint32_t *table = (volatile uint32_t *)SHELLPP_ABI_SETTINGS_TEMPLATE_ADDR;
    uintptr_t list = *(volatile uintptr_t *)SHELLPP_ABI_SETTINGS_LIST_SLOT_ADDR;
    uintptr_t second = *(volatile uintptr_t *)SHELLPP_ABI_SETTINGS_SECOND_LIST_SLOT_ADDR;
    volatile uint32_t *live = 0;
    struct shellpp_native_status app;
    struct shellpp_settings_template_state live_state = {0, 0};
    int result;
    uint32_t i;
    shellpp_native_get_status(&app);
    if (!app.registered || app.app_id != 0xcdu) return -102;
    for (i = 0; i < 10u; ++i) replacement[i] = original[i];
    replacement[1] = (uint32_t)(uintptr_t)shellpp_settings_type;
    replacement[2] = (uint32_t)(uintptr_t)shellpp_settings_create;
    replacement[3] = (uint32_t)(uintptr_t)shellpp_settings_bind;
    replacement[5] = (uint32_t)(uintptr_t)shellpp_settings_count;
    if (!shellpp_settings_template_matches(table,
            settings_state.installed ? replacement : original)) return -301;
    /* The two slots differ while firmware creates or destroys the list.
     * Reject that transitional state before publishing any callbacks. */
    if (list != second) return -303;
    if (list) {
        if ((list & 3u) || (list >> 24) != 0x20u ||
                (list & 0x00ffffffu) > 0x00fff000u)
            return -303;
        live = (volatile uint32_t *)(list + 0x5cu);
        if (shellpp_settings_template_matches(live, replacement)) {
            if (!settings_state.installed) return -303;
            live = 0;
        } else if (!shellpp_settings_template_matches(live, original)) {
            return -303;
        }
    }
    result = shellpp_settings_template_install(&settings_state, table,
        original, replacement);
    if (result) return result;
    if (live) {
        result = shellpp_settings_template_install(&live_state, live,
            original, replacement);
        if (result) return result;
        ((void (*)(void *))SHELLPP_ABI_SETTINGS_REFRESH_ADDR)((void *)list);
    }
    return 0;
}
