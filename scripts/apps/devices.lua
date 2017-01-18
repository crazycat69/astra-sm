-- Astra Applications (Hardware enumerator)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2017, Artem Kharitonov <artem@3phase.pw>
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

options_usage = [[
    --list-modules      show list of supported hardware types
    MODULE...           enumerate detected devices for specific module(s)
]]

local list_devices = true
local module_list = {}

options = {
    ["--list-modules"] = function(idx)
        list_devices = false
        return 0
    end,
    ["*"] = function(idx)
        local name = argv[idx]
        if hw_enum[name] == nil then
            print("Unknown module: " .. name)
            astra.exit(1)
        end
        table.insert(module_list, name)
        return 0
    end,
}

local function dump_device(data)
    if data["name"] ~= nil then
        print("    * " .. data["name"])
        data["name"] = nil
    else
        print("    * [name unavailable]")
    end

    local key_list = {}
    for key, _ in pairs(data) do
        table.insert(key_list, key)
    end
    table.sort(key_list)

    for _, key in ipairs(key_list) do
        if type(data[key]) == "table" then
            local tbl = {}
            tbl[key] = data[key]
            table.dump(tbl, nil, "        ")
        elseif type(data[key]) == "string" then
            print("        " .. key .. " = \"" .. data[key] .. "\"")
        else
            print("        " .. key .. " = " .. tostring(data[key]))
        end
    end
end

function main()
    if #module_list == 0 then
        -- list devices for every available module by default
        for name, _ in pairs(hw_enum) do
            table.insert(module_list, name)
        end
    end
    table.sort(module_list)

    if #module_list == 0 then
        print("No hardware-specific modules are available")
        astra.exit(1)
    end

    if not list_devices then
        print("Supported hardware types:")
    end

    local dup = {}
    for _, name in ipairs(module_list) do
        if not dup[name] then
            dup[name] = true
            if list_devices then
                print(hw_enum[name].description .. " [" .. name .. "]")
                local ret, dev_list = pcall(hw_enum[name].enumerate)
                if not ret then
                    print("    ! Error: " .. dev_list .. "\n")
                elseif #dev_list == 0 then
                    print("    ! No devices found\n")
                else
                    for _, data in ipairs(dev_list) do
                        dump_device(data)
                        print("")
                    end
                end
            else
                -- module names only
                print("    * " .. hw_enum[name].description .. " [" .. name .. "]")
            end
        end
    end

    astra.exit(0)
end
