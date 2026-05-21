cmake_minimum_required(VERSION 3.22.1)
project(omiiba_core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(omiiba_core SHARED
    AndroidBluetoothConnector.cpp
    ByteMagic.cpp
    BluetoothWrapper.cpp
    CommandSerializer.cpp
    Headphones.cpp
    ProtocolParser.cpp
    omiiba_jni.cpp
)

target_compile_options(omiiba_core PRIVATE -Wall -Wextra -Wno-unused-parameter)

find_library(log-lib log)

target_link_libraries(omiiba_core
    ${log-lib}
)

target_include_directories(omiiba_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
