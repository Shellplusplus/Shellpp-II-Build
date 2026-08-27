#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700

#include "shellpp_native_fs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct test_directory {
    DIR *stream;
    char path[PATH_MAX];
    uint8_t entry[257];
};

static char g_root[PATH_MAX];
static int g_failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++g_failures; \
    } \
} while (0)

#define CHECK_TEXT(actual, expected) do { \
    if (strcmp((actual), (expected)) != 0) { \
        fprintf(stderr, "FAIL %s:%d: expected %s, got %s\n", __FILE__, \
            __LINE__, (expected), (actual)); \
        ++g_failures; \
    } \
} while (0)

static int map_path(const char *path, char *output, size_t capacity) {
    int count;
    if (!path || path[0] != '/') return -1;
    count = snprintf(output, capacity, "%s%s", g_root, path);
    return count < 0 || (size_t)count >= capacity ? -1 : 0;
}

static int make_directory(const char *path) {
    char mapped[PATH_MAX];
    size_t root_length;
    char *cursor;
    if (map_path(path, mapped, sizeof(mapped)) < 0) return -1;
    root_length = strlen(g_root);
    for (cursor = mapped + root_length + 1u; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(mapped, 0777) < 0 && errno != EEXIST) return -1;
        *cursor = '/';
    }
    return mkdir(mapped, 0777) < 0 && errno != EEXIST ? -1 : 0;
}

static int make_parent(const char *path) {
    char parent[SHELLPP_FS_PATH_CAP];
    char *slash;
    size_t length = strlen(path);
    if (length >= sizeof(parent)) return -1;
    memcpy(parent, path, length + 1u);
    slash = strrchr(parent, '/');
    if (!slash || slash == parent) return 0;
    *slash = '\0';
    return make_directory(parent);
}

