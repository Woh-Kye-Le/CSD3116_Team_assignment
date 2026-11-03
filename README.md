# Secure DDS Pong

A real-time 2-player Pong game for CSD3116, built from the DDS server/client baseline. It uses WSL, OpenGL, GLFW and Cyclone DDS. Players log in with a password, and all game traffic is encrypted.

---

## 1. One-time setup

Open a WSL (Ubuntu) terminal and install the libraries:

```
sudo apt update
sudo apt install cyclonedds-dev cyclonedds-tools libglfw3-dev libgl1-mesa-dev libsodium-dev pkgconf make
```

You need Windows 11 (or Windows 10 with WSLg) so the game window can open from WSL.

---

## 2. Build

**Option A (Windows):** double-click **`build.bat`** in this folder.

**Option B (WSL terminal):**
```
cd /mnt/c/Users/User/Documents/CSD3116/Team_Ass/files
make
```

Either way, `idlc` generates `build/messages.c` and `build/messages.h` from `idl/messages.idl`. Then `bin/server` and `bin/client` are compiled.

---

## 3. Run a game

You need **three WSL terminals**, all in this folder (the one containing `Makefile`). Start the server first.

**Terminal 1: server**
```
./bin/server
```

**Terminal 2: player 1 (left paddle, cyan)**
```
./bin/client player1
```
Type the password `pong123` when asked. The password doesn't show as you type.

**Terminal 3: player 2 (right paddle, magenta)**
```
./bin/client player2
```
Password: `pong456`

The match starts with a 3-2-1 countdown once both players are in.

### Controls

| Key | Action |
|---|---|
| **W** / **Up arrow** | Move paddle up |
| **S** / **Down arrow** | Move paddle down |
| **Esc** (or close the window) | Leave the game |

### Rules

- The ball bounces off the top and bottom walls and off paddles.
- Where the ball hits your paddle sets the bounce angle: the edges give steep angles.
- The ball speeds up 6% on every paddle hit.
- If the ball gets past a paddle, the other player scores. The next serve goes towards the player who lost the point.
- **First to 5 wins.** The winner banner shows for 4 seconds, then a new match starts automatically.
- If a player leaves, the game goes back to "Waiting for an opponent" and anyone can join the free slot.

---

## 4. Accounts

| Username | Password |
|---|---|
| player1 | pong123 |
| player2 | pong456 |
| player3 | pong789 |

To add your own account:
```
make adduser NAME=<username> PASS=<password>
```
This is the same as `./bin/server adduser <username> <password>`.
Usernames are 1-31 characters (letters, digits, `_` or `-`). Passwords are at least 6 characters. Restart the server after adding users.

`data/users.txt` stores a derived key for each user, never the password itself. Don't delete it, or nobody can log in. The server always finds it via its own location (`bin/../data/users.txt`), so it works from any folder.

---

## 5. Clean

**Option A (Windows):** double-click **`clean.bat`**.

**Option B (WSL):** `make clean`

This deletes the generated `bin/` and `build/` folders only. `src/`, `idl/`, `third_party/`, `data/` and `baseline_original/` are kept. Run a build again before playing.

---

## 6. What you will see

**Server terminal**
```
Loaded 3 user(s) from /mnt/c/Users/User/Documents/CSD3116/Team_Ass/files/data/users.txt
Secure Pong server running. Waiting for players... (Ctrl+C to stop)
[auth] 'player1' logged in as Player 1 (left)
[auth] 'player2' logged in as Player 2 (right)
[game] Match starting
[game] Player 1 scores  (1 - 0)
[perf] 60.0 ticks/s | work per tick: avg 65.8 us, max 1997.9 us | players: 2
[game] Player 2 (player2) left the game
```

| Log tag | Meaning |
|---|---|
| `[auth]` | Logins, accepted or rejected |
| `[game]` | Joins, leaves, timeouts, scores, wins |
| `[security]` | A forged or tampered message was dropped |
| `[perf]` | Every 10 s: tick rate and time spent per tick (use in the slides) |

Stop the server with **Ctrl+C**.

---

## 7. Troubleshooting

