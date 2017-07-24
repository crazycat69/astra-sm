-- Astra Lua Library (Streaming)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2013-2015, Andrey Dyldin <and@cesbo.com>
--               2015-2017, Artem Kharitonov <artem@3phase.pw>
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

--
-- Analyze
--

local function on_status_channel(instance, stats)
    local event = instance.event
    local data = instance.event_data

    local input_id = instance.active_input_id
    if input_id == 0 then input_id = 1 end
    local input = instance.input[input_id]

    local last = data.last or stats
    data.last = stats

    local do_send = false

    if stats.on_air ~= last.on_air
       or stats.total.scrambled ~= last.total.scrambled
    then
        do_send = true
        data.ticks = math.random(event.interval)
    elseif data.ticks ~= nil then
        data.ticks = data.ticks - 1
        if data.ticks <= 0 then
            do_send = true
            data.ticks = event.interval
        end
    else
        data.ticks = math.random(event.interval)
    end

    if do_send then
        local dvb_name = nil
        if input.config.format == "dvb" then
            -- FIXME: figure out a prettier way of retrieving DVB instance's name
            dvb_name = input.input.input.__options.name
        end

        send_event(event, {
            type = "channel",
            channel = instance.config.name,

            stream = dvb_name,
            pnr = input.config.pnr,

            ready = stats.on_air,
            scrambled = stats.total.scrambled,
            bitrate = stats.total.bitrate,
            cc_error = stats.total.cc_errors,
            pes_error = stats.total.pes_errors,

            -- TODO: add output list
        })
    end
end

local function on_analyze_channel(channel_data, input_id, data)
    local input_data = channel_data.input[input_id]

    if data.error then
        log.error("[" .. input_data.config.name .. "] Error: " .. data.error)

    elseif data.psi then
        if dump_psi_info[data.psi] then
            dump_psi_info[data.psi]("[" .. input_data.config.name .. "] ", data)
        else
            log.error("[" .. input_data.config.name .. "] Unknown PSI: " .. data.psi)
        end

    elseif data.analyze then

        if data.on_air ~= input_data.on_air then
            local analyze_message = "[" .. input_data.config.name .. "] Bitrate:" .. data.total.bitrate .. "Kbit/s"

            if data.on_air == false then
                local m = nil
                if data.total.scrambled then
                    m = " Scrambled"
                else
                    m = " PES:" .. data.total.pes_errors .. " CC:" .. data.total.cc_errors
                end
                log.error(analyze_message .. m)
            else
                log.info(analyze_message)
            end

            input_data.on_air = data.on_air

            if channel_data.delay > 0 then
                if input_data.on_air == true and channel_data.active_input_id == 0 then
                    start_reserve(channel_data)
                else
                    channel_data.delay = channel_data.delay - 1
                    input_data.on_air = nil
                end
            else
                start_reserve(channel_data)
            end
        end

        if channel_data.event ~= nil then
            on_status_channel(channel_data, data)
        end
    end
end

--
-- Failover
--

function start_reserve(channel_data)
    local active_input_id = 0
    for input_id, input_data in ipairs(channel_data.input) do
        if input_data.on_air == true then
            channel_data.transmit:set_upstream(input_data.input.tail:stream())
            log.info("[" .. channel_data.config.name .. "] Active input #" .. input_id)
            active_input_id = input_id
            break
        end
    end

    if active_input_id == 0 then
        local next_input_id = 0
        for input_id, input_data in ipairs(channel_data.input) do
            if not input_data.input then
                next_input_id = input_id
                break
            end
        end
        if next_input_id == 0 then
            log.error("[" .. channel_data.config.name .. "] Failed to switch to reserve")
        else
            channel_init_input(channel_data, next_input_id)
        end
    else
        channel_data.active_input_id = active_input_id
        channel_data.delay = channel_data.config.timeout

        for input_id, input_data in ipairs(channel_data.input) do
            if input_data.input and input_id > active_input_id then
                channel_kill_input(channel_data, input_id)
                log.debug("[" .. channel_data.config.name .. "] Destroy input #" .. input_id)
                input_data.on_air = nil
            end
        end
        collectgarbage()
    end
end

--
-- Input
--

