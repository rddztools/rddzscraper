# -------------------------------------------------
# Project created by QtCreator 2012-03-26T16:01:28
# -------------------------------------------------
QT += core gui network sql xmlpatterns
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
unix:win32:!macx:TARGET = RDDZ_Scraper
macx: TARGET = "RDDZ Scraper"
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    scraper.cpp \
    tools.cpp \
    config.cpp \
    network.cpp \
    appabout.cpp \
    backlinks.cpp \
    footprintmgr.cpp \
    qreplytimeout.cpp \
    scraptable.cpp \
    deletebox.cpp \
    worker.cpp \
    searchreplacebox.cpp \
    comboboxdelegate.cpp \
    scrapenginemanager.cpp
HEADERS += mainwindow.h \
    scraper.h \
    tools.h \
    config.h \
    network.h \
    appabout.h \
    backlinks.h \
    footprintmgr.h \
    qreplytimeout.h \
    scraptable.h \
    deletebox.h \
    worker.h \
    searchreplacebox.h \
    comboboxdelegate.h \
    scrapenginemanager.h
FORMS += mainwindow.ui \
    licence.ui \
    appabout.ui \
    footprintmgr.ui \
    deletebox.ui \
    searchreplacebox.ui \
    scrapenginemanager.ui
OTHER_FILES += \
    rddzscraper.rc \
    dist/scraper.js
RESOURCES += \
    ressources/ressources.qrc \
    ressources/semanager.qrc
TRANSLATIONS =  \
    rddzscraper_fr.ts \
    rddzscraper_es.ts

VERSION = 1.7.8
DEFINES += APPVERSION=\\\"$$VERSION\\\"

QT_FATAL_WARNINGS=1
QMAKE_CXXFLAGS_RELEASE -= -O2
unix:!macx: QMAKE_CXXFLAGS_RELEASE += -O3 -s
win32: QMAKE_CXXFLAGS_RELEASE += -O3 -s
macx: QMAKE_CXXFLAGS_RELEASE += -O3
win32: RC_FILE = rddzscraper.rc
macx: ICON = rddzscraper.icns


DISTFILES +=

macx {
#QMAKE_MAC_SDK = macosx10.14
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.14
}

LIBS += -lz
