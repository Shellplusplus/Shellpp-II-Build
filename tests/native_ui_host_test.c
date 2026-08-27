#include "shellpp_native_fs.h"
#include "shellpp_native_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_FILES 1u
#define PAGE_APPS 8u
#define EVENT_CLICKED 7u
#define MAX_OBJECTS 128u
#define MAX_ROWS 32u

struct test_object {
    uint8_t page;
    uint8_t hidden;
    void (*callback)(void *);
    void *user_data;
    char primary[128];
    char secondary[256];
};

struct test_event {
    void *user_data;
    uint32_t code;
};

struct memory_reader {
    const uint8_t *data;
    uint32_t length;
    uint32_t offset;
};

static struct test_object g_objects[MAX_OBJECTS];
static struct test_object *g_rows[9][MAX_ROWS];
static uint8_t g_row_count[9];
static uint32_t g_object_count;
static uint32_t g_last_navigation;
static uint32_t g_navigation_count;
static uint32_t g_visible_state_count;
static uint32_t g_hidden_state_count;
static uint32_t g_visible_open_count;
static uint32_t g_hidden_open_count;
static char g_titles[9][64];
enum registry_scenario {
    REGISTRY_VALID,
    REGISTRY_MISSING,
    REGISTRY_EMPTY,
    REGISTRY_INVALID,
};
static enum registry_scenario g_registry_scenario = REGISTRY_VALID;
static const uint8_t g_visible_registry[] =
    "{\"InstalledApps\":[{\"package\":\"com.example.demo\","
    "\"name\":\"Demo App\",\"locked\":false}]}";
static const uint8_t g_invalid_registry[] = "not-json";
static struct memory_reader g_reader;
static int g_failures;

#define CHECK(value) do { \
    if (!(value)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\\n", __FILE__, __LINE__, \
            #value); \
        ++g_failures; \
    } \
} while (0)

static struct test_object *new_object(uint8_t page) {
    struct test_object *object;
    if (g_object_count >= MAX_OBJECTS) return 0;
    object = &g_objects[g_object_count++];
    memset(object, 0, sizeof(*object));
    object->page = page;
    return object;
}

static void copy_text(char *target, size_t capacity, const char *source) {
    if (!capacity) return;
    (void)snprintf(target, capacity, "%s", source ? source : "");
}

void *shellpp_test_content_create(void *root) {
    struct test_object *parent = root;
    return new_object(parent ? parent->page : 0u);
}

void *shellpp_test_page_title_create(void *root, const char *title,
        uint32_t mode, const void *back_callback, void *context) {
    struct test_object *parent = root;
    struct test_object *object = new_object(parent ? parent->page : 0u);
    uint8_t page = (uint8_t)(((uintptr_t)context >> 8) & 0xffu);
    (void)mode;
    if (!object || page >= 9u) return 0;
    object->callback = (void (*)(void *))back_callback;
    object->user_data = context;
    copy_text(g_titles[page], sizeof(g_titles[page]), title);
    return object;
}

void *shellpp_test_label_create(void *parent) {
    struct test_object *object = parent;
    return new_object(object ? object->page : 0u);
}

void shellpp_test_label_set_text(void *label, const char *text) {
    struct test_object *object = label;
    if (object) copy_text(object->primary, sizeof(object->primary), text);
}

void *shellpp_test_layer_top(void *display) {
    (void)display;
    return new_object(0u);
}

void *shellpp_test_timer_create(void (*callback)(void *), uint32_t period_ms,
        void *user_data) {
    struct test_object *object = new_object(0u);
    (void)period_ms;
    if (object) {
        object->callback = callback;
        object->user_data = user_data;
    }
    return object;
}

void shellpp_test_timer_delete(void *timer) { (void)timer; }

void shellpp_test_object_set_size(void *object, int32_t width,
        int32_t height) {
    (void)object;
    (void)width;
    (void)height;
}

void shellpp_test_object_align(void *object, uint32_t alignment,
        int32_t x_offset, int32_t y_offset) {
    (void)object;
    (void)alignment;
    (void)x_offset;
    (void)y_offset;
}

void shellpp_test_align_to(void *object, void *base, uint32_t alignment,
        int32_t x_offset, int32_t y_offset) {
    (void)object;
    (void)base;
    (void)alignment;
    (void)x_offset;
    (void)y_offset;
}

void shellpp_test_set_hidden(void *object, uint32_t hidden) {
    struct test_object *value = object;
    if (value) value->hidden = hidden ? 1u : 0u;
}

int shellpp_test_style_apply(void *object, const void *style,
        uint8_t opacity, uint8_t reserved) {
    (void)object;
    (void)style;
    (void)opacity;
    (void)reserved;
    return 0;
}

void *shellpp_test_row_create(void *parent, const char *primary,
        const char *secondary, uint32_t trailing) {
    struct test_object *container = parent;
    struct test_object *row = new_object(container ? container->page : 0u);
    uint8_t page;
    (void)trailing;
    if (!row) return 0;
    page = row->page;
    copy_text(row->primary, sizeof(row->primary), primary);
    copy_text(row->secondary, sizeof(row->secondary), secondary);
    if (page < 9u && g_row_count[page] < MAX_ROWS)
        g_rows[page][g_row_count[page]++] = row;
    return row;
}

void shellpp_test_row_update(void *row, const void *icon,
        const char *primary, const char *secondary, uint32_t trailing,
        uint8_t selected) {
    struct test_object *object = row;
    (void)icon;
    (void)trailing;
    (void)selected;
    if (!object) return;
    copy_text(object->primary, sizeof(object->primary), primary);
    copy_text(object->secondary, sizeof(object->secondary), secondary);
}

void *shellpp_test_row_trailing(void *row) {
    (void)row;
    return 0;
}

void shellpp_test_event_add(void *object, void (*callback)(void *),
        uint32_t event_code, void *user_data) {
    struct test_object *row = object;
    (void)event_code;
    if (row) {
        row->callback = callback;
        row->user_data = user_data;
    }
}

void *shellpp_test_event_user_data(void *event) {
    return ((struct test_event *)event)->user_data;
}

uint32_t shellpp_test_event_code(void *event) {
    return ((struct test_event *)event)->code;
}

void shellpp_test_navigate(uint32_t key, uint32_t arg1, uint32_t arg2,
        uint32_t arg3) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    g_last_navigation = key;
    ++g_navigation_count;
}

