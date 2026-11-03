# FFmpeg: the MPEG-2 decoder behind sceMpegGetPicture. A prebuilt tree (include/, lib/, bin/) under
# extern/ffmpeg or a sibling checkout; without one the port builds and movies stay black.
set(YAMIPAN_FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/extern/ffmpeg")
if(NOT EXISTS "${YAMIPAN_FFMPEG_ROOT}/include/libavcodec/avcodec.h")
    set(YAMIPAN_FFMPEG_ROOT "${YAMIPAN_SIBLING_EXTERN}/ffmpeg")
endif()

set(YAMIPAN_HAVE_FFMPEG OFF)
if(NOT EXISTS "${YAMIPAN_FFMPEG_ROOT}/include/libavcodec/avcodec.h")
    message(STATUS "FFmpeg not found: building without a video decoder")
    return()
endif()

foreach(library avcodec avutil swscale)
    find_file(ffmpeg_${library}_implib NAMES lib${library}.dll.a ${library}.lib
        PATHS ${YAMIPAN_FFMPEG_ROOT}/lib NO_DEFAULT_PATH)
    if(NOT ffmpeg_${library}_implib)
        message(STATUS "FFmpeg has no import library for ${library}: building without a video decoder")
        return()
    endif()
    add_library(ffmpeg::${library} SHARED IMPORTED)
    set_target_properties(ffmpeg::${library} PROPERTIES
        IMPORTED_IMPLIB "${ffmpeg_${library}_implib}"
        INTERFACE_INCLUDE_DIRECTORIES "${YAMIPAN_FFMPEG_ROOT}/include")
endforeach()
set(YAMIPAN_HAVE_FFMPEG ON)
message(STATUS "FFmpeg: ${YAMIPAN_FFMPEG_ROOT}")
