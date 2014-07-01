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
    src/httpclient.hpp
    src/httpmultipartreader.cpp
    src/httpmultipartreader.hpp
    src/httpnode.cpp
    src/httpnode.hpp
    src/httpparser.cpp
    src/httpparser.hpp
    src/httppostbodyreader.cpp
    src/httppostbodyreader.hpp
    src/httpserver.cpp
    src/httpserver.hpp
    src/httptcptransport.cpp
    src/httptcptransport.hpp
    src/httptransport.cpp
    src/httptransport.hpp
    src/httpwriter.cpp
    src/httpwriter.hpp
    src/network_global.hpp
    src/restfulhttpnode.cpp
    src/restfulhttpnode.hpp
    src/sslserver.cpp
    src/sslserver.hpp
    src/private/httpprivate.hpp
)

# C++11
LIST(APPEND CMAKE_CXX_FLAGS "-std=c++11 -DNURIA_NETWORK_LIBRARY")

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
INSTALL(DIRECTORY src/ DESTINATION include/nuria/network
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "private" EXCLUDE
)