void shellpp_test_finish(void *descriptor) { (void)descriptor; }

int shellpp_test_spawn(uint32_t *pid, const char *path, void *fa, void *attr,
        char *const *argv, char *const *envp) {
    (void)pid;
    (void)path;
    (void)fa;
    (void)attr;
    (void)argv;
    (void)envp;
    return 0;
}

int shellpp_test_restart_init(void *value) { (void)value; return 0; }

int shellpp_test_restart_addopen(void *fa, int fd, const char *path,
        int flags, uint32_t mode) {
    (void)fa;
    (void)fd;
    (void)path;
    (void)flags;
    (void)mode;
    return 0;
}

int shellpp_test_restart_destroy(void *value) { (void)value; return 0; }

int shellpp_test_waitpid(uint32_t pid, int *status, int options) {
    (void)pid;
    (void)status;
    (void)options;
    return 0;
}

void shellpp_test_soft_restart(void) { abort(); }

int shellpp_fs_path_type(const char *path, uint8_t *exists, uint8_t *type) {
    (void)exists;
    (void)type;
    if (path && strcmp(path, "/data/apps.json") == 0)
        ++g_visible_state_count;
    else if (path && strcmp(path, "/data/apps.json_hide") == 0)
        ++g_hidden_state_count;
    return SHELLPP_FS_ERR_ARGUMENT;
}

