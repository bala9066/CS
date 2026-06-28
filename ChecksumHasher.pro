QT       += core gui widgets network

# zlib for ODT extraction (ZIP decompression)
win32 {
    LIBS += -lz
}
unix {
    LIBS += -lz
}
macx {
    LIBS += -lz
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Separate build directories for generated files
MOC_DIR = build/moc
OBJECTS_DIR = build/objects
RCC_DIR = build/rcc
UI_DIR = build/ui

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/aiconfigmanager.cpp \
    src/folderscanner.cpp \
    src/checksumworker.cpp \
    src/crc32.cpp \
    src/iconhelper.cpp \
    src/statisticswidget.cpp \
    src/bulkchecksumdock.cpp \
    src/odtextractor.cpp \
    src/zipreader.cpp \
    src/vdddocumentreviewer.cpp

HEADERS += \
    src/mainwindow.h \
    src/aiconfigmanager.h \
    src/folderscanner.h \
    src/checksumworker.h \
    src/crc32.h \
    src/iconhelper.h \
    src/statisticswidget.h \
    src/bulkchecksumdock.h \
    src/odtextractor.h \
    src/zipreader.h \
    src/vdddocumentreviewer.h

RESOURCES += \
    src/resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
