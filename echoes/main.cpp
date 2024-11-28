/******************************************************************************

    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
    Copyright (C) 2018-2022  Giuseppe Massimo Bertani gmbertani(a)users.sourceforge.net


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.
    

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, http://www.gnu.org/copyleft/gpl.html

*******************************************************************************
Code style adopted:

- tab = 4 spaces
- camelcase symbol names for classes, methods and members
- type names start in uppercase, other names in lowercase
- underscores are used only in #define macros, always uppercase
- braces aligned horizontally

please note: the project has been moved from svn to git in Nov 2018
so the following tags won't be updated anymore:
*******************************************************************************
$Rev:: 363                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-09 00:03:43 +01#$: Date of last commit
$Id: main.cpp 363 2018-11-08 23:03:43Z gmbertani $
*******************************************************************************/
#include <signal.h>
#include <QAudioDeviceInfo>
#include <QtCore>
#include <QtWidgets>

#include <SoapySDR/Logger.hpp>


#ifdef NIX
    //under linux use journal/syslog
    //instead of log file
    #include <unistd.h>
    #include <syslog.h>
#else
    //mimics the syslog LOG_* enumeration
    #define LOG_DEBUG   ECHOES_LOG_ALL
    #define LOG_INFO    ECHOES_LOG_INFO
    #define LOG_WARNING ECHOES_LOG_WARNINGS
    #define LOG_CRIT    ECHOES_LOG_CRITICAL
    #define LOG_ALERT   ECHOES_LOG_FATAL

//log file is not used under linux
static QFile* logFile = nullptr;
static QString logFileName;

#endif


#include "setup.h"
#include "settings.h"
#include "postproc.h"
#include "control.h"
#include "mainwindow.h"

#ifdef __GNUG__
    #ifdef _DEBUG
         #warning "COMPILING FOR DEBUG"
    #else
         #warning "COMPILING FOR RELEASE"
    #endif
#endif

#ifdef _MSC_VER
    #ifdef _DEBUG
        #pragma message("COMPILING FOR DEBUG")
    #else
        #pragma message("COMPILING FOR RELEASE")
    #endif
#endif


//public globals
uint    logLevel = ECHOES_LOG_INFO;
XQDir*  prgDir = nullptr;
QThread* ctrlThread = nullptr;

//private globals
static bool             verbose = false;
static QString*         confFileName = nullptr;
static QElapsedTimer*   uptime = nullptr;
static QMutex*          mx = nullptr;
static QCoreApplication* specApp = nullptr;
static QApplication*    guiSpecApp = nullptr;
static Control*         ctrl = nullptr;


