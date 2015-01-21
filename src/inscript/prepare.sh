#!/bin/bash
set -e
cd "$(dirname "$0")"
gcc -std=c99 -o mkscript mkscript.c
echo -n >inscript.h
for i in analyze base dvbls dvbwrite relay stream; do
    ./mkscript ${i} ../../scripts/${i}.lua >>inscript.h
done
rm -f mkscript
