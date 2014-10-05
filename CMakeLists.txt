# CMake file for NuriaNetwork
# Source: https://github.com/NuriaProject/Network
cmake_minimum_required(VERSION 2.8.8)

# Base configuration
PROJECT(NuriaNetwork)

cmake_policy(SET CMP0020 NEW)

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
    src/httpurlencodedreader.cpp
    src/nuria/httpurlencodedreader.hpp
    src/httpnode.cpp
    src/nuria/httpnode.hpp
    src/httpparser.cpp
    src/nuria/httpparser.hpp
    src/httppostbodyreader.cpp
    src/nuria/httppostbodyreader.hpp
    src/httpfilter.cpp
    src/nuria/httpfilter.hpp
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
    src/private/standardfilters.cpp
    src/private/standardfilters.hpp
    src/private/crc32.h
    src/private/adler32.h
)

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

# Add public include directories to target
set_property(TARGET NuriaNetwork APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)

# Install target
INSTALL(TARGETS NuriaNetwork EXPORT NuriaNetworkConfig
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

INSTALL(DIRECTORY src/nuria/ DESTINATION include/nuria
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "private" EXCLUDE
)

INSTALL(EXPORT NuriaNetworkConfig DESTINATION lib/cmake/NuriaNetwork)

# Add Tests
enable_testing()
add_unittest(NAME tst_httpclient QT Network NURIA NuriaNetwork
             SOURCES httpmemorytransport.cpp httpmemorytransport.hpp)
add_unittest(NAME tst_httpmultipartreader QT Network NURIA NuriaNetwork)
add_unittest(NAME tst_httpurlencodedreader QT Network NURIA NuriaNetwork)
add_unittest(NAME tst_httpparser QT Network NURIA NuriaNetwork)
add_unittest(NAME tst_httpwriter QT Network NURIA NuriaNetwork)
add_unittest(NAME tst_restfulhttpnode QT Network NURIA NuriaNetwork
             SOURCES httpmemorytransport.cpp httpmemorytransport.hpp testnode.hpp)
