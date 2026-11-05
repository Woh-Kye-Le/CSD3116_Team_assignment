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

Each client opens on the **main menu**:

| Menu item | What it does |
|---|---|
| **PLAY** | Connects to the server and logs you in. Nothing is sent to the server until you press it |
| **INSTRUCTIONS** | How to connect, the controls and the rules. **BACK**, Esc or Enter returns to the menu |
| **QUIT** | Closes the client |

Choose an item with **Up/Down** (or **W/S**) and **Enter**, or click it with the mouse. Once **both** players have pressed PLAY, the match starts with a 3-2-1 countdown.

### Controls

| Key | Action |
|---|---|
| **W** / **Up arrow** | Move paddle up |
| **S** / **Down arrow** | Move paddle down |
| **Esc** | Pause / resume the game **for both players** |
| **Q** | Leave the match and go back to the main menu (press PLAY to rejoin) |
| Close the window | Leave the match and quit |

**Pausing:** either player can press Esc. Everything stops on both screens: ball, paddles and countdowns. Both windows show **PAUSED** and who paused it. Either player presses Esc again to resume. If a player leaves while the game is paused, the pause is cleared and the remaining player goes back to "Waiting for an opponent".

While a client is still connecting, Esc or Q cancels and goes back to the menu.

Only the window you last clicked receives keyboard input. When both clients run on one laptop, click a window to control that player's paddle.

### Rules

- The ball bounces off the top and bottom walls and off paddles.
- Where the ball hits your paddle sets the bounce angle: the edges give steep angles.
- The ball speeds up 6% on every paddle hit.
- If the ball gets past a paddle, the other player scores. The next serve goes towards the player who lost the point.
- **First to 5 wins.** The winner banner shows for 4 seconds, then a new match starts automatically.
- If a player leaves, the game goes back to "Waiting for an opponent" and anyone can join the free slot.

---

## 3b. Playing on two laptops

By default WSL runs behind its own private network, so DDS on another laptop can't see it. Do this once on **both** laptops.

1. **Same network.** Connect both laptops to the same Wi-Fi. School or campus Wi-Fi often blocks laptop-to-laptop traffic. If so, use a phone hotspot.
2. **Turn on WSL mirrored networking** (Windows 11 22H2 or newer). In Windows, create or edit `C:\Users\<you>\.wslconfig`:
   ```
   [wsl2]
   networkingMode=mirrored
   ```
   Then run `wsl --shutdown` in PowerShell and reopen WSL.
3. **Allow incoming traffic to WSL.** In PowerShell **as Administrator**:
   ```
   Set-NetFirewallHyperVVMSetting -Name '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -DefaultInboundAction Allow
   ```
   If Windows asks whether to allow `server` or `client` through the firewall, allow it on private networks.
4. **Copy the project** to the second laptop, install the libraries (section 1) and build it there.
5. **Start the game:**
   - Laptop 1: `./bin/server`, then `./bin/client player1`
   - Laptop 2: `./bin/client player2`

### If laptop 2 is stuck on "Connecting to server..."

