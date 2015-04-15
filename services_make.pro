TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    getopt.cpp \
    go_curl.cpp

HEADERS += \
    getopt.h \
    go_curl.h

DEFINES = WIN32 UNICODE CURL_STATICLIB
QMAKE_CXXFLAGS_RELEASE = -O2 -MT


#QMAKE_LFLAGS_CONSOLE = /subsystem:windows

include(deployment.pri)


win32:CONFIG(release, debug|release): LIBS += -L"C:/Program Files/Microsoft SDKs/Windows/v7.1/Lib/" -lAdvAPI32 -lUser32


win32: LIBS += -L$$PWD/../../Build_source/curl-7.41.0/builds/libcurl-vc10-x86-release-static-ipv6-sspi-winssl/lib/ -llibcurl_a

INCLUDEPATH += $$PWD/../../Build_source/curl-7.41.0/builds/libcurl-vc10-x86-release-static-ipv6-sspi-winssl/include/curl
DEPENDPATH += $$PWD/../../Build_source/curl-7.41.0/builds/libcurl-vc10-x86-release-static-ipv6-sspi-winssl/include/curl

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/../../Build_source/curl-7.41.0/builds/libcurl-vc10-x86-release-static-ipv6-sspi-winssl/lib/libcurl_a.lib
else:win32-g++: PRE_TARGETDEPS += $$PWD/../../Build_source/curl-7.41.0/builds/libcurl-vc10-x86-release-static-ipv6-sspi-winssl/lib/liblibcurl_a.a
