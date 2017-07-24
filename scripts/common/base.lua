-- Astra Lua Library (Basic functionality)
-- https://cesbo.com/astra/
--
-- Copyright (C) 2014-2015, Andrey Dyldin <and@cesbo.com>
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

default_event = nil

ifaddr_list = nil
if utils.ifaddrs then
    ifaddr_list = utils.ifaddrs()
end

table.dump = function(t, p, i)
    if not p then p = print end
    if not i then
        p("{")
        table.dump(t, p, "    ")
        p("}")
        return
    end

    for key,val in pairs(t) do
        if type(val) == "table" then
            p(i .. tostring(key) .. " = {")
            table.dump(val, p, i .. "    ")
            p(i .. "}")
        elseif type(val) == "string" then
            p(i .. tostring(key) .. " = \"" .. val .. "\"")
        else
            p(i .. tostring(key) .. " = " .. tostring(val))
        end
    end
end

string.split = function(s, d)
    if s == nil then
        return nil
    elseif type(s) == "string" then
        --
    elseif type(s) == "number" then
        s = tostring(s)
    else
        error("[split] string required")
    end

    local p = 1
    local t = {}
    while true do
        b = s:find(d, p)
        if not b then
            table.insert(t, s:sub(p))
            return t
        end
        table.insert(t, s:sub(p, b - 1))
        p = b + 1
    end
end

--
-- DVB descriptor dumping
--

dump_descriptor_func = {}

dump_descriptor_func["cas"] = function(prefix, desc)
    local data = ""
    if desc.data then data = " data: " .. desc.data end
    log.info(prefix .. ("CAS: caid: 0x%04X pid: %d%s")
                       :format(desc.caid, desc.pid, data))
end

dump_descriptor_func["lang"] = function(prefix, desc)
    log.info(prefix .. "Language: " .. desc.lang)
end

dump_descriptor_func["maximum_bitrate"] = function(prefix, desc)
    log.info(prefix .. "Maximum bitrate: " .. desc.maximum_bitrate)
end

dump_descriptor_func["stream_id"] = function(prefix, desc)
    log.info(prefix .. "Stream ID: " .. desc.stream_id)
end

dump_descriptor_func["service"] = function(prefix, desc)
    log.info(prefix .. "Service: " .. desc.service_name)
    log.info(prefix .. "Provider: " .. desc.service_provider)
end

dump_descriptor_func["caid"] = function(prefix, desc)
    log.info(prefix .. ("CAS: caid: 0x%04X"):format(desc.caid))
end

dump_descriptor_func["teletext"] = function(prefix, desc)
    log.info(prefix .. "type: teletext")
    for _, item in pairs(desc.items) do
        log.info(prefix .. ("   page: %03X (%s), language: %s")
                           :format(item.page_number, item.page_type, item.lang))
    end
end

dump_descriptor_func["ac3"] = function(prefix, desc)
    local optflags = ""
    if desc.component_type then optflags = ", component_type: " .. desc.component_type end
    if desc.bsid then optflags = optflags .. ", bsid: " .. desc.bsid end
    if desc.mainid then optflags = optflags .. ", mainid: " .. desc.mainid end
    if desc.asvc then optflags = optflags .. ", asvc: " .. desc.asvc end
    log.info(prefix .. "Type: AC3" .. optflags)
end

dump_descriptor_func["unknown"] = function(prefix, desc)
    log.info(prefix .. "descriptor: " .. desc.data)
end

function dump_descriptor(prefix, desc)
    if dump_descriptor_func[desc.type_name] then
        dump_descriptor_func[desc.type_name](prefix, desc)
    else
        log.info(prefix .. ("unknown descriptor. type: %s 0x%02X")
                           :format(tostring(desc.type_name), desc.type_id))
    end
end

--
-- SI content dumping
--

dump_psi_info = {}

dump_psi_info["pat"] = function(name, info)
    log.info(name .. ("PAT: tsid: %d"):format(info.tsid))
    for _, program_info in pairs(info.programs) do
        if program_info.pnr == 0 then
            log.info(name .. ("PAT: pid: %d NIT"):format(program_info.pid))
        else
            log.info(name .. ("PAT: pid: %d PMT pnr: %d"):format(program_info.pid, program_info.pnr))
        end
    end
    log.info(name .. ("PAT: crc32: 0x%X"):format(info.crc32))
end

dump_psi_info["cat"] = function(name, info)
    for _, descriptor_info in pairs(info.descriptors) do
        dump_descriptor(name .. "CAT: ", descriptor_info)
    end