**Check 1: is WSL really in mirrored mode?** On each laptop, compare:
- WSL: `hostname -I`
- Windows Command Prompt: `ipconfig` (look at the Wi-Fi adapter's IPv4 address)

They must show the **same** IP address. If they don't, mirrored mode isn't active:
- Make sure the file is named exactly `.wslconfig`, not `.wslconfig.txt`. Notepad adds `.txt` unless you choose "All files" when saving.
- It must be in `C:\Users\<you>\`.
- Run `wsl --shutdown` afterwards.
- You need Windows 11 22H2 or newer. Check with `winver`.

**Check 2: firewall.** Windows usually treats a hotspot as a *Public* network and blocks incoming traffic. On both laptops:
- Go to Settings → Network & internet → Wi-Fi → (your hotspot) → set **Network profile type** to **Private**.
- Rerun the `Set-NetFirewallHyperVVMSetting` command above in an Administrator PowerShell.

**Check 3: use LAN mode.** This turns off DDS's automatic discovery and connects the two laptops directly by IP. In **every** WSL terminal, on both laptops, run this before starting the server or client:

```
source scripts/lan.sh <the OTHER laptop's IP>
```

For example, if laptop 1 is `172.20.10.2` and laptop 2 is `172.20.10.3`:
- Laptop 1 terminals: `source scripts/lan.sh 172.20.10.3`, then `./bin/server` (and `./bin/client player1`)
- Laptop 2 terminal: `source scripts/lan.sh 172.20.10.2`, then `./bin/client player2`

**Check 4: test DDS without the game.** `ddsperf` comes with `cyclonedds-tools`. After `source scripts/lan.sh ...` on both laptops:
- Laptop 1: `ddsperf pong`
- Laptop 2: `ddsperf ping`

If laptop 2 prints round-trip times, the two laptops can reach each other and the game will connect too. If it prints nothing, the network or firewall is still blocking traffic (see checks 1 and 2). A normal `ping` isn't a reliable test, because Windows blocks ping replies by default.

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
[game] Player 1 paused the game
[game] Player 2 resumed the game
[perf] 60.0 ticks/s | work per tick: avg 65.8 us, max 1997.9 us | players: 2
[game] Player 2 (player2) left the game
```

| Log tag | Meaning |
|---|---|
| `[auth]` | Logins, accepted or rejected |
| `[game]` | Joins, leaves, timeouts, scores, wins, pause / resume |
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
| Client stuck on "Connecting to server..." | Start `./bin/server` first. For two laptops, follow section 3b |
| "Login failed: wrong username or password" | Check the account table above. Restart the client to try again |
| "Server full (2 players)" | Only 2 players at once. It joins automatically when a slot frees up |
| "This user is already in the game" | That account is already playing. Wait 3 s if the old client crashed, or use another account |
| Pressing keys does nothing | Click inside that client's window first. Only the focused window reads the keyboard |
| Windows says "Remote Desktop is not responding" with 2 clients on one laptop | WSL shows Linux windows through a hidden Remote Desktop connection, and two windows at once can overload it. Run each client on a different laptop on the same network |
| No window appears | Update WSL with `wsl --update` in PowerShell, then restart WSL with `wsl --shutdown` |

---

## 8. Project layout

```
files/
├── README.md               this guide
├── Makefile                build rules (make, make clean, make adduser)
├── build.bat               Windows: build in WSL
├── clean.bat               Windows: delete bin/ and build/
├── scripts/
│   └── lan.sh              connect two laptops directly by IP (source scripts/lan.sh <IP>)
├── idl/
│   └── messages.idl        DDS message types: login handshake + encrypted Input/GameState
├── src/
│   ├── common/                 shared by server and client
│   │   ├── protocol.h/.c       game constants, encrypted payload formats, monotonic clock
│   │   ├── dds_setup.h/.c      DDS participant, topics, QoS, reader/writer helpers
│   │   └── secure.h/.c         key derivation, HMAC login, encryption (libsodium)
│   ├── server/
│   │   ├── main.c              start-up, 60 Hz simulation loop, encrypted state broadcast
│   │   ├── users.h/.c          account file: load, find, add (data/users.txt)
│   │   ├── auth.h/.c           login handshake: challenge, proof check, admit
│   │   ├── world.h/.c          shared player table + game + mutex (the only shared state)
│   │   ├── game.h/.c           Pong rules: paddles, ball physics, scoring, phases, pause
│   │   ├── server_net.h/.c     DDS network thread: logins and decrypting player input
│   │   └── perf.h/.c           [perf] tick-rate and work-time statistics
│   └── client/
│       ├── main.c              start-up, window, 60 fps frame loop
│       ├── login.h/.c          username/password prompt and key derivation
│       ├── shared.h/.c         state shared between the two client threads
│       ├── client_net.h/.c     DDS network thread: login, send input, receive state
│       ├── screens.h/.c        menu, instructions, game and pause screens
│       ├── draw.h/.c           OpenGL drawing: text, glowing paddles and ball, buttons
│       ├── textures.h/.c       procedurally generated textures (background, glow)
│       ├── effects.h/.c        ball trail, sparks, score flash/shake, confetti, menu demo
│       └── input.h/.c          keyboard and mouse handling
├── third_party/
│   └── stb_easy_font.h     text drawing (from the baseline)
├── data/
│   └── users.txt           accounts (username + derived key)
├── baseline_original/      the original lecturer baseline, unchanged
│
├── build/                  generated by make: messages.c/.h and object files
└── bin/                    generated by make: server, client
```

`build/` and `bin/` are created by the build and removed by `clean.bat` / `make clean`. Leave them out when you zip the source for submission.

---

## 9. How it works

**Architecture.** The server owns the game state. Clients only send which way their paddle is moving, and they draw the state the server sends back. Both programs use two threads:

- **Server:** a network thread handles logins and input. The main thread runs the game at a fixed 60 Hz and sends the state.
- **Client:** a network thread handles DDS traffic. The main thread runs the window, keyboard and OpenGL drawing.

**Module boundaries.**
- `game.c` is pure game logic: no DDS, no threads, no global variables. It receives which players are present and their paddle directions, and updates the game.
- `world.c` owns the only state shared between server threads, and every function that touches it is named `..._locked`. Callers hold the mutex just long enough to copy data in or out.
- Encryption and DDS writes always happen outside the lock.
- On the client, `shared.c` is the only state both threads touch: a mutex-protected view, plus atomics for input, pause presses and requests.
- `make` compiles each `.c` file separately into `build/obj/`, so editing one file rebuilds only that module.

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