function channel_init_input(channel_data, input_id)
    local input_data = channel_data.input[input_id]

    -- merge channel-wide and input-specific PID maps
    if channel_data.config.map or input_data.config.map then
        local merged_map = {}
        if channel_data.config.map then
            merged_map = channel_data.config.map
        end
        if input_data.config.map then
            for _, input_val in pairs(input_data.config.map) do
                local found = false
                for chan_key, chan_val in pairs(merged_map) do
                    if input_val[1] == chan_val[1] then
                        -- override channel-wide mapping
                        merged_map[chan_key] = input_val
                        found = true
                    end
                end
                if found == false then
                    table.insert(merged_map, input_val)
                end
            end
        end
        input_data.config.map = merged_map
    end

    if channel_data.config.set_pnr then
        input_data.config.set_pnr = channel_data.config.set_pnr
    end

    input_data.input = init_input(input_data.config)

    if input_data.config.no_analyze ~= true then
        input_data.analyze = analyze({
            upstream = input_data.input.tail:stream(),
            name = input_data.config.name,
            cc_limit = input_data.config.cc_limit,
            bitrate_limit = input_data.config.bitrate_limit,
            callback = function(data)
                on_analyze_channel(channel_data, input_id, data)
            end,
        })
    end

    -- TODO: init additional modules

    channel_data.transmit:set_upstream(input_data.input.tail:stream())
end

function channel_kill_input(channel_data, input_id)
    local input_data = channel_data.input[input_id]

    -- TODO: kill additional modules

    input_data.analyze = nil
    input_data.on_air = nil

    kill_input(input_data.input)
    input_data.input = nil

    if input_id == 1 then channel_data.delay = 3 end
    channel_data.input[input_id] = { config = input_data.config, }
end

--
-- Output
--

init_output_option = {}
kill_output_option = {}

init_output_option.biss = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    if biss_encrypt == nil then
        log.error("[" .. output_data.config.name .. "] biss_encrypt module is not found")
        return nil
    end

    output_data.biss = biss_encrypt({
        upstream = channel_data.tail:stream(),
        key = output_data.config.biss,
    })

    channel_data.tail = output_data.biss
end

kill_output_option.biss = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.biss = nil
end

init_output_option.cbr = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    output_data.cbr = ts_cbr({
        upstream = channel_data.tail:stream(),
        name = output_data.config.name,
        rate = output_data.config.cbr,
        pcr_interval = output_data.config.cbr_pcr_interval,
        pcr_delay = output_data.config.cbr_pcr_delay,
        buffer_size = output_data.config.cbr_buffer_size,
    })

    channel_data.tail = output_data.cbr
end

kill_output_option.cbr = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.cbr = nil
end

-- TODO: remove this eventually
init_output_option.remux = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    error("[" .. output_data.config.name .. "] " ..
          "remux is no longer available, please use 'cbr=<BPS>' instead")
end

kill_output_option.remux = function(channel_data, output_id)
    --
end

init_output_module = {}
kill_output_module = {}