end

dump_psi_info["pmt"] = function(name, info)
    log.info(name .. ("PMT: pnr: %d"):format(info.pnr))
    log.info(name .. ("PMT: pid: %d PCR"):format(info.pcr))

    for _, descriptor_info in pairs(info.descriptors) do
        dump_descriptor(name .. "PMT: ", descriptor_info)
    end

    for _, stream_info in pairs(info.streams) do
        log.info(name .. ("%s: pid: %d type: %s (0x%02X)")
                         :format(stream_info.type_name,
                                 stream_info.pid,
                                 stream_info.type_description,
                                 stream_info.type_id))
        for _, descriptor_info in pairs(stream_info.descriptors) do
            dump_descriptor(name .. stream_info.type_name .. ": ", descriptor_info)
        end
    end
    log.info(name .. ("PMT: crc32: 0x%X"):format(info.crc32))
end

dump_psi_info["sdt"] = function(name, info)
    log.info(name .. ("SDT: tsid: %d"):format(info.tsid))

    for _, service in pairs(info.services) do
        log.info(name .. ("SDT: sid: %d"):format(service.sid))
        for _, descriptor_info in pairs(service.descriptors) do
            dump_descriptor(name .. "SDT:    ", descriptor_info)
        end
    end
    log.info(name .. ("SDT: crc32: 0x%X"):format(info.crc32))
end

--
-- URL parsing
--

parse_url_format = {}

parse_url_format.udp = function(url, data)
    local b = url:find("/")
    if b then
        url = url:sub(1, b - 1)
    end
    local b = url:find("@")
    if b then
        if b > 1 then
            data.localaddr = url:sub(1, b - 1)
            if ifaddr_list then
                local ifaddr = ifaddr_list[data.localaddr]
                if ifaddr and ifaddr.ipv4 then data.localaddr = ifaddr.ipv4[1] end
            end
        end
        url = url:sub(b + 1)
    end
    local b = url:find(":")
    if b then
        data.port = tonumber(url:sub(b + 1))
        data.addr = url:sub(1, b - 1)
    else
        data.port = 1234
        data.addr = url
    end

    -- check address
    if not data.port or data.port < 0 or data.port > 65535 then
        return false
    end

    local o = data.addr:split("%.")
    for _,i in ipairs(o) do
        local n = tonumber(i)
        if n == nil or n < 0 or n > 255 then
            return false
        end
    end

    return true
end

parse_url_format.rtp = parse_url_format.udp

local function parse_url_http(url, data)
    local b = url:find("/")
    if b then
        data.path = url:sub(b)
        url = url:sub(1, b - 1)
    else
        data.path = "/"
    end
    local b = url:find("@")
    if b then
        if b > 1 then
            local a = url:sub(1, b - 1)
            local bb = a:find(":")
            if bb then
                data.login = a:sub(1, bb - 1)
                data.password = a:sub(bb + 1)
            end
        end
        url = url:sub(b + 1)
    end
    local b = url:find(":")
    if b then
        data.host = url:sub(1, b - 1)
        data.port = tonumber(url:sub(b + 1))
    else
        data.host = url
        data.port = nil
    end

    return true
end

parse_url_format.http = function(url, data)
    local r = parse_url_http(url, data)
    if data.port == nil then data.port = 80 end
    return r
end

parse_url_format.https = function(url, data)
    local r = parse_url_http(url, data)
    if data.port == nil then data.port = 443 end
    return r
end

parse_url_format.np = function(url, data)
    local r = parse_url_http(url, data)
    if data.port == nil then data.port = 80 end
    return r
end

parse_url_format.dvb = function(url, data)
    data.addr = url
    return true
end

parse_url_format.file = function(url, data)
    data.filename = url
    return true
end

parse_url_format.pipe = function(url, data)
    data.command = url
    return true
end

