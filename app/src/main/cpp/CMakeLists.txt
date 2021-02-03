
cmake_minimum_required(VERSION 3.4.1)


project("ffmpegplayer")

aux_source_directory(. SOURCE)
add_library( native-lib
        SHARED
        ${SOURCE} )

#add_library(avoodec STATIC IMPORTED)
#set_target_properties(avcodec PROERTIES IMPORTED_LOCATION ...)

set(libs ${CMAKE_SOURCE_DIR}/${ANDROID_ABI})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${libs}")

#引入头文件
include_directories(include)


target_link_libraries(
        native-lib
        avfilter avformat avcodec avutil swresample swscale  z rtmp
        android
        OpenSLES
        log )