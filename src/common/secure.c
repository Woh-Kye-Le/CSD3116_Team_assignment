/**
 * @file    secure.c
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Security and encryption utilities.
 *
 * @version 1.0
 */
#include <string.h>
#include "secure.h"

/**
 * @brief Initializes the security subsystem.
 *
 * @return 0 on success, or -1 on failure.
 */
int sec_init(void)
{
    return sodium_init() < 0 ? -1 : 0;
}

/**
 * @brief Derives a secure key from a username and password.
 *
 * @param username The username string.
 * @param password The password string.
 * @param key The buffer to store the derived key.
 * @return 0 on success, or a negative error code on failure.
 */
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

/**
 * @brief Writes a 64-bit integer to a byte buffer in little-endian format.
 *
 * @param out The buffer to write to.
 * @param v The 64-bit integer to write.
 */
static void put_u64(uint8_t out[8], uint64_t v)
{
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)(v >> (8 * i));
}

/**
 * @brief Computes a MAC for a login transcript to prove identity or accept a join.
 *
 * @param key The user's key.
 * @param label A string label distinguishing the type of proof.
 * @param client_tag The client's unique tag.
 * @param username The username string.
 * @param client_nonce The client's nonce.
 * @param server_nonce The server's nonce.
 * @param player_id The assigned player ID (0 if not applicable).
 * @param out The buffer to store the computed MAC.
 */
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

/**
 * @brief Performs a constant-time memory comparison.
 *
 * @param a First memory buffer.
 * @param b Second memory buffer.
 * @param len Number of bytes to compare.
 * @return 1 if equal, 0 if not equal.
 */
int sec_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    return sodium_memcmp(a, b, len) == 0;
}

/**
 * @brief Constructs a nonce for symmetric encryption based on message parameters.
 *
 * @param nonce The buffer to store the generated nonce.
 * @param direction The direction of the message.
 * @param player_id The player ID.
 * @param seq The sequence number.
 */
static void make_nonce(uint8_t nonce[crypto_secretbox_NONCEBYTES],
                       uint8_t direction, int32_t player_id, uint64_t seq)
{
    memset(nonce, 0, crypto_secretbox_NONCEBYTES);
    nonce[0] = direction;
    nonce[1] = (uint8_t)player_id;
    put_u64(nonce + 2, seq);
}

/**
 * @brief Encrypts and authenticates a plaintext message.
 *
 * @param cipher The buffer to store the resulting ciphertext (must have SEC_BOX_OVERHEAD extra bytes).
 * @param plain The plaintext message to encrypt.
 * @param len The length of the plaintext message.
 * @param direction The direction of the message (SEC_DIR_C2S or SEC_DIR_S2C).
 * @param player_id The player ID associated with the message.
 * @param seq The sequence number of the message.
 * @param key The symmetric key used for encryption.
 */
void sec_seal(uint8_t *cipher, const uint8_t *plain, size_t len,
              uint8_t direction, int32_t player_id, uint64_t seq,
              const uint8_t key[SEC_KEY_LEN])
{
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    make_nonce(nonce, direction, player_id, seq);
    crypto_secretbox_easy(cipher, plain, len, nonce, key);
}

/**
 * @brief Decrypts and verifies a ciphertext message.
 *
 * @param plain The buffer to store the resulting plaintext.
 * @param cipher The ciphertext message to decrypt.
 * @param cipher_len The length of the ciphertext message.
 * @param direction The direction of the message (SEC_DIR_C2S or SEC_DIR_S2C).
 * @param player_id The player ID associated with the message.
 * @param seq The sequence number of the message.
 * @param key The symmetric key used for decryption.
 * @return 0 on success, or -1 if authentication fails.
 */
int sec_open(uint8_t *plain, const uint8_t *cipher, size_t cipher_len,
             uint8_t direction, int32_t player_id, uint64_t seq,
             const uint8_t key[SEC_KEY_LEN])
{
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    make_nonce(nonce, direction, player_id, seq);
    return crypto_secretbox_open_easy(plain, cipher, cipher_len, nonce, key) == 0 ? 0 : -1;
}

/**
 * @brief Generates secure random bytes.
 *
 * @param buf The buffer to fill with random bytes.
 * @param len The number of random bytes to generate.
 */
void sec_random(void *buf, size_t len)
{
    randombytes_buf(buf, len);
}

/**
 * @brief Securely zeroes out a memory buffer.
 *
 * @param buf The buffer to wipe.
 * @param len The number of bytes to wipe.
 */
void sec_wipe(void *buf, size_t len)
{
    sodium_memzero(buf, len);
}
