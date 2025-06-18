cmake_minimum_required(VERSION 3.15)
project(mendelcam LANGUAGES C CXX)

# ───────────────────────────────
# 1.  Tool‑chain / Cross compile
# ───────────────────────────────
# Expect the caller (build script) to have sourced the Petalinux SDK so that
#  * CC / CXX               → arm-linux-gnueabihf-gcc / g++
#  * SDKTARGETSYSROOT       → /workspace/peta_prj/Clocked_pwm_peta_v2/zynq_sdk/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi
# If you want to build natively, just omit these cache entries.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Allow callers to override from the command‑line, e.g.
#   cmake .. -DTARGET_CPU=cortex-a9 -DTARGET_FPU=neon -DTARGET_FLOAT=hard
set(TARGET_CPU    cortex-a9   CACHE STRING "Target CPU for -mcpu=<cpu>")
set(TARGET_FPU    neon         CACHE STRING "Target FPU for -mfpu=<fpu>")
set(TARGET_FLOAT  hard         CACHE STRING "hard/softfp")

# ───────────────────────────────
# 2.  Global compile options
# ───────────────────────────────
add_compile_options(
    -mcpu=${TARGET_CPU}
    -mfpu=${TARGET_FPU}
    -mfloat-abi=${TARGET_FLOAT}
    -O2
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
    -Wformat
    -Wformat-security
    -Werror=format-security)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# ───────────────────────────────
# 3.  Third‑party paths (adjust to your workspace layout)
# ───────────────────────────────
set(WORK_ROOT /workspace/TPU)

# Edge‑TPU & TensorFlow‑Lite
set(TFLITE_INC  "${WORK_ROOT}/tensorflow_src")
set(TFLITE_LIB  "${WORK_ROOT}/tensorflow_src/bazel-out/armhf-opt/bin/tensorflow/lite/libtensorflowlite.so")
set(EDGETPU_INC "${WORK_ROOT}/libedgetpu/tflite/public")
set(EDGETPU_LIB "${WORK_ROOT}/libedgetpu/out/throttled/armv7a/libedgetpu.so")
set(CORAL_INC "${WORK_ROOT}/libcoral")

# glog / abseil / flatbuffers (pre‑built)
set(GLOG_INC  "${WORK_ROOT}/glog")
set(GLOG_LIB  "${WORK_ROOT}/glog/build/libglog.a")
set(ABSL_INC  "${WORK_ROOT}/abseil-cpp")
set(ABSL_LIB  "${WORK_ROOT}/abseil-cpp/build_so/absl/libabseil.so")
set(FLAT_INC  "${WORK_ROOT}/flatbuffers/include")
set(FLAT_LIB  "${WORK_ROOT}/tensorflow_src/bazel-out/armhf-opt/bin/external/flatbuffers/src/libflatbuffers.pic.a")

# opencv
set(OPCV_INC  "/workspace/peta_prj/Clocked_pwm_peta_v2/zynq_sdk/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi/usr/include/opencv4")
find_package(OpenCV REQUIRED)

# Extract the folder part so we don’t rely on $(dirname …) (Bash) inside CMake
get_filename_component(TFLITE_LIB_DIR  ${TFLITE_LIB}  DIRECTORY)
get_filename_component(EDGETPU_LIB_DIR ${EDGETPU_LIB} DIRECTORY)
link_directories(${TFLITE_LIB_DIR} ${EDGETPU_LIB_DIR})


# ───────────────────────────────
# 4.  GStreamer (pkg‑config)
# ───────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
    gstreamer-1.0>=1.16
    gstreamer-app-1.0
    gstreamer-video-1.0
    gstreamer-allocators-1.0)

# ───────────────────────────────
# 5.  Target and include / link setup
# ───────────────────────────────
# 내 파일 경로 쓰는 곳
file(GLOB SRC
    # ${CMAKE_CURRENT_SOURCE_DIR}/tflite_wrapper.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/detection_camera.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/label_utils.cc
   # ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/BIT.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/receiver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/sender.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/CfgLoader.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/globals.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/Task.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/MotorControl.cpp 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/pwm.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/Logger.cpp 
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/Task.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/running_control_csc/ImgTaskv2.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/image_processing_csc/ImageProcessor.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/tracking_csc/sort_tracker.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/detecting_csc/edgetpu_detector.cpp
    )

add_executable(EOIR_system ${SRC})

# Includes
# --------
target_include_directories(EOIR_system PRIVATE
    ${TFLITE_INC}
    ${EDGETPU_INC}
    ${GLOG_INC}
    ${ABSL_INC}
    ${FLAT_INC}
    ${GST_INCLUDE_DIRS}
    ${OPCV_INC})
target_include_directories(EOIR_system
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Link libraries (order matters for static libs)
# ------------------------------------------------
set(EXTRA_LIBS
    ${GST_LIBRARIES}
    ${TFLITE_LIB}
    ${EDGETPU_LIB}
    ${GLOG_LIB}
    ${ABSL_LIB}
    ${FLAT_LIB}
    pthread)

# target_link_libraries(thread_structure ${OpenCV_LIBS} )
target_link_libraries(EOIR_system PRIVATE ${EXTRA_LIBS} ${OpenCV_LIBS})

# ───────────────────────────────
# 6.  Install rules (optional)
# ───────────────────────────────
# install(TARGETS detection DESTINATION bin)

# ./build cmake command
#cmake .. \
#  -DCMAKE_SYSROOT=${SDKTARGETSYSROOT} \
#  -DCMAKE_FIND_ROOT_PATH=${SDKTARGETSYSROOT} \
#  -DTARGET_CPU=cortex-a9 \
#  -DTARGET_FPU=neon \
#  -DTARGET_FLOAT=hard   \
#  -DCMAKE_BUILD_TYPE=Release