static void myMessageOutput(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    QMutexLocker ml(mx);

    QThread* current = QThread::currentThread();
    QString oName("Unknown Thread");
    QString stringBuffer;
    QTextStream ts(&stringBuffer);
    int syslogPriority = LOG_DEBUG;

    if(current != nullptr)
    {
        oName = current->objectName();
    }

    //MY_ASSERT(current != 0);

#ifdef WINDOZ
   ts << TARGET_NAME <<  "(" << QTime::currentTime().toString("hh:mm:ss.zzz") << ")";

#else
    //under linux, the current time is already displayed
    //in journal
    //uint secsNow = uptime->elapsed() / 1000;
    //uint msNow =  uptime->elapsed() % 1000;
    ts << TARGET_NAME;  //<<  "(" << secsNow << "." << msNow << ")";
#endif

    switch (type)
    {

         case QtDebugMsg:
            if(logLevel >= ECHOES_LOG_ALL)
            {
                ts << "Debug: ";
                syslogPriority = LOG_DEBUG;
            }
            else return;
            break;


        case QtInfoMsg:
            if(logLevel >= ECHOES_LOG_INFO)
            {
                ts << "Info: ";
                syslogPriority = LOG_INFO;
            }
            else return;
            break;

         case QtWarningMsg:
            if(logLevel >= ECHOES_LOG_WARNINGS)
            {
                ts << "Warning: " ;
                syslogPriority = LOG_WARNING;
            }
            else return;
            break;

         case QtCriticalMsg:
            if(logLevel >= ECHOES_LOG_CRITICAL)
            {
                ts << "Critical: ";
                syslogPriority = LOG_CRIT;
            }
            else return;
            break;

         case QtFatalMsg:
            if(logLevel >= ECHOES_LOG_FATAL)
            {
                ts << "Fatal: ";
                syslogPriority = LOG_ALERT;
            }
            else return;
            break;
    }

    if(verbose == true)
    {
        if(ctx.file != nullptr)
        {
            ts << " file:" << ctx.file << " line:" << ctx.line << " func:" << ctx.function ;
        }
    }
#ifndef NIX
    Q_UNUSED(syslogPriority)

    if(stringBuffer.isEmpty() == false &&
       logFile != nullptr &&
       logFile->isOpen() == true &&
       logFile->isWritable() == true)
    {
        ts << oName << ":" << current << " " << msg; // << MY_ENDL;
        if(verbose == true)
        {
            fprintf( stderr, qPrintable(stringBuffer) );
            fprintf( stderr, "\n" );
            fflush(stderr);
        }

        logFile->write( qPrintable(stringBuffer), stringBuffer.length() );
        logFile->write( "\n", 1 );
        logFile->flush();
    }
#else
    if(stringBuffer.isEmpty() == false)
    {
        ts << oName << ":" << current << " " << msg << MY_ENDL;

        if(verbose == true)
        {
            //output both to syslog and console
            fprintf( stderr, "%s", qPrintable(stringBuffer) );
            fflush( stderr );
        }

        syslog( syslogPriority, "%s", qPrintable(stringBuffer) );
    }
#endif

    if(ctrl != nullptr)
    {
        if(ctrl->isAcquisitionRunning())
        {
            if(type == QtCriticalMsg || type == QtFatalMsg)
            {
                qFatal("unrecoverable critical error, %s", msg.toLatin1().constData());
            }
        }
    }
}




static void soapyLogHandler(const SoapySDRLogLevel logLevel, const char *message)
{
    QtMsgType msgType;
    QMessageLogContext ctx;
    QString qMessage(message);
    ctx.file = nullptr;

    switch (logLevel)
    {
    case SOAPY_SDR_FATAL:
        msgType = QtFatalMsg;
        break;

    case SOAPY_SDR_CRITICAL:
    case SOAPY_SDR_ERROR:
        msgType = QtCriticalMsg;
        break;

    case SOAPY_SDR_WARNING:
        msgType = QtWarningMsg;
        break;


    case SOAPY_SDR_NOTICE:
    case SOAPY_SDR_INFO:
        msgType = QtInfoMsg;
        break;

    case SOAPY_SDR_DEBUG:
    case SOAPY_SDR_TRACE:
    case SOAPY_SDR_SSI:
        msgType = QtInfoMsg;
        break;
    }

    myMessageOutput(msgType, ctx, QString("[SOAPY] ")+qMessage);
}


/**
 *
 *
 */
[[ noreturn ]] void crashHandler( int what )
{
    MYINFO << "******** Signal " << what << " catched by crashHandler() *********";
    QThread *crashedThread = QThread::currentThread();
    if(crashedThread->objectName() == "Main Thread")
    {
        //only if main thread crashes, display a message window
        //and prepare a report.
        if(guiSpecApp != nullptr)
        {
            QMessageBox msgBox;
            QString data;
            data.setNum(what);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText("ABNORMAL PROGRAM TERMINATION");
            msgBox.setInformativeText(QObject::tr("received signal: ") + data + QObject::tr(". Press <Yes> to send a crash report, <No> to quit now."));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Yes)
            {
                QString content;
                QTextStream ts(&content);

                ts << TARGET_DESC << " v." << APP_VERSION << MY_ENDL;
                ts << "Build " << __TIMESTAMP__ << MY_ENDL;
    #ifdef _WIN32
                ts << "Operating System: " << QSysInfo::windowsVersion() << MY_ENDL;
    #endif

                ts << "Home dir: " <<  XQDir::homePath() << MY_ENDL;
                ts << "Application: " << qApp->applicationDirPath() << " " << qApp->applicationFilePath() << MY_ENDL;
                ts << "-----------------------------------------------------------------------------------------------" << MY_ENDL;
                ts << QObject::tr("Echoes just crashed. Sorry. ") << MY_ENDL;
    #ifndef NIX
                ts << QObject::tr("You can help the maintainer to solve this problem by sending him the log file ") << MY_ENDL;
                ts << logFileName << MY_ENDL;
    #else
                ts << QObject::tr("You can help the maintainer to solve this problem by sending him the last ") << MY_ENDL;
                ts << QObject::tr("10000 lines of your journal ( journalctl -n 10000 > buglog.txt ) ") << MY_ENDL;
    #endif
                ts << QObject::tr(" (if not too big!) and the configuration file you were using ") << MY_ENDL;
                ts << *confFileName << MY_ENDL;
                ts << QObject::tr("Please add a short description about what the program was doing  ");
                ts << QObject::tr("and the last operations you did when it crashed.") << MY_ENDL;
                ts << QObject::tr("Thanks.") << MY_ENDL << MY_ENDL;

                QUrl url("mailto:gmbertani@users.sourceforge.net?subject=Echoes crash report&body=" + content);
                QDesktopServices::openUrl(url);
            }
        }

    }
    else
    {
        MYWARNING << crashedThread->objectName() << " CRASHED !!!";
    }
    MYFATAL( "Abnormal program termination: %d", what);
    exit(what);
}

