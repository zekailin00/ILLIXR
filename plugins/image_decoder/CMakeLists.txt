# module to build and install the offline_imu ILLIXR plugin

set(PLUGIN_NAME plugin.image_decoder${ILLIXR_BUILD_SUFFIX})

# source files, listed individually so that any changes will trigger a rebuild
add_library(${PLUGIN_NAME} SHARED plugin.cpp
        decoder.cpp
        ../../include/illixr/data_format.hpp
        ../../include/illixr/managed_thread.hpp
        ../../include/illixr/relative_clock.hpp
        ../../include/illixr/switchboard.hpp
        ../../include/illixr/threadloop.hpp
)

set(FFMPEG_PATH "/usr/local/lib")

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE ${ILLIXR_SOURCE_DIR}/include ${Eigen3_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME}
        ${Eigne3_LIBRARIES}
        spdlog::spdlog

        ${FFMPEG_PATH}/libavdevice.so
        ${FFMPEG_PATH}/libavfilter.so
        ${FFMPEG_PATH}/libavformat.so
        ${FFMPEG_PATH}/libavcodec.so
        ${FFMPEG_PATH}/libswresample.so
        ${FFMPEG_PATH}/libswscale.so
        ${FFMPEG_PATH}/libavutil.so
)

install(TARGETS ${PLUGIN_NAME} DESTINATION lib)
