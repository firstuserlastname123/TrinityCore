# Cataclysm 4.3.4 Playerbots lifecycle-seam audit

## 1. Executive conclusion

**Audit result:** the smallest safe seam is a real `WorldSession`, owned exclusively by `World::m_sessions`, with an explicit socketless/server-originated mode. The bot runtime owns only an account/GUID handle and lifecycle state; it never owns or deletes the session or `Player`. Login should reuse `LoginQueryHolder` and the complete `WorldSession::HandlePlayerLogin(LoginQueryHolder const&)` completion path after server-side authorization. Logout must be `WorldSession::LogoutPlayer(true)`, followed by normal removal of the session by `World::UpdateSessions`.

This is **Alternative A (preferred)**. It preserves the important invariant that `Player::LoadFromDB` attaches the new player to its session, `Map::RemovePlayerFromMap(..., true)` deletes it, and the session merely clears its now-invalid pointer. A separately owned runtime session is a fallback because it duplicates scheduling, callback lifetime, and shutdown responsibilities.

Two socket assumptions require implementation changes for a null-socket session enrolled in `World`: (1) `WorldSession::Update` can unconditionally close the realm socket on idle, and (2) it returns `false` solely because the realm socket is null. Sends, closes, construction, destruction, logging, and Warden are already null-safe or inert. No client acknowledgement is needed for initial world entry or `MotionMaster::MovePoint`; time-sync response is health-neutral. Near/far teleports do require server-side completion, and logout already completes far teleports by calling `HandleMoveWorldportAck()`.

This is a source audit only. It does not establish runtime or Windows behavior.

## 2. Audit baseline

* Requested integration branch/base: `playerbots-434`; protected branch: `cata-preservation`.
* Repository checkout began at exact commit `1dfca11e01967abc85c3e9b08596a1752a3d7861` on the harness working branch `work`. The commit equals the supplied current base commit; no protected branch was checked out or changed.
* Prior plan read first: `docs/playerbots-434-port-plan.md`. Its recommendation to make TCPP lifecycle authoritative is confirmed; no factual correction was made.
* Protocol baseline: Cataclysm 4.3.4.15595. Scope is source/documentation only; no C++, SQL, configuration, CMake, character, runtime, or platform verification.
* Principal source set: `Handlers/CharacterHandler.cpp`, `Entities/Player/Player.{h,cpp}`, `Server/WorldSession.{h,cpp}`, `Server/WorldSocket.cpp`, `World/World.{h,cpp}`, `Maps/Map.cpp`, `Globals/ObjectAccessor.cpp`, `Handlers/MovementHandler.cpp`, `Movement/MotionMaster.cpp`, and `worldserver/Main.cpp`.

## 3. Normal client login call graph

`WorldSocket` authenticates and passes a raw `WorldSession*` to `World::AddSession`; the world-thread queue is drained by `World::UpdateSessions` into `m_sessions[accountId]`. `CMSG_PLAYER_LOGIN` is registered as `STATUS_AUTHED/PROCESS_THREADUNSAFE` in `Server/Protocol/Opcodes.cpp`, so normal dispatch is on the world-session unsafe pass.

