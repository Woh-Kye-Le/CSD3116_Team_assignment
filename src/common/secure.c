#include <string.h>
#include "secure.h"

int sec_init(void)
{
    return sodium_init() < 0 ? -1 : 0;
}

int sec_derive_user_key(const char *username, const char *password, uint8_t key[SEC_KEY_LEN])
{
    uint8_t salt[crypto_pwhash_SALTBYTES];
    crypto_generichash_state st;
    crypto_generichash_init(&st, NULL, 0, sizeof(salt));
    crypto_generichash_update(&st, (const uint8_t *)"pong-salt:", 10);
    crypto_generichash_update(&st, (const uint8_t *)username, strlen(username));
    crypto_generichash_final(&st, salt, sizeof(salt));

    return crypto_pwhash(key, SEC_KEY_LEN, password, strlen(password), salt,
                         crypto_pwhash_OPSLIMIT_INTERACTIVE,
                         crypto_pwhash_MEMLIMIT_INTERACTIVE,
                         crypto_pwhash_ALG_ARGON2ID13);
}

static void put_u64(uint8_t out[8], uint64_t v)
{
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)(v >> (8 * i));
}

void sec_transcript_mac(const uint8_t key[SEC_KEY_LEN], const char *label,
                        uint64_t client_tag, const char *username,
                        const uint8_t client_nonce[SEC_NONCE_LEN],
                        const uint8_t server_nonce[SEC_NONCE_LEN],
                        int32_t player_id, uint8_t out[SEC_MAC_LEN])
{
    crypto_auth_hmacsha256_state st;
    uint8_t buf[8];
    uint8_t name_len = (uint8_t)strlen(username);

    crypto_auth_hmacsha256_init(&st, key, SEC_KEY_LEN);
    crypto_auth_hmacsha256_update(&st, (const uint8_t *)label, strlen(label) + 1);
    put_u64(buf, client_tag);
    crypto_auth_hmacsha256_update(&st, buf, 8);
    crypto_auth_hmacsha256_update(&st, &name_len, 1);
    crypto_auth_hmacsha256_update(&st, (const uint8_t *)username, name_len);
    crypto_auth_hmacsha256_update(&st, client_nonce, SEC_NONCE_LEN);
    crypto_auth_hmacsha256_update(&st, server_nonce, SEC_NONCE_LEN);
    put_u64(buf, (uint64_t)(int64_t)player_id);
    crypto_auth_hmacsha256_update(&st, buf, 8);
    crypto_auth_hmacsha256_final(&st, out);
}

int sec_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    return sodium_memcmp(a, b, len) == 0;
}

static void make_nonce(uint8_t nonce[crypto_secretbox_NONCEBYTES],
                       uint8_t direction, int32_t player_id, uint64_t seq)
{
    memset(nonce, 0, crypto_secretbox_NONCEBYTES);
    nonce[0] = direction;
    nonce[1] = (uint8_t)player_id;
    put_u64(nonce + 2, seq);
}

void sec_seal(uint8_t *cipher, const uint8_t *plain, size_t len,
              uint8_t direction, int32_t player_id, uint64_t seq,
              const uint8_t key[SEC_KEY_LEN])
{
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    make_nonce(nonce, direction, player_id, seq);
    crypto_secretbox_easy(cipher, plain, len, nonce, key);
}

int sec_open(uint8_t *plain, const uint8_t *cipher, size_t cipher_len,
             uint8_t direction, int32_t player_id, uint64_t seq,
             const uint8_t key[SEC_KEY_LEN])
{
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    make_nonce(nonce, direction, player_id, seq);
    return crypto_secretbox_open_easy(plain, cipher, cipher_len, nonce, key) == 0 ? 0 : -1;
}

void sec_random(void *buf, size_t len)
{
    randombytes_buf(buf, len);
}

void sec_wipe(void *buf, size_t len)
{
    sodium_memzero(buf, len);
}
