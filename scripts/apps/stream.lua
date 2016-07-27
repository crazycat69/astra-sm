-- Astra Applications (Streaming)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2013-2015, Andrey Dyldin <and@cesbo.com>
--               2015-2016, Artem Kharitonov <artem@3phase.pw>
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
    FILE                Astra script
]]

options = {
    ["*"] = function(idx)
        local filename = argv[idx]
        local stat, stat_err = utils.stat(filename)
        if stat and stat.type == "file" then
            dofile(filename)
            return 0
        else
            if stat_err ~= nil then print(stat_err) end
        end
        return -1
    end,
}

function main()
    log.info("Starting " .. astra.fullname)
end