| File / symbol | Caller -> callee | Context / ownership | Side effect; socketless requirement / notes |
|---|---|---|---|
| `Server/WorldSocket.cpp`, authenticated socket path | socket auth -> `World::AddSession(_worldSession)` | network produces session; world queue takes it | Establishes world ownership. Bot must enter through `AddSession`, not retain owning pointer. |
| `World/World.cpp`, `World::AddSession`, `AddSession_` | add queue -> `m_sessions` | world update; `World` owns raw pointer | Replaces/kicks same-account session. Required to obtain canonical update/destruction. |
| `Handlers/CharacterHandler.cpp`, `WorldSession::HandlePlayerLoginOpcode` | opcode dispatch -> legitimacy check -> connect handoff | world unsafe session context | Rejects duplicate/loading session; sets `m_playerLoading`; calls `IsLegitCharacterForAccount`, then legacy continuation or sends instance-connect request. Bot needs equivalent explicit authorization, but not client connection handoff. |
| same, `HandleContinuePlayerLogin` | connection continuation -> `LoginQueryHolder::Initialize` -> `CharacterDatabase.DelayQueryHolder` | world unsafe; shared holder moves into DB callback machinery | Sends `ResumeComms` only in split connection mode, schedules holder, captures raw `this`. Bot must reuse holder and completion but skip connection presentation. |
| same, callback lambda | session callback processor -> `HandlePlayerLogin(holder)` | `WorldSession::Update` calls `ProcessQueryCallbacks` | Completion runs in session update, not a DB worker. Required. Loading pin is critical to lifetime. |
| same, `HandlePlayerLogin` | callback -> `new Player(this)` -> `Player::LoadFromDB` | world/session context; temporary raw player | Full canonical completion. Required, ideally factored into a server-originated entry rather than copied. |
| `Entities/Player/Player.cpp`, `Player::LoadFromDB` | login completion -> holder results | same context; player initially locally owned, then session-linked | Validates row/account/ban/identity; loads persistent subsystems; creates/selects map; calls `GetSession()->SetPlayer(this)`. Required wholesale. |
| `CharacterHandler.cpp`, completion | load -> motion init/account data/guild/initial packets | same context; session points to player | Packet sends can be discarded; state initialization, guild setup, first-login mutations cannot. |
| same | completion -> `Map::AddPlayerToMap` -> `ObjectAccessor::AddObject` | world invokes insertion; map thereafter updates player | Adds map membership then global player lookup. Both required in this exact order. |
| same | completion -> online updates/social/corpse/taxi/pets/at-login/achievements/scripts | world/session context | Canonical post-entry state. Required except client-only sends; do not truncate merely because milestone behavior is small. |

`Player::LoadFromDB` loads achievements/criteria, home bind, group/arena, currency, bound instances and lockouts, battleground data, transport/taxi state, skills, talents, spells, glyphs, auras, quests (ordinary/rewarded/daily/weekly/seasonal/monthly), LFG/random-BG state, reputation, inventory/void storage, actions, mail/items, social, cooldowns, declined names, equipment sets, and CUF profiles. It repairs invalid coordinates/transport/taxi/BG state and calls `sMapMgr->CreateMap`. Rest bonus is advanced from offline time. The completion additionally initializes `MotionMaster`, guild state, zone/position data, corpse/death state, taxi continuation, all-pet data and current pet, PvP state, at-login resets/first-login spells, achievements, and scripts.

Account/character protection is layered: character enumeration populates `_legitCharacters`; `HandlePlayerLoginOpcode` checks `IsLegitCharacterForAccount`; and `Player::LoadFromDB` compares the `characters.account` field to `WorldSession::GetAccountId`. A bot control plane must resolve/allowlist an account plus GUID and retain the database comparison; it must not fake `_legitCharacters` as its only check. Already-online protection comes from normal same-account session replacement plus character legitimacy/cache state, but there is no atomic character-GUID reservation visible in this path; the future manager must reject `ObjectAccessor::FindPlayer(guid)` and in-flight duplicate GUIDs before starting.

## 4. Character query-holder lifecycle

`LoginQueryHolder` is file-local in `CharacterHandler.cpp` and derives from `CharacterDatabaseQueryHolder`. Its constructor stores account ID and GUID. `Initialize()` sizes it to `MAX_PLAYER_LOGIN_QUERY` (39 indices in `Player.h`) and queues prepared statements for: character core row; group; instance binds; auras; spells/cooldowns; all quest cadence/reward/LFG statuses; reputation; inventory/void storage; action bars; mail/items; social; home bind; optional declined names; guild; arena; achievements/criteria; equipment sets/CUF; BG/random-BG; glyphs/talents; per-character account data; skills; bans; account instance lock times; currency; corpse; and all pet details.

Lifecycle:

1. `HandleContinuePlayerLogin` creates `std::shared_ptr<LoginQueryHolder>`, initializes every slot, then passes it to `CharacterDatabase.DelayQueryHolder`.
2. The database worker/future owns the shared query-holder while statements are pending. `SQLQueryHolderCallback` is moved into the session's `QueryCallbackProcessor` by `AddQueryHolderCallback`.
3. `WorldSession::Update` invokes `ProcessQueryCallbacks`; the `AfterComplete([this]...)` lambda uses a raw session pointer and calls `HandlePlayerLogin` on the world/session update context.
4. Normal lifetime protection is indirect: `m_playerLoading` remains set; `World::RemoveSession` refuses to kick a loading session; a null socket would normally make `Update` return false, which is why server-originated liveness must be fixed before scheduling.
5. If holder initialization fails, loading is cleared and the shared holder dies. If `LoadFromDB` fails, completion clears the session player pointer, kicks, deletes the unattached `Player`, clears loading, and returns. A DB result with no character row takes this route.
6. If a session is destroyed by an exceptional path despite loading protection, its callback processor is destroyed; no callback should then run. The raw `[this]` is safe only while world ownership plus loading pin is maintained. A separate runtime callback capturing a raw runtime/session would not be safe.