static int write_file_size(const char *path, uint32_t size) {
    char mapped[PATH_MAX];
    uint8_t bytes[128];
    uint32_t remaining = size;
    int fd;
    memset(bytes, 'x', sizeof(bytes));
    if (make_parent(path) < 0 || map_path(path, mapped, sizeof(mapped)) < 0)
        return -1;
    fd = open(mapped, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return -1;
    while (remaining) {
        uint32_t amount = remaining > sizeof(bytes) ? sizeof(bytes) : remaining;
        if (write(fd, bytes, amount) != (ssize_t)amount) {
            (void)close(fd);
            return -1;
        }
        remaining -= amount;
    }
    return close(fd);
}

static int write_text(const char *path, const char *value) {
    char mapped[PATH_MAX];
    size_t length = strlen(value);
    int fd;
    if (make_parent(path) < 0 || map_path(path, mapped, sizeof(mapped)) < 0)
        return -1;
    fd = open(mapped, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return -1;
    if (write(fd, value, length) != (ssize_t)length) {
        (void)close(fd);
        return -1;
    }
    return close(fd);
}

static int virtual_exists(const char *path) {
    char mapped[PATH_MAX];
    struct stat status;
    return map_path(path, mapped, sizeof(mapped)) == 0 &&
        lstat(mapped, &status) == 0;
}

static int make_link(const char *target, const char *link_path) {
    char mapped_target[PATH_MAX];
    char mapped_link[PATH_MAX];
    if (make_parent(link_path) < 0 ||
            map_path(target, mapped_target, sizeof(mapped_target)) < 0 ||
            map_path(link_path, mapped_link, sizeof(mapped_link)) < 0)
        return -1;
    return symlink(mapped_target, mapped_link);
}

static int make_fifo(const char *path) {
    char mapped[PATH_MAX];
    if (make_parent(path) < 0 || map_path(path, mapped, sizeof(mapped)) < 0)
        return -1;
    return mkfifo(mapped, 0666);
}

int32_t shellpp_test_open(const char *path, int32_t flags, ...) {
    char mapped[PATH_MAX];
    va_list arguments;
    unsigned int mode;
    if (map_path(path, mapped, sizeof(mapped)) < 0) return -1;
    va_start(arguments, flags);
    mode = va_arg(arguments, unsigned int);
    va_end(arguments);
    return open(mapped, flags, mode);
}

int32_t shellpp_test_read(int32_t fd, void *buffer, uint32_t length) {
    ssize_t result = read(fd, buffer, length);
    return result > INT32_MAX ? -1 : (int32_t)result;
}

int32_t shellpp_test_write(int32_t fd, const void *buffer, uint32_t length) {
    ssize_t result = write(fd, buffer, length);
    return result > INT32_MAX ? -1 : (int32_t)result;
}

int32_t shellpp_test_close(int32_t fd) {
    return close(fd);
}

int64_t shellpp_test_lseek(int32_t fd, int64_t offset, int32_t whence) {
    return (int64_t)lseek(fd, (off_t)offset, whence);
}

int32_t shellpp_test_unlink(const char *path) {
    char mapped[PATH_MAX];
    return map_path(path, mapped, sizeof(mapped)) < 0 ? -1 : unlink(mapped);
}

int32_t shellpp_test_rename(const char *source, const char *target) {
    char mapped_source[PATH_MAX];
    char mapped_target[PATH_MAX];
    if (map_path(source, mapped_source, sizeof(mapped_source)) < 0 ||
            map_path(target, mapped_target, sizeof(mapped_target)) < 0)
        return -1;
    return rename(mapped_source, mapped_target);
}

void *shellpp_test_opendir(const char *path) {
    char mapped[PATH_MAX];
    struct test_directory *directory;
    int count;
    if (map_path(path, mapped, sizeof(mapped)) < 0) return 0;
    directory = calloc(1u, sizeof(*directory));
    if (!directory) return 0;
    directory->stream = opendir(mapped);
    if (!directory->stream) {
        free(directory);
        return 0;
    }
    count = snprintf(directory->path, sizeof(directory->path), "%s", mapped);
    if (count < 0 || (size_t)count >= sizeof(directory->path)) {
        (void)closedir(directory->stream);
        free(directory);
        return 0;
    }
    return directory;
}

static uint8_t entry_type(const struct test_directory *directory,
        const struct dirent *entry) {
    char path[PATH_MAX];
    struct stat status;
    int count = snprintf(path, sizeof(path), "%s/%s", directory->path,
        entry->d_name);
    if (count < 0 || (size_t)count >= sizeof(path) || lstat(path, &status) < 0)
        return 0u;
    if (S_ISDIR(status.st_mode)) return 4u;
    if (S_ISREG(status.st_mode)) return 8u;
    if (S_ISLNK(status.st_mode)) return 10u;
    return 1u;
}

uint8_t *shellpp_test_readdir(void *opaque) {
    struct test_directory *directory = opaque;
    struct dirent *entry;
    size_t length;
    if (!directory) return 0;
    entry = readdir(directory->stream);
    if (!entry) return 0;
    length = strlen(entry->d_name);
    if (length >= sizeof(directory->entry) - 1u)
        length = sizeof(directory->entry) - 2u;
    directory->entry[0] = entry_type(directory, entry);
    memcpy(directory->entry + 1u, entry->d_name, length);
    directory->entry[1u + length] = 0u;
    return directory->entry;
}

int32_t shellpp_test_closedir(void *opaque) {
    struct test_directory *directory = opaque;
    int result;
    if (!directory) return -1;
    result = closedir(directory->stream);
    free(directory);
    return result;
}

int32_t shellpp_test_rmdir(const char *path) {
    char mapped[PATH_MAX];
    return map_path(path, mapped, sizeof(mapped)) < 0 ? -1 : rmdir(mapped);
}

static void test_paths(void) {
    char output[SHELLPP_FS_PATH_CAP];
    CHECK(shellpp_fs_validate_path("/") == SHELLPP_FS_OK);
    CHECK(shellpp_fs_validate_path("/data/app") == SHELLPP_FS_OK);
    CHECK(shellpp_fs_validate_path("data/app") == SHELLPP_FS_ERR_PATH);
    CHECK(shellpp_fs_validate_path("/data/../app") == SHELLPP_FS_ERR_PATH);
    CHECK(shellpp_fs_validate_path("/data//app") == SHELLPP_FS_ERR_PATH);
    CHECK(shellpp_fs_parent("/data/app/pkg", output, sizeof(output)) ==
        SHELLPP_FS_OK);
    CHECK_TEXT(output, "/data/app");
    CHECK(shellpp_fs_join("/data/app", "pkg", output, sizeof(output)) ==
        SHELLPP_FS_OK);
    CHECK_TEXT(output, "/data/app/pkg");
    CHECK(shellpp_fs_join("/data/app", "../pkg", output, sizeof(output)) ==
        SHELLPP_FS_ERR_PATH);
}

static void test_directory_pagination(void) {
    struct shellpp_fs_page page;
    struct shellpp_fs_cursor cursor;
    char path[64];
    CHECK(make_directory("/page/a_dir") == 0);
    CHECK(make_directory("/page/b_dir") == 0);
    for (unsigned int index = 0u; index < 30u; ++index) {
        (void)snprintf(path, sizeof(path), "/page/file%02u", index);
        CHECK(write_file_size(path, index + 1u) == 0);
    }
    CHECK(shellpp_fs_list_page("/page", 0, &page) == SHELLPP_FS_OK);
    CHECK(page.count == 30u);
    CHECK(page.has_next == 1u);
    CHECK_TEXT(page.entries[0].name, "a_dir");
    CHECK_TEXT(page.entries[1].name, "b_dir");
    CHECK_TEXT(page.entries[2].name, "file00");
    CHECK_TEXT(page.entries[29].name, "file27");
    CHECK(page.entries[2].size_known == 1u && page.entries[2].size == 1u);
    cursor = page.last;
    CHECK(shellpp_fs_list_page("/page", &cursor, &page) == SHELLPP_FS_OK);
    CHECK(page.count == 2u && page.has_next == 0u);
    CHECK_TEXT(page.entries[0].name, "file28");
    CHECK_TEXT(page.entries[1].name, "file29");
}

static void test_app_size_and_delete(void) {
    static const char *const size_roots[] = {
        "/data/app/pkg", "/data/quickapp/system/pkg",
        "/data/cache/pkg", "/data/files/pkg", "/data/mass/pkg"
    };
    static const char *const delete_roots[] = {
        "/data/app/remove", "/data/quickapp/system/remove",
        "/data/cache/remove", "/data/files/remove",
        "/data/mass/remove"
    };
    char path[SHELLPP_FS_PATH_CAP];
    uint32_t expected = 0u;
    uint32_t size = 0u;
    CHECK(write_file_size("/protected.txt", 17u) == 0);
    for (unsigned int root = 0u; root < 5u; ++root) {
        CHECK(make_directory(size_roots[root]) == 0);
        (void)snprintf(path, sizeof(path), "%s/root.bin", size_roots[root]);
        CHECK(write_file_size(path, 10u + root) == 0);
        expected += 10u + root;
    }
    (void)snprintf(path, sizeof(path), "%s", size_roots[0]);
    for (unsigned int depth = 0u; depth < 12u; ++depth) {
        size_t length = strlen(path);
        CHECK(length + 5u < sizeof(path));
        (void)snprintf(path + length, sizeof(path) - length, "/d%u", depth);
        CHECK(make_directory(path) == 0);
    }
    {
        size_t length = strlen(path);
        (void)snprintf(path + length, sizeof(path) - length, "/deep.bin");
    }
    CHECK(write_file_size(path, 23u) == 0);
    expected += 23u;
    CHECK(make_link("/protected.txt",
        "/data/app/pkg/protected-link") == 0);
    CHECK(shellpp_fs_app_size("pkg", &size) == SHELLPP_FS_OK);
    CHECK(size == expected);

    for (unsigned int index = 0u; index < 5u; ++index) {
        (void)snprintf(path, sizeof(path), "%s/nested/file.bin",
            delete_roots[index]);
        CHECK(write_file_size(path, index + 2u) == 0);
    }
    CHECK(make_link("/protected.txt",
        "/data/app/remove/nested/protected-link") == 0);
    CHECK(shellpp_fs_delete_app_package("remove") == SHELLPP_FS_OK);
    for (unsigned int index = 0u; index < 5u; ++index)
        CHECK(!virtual_exists(delete_roots[index]));
    CHECK(virtual_exists("/protected.txt"));
}

static void test_remove_tree_safety(void) {
    int result;
    CHECK(write_file_size("/tree/a/b/file.bin", 9u) == 0);
    CHECK(make_link("/protected.txt", "/tree/a/protected-link") == 0);
    CHECK(shellpp_fs_remove_tree("/tree") == SHELLPP_FS_OK);
    CHECK(!virtual_exists("/tree"));
    CHECK(virtual_exists("/protected.txt"));

    CHECK(make_directory("/unsafe") == 0);
    CHECK(make_fifo("/unsafe/device") == 0);
    result = shellpp_fs_remove_tree("/unsafe");
    CHECK(result == SHELLPP_FS_ERR_UNSAFE_TYPE);
    CHECK(virtual_exists("/unsafe/device"));
}

static void test_cache(void) {
    struct shellpp_cache_report report;
    CHECK(write_file_size("/data/shellpp-ii/cache/nested/a.bin", 10u) == 0);
    CHECK(write_file_size("/data/shellpp-ii/tmp/b.bin", 20u) == 0);
    CHECK(write_file_size("/data/log/c.bin", 5u) == 0);
    CHECK(write_file_size("/data/offlinelog/d.bin", 3u) == 0);
    CHECK(write_file_size("/data/shellpp-ii/logs/own.bin", 7u) == 0);
    CHECK(make_link("/protected.txt",
        "/data/shellpp-ii/cache/protected-link") == 0);
    CHECK(make_fifo("/data/shellpp-ii/cache/device") == 0);

    CHECK(shellpp_fs_cache_status(0u, &report) == SHELLPP_FS_OK);
    CHECK(report.root_count == 4u);
    CHECK(report.before_bytes == 38u);
    CHECK(report.roots[0].skipped == 2u);
    CHECK(shellpp_fs_cache_clear(0u, &report) == SHELLPP_FS_OK);
    CHECK(report.before_bytes == 38u);
    CHECK(report.after_bytes == 0u);
    CHECK(report.freed_bytes == 38u);
    CHECK(report.roots[0].deleted >= 2u);
    CHECK(virtual_exists("/protected.txt"));
    CHECK(virtual_exists("/data/shellpp-ii/cache/protected-link"));
    CHECK(virtual_exists("/data/shellpp-ii/cache/device"));
    CHECK(virtual_exists("/data/shellpp-ii/logs/own.bin"));
}

static void test_cpu(void) {
    char text[48];
    uint32_t percent;
    char oversized[128];
    CHECK(write_text("/proc/cpuload", "100.000 25.000\n") == 0);
    CHECK(shellpp_fs_read_cpu(text, sizeof(text), &percent) == SHELLPP_FS_OK);
    CHECK(percent == 25u);
    CHECK_TEXT(text, "CPU:25%");
    memset(oversized, '1', sizeof(oversized) - 1u);
    oversized[sizeof(oversized) - 1u] = '\0';
    CHECK(write_text("/proc/cpuload", oversized) == 0);
    CHECK(shellpp_fs_read_cpu(text, sizeof(text), &percent) ==
        SHELLPP_FS_ERR_TRUNCATED);
    CHECK_TEXT(text, "ERR:truncated");
}

static void test_memory(void) {
    char text[48];
    uint32_t percent;
    CHECK(write_text("/proc/meminfo",
        "1048576 262144 786432 0 Umem\n") == 0);
    CHECK(shellpp_fs_read_memory(text, sizeof(text), &percent) == SHELLPP_FS_OK);
    CHECK(percent == 25u);
    CHECK_TEXT(text, "256KB/1.0MB 25%");

    CHECK(write_text("/proc/meminfo",
        "MemTotal: 1024 kB\nMemFree: 128 kB\n"
        "MemAvailable: 256 kB\nBuffers: 64 kB\nCached: 64 kB\n") == 0);
    CHECK(shellpp_fs_read_memory(text, sizeof(text), &percent) == SHELLPP_FS_OK);
    CHECK(percent == 75u);
    CHECK_TEXT(text, "768KB/1.0MB 75%");
}

int main(void) {
    char root_template[] = "/tmp/shellpp-native-fs.XXXXXX";
    char *root = mkdtemp(root_template);
    int count;
    if (!root) {
        fprintf(stderr, "cannot create test root\n");
        return 2;
    }
    count = snprintf(g_root, sizeof(g_root), "%s", root);
    if (count < 0 || (size_t)count >= sizeof(g_root)) {
        fprintf(stderr, "test root is too long\n");
        return 2;
    }
    test_paths();
    test_directory_pagination();
    test_app_size_and_delete();
    test_remove_tree_safety();
    test_cache();
    test_cpu();
    test_memory();
    if (g_failures) {
        fprintf(stderr, "%d host regression test(s) failed; root=%s\n",
            g_failures, g_root);
        return 1;
    }
    printf("043 native filesystem host regressions passed; root=%s\n", g_root);
    return 0;
}
