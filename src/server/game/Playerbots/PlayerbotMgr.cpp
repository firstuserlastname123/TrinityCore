/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This milestone is a clean implementation against TrinityCore lifecycle APIs. The three
 * Playerbot projects listed in docs/playerbots-434-port-plan.md were architecture references;
 * no source code was copied from them.
 */

#include "PlayerbotMgr.h"
#include "CharacterCache.h"
#include "Config.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"
#include <cmath>
#include <sstream>

namespace Playerbots
{
namespace
{
constexpr uint32 MovementPointId = 0x50424F54; // 'PBOT'
constexpr uint32 MovementTimeoutMs = 15000;
constexpr float MovementDistance = 4.0f;
constexpr float ArrivalTolerance = 0.75f;
}

PlayerbotMgr::PlayerbotMgr() = default;

std::unordered_set<uint32> PlayerbotMgr::ParseAllowlist(std::string const& text)
{
    std::unordered_set<uint32> values;
    std::istringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        std::istringstream words(token);
        uint32 value;
        while (words >> value)
            if (value)
                values.insert(value);
    }
    return values;
}

void PlayerbotMgr::LoadConfig()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _enabled = sConfigMgr->GetBoolDefault("Playerbots.Enable", false);
    _telemetry = sConfigMgr->GetBoolDefault("Playerbots.TelemetryLog", true);
    _updateInterval = std::max<uint32>(100, sConfigMgr->GetIntDefault("Playerbots.UpdateIntervalMS", 500));
    _accountAllowlist = ParseAllowlist(sConfigMgr->GetStringDefault("Playerbots.AccountIds", ""));
    _characterAllowlist = ParseAllowlist(sConfigMgr->GetStringDefault("Playerbots.CharacterGuids", ""));
    TC_LOG_INFO("playerbots", "Playerbots milestone 1 is %s; %zu accounts and %zu characters allowlisted",
        _enabled ? "enabled" : "disabled", _accountAllowlist.size(), _characterAllowlist.size());
}

bool PlayerbotMgr::Start(uint32 guidLow, std::string& result)
{
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(guidLow);
    CharacterCacheEntry const* character = sCharacterCache->GetCharacterCacheByGuid(guid);
    if (!character) { result = "character does not exist in the character cache"; return false; }
    uint32 accountId = character->AccountId;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_enabled || _shuttingDown) { result = _shuttingDown ? "Playerbots are shutting down" : "Playerbots.Enable is 0"; return false; }
        if (character->Class != CLASS_WARRIOR) { result = "milestone 1 accepts Warrior characters only"; return false; }
        if (!_characterAllowlist.count(guidLow)) { result = "character GUID is not allowlisted"; return false; }
        if (!_accountAllowlist.count(accountId)) { result = "owning account is not allowlisted"; return false; }
        if (_runtimes.count(guidLow)) { result = "bot start is already pending or active"; return false; }
        if (!_runtimes.empty()) { result = "milestone 1 permits exactly one Playerbot at a time"; return false; }
        if (sWorld->FindSession(accountId)) { result = "account has an active or loading session"; return false; }
        if (ObjectAccessor::FindConnectedPlayer(guid)) { result = "character is already online"; return false; }

        Runtime runtime;
        runtime.AccountId = accountId;
        runtime.CharacterGuid = guidLow;
        runtime.Generation = _nextGeneration++;
        runtime.HeartbeatTimer = _updateInterval;
        runtime.TelemetryTimer = 30000;
        _runtimes.emplace(guidLow, runtime);
        TC_LOG_INFO("playerbots", "START_REQUESTED account=%u guid=%u generation=" UI64FMTD, accountId, guidLow, runtime.Generation);
    }

    WorldSession* session = new WorldSession(accountId, std::string("Playerbot"), nullptr,
        SEC_PLAYER, EXPANSION_CATACLYSM, 0, LOCALE_enUS, 0, false, SessionOrigin::Server);
    session->QueueServerPlayerLogin(guid);
    sWorld->AddSession(session);
    result = "socketless session queued; canonical asynchronous login requested";
    return true;
}

void PlayerbotMgr::OnLoginStarted(uint32 guidLow)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto itr = _runtimes.find(guidLow);
    if (itr != _runtimes.end())
    {
        itr->second.State = LifecycleState::Loading;
        TC_LOG_INFO("playerbots", "QUERYING account=%u guid=%u; LOADING holder scheduled", itr->second.AccountId, guidLow);
    }
}

void PlayerbotMgr::OnLoginComplete(uint32 accountId, uint32 guidLow, bool success, char const* reason)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto itr = _runtimes.find(guidLow);
    if (itr == _runtimes.end())
        return;
    if (!success)
    {
        Fail(itr->second, reason);
        if (WorldSession* session = sWorld->FindSession(accountId); session && session->IsServerOrigin())
            session->RequestServerRemoval();
        return;
    }
    itr->second.State = _shuttingDown ? LifecycleState::Stopping : LifecycleState::Online;
    TC_LOG_INFO("playerbots", "ENTERING_WORLD account=%u guid=%u; ONLINE", accountId, guidLow);
    if (_shuttingDown)
        if (WorldSession* session = sWorld->FindSession(accountId))
        {
            session->LogoutPlayer(true);
            session->RequestServerRemoval();
        }
}

