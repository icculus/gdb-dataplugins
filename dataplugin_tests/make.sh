#!/bin/sh

# !!! FIXME: should probably just write a meta-makefile instead...
set -e

for feh in * ; do
    if [ -f $feh/Makefile ]; then
        echo $feh ...
        cd $feh
        make $*
        cd ..
    fi
done
