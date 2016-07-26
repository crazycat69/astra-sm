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

require("common/base")

-- load scripts in autoexec.d
-- TODO

-- parse command line
if #argv == 0 then
    astra_usage()
end

-- FIXME: move into common/
require("apps/stream")

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
