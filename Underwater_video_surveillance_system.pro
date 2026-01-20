QT       += core gui charts multimedia multimediawidgets sql network websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Database/dbmanager.cpp \
    cameraclient.cpp \
    dataview.cpp \
    headerbar.cpp \
    main.cpp \
    mainwindow.cpp \
    rulerwidget.cpp \
    videopanorama.cpp \
    websocketclient.cpp \
    streamvideowidget.cpp

HEADERS += \
    Database/dbmanager.h \
    cameraclient.h \
    dataview.h \
    headerbar.h \
    mainwindow.h \
    rulerwidget.h \
    videopanorama.h \
    websocketclient.h \
    streamvideowidget.h

FORMS += \
    dataview.ui \
    headerbar.ui \
    mainwindow.ui \
    videopanorama.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

