#! /bin/sh

# die on error
set -e

# go to source directory
cd "$(dirname "$0")"
pwd

# delete autotools cruft
if [ -f Makefile ]; then
    make maintainer-clean || :
fi

for x in \
    aclocal.m4 \
    ar-lib \
    configure \
    config.guess \
    config.log \
    config.status \
    config.sub \
    config.cache \
    config.h.in \
    config.h \
    compile \
    libtool.m4 \
    ltmain.sh \
    ltoptions.m4 \
    ltsugar.m4 \
    ltversion.m4 \
    lt~obsolete.m4 \
    ltmain.sh \
    libtool \
    ltconfig \
    missing \
    mkinstalldirs \
    depcomp \
    install-sh \
    stamp-h[0-9]* \
; do
    rm -vf "$x" "$x~"
    rm -vf "build-aux/m4/$x" "build-aux/m4/$x~"
    rm -vf "build-aux/$x" "build-aux/$x~"
done

find -type f \( -name Makefile -or -name Makefile.in \) \
    -exec rm -vf {} \;

# generate it again if needed
if [ "$1" != "clean" ]; then
    autoreconf -vif

    # don't try to delete astra core
    sed -e 's/^\s*rm -f core/rm -f/' -i configure
fi
rm -Rf autom4te.cache