| Problem | Fix |
|---|---|
| `Command 'pkg-config' not found` | `sudo apt install pkgconf` |
| `sodium.h: No such file or directory` | `sudo apt install libsodium-dev` |
| `make: command not found` | `sudo apt install make` |
| `idlc` prints `Usage: idlc [OPTIONS] FILE` | You copied a long dash `–` from the slides. Type a normal `-` |
| `make: *** No rule to make target` | Run `make` from the project folder (the one with `Makefile`), not from `bin/` or `src/` |
| `No users found in .../data/users.txt` | `data/users.txt` is missing or empty. Add a user with `make adduser NAME=... PASS=...` |
| Client stuck on "Connecting to server..." | Start `./bin/server` first, in the same WSL instance |
| "Login failed: wrong username or password" | Check the account table above. Restart the client to try again |
| "Server full (2 players)" | Only 2 players at once. It joins automatically when a slot frees up |
| "This user is already in the game" | That account is already playing. Wait 3 s if the old client crashed, or use another account |
| No window appears | Update WSL with `wsl --update` in PowerShell, then restart WSL with `wsl --shutdown` |

---

## 8. Project layout

```
files/
├── README.md               this guide
├── Makefile                build rules (make, make clean, make adduser)
├── build.bat               Windows: build in WSL
├── clean.bat               Windows: delete bin/ and build/
├── idl/
│   └── messages.idl        DDS message types: login handshake + encrypted Input/GameState
├── src/
│   ├── common/
│   │   ├── pong_common.h   shared constants, state packing, DDS topics and QoS
│   │   ├── secure.h        security API
│   │   └── secure.c        password key derivation, HMAC login, encryption (libsodium)
│   ├── server/
│   │   └── server.c        game server: logins, physics, scoring, state broadcast
│   └── client/
│       └── client.c        GLFW/OpenGL client: login prompt, input, drawing
├── third_party/
│   └── stb_easy_font.h     text drawing (from the baseline)
├── data/
│   └── users.txt           accounts (username + derived key)
├── baseline_original/      the original lecturer baseline, unchanged
│
├── build/                  generated by make: messages.c, messages.h
└── bin/                    generated by make: server, client
```

`build/` and `bin/` are created by the build and removed by `clean.bat` / `make clean`. Leave them out when you zip the source for submission.

---

## 9. How it works

**Architecture.** The server owns the game state. Clients only send which way their paddle is moving, and they draw the state the server sends back. Both programs use two threads:

- **Server:** a network thread handles logins and input. The main thread runs the game at a fixed 60 Hz and sends the state.
- **Client:** a network thread handles DDS traffic. The main thread runs the window, keyboard and OpenGL drawing.

**Login (challenge-response).**
1. The client sends its username and a random nonce.
2. The server replies with its own random nonce.
3. The client sends an HMAC-SHA256 of both nonces, keyed with its password-derived key (Argon2id).
4. The server checks it, assigns a paddle, and replies with its own HMAC. This proves the server is genuine.

The password never crosses the network, and each login creates a fresh session key.

**Security.**
- Every Input and GameState message is encrypted and authenticated (XSalsa20-Poly1305).
- Changed messages fail the check. Replayed messages are rejected by sequence number.
- One player cannot send input as the other.
- Unknown usernames still get a challenge, so usernames can't be probed.
- The Ubuntu Cyclone DDS 0.10.4 package has no DDS Security plugins, so this is done in the application.

**Optimization compared with the baseline.**
- DDS waitsets let threads sleep until data arrives. The baseline busy-loops at 100% CPU.
- The game runs on a fixed 60 Hz timestep with no sleeping while the mutex is held. The baseline slept 20 ms per player inside the lock.
- Each tick uses one short lock, with encryption and DDS writes outside it. State is packed once per tick.
- QoS is set per topic: game state is best-effort, keep-last-1, keyed per player. Login and input are reliable.
- The client uses vsync, one lock per frame, and atomic (lock-free) input.

**Known limits.**
- DDS discovery data, such as topic names, is not encrypted.
- Someone who records a login could try to brute-force a weak password offline. Argon2id makes this slow.
- Message flooding (DoS) is not prevented.