/**
 *
 *
 */
[[ noreturn ]] void shutDown( int what )
{
    MYINFO << "******** Signal " << what << " catched. *********";

    if(!ctrl->isConsoleMode())
    {
        QMessageBox::critical(nullptr, "Manual interrupt request",
            "The program will stop after opened files closing");
         ctrl->slotStopAcquisition(ASC_KILLED_BY_SIGNAL);

         //in GUI mode, the program should never be stopped with CTRL-C
    }
    else
    {
        //while in console mode there is no other way to stop it
        MYWARNING << "Manual interrupt request: The program will stop after opened files closing";

    }




    while(ctrl->isAcquisitionRunning())
    {
        QThread::msleep(100);
    }

    if ( qApp != nullptr )
    {
        qApp->quit();
        delete qApp;
    }

    MYINFO << "Terminating program.";
    exit( what );
}



/**
 * Command line handling
 */
int cmdLineScan( Settings* as, bool* console, bool* dumb, bool* verb, bool* vacuum, uint* level, QString* configFile, QString* langFile, QString* wdName  )
{
    *console = false;
    *dumb = false;
    *vacuum = false;

    QCommandLineParser parser;

    QString s;
    QTextStream ts(&s);
    ts << QObject::tr(TARGET_DESC) << " v." << APP_VERSION << "." << MY_ENDL;
    ts << "Architecture: " << QSysInfo::buildCpuArchitecture() << "  ABI: " << QSysInfo::buildAbi() << MY_ENDL;
    ts << QObject::tr("(C)Giuseppe Massimo Bertani 2018..2025.") << MY_ENDL << MY_ENDL;
    ts << QObject::tr("Usage :\t") << TARGET_NAME << QObject::tr(" [options]") << MY_ENDL;

    parser.setApplicationDescription(s);
    parser.addHelpOption();

    QCommandLineOption languageOption("l",
            QObject::tr( "loads the language qm file specified (defaults to local language, otherwise english)." ),
            QObject::tr( "language") );
    parser.addOption(languageOption);

    QCommandLineOption configOption("s",
            QObject::tr( "loads the settings from user config file given." ),
            QObject::tr( "config") );
    parser.addOption(configOption);

    QCommandLineOption wDirOption("w",
            QObject::tr( "sets this directory as working directory instead of $HOME/echoes" ),
            QObject::tr( "wdName") );
    parser.addOption(wDirOption);

    s.setNum(logLevel); //takes the default value from global variable initial setting
    QCommandLineOption logOption("n",
            QObject::tr( "log level: \n"
                         "0: do not create a program log\n"
                         "1: only fatal messages (crashes) are logged \n"
                         "2: logs fatal and critical messages (alerts about possible crashes) \n"
                         "3: logs warnings too (the program does not behave as expected) \n"
                         "4: logs status messages too (useful for console mode).\n"
                         "5: logs everything including debug messages (huge logs!).\n" ),
            QObject::tr("level"),
            s);
    parser.addOption(logOption);

    QCommandLineOption consoleOption("c", QObject::tr( "console mode: acquisition and capture starts automatically, no windows will be shown." ));
    parser.addOption(consoleOption);

    QCommandLineOption dumbOption("d", QObject::tr( "dumb console mode: acquisition starts automatically, no windows will be shown and nothing is captured. "
                                                    " The IQ buffers are forwarded to external clients only." ));
    parser.addOption(dumbOption);

    QCommandLineOption restoreOption("r", QObject::tr( "restores the hardcoded default settings (config_file if given will be ignored)." ));
    parser.addOption(restoreOption);

    QCommandLineOption verboseOption("v", QObject::tr( "verbose debug output on text console." ));
    parser.addOption(verboseOption);

    QCommandLineOption vacuumOption("x", QObject::tr( "database vacuum only, no windows will be shown neither acquisition will be started."
                                                       "The database depends of the config file chosen with -s option [REQUIRED]" ));
    parser.addOption(vacuumOption);

    // Process the actual command line arguments given by the user
    parser.process(*qApp);

    //FIRST must change the working directory if required
    *wdName = parser.value(wDirOption);

    //loads settings from configfile given
    QString value = parser.value(configOption);
    QFileInfo fi(value);
    *configFile = fi.baseName();

    if(guiSpecApp != nullptr)
    {
        *console = parser.isSet(consoleOption);
    }
    else
    {
        MYWARNING << "Echoes started in full console mode - the -c switch is implicitly set";
        *console = true;
    }

    *dumb = parser.isSet(dumbOption);
    *verb = parser.isSet(verboseOption);
    *vacuum = parser.isSet(vacuumOption);

    if(*vacuum)
    {
        if(configFile->length() == 0)
        {
            MYWARNING << "vacuum mode ignored because no config file has been specified with -s";
            *vacuum = false;
        }
    }

    //restores the default hardcoded settings
    bool restore = parser.isSet(restoreOption);
    if(restore == true)
    {
        QSettings ini( *configFile, QSettings::IniFormat );
        as->save(ini);
        MYINFO << QObject::tr("Default configuration file reset to hardcoded settings done ");
    }

    //language file
    value = parser.value(languageOption) + ".qm";
    QString lang = prgDir->absoluteFilePath(value);
    if (QFile::exists(lang) == true)
    {
        langFile->append( lang );
        MYINFO << QObject::tr("Language file ") << lang << QObject::tr(" found.");
    }
    else if(value.isEmpty() == false)
    {
        MYWARNING << QObject::tr("Language file") << lang << QObject::tr(" not found.");
    }



    //log level
    value = parser.value(logOption);
    if (value.toUInt() >= ECHOES_LOG_ALL)
    {
        *level = ECHOES_LOG_ALL;
    }
    else
    {
        *level = value.toUInt();
    }

    fprintf(stderr, "Log level: %u\r\n", *level);
    return 0;
}

