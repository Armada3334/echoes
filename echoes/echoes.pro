#-------------------------------------------------
#echoes - RF spectrum analyzer for SDR devices
#$Id: echoes.pro 383 2018-12-20 07:45:17Z gmbertani $
#Project created by QtCreator 2015-04-22T07:21:07
#-------------------------------------------------
#CONFIG += app
CONFIG += app console c++11

QT += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets multimedia charts network


TARGET = echoes

TEMPLATE = app


SOURCES += main.cpp\
    dbif.cpp \
    dclient.cpp \
    iqbuf.cpp \
    iqdevice.cpp \
    mainwindow.cpp \
    control.cpp \
    pool.cpp \
    radio.cpp \
    circbuf.cpp \
    settings.cpp \
    waterfall.cpp \
    waterfallwidget.cpp \
    dbmpalette.cpp \
    ruler.cpp \
    scan.cpp \
    funcgen.cpp \
    postproc.cpp \
    syncrx.cpp \
    notchpopup.cpp \
    xqdir.cpp

HEADERS  += mainwindow.h \
    control.h \
    dbif.h \
    dclient.h \
    iqdevice.h \
    radio.h \
    setup.h \
    circbuf.h \
    settings.h \
    waterfall.h \
    waterfallwidget.h \
    dbmpalette.h \
    ruler.h \
    scan.h \
    funcgen.h \
    postproc.h \
    pool.h \
    iqbuf.h \
    syncrx.h \
    notchpopup.h \
    xqdir.h

FORMS    += mainwindow.ui \
    waterfall.ui \
    notchpopup.ui

#uic generated headers must go in the source directory with other header files
UI_HEADERS_DIR = .



win32-msvc:contains(QT_ARCH, x86_64) {
    message("Host is 64bit")
    message( "-->version: "$$APP_VERSION" build:"$$BUILD_DATE )

#for Windows 64bit
#built with MSVC toolchain
#Please check the Soapy libraries and includes paths are ok for your development machine first

    message("Building for Windows 64bit, compiler MSVC")
    message($$QT_ARCH)

    INCLUDEPATH += "\program files\PothosSDR\include"
    INCLUDEPATH += "\program files\PothosSDR\include\liquid"

    #The order matters here:
    LIBS += "\program files\PothosSDR\lib\SoapySDR.lib"
    LIBS += "\program files\PothosSDR\lib\libliquid.lib"


#generic windows OS
    DEFINES += WINDOZ \
        APP_VERSION=\\\"$$APP_VERSION\\\" \
        BUILD_DATE=\\\"$$BUILD_DATE\\\" \
        COMPILER_BRAND=\\\"MSVC64\\\" \
        COMPILER_BRAND_MSVC64

}

#Windows MSVC 32bit not supported (PothosSDR binaries not available for 32bit archs)
win32-msvc:contains(QT_ARCH, x86) {
    message("Host is 32bit")
    message( "*** Windows MSVC 32bit not supported (PothosSDR binaries not available for 32bit archs) ***" )
}

win32-g++:contains(QT_ARCH, x86_64) {
    message("Host is 64bit")
    message( "-->version: "$$APP_VERSION" build:"$$BUILD_DATE )



#for Windows10 64bit
#built with MinGW64 toolchain:
#I used Qt5.14.2 and MinGW64  from MSYS2
#Dependencies libraries and includes have been copied in deps/ as binaries

    message("Building for Windows 64bit, compiler MinGW")
    message($$QT_ARCH)
    QMAKE_LFLAGS += "-Wl,-Map=echoes.map"
    QMAKE_CXXFLAGS += -Wno-trigraphs
    #21sep18:
    #under g++, QMAKE_CXXFLAGS produces a sequence -Wno-trigraphs -W
    #so the -W switches preceding -W (warn_on in CONFIG set by default)
    #are ignored and the trigraphs are always replaced!!
    #I must use CXXFLAGS_THREAD instead:
    QMAKE_CXXFLAGS_THREAD += -Wno-trigraphs

    INCLUDEPATH += ./deps/inc
    INCLUDEPATH += ./deps/x86_64/Soapy/include
    #INCLUDEPATH += ./

    #The order matters here:
    LIBS += -L$$PWD/deps/x86_64
    LIBS += -lliquid
    LIBS += -L$$PWD/deps/x86_64/Soapy/bin
    LIBS += -lSoapySDR

#generic windows OS
    DEFINES += WINDOZ \
        APP_VERSION=\\\"$$APP_VERSION\\\" \
        BUILD_DATE=\\\"$$BUILD_DATE\\\" \
        COMPILER_BRAND=\\\"MinGW64\\\" \
        COMPILER_BRAND_MINGW64

}


