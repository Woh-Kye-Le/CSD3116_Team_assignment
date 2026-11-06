/**
 * @file    users.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   User management and data structures.
 *
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include "users.h"

static UserRecord g_users[MAX_USERS];
static int        g_num_users = 0;
static char       g_users_path[PATH_MAX + 16] = "data/users.txt";

/**
 * @brief Checks if a username is valid.
 *
 * @param name The username to validate.
 * @return true if valid, false otherwise.
 */
bool users_name_valid(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len > USERNAME_LEN - 1)
        return false;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-')
            return false;
    return true;
}

/**
 * @brief Resolves the absolute path to the users data file.
 */
void users_resolve_path(void)
{
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len <= 0)
        return;
    exe[len] = '\0';
    char *slash = strrchr(exe, '/');
    if (!slash)
        return;
    *slash = '\0';

    char data_dir[PATH_MAX + 16];
    char resolved[PATH_MAX];
    snprintf(data_dir, sizeof(data_dir), "%s/../data", exe);
    if (realpath(data_dir, resolved))
        snprintf(g_users_path, sizeof(g_users_path), "%s/users.txt", resolved);
}

/**
 * @brief Gets the current path to the users data file.
 *
 * @return The path string.
 */
const char *users_path(void)
{
    return g_users_path;
}

/**
 * @brief Gets the number of currently loaded users.
 *
 * @return The user count.
 */
int users_count(void)
{
    return g_num_users;
}

/**
 * @brief Finds a user by their username.
 *
 * @param name The username to search for.
 * @return Pointer to the UserRecord if found, or NULL.
 */
const UserRecord *users_find(const char *name)
{
    for (int i = 0; i < g_num_users; i++)
        if (strcmp(g_users[i].name, name) == 0)
            return &g_users[i];
    return NULL;
}

/**
 * @brief Loads the user database from disk.
 *
 * @return The number of users loaded, or -1 on error.
 */
int users_load(void)
{
    FILE *f = fopen(g_users_path, "r");
    if (!f)
        return -1;

    char line[256];
    g_num_users = 0;
    while (g_num_users < MAX_USERS && fgets(line, sizeof(line), f))
    {
        char name[64], hex[160];
        size_t bin_len = 0;
        if (line[0] == '#' || sscanf(line, "%63s %159s", name, hex) != 2)
            continue;
        if (!users_name_valid(name))
            continue;
        if (sodium_hex2bin(g_users[g_num_users].key, SEC_KEY_LEN, hex, strlen(hex),
                           NULL, &bin_len, NULL) != 0 || bin_len != SEC_KEY_LEN)
            continue;
        snprintf(g_users[g_num_users].name, sizeof(g_users[0].name), "%.31s", name);
        g_num_users++;
    }
    fclose(f);
    return g_num_users;
}

/**
 * @brief Adds a new user to the database and saves it.
 *
 * @param name The new username.
 * @param password The user's password.
 * @return 0 on success, or a positive error code on failure.
 */
int users_add(const char *name, const char *password)
{
    if (!users_name_valid(name))
    {
        fprintf(stderr, "Username must be 1-31 characters: letters, digits, _ or -\n");
        return 1;
    }
    if (strlen(password) < 6)
    {
        fprintf(stderr, "Password must be at least 6 characters\n");
        return 1;
    }
    users_load();
    if (users_find(name))
    {
        fprintf(stderr, "User '%s' already exists in %s\n", name, g_users_path);
        return 1;
    }

    uint8_t key[SEC_KEY_LEN];
    char hex[SEC_KEY_LEN * 2 + 1];
    if (sec_derive_user_key(name, password, key) != 0)
    {
        fprintf(stderr, "Key derivation failed (out of memory?)\n");
        return 1;
    }
    sodium_bin2hex(hex, sizeof(hex), key, SEC_KEY_LEN);
    sec_wipe(key, sizeof(key));

    FILE *f = fopen(g_users_path, "a");
    if (!f)
    {
        perror(g_users_path);
        return 1;
    }
    fprintf(f, "%s %s\n", name, hex);
    fclose(f);
    chmod(g_users_path, 0600);
    printf("Added user '%s' to %s\n", name, g_users_path);
    return 0;
}

/**
 * @brief Wipes all user data from memory securely.
 */
void users_wipe(void)
{
    sec_wipe(g_users, sizeof(g_users));
    g_num_users = 0;
}