int shellpp_fs_reader_open(const char *path, struct shellpp_fs_reader *reader) {
    const uint8_t *data = 0;
    uint32_t length = 0u;
    if (!path || !reader) return SHELLPP_FS_ERR_ARGUMENT;
    if (strcmp(path, "/data/apps.json_hide") == 0) {
        ++g_hidden_open_count;
        return SHELLPP_FS_ERR_OPEN;
    }
    if (strcmp(path, "/data/apps.json") != 0)
        return SHELLPP_FS_ERR_OPEN;
    ++g_visible_open_count;
    if (g_registry_scenario == REGISTRY_MISSING)
        return SHELLPP_FS_ERR_OPEN;
    if (g_registry_scenario == REGISTRY_VALID) {
        data = g_visible_registry;
        length = (uint32_t)(sizeof(g_visible_registry) - 1u);
    } else if (g_registry_scenario == REGISTRY_INVALID) {
        data = g_invalid_registry;
        length = (uint32_t)(sizeof(g_invalid_registry) - 1u);
    } else {
        data = g_visible_registry;
    }
    g_reader.data = data;
    g_reader.length = length;
    g_reader.offset = 0u;
    reader->fd = 1;
    return SHELLPP_FS_OK;
}

int shellpp_fs_reader_read(struct shellpp_fs_reader *reader, uint8_t *buffer,
        uint32_t capacity, uint32_t *read_count) {
    uint32_t remaining;
    uint32_t amount;
    if (!reader || reader->fd != 1 || !buffer || !read_count)
        return SHELLPP_FS_ERR_ARGUMENT;
    remaining = g_reader.length - g_reader.offset;
    amount = remaining < capacity ? remaining : capacity;
    memcpy(buffer, g_reader.data + g_reader.offset, amount);
    g_reader.offset += amount;
    *read_count = amount;
    return SHELLPP_FS_OK;
}

void shellpp_fs_reader_close(struct shellpp_fs_reader *reader) {
    if (reader) reader->fd = -1;
}

int shellpp_fs_app_size(const char *package_name, uint32_t *size) {
    (void)package_name;
    if (!size) return SHELLPP_FS_ERR_ARGUMENT;
    *size = 4096u;
    return SHELLPP_FS_OK;
}

#define FS_STUB(name, signature) \
    int name signature { return SHELLPP_FS_ERR_ARGUMENT; }
FS_STUB(shellpp_fs_list_page, (const char *path,
    const struct shellpp_fs_cursor *after, struct shellpp_fs_page *page))
FS_STUB(shellpp_fs_previous_cursor, (const char *path,
    const struct shellpp_fs_cursor *before, struct shellpp_fs_cursor *after))
FS_STUB(shellpp_fs_file_size, (const char *path, uint32_t *size,
    uint8_t *saturated))
FS_STUB(shellpp_fs_read_at, (const char *path, uint32_t offset,
    uint8_t *buffer, uint32_t capacity, uint32_t *read_count))
FS_STUB(shellpp_fs_read_cpu, (char *text, uint32_t capacity,
    uint32_t *percent))
FS_STUB(shellpp_fs_read_memory, (char *text, uint32_t capacity,
    uint32_t *percent))
FS_STUB(shellpp_fs_atomic_begin, (const char *path,
    struct shellpp_fs_atomic_writer *writer))
FS_STUB(shellpp_fs_atomic_write, (struct shellpp_fs_atomic_writer *writer,
    const uint8_t *data, uint32_t length))
FS_STUB(shellpp_fs_atomic_commit, (const char *path,
    struct shellpp_fs_atomic_writer *writer))
FS_STUB(shellpp_fs_copy, (const char *source, const char *target,
    uint8_t *scratch, uint32_t scratch_size))
FS_STUB(shellpp_fs_move, (const char *source, const char *target,
    uint8_t *scratch, uint32_t scratch_size))
FS_STUB(shellpp_fs_delete_file, (const char *path))
FS_STUB(shellpp_fs_delete_app_package, (const char *package_name))
FS_STUB(shellpp_fs_cache_status, (uint8_t include_logs,
    struct shellpp_cache_report *report))
FS_STUB(shellpp_fs_cache_clear, (uint8_t include_logs,
    struct shellpp_cache_report *report))
#undef FS_STUB

void shellpp_fs_atomic_abort(const char *path,
        struct shellpp_fs_atomic_writer *writer) {
    (void)path;
    (void)writer;
}

const char *shellpp_fs_basename(const char *path) { return path; }

