QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    electrodewindow.cpp \
    historywindow.cpp \
    main.cpp \
    mainwindow.cpp \
    sessionwindow.cpp \
    startwindow.cpp

HEADERS += \
    electrodewindow.h \
    historywindow.h \
    mainwindow.h \
    sessionwindow.h \
    startwindow.h

FORMS +=

TRANSLATIONS += \
    GUI_en_US.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    build/Desktop_Qt_6_9_2_MinGW_64_bit-Debug/regain-logo.png

RESOURCES += \
    logo.qrc
