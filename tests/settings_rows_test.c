#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "shellpp_firmware_abi.h"
#include "shellpp_native_app.h"

static unsigned count_value, forwarded_create, forwarded_bind, configured;
static unsigned event_added, updated, navigated, registered = 1;
static int list_object, row_object, context_object;
static void (*click_callback)(void *);
static uint32_t mock_count(void *list, void *context) {
    assert(list == &list_object && context == &context_object);
    return count_value;
}
static uint32_t mock_type(void *list, uint32_t index, void *context) {
    assert(mock_count(list, context) > index);
    return index;
}
static void *mock_create(void *list, uint32_t index, void *context) {
    assert(mock_count(list, context) > index);
    forwarded_create++;
    return &row_object;
}
static void mock_bind(void *list, void *row, uint32_t index, void *context) {
    assert(mock_count(list, context) > index && row == &row_object);
    forwarded_bind++;
}
static void *mock_row(void *list) {
    assert(list == &list_object);
    return &row_object;
}
static void check_text(void *row, const char *icon, const char *title,
        const char *subtitle) {
    assert(row == &row_object);
    assert(strcmp(icon, "/data/shellpp-ii/shellpp_ii_settings_icon.bin") == 0);
    assert(strcmp(title, "Shell++ II") == 0 && subtitle == 0);
}
static void mock_configure(void *row, const char *icon, const char *title,
        const char *subtitle, uint32_t trailing, uint8_t selected, uint8_t style) {
    check_text(row, icon, title, subtitle);
    assert(trailing == 0 && selected == 0 && style == 10);
    configured++;
}
void shellpp_test_row_update(void *row, const void *icon, const char *title,
        const char *subtitle, uint32_t trailing, uint8_t selected) {
    check_text(row, icon, title, subtitle);
    assert(trailing == 0 && selected == 1);
    updated++;
}
void shellpp_test_event_add(void *row, void (*callback)(void *),
        uint32_t code, void *context) {
    assert(row == &row_object && code == 7 && context == 0);
    click_callback = callback;
    event_added++;
}
uint32_t shellpp_test_event_code(void *event) { return *(uint32_t *)event; }
void shellpp_test_navigate(uint32_t key, uint32_t a, uint32_t b, uint32_t c) {
    assert(key == 0x00cd0000 && a == 0 && b == 0 && c == 0);
    navigated++;
}
void shellpp_native_get_status(struct shellpp_native_status *status) {
    memset(status, 0, sizeof(*status));
    status->app_id = 0xcd;
    status->registered = registered;
}
#ifndef SHELLPP_ABI_FIRMWARE_CODE
#define SHELLPP_ABI_FIRMWARE_CODE 3101043u
#endif
#define SHELLPP_ABI_SETTINGS_COUNT_ADDR mock_count
#define SHELLPP_ABI_SETTINGS_TYPE_ADDR mock_type
#define SHELLPP_ABI_SETTINGS_CREATE_ADDR mock_create
#define SHELLPP_ABI_SETTINGS_BIND_ADDR mock_bind
#define SHELLPP_ABI_SETTINGS_ROW_CREATE_ADDR mock_row
#define SHELLPP_ABI_SETTINGS_ROW_CONFIGURE_ADDR mock_configure
#if SHELLPP_ABI_FIRMWARE_CODE == 3101036u
#include "../targets/xiaomi-band-10-pro-3.101.036/settings_rows.c"
#else
#include "../targets/xiaomi-band-10-pro-3.101.043/settings_rows.c"
#endif

int main(void) {
    for (count_value = 9; count_value <= 10; count_value++) {
        forwarded_create = forwarded_bind = configured = updated = event_added = 0;
        assert(shellpp_settings_count(&list_object, &context_object) == count_value + 1);
        for (unsigned i = 0; i < count_value; i++) {
            assert(shellpp_settings_type(&list_object, i, &context_object) == i);
            assert(shellpp_settings_create(&list_object, i, &context_object) == &row_object);
            shellpp_settings_bind(&list_object, &row_object, i, &context_object);
        }
        assert(shellpp_settings_type(&list_object, count_value, &context_object) == 0xfe);
        assert(shellpp_settings_create(&list_object, count_value, &context_object) == &row_object);
        for (unsigned i = 0; i < 5; i++)
            shellpp_settings_bind(&list_object, &row_object, count_value, &context_object);
        assert(forwarded_create == count_value && forwarded_bind == count_value);
        assert(configured == 1 && event_added == 1 && updated == 5);
        assert(shellpp_settings_create(&list_object, count_value + 1, &context_object) == 0);
        shellpp_settings_bind(&list_object, &row_object, count_value + 1, &context_object);
        assert(forwarded_bind == count_value && updated == 5);
        uint32_t code = 24;
        unsigned before = navigated;
        click_callback(&code);
        assert(navigated == before);
        code = 7; registered = 0; click_callback(&code);
        assert(navigated == before);
        registered = 1; click_callback(&code);
        assert(navigated == before + 1);
    }
    puts("Settings row routing and click tests passed (mock firmware)");
    return 0;
}