function parse_url(url)
    if not url then return nil end

    local data={}
    local b = url:find("://")
    if not b then return nil end
    data.format = url:sub(1, b - 1)
    url = url:sub(b + 3)

    local b = url:find("#")
    local opts = nil
    if b then
        opts = url:sub(b + 1)
        url = url:sub(1, b - 1)
    end

    local _parse_url_format = parse_url_format[data.format]
    if _parse_url_format then
        if _parse_url_format(url, data) ~= true then
            return nil
        end
    else
        data.addr = url
    end

    if opts then
        local function parse_key_val(o)
            local k, v
            local x = o:find("=")
            if x then
                k = o:sub(1, x - 1)
                v = o:sub(x + 1)
            else
                k = o
                v = true
            end
            local x = k:find("%.")
            if x then
                local _k = k:sub(x + 1)
                k = k:sub(1, x - 1)
                if type(data[k]) ~= "table" then data[k] = {} end
                table.insert(data[k], { _k, v })
            else
                data[k] = v
            end
        end
        local p = 1
        while true do
            local x = opts:find("&", p)
            if x then
                parse_key_val(opts:sub(p, x - 1))
                p = x + 1
            else
                parse_key_val(opts:sub(p))
                break
            end
        end
    end

    return data
end

--
-- Event notification
--

function make_event(conf)
    -- notification service endpoint URL
    if type(conf) ~= "table" or conf.url == nil then
        error("[make_event] option 'url' is required")
    end

    local http_conf = parse_url(conf.url)
    if http_conf == nil then
        error("[make_event] invalid URL format")
    elseif http_conf.format ~= "http" and http_conf.format ~= "https" then
        error("[make_event] cannot use scheme '" .. http_conf.format .. "' for event notification")
    end

    -- interval in seconds between requests
    local interval = 30
    if conf.interval ~= nil then
        interval = tonumber(conf.interval)
    end
    if interval == nil or interval < 1 then
        error("[make_event] invalid request interval: '" .. interval .. "'")
    end

    -- maximum number of simultaneous requests
    local backlog = 10
    if conf.backlog ~= nil then
        backlog = tonumber(conf.backlog)
    end
    if backlog == nil or backlog < 1 then
        error("[make_event] invalid backlog size: '" .. backlog .. "'")
    end

    -- hostname to report to remote server
    local hostname = conf.hostname or utils.hostname()
    if type(hostname) ~= "string" then
        error("[make_event] invalid format for option 'hostname'")
    end

    -- additional HTTP headers
    local headers = {}
    if conf.headers ~= nil then
        if type(conf.headers) ~= "table" then
            error("[make_event] invalid format for option 'headers'")
        end
        for key, val in ipairs(conf.headers) do
            if type(val) ~= "string" then
                error("[make_event] invalid header value at index " .. key)
            end
            table.insert(headers, val)
        end
    end

    local event = {
        http_conf = http_conf,
        interval = interval,
        backlog = backlog,
        hostname = hostname,
        headers = headers,
        requests = {},
    }

    return event
end

function send_event(event, data)
    local http_conf = event.http_conf

    data.server = event.hostname
    local content = json.encode(data)

    local headers = {
        "User-Agent: " .. http_user_agent,
        "Content-Type: application/jsonrequest",
        "Content-Length: " .. #content,
        "Connection: close",
    }
    local add_host = true
    for _, val in ipairs(event.headers) do
        table.insert(headers, val)
        if val:lower():sub(1, 5) == "host:" then
            -- do not add duplicate Host header
            add_host = false
        end
    end
    if add_host then
        local hdr = "Host: " .. http_conf.host .. ":" .. http_conf.port
        table.insert(headers, hdr)
    end

    local conf = {
        host = http_conf.host,
        port = http_conf.port,
        path = http_conf.path,
        timeout = http_conf.timeout,
        -- FIXME: add TLS support to http_request

        method = "POST",
        headers = headers,
        content = content,

        callback = function(self, data)
            if type(data) == "table" and data.code ~= 200 then
                log.error(("[event %s] HTTP request failed (%d: %s)")
                          :format(http_conf.host, data.code, data.message))
            end
        end,
    }

    table.insert(event.requests, http_request(conf))
    while #event.requests > event.backlog do
        local req = table.remove(event.requests, 1)
        req:close()
        -- FIXME: do not disable GC by default for http_request
    end
end

function parse_event(val)
    local event = nil
    if type(val) == "string" then
        event = _G[val]
    elseif type(val) == "table" then
        event = val
    elseif val ~= false then
        event = _G["default_event"]
    end
    return event
end

--
-- TS inputs
--

init_input_module = {}
kill_input_module = {}