**Reuse decision:** yes. Make the holder accessible through a narrow lifecycle seam (move to a small header/source or expose a session method); do not create synchronous queries or a reduced holder. Completion must stay session-owned. Add a generation/token check in the future runtime only for control-plane status, not as a replacement for session lifetime.

## 5. WorldSession ownership model

`WorldSocket` initially creates the normal session and hands it to `World::AddSession`; `World::m_sessions` becomes the deleting owner. `AddSession_` deletes a rejected/replaced incoming session and stores one session per account. `UpdateSessions` deletes a session when `Update` returns false. `World::~World` deletes any residue.

The constructor accepts `std::shared_ptr<WorldSocket>` and already permits null: address/timeout/account-online work is conditional, and socket slots receive the possibly-null shared pointer. The destructor calls `LogoutPlayer(true)` if `_player`, conditionally closes sockets, deletes Warden/RBAC/GameClient, drains packet queue, and clears Login DB account-online.

**Recommendation:** add an explicit immutable `SessionOrigin`/`IsServerOriginated()` property (not inference from null alone); construct a normal session with no socket; immediately enqueue it with `World::AddSession`; thereafter store only `(accountId, characterGuid, lifecycle generation)` in `PlayerbotRuntime`. `World` alone deletes it. The runtime asks the world-owned session to start/stop and erases its handle after observing completion. Do not use `unique_ptr<WorldSession>` in runtime and do not delete `Player`.

## 6. Player ownership model

`HandlePlayerLogin` creates `Player*`. Before successful load it is local cleanup responsibility. During `LoadFromDB`, `GetSession()->SetPlayer(this)` attaches it; failure handling deliberately calls `SetPlayer(nullptr)` before manual deletion. On success the player enters a map.

At logout `WorldSession` performs state cleanup and calls `Map::RemovePlayerFromMap(_player, true)`. `Map::DeleteFromWorld(Player*)` unregisters it from `ObjectAccessor`, and map removal deletes it; only then does the session execute `SetPlayer(nullptr)`. Therefore map removal is the authoritative successful-login deletion path. The runtime must never cache a dereferenceable `Player*` across ticks; resolve by GUID during the safe context and discard immediately.

## 7. Map/world/object registration sequence

1. `LoadFromDB` validates/repairs persisted location, transport, taxi and instance/BG state; obtains a map through `sMapMgr->CreateMap`; associates it with the player; loads phase/zone-related fields and calls `UpdatePositionData` later in completion.
2. `HandlePlayerLogin` initializes motion and sends/executes pre-map initialization.
3. `Map::AddPlayerToMap` adds to the map's player/container world structures and makes normal map updates possible. Failure attempts a go-back/home-bind teleport; this can create a far-teleport pending state and must be completed server-side or treated as failed login.
4. `ObjectAccessor::AddObject` registers the player globally only after map insertion.
5. The completion emits verify-world, updates position data, removes login-interrupt auras, executes after-map initialization, marks character and account online, announces group/social presence, loads corpse/pets, continues taxi, applies login flags and achievement/script hooks, and finally clears `m_playerLoading`.

The online character flag is set only after map/object insertion. The authoritative offline clear is `CHAR_UPD_ACCOUNT_ONLINE` in logout (with global `ClearOnlineAccounts` as shutdown repair); do not independently toggle rows in runtime.

## 8. Socket assumptions

The null socket is supported by construction, destruction, `KickPlayer`, instance close, Warden checks, and `SendPacket` (it logs and returns before dereference). `GetRemoteAddress()` returns stored `m_Address` and is safe though empty; bot logging should supply a descriptive origin rather than depend on IP. Encryption and link keys live in sockets/connection handoff and do not drive loaded-player state. Latency defaults to zero. AntiDOS is reached only for queued inbound packets. Warden updates require a realm socket and Warden pointer.

The two lifecycle blockers are within `WorldSession::Update`: idle close dereferences `m_Socket[REALM]` without a local null check, and the unsafe pass returns false whenever the realm socket is absent. A server-origin marker must bypass network idle/expiry/removal while still processing query callbacks, logout requests, and normal session bookkeeping. A fake `WorldSocket` is not justified.

