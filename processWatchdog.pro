#
# Process Watchdog Application Manager
# Qt project file
#
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    src/filecmd.c \
    src/ini.c \
    src/apps.c \
    src/log.c \
    src/main.c \
    src/server.c \
    src/stats.c \
    src/test.c \
    src/utils.c

HEADERS += \
    src/ini.h \
    src/filecmd.h \
    src/apps.h \
    src/log.h \
    src/server.h \
    src/stats.h \
    src/test.h \
    src/utils.h