int shellpp_fs_join(const char *base, const char *name, char *output,
        uint32_t capacity) {
    (void)base;
    (void)name;
    (void)output;
    (void)capacity;
    return SHELLPP_FS_ERR_ARGUMENT;
}

int shellpp_fs_parent(const char *path, char *output, uint32_t capacity) {
    (void)path;
    (void)output;
    (void)capacity;
    return SHELLPP_FS_ERR_ARGUMENT;
}

static void click_row(uint8_t page, uint8_t slot) {
    struct test_object *row;
    struct test_event event;
    CHECK(page < 9u && slot < g_row_count[page]);
    if (page >= 9u || slot >= g_row_count[page]) return;
    row = g_rows[page][slot];
    CHECK(row && row->callback);
    if (!row || !row->callback) return;
    event.user_data = row->user_data;
    event.code = EVENT_CLICKED;
    row->callback(&event);
}

static uint8_t page_has_primary(uint8_t page, const char *text) {
    uint8_t index;
    for (index = 0u; page < 9u && index < g_row_count[page]; ++index)
        if (strcmp(g_rows[page][index]->primary, text) == 0) return 1u;
    return 0u;
}

static void reset_host_state(enum registry_scenario scenario) {
    memset(g_objects, 0, sizeof(g_objects));
    memset(g_rows, 0, sizeof(g_rows));
    memset(g_row_count, 0, sizeof(g_row_count));
    memset(g_titles, 0, sizeof(g_titles));
    memset(&g_reader, 0, sizeof(g_reader));
    g_object_count = 0u;
    g_visible_state_count = 0u;
    g_hidden_state_count = 0u;
    g_visible_open_count = 0u;
    g_hidden_open_count = 0u;
    g_registry_scenario = scenario;
    shellpp_ui_reset();
}

static void check_registry_case(enum registry_scenario scenario,
        const char *expected_row) {
    struct test_object apps_root;
    uint32_t apps_descriptor = 8u;
    memset(&apps_root, 0, sizeof(apps_root));
    apps_root.page = PAGE_APPS;
    reset_host_state(scenario);
    CHECK(shellpp_ui_page_create(PAGE_APPS, &apps_descriptor,
        &apps_root) == 0);
    CHECK(strcmp(g_titles[PAGE_APPS], "应用管理") == 0);
    CHECK(page_has_primary(PAGE_APPS, expected_row));
    CHECK(!page_has_primary(PAGE_APPS, "读取失败"));
    CHECK(g_visible_state_count == 0u);
    CHECK(g_hidden_state_count == 0u);
    CHECK(g_visible_open_count == 1u);
    CHECK(g_hidden_open_count == 1u);
    CHECK(shellpp_ui_page_destroy(PAGE_APPS) == 0);
}

int main(void) {
    struct test_object files_root;
    uint32_t files_descriptor = 1u;
    memset(&files_root, 0, sizeof(files_root));
    files_root.page = PAGE_FILES;

    reset_host_state(REGISTRY_VALID);
    CHECK(shellpp_ui_page_create(PAGE_FILES, &files_descriptor,
        &files_root) == 0);
    CHECK(strcmp(g_titles[PAGE_FILES], "文件与应用管理") == 0);
    CHECK(g_row_count[PAGE_FILES] == 3u);
    CHECK(strcmp(g_rows[PAGE_FILES][1]->primary, "应用管理") == 0);

    click_row(PAGE_FILES, 1u);
    CHECK(g_navigation_count == 1u);
    CHECK(g_last_navigation == ((0x00cdu << 16) | PAGE_APPS));

    CHECK(shellpp_ui_page_destroy(PAGE_FILES) == 0);

    check_registry_case(REGISTRY_VALID, "Demo App");
    check_registry_case(REGISTRY_MISSING, "暂无应用");
    check_registry_case(REGISTRY_EMPTY, "暂无应用");
    check_registry_case(REGISTRY_INVALID, "暂无应用");

    if (g_failures) {
        fprintf(stderr, "%d UI regression test(s) failed\\n", g_failures);
        return 1;
    }
    puts("043 native UI navigation and registry regressions passed");
    return 0;
}
