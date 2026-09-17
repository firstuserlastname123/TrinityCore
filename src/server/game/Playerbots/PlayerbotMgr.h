/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef PlayerbotMgr_h__
#define PlayerbotMgr_h__

#include "PlayerbotRuntime.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Map;
class Player;

namespace Playerbots
{
class TC_GAME_API PlayerbotMgr
{
public:
    PlayerbotMgr();

    void LoadConfig();
    void Update(uint32 diff);
    void UpdateMap(Map* map, uint32 diff);
    void BeginShutdown();
    bool IsDrained() const;

    bool Start(uint32 guidLow, std::string& result);
    bool Stop(uint32 guidLow, std::string& result);
    bool MoveTest(uint32 guidLow, std::string& result);
    std::string Status(uint32 guidLow = 0) const;

    void OnLoginStarted(uint32 guidLow);
    void OnLoginComplete(uint32 accountId, uint32 guidLow, bool success, char const* reason);

private:
    using RuntimeMap = std::unordered_map<uint32, Runtime>;
    static std::unordered_set<uint32> ParseAllowlist(std::string const& text);
    static char const* StateName(LifecycleState state);
    static char const* MovementName(MovementState state);
    Player* FindPlayerOnMap(Map* map, uint32 guidLow) const;
    void Fail(Runtime& runtime, char const* reason);

    mutable std::mutex _mutex;
    RuntimeMap _runtimes;
    std::unordered_set<uint32> _accountAllowlist;
    std::unordered_set<uint32> _characterAllowlist;
    bool _enabled = false;
    bool _telemetry = true;
    bool _shuttingDown = false;
    uint32 _updateInterval = 500;
    uint64 _nextGeneration = 1;
};
}

#endif
