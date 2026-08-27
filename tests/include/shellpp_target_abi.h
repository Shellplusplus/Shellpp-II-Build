#ifndef SHELLPP_TARGET_ABI_H
#define SHELLPP_TARGET_ABI_H

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define SHELLPP_TARGET_ABI_GENERATED 1
#define SHELLPP_TARGET_FIRMWARE_VERSION "3.101.043"

int32_t shellpp_test_open(const char *path, int32_t flags, ...);
int32_t shellpp_test_read(int32_t fd, void *buffer, uint32_t length);
int32_t shellpp_test_write(int32_t fd, const void *buffer, uint32_t length);
int32_t shellpp_test_close(int32_t fd);
int64_t shellpp_test_lseek(int32_t fd, int64_t offset, int32_t whence);
int32_t shellpp_test_unlink(const char *path);
int32_t shellpp_test_rename(const char *source, const char *target);
void *shellpp_test_opendir(const char *path);
int32_t shellpp_test_closedir(void *directory);
uint8_t *shellpp_test_readdir(void *directory);
int32_t shellpp_test_rmdir(const char *path);

#define SHELLPP_ABI_OPEN_ADDR shellpp_test_open
#define SHELLPP_ABI_READ_ADDR shellpp_test_read
#define SHELLPP_ABI_WRITE_ADDR shellpp_test_write
#define SHELLPP_ABI_CLOSE_ADDR shellpp_test_close
#define SHELLPP_ABI_LSEEK_ADDR shellpp_test_lseek
#define SHELLPP_ABI_UNLINK_ADDR shellpp_test_unlink
#define SHELLPP_ABI_RENAME_ADDR shellpp_test_rename
#define SHELLPP_ABI_OPENDIR_ADDR shellpp_test_opendir
#define SHELLPP_ABI_CLOSEDIR_ADDR shellpp_test_closedir
#define SHELLPP_ABI_READDIR_ADDR shellpp_test_readdir
#define SHELLPP_ABI_RMDIR_ADDR shellpp_test_rmdir

#define SHELLPP_ABI_O_RDONLY O_RDONLY
#define SHELLPP_ABI_O_WRONLY O_WRONLY
#define SHELLPP_ABI_O_CREAT O_CREAT
#define SHELLPP_ABI_O_TRUNC O_TRUNC
#define SHELLPP_ABI_SEEK_SET SEEK_SET
#define SHELLPP_ABI_SEEK_END SEEK_END
#define SHELLPP_ABI_DT_DIR 4u
#define SHELLPP_ABI_DT_REG 8u
#define SHELLPP_ABI_DT_LNK 10u

void *shellpp_test_content_create(void *root);
void *shellpp_test_page_title_create(void *root, const char *title,
    uint32_t mode, const void *back_callback, void *context);
void *shellpp_test_label_create(void *parent);
void shellpp_test_label_set_text(void *label, const char *text);
void *shellpp_test_layer_top(void *display);
void *shellpp_test_timer_create(void (*callback)(void *), uint32_t period_ms,
    void *user_data);
void shellpp_test_timer_delete(void *timer);
void shellpp_test_object_set_size(void *object, int32_t width, int32_t height);
void shellpp_test_object_align(void *object, uint32_t alignment,
    int32_t x_offset, int32_t y_offset);
void shellpp_test_align_to(void *object, void *base, uint32_t alignment,
    int32_t x_offset, int32_t y_offset);
void shellpp_test_set_hidden(void *object, uint32_t hidden);
int shellpp_test_style_apply(void *object, const void *style,
    uint8_t opacity, uint8_t reserved);
void *shellpp_test_row_create(void *parent, const char *primary,
    const char *secondary, uint32_t trailing);
void shellpp_test_row_update(void *row, const void *icon,
    const char *primary, const char *secondary, uint32_t trailing,
    uint8_t selected);
void *shellpp_test_row_trailing(void *row);
void shellpp_test_event_add(void *object, void (*callback)(void *),
    uint32_t event_code, void *user_data);
void *shellpp_test_event_user_data(void *event);
uint32_t shellpp_test_event_code(void *event);
void shellpp_test_navigate(uint32_t key, uint32_t arg1, uint32_t arg2,
    uint32_t arg3);