## 9. Outbound packet classification

| Packet/family | Emission / state ordering | Physical-client consequence |
|---|---|---|
| `ConnectToInstance`, `ResumeComms` | before query continuation; connection protocol | Skip only for explicit server origin. It gates client routing, not DB/player state. |
| account data times, server info, dance/hotfix, MOTD, feature status, cinematic | during completion, before map add | Presentation only; `SendPacket` dropping them is safe. Preserve server-side first-login cinematic flag mutation. |
| `SendInitialPacketsBeforeAddToMap` | before insertion | Mostly client snapshot/times/systems. Calls may include state initialization; reuse whole method initially rather than selectively omit. Sends safely drop. |
| `SMSG_LOGIN_VERIFY_WORLD` | after object registration | Client world presentation. No initial-login ACK handler drives insertion. |
| `SendInitialPacketsAfterAddToMap`, guild/group/social packets | after insertion | Notifications/snapshots; server state has already transitioned. |
| `SMSG_TIME_SYNC_REQ` | periodic session safe/map pass | Lack of response leaves clock delta/default queues; it does not remove player. Adapter may suppress it to avoid pending growth/noise. |
| movement broadcasts from server generators | while map updates movement | Notify nearby real clients; bot itself need not acknowledge `MovePoint`. |
| `SMSG_TRANSFER_PENDING`/`SMSG_NEW_WORLD` and teleport movement | before far/near transfer completion | Unlike ordinary sends, server state waits for client ACK. Server origin must invoke the matching completion path. |
| `SMSG_LOGOUT_COMPLETE` | after map deletion/session detach | Notification only; noted client trade cancels are unhandled. No ACK needed. |

## 10. Required client acknowledgements

Initial login has no required client reply after canonical completion. Time sync affects client/server movement timestamp reconciliation, not session health. Server-generated `MotionMaster::MovePoint` advances through map/Unit/MotionMaster updates and broadcasts movement; it does not use client movement ACK.

Near teleports wait for `HandleMoveTeleportAck`; far teleports wait for `HandleMoveWorldportAck` (and split-connection flow may include suspend-token response before `NewWorld`). These are required state transitions. Logout already loops over far teleports and directly calls `HandleMoveWorldportAck`. The milestone should reject/defer taxi, transport transfers, battleground entry, and arbitrary teleport behavior except the canonical login repair/logout completion paths until an adapter invokes and tests the appropriate server completion.

## 11. Update/thread-context map

| Context | What runs / safe mutation | Playerbot rule |
|---|---|---|
| `World::Update` / world update loop | global scheduling, `UpdateSessions`, manager updates; one world thread orchestrates | Control state and enqueue lifecycle operations here. Never wait on DB. |
| `WorldSession::Update` with `WorldSessionFilter` | unsafe opcodes, query callbacks, logout decisions, session removal | Login completion and authoritative logout belong here. A manager may request, not concurrently delete. |
| session update with `MapSessionFilter` | thread-safe queued packets and periodic time sync while map updates | Avoid fabricating packets. It is not the AI interface. |
| `Map::Update` | owns in-world object updates for that map (potential map worker context) | In-world `Player` mutation must be executed in this context or via the repository's map-safe task mechanism. Do not mutate a player directly from an arbitrary global-manager iteration if map updates can be parallel. |
| `Player::Update` -> `Unit::Update` | timers, auras and normal player/unit state | Automatic once map member. No separate runtime `Player::Update`. |
| `Unit::Update` -> `MotionMaster::UpdateMotion` | server movement generator advances position/spline | `MovePoint` initiation should be a map-context command; completion naturally updates here. |
| DB worker | executes holder statements/fills results | Must not touch session/player. |
| session `ProcessQueryCallbacks` | consumes ready DB future on session/world update | Safe canonical construction/login context; raw callback target valid only under world ownership/loading pin. |

Shutdown implication: stop accepting bot starts before session drain; cancel/ignore manager intents; continue session updates until holders complete or are safely discarded and every online bot has passed logout. No AI work may be queued after maps stop updating.

## 12. Movement implications

`Player` is map-updated through `Unit::Update`, which calls `MotionMaster::UpdateMotion`. `MotionMaster::MovePoint` installs a server-side point movement generator; it does not depend on `CMSG_MOVE_*`, client timestamps, teleport ACKs, or a mover socket. Nearby clients receive normal object movement broadcasts.

