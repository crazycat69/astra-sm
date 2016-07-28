-- Astra Lua Library (Bootstrap)
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

-- load common libraries needed by most applications
--
-- packages are searched in the following order:
--   1. 'ASC_SCRIPTDIR' environment variable (if set)
--   2. local script directory (e.g. /etc/astra/scripts)
--   3. data directory (e.g. /usr/share/astra)
--   4. built-in scripts (if enabled at compile time)
--
require("common/base")
require("common/stream")

-- scan package path for autoexec directories
local auto_files = {}
local auto_keys = {}

local pkgpath = package.path:split(";")
for k = #pkgpath, 1, -1 do
    local dirname, nsub = pkgpath[k]:gsub("?.lua$", "autoexec.d")
    if nsub ~= 1 then
        goto continue
    end
    local stat = utils.stat(dirname)
    if not stat or stat.type ~= "directory" then
        goto continue
    end

    local ret, err = pcall(function()
        for name in utils.readdir(dirname) do
            if auto_files[name] == nil then
                table.insert(auto_keys, name)
            end
            auto_files[name] = dirname .. os.dirsep .. name
        end
        return true
    end)
    if ret == false then
        log.error(err)
    end

    ::continue::
end

table.sort(auto_keys)
for _, k in ipairs(auto_keys) do
    dofile(auto_files[k])
end

-- parse command line
if #argv == 0 then
    astra_usage()
end

local app_list = require("applist")
local app_pkg = app_list[1].pkg
local arg_idx = 0

for idx, arg in pairs(argv) do
    local found = false
    for _, app in pairs(app_list) do
        if app.arg == arg then
            found = true
            app_pkg = app.pkg
            arg_idx = idx
            break
        end
    end
    if found then
        break
    end
end

require(app_pkg)
astra_parse_options(arg_idx + 1)
if main ~= nil then
    main()
end
