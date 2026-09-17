/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef PlayerbotRuntime_h__
#define PlayerbotRuntime_h__

#include "Define.h"
#include <string>

namespace Playerbots
{
enum class LifecycleState : uint8
{
    Querying,
    Loading,
    Online,
    Stopping,
    Failed
};

enum class MovementState : uint8
{
    Idle,
    Requested,
    Moving,
    Success,
    Timeout,
    Rejected
};

struct Runtime
{
    uint32 AccountId = 0;
    uint32 CharacterGuid = 0;
    uint64 Generation = 0;
    LifecycleState State = LifecycleState::Querying;
    MovementState Movement = MovementState::Idle;
    uint64 HeartbeatSequence = 0;
    uint32 HeartbeatTimer = 0;
    uint32 TelemetryTimer = 0;
    uint32 MovementTimer = 0;
    uint32 LastMap = 0, LastZone = 0, LastArea = 0;
    float LastX = 0.0f, LastY = 0.0f, LastZ = 0.0f;
    float StartX = 0.0f, StartY = 0.0f, StartZ = 0.0f;
    float TargetX = 0.0f, TargetY = 0.0f, TargetZ = 0.0f;
    std::string LastMovementResult = "not requested";
};
}

#endif