For milestone 1, resolve the player by GUID in a map-owned callback, verify `IsInWorld`, not teleporting, alive, not on taxi/transport, not in combat, and destination on the same map with valid coordinates/path. Call one short `MovePoint(id, x, y, z)`, observe generator completion/position, then `MotionMaster::Clear` or `MoveIdle`. Do not invoke it directly from a DB callback or network thread. The exact queue primitive from world manager to a particular map is the highest-risk remaining implementation detail and must be confirmed against the configured map updater before code is accepted.

## 13. Normal logout call graph

`WorldSession::LogoutPlayer(save)` is authoritative and idempotence must be enforced at the request layer:

1. Complete pending far teleports by direct `HandleMoveWorldportAck` calls; mark logout/save in progress.
2. Release loot; resolve death/spirit/pending bind; notify battleground and remove BG queues; repair invalid instance; notify OutdoorPvP.
3. Complete any new far transfer; notify guild; save/remove pet; clear whisper list; fail logout quests.
4. If `save`, clear buyback fields and call `Player::SaveToDB` while player is still a map member.
5. Clean channels; uninvite/group updates (non-raid removal is conditional on a real realm socket, so socketless group semantics must remain deferred/audited); social offline/remove; script and metrics.
6. Mark destroyed and call `CleanupsBeforeDelete` (combat/movement and object cleanup); log.
7. If a map exists, `Map::RemovePlayerFromMap(player, true)` removes membership, invokes `ObjectAccessor::RemoveObject`, and deletes the player.
8. `SetPlayer(nullptr)` clears the sole session pointer; send logout-complete notification; `CHAR_UPD_ACCOUNT_ONLINE` clears character online rows for the account.
9. Close/reset instance socket if present; reset logout flags, set recently-logged-out, clear logout request.
10. With server-origin stop/removal requested, the next unsafe `WorldSession::Update` should return false under explicit lifecycle state; `World::UpdateSessions` erases and deletes the session. Destructor then has no player and clears Login DB account-online.

## 14. Failure unwind paths

| Failure | Authoritative unwind |
|---|---|
| holder initialization / DB execution failure | Clear `m_playerLoading`; destroy holder/callback. For bot mode, mark failed then request session removal; destructor clears account online. |
| session disappears while load pending | Must be prevented by world ownership/loading pin. Shutdown must either drain callbacks or destroy the session callback processor only after DB workers are quiesced; never leave a standalone raw capture. |
| character already online / duplicate start | Reject before query using runtime in-flight set and `ObjectAccessor`; `AddSession_` resolves same-account conflicts but should not be used to evict a real client silently. |
| `Player::LoadFromDB` false | Existing path detaches, kicks, manually deletes local player, clears loading. Bot removal follows. No map/object registration or character-online write has occurred. |
| invalid map/location | `LoadFromDB` repairs to bind/default or returns false. If post-load `AddPlayerToMap` fails and initiates teleport, adapter must finish it and confirm in-world; otherwise call logout without claiming success. |
| failure after map add but before online write | Call `LogoutPlayer(true)`; map removal is authoritative. Shutdown `ClearOnlineAccounts` is only a final repair, not normal ownership. |
| logout during map transfer | Existing logout synchronously finishes far teleport first, then saves/removes. Near teleport needs explicit bot adapter completion before logout or feature deferral. |
| repeated stop/logout | Runtime state machine accepts stop once (`Online -> Stopping`); later requests observe only. Never call logout on a cached session/player after world removal. |
| destructor without successful login | No player logout; closes only present sockets, destroys callbacks/support objects, clears account online. Ensure loading callback cannot outlive destructor. |
| shutdown while loading | Disable starts, retain/update session until callback completes then immediately logout, or cancel callback after DB completion is safely drained. `KickAll` cannot kick `PlayerLoading`, so the current generic shutdown alone is insufficient for a bot loading at the boundary. |
| shutdown while online | Request `LogoutPlayer(true)` before generic `KickAll`; drain `UpdateSessions` until detached/deleted. Generic null-socket `KickPlayer` alone does nothing. |

## 15. Server shutdown ordering

