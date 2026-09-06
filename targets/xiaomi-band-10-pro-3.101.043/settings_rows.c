#include <stdint.h>
#include "shellpp_firmware_abi.h"
#include "shellpp_native_app.h"

/* Only the 043 build may compile this translation unit. */
#if SHELLPP_ABI_FIRMWARE_CODE != 3101043u
#error Settings callbacks require firmware 3.101.043
#endif

typedef uint32_t (*type_fn)(void *, uint32_t, void *);
typedef uint32_t (*count_fn)(void *, void *);
typedef void *(*create_fn)(void *, uint32_t, void *);
typedef void (*bind_fn)(void *, void *, uint32_t, void *);
typedef void *(*row_create_fn)(void *);
typedef void (*row_configure_fn)(void *, const char *, const char *,
    const char *, uint32_t, uint8_t, uint8_t);
typedef void (*row_update_fn)(void *, const void *, const char *,
    const char *, uint32_t, uint8_t);
typedef void (*event_add_fn)(void *, void (*)(void *), uint32_t, void *);
typedef uint32_t (*event_code_fn)(void *);
typedef void (*navigate_fn)(uint32_t, uint32_t, uint32_t, uint32_t);

static const char settings_icon[] =
    "/data/shellpp-ii/shellpp_ii_settings_icon.bin";
static const char settings_title[] = "Shell++ II";

static uint32_t original_count(void *list, void *context) {
    return ((count_fn)SHELLPP_ABI_SETTINGS_COUNT_ADDR)(list, context);
}

static int valid_count(uint32_t count) {
    return count == 9u || count == 10u;
}

uint32_t shellpp_settings_count(void *list, void *context) {
    uint32_t count = original_count(list, context);
    return valid_count(count) ? count + 1u : count;
}

uint32_t shellpp_settings_type(void *list, uint32_t index, void *context) {
    uint32_t count = original_count(list, context);
    /* Firmware cache stores type in one byte. Original types are 0..9. */
    if (valid_count(count) && index == count) return 0xfeu;
    if (index < count)
        return ((type_fn)SHELLPP_ABI_SETTINGS_TYPE_ADDR)(list, index, context);
    return 0xffu;
}

static void settings_click(void *event) {
    struct shellpp_native_status status;
    if (!event || ((event_code_fn)SHELLPP_ABI_LVX_EVENT_GET_CODE_ADDR)(event)
            != SHELLPP_ABI_EVENT_CLICKED) return;
    shellpp_native_get_status(&status);
    if (!status.registered || status.app_id != 0xcdu) return;
    ((navigate_fn)SHELLPP_ABI_ACTIVITY_NAVIGATE_ADDR)(0x00cd0000u, 0, 0, 0);
}

void *shellpp_settings_create(void *list, uint32_t index, void *context) {
    uint32_t count = original_count(list, context);
    void *row;
    if (!list) return 0;
    if (index < count)
        return ((create_fn)SHELLPP_ABI_SETTINGS_CREATE_ADDR)(list, index, context);
    if (!valid_count(count) || index != count) return 0;
    row = ((row_create_fn)SHELLPP_ABI_SETTINGS_ROW_CREATE_ADDR)(list);
    if (!row) return 0;
    ((row_configure_fn)SHELLPP_ABI_SETTINGS_ROW_CONFIGURE_ADDR)(row,
        settings_icon, settings_title, 0, 0u, 0u, 10u);
    ((event_add_fn)SHELLPP_ABI_LVX_EVENT_ADD_ADDR)(row, settings_click,
        SHELLPP_ABI_EVENT_CLICKED, 0);
    return row;
}

void shellpp_settings_bind(void *list, void *row, uint32_t index,
        void *context) {
    uint32_t count = original_count(list, context);
    if (!list || !row) return;
    if (index < count) {
        ((bind_fn)SHELLPP_ABI_SETTINGS_BIND_ADDR)(list, row, index, context);
        return;
    }
    if (!valid_count(count) || index != count) return;
    /* The unique type keeps this row out of the original callback cache.
     * Its click handler is installed once at creation and survives reuse. */
    ((row_update_fn)SHELLPP_ABI_LVX_LIST_ROW_UPDATE_ADDR)(row, settings_icon,
        settings_title, 0, 0u, 1u);
}
