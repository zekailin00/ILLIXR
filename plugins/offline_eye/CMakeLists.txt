# module to build and install the offline_eye ILLIXR plugin

set(PLUGIN_NAME plugin.offline_eye${ILLIXR_BUILD_SUFFIX})

# source files, listed individually so that any changes will trigger a rebuild
add_library(${PLUGIN_NAME} SHARED
    plugin.cpp
    data_loading.hpp
    ../../include/illixr/plugin.hpp
    ../../include/illixr/opencv_data_types.hpp
    ../../include/illixr/phonebook.hpp
    ../../include/illixr/pose_prediction.hpp
    ../../include/illixr/relative_clock.hpp
    ../../include/illixr/threadloop.hpp
    ../../include/illixr/csv_iterator.hpp
    ../../include/illixr/data_format.hpp
)

# if building on CentOS make sure we use the correct OpenCV
if(BUILD_OPENCV)
    add_dependencies(${PLUGIN_NAME} OpenCV_Viz)
endif()

set(ONNXRUNTIME_ROOT_PATH /home/zekailin00/Downloads/onnxruntime-linux-x64-1.19.2)
set(ONNXRUNTIME_INCLUDE_DIRS
  ${ONNXRUNTIME_ROOT_PATH}/include/
  ${ONNXRUNTIME_ROOT_PATH}
  ${ONNXRUNTIME_ROOT_PATH}/include/core/providers
)

set(ONNXRUNTIME_LIB ${ONNXRUNTIME_ROOT_PATH}/lib/libonnxruntime.so)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${ILLIXR_SOURCE_DIR}/include
    ${Eigen3_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
)
target_link_libraries(${PLUGIN_NAME}
    ${OpenCV_LIBRARIES}
    ${Eigen3_LIBRARIES}
    spdlog::spdlog
    ${ONNXRUNTIME_LIB}
)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)

install(TARGETS ${PLUGIN_NAME} DESTINATION lib)

message(STATUS "PyTorch Include Dirs: ${TORCH_INCLUDE_DIRS}")
message(STATUS "PyTorch Libraries: ${TORCH_LIBRARIES}")