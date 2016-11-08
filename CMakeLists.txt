cmake_minimum_required (VERSION 2.6)
project (libnix)

include_directories(src include)

file(GLOB src_h include/libnix.h include/libnix/*.h)
file(GLOB src_libnix src/*.c src/*.h)

add_library(libnix ${src_h} ${src_libnix})
set_target_properties(libnix PROPERTIES PREFIX "")