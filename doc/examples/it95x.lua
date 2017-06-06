--
-- Configuration examples for the it95x:// output.
--

--
-- UT-100C
--
my_ut100 = make_it95x({
    name = "ut100_dev",

    -- device identification
    adapter = 0,
    -- devpath = "see --devices output",

    -- TODO: describe auto CBR padding and PCR restamping

    -- TODO: sync/buffer_size and all that crap

    -- channel frequency and bandwidth in kHz (required)
    -- NOTE: these are treated as MHz if <= 3000
    frequency = 498000,
    bandwidth = 8000,

    -- modulation parameters (required)
    coderate =
    tx_mode =
    constellation =
    guardinterval =

    -- TPS cell ID (optional)
    -- cell_id = 0,

    -- output gain/attenuation in dB (optional)
    -- gain = 0,

    -- I/Q calibration (optional)
    -- dc_i = 0,
    -- dc_q = 0,
    -- iq_table = {
    --     -- see doc/it95x_iq for .bin converter tool --
    -- },
})

make_channel({
    name = "ut100_ch",
    input = { "udp://@239.1.2.3:1234" },
    output = { "it95x://my_ut100" },
})

--
-- UT-210, DVB-T
--
my_ut210_dvb = make_it95x({
})

make_channel({
    name = "ut210_dvb_ch",
})

--
-- UT-210, ISDB-T (partial reception)
--
my_ut210_isdb = make_it95x({
    name = "ut210_isdb_ch",

    -- TODO: throw error if cbr != nil && partial
})