function init_input(conf)
    if conf == nil or conf.name == nil then
        error("[init_input] option 'name' is required")
    elseif conf.format == nil then
        error("[init_input " .. conf.name .. "] option 'format' is required")
    end

    local init_module = init_input_module[conf.format]
    if init_module == nil then
        error("[init_input " .. conf.name .. "] unknown input format: '" .. conf.format .. "'")
    end

    local instance = {
        config = conf,
        input = init_module(conf),
    }
    instance.tail = instance.input

    -- Attach TS demultiplexer if needed
    if conf.pnr == nil then
        local function need_demux()
            if conf.set_pnr ~= nil then return true end
            if conf.no_sdt == true then return true end
            if conf.no_eit == true then return true end
            if conf.map then return true end
            if conf.filter then return true end
            if conf["filter~"] then return true end
            return false
        end
        if need_demux() then conf.pnr = 0 end
    end

    if conf.pnr ~= nil then
        if conf.cam and conf.cam ~= true then
            conf.cas = true
        end
        instance.channel = channel({
            upstream = instance.tail:stream(),
            name = conf.name,
            pnr = conf.pnr,
            pid = conf.pid,
            no_sdt = conf.no_sdt,
            no_eit = conf.no_eit,
            cas = conf.cas,
            pass_sdt = conf.pass_sdt,
            pass_eit = conf.pass_eit,
            set_pnr = conf.set_pnr,
            map = conf.map,
            filter = string.split(conf.filter, ","),
            ["filter~"] = string.split(conf["filter~"], ","),
            no_reload = conf.no_reload,
        })
        instance.tail = instance.channel
    end

    -- Configure conditional access
    if conf.biss then
        -- Software BISS descrambling
        instance.decrypt = decrypt({
            upstream = instance.tail:stream(),
            name = conf.name,
            biss = conf.biss,
        })
        instance.tail = instance.decrypt
    elseif conf.cam == true then
        -- CA handled by upstream module (e.g. hardware CI CAM)
    elseif conf.cam then
        -- Software CAM
        local cam = nil
        if type(conf.cam) == "string" then
            cam = _G[conf.cam]
        elseif type(conf.cam) == "table" then
            cam = conf.cam
        end
        if type(cam) ~= "table" or type(cam.cam) ~= "function" then
            cam = nil
        end
        if cam == nil then
            error("[" .. conf.name .. "] CAM definition not found: '" .. tostring(conf.cam) .. "'")
        end
        conf.cam = nil

        local cas_pnr = nil
        if conf.pnr and conf.set_pnr then
            cas_pnr = conf.pnr
        end

        instance.decrypt = decrypt({
            upstream = instance.tail:stream(),
            name = conf.name,
            cam = cam:cam(),
            cas_data = conf.cas_data,
            cas_pnr = cas_pnr,
            disable_emm = conf.no_emm,
            ecm_pid = conf.ecm_pid,
            shift = conf.shift,
        })
        instance.tail = instance.decrypt
    end

    return instance
end

function kill_input(instance)
    if not instance then return nil end

    instance.tail = nil

    kill_input_module[instance.config.format](instance.input, instance.config)
    instance.input = nil
    instance.config = nil

    instance.channel = nil
    instance.decrypt = nil
end

--
-- Input: udp://
--

udp_input_instance_list = {}

init_input_module.udp = function(conf)
    local instance_id = tostring(conf.localaddr) .. "@" .. conf.addr .. ":" .. conf.port
    local instance = udp_input_instance_list[instance_id]

    if not instance then
        instance = { clients = 0, }
        udp_input_instance_list[instance_id] = instance

        instance.input = udp_input({
            addr = conf.addr, port = conf.port, localaddr = conf.localaddr,
            socket_size = conf.socket_size,
            renew = conf.renew,
            rtp = conf.rtp,
        })
    end

    instance.clients = instance.clients + 1
    return instance.input
end

kill_input_module.udp = function(module, conf)
    local instance_id = tostring(conf.localaddr) .. "@" .. conf.addr .. ":" .. conf.port
    local instance = udp_input_instance_list[instance_id]

    instance.clients = instance.clients - 1
    if instance.clients == 0 then
        instance.input = nil
        udp_input_instance_list[instance_id] = nil
    end
end

init_input_module.rtp = function(conf)
    conf.rtp = true
    return init_input_module.udp(conf)
end

kill_input_module.rtp = function(module, conf)
    kill_input_module.udp(module, conf)
end

--
-- Input: file://
--

init_input_module.file = function(conf)
    conf.callback = function()
        log.error("[" .. conf.name .. "] end of file")
        if conf.on_error then conf.on_error() end
    end
    return file_input(conf)
end

kill_input_module.file = function(module)
    --
end

--
-- Input: http://
--