function channel_init_output(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    for key,_ in pairs(output_data.config) do
        if init_output_option[key] then
            init_output_option[key](channel_data, output_id)
        end
    end

    init_output_module[output_data.config.format](channel_data, output_id)
end

function channel_kill_output(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    for key,_ in pairs(output_data.config) do
        if kill_output_option[key] then
            kill_output_option[key](channel_data, output_id)
        end
    end

    kill_output_module[output_data.config.format](channel_data, output_id)
    channel_data.output[output_id] = { config = output_data.config, }
end

--
-- Output: udp://, rtp://
--

init_output_module.udp = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    local localaddr = output_data.config.localaddr
    if localaddr and ifaddr_list then
        local ifaddr = ifaddr_list[localaddr]
        if ifaddr and ifaddr.ipv4 then localaddr = ifaddr.ipv4[1] end
    end
    output_data.output = udp_output({
        upstream = channel_data.tail:stream(),
        addr = output_data.config.addr,
        port = output_data.config.port,
        ttl = output_data.config.ttl,
        localaddr = localaddr,
        socket_size = output_data.config.socket_size,
        rtp = (output_data.config.format == "rtp"),
        sync = output_data.config.sync,
        sync_opts = output_data.config.sync_opts,
    })
end

kill_output_module.udp = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.output = nil
end

init_output_module.rtp = function(channel_data, output_id)
    init_output_module.udp(channel_data, output_id)
end

kill_output_module.rtp = function(channel_data, output_id)
    kill_output_module.udp(channel_data, output_id)
end

--
-- Output: file://
--

init_output_module.file = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.output = file_output({
        upstream = channel_data.tail:stream(),
        filename = output_data.config.filename,
        m2ts = output_data.config.m2ts,
        buffer_size = output_data.config.buffer_size,
        aio = output_data.config.aio,
        directio = output_data.config.directio,
    })
end

kill_output_module.file = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.output = nil
end

--
-- Output: http://
--

http_output_client_list = {}
http_output_instance_list = {}

function http_output_client(server, client, request)
    local client_data = server:data(client)

    if not request then
        http_output_client_list[client_data.client_id] = nil
        client_data.client_id = nil
        return nil
    end

    local function get_unique_client_id()
        local _id = math.random(10000000, 99000000)
        if http_output_client_list[_id] ~= nil then
            return nil
        end
        return _id
    end

    repeat
        client_data.client_id = get_unique_client_id()
    until client_data.client_id ~= nil

    http_output_client_list[client_data.client_id] = {
        server = server,
        client = client,
        request = request,
        st   = os.time(),
    }
end

function http_output_on_request(server, client, request)
    local client_data = server:data(client)

    if not request then
        if client_data.client_id then
            local channel_data = client_data.output_data.channel_data
            channel_data.clients = channel_data.clients - 1
            if channel_data.clients == 0 and channel_data.input[1].input ~= nil then
                for input_id, input_data in ipairs(channel_data.input) do
                    if input_data.input then
                        channel_kill_input(channel_data, input_id)
                    end
                end
                channel_data.active_input_id = 0
            end

            http_output_client(server, client, nil)
            collectgarbage()
        end
        return nil
    end

    client_data.output_data = server.__options.channel_list[request.path]
    if not client_data.output_data then
        server:abort(client, 404)
        return nil
    end

    http_output_client(server, client, request)

    local channel_data = client_data.output_data.channel_data
    channel_data.clients = channel_data.clients + 1

    local allow_channel = function()
        if not channel_data.input[1].input then
            channel_init_input(channel_data, 1)
        end

        server:send(client, {
            upstream = channel_data.tail:stream(),
            buffer_size = client_data.output_data.config.buffer_size,
            buffer_fill = client_data.output_data.config.buffer_fill,
        })
    end

    allow_channel()
end

init_output_module.http = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    local instance_id = output_data.config.host .. ":" .. output_data.config.port
    local instance = http_output_instance_list[instance_id]

    if not instance then
        instance = http_server({
            addr = output_data.config.host,
            port = output_data.config.port,
            sctp = output_data.config.sctp,
            route = {
                { "/*", http_upstream({ callback = http_output_on_request }) },
            },
            channel_list = {},
        })
        http_output_instance_list[instance_id] = instance
    end

    output_data.instance = instance
    output_data.instance_id = instance_id
    output_data.channel_data = channel_data

    instance.__options.channel_list[output_data.config.path] = output_data
end

kill_output_module.http = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]

    local instance = output_data.instance
    local instance_id = output_data.instance_id

    for _, client in pairs(http_output_client_list) do
        if client.server == instance then
            instance:close(client.client)
        end
    end

    instance.__options.channel_list[output_data.config.path] = nil

    local is_instance_empty = true
    for _ in pairs(instance.__options.channel_list) do
        is_instance_empty = false
        break
    end

    if is_instance_empty then
        instance:close()
        http_output_instance_list[instance_id] = nil
    end

    output_data.instance = nil
    output_data.instance_id = nil
    output_data.channel_data = nil
end

--
-- Output: np://
--

init_output_module.np = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    local conf = output_data.config

    local http_conf = {
        host = conf.host,
        port = conf.port,
        path = conf.path,
        upstream = channel_data.tail:stream(),
        buffer_size = conf.buffer_size,
        buffer_fill = conf.buffer_size,
        timeout = conf.timeout,
        sctp = conf.sctp,
        headers = {
            "User-Agent: " .. http_user_agent,
            "Host: " .. conf.host,
            "Connection: keep-alive",
        },
    }

    local timer_conf = {
        interval = 5,
        callback = function(self)
            output_data.timeout:close()
            output_data.timeout = nil

            if output_data.request then output_data.request:close() end
            output_data.request = http_request(http_conf)
        end
    }

    http_conf.callback = function(self, response)
        if not response then
            output_data.request:close()
            output_data.request = nil
            output_data.timeout = timer(timer_conf)

        elseif response.code == 200 then
            if output_data.timeout then
                output_data.timeout:close()
                output_data.timeout = nil
            end

        elseif response.code == 301 or response.code == 302 then
            if output_data.timeout then
                output_data.timeout:close()
                output_data.timeout = nil
            end

            output_data.request:close()
            output_data.request = nil

            local o = parse_url(response.headers["location"])
            if o then
                http_conf.host = o.host
                http_conf.port = o.port
                http_conf.path = o.path
                http_conf.headers[2] = "Host: " .. o.host

                log.info("[" .. conf.name .. "] Redirect to http://" .. o.host .. ":" .. o.port .. o.path)
                output_data.request = http_request(http_conf)
            else
                log.error("[" .. conf.name .. "] NP Error: Redirect failed")
                output_data.timeout = timer(timer_conf)
            end

        else
            output_data.request:close()
            output_data.request = nil
            log.error("[" .. conf.name .. "] NP Error: " .. response.code .. ":" .. response.message)
            output_data.timeout = timer(timer_conf)
        end
    end

    output_data.request = http_request(http_conf)
end

kill_output_module.np = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    if output_data.timeout then
        output_data.timeout:close()
        output_data.timeout = nil
    end
    if output_data.request then
        output_data.request:close()
        output_data.request = nil
    end
end

--
-- Output: pipe://
--

init_output_module.pipe = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.output = pipe_generic({
        upstream = channel_data.tail:stream(),
        name = "pipe_output " .. channel_data.config.name .. " #" .. output_id,
        command = output_data.config.command,
        restart = output_data.config.restart,
    })
end

kill_output_module.pipe = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    output_data.output = nil
end

--
-- Output: it95x://
--

function make_it95x(conf)
    -- check parameters
    if _G["it95x_output"] == nil then
        error("[make_it95x] this feature is not available on this platform")
    end
    if conf == nil or conf.name == nil then
        error("[make_it95x] option 'name' is required")
    end

    -- instantiate output module
    local cbr_conf = conf.cbr
    conf.cbr = nil

    local mod_it95x = it95x_output(conf)
    local head = mod_it95x

    -- instantiate CBR muxer if needed
    local mod_cbr = nil
    if cbr_conf ~= false then
        if cbr_conf == nil then
            cbr_conf = {}
        elseif type(cbr_conf) == "number" then
            cbr_conf = {
                rate = cbr_conf,
            }
        elseif type(cbr_conf) ~= "table" then
            error("[make_it95x " .. conf.name .. "] option 'cbr' has wrong format")
        end

        if cbr_conf.rate == nil then
            local bitrate = mod_it95x:bitrate()
            if type(bitrate) == "number" then
                -- use channel rate as target bitrate
                cbr_conf.rate = bitrate
            else
                -- CBR is disabled by default for ISDB-T partial mode
                log.debug("[make_it95x " .. conf.name .. "] not using CBR for partial reception mode")
            end
        end

        if cbr_conf.rate ~= nil then
            local units = " bps"
            if cbr_conf.rate <= 1000 then
                units = " mbps"
            end
            log.debug("[make_it95x " .. conf.name .. "] padding output TS to " .. cbr_conf.rate .. units)

            cbr_conf.name = conf.name
            mod_cbr = ts_cbr(cbr_conf)
            mod_it95x:set_upstream(mod_cbr:stream())
            head = mod_cbr
        end
    end

    local instance = {
        name = conf.name,
        head = head,
        it95x = mod_it95x,
        cbr = mod_cbr,
        busy = false,
    }

    return instance
end

init_output_module.it95x = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    local name = output_data.config.name

    local instance = _G[output_data.config.addr]
    local function check_instance()
        if instance == nil then return false end
        if type(instance) ~= "table" then return false end
        if instance.name == nil then return false end
        if instance.it95x == nil then return false end
        if tostring(instance.it95x) ~= "it95x_output" then return false end
        if instance.head == nil then return false end
        return true
    end
    if not check_instance() then
        error("[" .. name .. "] it95x definition not found")
    end
    if instance.busy then
        error("[" .. name .. "] it95x output '" .. instance.name .. "' is already in use")
    end

    instance.busy = true
    instance.head:set_upstream(channel_data.tail:stream())
    output_data.output = instance
end

kill_output_module.it95x = function(channel_data, output_id)
    local output_data = channel_data.output[output_id]
    local instance = output_data.output

    output_data.output = nil
    instance.busy = false
end

--
-- Transform
--

init_transform_module = {}
kill_transform_module = {}

function stream_init_transform(stream_data, xfrm_id)
    local xfrm_data = stream_data.transform[xfrm_id]

    local conf = {}
    for k, v in pairs(xfrm_data.config) do
        conf[k] = v
    end

    conf.format = nil
    conf.upstream = stream_data.tail:stream()

    local xfrm = init_transform_module[xfrm_data.config.format](conf)
    xfrm_data.transform = xfrm
    stream_data.tail = xfrm
end

function stream_kill_transform(stream_data, xfrm_id)
    local xfrm_data = stream_data.transform[xfrm_id]

    kill_transform_module[xfrm_data.config.format](xfrm_data)
    stream_data.transform[xfrm_id] = { config = xfrm_data.config }
end

--
-- Transform: cbr
--

init_transform_module.cbr = function(conf)
    return ts_cbr(conf)
end

kill_transform_module.cbr = function(instance)
    --
end

--
-- Transform: pipe
--

init_transform_module.pipe = function(conf)
    conf.name = "pipe_xfrm " .. conf.name
    conf.stream = true

    return pipe_generic(conf)
end

kill_transform_module.pipe = function(instance)
    --
end

--
-- Channel
--

channel_list = {}

function make_channel(conf)
    if conf == nil or conf.name == nil then
        error("[make_channel] option 'name' is required")
    elseif conf.input == nil or #conf.input == 0 then
        error("[make_channel " .. conf.name .. "] option 'input' is required")
    end

    if conf.transform == nil then conf.transform = {} end
    if conf.output == nil then conf.output = {} end
    if conf.timeout == nil then conf.timeout = 0 end
    if conf.enable == nil then conf.enable = true end

    if conf.enable == false then
        log.info("[make_channel " .. conf.name .. "] channel is disabled via configuration")
        return nil
    end

    local event = parse_event(conf.event)
    if conf.event and event == nil then
        error("[make_channel " .. conf.name .. "] event definition not found: '" .. tostring(conf.event) .. "'")
    end
    conf.event = nil

    local instance = {
        config = conf,
        input = {},
        transform = {},
        output = {},
        delay = 3,
        clients = 0,
        event = event,
        event_data = {},
    }

    local function add_urls(obj)
        local url_list = conf[obj]
        local parsed_list = instance[obj]
        local init_list = _G["init_" .. obj .. "_module"]
        local function check_item_cfg(tbl)
            if type(tbl) ~= "table" then return false end
            if not tbl.format then return false end
            if not init_list[tbl.format] then return false end
            return true
        end
        for n, url in ipairs(url_list) do
            local item = {}
            if type(url) == "string" then
                item.config = parse_url(url)
            elseif type(url) == "table" then
                if url.url then
                    local u = parse_url(url.url)
                    for k, v in pairs(u) do url[k] = v end
                end
                item.config = url
            end
            if not check_item_cfg(item.config) then
                error("[make_channel " .. conf.name .. "] invalid URL format for " .. obj .. " #" .. n)
            end
            item.config.name = conf.name .. " #" .. n
            table.insert(parsed_list, item)
        end
    end

    add_urls("input")
    add_urls("transform")
    add_urls("output")

    if #instance.output == 0 then
        instance.clients = 1
    else
        for _, o in pairs(instance.output) do
            if o.config.format ~= "http" or o.config.keep_active == true then
                instance.clients = instance.clients + 1
            end
        end
    end

    instance.active_input_id = 0
    instance.transmit = transmit()
    instance.tail = instance.transmit

    for xfrm_id in ipairs(instance.transform) do
        stream_init_transform(instance, xfrm_id)
    end

    if instance.clients > 0 then
        channel_init_input(instance, 1)
    end

    for output_id in ipairs(instance.output) do
        channel_init_output(instance, output_id)
    end

    table.insert(channel_list, instance)
    return instance
end

function kill_channel(instance)
    local channel_id = 0
    if instance ~= nil then
        for key, value in pairs(channel_list) do
            if value == instance then
                channel_id = key
                break
            end
        end
    end
    if channel_id == 0 then
        error("[kill_channel] channel not found")
    end

    while #instance.input > 0 do
        channel_kill_input(instance, 1)
        table.remove(instance.input, 1)
    end
    instance.input = nil

    while #instance.transform > 0 do
        stream_kill_transform(instance, 1)
        table.remove(instance.transform, 1)
    end
    instance.transform = nil

    while #instance.output > 0 do
        channel_kill_output(instance, 1)
        table.remove(instance.output, 1)
    end
    instance.output = nil

    instance.tail = nil
    instance.transmit = nil
    instance.config = nil

    table.remove(channel_list, channel_id)
    collectgarbage()
end

function find_channel(key, value)
    for _, instance in pairs(channel_list) do
        if instance.config[key] == value then
            return instance
        end
    end
    return nil
end
