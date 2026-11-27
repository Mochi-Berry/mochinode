cmake_minimum_required(VERSION 3.16)
project(mb_mochinode C)
# ENGINE_INC: directory holding mbjs.h + mbjs_runtime.h (no absolute host paths).
set(ENGINE_INC "${ENGINE_INC}" CACHE PATH "engine native headers dir")
if(NOT ENGINE_INC)
  set(ENGINE_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../libmochiberry/native")
endif()
add_library(mb_mochinode SHARED src/mochinode.c)
target_include_directories(mb_mochinode PRIVATE "${ENGINE_INC}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32 AND NOT "${ENGINE_INC}" STREQUAL "")
  target_link_libraries(mb_mochinode PRIVATE )
endif()
set_target_properties(mb_mochinode PROPERTIES PREFIX "" OUTPUT_NAME "mochinode_x86_64"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
