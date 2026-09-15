# Cataclysm 4.3.4 server-side Playerbots port plan

**Research baseline:** `playerbots-434` at `aa47817dfbe27ff2a5cda0fbc405705e2f64e197` (WoW 4.3.4.15595).
**Scope:** architecture only; no C++, SQL, runtime, or Windows-build claims. Repository links and reference commits were checked on 2026-09-15. Commit pins are evidence, not proposed submodules.

## 1. Executive summary

Choose **D, a hybrid**, delivered vertically. Use this TCPP branch's own Cataclysm login/map/session mechanics as the lifecycle authority; use the GPL-2.0 `mod-playerbots`/ike3 behavior vocabulary, action/trigger ideas, throttling lessons, and tests selectively. Do **not** transplant AzerothCore core APIs or begin by importing its large strategy tree. The first patch should prove one genuine, database-backed Warrior can load without a socket, enter a normal map, tick, perform one `MotionMaster::MovePoint`, stop, and cleanly save/logout.

The closest expansion reference found is the CMaNGOS Cataclysm implementation [`jacatinelord/mangos3-playerbot`](https://github.com/jacatinelord/mangos3-playerbot/tree/bbf682135fb173bcc0afc0e55d65b15b27dd8aab/src/modules/Bots/playerbot), but its CMaNGOS lifecycle is not ABI/API-compatible with TCPP. It is the **primary behavioral lineage**, paired with TCPP's native `WorldSession`/`Player` login code as the primary lifecycle source. Modern [`mod-playerbots`](https://github.com/mod-playerbots/mod-playerbots/tree/b6696bdbd3740e575598d167d69f39f68cc0b907) is the secondary architecture/scaling reference. This combination minimizes core edits and avoids mistaking ArkCORE's NPC companions for player characters.

## 2. Repositories/source lineages researched

| Candidate (exact revision researched) | What it actually is | Useful paths | Finding |
|---|---|---|---|
| [`mod-playerbots/mod-playerbots`, `master`, `b6696bdb`](https://github.com/mod-playerbots/mod-playerbots/tree/b6696bdbd3740e575598d167d69f39f68cc0b907) | AzerothCore 3.3.5 module; autonomous real `Player` characters | `src/Bot/PlayerbotMgr.cpp`, `RandomPlayerbotMgr.cpp`, `PlayerbotAI.cpp`, `Engine/`, `Strategy/`, `Travel/`, `src/Db/` | Best maintained concepts; core and expansion mismatch is large. |
| [`jacatinelord/mangos3-playerbot`, `master`, `bbf68213`](https://github.com/jacatinelord/mangos3-playerbot/tree/bbf682135fb173bcc0afc0e55d65b15b27dd8aab/src/modules/Bots/playerbot) | CMaNGOS Cataclysm autonomous real-character Playerbots, descended from ike3 | `PlayerbotAI.*`, `PlayerbotMgr.*`, `RandomPlayerbotMgr.*`, `strategy/`, `TravelMgr.*`, `PlayerbotFactory.*` | Closest gameplay/DBC-era reference, not a drop-in Trinity port. |
| [`normalzero/LegionPlayerBot`, `main`, `32e6abf0`](https://github.com/normalzero/LegionPlayerBot/tree/32e6abf03b6d4c55080a70d4110d14450c9d4447) | Trinity-derived Legion server-side real-player bot fork | `src/server/game/Server/PlayerBotSession.*`, `src/server/game/PlayerBot/PlayerBotMgr.*`, `FieldBotMgr.*`, class AIs | Valuable fake-session precedent, but Legion signatures/opcodes/data and unclear top-level licensing make copying unsafe. |
| [`Arkania/ArkCORE-NG`, `master`, `a7304c30`](https://github.com/Arkania/ArkCORE-NG/tree/a7304c3075bf8ee7eb5bdf45f31cda01c8626b52) | Cataclysm Trinity fork containing **NPCBot companions**, not target Playerbots | `src/server/game/AI/NpcBots/`, `README.PlayerBot`, `sql/updates/auth/playerbot/auth_playerbot.sql` (permissions are named `npcbot`) | Reject as lineage. Useful only for Cata combat API comparison. |
| [`SinglePlayerProject` repositories](https://github.com/orgs/SinglePlayerProject/repositories) and [`celguar/spp-classics`](https://github.com/celguar/spp-classics) | Packaging/forks spanning AzerothCore, CMaNGOS, and newer cores | Repository manifests/modules must be inspected individually | “SPP” is distribution branding, not proof of Playerbots. No verified Cata-SPP autonomous real-character implementation was found. |
| [`celguar/playerbots`](https://github.com/celguar/playerbots) / older ike3 ecosystem | Historical action-trigger-value Playerbot lineage | `playerbot/strategy`, managers, factories | Important genealogy; prefer its maintained descendants and preserve author history. |
| Trinity/ArkCORE historical Cataclysm forks | Core forks, often labeled “playerbot” while shipping `NpcBots` | search concrete class and session ownership, not repository title | No verified close Trinity 4.3.4 real-player implementation strong enough to resurrect directly. |

Classification rule used: a target implementation must instantiate/load a legitimate `Player`, own a server-side session/lifetime, insert that player into maps/object registries, and autonomously act. A `Creature` follower (`NpcBots`), scripted creature AI, client/multibox process, or population-only fake-online row is not equivalent.

## 3. Genealogy and provenance

The useful genealogy is broadly **early MaNGOS/ike3 Playerbot → CMaNGOS playerbot branches → AzerothCore module (`mod-playerbots`)**. The recognizable manager/factory plus action-trigger-value strategy system persists, but every generation is coupled to its host core. `mangos3-playerbot` carries that lineage into Cataclysm-era data/gameplay. `LegionPlayerBot` is a separate Trinity-derived later-expansion implementation with a `PlayerBotSession` subclass; it is a corroborating design, not demonstrated ancestry of mod-playerbots.

ArkCORE-NG's `AI/NpcBots` derives companion creatures controlled by a player; its README commands `.npcbot add/follow/stay`. Cata-SPP claims must therefore be verified at code level. Packaging a Cata core with NPCBots or an external launcher does not establish autonomous player characters.

## 4. Candidate comparison

| Strategy | Core invasiveness | Incremental lifecycle proof | Long-term autonomy | Risk | Decision |
|---|---:|---:|---:|---:|---|
| A: resurrect alleged old Trinity Cata patch | potentially low | uncertain | uncertain/abandoned | provenance and completeness unknown | Reject until a pinned, auditable real-player patch appears. |
| B: backport Legion/newer Trinity fork | high | medium | good | session, packets, spells, movement, data all diverged | Secondary reference only. |
| C: transplant modern AzerothCore module | very high | poor | excellent | thousands of assumptions encourage rewriting TCPP | Reject wholesale port. |
| D: TCPP lifecycle + Cata lineage concepts | **low** | **excellent** | excellent | deliberate adapters required | **Recommended.** |

Feature count is deliberately not the deciding metric. A small lifecycle seam tested against native TCPP is more valuable for preservation QA than a large compile-only import.

## 5. Recommended primary source lineage

Use `mangos3-playerbot@bbf68213` as the primary **behavioral/Cataclysm semantics** lineage, especially `PlayerbotAI`, `PlayerbotMgr`, `RandomPlayerbotMgr`, `PlayerbotFactory`, `TravelMgr`, and `strategy/`. Reimplement against TCPP types; do not mechanically copy its host-core glue. For lifecycle, the authoritative source is this repository's `WorldSession` constructor/update/logout plus `CharacterHandler.cpp` login flow and `Player::LoadFromDB`.

The phase-one design should not include its random population, factory, strategy engine, or custom tables. That restraint makes later replacement or selective import possible.

## 6. Secondary reference sources

* Modern mod-playerbots: action/trigger/value separation; per-bot delay; random population manager; performance monitoring; travel and maintenance behavior. Its lifecycle creates a socketless `WorldSession` in `src/Bot/PlayerbotMgr.cpp`, and `PlayerbotAI::UpdateAI` gates work before `UpdateAIInternal`.
* LegionPlayerBot: demonstrates a Trinity-derived `PlayerBotSession` subclass and manager update path. Treat all Legion session constructor parameters, packet handlers, and class rotations as nonportable.
* ArkCORE-NG: only for comparing Cataclysm spell/combat calls—not lifecycle or object model.
* TCPP handlers: behavioral invariants for quest, loot, NPC, taxi, gossip, and death operations. Bots should call domain methods/adapters preserving these validations rather than synthesize arbitrary client packets by default.

## 7. Rejected approaches and why

1. **NPCBot conversion:** a `Creature` cannot naturally provide character DB persistence, quests, inventory, player groups, taxi, or authentic QA coverage.
2. **CataGhost integration/client automation:** explicitly out of scope; it remains a later black-box validator.
3. **Fake online counters/database rows:** no world object, pathing, combat, or quest coverage.
4. **Packet-loopback as the core AI interface:** brittle opcode serialization and anti-cheat/session state obscure server-authoritative intent. Use narrow services/domain calls, while preserving the same validations.
5. **Wholesale AzerothCore compatibility layer:** would alter TCPP to fit the module, increasing regression surface.
6. **Immediate random population/class strategy import:** lifecycle failures would be buried under scale and AI complexity.

## 8. Exact TCPP integration map

| Concern | Exact TCPP location/symbol | Intended use / constraint |
|---|---|---|
| World ownership/update | `src/server/game/World/World.h`; `World::AddSession`, `World::Update` in `World.cpp` | Manager ownership and one bounded manager tick; never block DB work here. |
| Session lifecycle | `src/server/game/Server/WorldSession.h/.cpp`; constructor, `Update(uint32, PacketFilter&)`, `LogoutPlayer(bool)` | Socketless session must tolerate absent `WorldSocket`; determine whether world session queues are required. |
| Canonical login | `src/server/game/Handlers/CharacterHandler.cpp`; character query holder/login completion and `Map::AddPlayerToMap` near login path | Extract/reuse the smallest safe internal login operation rather than duplicating its ordering. |
| Player persistence | `src/server/game/Entities/Player/Player.h/.cpp`; `LoadFromDB`, `SaveToDB`, `ResurrectPlayer`, `RepopAtGraveyard` | Real account ownership is validated by `LoadFromDB`; logout is preferred save owner. |
| Unit/combat | `src/server/game/Entities/Unit/Unit.h/.cpp`; attack state, `CastSpell`; `Creature.h/.cpp` | Milestone 2 uses normal hostility/threat/combat, never NPC shortcuts. |
| Object lookup | `src/server/game/Globals/ObjectAccessor.h/.cpp`; `FindPlayer`, `FindPlayerByName`, typed lookups | Bots remain normal registered world objects. |
| Static data | `src/server/game/Globals/ObjectMgr.h/.cpp`; quest/item/creature templates | Read-only discovery; no custom gameplay data in phase one. |
| Accounts | `src/server/game/Accounts/AccountMgr.h/.cpp`; account lookup/security | Dedicated normal test account; never bypass ownership/security checks. |
| Databases | `src/server/database/Database/Implementation/{LoginDatabase,CharacterDatabase}.{h,cpp}` and `DatabaseEnv.*` | Prepared/async query holders; phase one adds no schema. |
| Maps | `src/server/game/Maps/Map.h/.cpp` `Map::AddPlayerToMap`; `MapManager.h/.cpp` | Reuse canonical login map creation/insertion, map-thread rules, removal ordering. |
| Movement | `src/server/game/Movement/MotionMaster.h/.cpp` `MovePoint`, `MoveChase`, `MoveFollow`, `Clear`; `MovementGenerator.*`; `PathGenerator.h/.cpp`; `MMapFactory.*` | Server-authoritative movement/MMAP. First milestone uses one short reachable point. |
| Groups | `src/server/game/Groups/Group.h/.cpp`; `GroupMgr.h/.cpp` `AddGroup`, `RemoveGroup`, `GetGroupByGUID` | Deferred; use normal membership/invite rules. |
| Quests | `src/server/game/Quests/QuestDef.h`; `Player::CanTakeQuest/AddQuest/CompleteQuest/RewardQuest`; `Handlers/QuestHandler.cpp` accept/reward handlers | Milestone 3 adapter must preserve giver relation, requirements, objective credit, and reward validation. |
| Spells/cooldowns | `src/server/game/Spells/Spell.h/.cpp`, `SpellMgr.h/.cpp`; cooldown members/APIs in `Player.h/.cpp` | This branch has no modern standalone `SpellHistory` class; adapt Cata `Player` cooldown APIs. |
| Loot/items | `src/server/game/Loot/Loot.h/.cpp`; `Handlers/LootHandler.cpp`; `Entities/Item/Item.h/.cpp`, `Player` inventory methods | Deferred maintenance; preserve ownership, bag, and roll checks. |
| Gossip/vendor/trainer | `Entities/Creature/GossipDef.*`; `Handlers/GossipHandler.cpp`, `ItemHandler.cpp::HandleListInventoryOpcode`, `NPCHandler.cpp::HandleTrainerListOpcode` | Build server-side adapters around validated interactions; do not assume modern helpers. |
| Taxi/travel | `Handlers/TaxiHandler.cpp` (`HandleActivateTaxiOpcode`, express variant); Player taxi state | Deferred and stateful; packet APIs are not an AI contract. |
| Death | `Handlers/MiscHandler.cpp::HandleRepopRequestOpcode`; `Player::RepopAtGraveyard`, `ResurrectPlayer` | Milestone 2 recovery uses existing state transitions. |
| Battlegrounds | `src/server/game/Battlegrounds/Battleground*.{h,cpp}` and `BattleGroundHandler.cpp` | Optional future hook; queues/map ownership make this high risk. |
| Commands | `src/server/game/Chat/Chat.h`, `ChatCommands/`; `src/server/scripts/Commands/` | Add an administrator-only command script later; phase-one lifecycle may expose start/status/stop. |
| Configuration | `World::LoadConfigSettings` in `World.cpp`; `src/server/worldserver/worldserver.conf.dist` | Parse `Playerbots.Enable`, account/character allowlist, tick budget; default disabled. |
| DB migrations | `sql/updates/{auth,characters,world}` conventions and database loaders | No schema through milestone 1; later migrations must be explicit and reversible. |

## 9. Compatibility matrix (preferred lineage to TCPP)

“Reference” below means `mangos3-playerbot@bbf68213` unless noted.

| Reference API | Reference source/path | TCPP equivalent | TCPP source/path | Compatibility | Notes / real cost |
|---|---|---|---|---|---|
| bot session construction/login | `PlayerbotMgr.*` | `WorldSession::WorldSession`; character login flow | `Server/WorldSession.*`, `Handlers/CharacterHandler.cpp` | major adaptation | Different CMaNGOS session/query-holder ownership. Highest-risk seam. |
| `Player::LoadFromDB` | manager/login glue | `Player::LoadFromDB(ObjectGuid, CharacterDatabaseQueryHolder const&)` | `Entities/Player/Player.cpp` | major adaptation | TCPP requires populated holder and matching account. |
| add player to world | manager login | `Map::AddPlayerToMap(Player*)` after canonical initialization | `Maps/Map.cpp`, `CharacterHandler.cpp` | minor adaptation | Call order/phasing/social/online flags matter. |
| logout/save | manager holder logout | `WorldSession::LogoutPlayer(bool)` / `Player::SaveToDB` | `Server/WorldSession.cpp`, `Entities/Player/Player.cpp` | minor adaptation | Session should own teardown; never double-delete. |
| AI attachment | `PlayerbotAI.*`, manager map | external manager `BotRuntime` keyed by GUID | new subtree + one world hook | major adaptation | Avoid adding AI pointer to `Player` initially. |
| throttled update | `PlayerbotAI::UpdateAI`, `RandomPlayerbotMgr` | bounded call from `World::Update` | `World/World.cpp` | minor adaptation | Time budget and round-robin queue required. |
| `MotionMaster` movement | actions/values | `MotionMaster::MovePoint/MoveChase/MoveFollow/Clear` | `Movement/MotionMaster.*` | minor adaptation | Signatures and completion observation differ. |
| `PathFinder`/travel | movement/travel actions, `TravelMgr.*` | `PathGenerator`, MMAP | `Movement/PathGenerator.*`, `MMapFactory.*` | major adaptation | Nav availability, partial paths, transports, phases. |
| unit lookup | values/targeting | `ObjectAccessor` typed find functions | `Globals/ObjectAccessor.*` | minor adaptation | GUID representation differs. |
| spell metadata/cast | class actions | `sSpellMgr`, `Unit::CastSpell`, `Spell` | `Spells/SpellMgr.*`, `Spell.*`, `Entities/Unit/*` | major adaptation | Cataclysm power, facing, GCD and cooldowns; no modern `SpellHistory`. |
| cooldown queries | spell actions | Player spell/cooldown APIs | `Entities/Player/Player.*` | major adaptation | Reference abstractions cannot be copied. |
| quest selection/actions | `strategy/actions/QuestAction.*`, values | `Quest`, ObjectMgr templates, Player quest APIs | `Quests/QuestDef.h`, `Player.*`, `Handlers/QuestHandler.cpp` | major adaptation | Preserve giver/objective/reward validation and Cata phasing. |
| loot/inventory | loot actions/values | Loot + Player inventory + Item | `Loot/*`, `Handlers/LootHandler.cpp`, `Entities/Item/*` | major adaptation | Packet-driven reference paths and item layouts differ. |
| gossip/vendor/trainer | interaction actions | TCPP handler/domain logic | `Handlers/{Gossip,Item,NPC}Handler.cpp` | major adaptation | Create narrow validated services; avoid fabricated packets. |
| Group APIs | group actions | `Group`, `GroupMgr` | `Groups/*` | minor adaptation | GUID/role and invite semantics differ. |
| Battleground APIs | BG strategies | TCPP battleground manager/handlers | `Battlegrounds/*`, `Handlers/BattleGroundHandler.cpp` | major adaptation | Defer until world loop stable. |
| config API | `PlayerbotAIConfig.*`, module config | `World::LoadConfigSettings`, worldserver config | `World/World.cpp`, `worldserver/worldserver.conf.dist` | major adaptation | Do not import AzerothCore module config API. |
| bot DB repositories | `PlayerbotDbStore.*` / modern `src/Db/*` | Character/Login DB prepared statements | `server/database/Database/Implementation/*` | major adaptation | Start schema-free; async reads off hot path. |
| command framework | bot command handlers | Trinity Chat command scripts | `game/Chat/*`, `scripts/Commands/*` | major adaptation | RBAC/API shapes differ. |
| random population | `RandomPlayerbotMgr.*` | absent | new manager | absent | Postpone until lifecycle/load metrics exist. |
| strategy engine | `strategy/*` | absent | new subtree | absent | Selectively reimplement after milestone 2, not a core hook. |

## 10. Proposed Playerbots directory architecture

```text
src/server/game/Playerbots/
  PlayerbotMgr.{h,cpp}              # sole owner/orchestrator; GUID -> runtime
  PlayerbotRuntime.{h,cpp}          # session/player lifecycle state machine
  PlayerbotAI.{h,cpp}               # thin scheduler-facing controller
  Lifecycle/PlayerbotLogin.{h,cpp}  # async character query + canonical completion adapter
  Movement/MovementController.*     # intents, result/stuck observation
  Combat/CombatController.*         # milestone 2
  Quest/QuestController.*           # milestone 3
  Strategy/                         # later action/trigger/value concepts
  Telemetry/PlayerbotEvent.*
  Telemetry/PlayerbotTelemetry.*
  PlayerbotConfig.*
  CMakeLists.txt
src/server/scripts/Commands/cs_playerbots.cpp  # admin surface only
```

Use namespace `Playerbots`. New code depends on TCPP; TCPP must not depend on behavior subclasses. Keep protocol emulation behind an optional adapter, not throughout strategies. No new third-party dependencies.

## 11. Minimum required TCPP hooks

Only these are unavoidable for milestone 1:

1. **Build registration:** add the new directory to the existing game CMake source collection.
2. **One lifecycle/update owner:** construct/shut down `PlayerbotMgr` with `World`, and invoke `PlayerbotMgr::Update(diff, budget)` at a defined point in `World::Update`. Default-disabled means a near-zero branch when unused.
3. **Canonical internal login seam:** refactor only enough of `CharacterHandler.cpp`'s successful login completion to accept a server-originated, already-authorized bot request and the existing query holder. It must remain the same path for clients and bots. If safe reuse is possible without refactoring, prefer it.
4. **Socketless safety:** audit `WorldSession` constructor, send/update/logout paths. Add narrowly named checks only where an unconditional socket dereference is proven. Do not broadly teach gameplay code about bots.
5. **Admin command/config:** allowlist-driven start/status/stop, default disabled. This is control plane, not gameplay behavior.

Do **not** add `PlayerbotAI*` to `Player`, change `Unit`/`Creature`, or hook every handler in milestone 1. Optional later seams are validated interaction services, map-thread event delivery, BG queue support, and QA exporters.

## 12. Session/login lifecycle design

State machine: `Requested → Querying → ConstructingSession → LoadingPlayer → EnteringWorld → Online → LoggingOut → Stopped/Failed`.

* Select an explicitly configured account ID and character GUID. Require normal account/character rows, `SEC_PLAYER`, correct expansion, not banned, not already online, and no live session for the account. Never auto-create in phase one.
* Submit the same character login query holder used by `CharacterHandler.cpp`. Completion returns to the owning world context; do not wait synchronously inside `World::Update`.
* Create a socketless bot session with the exact TCPP constructor signature. Ownership must be explicit (`World` session store or `PlayerbotRuntime`, never both). Before implementation, trace destructor and `LogoutPlayer` deletion behavior and document it in code.
* Allocate `Player`, attach session, call `LoadFromDB`, then perform the canonical post-load sequence from `CharacterHandler.cpp`, including map creation/insertion and online state. No hand-written subset unless every skipped operation is justified.
* Online ticks resolve the player by GUID and never retain an unvalidated raw pointer across removal/map transfer boundaries.
* Stop cancels pending AI intent, clears `MotionMaster`, calls `WorldSession::LogoutPlayer(true)`, allows its normal map removal/save/online cleanup, then destroys the session exactly once. Shutdown drains bots before database pools stop.
* Treat missing socket output as discardable only for messages genuinely destined for a client. Server state transitions must still occur.

Open design decision for the next task: whether bot sessions enter `World::_sessions` and normal `WorldSession::Update`, or are owned separately. Decide from a call/ownership audit, not convenience. Separate ownership is preferred only if normal session invariants can still be met.

## 13. AI update design

The manager receives `diff` once per world update but runs bots round-robin on `steady_clock` deadlines. Phase-one cadence is 500–1000 ms and a small configurable wall-time budget. Each due bot performs one bounded state-machine step; no DB query, path computation loop, or retry loop may block the world tick. Async completions enqueue results.

`PlayerbotAI` is attached in `PlayerbotRuntime`, not `Player`. Events are compact facts (`EnteredWorld`, `MovementComplete`, `Died`, `QuestCredit`) and are consumed on the owning update context. Use deadline/backoff and per-action retry caps. Instrument queue delay, action duration, skipped ticks, and manager budget overruns. Path requests and map-affecting actions obey existing map-thread rules; this requires explicit validation before scale.

## 14. Database and configuration strategy

Milestone 1 adds **no tables and no migrations**. Normal LoginDatabase accounts and CharacterDatabase characters are the source of truth. Configuration:

```ini
Playerbots.Enable = 0
Playerbots.AccountIds = ""        # explicit QA-only allowlist
Playerbots.CharacterGuids = ""    # explicit allowlist
Playerbots.UpdateIntervalMS = 500
Playerbots.UpdateBudgetMS = 2
Playerbots.TelemetryLog = 1
```

Names are proposals subject to the configuration convention audit. Reject characters outside both allowlists. Never store passwords. Later population metadata belongs in narrowly scoped CharacterDatabase tables only after requirements stabilize; prepared statements are added to the corresponding `CharacterDatabase` enum/implementation and migrations follow this repository's updater layout. Telemetry begins in structured logs; a database sink must be asynchronous/batched and optional.

## 15. First three implementation milestones

### Milestone 1 — one live Warrior lifecycle proof

Precondition: a human creates a dedicated low-privilege QA account and legitimate Warrior through supported tools/client once, logs it out in a safe outdoor location, records account ID/GUID in the allowlist, and backs up the character DB.

Acceptance sequence: enable module; administrator requests that GUID; same native query holder loads it; bot appears through `ObjectAccessor`, enters its expected map, and logs GUID/map/zone/area/XYZ every 30 seconds for at least five minutes. It receives at least ten AI ticks, commands one short nav-valid `MovePoint`, observes arrival or timeout, calls `MotionMaster::Clear`, remains online, then admin stop/server shutdown executes `LogoutPlayer(true)`. Restart/reload verifies saved position and database online flag is clear. Test missing map, already-online character, disabled feature, invalid account ownership, load failure, and shutdown during query. No combat, quest, random manager, or schema.

This is **not claimed live-verified** by this research task.

### Milestone 2 — crowded-world movement/combat/death

Add bounded roaming destinations based on reachable nearby points, path result/stuck timeout/repath, hostile selection using normal visibility/hostility and accessibility, facing/range/chase, Warrior autoattack plus one known melee ability through normal spell checks, attackers/threat reevaluation, and leash/retreat rules. Test in an ordinary spawn-dense leveling area where two or more creatures can aggro; record primary/add GUIDs and outcomes. On death, detect final death state, release/repop through normal transitions, navigate corpse/graveyard as supported, resurrect, and resume only after state is consistent. Acceptance includes wins, an add, a death/recovery, path failure, no-target idling, and clean logout while in/recently out of combat.

### Milestone 3 — authoritative basic quest loop

Choose one known simple kill quest with optional reward-choice coverage. Discover a reachable giver and eligible quest via ObjectMgr/Player validation; interact in range; call an adapter preserving the validations in `QuestHandler.cpp`; accept; derive creature objective; kill through milestone-2 combat; rely on normal server kill credit (never mutate counters); detect completion; return; validate giver/completion; choose a valid reward using inventory/usefulness fallback; reward through normal domain logic. Verify quest log, authoritative credit, item/money/XP, persistence after logout, full bags, unavailable giver, competing players, death, and duplicate interaction.

## 16. Build and test strategy

This document changes no runnable code. For implementation patches:

1. Linux CI: configure/build changed targets with warnings, unit tests where seams permit, formatting/static checks already used by TCPP.
2. Deterministic lifecycle tests: disabled no-op; allowlist; query failure; login/logout ordering; double-stop; shutdown race; socketless sends; database online flag.
3. Integration realm: fresh DB snapshot, one bot, timestamped logs, five-minute soak, restart persistence; then crowded milestone scenarios. Never call compile success “live verification.”
4. Human authority: Windows 10, VS Community 2026, CMake 4.4.2, MSVC 19.51, Boost 1.83.0, OpenSSL 3.6.3, MySQL 8.4.11. Preserve dependencies and require the human Windows build before merge.
5. At scale, staged 1/10/50/100/target bot soaks with baseline comparisons for world update percentiles, map tick, RSS, DB query rate/latency, path request rate, and clean shutdown time.

## 17. QA/preservation telemetry architecture

`Telemetry/PlayerbotEvent` is a stable internal envelope emitted by lifecycle and controllers to `PlayerbotTelemetry`; sinks initially write a dedicated structured log category. Strategies never write SQL. Required fields are nullable where irrelevant:

`timestamp`, `account`, `character`, `player_guid`, `map`, `zone`, `area`, `x/y/z/o`, `quest_id`, `objective`, `target_guid`, `target_entry`, `current_action`, `previous_successful_action`, `movement_result`, `combat_result`, `quest_result`, `retry_count`, plus `event_id`, bot build/core commit, state, duration, and correlation ID.

Failure category enum: `BOT_AI`, `SERVER_CPP`, `DATABASE_SQL`, `SCRIPT`, `PATHING`, `DATA`, `UNKNOWN`. Classification is an observation/hypothesis, never an automatic blame assertion. Emit action start/end/failure, state transition, stuck/repath, target invalidation, unexpected server result, quest progress mismatch, death/recovery, login/logout, and budget overrun. Rate-limit repeated identical failures and produce an aggregate count on suppression. Redact credentials/chat/private data. A later JSON-lines/file or asynchronous DB/export sink can subscribe without changing AI.

## 18. Scaling considerations

* Round-robin deadlines with jitter prevent synchronized spikes; separate idle, travel, combat, and critical-state cadences.
* Enforce global and per-map time/action/path budgets; cap concurrent logins and async DB queries.
* Cache immutable template-derived decisions by ID/revision, never mutable `Player`/`WorldObject*` across ticks.
* Population manager is separate from per-bot AI: desired counts, eligible allowlisted pool, login/logout rate, map/level distribution, and maintenance windows.
* Prefer pre-created QA accounts/characters and one active character per account until account-session semantics are proven. Hundreds of accounts have auth/DB operational cost; multiple simultaneous characters per account may violate core assumptions.
* Batch/rate-limit telemetry and saves; avoid save-on-every-action. Measure pathfinding and grid activation, likely dominant costs in spread-out populations.
* Apply overload shedding: postpone low-priority perception/travel, never skip lifecycle teardown or death consistency.

## 19. Risks and unknowns

| Risk | Severity | Required investigation/mitigation |
|---|---:|---|
| Socketless `WorldSession` invariants and ownership/deletion | critical | Trace constructor through `AddSession`, update filters, sends, logout, destructor; ASan lifecycle test on Linux. |
| Canonical login is packet-handler-shaped/asynchronous | critical | Extract minimal shared completion or invoke safe internal seam; compare every side effect in `CharacterHandler.cpp`. |
| World thread vs map update thread | critical | Prove which context may mutate Player/MotionMaster; queue work rather than cross-thread calls. |
| CMaNGOS/AzerothCore API divergence | high | Port behavior intent through adapters, never mass search/replace. |
| Cataclysm spell/cooldown/quest/phasing semantics | high | Use TCPP APIs and scripted crowded-world cases; no modern `SpellHistory` assumptions. |
| Null socket sends/anti-cheat/timeouts | high | Inventory unconditional socket use and session timeout/kick paths before milestone 1. |
| Movement completion and player-control assumptions | high | Inspect movement generators/spline acknowledgements; define server-only completion evidence. |
| Logout during async load/map transfer/combat | high | Idempotent state machine, cancellation token, exact single owner. |
| Unknown historical fork provenance | high | Require license file, commit pin, author history, and origin record before copying. |
| QA false attribution | medium | Telemetry separates observation from classification and preserves correlation context. |
| Windows-only compiler differences | medium | Keep standard/library patterns consistent; human MSVC build remains merge gate. |

## 20. Licensing and provenance notes

This repository carries GPL-family TrinityCore provenance (verify the exact root notice/header for every later patch). Licensing review is still required; this is engineering guidance, not legal advice.

| Source | Observed license/provenance | Copy policy |
|---|---|---|
| mod-playerbots | repository `LICENSE`: GPL-2.0; history includes many authors | GPL-to-GPL may be compatible if TCPP's exact license obligations are met. Preserve copyright headers, commit/source URL, revision, and notices; record substantial copied files. |
| mangos3-playerbot | GitHub reports GPL-2.0 and repository includes GPL notice | Same; preserve ike3/CMaNGOS author history. Prefer reimplementation where host-core code is intertwined. |
| LegionPlayerBot | no clear top-level license was observed at researched revision | **Do not copy code.** Architectural observation only until owner/license and inherited Trinity notices are established. |
| ArkCORE-NG | Trinity-derived/GPL codebase, but inspected feature is NPCBots | Not a target source. If any isolated Cata idea is used, verify that file's header/history and attribute it. |
| SPP forks/packages | license varies by component; branding is not provenance | Do not copy from binary bundles or unattributed aggregate patches. Trace each file to its upstream and license. |
| historical snippets/forums | frequently unclear or incomplete | Clean reimplementation from described behavior only, or obtain permission/provenance. |

Every implementation PR should add a provenance ledger section listing source repository URL, branch, full commit, file, authors/header retained, whether code is copied/adapted/reimplemented, and reviewer licensing decision. Architecture and facts are not copied expression, but close translations can still be derivative; do not strip attribution.

## 21. Exact recommended next Codex task

> **Lifecycle seam audit only (documentation, no implementation):** On a new branch based on `playerbots-434`, trace TCPP commit `aa47817dfbe27ff2a5cda0fbc405705e2f64e197` from character-selection packet entry through query-holder preparation/completion, `WorldSession`/`Player` ownership, `Player::LoadFromDB`, map creation and `Map::AddPlayerToMap`, object registration, session/world/map updates, outbound socket use, timeout/kick logic, `LogoutPlayer(true)`, save, map removal, online-flag clearing, destructors, and shutdown/database-pool ordering. Produce `docs/playerbots-434-lifecycle-audit.md` with an exact call graph (file, symbol, line), ownership table, thread/context table, all unconditional socket dereferences, all client acknowledgements required for movement/login, failure unwind paths, and two minimal hook alternatives ranked by invasiveness. Verify the current branch/base/status first; make no C++/SQL/config changes and do not claim runtime or Windows verification.

Only after that audit should an implementation task scaffold the disabled manager and one-Warrior lifecycle proof.
