#include "fs.h"
#include "libk.h"

static fs_node_t nodes[FS_MAX_NODES];

static int find_child(int parent, const char *name) {
    for (int index = 0; index < FS_MAX_NODES; index++)
        if (nodes[index].used && nodes[index].parent == parent && kstrcmp(nodes[index].name, name) == 0) return index;
    return -1;
}

static int resolve_component_path(const char *path, int current) {
    int node = path[0] == '/' ? 0 : current;
    const char *cursor = path;
    char part[FS_NAME_MAX + 1];
    if (*cursor == '/') cursor++;
    while (*cursor) {
        uint16_t length = 0;
        while (*cursor && *cursor != '/') {
            if (length >= FS_NAME_MAX) return -1;
            part[length++] = *cursor++;
        }
        part[length] = '\0';
        while (*cursor == '/') cursor++;
        if (length == 0 || kstrcmp(part, ".") == 0) continue;
        if (kstrcmp(part, "..") == 0) { if (nodes[node].parent >= 0) node = nodes[node].parent; continue; }
        node = find_child(node, part);
        if (node < 0) return -1;
        if (*cursor && nodes[node].type != FS_DIR) return -1;
    }
    return node;
}

void fs_init(void) {
    kmemset(nodes, 0, sizeof(nodes));
    nodes[0].used = 1; nodes[0].type = FS_DIR; nodes[0].parent = -1;
    (void)fs_create("/games", FS_DIR, 0);
    (void)fs_write("/readme.txt", "MiniOS RAM filesystem. Files disappear after reboot.", 0);
}

int fs_root(void) { return 0; }
int fs_resolve(const char *path, int current) { return path && *path ? resolve_component_path(path, current) : current; }

static fs_result_t split_parent(const char *path, int current, int *parent, char *name) {
    char parent_path[80];
    uint16_t last_slash = 0xFFFF, length = 0;
    if (!path || !*path) return FS_INVALID;
    while (path[length]) { if (path[length] == '/') last_slash = length; length++; }
    if (last_slash == 0xFFFF) { *parent = current; kstrcpy(name, path, FS_NAME_MAX + 1); }
    else {
        uint16_t start = last_slash + 1;
        if (!path[start]) return FS_INVALID;
        if (last_slash == 0) { parent_path[0] = '/'; parent_path[1] = '\0'; }
        else { kmemcpy(parent_path, path, last_slash); parent_path[last_slash] = '\0'; }
        *parent = fs_resolve(parent_path, current);
        kstrcpy(name, path + start, FS_NAME_MAX + 1);
    }
    if (kstrlen(last_slash == 0xFFFF ? path : path + last_slash + 1) > FS_NAME_MAX) return FS_INVALID;
    if (*parent < 0 || !*name || kstrcmp(name, ".") == 0 || kstrcmp(name, "..") == 0) return FS_INVALID;
    if (nodes[*parent].type != FS_DIR) return FS_NOT_DIR;
    return FS_OK;
}

fs_result_t fs_create(const char *path, fs_type_t type, int current) {
    int parent; char name[FS_NAME_MAX + 1];
    fs_result_t result = split_parent(path, current, &parent, name);
    if (result != FS_OK) return result;
    if (find_child(parent, name) >= 0) return FS_EXISTS;
    for (int index = 1; index < FS_MAX_NODES; index++) if (!nodes[index].used) {
        nodes[index].used = 1; nodes[index].type = type; nodes[index].parent = (int8_t)parent; nodes[index].size = 0;
        kstrcpy(nodes[index].name, name, sizeof(nodes[index].name)); nodes[index].data[0] = '\0';
        return FS_OK;
    }
    return FS_FULL;
}

fs_result_t fs_write(const char *path, const char *text, int current) {
    int index = fs_resolve(path, current);
    if (index < 0) { fs_result_t created = fs_create(path, FS_FILE, current); if (created != FS_OK) return created; index = fs_resolve(path, current); }
    if (nodes[index].type != FS_FILE) return FS_INVALID;
    size_t length = kstrlen(text);
    if (length > FS_FILE_MAX) return FS_TOO_LARGE;
    kstrcpy(nodes[index].data, text, sizeof(nodes[index].data)); nodes[index].size = (uint16_t)length;
    return FS_OK;
}

fs_result_t fs_remove(const char *path, int current, uint8_t directory) {
    int index = fs_resolve(path, current);
    if (index <= 0) return FS_NOT_FOUND;
    if ((nodes[index].type == FS_DIR) != directory) return FS_INVALID;
    if (directory) for (int child = 0; child < FS_MAX_NODES; child++) if (nodes[child].used && nodes[child].parent == index) return FS_NOT_EMPTY;
    nodes[index].used = 0;
    return FS_OK;
}

const fs_node_t *fs_node(int index) { return index >= 0 && index < FS_MAX_NODES && nodes[index].used ? &nodes[index] : 0; }
uint8_t fs_used_count(void) { uint8_t count = 0; for (int i=0;i<FS_MAX_NODES;i++) if(nodes[i].used) count++; return count; }

void fs_path(int index, char *buffer, uint16_t capacity) {
    int chain[16]; uint16_t count = 0;
    if (index == 0) { kstrcpy(buffer, "/", capacity); return; }
    while (index > 0 && count < 16) { chain[count++] = index; index = nodes[index].parent; }
    uint16_t out = 0; buffer[out++] = '/';
    while (count) {
        const char *name = nodes[chain[--count]].name;
        for (uint16_t i=0;name[i] && out + 1 < capacity;i++) buffer[out++] = name[i];
        if (count && out + 1 < capacity) buffer[out++] = '/';
    }
    buffer[out] = '\0';
}