`worldserver/Main.cpp` exits `WorldUpdateLoop`, stops the I/O context/thread pool, runs `ScriptMgr::OnShutdown`, and later the `sWorldSocketMgr` scope deleter calls `World::KickAll`, one `World::UpdateSessions(1)`, stops network, and `ClearOnlineAccounts`. During outer teardown, OutdoorPvP is destroyed before `sMapMgr->UnloadAll`; database pools close later (`CharacterDatabase`, `WorldDatabase`, `LoginDatabase`, `HotfixDatabase`). `World::~World` finally deletes residual sessions, whose destructors may logout/save.

**Safe policy:** begin bot shutdown at shutdown initiation (`ScriptMgr::OnShutdownInitiate` or a direct world-owned equivalent), while normal world and maps still update. Reject new starts immediately. All holders must complete/cancel safely and all bot sessions must be logged out, removed from maps/ObjectAccessor, and deleted **before `WorldUpdateLoop` ends and therefore before map unload begins**. Do not rely on the late `KickAll`: null-socket kick is inert and one session tick cannot guarantee async completion. Database pools must remain alive through the drain. `ClearOnlineAccounts` remains crash recovery.

## 16. Ownership table

| Object | Creator / owner / deleter | Nullable and observers | Logout / shutdown |
|---|---|---|---|
| `WorldSession` | normal socket or bot factory creates; after enqueue `World::m_sessions` exclusively owns; `AddSession_`, `UpdateSessions`, or `World::~World` deletes | runtime stores account ID/generation only; socket points back during normal networking | owns logout orchestration; must be drained before maps/DB. |
| `WorldSocket` | socket manager creates shared object; session stores up to two `shared_ptr`s | null from bot construction; networking owns other refs | conditional close/reset; bot has none. |
| `Player` | `HandlePlayerLogin` allocates; pre-load local path deletes on failure; successful session/map lifecycle controls it | session raw `_player`, map containers, ObjectAccessor, social/group/guild references; null before/after login | `RemovePlayerFromMap(..., true)` unregisters/deletes exactly once; session then nulls pointer. |
| `LoginQueryHolder` / `CharacterDatabaseQueryHolder` | continuation creates shared holder; DB future/callback machinery owns while pending | callback receives const base reference during invocation | destroyed with completed/cancelled callback; callback captures raw session, requiring pin/drain. |
| Map membership | `AddPlayerToMap` creates membership; map owns update/removal responsibility, not an independent player owner API | absent before insertion/during transfer/after removal | `RemovePlayerFromMap(..., true)` is authoritative removal/deletion path. |
| hypothetical `PlayerbotRuntime` | manager creates/owns value by account/GUID | may resolve ephemeral session/player only in safe context; never stores owning/raw long-lived pointers | state machine requests logout, observes disappearance, erases itself before manager destruction. |

These rules prevent double delete (one world owner, map deletes loaded player), dangling pointers (GUID re-resolution and state observation), callback-after-destruction (loading pin plus drain), stale DB-online rows (canonical logout plus global crash repair), and abandoned map objects (logout before session deletion/maps unload).

## 17. Socket dereference table

| Site | Assumption | Class | Code change? |
|---|---|---:|---:|
| `WorldSession` constructor socket block | address, timeout, account-online only with socket | A | no |
| `WorldSession::SendPacket` | validates socket and returns before `SendPacket` dereference | A | no (consider suppressing expected bot log noise) |
| `WorldSession::Update`, idle branch | `IsConnectionIdle()` may lead to unconditional realm `CloseSocket()` | B | **yes, 1**: exclude explicit server origin / guard socket |
| `WorldSession::Update`, receive loop/Warden/close cleanup | all dereferences locally guarded | A | no |
| `WorldSession::Update`, final `if (!realm socket) return false` | equates no network with dead session | C | **yes, 2**: server-origin liveness/removal state |
| `PlayerDisconnected()` | defines disconnected from two live sockets | C/E | no for milestone if server-origin callers avoid network policy; audit future uses |
| destructor, `KickPlayer`, logout instance close | loops/branches guard socket | A | no |
| logout non-raid group removal | behavior is conditional on realm socket | E | no; groups deferred, revisit before group milestone |
| `GetRemoteAddress` login/logout logs | returns stored string, no socket dereference | A | no; empty origin is cosmetic |
| latency/time sync | zero/default fields; send drops | A | no |
| Warden/AntiDOS/encryption | Warden requires socket; AntiDOS only inbound; encryption is socket-side | E | no |
| split-connection handoff (`SendConnectToInstance`) | requires actual connection protocol | C | bypass through explicit server-origin login entry, not fake socket |

