#ifndef USERS_H
#define USERS_H

#include <stdbool.h>
#include <stdint.h>
#include "secure.h"

#define MAX_USERS    32
#define USERNAME_LEN 32

typedef struct
{
    char    name[USERNAME_LEN];
    uint8_t key[SEC_KEY_LEN];
} UserRecord;

bool users_name_valid(const char *name);

void        users_resolve_path(void);
const char *users_path(void);

int               users_load(void);
int               users_count(void);
const UserRecord *users_find(const char *name);
int               users_add(const char *name, const char *password);
void              users_wipe(void);

#endif
