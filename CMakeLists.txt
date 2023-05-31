cmake_minimum_required(VERSION 3.0)

project(heif-264-gimp-plugin)


set(GIMPTOOL "gimptool-2.0" CACHE STRING "GIMP tool")
execute_process(COMMAND ${GIMPTOOL} --cflags OUTPUT_VARIABLE CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${GIMPTOOL} --libs OUTPUT_VARIABLE LDLIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} -Wall")

set(HEIF_INC ${CMAKE_CURRENT_SOURCE_DIR}/include/heif)
set(HEIF_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/heif)
set(OPENH264_INC ${CMAKE_CURRENT_SOURCE_DIR}/include/openh264)
set(OPENH264_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/openh264)
set(SOURCES
    ./src/main.cpp
)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${OPENH264_INC}
    ${HEIF_INC}/common
    ${HEIF_INC}/reader
)

link_directories(${HEIF_LIB})
link_directories(${OPENH264_LIB})
link_libraries(heif_static)
link_libraries(heif_writer_static)
link_libraries(openh264)
link_libraries(${LDLIBS})

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
#target_link_libraries(${CMAKE_PROJECT_NAME} ${LDLIBS})
