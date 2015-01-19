# CMake file for 'autobahn'
# Sub-project of the NuriaProject Framework Network module.

cmake_minimum_required(VERSION 2.8.8)
PROJECT(AutobahnServer)

cmake_policy(SET CMP0020 NEW)

# Binary
add_executable(autobahn autobahn.cpp)
target_link_libraries(autobahn NuriaNetwork)
QT5_USE_MODULES(autobahn Core Network)

# 
add_custom_target(
    websockettest
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runAutobahn.sh $<TARGET_FILE:autobahn>
    WORKING_DIRECTORY .
    DEPENDS autobahn
    VERBATIM
)

# 
message(STATUS "  To run AutobahnTests for WebSockets run: make websockettest")
