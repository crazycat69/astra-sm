-- Astra Applications (Windows Service Installer)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2016, Artem Kharitonov <artem@3phase.pw>
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

local svc_action = nil
local svc_opts = {
    name = nil,
    displayname = nil,
    description = nil,
    arguments = nil,
    start = nil,
    force = nil,
}

options_usage = [[
    --install           install service
    --uninstall         uninstall service

    --name              service name (optional)
    --displayname       name displayed in the services MSC (optional)
    --description       description displayed in the services MSC (optional)
    --arguments         command line arguments for service (required)
    --start             startup mode: auto, manual or disabled [manual]
    --force             skip safety checks when removing service
]]

options = {
    ["--install"] = function(idx)
        svc_action = "install"
        return 0
    end,
    ["--uninstall"] = function(idx)
        svc_action = "uninstall"
        return 0
    end,
    ["--name"] = function(idx)
        svc_opts["name"] = argv[idx + 1]
        return 1
    end,
    ["--displayname"] = function(idx)
        svc_opts["displayname"] = argv[idx + 1]
        return 1
    end,
    ["--description"] = function(idx)
        svc_opts["description"] = argv[idx + 1]
        return 1
    end,
    ["--arguments"] = function(idx)
        svc_opts["arguments"] = argv[idx + 1]
        return 1
    end,
    ["--start"] = function(idx)
        svc_opts["start"] = argv[idx + 1]
        return 1
    end,
    ["--force"] = function(idx)
        svc_opts["force"] = true
        return 0
    end,
}

function main()
    if svc_action == nil then
        print("Please supply either --install or --uninstall.")
        print("Try --help for full option list.")
        astra.exit(1)
    end

    func = winsvc[svc_action]
    local success, errstr = pcall(func, svc_opts)
    if not success then
        print("Couldn't " .. svc_action .. " service:")
        print(errstr)
        astra.exit(1)
    else
        print("Service successfully " .. svc_action .. "ed.")
        astra.exit(0)
    end
end
