# Robo Cat Ears Watch

ESP-IDF firmware in C++ for the wrist controller: a Waveshare ESP32-S3 Touch AMOLED 2.06, LVGL
through `esp_lvgl_port`, with the UI built on **esp-brookesia** (which is why the CMake project is
named `esp-brookesia`, not after this repo).

The watch is a BLE **client** of the ears, and a **play-only** one. Licensed GPL-3.0-or-later.

## Layout

```
main/                 boot and init (main.cpp), the dark stylesheet (dark/), system status (system/)
components/
  brookesia_app_robo_cat_ears/   the app: one .cpp/.hpp pair per screen under screens/
  brookesia_app_system_info/     the system info app
  services/                      one component per domain service
  brookesia_core/, XPowersLib/   vendored third party - don't edit
tools/                convert_rgb24_to_rgba32.py
```

`components/services/` holds `bluetooth_service`, `animation_store_service`, `animation_mode_service`,
`lighting_service` and `calibration_service`. They sit a level deeper than the build system scans, so
the root `CMakeLists.txt` names their parent in `EXTRA_COMPONENT_DIRS` — a new service directory there
is picked up automatically, a new nesting level is not.

Screens talk to services; services own the BLE traffic. A screen should not be reaching for
`esp_gattc_api` directly.

## Development

```bash
idf.py build                       # target comes from the committed sdkconfig (esp32s3)
idf.py -p /dev/ttyACM0 flash monitor
idf.py menuconfig
```

`README.md` covers getting the board visible from WSL over `usbipd` — that setup is a prerequisite
for anything involving `-p /dev/ttyACM0`. It also notes the image pipeline: art is converted to
`LV_COLOR_FORMAT_ARGB8888` and checked in as a C array under an app's `assets/`.

**There are no host tests here.** Unlike `robo-cat-ears`, nothing in this repo builds off-target, so
verification is a build plus a run on the watch. Say which of the two you actually did.

`esp_lvgl_port` is pinned `<2.8.0` on purpose: 2.8+ uses `LV_COLOR_FORMAT_RGB565_SWAPPED`, which the
BSP's LVGL 9.2.2 lacks. Don't unpin it to clear a dependency warning.

## The BLE protocol is the contract

The wire contract is **`docs/ble-protocol.md` in [`junderdo/robo-cat-ears`](https://github.com/junderdo/robo-cat-ears)**,
which owns it. This repo is one of its three parties, alongside the ears (the GATT server) and the
`milk-lab-creations` web app (the authoring tool). Read it before changing anything that touches the
wire, and change it there first.

Three things this watch is bound by, all of them easy to break from inside this repo alone:

- **The watch owns no animations.** It reads the ears' store, caches it for the life of the
  connection, and plays by slot index. It never stores, deletes, renames, or persists animations —
  that's the web app's job. This was deliberately stripped out; don't reintroduce it.
- **Store protocol version is exact equality, then disconnect.** `ANIMATION_STORE_PROTOCOL_VERSION`
  refuses anything else rather than degrading, because degrading means guessing what changed in a
  version it has never seen.
- **Length guards on replies are minimums, and must stay minimums.** `CAPABILITY` is checked with
  `_rx_length < 4`, not `!= 4`, so the ears can append to the record — and they since have, with the
  six-byte device serial. Tightening one of these to an equality hard-disconnects the watch from
  newer ears and looks like a firmware bug on the wrong side of the link.

`DataType` in `bluetooth_service.hpp` mirrors the protocol's type bytes (`0x01` animation, `0x02`
lighting, `0x03` calibration, `0x04` animation mode, `0x06` store). It is a copy of a contract owned
elsewhere; keep the numbers and the document in step.

## Coding standards

There is no separate standards document here — match the surrounding C++. What the code already
commits to and review holds to:

- **Every new `.cpp`/`.hpp` carries the file header block**: description, author, copyright, and
  `SPDX-License-Identifier: GPL-3.0-or-later`. Nearly every file in `main/` and the first-party
  components has one.
- Project code lives in `namespace robo_cat_ears`. Headers carry Doxygen comments on the public API —
  what a caller can't read off the signature, especially the *why* behind a constant.
- One `.cpp`/`.hpp` pair per screen in `screens/`, one component per service.
- Vendored trees (`brookesia_core`, `XPowersLib`, `managed_components/`) are not ours to edit. A
  change that seems to need one is a sign to look again.

## Issue tracking (Trello)

Issues for this project are tracked on the **Robo Cat Ears** Trello board
(<https://trello.com/b/DHDPlEuL/robo-cat-ears>) — the same board as the ears firmware, since one
product spans both — using the `trello` CLI (npm package `trello-cli`, installed globally).

The board's lists are **Backlog**, **Todo**, **In Progress**, **Ready for Review**, and **Done**.

### Common commands

```bash
trello list:list --board "Robo Cat Ears"                    # show the board's lists
trello card:list --board "Robo Cat Ears" --list "Todo"      # list cards in a list
trello card:get-by-id --id <card-id>                        # read a card in full
trello card:create --board "Robo Cat Ears" --list "Todo" -n "Card title" --description "Details"
trello card:move --board "Robo Cat Ears" --list "Todo" --card "Card title" --to "In Progress"
trello search --query "watch" --board "Robo Cat Ears"       # search cards
```

Run `trello <topic> --help` (e.g. `trello card --help`) to discover subcommands. Card body shape,
label handling, wayfinder conventions, and the CLI's sharp edges are in
`docs/agents/issue-tracker.md`.

### Workflow

- New bugs/ideas/tasks go in **Todo** as cards; **Backlog** holds what isn't queued yet.
- Move a card to **In Progress** when work starts, **Ready for Review** when a PR is open, **Done**
  when it lands.
- Reference the card title in related commit messages when it makes sense.
- The board covers the whole product — ears firmware, this watch app, the web app, the PCB, the
  3D-printed parts. Cards for this repo usually say "watch" in the title, but check before assuming.

### Auth

Credentials are stored in `~/.trello-cli/` (set up once via `trello auth:api-key <key>` and
`trello auth:token <token>`; key/token come from <https://trello.com/power-ups/admin>). If a command
fails with an auth error, ask the user to re-authenticate — do not attempt to fetch tokens yourself.

## Agent skills

Like `robo-cat-ears` and unlike `milk-lab-creations`, this repo does **not** vendor the engineering
skills — `.claude/` is gitignored and the skills come from the global install in `~/.claude/skills/`.
So a skill here is whatever version is installed globally.

What the repo does own is the configuration those skills read: the three files in `docs/agents/`.

### Issue tracker

Cards on the **Robo Cat Ears** Trello board, driven by the `trello` CLI — not GitHub issues. The
GitHub remote is for code and pull requests only. See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical roles, each label string equal to its name (`needs-triage`, `needs-info`,
`ready-for-agent`, `ready-for-human`, `wontfix`). Only two exist on the board so far. See
`docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and one `docs/adr/` at the repo root would cover the whole watch
app. Neither exists yet, and the wire contract lives in another repo. See `docs/agents/domain.md`.
