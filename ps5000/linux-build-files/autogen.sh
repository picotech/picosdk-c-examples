#!/bin/bash
(glibtoolize --version) < /dev/null > /dev/null 2>&1 && LIBTOOLIZE=glibtoolize || LIBTOOLIZE=libtoolize

$LIBTOOLIZE --copy --force || exit 1
export AUTOMAKE="automake --foreign --add-missing"
autoreconf || exit 1

if [ "x""$1" = "x" ];
then
    SW="-build"
else
    SW="$1"
fi

if [ "x""$SW" = "x""-build" ];
then
	./configure --enable-silent-rules --prefix=/opt/picoscope CFLAGS="-Wl,-s -O2 -w -DNDEBUG" CXXFLAGS="-Wl,-s -O2 -w -DNDEBUG" || exit 1
else
    if [ "x""$SW" = "x""-debug" ];
    then
	./configure --enable-debug --prefix=/opt/picoscope CFLAGS="-g3 -O0 -Wall " CXXFLAGS="-g3 -O0 -Wall " || exit 1
    fi
fi
