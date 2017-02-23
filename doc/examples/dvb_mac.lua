--
-- This is a sample script to create MAC-to-adapter maps for DVB cards on Linux.
--
-- How to use:
-- 1. Run this script at boot time:
--    # astra dvb_mac.lua > adapters.lua
--
-- 2. Include adapters.lua in your configuration:
--    dofile("adapters.lua")
--    -- OR --
--    require("adapters")
--    -- OR put it in ${ASC_SCRIPTDIR}/autoexec.d
--
-- 3. Supply 'mac' option to dvb_tune():
--    dvb_tune({
--        ...
--        mac = "de:ad:00:00:be:ef",
--        ...
--    })
--

local ret, dvb_enum = pcall(hw_enum["dvb_input"].enumerate)
if not ret then
    io.stderr:write(dvb_enum .. "\n")
    astra.exit(1)
end

io.stdout:write(
    "--\n",
    "-- automatically generated file. do not edit.\n",
    "--\n",
    "dvb_list = {\n"
)

for _, data in ipairs(dvb_enum) do
    if data.adapter == nil then
        data.adapter = "?"
    end
    if data.frontend == nil then
        data.frontend = "?"
    end
    local pfx = "adapter " .. data.adapter .. "." .. data.frontend

    if data.error ~= nil then
        io.stderr:write(pfx .. ": " .. data.error .. "\n")
    else
        if data.net_error ~= nil then
            io.stderr:write(pfx .. ": " .. data.net_error .. "\n")
        end
        if data.mac == nil then
            data.mac = "ERROR"
        end
        local s = string.format("\t{ adapter = %d, frontend = %d, mac = \"%s\" },\n",
                                data.adapter, data.frontend, data.mac)
        io.stdout:write(s)
    end
end

io.stdout:write("}\n")
astra.exit(0)
