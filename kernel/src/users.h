#ifndef USERS_H
#define USERS_H

#include <stdint.h>

void users_init(void);
uint8_t users_is_root(void);
const char *users_current_name(void);
const char *users_current_role(void);
const char *users_current_home(void);
uint8_t users_current_uid(void);
int users_find_uid(const char *name);
void users_list(void);
void users_add(const char *name, const char *role);
uint8_t users_login(const char *name);

#endif
