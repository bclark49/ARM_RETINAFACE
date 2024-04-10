QT       += core gui widgets

#greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    client.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += /usr/local/include/opencv4 /usr/include/gstreamer-1.0 /usr/include/glib-2.0 /usr/lib/aarch64-linux-gnu/glib-2.0/include 
LIBS += $(shell pkg-config --libs opencv4 gstreamer-1.0 gstreamer-app-1.0 glib-2.0) -lgomp -lpthread -lm

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