bool PlayerbotMgr::Stop(uint32 guidLow, std::string& result)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto itr = _runtimes.find(guidLow);
    if (itr == _runtimes.end()) { result = "bot is not tracked (already stopped)"; return true; }
    Runtime& runtime = itr->second;
    if (runtime.State == LifecycleState::Stopping) { result = "stop already requested"; return true; }
    runtime.State = LifecycleState::Stopping;
    TC_LOG_INFO("playerbots", "STOP_REQUESTED account=%u guid=%u", runtime.AccountId, guidLow);
    if (WorldSession* session = sWorld->FindSession(runtime.AccountId))
    {
        if (!session->PlayerLoading())
        {
            TC_LOG_INFO("playerbots", "LOGGING_OUT account=%u guid=%u", runtime.AccountId, guidLow);
            session->LogoutPlayer(true);
            session->RequestServerRemoval();
        }
    }
    result = "canonical logout requested";
    return true;
}

bool PlayerbotMgr::MoveTest(uint32 guidLow, std::string& result)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto itr = _runtimes.find(guidLow);
    if (itr == _runtimes.end() || itr->second.State != LifecycleState::Online) { result = "bot is not online"; return false; }
    if (itr->second.Movement == MovementState::Requested || itr->second.Movement == MovementState::Moving) { result = "movement test is already pending"; return false; }
    itr->second.Movement = MovementState::Requested;
    itr->second.LastMovementResult = "requested";
    TC_LOG_INFO("playerbots", "MOVE_REQUESTED account=%u guid=%u", itr->second.AccountId, guidLow);
    result = "movement test queued for owning map update";
    return true;
}

void PlayerbotMgr::Update(uint32 /*diff*/)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto itr = _runtimes.begin(); itr != _runtimes.end();)
    {
        Runtime& runtime = itr->second;
        WorldSession* session = sWorld->FindSession(runtime.AccountId);
        if (runtime.State == LifecycleState::Stopping && session && !session->PlayerLoading())
        {
            session->LogoutPlayer(true);
            session->RequestServerRemoval();
        }
        if ((runtime.State == LifecycleState::Stopping || runtime.State == LifecycleState::Failed) && !session)
        {
            TC_LOG_INFO("playerbots", "STOPPED account=%u guid=%u", runtime.AccountId, runtime.CharacterGuid);
            itr = _runtimes.erase(itr);
        }
        else
            ++itr;
    }
}

Player* PlayerbotMgr::FindPlayerOnMap(Map* map, uint32 guidLow) const
{
    Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guidLow));
    return player && player->GetMap() == map ? player : nullptr;
}

