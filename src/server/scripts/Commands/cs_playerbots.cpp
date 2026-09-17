/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "Chat.h"
#include "PlayerbotMgr.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "World.h"

class playerbot_commandscript : public CommandScript
{
public:
    playerbot_commandscript() : CommandScript("playerbot_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> subcommands =
        {
            { "start",     rbac::RBAC_PERM_COMMAND_DEBUG, true, &HandleStart,  "" },
            { "status",    rbac::RBAC_PERM_COMMAND_DEBUG, true, &HandleStatus, "" },
            { "move-test", rbac::RBAC_PERM_COMMAND_DEBUG, true, &HandleMove,   "" },
            { "stop",      rbac::RBAC_PERM_COMMAND_DEBUG, true, &HandleStop,   "" }
        };
        static std::vector<ChatCommand> commands =
        {
            { "playerbot", rbac::RBAC_PERM_COMMAND_DEBUG, true, nullptr, "", subcommands }
        };
        return commands;
    }

private:
    static bool ParseGuid(char const* args, uint32& guid)
    {
        if (!args || !*args)
            return false;
        guid = uint32(strtoul(args, nullptr, 10));
        return guid != 0;
    }

    static bool HandleStart(ChatHandler* handler, char const* args)
    {
        uint32 guid; if (!ParseGuid(args, guid)) return false;
        std::string result; bool ok = sWorld->GetPlayerbotMgr().Start(guid, result);
        handler->SendSysMessage(result.c_str()); return ok;
    }

    static bool HandleStop(ChatHandler* handler, char const* args)
    {
        uint32 guid; if (!ParseGuid(args, guid)) return false;
        std::string result; bool ok = sWorld->GetPlayerbotMgr().Stop(guid, result);
        handler->SendSysMessage(result.c_str()); return ok;
    }

    static bool HandleMove(ChatHandler* handler, char const* args)
    {
        uint32 guid; if (!ParseGuid(args, guid)) return false;
        std::string result; bool ok = sWorld->GetPlayerbotMgr().MoveTest(guid, result);
        handler->SendSysMessage(result.c_str()); return ok;
    }

    static bool HandleStatus(ChatHandler* handler, char const* args)
    {
        uint32 guid = 0;
        if (args && *args && !ParseGuid(args, guid)) return false;
        handler->SendSysMessage(sWorld->GetPlayerbotMgr().Status(guid).c_str());
        return true;
    }
};

void AddSC_playerbot_commandscript()
{
    new playerbot_commandscript();
}
