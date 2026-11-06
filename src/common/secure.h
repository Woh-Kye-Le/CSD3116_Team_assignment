/**
 * @file    secure.h
 * @author  Sung Hong Chieh Jethro
 * @date    2026-11-04
 *
 * @brief   Security and encryption utilities.
 *
 * @version 1.0
 */
#ifndef SECURE_H
#define SECURE_H

#include <stdint.h>
#include <stddef.h>
#include <sodium.h>

#define SEC_KEY_LEN    32
#define SEC_NONCE_LEN  16
#define SEC_MAC_LEN    32
#define SEC_BOX_OVERHEAD crypto_secretbox_MACBYTES

#define SEC_DIR_C2S 0x01
#define SEC_DIR_S2C 0x02

#define SEC_LABEL_PROOF   "pong-client-proof"
#define SEC_LABEL_ACCEPT  "pong-server-accept"
#define SEC_LABEL_SESSION "pong-session-key"

/**
 * @brief Initializes the security subsystem.
 *
 * @return 0 on success, or -1 on failure.
 */
int sec_init(void);

/**
 * @brief Derives a secure key from a username and password.
 *
 * @param username The username string.
 * @param password The password string.
 * @param key The buffer to store the derived key.
 * @return 0 on success, or a negative error code on failure.
 */
int sec_derive_user_key(const char *username, const char *password, uint8_t key[SEC_KEY_LEN]);

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
                        int32_t player_id, uint8_t out[SEC_MAC_LEN]);

/**
 * @brief Performs a constant-time memory comparison.
 *
 * @param a First memory buffer.
 * @param b Second memory buffer.
 * @param len Number of bytes to compare.
 * @return 1 if equal, 0 if not equal.
 */
int sec_equal(const uint8_t *a, const uint8_t *b, size_t len);

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
              const uint8_t key[SEC_KEY_LEN]);

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
             const uint8_t key[SEC_KEY_LEN]);

/**
 * @brief Generates secure random bytes.
 *
 * @param buf The buffer to fill with random bytes.
 * @param len The number of random bytes to generate.
 */
void sec_random(void *buf, size_t len);

/**
 * @brief Securely zeroes out a memory buffer.
 *
 * @param buf The buffer to wipe.
 * @param len The number of bytes to wipe.
 */
void sec_wipe(void *buf, size_t len);

#endif
