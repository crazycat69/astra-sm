--
-- Configuration examples for the it95x:// output.
--

--
-- UT-100C
--
my_ut100 = make_it95x({
    -- Instance identifier for logging (required).
    name = "ut100_dev",

    --
    -- Device identification. Either adapter number or unique device path
    -- MUST be specified. Run `astra --devices it95x_output` for more
    -- information.
    --
    adapter = 0,
    -- OR:
    --devpath = "\\\\?\\usb#vid_1234&pid_5678", -- escape backslashes

    --
    -- Output buffer size in megabytes (optional).
    -- Supported values: 1-100 MiB, defaults to 1 MiB.
    --
    --buffer_size = 1,

    --
    -- Channel frequency and bandwidth in kHz (required).
    -- These are treated as MHz if less than or equal to 3000.
    -- Supported frequency range is device-specific.
    --
    frequency = 498000,
    bandwidth = 8000,

    --
    -- Modulation parameters (required).
    --
    coderate = "7/8",          -- "1/2", "2/3", "3/4", "5/6", "7/8"
    tx_mode = "2K",            -- "2K", "8K", "4K" (4K is ISDB-T only)
    constellation = "64QAM",   -- "QPSK", "16QAM", "64QAM"
    guardinterval = "1/32",    -- "1/4", "1/8", "1/16", "1/32"

    --
    -- Null padding and PCR restamping. By default, the output stream
    -- is padded to channel bitrate, which is automatically calculated
    -- from the modulation parameters described above.
    --
    -- The 'cbr' option can be used to override the default behavior:
    --cbr = {
    --    --rate = 10,         -- pad to 10 Mbps instead of auto-guessed rate
    --    --pcr_interval = 20, -- force PCR insertion every 20 milliseconds
    --    --pcr_delay = 50,    -- subtract 50 ms from restamped PCRs
    --    --buffer_size = 300, -- set buffer to 300 ms at target rate
    --},
    --
    -- Or to disable CBR completely:
    --cbr = false,

    --
    -- TPS cell ID (optional).
    -- Supported values: 0-65535, default is 0.
    --
    --cell_id = 0,

    --
    -- Output gain/attenuation in dB (optional).
    -- Allowed gain range is device-specific.
    --
    --gain = 0,

    --
    -- DC offset compensation for I/Q (optional).
    -- Supported values: -512 to 512, both options default to 0.
    --
    --dc_i = 0,
    --dc_q = 0,

    --
    -- I/Q calibration table (optional).
    --
    --iq_table = {
    --    -- { <frequency>, <amp>, <phi> },
    --    -- NOTE: See doc/it95x_iq for .bin converter tool.
    --},
})

-- Feed the modulator from an IPTV multicast.
make_channel({
    name = "ut100_ch",
    input = { "udp://@239.1.2.3:1234" },
    output = { "it95x://my_ut100" },
})

--
-- UT-210, DVB-T
--
my_ut210_dvb = make_it95x({
    name = "ut210_dvb_dev",

    --
    -- Basic DVB-T options behave identically to UT-100.
    --
    adapter = 1,
    frequency = 514, -- MHz
    bandwidth = 7, -- MHz
    coderate = "5/6",
    tx_mode = "8K",
    constellation = "64QAM",
    guardinterval = "1/16",
    --devpath = nil,
    --cbr = { },
    --cell_id = 0,
    --gain = 0,
    --dc_i = 0,
    --dc_q = 0,
    --iq_table = { },
    --buffer_size = 1,

    --
    -- NOTE: Following options are only supported by the newer IT9510 chips.
    --

    --
    -- Delivery system (optional).
    -- Supported values: "dvbt", "isdbt". Default is "dvbt".
    --
    --system = "dvbt",

    --
    -- ISDB-T system identification (optional).
    -- Supported values: "arib-std-b31", "isdb-tsb".
    -- Default is "arib-std-b31".
    --
    --sysid = "arib-std-b31",

    --
    -- OFS calibration values (optional).
    -- Supported values: 0 to 255, both options default to 0.
    --
    --ofs_i = 0,
    --ofs_q = 0,

    --
    -- Hardware PCR restamping (optional).
    -- This has nothing to do with the 'cbr' option described earlier.
    -- See IT9510 SDK documentation for more information on available
    -- restamping modes.
    --
    -- Supported values: 0, 1, 2 and 3.
    -- Modes 1 and 3 are limited to a maximum of 5 PCR PID's.
    -- Default is 0, which disables this feature.
    --
    --pcr_mode = 0,

    --
    -- TPS encryption key (optional).
    -- Supported values: 0 to 4294967295.
    -- Default is 0, which disables this feature.
    --
    --tps_crypt = 0,
})

-- Feed the modulator from a locally stored file.
make_channel({
    name = "ut210_dvb_ch",
    input = { "file://./test.ts#loop" },
    output = { "it95x://my_ut210_dvb" },
})

--
-- UT-210, ISDB-T (partial reception)
--
my_ut210_isdb = make_it95x({
    name = "ut210_isdb_dev",

    --
    -- Generic adapter and channel configuration.
    --
    adapter = 2,
    frequency = 473142, -- UHF channel 14
    bandwidth = 6000,
    tx_mode = "4k",
    guardinterval = "1/32",
    --devpath = nil,
    --buffer_size = 1,
    --cell_id = 0,
    --gain = 0,
    --dc_i = 0,
    --dc_q = 0,
    --ofs_i = 0,
    --ofs_q = 0,
    --iq_table = { },
    --sysid = "arib-std-b31",
    --tps_crypt = 0,

    --
    -- Enable ISDB-T partial reception (1seg + 12seg).
    -- Options 'cbr' and 'pcr_mode' are not supported in this configuration.
    --
    system = "isdbt",
    partial = true,

    --
    -- Layer A (1seg) code rate and constellation.
    --
    coderate = "3/4",
    constellation = "qpsk",

    --
    -- Layer B (12seg) code rate and constellation.
    --
    b_coderate = "7/8",
    b_constellation = "64qam",

    --
    -- Configure PID filter.
    --
    pid_list = {
        -- { <pid>, <layer> },
        -- NOTE: Up to 31 PIDs max.
        { 0, "ab" },
        { 17, "ab" },
        { 18, "ab" },
        { 100, "a" },
        { 101, "a" },
        { 102, "a" },
        { 200, "b" },
        { 201, "b" },
        { 202, "b" },
    },
    pid_layer = "ab", -- enable for layers A and B
})

-- Feed the modulator from an HTTP unicast stream.
make_channel({
    name = "ut210_isdb_ch",
    input = { "http://1.2.3.4:8001/stream1#sync" },
    output = { "it95x://my_ut210_isdb" },
})