**Count requiring milestone-1 source changes: 2.** The explicit server-origin login entry is a lifecycle seam, not a third socket dereference fix.

## 18. Acknowledgement table

| Client response | Server dependency | Milestone-1 classification |
|---|---|---|
| initial login / verify-world | no ACK gates map/ObjectAccessor/online transitions | does not encounter |
| load-screen notify | handler is TODO/no-op | does not encounter |
| time-sync response | refines clock delta/movement timing; absence does not timeout session | safely suppress request or tolerate; no required ACK |
| client movement/heartbeat | validates client-driven mover state; server point generator is independent | does not encounter |
| `MSG_MOVE_TELEPORT_ACK` (near) | clears near semaphore, applies position/zone/pet/delayed operations | defer feature; adapter required if canonical repair produces it |
| `MSG_MOVE_WORLDPORT_ACK` (far) | creates/enters destination map, initializes zone/flight/pet/delayed operations | small adapter: call existing server method; logout already does |
| suspend-token response/new-world (split connection) | stages far client transfer packet | bypass connection presentation only; still call worldport completion |
| taxi/transport transitions | stateful generator/map/transport handling, sometimes transfer-dependent | defer taxi/transport action; canonical relog continuation remains canonical code |
| session heartbeat/timeout | network timeout assumes sockets | explicit server-origin liveness adapter required; no synthetic heartbeat |

No required client acknowledgement blocks the one same-map `MovePoint`. Far-teleport completion is the only acknowledgement-shaped operation that canonical failure/logout can encounter and must be handled server-side.

## 19. Alternative A — maximum normal World ownership (**preferred**)

**Files/symbols to modify in the future (not now):** `Handlers/CharacterHandler.cpp` (`LoginQueryHolder`, `HandleContinuePlayerLogin`, `HandlePlayerLogin` extraction/server entry); `Server/WorldSession.{h,cpp}` (constructor origin, `Update` liveness, server login/stop); `World/World.{h,cpp}` (`AddSession`, `UpdateSessions`, manager lifecycle/tick); `worldserver/Main.cpp` or world shutdown hooks (pre-loop-exit drain); build/config/command files only when explicitly included by the implementation task. New files: `game/Playerbots/PlayerbotMgr.{h,cpp}`, `PlayerbotRuntime.{h,cpp}`, and `Lifecycle/PlayerbotLogin.{h,cpp}`; one admin command script.

* Ownership: world owns/deletes session; map logout deletes player; runtime owns IDs/state only.
* Login: authorize allowlisted account/GUID, reject real/in-flight online conflicts, create null-socket server-origin session, enqueue `AddSession`, set loading and schedule the exact holder, complete in session callback through canonical login.
* Update: normal `WorldSession::Update`; normal map/player/motion updates; manager schedules bounded GUID-based map-context commands.
* Logout: one stop transition requests `LogoutPlayer(true)` in unsafe session context, then session removal flag makes normal `UpdateSessions` delete it.
* Null-socket changes: exactly the two classified update/liveness sites; packet sends remain drops.
* Pros: strongest invariants, callback naturally pinned, normal shutdown/session telemetry, smallest ownership novelty.
* Cons: one explicit session-origin concept in core; must avoid counting bots as client capacity if desired; canonical completion needs careful extraction.
* Regression risk: low-to-medium, isolated and default-inactive; normal clients retain identical path.
* Size/testability: small vertical patch; unit-test state transitions and integration-test one real character. This is the only design recommended for first live proof.

## 20. Alternative B — separately owned PlayerbotRuntime session (**fallback**)

**Files/symbols:** same canonical login extraction in `CharacterHandler.cpp` and socket safety in `WorldSession`; fewer `World::AddSession/UpdateSessions` changes, but new `Playerbots/PlayerbotRuntime.{h,cpp}` owns `std::unique_ptr<WorldSession>`, its callback processor/update/removal, plus manager and login files; shutdown in `World`/`Main.cpp` still changes.