bool DBbackup(QSqlDatabase& db, QString& rootDir, Settings* as)
{
    bool result = false;
    QDate today = as->currentUTCdate();
    QString dbSnapshot;
    if(as->getOverwriteSnap())
    {
        dbSnapshot = QString("snapshot_%1.sqlite3").arg(as->getConfigName());
    }
    else
    {
        dbSnapshot = QString("snapshot_%1_%2.sqlite3").arg(as->getConfigName()).arg(today.toString("yyyy-MM-dd"));
    }
    dbSnapshot = QString("%1/%2").arg(rootDir, dbSnapshot );
    QFile::remove(dbSnapshot); //delete the file in case already exists

    QSqlQuery r(db);
    MYINFO << __func__ << "vacuuming data into a database snapshot " << dbSnapshot;
    result =  r.exec( QString("VACUUM INTO '%1'").arg(dbSnapshot) );
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
        return false;
    }

    MYINFO << __func__ << "vacuuming the current database too";
    result =  r.exec("VACUUM");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
        return false;
    }
    MYINFO << __func__ << "vacuuming done";

    return true;
}


int main(int argc, char *argv[])
{    
    bool        console = false;
    bool        dumb = false;
    bool        vacuum = false;
    bool        autoStart = false;
    int         retVal = 0;
    Settings*   as = nullptr;
    QString*    langFile;
    QString*    wDirName;
    XQDir*       wd;
    MainWindow* w = nullptr;

    QString appInvocation(argv[0]);
    if( appInvocation.contains( FULL_CONSOLE_APPNAME ))
    {
        MYINFO << "+++Console application+++";
        specApp = new QCoreApplication( argc, argv );
        MY_ASSERT( nullptr != specApp)
        console = true;
    }
    else
    {
        MYINFO << "+++GUI application+++";
        guiSpecApp = new QApplication( argc, argv );
        MY_ASSERT( nullptr != guiSpecApp)
        specApp = guiSpecApp;
    }

    specApp->setApplicationName(TARGET_NAME);
    specApp->setApplicationVersion( APP_VERSION );
    specApp->thread()->setObjectName("Main Thread");

    mx = new QMutex;
    MY_ASSERT( nullptr != mx)

    uptime = new QElapsedTimer;
    MY_ASSERT( nullptr != uptime)
    uptime->start();

    qInstallMessageHandler(myMessageOutput);

    //create the default working directory under homedir
    wDirName = new QString;
    MY_ASSERT( nullptr != wDirName)

    confFileName = new QString;
    MY_ASSERT( nullptr != confFileName)

    langFile = new QString;
    MY_ASSERT( nullptr != langFile)

    prgDir = new XQDir;
    MY_ASSERT( nullptr != prgDir)

    wd = new XQDir;
    MY_ASSERT( nullptr != wd)

    *wDirName = XQDir::homePath();
    wd->setCurrent( *wDirName );
    if( wd->cd(WORKING_PATH) == false )
    {
        wd->mkdir(WORKING_PATH);
        if( wd->cd(WORKING_PATH) == true )
        {
            //wd->setCurrent( WORKING_PATH );
            MYWARNING << "created working directory " << WORKING_PATH;
        }
    }

    wDirName->clear();
    wd->makeAbsolute();


#ifdef NIX
    //looks under auxiliary files path for translations
    prgDir->setPath( AUX_PATH );
#else
    //looks under the program directory for translations
    prgDir->setPath( specApp->applicationDirPath() );
#endif

    bool _console = false;

    if ( cmdLineScan( as, &_console, &dumb, &verbose, &vacuum, &logLevel, confFileName, langFile, wDirName ) != 0 )
    {
        MYCRITICAL << "Error: wrong parameters";
        return -1;
    }

    //when starting as console_echoes, vacuum or dumb mode, the -c switch is implicitly set
    if(_console == true || dumb == true || vacuum == true)
    {
        console = true;
    }

    if(wDirName->isEmpty() == false)
    {
        if( wd->exists(*wDirName) == false)
        {
            QString msg = QString("%1%2%3").
                    arg(QObject::tr("The directory ")).
                    arg(*wDirName).
                    arg(QObject::tr(" specified with -w does not exist"));

            QMessageBox::critical(nullptr, QObject::tr("Fatal error"), msg);
            return -1;
        }

        //different working directory specified
        if( wd->cd(*wDirName) == false )
        {
            qFatal("The directory %s specified with -w does not exist", qPrintable(*wDirName));
        }
        MYINFO << "Working directory changed to " << *wDirName;
    }

    SoapySDR_registerLogHandler(soapyLogHandler);
    SoapySDR_setLogLevel(SOAPY_SDR_DEBUG);

    qint64 pid = specApp->applicationPid();

#ifndef NIX
    QString logFileName = QString("%1.log").arg(LOGFILE);
    if(vacuum)
    {
        logFileName = QString("%1-child.log").arg(LOGFILE);
    }

    logFileName = wd->absoluteFilePath(logFileName);
    QString logBakName = logFileName + ".bak";

    MYINFO << "Renaming existing " << logFileName << " to " << logBakName;
    QFile::copy(logFileName, logBakName);

    logFile = new QFile(logFileName);
    MY_ASSERT( nullptr != logFile)
    if( logFile->open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text) == true )
    {
        logFile->write( "******************** Starting Echoes  ********************\n" );
        logFile->flush();
    }
