cmake_minimum_required(VERSION 3.16)
project(endfield_nocollide C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)

add_library(endfield_nocol SHARED src/nocollide.cpp)
target_link_libraries(endfield_nocol PRIVATE minhook)
set_target_properties(endfield_nocol PROPERTIES PREFIX "" OUTPUT_NAME "endfield_nocol")
if(MSVC)
  target_compile_options(endfield_nocol PRIVATE /utf-8)
  set_property(TARGET endfield_nocol minhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
  target_link_options(endfield_nocol PRIVATE -static -static-libgcc -static-libstdc++ -s)
endif()
