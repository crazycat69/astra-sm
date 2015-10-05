EXTRA_DIST = pm5544.png
patterndir = $(datadir)/astra

if HAVE_FFMPEG
pattern_DATA = \
    test_audio.ts \
    test_mpeg2.ts \
    test_mpeg4_sd.ts \
    test_mpeg4_hd.ts

PICTURE_FILE = $(srcdir)/pm5544.png
SOUND_FILE = tone.wav
BUILT_SOURCES = $(SOUND_FILE)

TONEGEN = $(top_builddir)/tools/tonegen$(BUILD_EXEEXT)
$(SOUND_FILE):
	$(AM_V_GEN)$(TONEGEN) -t 30 -o $(SOUND_FILE)

test_audio.ts: $(SOUND_FILE)
	$(AM_V_GEN)$(FFMPEG) -loglevel fatal -y \
	    -i $(SOUND_FILE) -acodec mp2 -ab 256000 \
	    -mpegts_service_id 500 -mpegts_pmt_start_pid 500 -mpegts_start_pid 501 \
	    -metadata service_provider="$(PACKAGE_STRING)" \
	    -metadata service_name="MP2 Test Stream" \
	    $@

test_mpeg2.ts: $(SOUND_FILE) $(PICTURE_FILE)
	$(AM_V_GEN)$(FFMPEG) -loglevel fatal -y \
	    -loop 1 -i $(PICTURE_FILE) -i $(SOUND_FILE) -shortest \
	    -acodec mp2 -ab 256000 -vcodec mpeg2video -r 25 -q:v 5 \
	    -vf scale=720:576 -pix_fmt yuv420p -g 50 -maxrate 1500k \
	    -flags +ilme+ildct -top 1 -bf 1 \
	    -mpegts_service_id 500 -mpegts_pmt_start_pid 500 -mpegts_start_pid 501 \
	    -metadata service_provider="$(PACKAGE_STRING)" \
	    -metadata service_name="MPEG2 Test Stream" \
	    $@

test_mpeg4_sd.ts: $(SOUND_FILE) $(PICTURE_FILE)
	$(AM_V_GEN)$(FFMPEG) -loglevel fatal -y \
	    -loop 1 -i $(PICTURE_FILE) -i $(SOUND_FILE) -shortest \
	    -acodec mp2 -ab 256000 -vcodec libx264 -r 25.000 -crf 1 \
	    -preset ultrafast -profile:v main -level 3.0 \
	    -x264opts fps=25.000:tff=1:keyint=100 -maxrate 1500k \
	    -vf scale=720:576 -pix_fmt yuv420p \
	    -mpegts_service_id 500 -mpegts_pmt_start_pid 500 -mpegts_start_pid 501 \
	    -metadata service_provider="$(PACKAGE_STRING)" \
	    -metadata service_name="MPEG4/SD Test Stream" \
	    $@

test_mpeg4_hd.ts: $(SOUND_FILE) $(PICTURE_FILE)
	$(AM_V_GEN)$(FFMPEG) -loglevel fatal -y \
	    -loop 1 -i $(PICTURE_FILE) -i $(SOUND_FILE) -shortest \
	    -acodec mp2 -ab 256000 -vcodec libx264 -r 25.000 -crf 1 \
	    -preset ultrafast -profile:v main -level 3.0 \
	    -x264opts fps=25.000:tff=1:keyint=100 -maxrate 1500k \
	    -vf scale=1920:1080 -aspect 16:9 -pix_fmt yuv420p \
	    -mpegts_service_id 500 -mpegts_pmt_start_pid 500 -mpegts_start_pid 501 \
	    -metadata service_provider="$(PACKAGE_STRING)" \
	    -metadata service_name="MPEG4/HD Test Stream" \
	    $@

BUILT_SOURCES += $(pattern_DATA)
CLEANFILES = $(BUILT_SOURCES)
else
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(patterndir)
endif
