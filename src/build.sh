#!/bin/sh
gcc -m32 -std=gnu99 -g3 steamcompmgr.c -o steamcompmgr -L /valve/steam-runtime/runtime-release/i386/usr/lib/i386-linux-gnu/ -L /usr/lib32/nvidia-319/ -lGL -lXrender -lXcomposite -lXfixes -lXdamage -lXxf86vm