http_user_agent = "Astra"
http_input_instance_list = {}

init_input_module.http = function(conf)
    local instance_id = conf.host .. ":" .. conf.port .. conf.path
    local instance = http_input_instance_list[instance_id]

    if not instance then
        instance = { clients = 0, }
        http_input_instance_list[instance_id] = instance

        instance.on_error = function(message)
            log.error("[" .. conf.name .. "] " .. message)
            if conf.on_error then conf.on_error(message) end
        end

        local http_conf = {
            host = conf.host,
            port = conf.port,
            path = conf.path,
            stream = true,
            sync = conf.sync,
            sync_opts = conf.sync_opts,
            timeout = conf.timeout,
            sctp = conf.sctp,
            headers = {
                "User-Agent: " .. http_user_agent,
                "Host: " .. conf.host .. ":" .. conf.port,
                "Connection: close",
            }
        }

        if conf.login and conf.password then
            local auth = base64.encode(conf.login .. ":" .. conf.password)
            table.insert(http_conf.headers, "Authorization: Basic " .. auth)
        end

        local timer_conf = {
            interval = 5,
            callback = function(self)
                instance.timeout:close()
                instance.timeout = nil

                if instance.request then instance.request:close() end
                instance.request = http_request(http_conf)
            end
        }

        http_conf.callback = function(self, response)
            if not response then
                instance.request:close()
                instance.request = nil
                instance.timeout = timer(timer_conf)

            elseif response.code == 200 then
                if instance.timeout then
                    instance.timeout:close()
                    instance.timeout = nil
                end

                instance.transmit:set_upstream(self:stream())

            elseif response.code == 301 or response.code == 302 then
                if instance.timeout then
                    instance.timeout:close()
                    instance.timeout = nil
                end

                instance.request:close()
                instance.request = nil

                local o = parse_url(response.headers["location"])
                if o then
                    http_conf.host = o.host
                    http_conf.port = o.port
                    http_conf.path = o.path
                    http_conf.headers[2] = "Host: " .. o.host .. ":" .. o.port

                    log.info("[" .. conf.name .. "] Redirect to http://" .. o.host .. ":" .. o.port .. o.path)
                    instance.request = http_request(http_conf)
                else
                    instance.on_error("HTTP Error: Redirect failed")
                    instance.timeout = timer(timer_conf)
                end

            else
                instance.request:close()
                instance.request = nil
                instance.on_error("HTTP Error: " .. response.code .. ":" .. response.message)
                instance.timeout = timer(timer_conf)
            end
        end

        instance.transmit = transmit({ instance_id = instance_id })
        instance.request = http_request(http_conf)
    end

    instance.clients = instance.clients + 1
    return instance.transmit
end

kill_input_module.http = function(module)
    local instance_id = module.__options.instance_id
    local instance = http_input_instance_list[instance_id]

    instance.clients = instance.clients - 1
    if instance.clients == 0 then
        if instance.timeout then
            instance.timeout:close()
            instance.timeout = nil
        end
        if instance.request then
            instance.request:close()
            instance.request = nil
        end
        instance.transmit = nil
        http_input_instance_list[instance_id] = nil
    end
end

--
-- Input: dvb://
--

dvb_input_instance_list = {}
dvb_list = nil