* Ownership: runtime uniquely owns session; session/map rules still own player destruction. Runtime must destroy session only after logout and callbacks are empty.
* Login: runtime creates session and schedules exact holder, but must independently call `WorldSession::Update`/callback processing and pin itself across async work.
* Update: manager drives session unsafe updates separately while maps drive player; explicit ordering is needed each tick.
* Logout: runtime calls canonical logout, verifies player null/map removal, then destroys session once.
* Null-socket changes: a separate update mode can avoid the world-removal condition, but idle/socket logic still needs origin handling; packet drops unchanged.
* Pros: bot sessions do not occupy `World::m_sessions`; specialized control is easy.
* Cons: recreates session container, scheduling, deletion, replacement, callback lifetime and shutdown code; real-client same-account conflict becomes cross-container; more dangling-pointer risk.
* Regression risk: medium-to-high because two lifecycle authorities coexist.
* Size/testability: medium; substantially more failure-order tests. Retain only as fallback if enrollment in `m_sessions` proves incompatible with unavoidable socket/network accounting.

## 21. Ranked recommendation

1. **PREFERRED — Alternative A.** `World::m_sessions` is the sole `WorldSession` owner; explicit server origin adjusts two network-liveness assumptions; canonical holder/login/logout remain authoritative.
2. **FALLBACK — Alternative B.** Runtime owns a session only if measured core constraints make normal enrollment impossible. That has not been proven, so it should not be implemented now.

## 22. Exact minimum implementation patch

The next patch should support exactly one configured/allowlisted existing Warrior and nothing else.

Add `src/server/game/Playerbots/PlayerbotMgr.{h,cpp}`, `PlayerbotRuntime.{h,cpp}`, and `Lifecycle/PlayerbotLogin.{h,cpp}` (plus minimal CMake registration), and one administrator command source for `start <account> <guid>`, `status`, and `stop`. Modify only the minimum symbols identified in Alternative A: expose/reuse the complete holder and completion; mark a session as server-originated; keep that session alive in `Update`; enroll it with `World::AddSession`; add bounded manager update and pre-shutdown drain. Configuration should be default-off with one explicit allowlist; add no schema.

Acceptance sequence: start -> async 39-slot holder -> canonical load/map/ObjectAccessor/online -> report GUID/map/XYZ -> GUID-resolved heartbeat for at least five minutes -> enqueue one short same-map map-context `MovePoint` -> stop/idle -> canonical `LogoutPlayer(true)` -> session removal/destruction -> restart and verify saved position plus offline rows. Instrument each state transition and failure. No combat, quests, population, class AI, tables, taxi/transport/BG, near teleport, or client packet emulation.

## 23. Exact next Codex task prompt outline

> Implement the documentation-audited **Alternative A** lifecycle seam from `docs/playerbots-434-lifecycle-audit.md` for exactly one existing allowlisted Warrior. Keep `World::m_sessions` the exclusive session owner and map logout the exclusive loaded-Player deletion path. Reuse all 39 `LoginQueryHolder` queries and the complete canonical `HandlePlayerLogin` initialization; do not add synchronous DB shortcuts. Add explicit server-origin session state and change only the two audited null-socket liveness assumptions. Add a GUID/state-only runtime, default-off allowlist, admin start/status/stop, bounded heartbeat, one map-context same-map `MotionMaster::MovePoint`, canonical save/logout, and a pre-map-unload shutdown drain. Add no SQL/schema, combat, quests, random bots, class AI, fake socket, or broad packet emulation. Test duplicate/online rejection, load failure, map failure, repeated stop, loading/online shutdown, five-minute tick, movement, save/offline rows, restart persistence, and normal client regression. Report Linux runtime separately; do not claim Windows without running it.

## 24. Remaining unknowns

1. **Highest risk:** the exact existing facility and ordering for enqueueing a manager intent onto the owning map update context under every map-update configuration. Confirm this before calling `MovePoint`; do not assume the world thread is the map thread.
2. Whether `Map::AddPlayerToMap` failure followed by `TeleportTo` always leaves a recoverable far-transfer state for a null-socket session needs runtime fault injection.
3. The shutdown loop needs a bounded drain/cancellation design for a holder whose DB future is slow; current `KickAll` refuses loading sessions and one `UpdateSessions` pass is insufficient.
4. Character-level duplicate login across different accounts is not atomically reserved by the audited path; manager in-flight reservation and `ObjectAccessor` check require concurrency tests.
5. `SendInitialPacketsBefore/AfterAddToMap` may produce noisy missing-socket logs. Suppression is cosmetic and should not become selective state skipping without instrumentation.
6. Group removal intentionally differs when no realm socket exists. Groups are outside milestone 1 and must be audited before enabled.
7. Normal client, Linux runtime, persistence, shutdown failure injection, and Windows behavior remain unverified by this documentation-only task.
