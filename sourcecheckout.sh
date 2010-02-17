#! /bin/bash

usage () {
    echo "Usage: ./sourcecheckout.sh" 1>&2
    exit 1
}

if [ ! -f main.c ]; then
    echo "sourcecheckout.sh must be run from the top xwrits source directory" 1>&2
    usage
fi
if [ ! -f ../liblcdf/include/lcdf/clp.h ]; then
    echo "sourcecheckout.sh can't find ../liblcdf" 1>&2
    usage
fi

test -d include || mkdir include
test -d include/lcdf || mkdir include/lcdf
test -d include/lcdfgif || mkdir include/lcdfgif
for i in inttypes; do
    ln -sf ../../../liblcdf/include/lcdf/$i.h include/lcdf/$i.h
done
for i in gif gifx; do
    ln -sf ../../../liblcdf/include/lcdfgif/$i.h include/lcdfgif/$i.h
done
for i in fmalloc strerror; do
    ln -sf ../liblcdf/liblcdf/$i.c $i.c
done
for i in giffunc gifread giftoc gifx; do
    ln -sf ../liblcdf/liblcdfgif/$i.c $i.c
done
