#ifndef USERS_H
#define USERS_H

#include <stdint.h>

#define USER_NAME_MAX 16

typedef enum {
    USERS_RESULT_OK,
    USERS_RESULT_ONLY_ROOT,
    USERS_RESULT_LIMIT,
    USERS_RESULT_INVALID,
    USERS_RESULT_BAD_ROLE
} users_result_t;

typedef struct {
    char name[USER_NAME_MAX + 1];
    uint8_t root;
    uint8_t current;
} user_info_t;

void users_init(void);
uint8_t users_is_root(void);
const char *users_current_name(void);
const char *users_current_role(void);
const char *users_current_home(void);
uint8_t users_current_uid(void);
int users_find_uid(const char *name);
uint8_t users_snapshot(user_info_t *out, uint8_t maximum);
users_result_t users_add_result(const char *name, const char *role);
const char *users_result_text(users_result_t result);
void users_list(void);
void users_add(const char *name, const char *role);
uint8_t users_login_result(const char *name);
uint8_t users_login(const char *name);

#endif
