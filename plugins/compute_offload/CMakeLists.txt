# module to build and install the offline_imu ILLIXR plugin

set(PLUGIN_NAME plugin.compute_offload${ILLIXR_BUILD_SUFFIX})

# source files, listed individually so that any changes will trigger a rebuild
add_library(${PLUGIN_NAME} SHARED plugin.cpp
        offload_protocol.h
        bridge.h
        ../../include/illixr/data_format.hpp
        ../../include/illixr/managed_thread.hpp
        ../../include/illixr/relative_clock.hpp
        ../../include/illixr/switchboard.hpp
        ../../include/illixr/threadloop.hpp
        )

message("--- directory added")

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE ${ILLIXR_SOURCE_DIR}/include ${Eigen3_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME}
        ${Eigne3_LIBRARIES}
        spdlog::spdlog
        ${OpenCV_LIBRARIES}
)

install(TARGETS ${PLUGIN_NAME} DESTINATION lib)
