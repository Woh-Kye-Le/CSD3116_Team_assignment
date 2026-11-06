/**
 * @file    users.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   User management and data structures.
 *
 * @version 1.0
 */
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

/**
 * @brief Checks if a username is valid.
 *
 * @param name The username to validate.
 * @return true if valid, false otherwise.
 */
bool users_name_valid(const char *name);

/**
 * @brief Resolves the absolute path to the users data file.
 */
void        users_resolve_path(void);

/**
 * @brief Gets the current path to the users data file.
 *
 * @return The path string.
 */
const char *users_path(void);

/**
 * @brief Loads the user database from disk.
 *
 * @return The number of users loaded, or -1 on error.
 */
int               users_load(void);

/**
 * @brief Gets the number of currently loaded users.
 *
 * @return The user count.
 */
int               users_count(void);

/**
 * @brief Finds a user by their username.
 *
 * @param name The username to search for.
 * @return Pointer to the UserRecord if found, or NULL.
 */
const UserRecord *users_find(const char *name);

/**
 * @brief Adds a new user to the database and saves it.
 *
 * @param name The new username.
 * @param password The user's password.
 * @return 0 on success, or a positive error code on failure.
 */
int               users_add(const char *name, const char *password);

/**
 * @brief Wipes all user data from memory securely.
 */
void              users_wipe(void);

#endif
