#ifndef LOGIN_H
#define LOGIN_H

#include <stdint.h>
#include "secure.h"

#define USERNAME_LEN 32

typedef struct
{
    char    username[USERNAME_LEN];
    uint8_t user_key[SEC_KEY_LEN];
} Credentials;

int  login_read(int argc, char *argv[], Credentials *out);
void login_wipe(Credentials *c);

#endif
