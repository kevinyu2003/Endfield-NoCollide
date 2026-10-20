#!/bin/sh
# Cross-compile from Linux / WSL with mingw-w64:  sh build_mingw.sh
set -e
cd "$(dirname "$0")"
cmake -S . -B build-mingw -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw
echo "Built: build-mingw/endfield_nocol.dll"
