
function main()
    if not out_file then
        astra_usage()
    end

    if not dvbls then
        log.error("dvbls module is not found")
        os.exit(1)
    end

    local fh = assert(io.open(out_file, "w"))
    fh:write(
        "--\n",
        "-- automatically generated file. do not edit.\n",
        "--\n",
        "persistent_dvbs = {\n"
    )

    -- list adapters
    local dvb_list = dvbls()
    for _,dvb_info in pairs(dvb_list) do
        local strerror = "Adapter " .. dvb_info.adapter .. ", device " .. dvb_info.device .. ": "
        local fatal = false

        if dvb_info.error then
            -- failed to open frontend
            log.error(strerror .. "check_device_fe(): " .. dvb_info.error)
            fatal = true
        elseif dvb_info.net_error then
            -- failed to read MAC address
            log.error(strerror .. "check_device_net(): " .. dvb_info.net_error)
        end

        if not fatal then
            s = string.format("\t{ adapter = %d, device = %d, mac = \"%s\" },\n",
                              dvb_info.adapter, dvb_info.device, dvb_info.mac)

            fh:write(s)
        end
    end

    fh:write("}\n")
    fh:close()

    os.exit(0)
end

out_file = nil

options_usage = [[
    FILE                Output file
]]

options = {
    ["*"] = function(idx)
        out_file = argv[idx]
        return 0
    end,
}
