cmake_minimum_required(VERSION 3.10)
project(thread_structure)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

add_executable(thread_structure main.cpp
    src/running_control_csc/BIT.cpp
    src/running_control_csc/receiver.cpp
    src/running_control_csc/sender.cpp
    src/running_control_csc/CfgLoader.cpp
    src/running_control_csc/globals.cpp
    src/running_control_csc/Task.cpp
    src/running_control_csc/MotorControl.cpp 
    src/running_control_csc/pwm.cpp
    src/running_control_csc/Logger.cpp 
    )


target_include_directories(thread_structure
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)


target_link_libraries(thread_structure ${OpenCV_LIBS} )