win32-g++:contains(QMAKE_HOST.arch, x86) {
    message("Host is 32bit")
    message( "-->version: "$$APP_VERSION" build:"$$BUILD_DATE )

#for Windows7 32bit
#built with MinGW32 toolchain:
#I used Qt5.14.2 and MinGW32  from MSYS2
#Dependencies libraries and includes have been copied in deps/ as binaries

    message("Building for Windows 32bit, compiler MinGW")
    message($$QT_ARCH)

    QMAKE_CXXFLAGS += -Wno-trigraphs
    #21sep18:
    #under g++, QMAKE_CXXFLAGS produces a sequence -Wno-trigraphs -W
    #so the -W switches preceding -W (warn_on in CONFIG set by default)
    #are ignored and the trigraphs are always replaced!!
    #I must use CXXFLAGS_THREAD instead:
    QMAKE_CXXFLAGS_THREAD += -Wno-trigraphs


    INCLUDEPATH += ./deps/inc
    INCLUDEPATH += ./deps/x86_32/Soapy/include

    #The order matters here:
    LIBS += -L$$PWD/deps/x86_32
    LIBS += -lliquid
    LIBS += -L$$PWD/deps/x86_32/Soapy/bin
    LIBS += -lSoapySDR


#generic windows OS
    DEFINES += WINDOZ \
        APP_VERSION=\\\"$$APP_VERSION\\\" \
        BUILD_DATE=\\\"$$BUILD_DATE\\\" \
        COMPILER_BRAND=\\\"MinGW32\\\" 

}


#For linux and other unices: SoapySDR and libliquid are available
#as binary packages on many distros.
#Please be sure to have them installed, including the relative *-devel packages
#containing the header files!

#november 2021
#note: Qt::endl instead of endl should be used while building with Qt5.14++
#since many Linux distros still use lower versions, their installers
#should replace Qt::endl with endl in sources before compiling



unix | linux-g++ {

    message("Building for *NIX")
    QMAKE_CXXFLAGS_THREAD += -Wno-trigraphs

    #WARNING: must be 0.8 or higher
    INCLUDEPATH += /usr/include/SoapySDR
    exists(/usr/include/SoapySDR/Device.hpp) {
        #if SoapySDR is found, takes it
        message("Found SoapySDR")
        LIBS += -lSoapySDR
    }

    #WARNING: must be 1.3.2 or higher:
    INCLUDEPATH += /usr/include/liquid
    exists(/usr/include/liquid/liquid.h) {
        #if liquid is found, takes it
        message("Found Liquid")
        LIBS += -lliquid
    }

    exists(/usr/bin/paplay) {
        message("Found paplay (pulseaudio)")
        PULSEAUDIO=1
        SOX=0
    }

    exists(/usr/bin/play) {
        message("Found play (sox)")
        PULSEAUDIO=0
        SOX=1
    }

    isEmpty( BIN_INSTALL_DIR ) {
        BIN_INSTALL_DIR = /usr/bin 
    }

    isEmpty( AUX_INSTALL_DIR ) {
        AUX_INSTALL_DIR = /usr/share 
    }


    isEmpty( APP_VERSION ) {
        APP_VERSION = 0.0
    }

    isEmpty( BUILD_DATE ) {
        BUILD_DATE = 01/01/1970
    }

    target.path = $$BIN_INSTALL_DIR

    aux.path = $$AUX_INSTALL_DIR/echoes
    aux.files +=  langs/Italian.qm \
        langs/English.qm \
        docs/tests/*.rts \
        docs/*.pdf \
        docs/README.txt \
        html/*.html \
        html/*.css \
        license.txt \
        echoes.sqlite3
        
    desktop.path = $$AUX_INSTALL_DIR/applications
    desktop.files += echoes.desktop
    
    sounds.path = $$AUX_INSTALL_DIR/sounds
    sounds.files += \
        sounds/ping.wav 

    icons.path = $$AUX_INSTALL_DIR/icons/hicolor/32x32/apps
    icons.files += \
        icons/echoes_xpm32.xpm

    fonts.path = $$AUX_INSTALL_DIR/fonts/truetype
    fonts.files += \
        fonts/Gauge-Heavy.ttf \
        fonts/Gauge-Oblique.ttf \
        fonts/Gauge-Regular.ttf 


    INSTALLS += target aux desktop sounds icons fonts

    message($$INSTALLS)

    #please note: the translation will be taken from the
    #system (/usr/share..) even while debugging under IDE

    DEFINES += \
        NIX \
        AUX_PATH=\\\"$$aux.path\\\" \
        SOUNDS_PATH=\\\"$$sounds.path\\\" \
        WANT_PULSEAUDIO=$$PULSEAUDIO \
        WANT_SOX=$$SOX \
        APP_VERSION=\\\"$$APP_VERSION\\\" \
        BUILD_DATE=\\\"$$BUILD_DATE\\\" \
#       NO_NATIVE_DIALOGS \
        COMPILER_BRAND=\\\"GNU\\\" 

}

RESOURCES += \
    graphic.qrc


TRANSLATIONS = \
    langs/Italian.ts \
    langs/English.ts

CONFIG(debug, debug|release) {
    DEFINES += _DEBUG
    #message("Compiling for DEBUG")

} else {
    DEFINES += _RELEASE

    #Useful for post-mortem debugging
    #avoids symbol table stripping
    #(-s is set by default in qmake.conf)
    QMAKE_CXXFLAGS_RELEASE += -g
    QMAKE_CFLAGS_RELEASE += -g
    QMAKE_LFLAGS_RELEASE =
    #message("Compiling for RELEASE")

}

OTHER_FILES += \
    deps/db/echoes.sqlite3



