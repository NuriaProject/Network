QT += network
QT -= gui

DESTDIR = ../lib
TARGET = NuriaNetworkQt$$QT_MAJOR_VERSION
TEMPLATE = lib

CONFIG += create_prl c++11
DEFINES += NETWORK_LIBRARY
DEFINES += NURIA_MODULE=\\\"\"NuriaNetwork\\\"\"

isEmpty(INCDIR):INCDIR=..

INCLUDEPATH += . ../Core
LIBS += -L../lib -lNuriaCoreQt$$QT_MAJOR_VERSION

SOURCES += httpclient.cpp \
    httpnode.cpp \
    httpserver.cpp \
    sslserver.cpp \
    restfulhttpnode.cpp \
    httpparser.cpp \
    httptransport.cpp \
    httptcptransport.cpp

GLOBAL_HEADERS = network_global.hpp \
    httpclient.hpp \
    httpnode.hpp \
    restfulhttpnode.hpp \
    httpserver.hpp \
    sslserver.hpp \
    httpparser.hpp \
    httptransport.hpp \
    httptcptransport.hpp
    
PRIVATE_HEADERS = \
    private/httpprivate.hpp

HEADERS += $$GLOBAL_HEADERS $$PRIVATE_HEADERS

# Install stuff
includes.path = $$INCDIR/nuria
includes.files = $$GLOBAL_HEADERS

INSTALLS += includes