#else
    MYINFO << "******************** Starting Echoes  ********************";
#endif


    //portable configuration file
    as = new Settings();
    MY_ASSERT( nullptr != as)

    if(console == false)
    {
        MYINFO << "GUI application: loading style sheet";
        XQDir ap( specApp->applicationDirPath()  );
        QString styleSheetName = ap.absoluteFilePath(GLOBAL_STYLESHEET_FILE);
        QString ss;
        QFile fss(styleSheetName);
        if( fss.open(QIODevice::ReadOnly) == true)
        {
            QTextStream in(&fss);
            ss = in.readAll();
            guiSpecApp->setStyleSheet( ss );
        }
        else
        {
            qFatal("qss file %s not found.", styleSheetName.toLatin1().constData());
        }

        //enables fonts antialiasing everywhere
        QFont f=QApplication::font();
        f.setStyleStrategy(QFont::PreferAntialias);
        QApplication::setFont(f);

    }


    QLocale loc;

    MYINFO << "************************************************************************";
    MYINFO << TARGET_DESC;
    MYINFO << " v." << APP_VERSION ;
    MYINFO << "(C) Giuseppe Massimo Bertani 2016..2025";
    MYINFO << "Build " << __TIMESTAMP__ ;
    MYINFO << "Locale country: " << loc.nativeCountryName() << " language: " << loc.nativeLanguageName();
    MYINFO << "QThread::idealThreadCount()=" << QThread::idealThreadCount();
    MYINFO << "PID: " << pid;
    MYINFO << "************************************************************************";

    *confFileName = wd->absoluteFilePath( QString(*confFileName)+QString(CONFIG_EXT) );
    if (QFile::exists(*confFileName) == true)
    {
        QSettings ini( *confFileName, QSettings::IniFormat );
        as->load(ini);
        MYINFO << QObject::tr("Configuration file") << *confFileName << QObject::tr(" loaded.");

        //when a configuration file is given as -s argument, the acquisition starts automatically
        autoStart = true;
    }
    else if(confFileName->isEmpty() == false)
    {
        MYINFO << QObject::tr("Configuration file") << *confFileName << QObject::tr(" not found, looking for default one");


        //invocation without parameters or no configuration file specified
        //loads the default configuration file (latest settings)

        *confFileName = wd->absoluteFilePath( QString(DEFAULT_CONFIG_NAME CONFIG_EXT) );
        QFile config(*confFileName);
        if (config.exists() == false)
        {
            QSettings ini( *confFileName, QSettings::IniFormat );
            as->save(ini);
            MYINFO << QObject::tr("Default file not found, creating it as ") << *confFileName;
        }
        else
        {
            QSettings ini( *confFileName, QSettings::IniFormat );
            as->load(ini);
            MYINFO << QObject::tr("Default configuration file") << *confFileName << QObject::tr(" loaded.");
        }

        //no autostart in this case
    }

    //signal handlers
    signal( SIGTERM, shutDown );
    signal( SIGINT,  shutDown );

