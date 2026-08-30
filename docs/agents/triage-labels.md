# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual
label strings used on the **Robo Cat Ears** Trello board.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage`       | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info`         | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent`    | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label
string from this table.

Edit the right-hand column to match whatever vocabulary you actually use.

## Board notes

- Only **`ready-for-agent`** (lime) and **`needs-triage`** (yellow) exist on the board. The other
  three are created on first use:
  `trello label:create --board "Robo Cat Ears" -n "needs-info" --color <color>`
- Apply with `trello card:label --board "Robo Cat Ears" --list "<current list>" --card "<title>" --label "<label>"`.
- Remove with `trello card:unlabel --board "Robo Cat Ears" --list "<current list>" --card "<title>" --label "<label>"`.
  Since the five roles are mutually exclusive, a transition is an `unlabel` of the old role followed
  by a `label` of the new one — the Backlog / Todo / In Progress / Ready for Review / Done position
  still carries workflow state, but a card should never wear two triage labels at once.
- `wontfix` is the one role where the label matters more than the list — a wontfix card should carry
  the label *and* move to Done, so Todo stays a true queue.
- `ready-for-human` deserves real use on this board. A large share of the cards are PCB, battery,
  fur, and 3D-print work that no agent can do; `ready-for-agent` should be reserved for cards whose
  whole deliverable is code.
- For a watch card, "code" still ends on hardware. Nothing in this repo builds off-target, so an
  agent can carry a card to a clean `idf.py build` but not to a verified one. Say so on the card
  rather than moving it to Done unflashed.