void shellpp_test_finish(void *descriptor);
int shellpp_test_spawn(uint32_t *pid, const char *path, void *fa, void *attr,
    char *const *argv, char *const *envp);
int shellpp_test_restart_init(void *value);
int shellpp_test_restart_addopen(void *fa, int fd, const char *path,
    int flags, uint32_t mode);
int shellpp_test_restart_destroy(void *value);
int shellpp_test_waitpid(uint32_t pid, int *status, int options);
void shellpp_test_soft_restart(void);

#define SHELLPP_ABI_LVX_CONTENT_CREATE_ADDR shellpp_test_content_create
#define SHELLPP_ABI_LVX_PAGE_TITLE_CREATE_ADDR shellpp_test_page_title_create
#define SHELLPP_ABI_LVX_LABEL_CREATE_ADDR shellpp_test_label_create
#define SHELLPP_ABI_LVX_LABEL_SET_TEXT_ADDR shellpp_test_label_set_text
#define SHELLPP_ABI_LV_DISPLAY_GET_LAYER_TOP_ADDR shellpp_test_layer_top
#define SHELLPP_ABI_LV_TIMER_CREATE_ADDR shellpp_test_timer_create
#define SHELLPP_ABI_LV_TIMER_DELETE_ADDR shellpp_test_timer_delete
#define SHELLPP_ABI_LVX_OBJECT_SET_SIZE_ADDR shellpp_test_object_set_size
#define SHELLPP_ABI_LVX_OBJECT_ALIGN_ADDR shellpp_test_object_align
#define SHELLPP_ABI_LVX_ALIGN_TO_ADDR shellpp_test_align_to
#define SHELLPP_ABI_LVX_SET_HIDDEN_ADDR shellpp_test_set_hidden
#define SHELLPP_ABI_LVX_STYLE_APPLY_ADDR shellpp_test_style_apply
#define SHELLPP_ABI_LVX_LIST_ROW_CREATE_ADDR shellpp_test_row_create
#define SHELLPP_ABI_LVX_LIST_ROW_UPDATE_ADDR shellpp_test_row_update
#define SHELLPP_ABI_LVX_LIST_ROW_TRAILING_ADDR shellpp_test_row_trailing
#define SHELLPP_ABI_LVX_EVENT_ADD_ADDR shellpp_test_event_add
#define SHELLPP_ABI_LVX_EVENT_GET_USER_DATA_ADDR shellpp_test_event_user_data
#define SHELLPP_ABI_LVX_EVENT_GET_CODE_ADDR shellpp_test_event_code
#define SHELLPP_ABI_ACTIVITY_NAVIGATE_ADDR shellpp_test_navigate
#define SHELLPP_ABI_ACTIVITY_FINISH_ADDR shellpp_test_finish
#define SHELLPP_ABI_POSIX_SPAWN_ADDR shellpp_test_spawn
#define SHELLPP_ABI_FILE_ACTIONS_INIT_ADDR shellpp_test_restart_init
#define SHELLPP_ABI_FILE_ACTIONS_ADDOPEN_ADDR shellpp_test_restart_addopen
#define SHELLPP_ABI_FILE_ACTIONS_DESTROY_ADDR shellpp_test_restart_destroy
#define SHELLPP_ABI_SPAWNATTR_INIT_ADDR shellpp_test_restart_init
#define SHELLPP_ABI_SPAWNATTR_DESTROY_ADDR shellpp_test_restart_destroy
#define SHELLPP_ABI_WAITPID_ADDR shellpp_test_waitpid
#define SHELLPP_ABI_SOFT_RESTART_ADDR shellpp_test_soft_restart
#define SHELLPP_ABI_STYLE_MISANS_DEMIBOLD_32_ADDR 0u
#define SHELLPP_ABI_ALIGN_TOP_MID 2u
#define SHELLPP_ABI_ALIGN_TOP_LEFT 1u
#define SHELLPP_ABI_ALIGN_OUT_BOTTOM_MID 13u
#define SHELLPP_ABI_EVENT_CLICKED 7u
#define SHELLPP_ABI_TRAILING_NONE 0u

#endif
