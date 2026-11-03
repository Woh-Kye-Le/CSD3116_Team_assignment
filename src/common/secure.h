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

int sec_init(void);

int sec_derive_user_key(const char *username, const char *password, uint8_t key[SEC_KEY_LEN]);

void sec_transcript_mac(const uint8_t key[SEC_KEY_LEN], const char *label,
                        uint64_t client_tag, const char *username,
                        const uint8_t client_nonce[SEC_NONCE_LEN],
                        const uint8_t server_nonce[SEC_NONCE_LEN],
                        int32_t player_id, uint8_t out[SEC_MAC_LEN]);

int sec_equal(const uint8_t *a, const uint8_t *b, size_t len);

void sec_seal(uint8_t *cipher, const uint8_t *plain, size_t len,
              uint8_t direction, int32_t player_id, uint64_t seq,
              const uint8_t key[SEC_KEY_LEN]);

int sec_open(uint8_t *plain, const uint8_t *cipher, size_t cipher_len,
             uint8_t direction, int32_t player_id, uint64_t seq,
             const uint8_t key[SEC_KEY_LEN]);

void sec_random(void *buf, size_t len);

void sec_wipe(void *buf, size_t len);

#endif
