/**
 * @file    login.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Login behavior and UI for the client.
 *
 * @version 1.0
 */
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

/**
 * @brief Reads login credentials from command line or prompts the user.
 *
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments array.
 * @param out Pointer to the Credentials structure to populate.
 * @return 0 on success, or -1 on failure.
 */
int  login_read(int argc, char *argv[], Credentials *out);

/**
 * @brief Wipes the credentials from memory securely.
 *
 * @param c Pointer to the Credentials structure to wipe.
 */
void login_wipe(Credentials *c);

#endif