void PlayerbotMgr::UpdateMap(Map* map, uint32 diff)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& pair : _runtimes)
    {
        Runtime& runtime = pair.second;
        if (runtime.State != LifecycleState::Online)
            continue;
        Player* player = FindPlayerOnMap(map, runtime.CharacterGuid);
        if (!player || !player->IsInWorld())
            continue;

        runtime.LastMap = player->GetMapId(); runtime.LastZone = player->GetZoneId(); runtime.LastArea = player->GetAreaId();
        runtime.LastX = player->GetPositionX(); runtime.LastY = player->GetPositionY(); runtime.LastZ = player->GetPositionZ();

        if (runtime.HeartbeatTimer <= diff)
        {
            runtime.HeartbeatTimer = _updateInterval;
            ++runtime.HeartbeatSequence;
        }
        else runtime.HeartbeatTimer -= diff;
        if (runtime.TelemetryTimer <= diff)
        {
            runtime.TelemetryTimer = 30000;
            if (_telemetry)
                TC_LOG_INFO("playerbots", "HEARTBEAT account=%u guid=%u map=%u zone=%u area=%u xyz=%.3f,%.3f,%.3f state=ONLINE sequence=" UI64FMTD,
                    runtime.AccountId, runtime.CharacterGuid, player->GetMapId(), player->GetZoneId(), player->GetAreaId(),
                    player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), runtime.HeartbeatSequence);
        }
        else runtime.TelemetryTimer -= diff;

        if (runtime.Movement == MovementState::Requested)
        {
            if (!player->IsAlive() || player->IsBeingTeleported() || player->IsInFlight() || player->GetTransport() || player->IsInCombat())
            {
                runtime.Movement = MovementState::Rejected;
                runtime.LastMovementResult = "unsafe player state (dead/teleport/taxi/transport/combat)";
                continue;
            }
            runtime.StartX = player->GetPositionX(); runtime.StartY = player->GetPositionY(); runtime.StartZ = player->GetPositionZ();
            Position destination = player->GetFirstCollisionPosition(MovementDistance, 0.0f);
            runtime.TargetX = destination.GetPositionX(); runtime.TargetY = destination.GetPositionY(); runtime.TargetZ = destination.GetPositionZ();
            if (!MapManager::IsValidMapCoord(player->GetMapId(), destination))
            {
                runtime.Movement = MovementState::Rejected;
                runtime.LastMovementResult = "invalid destination";
                continue;
            }
            player->GetMotionMaster()->MovePoint(MovementPointId, destination, true);
            runtime.Movement = MovementState::Moving;
            runtime.MovementTimer = MovementTimeoutMs;
            runtime.LastMovementResult = "moving";
            TC_LOG_INFO("playerbots", "MOVE_STARTED account=%u guid=%u map=%u start=%.3f,%.3f,%.3f target=%.3f,%.3f,%.3f",
                runtime.AccountId, runtime.CharacterGuid, player->GetMapId(), runtime.StartX, runtime.StartY, runtime.StartZ,
                runtime.TargetX, runtime.TargetY, runtime.TargetZ);
        }
        else if (runtime.Movement == MovementState::Moving)
        {
            float distance = player->GetDistance(runtime.TargetX, runtime.TargetY, runtime.TargetZ);
            if (distance <= ArrivalTolerance)
            {
                player->GetMotionMaster()->Clear();
                player->GetMotionMaster()->MoveIdle();
                runtime.Movement = MovementState::Success;
                runtime.LastMovementResult = "arrived";
                TC_LOG_INFO("playerbots", "MOVE_SUCCESS account=%u guid=%u end=%.3f,%.3f,%.3f", runtime.AccountId,
                    runtime.CharacterGuid, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
            }
            else if (runtime.MovementTimer <= diff)
            {
                player->GetMotionMaster()->Clear();
                player->GetMotionMaster()->MoveIdle();
                runtime.Movement = MovementState::Timeout;
                runtime.LastMovementResult = "15 second timeout";
                TC_LOG_INFO("playerbots", "MOVE_TIMEOUT account=%u guid=%u target=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f",
                    runtime.AccountId, runtime.CharacterGuid, runtime.TargetX, runtime.TargetY, runtime.TargetZ,
                    player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
            }
            else runtime.MovementTimer -= diff;
        }
    }
}

void PlayerbotMgr::BeginShutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _shuttingDown = true;
    for (auto& pair : _runtimes)
    {
        Runtime& runtime = pair.second;
        runtime.State = LifecycleState::Stopping;
        if (WorldSession* session = sWorld->FindSession(runtime.AccountId))
            if (!session->PlayerLoading())
            {
                session->LogoutPlayer(true);
                session->RequestServerRemoval();
            }
    }
}

bool PlayerbotMgr::IsDrained() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _runtimes.empty();
}

void PlayerbotMgr::Fail(Runtime& runtime, char const* reason)
{
    runtime.State = LifecycleState::Failed;
    TC_LOG_ERROR("playerbots", "FAILED account=%u guid=%u reason=%s", runtime.AccountId, runtime.CharacterGuid, reason);
}

char const* PlayerbotMgr::StateName(LifecycleState state)
{
    switch (state) { case LifecycleState::Querying: return "QUERYING"; case LifecycleState::Loading: return "LOADING";
        case LifecycleState::Online: return "ONLINE"; case LifecycleState::Stopping: return "STOPPING"; case LifecycleState::Failed: return "FAILED"; }
    return "UNKNOWN";
}

char const* PlayerbotMgr::MovementName(MovementState state)
{
    switch (state) { case MovementState::Idle: return "IDLE"; case MovementState::Requested: return "REQUESTED";
        case MovementState::Moving: return "MOVING"; case MovementState::Success: return "SUCCESS";
        case MovementState::Timeout: return "TIMEOUT"; case MovementState::Rejected: return "REJECTED"; }
    return "UNKNOWN";
}

std::string PlayerbotMgr::Status(uint32 guidLow) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::ostringstream out;
    bool found = false;
    for (auto const& pair : _runtimes)
    {
        if (guidLow && pair.first != guidLow) continue;
        Runtime const& runtime = pair.second;
        found = true;
        out << "guid=" << runtime.CharacterGuid << " account=" << runtime.AccountId << " state=" << StateName(runtime.State)
            << " online=" << (runtime.State == LifecycleState::Online ? "yes" : "no") << " map=" << runtime.LastMap
            << " zone=" << runtime.LastZone << " area=" << runtime.LastArea
            << " xyz=" << runtime.LastX << "," << runtime.LastY << "," << runtime.LastZ << " heartbeat=" << runtime.HeartbeatSequence
            << " movement=" << MovementName(runtime.Movement) << " movement-result=\"" << runtime.LastMovementResult << "\"";
        if (!guidLow) out << '\n';
    }
    if (!found) return guidLow ? "Playerbot is not tracked" : "No Playerbots are tracked";
    return out.str();
}
}
