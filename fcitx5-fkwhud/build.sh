#! /bin/sh -e
# rm -rf ./CMakeCache.txt ./build
cmake -B build -DCMAKE_INSTALL_PREFIX=~/local -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
cmake --install build
export FCITX_ADDON_DIRS=/home/ccjmne/local/lib/fcitx5:/usr/lib/fcitx5
fcitx5 -r
