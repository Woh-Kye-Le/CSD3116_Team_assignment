/**
 * @file    shared.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Shared utilities and state.
 *
 * @version 1.0
 */
#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

enum { CONN_IDLE, CONN_CONNECTING, CONN_AUTHENTICATING, CONN_JOINED, CONN_REJECTED };
enum { REQ_NONE, REQ_PLAY, REQ_LEAVE };

#define STATUS_LEN 96

typedef struct
{
    int          conn;
    int          player_id;
    bool         have_snap;
    GameSnapshot snap;
    char         status[STATUS_LEN];
} ClientView;

/**
 * @brief Retrieves the current client view state.
 * @param out Pointer to the ClientView structure to populate.
 */
void shared_view_get(ClientView *out);

/**
 * @brief Updates the client connection state and status message.
 * @param conn The new connection state.
 * @param player_id The assigned player ID, or -1 if none.
 * @param status A string describing the current status.
 */
void shared_view_set_conn(int conn, int player_id, const char *status);

/**
 * @brief Updates the game snapshot in the shared view.
 * @param snap Pointer to the GameSnapshot structure.
 */
void shared_view_set_snapshot(const GameSnapshot *snap);

/**
 * @brief Checks if the client is currently in an active match.
 * @return true if connected and in a match, false otherwise.
 */
bool shared_in_match(void);

/**
 * @brief Sets the player's input direction.
 * @param dir The new input direction.
 */
void   shared_set_input_dir(int dir);

/**
 * @brief Gets the current input direction.
 * @return The current input direction.
 */
int8_t shared_input_dir(void);

/**
 * @brief Sets the current client request.
 * @param req The request to set.
 */
void shared_request(int req);

/**
 * @brief Retrieves and clears the current client request.
 * @return The current request, or REQ_NONE if none.
 */
int  shared_take_request(void);

/**
 * @brief Increments the pause button press counter.
 */
void    shared_press_pause(void);

/**
 * @brief Gets the total number of pause button presses.
 * @return The pause press count.
 */
uint8_t shared_pause_presses(void);

/**
 * @brief Requests the application to quit.
 */
void shared_request_quit(void);

/**
 * @brief Checks if a quit has been requested.
 * @return true if quit requested, false otherwise.
 */
bool shared_quit_requested(void);

#endif
