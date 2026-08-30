# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the
codebase. This repo is **single-context**: one `CONTEXT.md` and one `docs/adr/` at the root would
cover `main/` and the first-party components alike — one vocabulary (ears, screens, services, slots,
animations, lighting, the wire format), not several.

## Before exploring, read these

- **`docs/ble-protocol.md` in [`junderdo/robo-cat-ears`](https://github.com/junderdo/robo-cat-ears)** —
  the wire contract, owned there, and always worth reading before touching
  `components/services/bluetooth_service` or `components/services/animation_store_service`
- **`CONTEXT.md`** at the repo root
- **`docs/adr/`** — read ADRs that touch the area you're about to work in

If `CONTEXT.md` or `docs/adr/` don't exist, **proceed silently**. Don't flag their absence; don't
suggest creating them upfront. The `/domain-modeling` skill (reached via `/grill-with-docs` and
`/improve-codebase-architecture`) creates them lazily when terms or decisions actually get resolved.

As of this writing neither exists yet — that is expected, not a gap to fix. The protocol document
does exist, in another repo, and is not optional.

## File structure

```
/
├── CONTEXT.md
├── docs/
│   ├── adr/
│   │   ├── 0001-....md
│   │   └── 0002-....md
│   ├── agents/           ← this directory: skill configuration
│   ├── research/         ← findings from /research
│   └── spec/             ← specs / PRDs from /to-spec
├── main/                 ← boot, dark stylesheet, system status
└── components/
    ├── brookesia_app_*/  ← the apps; one .cpp/.hpp pair per screen
    ├── services/         ← one component per domain service
    └── brookesia_core/, XPowersLib/   ← vendored third party
```

`docs/research/` and `docs/spec/` are established homes for long-form output and are not ADRs — an
ADR records a decision and its consequences, a spec describes what to build, research captures what
was learned. Keep them apart.

A component-level `README.md` (as `lighting_service` and `calibration_service` have) is a different
thing again: it documents how to use that component, and it is not where a decision or a constraint
should end up.

## The three have different lifespans

This is what decides where a fact goes, more than what the fact is about.

A **spec is fulfilled**, not superseded — it describes work that gets done, and once the code exists
the code is the record of _what_. A spec keeps its reasoning about the things code cannot show (why
this shape and not that one), and it stops being edited. An **ADR outlives the build**: it is read
cold, years later, by someone who never saw the spec and is about to change something. **Research is a
dated snapshot** of what the sources said; it is never revised to stay true, only cited.

So: if a fact must still be true and findable after the feature ships — a constraint, a one-way door, a
posture — it belongs in an ADR, even when it was discovered while writing a spec. Lift it out and leave
a pointer. Don't duplicate: two copies of an argument drift, and the stale one still reads
authoritatively.

**Specs carry a `Status:` line** under the title, in the same position and idiom as an ADR's:

| Status        | Means                                                                       |
| ------------- | --------------------------------------------------------------------------- |
| `in assembly` | Still being written; sections are landing as questions get settled          |
| `settled`     | Every question answered, nothing open — but not built yet                   |
| `built`       | Shipped. The argument is over; go read the code, come back only for the why |

Never mark a spec `superseded`. That word is a truth claim and belongs to ADRs, where a later decision
genuinely contradicts an earlier one.

## Across a repository boundary, duplicate the warning and point for the rest

Within a repo a link is strong — same history, one grep away. Across repos it is weak, and the reader
most likely to break a cross-repo invariant is the one least likely to follow the link.

**This repo is the most exposed of the three.** The domain spans `robo-cat-ears` (the firmware and
GATT server, which owns `docs/ble-protocol.md`), `milk-lab-creations` (the web authoring app, and the
home of the ADRs and specs), and this watch — which owns none of that documentation and is a client
of all of it. Nearly every invariant this code must hold is written down somewhere else.

That is why the load-bearing ones are restated in `CLAUDE.md` rather than linked: the watch owns no
animations, the store protocol version is exact-equality-then-disconnect, and reply length guards are
minimums so the ears can append. When you learn a new one, write it where the person about to break
it will be looking — in the header next to the constant, or in `CLAUDE.md` — and point at the
contract for the rest.

Two comments in `animation_store_service.hpp` are the model: the version constant says what it
refuses *and why degrading would be guessing*, and `StoredAnimation` records that the wire also
carries the web app's animation id which this client has no use for. Both facts live in the other
repos; both are needed here.

## Use the glossary's vocabulary

When your output names a domain concept (in a card title, a refactor proposal, a hypothesis, a test
name), use the term as defined in `CONTEXT.md` — and, for anything on the wire, the term
`docs/ble-protocol.md` uses. Don't drift to synonyms: a *slot* is not a "preset", and the watch
*plays* animations rather than owning them.

If the concept you need isn't in the glossary yet, that's a signal — either you're inventing language
the project doesn't use (reconsider) or there's a real gap (note it for `/domain-modeling`).

## Flag ADR conflicts

If your output contradicts an existing ADR — they live in `milk-lab-creations` — surface it
explicitly rather than silently overriding:

> _Contradicts ADR-0002 (how a pair of ears is identified) — but worth reopening because…_