#ifndef _DEBUG
    signal( SIGFPE,  crashHandler );
    signal( SIGSEGV, crashHandler );
    signal( SIGILL,  crashHandler );
#endif

#ifdef NIX
    //(linux etc.)
    signal( SIGQUIT, shutDown );
    signal( SIGKILL, shutDown );
#endif

    MYINFO << "console=" << console << " dumb=" << dumb;
    as->setConsoleMode(console);
    as->setDumbMode(dumb);

    if(vacuum)
    {

        QString rootDir = wd->absoluteFilePath( as->getConfigName() );
        QString dbPath = QString("%1/%2.sqlite3").arg(rootDir, as->getConfigName() );
        QString connName("ECHOES_VACUUM");

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);

        db.setDatabaseName( dbPath );
        bool ok = db.open();
        if(!ok)
        {
            MYFATAL("Unable to open database %s", dbPath.toLatin1().constData());
        }

        MYINFO << "DB vacuum connection = " << db;
        DBbackup(db, rootDir, as);
    }
    else
    {
        as->setResetCounter();

        if(  QAudioDeviceInfo::availableDevices(QAudio::AudioOutput).isEmpty() == true )
        {
            MYWARNING << "Sound system not available" ;
        }

        MYDEBUG << "Looking for language files";

        QStringList dirList = prgDir->entryList();

        QString entry;
        QTranslator *t = nullptr;
        QMap<QString,QTranslator *> tMap;

        if(*langFile == "")
        {
            //if no language file specified in command line
            //loads the local language, if file exists
            *langFile = loc.languageToString( loc.language() );
            *langFile += ".qm";

        }

        for (int i = 0; i < dirList.size(); ++i)
        {
            entry = dirList.at(i);
            if( prgDir->match( "*.qm", entry ) == true )
            {
                t = new QTranslator(specApp);
                MY_ASSERT( nullptr != t)
                t->load( entry, prgDir->absolutePath() );
                tMap[entry] = t;
                MYDEBUG << entry;
                if(langFile == entry)
                {
                    specApp->installTranslator( t );
                    MYINFO << entry << " : installed.";
                }
            }
        }
        MYDEBUG << "Found total " << tMap.count() << " language files in " << prgDir->absolutePath() ;


        //the GUI run in main thread
        QThread *mainThread = QThread::currentThread();
        mainThread->setObjectName("Main thread");

        ctrl = new Control(as, *wd);
        MY_ASSERT( nullptr != ctrl )

        //the control thread manages the acquisition and capture tasks
        ctrlThread = new QThread(mainThread);
        MY_ASSERT( nullptr != ctrlThread )

        ctrlThread->setObjectName("Control thread");
        ctrl->moveToThread(ctrlThread);

        //starts the control thread so it will be able
        //to communicate with main window through signals/slots
        ctrlThread->start();

        if(console == false)
        {
            w = new MainWindow(as, ctrl, nullptr);
            MY_ASSERT( nullptr != w)
            w->show();
            as->setMainWindow(w);
            if(autoStart == true)
            {
                //self-pressing the start button
                w->slotStart();
            }
        }
        else
        {
            if(autoStart == true)
            {
                //in console mode there is no main window
                //so the control thread must start while starting acquisition
                ctrl->slotAcquisition(true);
            }
            else
            {
                MYCRITICAL << "Error: in console mode, a configuration file MUST be specified with -s option." ;
            }
        }

        try
        {
            retVal = specApp->exec();
        }
        catch (const std::exception& e)
        {
            MYCRITICAL << " exception " << e.what() << " catched";
            crashHandler( -1 );
        }

        as->blockSignals(true);
        if(ctrl->isAcquisitionRunning())
        {
            ctrl->slotAcquisition(false);
        }

        ctrlThread->quit();

        if(as->changes() != 0)
        {
            PostProc* pp = ctrl->postProcessor();
            MY_ASSERT( nullptr != pp)

            if( as->getConfigName() == DEFAULT_CONFIG_NAME )
            {
                //The default configuration file is updated
                QString cfg = QString("%1%2").arg( as->getConfigName() ).arg(CONFIG_EXT);
                MYINFO << "Saving defaults...";
                QSettings ini( wd->absoluteFilePath(cfg), QSettings::IniFormat );
                as->save(ini);

            }
            else if( QMessageBox::question(w, QObject::tr("Configuration changed"),
                QObject::tr("Press YES to update the configuration on disk \n or NO to keep it unchanged") ) == QMessageBox::Yes)
            {
                //The configuration file opened is updated
                QString cfg = QString("%1%2").arg( as->getConfigName() ).arg(CONFIG_EXT);
                MYINFO << "Saving " << cfg ;
                QSettings ini( wd->absoluteFilePath(cfg), QSettings::IniFormat );
                as->save(ini);
            }
        }
        QThread::sleep(1);
        delete ctrl;
        ctrl = nullptr;
        delete w;
        w = nullptr;

        MYINFO << "Main window destroyed, destroying control thread";

#ifndef NIX
        logFile->close();
        delete logFile;
        logFile = nullptr;

#endif

        as->setResetCounter(true);
    }
    delete as;
    as = nullptr;

    delete specApp;
    specApp = nullptr;

    MYINFO << "Regular program termination";

    return retVal;

}


