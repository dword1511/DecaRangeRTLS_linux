#-------------------------------------------------
#
# Project created by QtCreator 2014-05-28T11:50:00
#
#-------------------------------------------------
cache()

QT       += core gui network xml serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DecaRangeRTLS
TEMPLATE = app
QMAKE_INFO_PLIST = Info.plist

INCLUDEPATH += models network views util tools

INCLUDEPATH += $$PWD/armadillo-3.930.0/include


LIBS += -L$$PWD/armadillo-3.930.0/lib/ -lblas

LIBS += -L$$PWD/armadillo-3.930.0/lib/ -llapack

SOURCES += main.cpp\
    RTLSDisplayApplication.cpp \
    views/mainwindow.cpp \
    network/RTLSClient.cpp \
    views/GraphicsView.cpp \
    views/GraphicsWidget.cpp \
    views/ViewSettingsWidget.cpp \
	views/MinimapView.cpp \
    views/connectionwidget.cpp \
    models/ViewSettings.cpp \
    tools/OriginTool.cpp \
    tools/RubberBandTool.cpp \
    tools/ScaleTool.cpp \
    util/QPropertyModel.cpp \
    network/SerialConnection.cpp \
    tools/trilateration.cpp

HEADERS  += \
    RTLSDisplayApplication.h \
	views/mainwindow.h \
    network/RTLSClient.h \
    views/GraphicsView.h \
    views/GraphicsWidget.h \
    views/ViewSettingsWidget.h \
    views/MinimapView.h \
    views/connectionwidget.h \
    models/ViewSettings.h \
    tools/AbstractTool.h \
    tools/OriginTool.h \
    tools/RubberBandTool.h \
    tools/ScaleTool.h \
    util/QPropertyModel.h \
    network/SerialConnection.h \
    tools/trilateration.h
FORMS    += \
    views/mainwindow.ui \
    views/GraphicsWidget.ui \
    views/ViewSettingsWidget.ui \
    views/connectionwidget.ui


RESOURCES += \
    res/resources.qrc