local function on_status_dvb(instance, stats)
    local event = instance.event
    local data = instance.event_data

    -- calculate average bitrate over the last 10 seconds
    local sample_secs = 10

    local last = data.last or stats
    local delta = stats.packets - last.packets
    data.last = stats

    if data.delta_log == nil then
        data.delta_log = {}
    end

    if delta < 0 then
        -- packet counter wraparound, reuse last delta
        if #data.delta_log > 0 then
            delta = data.delta_log[#data.delta_log]
        else
            delta = 0
        end
    end

    table.insert(data.delta_log, delta)
    while #data.delta_log > sample_secs do
        table.remove(data.delta_log, 1)
    end

    local bitrate = 0
    for _, val in ipairs(data.delta_log) do
        bitrate = bitrate + val
    end
    bitrate = ((bitrate / sample_secs) * 1504) / 1000
    bitrate = tonumber(string.format("%d", bitrate))

    -- send reports at configured interval or on signal status change
    local do_send = false

    if stats.lock ~= last.lock
       or stats.ber ~= last.ber
       or stats.uncorrected ~= last.uncorrected
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
        send_event(event, {
            type = "dvb",
            stream = instance.name,

            adapter = instance.conf.adapter,
            frontend = instance.conf.frontend,
            devpath = instance.conf.devpath,

            signal = stats.strength,
            snr = stats.quality,
            ber = stats.ber,
            unc = stats.uncorrected,
            lock = stats.lock,
            packets = stats.packets,

            bitrate = bitrate,
        })
    end
end

function dvb_tune(conf)
    if type(conf) ~= "table" or conf.name == nil then
        error("[dvb_tune] option 'name' is required")
    end
    local name = conf.name

    -- look up MAC if needed
    if conf.mac ~= nil then
        conf.adapter = nil
        conf.frontend = nil
        conf.devpath = nil

        local found = false
        if dvb_list ~= nil then
            local mac = conf.mac:upper()
            for _, val in ipairs(dvb_list) do
                if val.mac ~= nil and val.mac:upper() == mac then
                    local msg = ""

                    if val.adapter ~= nil then
                        msg = msg .. ", adapter: " .. val.adapter
                        conf.adapter = val.adapter
                        if val.frontend ~= nil then
                            msg = msg .. ", frontend: " .. val.frontend
                            conf.frontend = val.frontend
                        end
                    end
                    if val.devpath ~= nil then
                        msg = msg .. ", devpath: '" .. val.devpath .. "'"
                        conf.devpath = val.devpath
                    end

                    if #msg > 0 then
                        log.debug("[dvb_tune " .. name .. "] MAC '" .. mac .. "' found" .. msg)
                        found = true
                        break
                    end
                end
            end
        else
            log.warning("[dvb_tune " .. name .. "] dvb_list is not set; searching adapters by MAC will not work")
        end

        if not found then
            error("[dvb_tune " .. name .. "] failed to find adapter with MAC address '" .. conf.mac .. "'")
        end
    end

    -- fix up adapter and frontend numbers
    if conf.adapter ~= nil then
        local a = string.split(tostring(conf.adapter), "%.")
        if #a == 1 then
            conf.adapter = tonumber(a[1])
        elseif #a == 2 then
            -- handle 'adapter = "X.Y"' syntax
            if conf.frontend ~= nil then
                log.warning("[dvb_tune " .. name .. "] ignoring 'frontend' option")
            end
            conf.adapter = tonumber(a[1])
            conf.frontend = tonumber(a[2])
        else
            error("[dvb_tune " .. name .. "] option 'adapter' has wrong format: '" .. conf.adapter .. "'")
        end

        -- make sure both options are filled in
        if conf.adapter == nil then
            conf.adapter = 0
        end
        if conf.frontend == nil then
            conf.frontend = 0
        end
    elseif conf.frontend ~= nil then
        error("[dvb_tune " .. name .. "] option 'adapter' must be specified if 'frontend' is present")
    end

    -- apply option wrappers
    if conf.tp ~= nil then
        -- satellite transponder: 'tp = "FFFFF:P:SSSSS"'
        local a = string.split(conf.tp, ":")
        if #a ~= 3 then
            error("[dvb_tune " .. name .. "] option 'tp' has wrong format: '" .. conf.tp .. "'")
        end
        conf.frequency, conf.polarization, conf.symbolrate = a[1], a[2], a[3]
    end

    if conf.lnb ~= nil then
        -- satellite LNB: 'lnb = "LO:HI:SW"'
        local a = string.split(conf.lnb, ":")
        if #a ~= 3 then
            error("[dvb_tune " .. name .. "] option 'lnb' has wrong format: '" .. conf.lnb .. "'")
        end
        conf.lof1, conf.lof2, conf.slof = a[1], a[2], a[3]
    end

    if conf.unicable ~= nil then
        -- Unicable: 'unicable = "S:F"'
        local a = string.split(conf.unicable, ":")
        if #a ~= 2 then
            error("[dvb_tune " .. name .. "] option 'unicable' has wrong format: '" .. conf.unicable .. "'")
        end
        conf.uni_scr, conf.uni_frequency = a[1], a[2]
        -- TODO: put this in conf.diseqc
        --       raise error if conf.diseqc ~= nil
        -- postponed until linux dvb_input rewrite
    end

    if conf.type ~= nil then
        -- alternate format for S2/T2: 'type = "s", s2 = true'
        local t = conf.type:lower()
        if t == "s" or t == "dvbs" then
            if conf.s2 then conf.type = "s2" end
        elseif t == "t" or t == "dvbt" then
            if conf.t2 then conf.type = "t2" end
        end
    end

    -- configure event notification
    local event = parse_event(conf.event)
    if conf.event and event == nil then
        error("[dvb_tune " .. name .. "] event definition not found: '" .. tostring(conf.event) .. "'")
    end
    conf.event = nil

    -- reuse module instance if it already exists
    local instance_id = string.format("a%s:f%s:d%s",
                                      tostring(conf.adapter),
                                      tostring(conf.frontend),
                                      tostring(conf.devpath))

    local instance = dvb_input_instance_list[instance_id]
    if instance == nil then
        conf.id = instance_id
        instance = {
            name = name,
            conf = conf,
            clients = 0,
        }
        if event ~= nil then
            instance.event = event
            conf.callback = function(data)
                on_status_dvb(instance, data)
            end
        end
    end

    return instance
end

init_input_module.dvb = function(conf)
    local instance = nil
    if conf.addr == nil or #conf.addr == 0 then
        -- ad-hoc configuration
        instance = dvb_tune(conf)
    else
        -- pre-defined tuner
        instance = _G[conf.addr]
    end

    local function check_def()
        if not instance then return false end
        if not instance.name then return false end
        if not instance.conf then return false end
        if not instance.conf.id then return false end
        return true
    end
    if not check_def() then
        error("[" .. conf.name .. "] dvb tuner definition not found")
    end

    if instance.clients == 0 then
        -- TODO: create dveo:// scheme for dveo dvb master rx/tx
        local func = _G["dvb_input"]
        if tostring(instance.conf.type):lower() == "asi" then
            func = _G["asi_input"]
        end

        instance.module = func(instance.conf)
        instance.event_data = {}
        dvb_input_instance_list[instance.conf.id] = instance
    end

    if conf.cam == true and conf.pnr ~= nil then
        instance.module:ca_set_pnr(conf.pnr, true)
    end

    instance.clients = instance.clients + 1
    return instance.module
end

kill_input_module.dvb = function(module, conf)
    local instance_id = module.__options.id
    local instance = dvb_input_instance_list[instance_id]

    if conf.cam == true and conf.pnr ~= nil then
        module:ca_set_pnr(conf.pnr, false)
    end

    instance.clients = instance.clients - 1
    if instance.clients == 0 then
        instance.module = nil
        instance.event_data = nil
        dvb_input_instance_list[instance_id] = nil
    end
end

--
-- Input: pipe://
--

function make_pipe(conf)
    if conf.name == nil then
        error("[make_pipe] option 'name' is required")
    end

    conf.name = "pipe " .. conf.name
    conf.stream = true

    return pipe_generic(conf)
end

init_input_module.pipe = function(conf)
    if conf.command then
        local instance = _G[conf.command]
        local module_name = tostring(instance)

        if module_name == "pipe_generic" then
            return instance
        end
    end

    conf.name = "pipe_input " .. conf.name
    conf.stream = true

    return pipe_generic(conf)
end

kill_input_module.pipe = function(module, conf)
    --
end

--
-- Input: t2mi://
--

t2mi_input_instance_list = {}

function make_t2mi_decap(conf)
    if conf.name == nil then
        error("[make_t2mi_decap] option 'name' is required")
    end
    local instance = t2mi_input_instance_list[conf.name]
    if instance ~= nil then
        return instance
    end

    if conf.input == nil then
        error("[make_t2mi_decap] option 'input' is required")
    end
    local input = parse_url(conf.input)
    if input == nil then
        error("[make_t2mi_decap] wrong input format")
    end
    input.name = conf.name

    instance = {
        name = conf.name,
        input = input,
        conf = {
            name = conf.name,
            pnr = conf.pnr,
            pid = conf.pid,
            plp = conf.plp,
        },
        clients = 0,
    }

    return instance
end

init_input_module.t2mi = function(conf)
    local instance = nil
    if conf.addr == nil or #conf.addr == 0 then
        -- ad-hoc configuration
        local input = parse_url(conf.t2mi_input)
        if not input then
            error("[" .. conf.name .. "] wrong t2mi input format")
        end
        for k, v in pairs(conf) do
            if k ~= "format" and k ~= "addr" and not k:find("t2mi_", 1) then
                input[k] = v
            end
        end
        instance = {
            name = conf.name,
            input = input,
            conf = {
                name = conf.name,
                pnr = conf.t2mi_pnr,
                pid = conf.t2mi_pid,
                plp = conf.t2mi_plp,
            },
            clients = 0,
        }
    else
        -- pre-defined decapsulator
        instance = _G[conf.addr]
    end

    local function check_def()
        if not instance then return false end
        if not instance.name then return false end
        if not instance.input then return false end
        if not instance.conf then return false end
        return true
    end
    if not check_def() then
        error("[" .. conf.name .. "] t2mi decap definition not found")
    end

    if instance.clients == 0 then
        instance.source = init_input(instance.input)
        instance.conf.upstream = instance.source.tail:stream()
        instance.t2mi = t2mi_decap(instance.conf)

        t2mi_input_instance_list[instance.name] = instance
    end

    instance.clients = instance.clients + 1
    return instance.t2mi
end

kill_input_module.t2mi = function(module, conf)
    local instance_id = module.__options.name
    local instance = t2mi_input_instance_list[instance_id]

    instance.clients = instance.clients - 1
    if instance.clients == 0 then
        instance.t2mi = nil
        kill_input(instance.source)
        t2mi_input_instance_list[instance_id] = nil
    end
end

--
-- Input: reload://
--

init_input_module.reload = function(conf)
    return transmit({
        timer = timer({
            interval = tonumber(conf.addr),
            callback = function(self)
                self:close()
                astra.reload()
            end,
        })
    })
end

kill_input_module.reload = function(module)
    module.__options.timer:close()
end

--
-- Input: stop://
--

init_input_module.stop = function(conf)
    return transmit({})
end

kill_input_module.stop = function(module)
    --
end

--
-- Option parsing
--

function astra_usage()
    print("Usage: " .. argv0 .. " [APP] [OPTIONS]")
    print("\nAvailable Applications:")

    local function spaces(num)
        local sp = ""
        for i = 1, num, 1 do
            sp = sp .. " "
        end
        return sp
    end
    for _, app in pairs(require("applist")) do
        local txt = string.gsub(app.txt, "\n", "\n" .. spaces(24))
        local splen = 20 - #app.arg
        if splen < 0 then splen = 1 end

        print("    " .. app.arg .. spaces(splen) .. txt)
    end

print([[
    SCRIPT              launch Astra script (implies --stream)

Common Options:
    -h, --help          command line arguments
    -v, --version       version number
    --pid FILE          create PID-file
    --syslog NAME       send log messages to syslog
    --log FILE          write log to file
    --no-stdout         do not print log messages into console
    --color             colored log messages in console
    --debug             print debug messages
]])

    if _G.options_usage then
        print("Application Options:")
        print(_G.options_usage)
    end

    astra.exit()
end

function astra_version()
    print(astra.fullname)
    astra.exit()
end

astra_options = {
    ["-h"] = function(idx)
        astra_usage()
        return 0
    end,
    ["--help"] = function(idx)
        astra_usage()
        return 0
    end,
    ["-v"] = function(idx)
        astra_version()
        return 0
    end,
    ["--version"] = function(idx)
        astra_version()
        return 0
    end,
    ["--pid"] = function(idx)
        if argv[idx + 1] == nil then
            print("--pid: this option requires an argument")
            astra.exit(1)
        end
        pidfile(argv[idx + 1])
        return 1
    end,
    ["--syslog"] = function(idx)
        if argv[idx + 1] == nil then
            print("--syslog: this option requires an argument")
            astra.exit(1)
        end
        log.set({ syslog = argv[idx + 1] })
        return 1
    end,
    ["--log"] = function(idx)
        if argv[idx + 1] == nil then
            print("--log: this option requires an argument")
            astra.exit(1)
        end
        log.set({ filename = argv[idx + 1] })
        return 1
    end,
    ["--no-stdout"] = function(idx)
        log.set({ stdout = false })
        return 0
    end,
    ["--color"] = function(idx)
        log.set({ color = true })
        return 0
    end,
    ["--debug"] = function(idx)
        log.set({ debug = true })
        return 0
    end,
}

function astra_parse_options(idx)
    function set_option(idx)
        local a = argv[idx]
        local c = nil

        if _G.options then c = _G.options[a] end
        if not c then c = astra_options[a] end
        if not c and _G.options then c = _G.options["*"] end

        if not c then return -1 end
        local ac = c(idx)
        if ac == -1 then return -1 end
        idx = idx + ac + 1
        return idx
    end

    while idx <= #argv do
        local next_idx = set_option(idx)
        if next_idx == -1 then
            print("unknown option: " .. argv[idx])
            astra.exit(1)
        end
        idx = next_idx
    end
end
