#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include "login.h"

#define PASSWORD_LEN 128

static void read_line(const char *prompt, char *buf, size_t len, bool hidden)
{
    struct termios old_t, new_t;
    bool tty = hidden && isatty(STDIN_FILENO);

    printf("%s", prompt);
    fflush(stdout);
    if (tty)
    {
        tcgetattr(STDIN_FILENO, &old_t);
        new_t = old_t;
        new_t.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
    }
    if (!fgets(buf, (int)len, stdin))
        buf[0] = '\0';
    if (tty)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
        printf("\n");
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

int login_read(int argc, char *argv[], Credentials *out)
{
    char password[PASSWORD_LEN];

    if (argc >= 2)
        snprintf(out->username, sizeof(out->username), "%s", argv[1]);
    else
        read_line("Username: ", out->username, sizeof(out->username), false);

    if (argc >= 3)
    {
        snprintf(password, sizeof(password), "%s", argv[2]);
        fprintf(stderr, "Note: passing the password on the command line is insecure; omit it to be prompted.\n");
    }
    else
        read_line("Password: ", password, sizeof(password), true);

    if (out->username[0] == '\0')
    {
        fprintf(stderr, "Username required\n");
        sec_wipe(password, sizeof(password));
        return -1;
    }

    printf("Deriving login key...\n");
    int rc = sec_derive_user_key(out->username, password, out->user_key);
    sec_wipe(password, sizeof(password));
    if (rc != 0)
    {
        fprintf(stderr, "Key derivation failed\n");
        return -1;
    }
    return 0;
}

void login_wipe(Credentials *c)
{
    sec_wipe(c->user_key, sizeof(c->user_key));
}
