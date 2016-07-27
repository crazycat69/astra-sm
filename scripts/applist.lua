-- Astra Lua Library (Application list)
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

local list = {
    {
        pkg = "apps/stream",
        arg = "--stream",
        txt = "digital TV streaming (default)",
    },
    {
        pkg = "apps/relay",
        arg = "--relay",
        txt = "UDP to HTTP relay",
    },
    {
        pkg = "apps/analyze",
        arg = "--analyze",
        txt = "command line MPEG TS analyzer",
    },
}

if dvbls ~= nil then
    table.insert(list, {
        pkg = "apps/dvbls",
        arg = "--dvbls",
        txt = "list installed DVB adapters",
    })
end

if dvb_input ~= nil then
    table.insert(list, {
        pkg = "apps/femon",
        arg = "--femon",
        txt = "monitor DVB adapter status",
    })
end

table.insert(list, {
    pkg = nil, -- hardcoded in main.c
    arg = "--dumb",
    txt = "run scripts without loading Lua libraries",
})

return list
