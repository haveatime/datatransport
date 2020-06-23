QT -= gui
QT += network

TARGET        = dtlocalbusplugin
DESTDIR       = ../bin
TEMPLATE      = lib
CONFIG       += plugin debug

INCLUDEPATH  += ../datatransportinterface

DEFINES += DT_LOCALBUS_PLUGIN_EXPORT

HEADERS       = dtlocalbusplugin.h

SOURCES       = dtlocalbusplugin.cpp

LIBS += -L../bin -ldatatransportinterface

win32 {
LIBS += -lws2_32
}
