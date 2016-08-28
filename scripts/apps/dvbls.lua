-- Astra Applications (DVB card enumeration)
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

local out_file

options_usage = [[
    -o FILE             Write DVB adapter list to file
]]

options = {
    ["-o"] = function(idx)
        out_file = argv[idx + 1]
        if out_file == nil then
            print("-o: this option requires a file name")
            astra.exit(1)
        end
        return 1
    end,
}

function main()
    log.info("Starting " .. astra.fullname)
    log.set({ color = true })

    local fh
    if out_file ~= nil then
        fh, err = io.open(out_file, "w")
        if fh == nil then
            print(err)
            astra.exit(1)
        end
        fh:write(
            "--\n",
            "-- automatically generated file. do not edit.\n",
            "--\n",
            "dvb_list = {\n"
        )
    end

    for _,dvb_info in pairs(dvbls()) do
        if dvb_info.error then
            log.error("adapter = " .. dvb_info.adapter .. ", device = " .. dvb_info.device)
            log.error("    check_device_fe(): " .. dvb_info.error)
        else
            local func = log.info
            if dvb_info.busy == true then
                func = log.warning
            end

            func("adapter = " .. dvb_info.adapter .. ", device = " .. dvb_info.device)
            if dvb_info.busy == true then
                func("    adapter in use")
            end
            func("    mac = " .. dvb_info.mac)
            func("    frontend = " .. dvb_info.frontend)
            func("    type = " .. dvb_info.type)

            if dvb_info.net_error then
                log.error("    check_device_net(): " .. dvb_info.net_error)
            end
            if fh ~= nil then
                s = string.format("\t{ adapter = %d, device = %d, mac = \"%s\" },\n",
                                  dvb_info.adapter, dvb_info.device, dvb_info.mac)

                fh:write(s)
            end
        end
    end

    if fh ~= nil then
        fh:write("}\n")
        fh:close()
        fh = nil
    end

    astra.exit()
end
