-- Astra Applications (DVB frontend monitor)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2015, Andrey Dyldin <and@cesbo.com>
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

log.set({ color = true })

options_usage = [[
    -a ADAPTER          DVB adapter number: /dev/dvb/adapter0
    -f FRONTEND         DVB frontend number: /dev/dvb/adapter0/frontend0
    -r                  show signal/SNR values in dBm

    URL                 dvb:// address
                        Example: "dvb://#adapter=0&type=S2&tp=..."
]]

local dvb_conf = {}
local dvb_femon = nil

options = {
    ["-a"] = function(idx)
        dvb_conf.adapter = tonumber(argv[idx + 1])
        return 1
    end,
    ["-f"] = function(idx)
        dvb_conf.frontend = tonumber(argv[idx + 1])
        return 1
    end,
    ["-r"] = function(idx)
        dvb_conf.raw_signal = true
        return 0
    end,
    ["*"] = function(idx)
        dvb_conf = parse_url(argv[idx])
        if not dvb_conf or dvb_conf.format ~= "dvb" then
            log.error("[femon] wrong address format")
            astra.exit(1)
        end
        return 0
    end,
}

function main()
    log.info("Starting " .. astra.fullname)

    if not dvb_conf.adapter and not dvb_conf.mac then
        astra_usage()
    end

    dvb_conf.format = "dvb"
    dvb_conf.name = "femon"
    dvb_conf.log_signal = true
    dvb_conf.no_dvr = true

    dvb_femon = init_input(dvb_conf)
end
