# CMake file for NuriaNetwork
# Source: https://github.com/NuriaProject/Network
cmake_minimum_required(VERSION 2.8.8)

# Base configuration
PROJECT(NuriaCore)

# Enable moc
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

# Dependencies
FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)

# 
SET(NuriaNetwork_SRC
    src/httpclient.cpp
    src/nuria/httpclient.hpp
    src/httpmultipartreader.cpp
    src/nuria/httpmultipartreader.hpp
    src/httpnode.cpp
    src/nuria/httpnode.hpp
    src/httpparser.cpp
    src/nuria/httpparser.hpp
    src/httppostbodyreader.cpp
    src/nuria/httppostbodyreader.hpp
    src/httpserver.cpp
    src/nuria/httpserver.hpp
    src/httptcptransport.cpp
    src/nuria/httptcptransport.hpp
    src/httptransport.cpp
    src/nuria/httptransport.hpp
    src/httpwriter.cpp
    src/nuria/httpwriter.hpp
    src/nuria/network_global.hpp
    src/restfulhttpnode.cpp
    src/nuria/restfulhttpnode.hpp
    src/sslserver.cpp
    src/nuria/sslserver.hpp
    src/private/httpprivate.hpp
)

# C++11
LIST(APPEND CMAKE_CXX_FLAGS "-std=c++11")
add_definitions(-DNURIA_NETWORK_LIBRARY)

# Create build target
ADD_LIBRARY(NuriaNetwork SHARED ${NuriaNetwork_SRC})
target_link_libraries(NuriaNetwork NuriaCore)
QT5_USE_MODULES(NuriaNetwork Core Network)

# 
set_target_properties(
    NuriaNetwork PROPERTIES
    VERSION ${NURIA_VERSION}
    SOVERSION ${NURIA_SOVERSION}
)

# Install target
INSTALL(TARGETS NuriaNetwork 
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION lib
)

INSTALL(DIRECTORY src/nuria/ DESTINATION include/nuria
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "private" EXCLUDE
)