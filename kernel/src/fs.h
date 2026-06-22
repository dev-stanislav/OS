#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAX_NODES 32
#define FS_NAME_MAX 24
#define FS_FILE_MAX 1024
#define FS_CAPACITY_BYTES (FS_MAX_NODES * FS_FILE_MAX)

typedef enum { FS_FILE, FS_DIR } fs_type_t;
typedef struct {
    uint8_t used;
    fs_type_t type;
    uint8_t owner;
    uint16_t mode;
    int8_t parent;
    char name[FS_NAME_MAX + 1];
    uint16_t size;
    char data[FS_FILE_MAX + 1];
} fs_node_t;

typedef enum { FS_OK, FS_NOT_FOUND, FS_EXISTS, FS_FULL, FS_INVALID, FS_NOT_DIR, FS_NOT_EMPTY, FS_TOO_LARGE } fs_result_t;

void fs_init(void);
int fs_root(void);
int fs_resolve(const char *path, int current);
fs_result_t fs_create(const char *path, fs_type_t type, int current);
fs_result_t fs_remove(const char *path, int current, uint8_t directory);
fs_result_t fs_write(const char *path, const char *text, int current);
fs_result_t fs_append(const char *path, const char *text, int current);
fs_result_t fs_copy(const char *source, const char *destination, int current);
fs_result_t fs_move(const char *source, const char *destination, int current);
const fs_node_t *fs_node(int index);
uint8_t fs_used_count(void);
uint32_t fs_used_bytes(void);
uint32_t fs_capacity_bytes(void);
void fs_path(int index, char *buffer, uint16_t capacity);
void fs_set_actor(uint8_t uid, uint8_t root);
uint8_t fs_can_read(int index);
uint8_t fs_can_write(int index);
void fs_chmod(const char *path, uint16_t mode, int current);
void fs_chown(const char *path, uint8_t owner, int current);

#endif
