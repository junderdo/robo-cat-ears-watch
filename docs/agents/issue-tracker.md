# Issue tracker: Trello

Issues, tasks, and specs (you may know a spec as a PRD) for this repo live as **cards on the
"Robo Cat Ears" Trello board** (<https://trello.com/b/DHDPlEuL/robo-cat-ears>). Use the `trello` CLI
(npm package `trello-cli`, installed globally) for all operations. Do not use a GitHub-issues
workflow here — the GitHub remote is for code and pull requests only.

The board's lists are **Backlog**, **Todo**, **In Progress**, **Ready for Review**, and **Done**.
List position carries workflow state; labels carry triage role and wayfinder type.

**The board is the whole product, not this repo.** Cards cover this watch app, the `robo-cat-ears`
firmware, the web authoring app, the PCB, and the 3D-printed hardware alike. Read what a card
actually asks for — and which repo it names — before starting.

## Conventions

Almost every command needs `--board "Robo Cat Ears"` plus the `--list` the card currently sits in.
Add `--format json` to any command when you need to parse the output.

- **Create a card**: `trello card:create --board "Robo Cat Ears" --list "Todo" -n "Title" --description "..."`.
  Use a heredoc or a `$(cat file)` for multi-line descriptions. Optional: `--label <name>` (repeatable),
  `--due <date>`, `--position top|bottom`.
- **List cards**: `trello card:list --board "Robo Cat Ears" --list "Todo"` — prints `Name (ID: ...)`
  per card. This is the way to get a card's ID.
- **Read a card**: `trello card:get-by-id --id <card-id>` — the most reliable read, since it needs no
  `--list`. `trello card:show --card "<title>" --board "Robo Cat Ears" --list "Todo"` works too
  when you know the list.
- **Comment**: `trello card:comment --board "Robo Cat Ears" --list "<list>" --card "<title>" --text "..."`
- **Read comments**: `trello card:comments --board "Robo Cat Ears" --list "<list>" --card "<title>"`
- **Apply a label**: `trello card:label --board "Robo Cat Ears" --list "<list>" --card "<title>" --label "<label>"`;
  remove one with `trello card:unlabel ... --label "<label>"`
- **Assign**: `trello card:assign --board "Robo Cat Ears" --list "<list>" --card "<title>" --user <username>`.
  The username must be a real Trello username (`jeffreyunderdown`) — `--user me` fails with a 400.
  Confusingly, `trello card:assigned-to --user me` *does* work; `me` is that command's default.
- **Move between lists** (this is how state changes): `trello card:move --board "Robo Cat Ears" --list "Todo" --card "<title>" --to "In Progress"`
- **Close**: move the card to **Done**. A card whose PR is open but unmerged belongs in **Ready for
  Review**, not Done. Reserve `trello card:archive` for cards that were mistakes or duplicates — a
  card that got built belongs in Done, not archived.
- **Search**: `trello search --query "some text" --board "Robo Cat Ears" --type cards`
- **Labels on the board**: `trello label:list --board "Robo Cat Ears"`;
  create one with `trello label:create --board "Robo Cat Ears" -n "<name>" --color <green|yellow|orange|red|purple|blue|sky|lime|pink|black>`
- **Checklists**: `trello card:checklist ... -n "<name>"` creates an empty checklist and
  `trello card:check-item ... --item "<item>" --state complete|incomplete [--checklist "<name>"]`
  ticks an existing item — but **the CLI cannot add items to a checklist, or delete a checklist**.
  A checklist created from the CLI stays empty forever unless a human fills it in through the web UI.
  Don't build a workflow on checklists.

### Card body shape

Cards are the unit of work; the description is the issue body. The established shape on this board:

```markdown
## Parent

<title of the parent spec/map card> — <its trello.com/c/... URL>

## Repo

<which repo this card lands in, and its role>

## What to build

<prose: the observable behavior this card delivers>

## Acceptance criteria

- [ ] ...
- [ ] ...

## Blocked by

<card titles + URLs, or "None — can start immediately.">
```

Keep `## Parent` and `## Blocked by` even when empty — they're what makes the board navigable
without opening every card. ["Watch animate screen plays the ears' real
slots"](https://trello.com/c/G7sGU4sj) is the worked example.

**`## Repo` matters more here than on a single-repo board.** One board serves the watch, the ears
firmware, the web app and the physical build, so a card that doesn't name its repo is ambiguous
before it is anything else. Name the repo and what it is in the system ("`robo-cat-ears-watch` - the
watch firmware, a BLE client").

**`## Parent` may point out of Trello.** Cross-repo work is normal here, and a card's parent is often
a spec or ADR in `milk-lab-creations`, or a section of `docs/ble-protocol.md` in `robo-cat-ears`,
rather than a map card on this board. Name the file path and the sections when that's the case, and
link the map card alongside it if one exists.

Many older cards on this board are one-line human notes with no description at all. That is fine —
the shape above is for cards a skill creates or an agent is meant to pick up, not a standard to
retrofit onto the backlog.

## When a skill says "publish to the issue tracker"

Create a card in **Todo** with `trello card:create`, using the body shape above.

## When a skill says "fetch the relevant ticket"

`trello card:list` the relevant list to find the card's ID, then `trello card:get-by-id --id <id>`.
Follow with `trello card:comments` if the conversation history matters. The user will normally name
the card by title. Search across lists with `trello search` when you don't know which list holds it —
this board has five.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a card with one **child** card per ticket.

- **Map**: a card labelled `wayfinder:map` whose description holds the Notes / Decisions-so-far / Fog
  body. Create with `trello card:create --board "Robo Cat Ears" --list "Todo" -n "Spec: <effort>" --label "wayfinder:map"`.
- **Child ticket**: its own card in **Todo**, labelled `wayfinder:<type>`
  (`research` / `prototype` / `grilling` / `task`), with `## Parent` in the description linking the
  map card's URL. Trello has no native parent/child, and the CLI cannot populate a checklist, so
  **the `## Parent` link is the whole representation** — every child must carry it.
- **No `wayfinder:*` label exists on this board yet.** `trello label:create` each one the first time
  you need it, and check `trello label:list` first so you don't add a second copy in another color.
- **Blocking**: the `## Blocked by` section of the child's description, listing blocker card titles
  and URLs. A ticket is unblocked when every card it lists is in **Done**.
- **Frontier query**: list **Todo** (`trello card:list --board "Robo Cat Ears" --list "Todo"`),
  drop cards whose `## Blocked by` names a card not yet in Done, drop cards with a member assigned;
  first in board order wins. **Backlog is not the frontier** — a card there hasn't been queued.
- **Claim**: `trello card:assign --board "Robo Cat Ears" --list "Todo" --card "<title>" --user jeffreyunderdown`
  and move it to **In Progress** — the session's first write. `--user me` fails.
- **Create then wire**: create child cards in **dependency order** so each blocker's URL already
  exists when the card that lists it is created. That collapses wayfinder's create-then-wire two-pass
  into one pass, since `## Blocked by` is written at create time.
- **Resolve**: `trello card:comment` the answer onto the card, move it to **Done**, then append a
  context pointer (gist + card URL) to the map card's Decisions-so-far. A full resolution usually
  exceeds the 414 ceiling — see Gotchas for measuring it and splitting across ordered comments.

## Gotchas

- **`--list` must be the card's *current* list.** After a `card:move`, later commands need the new
  list name. Commands that take a card ID (`card:get-by-id`) sidestep this.
- **Long text fails with a 414, and the budget is measured *URL-encoded*.** The CLI puts text in the
  request URL, so `card:create`, `card:update` **and `card:comment`** all reject long bodies with
  `AxiosError: Request failed with status code 414`. Measured on the sibling Milk Lab Creations
  board: **~10,400 encoded characters succeeds, ~10,700 fails.** Treat ~10,400 as the ceiling here
  too — the limit is the URL, not the board. Long card titles eat into the same budget.

  Raw byte count is a misleading proxy, because every non-ASCII character costs 3× — an em-dash (`—`)
  is 9 encoded characters against 1 for a hyphen, so 30-odd em-dashes alone are ~250 characters of
  budget. Before a long write, measure the real number:

  ```bash
  python3 -c "import urllib.parse,sys;s=open('body.md').read();print(len(urllib.parse.quote(s)))"
  ```

  Practical rules:

  - Pass long bodies from a file (`--description "$(cat body.md)"`) so you can measure and edit them.
  - **Prefer ASCII punctuation** in card bodies — `-` over `—`/`–`, `->` over `→`. Cheapest win
    available, and worth doing pre-emptively on anything long.
  - Keep a map card's body an **index** — gists plus links — with detail in child cards and comments.
    A map that has accumulated many decisions will sit near the ceiling, so **budget for it**: pay for
    a new Decisions-so-far entry by tightening older gists, whose detail is already in their tickets.
  - **Split long resolutions across several comments** ("part 1 of 2", …). Post them **in order**:
    comments render newest-first, and a failed part you re-split and re-post lands *after* the parts
    that already succeeded.
  - **There is no way to delete or edit a comment from the CLI** (`card:comment` and `card:comments`
    are the only comment verbs), so a mis-posted comment is permanent. Measure before posting.
- **`--format json` names the body `description`, not `desc`.** The Trello REST API calls this field
  `desc`, so reaching for `d['desc']` is the natural mistake and yields a bare `KeyError`. To pull a
  card's body for editing:

  ```bash
  trello card:get-by-id --id <card-id> --format json \
    | python3 -c "import sys,json;print(json.load(sys.stdin)['description'],end='')" > body.md
  ```

- **Mutations succeed silently.** `card:move`, `card:update`, `card:assign` and `card:label` print
  **nothing** on success — indistinguishable from a no-op. Confirm with a `card:get-by-id` or a
  `card:list` of the destination list rather than assuming the write landed.
- **The name→ID cache is local SQLite** (`~/.trello-cli/default/trello.db`). If a board or list was
  renamed and the CLI reports it "not found" even though `board:list` shows it, the cache is stale —
  run `trello sync`.
- **Auth failures are the user's to fix.** Credentials live in `~/.trello-cli/`. If a command fails
  with an auth error, ask the user to re-authenticate; never attempt to fetch tokens yourself.
