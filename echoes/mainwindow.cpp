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
$Rev:: 382                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-12-20 08:43:50 +01#$: Date of last commit
$Id: mainwindow.cpp 382 2018-12-20 07:43:50Z gmbertani $
*******************************************************************************/
#include <QFileDialog>
#include "ui_mainwindow.h"
#include "waterfall.h"
#include "radio.h"
#include "control.h"
#include "postproc.h"
#include "mainwindow.h"

int MainWindow::busyInst = 0;           ///instance counter for showBusy() calls

MainWindow::MainWindow(Settings* appSettings, Control* appControl, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    setObjectName("MainWindow widget");
    ui->setupUi(this);

    init = true;
    outputEnabled = false;
    deviceChanged = false;
    shutdownDetected = false;
    minTuneHz = 0;
    maxTuneHz = 0;
    SRRchanged = false;
    SRchanged = false;
    BBRchanged = false;
    BBchanged = false;
    dsBw = 0;


    MY_ASSERT( nullptr !=  appControl)
    ac = appControl;

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    tune = as->getTune();

    wf = new Waterfall(as, ac, this);
    MY_ASSERT( nullptr !=  wf)
    wf->hide();

    //make wf pointer available for Control
    as->setWaterfallWindow( wf );

    r = ac->getRadio();
    MY_ASSERT( nullptr !=  r)

    sysCheck = new QTimer;
    MY_ASSERT( nullptr !=  sysCheck)

    PostProc* pp = ac->postProcessor();
    MY_ASSERT( nullptr !=  pp )

    busyCur = new QCursor(Qt::WaitCursor);
    MY_ASSERT( nullptr != busyCur )

    freeCur = new QCursor(Qt::ArrowCursor);
    MY_ASSERT( nullptr != freeCur )

    //GUI widgets arrangement by tabs:
    guiWidgetsList.resize(GT_TOTAL);

    guiWidgetsList[GT_GLOBAL]={ui->pbOpen, ui->pbSaveAs, /* ui->pbAbout, */ ui->pbQuit, ui->pbStart /*, ui->pbHelp */};

    guiWidgetsList[GT_DEVICE]={ ui->cbDevice, ui->sbSR,  ui->cbSRrange, ui->sbBB, ui->cbBBrange,
                                ui->pbm1kT, ui->pbm10kT,  ui->pbm100kT,  ui->pbm1MT,  ui->pbm10MT,
                                ui->pbp1kT, ui->pbp10kT,  ui->pbp100kT,  ui->pbp1MT,  ui->pbp10MT,                           
                                ui->sbPpm, ui->sbGain, ui->chkAGC, ui->cbAntenna, ui->cbFreqRange, ui->lcdTune };

    //FFT tab
    guiWidgetsList[GT_FFT]={ui->cbFtw,  ui->pbIncRes,  ui->pbDecRes, ui->chkBypass,
                                ui->cbDownsampler };

    //Output
    guiWidgetsList[GT_OUTPUT]={  ui->gbAcqMode, ui->sbInterval,  ui->sbAfter,  ui->sbMaxShots,  ui->sbRecTime,
                                ui->cbThresholds, ui->sbUpThreshold, ui->sbDnThreshold, ui->sbAvg, ui->sbTrange,
                                ui->sbJoinTime, ui->cbPalette, ui->sbNoiseLimit, ui->chkDumps, ui->chkSShots };

    //Storage
    guiWidgetsList[GT_STORAGE]={ ui->chkEraseOldLogs, ui->chkEraseOldShots, ui->sbDataLasting,  ui->sbImgLasting,
                                ui->sbMinFree, ui->sbDBsnap, ui->chkOverwriteSnap };

    //Preferences
    guiWidgetsList[GT_PREFERENCES]={ ui->chkFticks,  ui->chkPticks, ui->chkTticks, ui->chkBoundaries, ui->chkTooltips,
                         ui->chkEnablePing, ui->chkEnableFaultSound, ui->chkEnableDAT, ui->sbDbOff, ui->sbDbGain, ui->leServerIP,
                         ui->sbServerPort, ui->sbUDPdelay, ui->gbScanInteg, ui->sbMaxEvLast, };



    //plugging signals:

    //pushbuttons
    connect( ui->pbAbout,       SIGNAL( pressed() ), this,      SLOT( slotAboutDialog() ) );
    connect( ui->pbOpen,        SIGNAL( pressed() ), this,      SLOT( slotOpenConfig() ) );
    connect( ui->pbSave,        SIGNAL( pressed() ), this,      SLOT( slotSave() ) );
    connect( ui->pbSaveAs,      SIGNAL( pressed() ), this,      SLOT( slotSaveAs() ) );
    connect( ui->pbQuit,        SIGNAL( pressed() ), this,      SLOT( slotQuit() ) );
    connect( ui->pbStart,       SIGNAL( pressed() ), this,      SLOT( slotStart() ) );
    connect( ui->pbStop,        SIGNAL( pressed() ), this,      SLOT( slotStop() ) );
    connect( ui->pbHelp, &QRadioButton::pressed, [=] (){ QDesktopServices::openUrl( QUrl(ONLINE_MANUAL_URL) ); } );


    //device
    //variable elements comboboxes: strings must be converted to indexes before forwarding events to as
    connect( ui->cbDevice,      SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetDevice     (const QString &) ) );

    connect( ui->cbAntenna,     SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetAntenna    (const QString &) ) );
    connect( ui->cbSRrange,     SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetSampleRateRange(const QString &) ) );
    connect( ui->cbBBrange,     SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetBandwidthRange (const QString &) ) );
    connect( ui->cbFreqRange,   SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetFreqRange  (const QString &) ) );
    connect( ui->sbSR,          SIGNAL( valueChanged(int) ),                    this, SLOT( slotSetSampleRate (int) ) );
    connect( ui->sbBB,          SIGNAL( valueChanged(int) ),                    this, SLOT( slotSetBandwidth(int) ) );

    connect( ui->sbGain,        SIGNAL( valueChanged(int)  ),                           this,       SLOT( slotSetGain(int) ) );
    connect( ui->chkAGC,        SIGNAL( stateChanged(int) ),                            this,       SLOT( slotSetAutoGain(int) ) );
    connect( ui->sbPpm,         SIGNAL( valueChanged(double) ),                         as,         SLOT( setError(double) ) );
    connect( ui->lcdTune,       SIGNAL( customContextMenuRequested(const QPoint&) ),    this,       SLOT( slotEnterTune(const QPoint&) ) );
    connect( ui->pbp10MT,       SIGNAL( pressed() ),                                    this,       SLOT( slotTuneAdd10M() ) );
    connect( ui->pbp1MT,        SIGNAL( pressed() ),                                    this,       SLOT( slotTuneAdd1M() ) );
    connect( ui->pbp100kT,      SIGNAL( pressed() ),                                    this,       SLOT( slotTuneAdd100k() ) );
    connect( ui->pbp10kT,       SIGNAL( pressed() ),                                    this,       SLOT( slotTuneAdd10k() ) );
    connect( ui->pbp1kT,        SIGNAL( pressed() ),                                    this,       SLOT( slotTuneAdd1k() ) );
    connect( ui->pbm10MT,       SIGNAL( pressed() ),                                    this,       SLOT( slotTuneSub10M() ) );
    connect( ui->pbm1MT,        SIGNAL( pressed() ),                                    this,       SLOT( slotTuneSub1M() ) );
    connect( ui->pbm100kT,      SIGNAL( pressed() ),                                    this,       SLOT( slotTuneSub100k() ) );
    connect( ui->pbm10kT,       SIGNAL( pressed() ),                                    this,       SLOT( slotTuneSub10k() ) );
    connect( ui->pbm1kT,        SIGNAL( pressed() ),                                    this,       SLOT( slotTuneSub1k() ) );


    //FFT settings
    connect( ui->chkBypass,     SIGNAL( stateChanged(int) ),                    this, SLOT( slotSetSsBypass(int) ) );
    connect( ui->cbDownsampler, SIGNAL( currentIndexChanged(const QString &) ), this, SLOT( slotSetDsBandwidth(const QString &)) );

    connect( ui->cbFtw,         SIGNAL( currentIndexChanged(int) ), as, SLOT( setWindow(int) ) );
    connect( ui->pbIncRes,      SIGNAL( pressed() ), this,        SLOT( slotIncResolution() ) );
    connect( ui->pbDecRes,      SIGNAL( pressed() ), this,        SLOT( slotDecResolution() ) );

    //output settings
    connect( ui->rbContinuous,  &QRadioButton::pressed, [=] () { this->slotSetAcqMode(AM_CONTINUOUS); });
    connect( ui->rbPeriodic,    &QRadioButton::pressed, [=] () { this->slotSetAcqMode(AM_PERIODIC); });
    connect( ui->rbAutomatic,   &QRadioButton::pressed, [=] () { this->slotSetAcqMode(AM_AUTOMATIC); });

    connect( ui->sbInterval,        SIGNAL( valueChanged(int) ),        this, SLOT( slotStop() ) );
    connect( ui->sbInterval,        SIGNAL( valueChanged(int) ),        as, SLOT( setInterval(int) ) );
    connect( ui->sbMaxShots,        SIGNAL( valueChanged(int) ),        as, SLOT( setShots     (int) ) );
    connect( ui->sbRecTime,         SIGNAL( valueChanged(int) ),        as, SLOT( setRecTime   (int) ) );
    connect( ui->sbAfter,           SIGNAL( valueChanged(int) ),        as, SLOT( setAfter     (int) ) );
    connect( ui->cbThresholds,      SIGNAL( currentIndexChanged(int) ), as, SLOT( setThresholdsBehavior(int)));
    connect( ui->sbUpThreshold,     SIGNAL( valueChanged(double) ),     as, SLOT( setUpThreshold (double) ) );
    connect( ui->sbDnThreshold,     SIGNAL( valueChanged(double) ),     as, SLOT( setDnThreshold (double) ) );
    connect( ui->sbTrange,          SIGNAL( valueChanged(int) ),        as, SLOT( setDetectionRange (int) ) );
    connect( ui->chkDumps,          SIGNAL( stateChanged(int) ),        as, SLOT( setDumps(int)) );
    connect( ui->chkSShots,         SIGNAL( stateChanged(int) ),        as, SLOT( setScreenshots(int)) );
    connect( ui->sbAvg,             SIGNAL( valueChanged(int) ),        as, SLOT( setAvgdScans(int) ) );
    connect( ui->sbJoinTime,        SIGNAL( valueChanged(int) ),        as, SLOT( setJoinTime(int) ) );
    connect( ui->cbPalette,         SIGNAL( currentIndexChanged(int) ), as, SLOT( setPalette        (int) ) );
    connect( ui->sbNoiseLimit,      SIGNAL( valueChanged(int) ),        as, SLOT( setNoiseLimit     (int)));

    //storage settings
    connect( ui->chkEraseOldLogs,   SIGNAL( stateChanged(int) ),        as, SLOT( setEraseLogs    (int) ) );
    connect( ui->chkEraseOldShots,  SIGNAL( stateChanged(int) ),        as, SLOT( setEraseShots   (int) ) );
    connect( ui->chkOverwriteSnap,  SIGNAL( stateChanged(int) ),        as, SLOT( setOverwriteSnap(int) ) );

    connect( ui->sbDBsnap,          SIGNAL( valueChanged(int) ),        as, SLOT( setDBsnap     (int) ) );
    connect( ui->sbDataLasting,     SIGNAL( valueChanged(int) ),        as, SLOT( setDataLasting(int) ) );
    connect( ui->sbImgLasting,      SIGNAL( valueChanged(int) ),        as, SLOT( setImgLasting (int) ) );
    connect( ui->sbMinFree,         SIGNAL( valueChanged(int) ),        as, SLOT( setMinFree    (int)) );


    //preferences
    connect( ui->sbMaxEvLast,       SIGNAL( valueChanged(int) ),        as, SLOT( setMaxEventLasting(int) ) );
    connect( ui->chkFticks,         SIGNAL( stateChanged(int) ),        as, SLOT( setHz             (int) ) );
    connect( ui->chkPticks,         SIGNAL( stateChanged(int) ),        as, SLOT( setDbfs           (int) ) );
    connect( ui->chkTticks,         SIGNAL( stateChanged(int) ),        as, SLOT( setSec            (int) ) );
    connect( ui->chkBoundaries,     SIGNAL( stateChanged(int) ),        as, SLOT( setRangeBoundaries(int) ) );
    connect( ui->chkTooltips,       SIGNAL( stateChanged(int) ),        as, SLOT( setTooltips       (int) ) );
    connect( ui->chkEnablePing,     SIGNAL( stateChanged(int) ),        as, SLOT( setPing           (int) ) );
    connect( ui->chkEnableFaultSound,SIGNAL( stateChanged(int) ),       as, SLOT( setFaultSound     (int) ) );
    connect( ui->sbDbOff,           SIGNAL( valueChanged(double) ),     as, SLOT( setDbfsOffset     (double)));
    connect( ui->sbDbGain,          SIGNAL( valueChanged(double) ),     as, SLOT( setDbfsGain       (double)));
    connect( ui->sbServerPort,      SIGNAL( valueChanged(int) ),        as, SLOT( setServerPort     (int)));
    connect( ui->sbUDPdelay ,       SIGNAL( valueChanged(int) ),        as, SLOT( setDatagramDelay  (int)));
    connect( ui->chkEnableDAT,      SIGNAL( stateChanged(int) ),        as, SLOT( setDATdumps       (int) ) );

    connect( ui->rbLast,    &QRadioButton::pressed, [=] () { this->slotSetScanMode(SCAN_LAST); });
    connect( ui->rbAverage, &QRadioButton::pressed, [=] () { this->slotSetScanMode(SCAN_AVG); });
    connect( ui->rbMax,     &QRadioButton::pressed, [=] () { this->slotSetScanMode(SCAN_MAX); });


    connect( qApp,  SIGNAL( focusChanged(QWidget*, QWidget*) ), this, SLOT( slotFocusChanged(QWidget*, QWidget*)) );

    connect( r, SIGNAL( status(const QString&, int) ),  this, SLOT( slotShowStatus(const QString&, int) ),  Qt::QueuedConnection );
    connect( r, SIGNAL( GUIupdateNeeded() ),            this, SLOT( slotGUIinit() ),                        Qt::QueuedConnection );


    //feedbacks
    connect( as,  SIGNAL( notifySetResolution() ),          this, SLOT( slotAdjustResolution() ) );
    connect( as,  SIGNAL( notifyOpened(const QString*) ),   this, SLOT( slotReopen(const QString*) ) );
    connect( as,  SIGNAL( notifySetUpThreshold()),          this, SLOT( slotValidateUpThreshold() ) );
    connect( as,  SIGNAL( notifySetDnThreshold()),          this, SLOT( slotValidateDnThreshold() ) );
    connect( as,  SIGNAL( notifySetThresholdsBehavior()),   this, SLOT( slotSetSbThresholdsParam() ) );

    connect( sysCheck,  SIGNAL(timeout()),      this,   SLOT( slotUpdateSysInfo()) );

    connect( ac, SIGNAL( GUIfree() ),               this, SLOT(showFree()),                         Qt::QueuedConnection);
    connect( ac, SIGNAL( GUIbusy() ),               this, SLOT(showBusy()),                         Qt::QueuedConnection);
    connect( ac, SIGNAL( status(const QString&) ),  this, SLOT( slotShowStatus(const QString&) ),   Qt::QueuedConnection );
    connect( ac, SIGNAL( notifyStop() ),            this, SLOT( slotAcqStopped() ),                 Qt::QueuedConnection );
    connect( ac, SIGNAL( notifyStart() ),           this, SLOT( slotAcqStarted() ),                 Qt::QueuedConnection );

    connect( this, SIGNAL( acquisitionStatus(bool) ),   ac, SLOT(slotAcquisition(bool)),            Qt::QueuedConnection );
    connect( this, SIGNAL( acquisitionAbort(int) ),     ac, SLOT(slotStopAcquisition(int)),         Qt::QueuedConnection );
    connect( this, SIGNAL( notifyReady() ),             ac, SLOT( slotInitParams() ),               Qt::QueuedConnection );


    ast = AST_INIT;

    qApp->installEventFilter(this);

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect  screenGeometry = screen->geometry();
    int screenHeight = screenGeometry.height();

    if( height() > screenHeight )
    {
        //for small screens: shrinks vertically the main window
        //to avoid the start button go out the screen

        resize( width(), screenHeight );
    }

    slotReopen(nullptr);

    MYDEBUG << "done." << MY_ENDL;
}

MainWindow::~MainWindow()
{
    delete sysCheck;
    delete wf;
    delete ui;
    delete freeCur;
    delete busyCur;
}

//to disable tooltips globally
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(as->getTooltips() != 0)
    {
        if (event->type() == QEvent::ToolTip)
        {
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}


// Override nativeEvent to intercept Windows-specific messages
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG")
    {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message)
        {

        case WM_QUERYENDSESSION:
            // This message is sent by Windows when the system is about to shut down
            MYINFO << "System shutdown initiated (WM_QUERYENDSESSION)";
            shutdownDetected = true;   // Set flag for later use in closeEvent
            *result = TRUE;            // Indicate that the application agrees to close
            return true;               // Message was handled

        case WM_ENDSESSION:
            // This message confirms the end of the session
            MYINFO << "Session is ending (WM_ENDSESSION), lParam=" << msg->lParam;
            return false;              // Let Windows handle this message normally
        }
    }
#endif
    // Pass unhandled messages to the base class
    return QMainWindow::nativeEvent(eventType, message, result);
}

// Override closeEvent to distinguish between user-initiated and system-initiated exits
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (shutdownDetected)
    {
        MYINFO << "Closing due to system shutdown (e.g., triggered by UPS)";
        ac->slotStopAcquisition(ASC_KILLED_BY_SIGNAL);
        // Insert cleanup code here: save files, stop threads, etc.
    }
    else
    {
        MYINFO << "Normal user-initiated closure";
        ac->slotStopAcquisition(ASC_MANUAL_REQ);
    }

    // Call base class implementation to proceed with closing
    QMainWindow::closeEvent(event);

}




void MainWindow::slotGUIinit()
{
    int i = -1;
    qint64 val = 0, max = -1, min = -1;
    float d = 0.0;
    bool ok = false;
    QString s = "";

    MYINFO << "*** initializing GUI ***" << MY_ENDL;

    blockAllSignals(true);

    outputEnabled = false;

    ui->tabWidget->setCurrentIndex(0);

    //device settings


    //Then inizializes the value of every control.
    //If no rts has been loaded, the default values could not
    //be suited for the selected SDR, so they must be
    //automatically corrected with alloweed values.

    //selected antenna
    QStringList sl = r->deviceOpened()->getAntennas();
    MYINFO << "loading antennas: " << sl;
    if(sl.at(0) != "N.A.")
    {
        //this device provides specific antennas, so
        //the antenna loaded from rts must be checked
        //for compatibility
        ui->cbAntenna->clear();
        ui->cbAntenna->addItems( sl );
        MY_ASSERT( ui->cbAntenna->count() == sl.count() )
        s = as->getAntenna();
        i = ui->cbAntenna->findText( s, Qt::MatchFixedString );
        if(i == -1)
        {
            MYDEBUG << "string " << s << " not found in ui->cbAntenna, taking value at index zero" << MY_ENDL;
            ui->cbAntenna->setCurrentIndex( 0 );
            s = ui->cbAntenna->currentText();
            as->setAntenna(s);
        }
        else
        {
            ui->cbAntenna->setCurrentIndex( i );
        }
    }
    else
    {
        //this device doesn't provide specific antennas, so it
        //trusts the antenna loaded from rts
        ui->cbAntenna->clear();
        s = as->getAntenna();
        ui->cbAntenna->addItem( s );
        ui->cbAntenna->setCurrentIndex( 0 );
    }

    //gets the required sample rate
    int sr = as->getSampleRate();
    val = sr;
    //sample rate ranges allowed by device
    sl = r->deviceOpened()->getSRranges();

    MYINFO << "loading sample rate ranges: " << sl;
    if(sl.at(0) != "N.A.")
    {
        //this device provides specific sample rate ranges, so
        //the SRR loaded from rts must be checked
        //for compatibility
        ui->cbSRrange->clear();
        localizeRanges(sl);
        ui->cbSRrange->addItems( sl );
        MY_ASSERT( ui->cbSRrange->count() == sl.count() )

        //the required sample rate must match one of the
        //loaded ranges.
        s = loc.toString( val );
        i = as->isIntValueInList(sl, s);
        if(i == -1)
        {
            MYINFO << "Sample rate " << s << " doesn't match any range.";

            i = sl.count()-1;
            ui->cbSRrange->setCurrentIndex( i );
            s = sl.at(i);
            delocalizeRange(s);
            as->setSampleRateRange(s);
            sl = s.split(":");
            min = sl.at(0).toInt(&ok);
            MY_ASSERT(ok == true)
            max = sl.at(1).toInt(&ok);
            MY_ASSERT(ok == true)
            val = max;
            MY_ASSERT(ok == true)
            MYINFO << "Taking the highest value of last range: " << val;
        }
        else
        {
            MYINFO << "Sample rate " << s << " is in range " << sl.at(i);
            ui->cbSRrange->setCurrentIndex( i );
            s = sl.at(i);
            delocalizeRange(s);
            as->setSampleRateRange(s);
            sl = s.split(":");
            min = sl.at(0).toInt(&ok);
            MY_ASSERT(ok == true)
            if(sl.count() > 1)
            {
                max = sl.at(1).toInt(&ok);
            }
            else
            {
                max = min;
            }
            MY_ASSERT(ok == true)
        }
        as->setSampleRate(val, false);
        ui->sbSR->setMinimum(min);
        ui->sbSR->setMaximum(max);
        ui->sbSR->setValue(val);
    }
    else
    {
        //this device doesn't provide specific SRR, so it
        //trusts the SRR loaded from rts
        ui->cbSRrange->clear();
        s = as->getSampleRateRange();
        ui->cbSRrange->addItem( s );
        ui->cbSRrange->setCurrentIndex( 0 );

        ui->sbSR->setMinimum(sr);
        ui->sbSR->setMaximum(sr);
        ui->sbSR->setValue(sr);
    }

    int dsBwOrig = as->getDsBandwidth();
    MYINFO << "loaded dsBw=" << dsBwOrig;

    //bandwidth required
    int bb = as->getBandwidth();
    MYINFO << "bandwidth required:" << bb;

    //bandwidth ranges allowed by device
    sl = r->deviceOpened()->getBBranges();
    MYINFO << "loading bandwidth ranges: " << sl;
    if(sl.at(0) != "N.A.")
    {
        //this device provides specific bandwidth ranges, so
        //the BBR loaded from rts must be checked
        //for compatibility

        ui->cbBBrange->clear();
        localizeRanges(sl);
        ui->cbBBrange->addItems( sl );
        MY_ASSERT( ui->cbBBrange->count() == sl.count() )
        if(sl.count() <= 1 && sl.at(0) == "None")
        {
            //the device has not band filters
            //so the bandwidth is set equal to sample rate
            MYINFO << "This device has no band filters, setting to SR";
            i = 0;
            ui->cbBBrange->setCurrentIndex( i );
            s = ui->cbBBrange->currentText();
            delocalizeRange(s);
            as->setBandwidthRange(s);

        }
        else
        {
            val = bb;
            s = loc.toString(val);
            i = as->isIntValueInList(sl, s);
            if(i == -1)
            {
                MYINFO << "Bandwidth " << s << " doesn't match any range of " << sl << " taking the closest range";
                i = as->getClosestIntValueFromList(sl, s);
                ui->cbBBrange->setCurrentIndex( i );
                s = ui->cbBBrange->currentText();
                delocalizeRange(s);
                as->setBandwidthRange(s);
                QStringList sl = s.split(':');
                if(sl.count() == 2)
                {
                    //ranges as intervals, takes the highest value
                    val = sl.at(1).toInt(&ok);
                }
                else
                {
                    //ranges as discrete values, takes directly the value
                    val = s.toInt(&ok);
                }
                MY_ASSERT(ok == true)
                MYINFO << "taking closest available bandwidth" << val << " in range " << s << MY_ENDL;
            }
            else
            {
                MYINFO << "Bandwidth " << s << " is in range " << sl.at(i);
                val = loc.toInt(s, &ok);
                MY_ASSERT(ok == true)
                ui->cbBBrange->setCurrentIndex( i );
                s = ui->cbBBrange->currentText();
                delocalizeRange(s);
                as->setBandwidthRange(s);
            }
        }

        s = ui->cbBBrange->currentText();
        sl = s.split(':');
        min = loc.toInt(sl.at(0), &ok);
        MY_ASSERT(ok == true)
        if(sl.count() > 1)
        {
            //range
            max = loc.toInt(sl.at(1), &ok);
        }
        else
        {
            //single value == max
            max = val;
            min = val;
        }
        MY_ASSERT(ok == true)
        ui->sbBB->setMinimum(min);
        ui->sbBB->setMaximum(max);
        ui->sbBB->setValue(val);
        as->setBandwidth(val);

    }
    else
    {
        //this device doesn't provide specific BBR, so it
        //trusts the BBR and BB loaded from rts
        ui->cbBBrange->clear();
        s = as->getBandwidthRange();
        localizeRange(s);
        ui->cbBBrange->addItem( s );
        ui->cbBBrange->setCurrentIndex( 0 );
        ui->sbBB->setMinimum(bb);
        ui->sbBB->setMaximum(bb);
        ui->sbBB->setValue(bb);
    }

    //tuning frequency ranges allowed by device
    sl = r->deviceOpened()->getFreqRanges();
    MYINFO << "loading tuning frequency ranges: " << sl;
    if(sl.at(0) != "N.A.")
    {
        //this device provides specific frequency ranges, so
        //the FR loaded from rts must be checked
        //for compatibility

        ui->cbFreqRange->clear();
        localizeRanges(sl);
        ui->cbFreqRange->addItems( sl );
        MY_ASSERT( ui->cbFreqRange->count() == sl.count() )
        if(sl.count() <= 1 && sl.at(0) == "None")
        {
            //the device has no frequency ranges, creating a brand new one
            MYINFO << "Frequency range " << s << " doesn't match any range.";
            val = as->getTune();
            s = QString("0:%1").arg(val / 1000); //kHz
            as->setFreqRange( s );
            localizeRange(s);
            ui->cbFreqRange->setCurrentText(s);
            MYINFO << "Frequency range " << s << " in kHz, self generated and selected.";
        }
        else
        {
            s = loc.toString( as->getTune() / 1000.0 );
            i = as->isFloatValueInList(sl, s);
            if(i == -1)
            {
                MYINFO << "Tuning frequency " << s << " doesn't match any range.";
                i = 0;
                ui->cbFreqRange->setCurrentIndex( i );
                s = sl.at(i);
                delocalizeRange(s);
                as->setFreqRange(s);
                QStringList sl = s.split(':');

                /*
                 * as default takes the lowest frequency allowed
                 * this for 2 reasons:
                 * 1-SoapyRTLSDR takes as stated the tuner is a R820T so the range returned
                 * could be wider than what supported by device
                 * 2-E4000 based dongles have a tuning gap at 1G1-1G2 so an averaging could
                 * bring to tune in that gap causing exceptions.
                 */
                val = sl.at(0).toInt(&ok);
                MY_ASSERT(ok == true)
                as->setTune(val * 1000);
                MYINFO << "Tuning frequency readjusted to " << val << " to fit the range " << s;
            }
            else
            {
                MYINFO << "Tuning frequency " << s << " is in range " << sl.at(i);
                ui->cbFreqRange->setCurrentIndex( i );
                s = sl.at(i);
                delocalizeRange(s);
                as->setFreqRange(s);
            }
        }
    }
    else
    {
        //this device doesn't provide specific FR, so it
        //trusts the FR loaded from rts
        ui->cbFreqRange->clear();
        s = as->getFreqRange();
        localizeRange(s);
        ui->cbFreqRange->addItem( s );
        ui->cbFreqRange->setCurrentIndex( 0 );
    }

    s = as->getFreqRange();
    MYINFO << "Extract tuning limits" << s;
    sl = s.split(':');
    s = sl.at(0);

    if(as->isAudioDevice())
    {
        //the tuning frequency is set to zero for audio devices
        tune = 0;
        as->setTune(tune);
        minTuneHz = 0;
    }
    else
    {
        minTuneHz = s.toLongLong( &ok ) * 1000;
        MY_ASSERT(ok == true)
    }

    s = sl.at(1);
    maxTuneHz = s.toLongLong( &ok ) * 1000;
    MY_ASSERT(ok == true)
    tune = as->getTune();
    if(tune < minTuneHz || tune > maxTuneHz)
    {
        //audio spectra are represented as tuned to zero Hz
        //starting from the left, frequency raising rightwards
        tune = minTuneHz;
        as->setTune(tune);
    }
    ui->lcdTune->display(loc.toString(tune));

    i = as->getGain();
    if( i == AUTO_GAIN )
    {
        ui->chkAGC->setChecked(true);
        ui->sbGain->setEnabled(false);
    }
    else
    {
        ui->chkAGC->setChecked(false);
        ui->sbGain->setEnabled(true);
        ui->sbGain->setValue(i);
    }

    s = as->getDevice();
    i = ui->cbDevice->findText( s, Qt::MatchFixedString );
    if(i == -1)
    {
        MYDEBUG << "string " << s << " not found in ui->cbDevice" << MY_ENDL;
    }
    else
    {
        ui->cbDevice->setCurrentIndex( i );
    }

    Qt::CheckState k = static_cast<Qt::CheckState>( as->getSsBypass() );
    ui->chkBypass->setChecked(k);
    slotSetSsBypass(k);

    calcDownsamplingFreqs();

    //restore the downsampling frequency taken from rts
    MYINFO << "downsample bw to select: " << dsBwOrig;
    QString candidateDsBw = loc.toString(dsBwOrig);
    int dsIndex = ui->cbDownsampler->findText(candidateDsBw);

    if( dsIndex != -1 )
    {
        MYINFO << "ds found at [" << dsIndex << "] = " << candidateDsBw;
        ui->cbDownsampler->setCurrentIndex(dsIndex);
        dsBw = dsBwOrig;
    }
    else
    {
        candidateDsBw = ui->cbDownsampler->itemText(0);
        dsBw = candidateDsBw.toInt(&ok);
        if(ok)
        {
            ui->cbDownsampler->setCurrentIndex(0);
            as->setDsBandwidth(dsBw);
            MYINFO << "ds NOT found, setting dsBw at[0] = " << candidateDsBw;
        }
    }

    MYINFO << "current downsample bandwidth: " << dsBw;

    d = as->getError();
    ui->sbPpm->setValue( static_cast<double>(d) );


    //cbFtw and cbOutput have fixed values hardcoded in ui
    //that match exactly FFT_WINDOWS and OUT_TYPES

    //FFT settings:
    i =  static_cast<int>( as->getWindow() );
    ui->cbFtw->setCurrentIndex( i );

    as->updateResolution();
    d = as->getResolution();   
    s.setNum(d, 'f', 2);
    ui->lbResolution->setText(s);

    s.setNum( as->getFFTbins() );
    ui->lbPoints->setText(s);


    //output settings

    i = as->getAcqMode();
    switch(i)
    {
        default:
        case AM_CONTINUOUS:
            MYINFO << "Acquisition mode: continuous";
            ui->rbContinuous->setChecked(true);
            break;

        case AM_PERIODIC:
            MYINFO << "Acquisition mode: periodic";
            ui->rbPeriodic->setChecked(true);
            break;

        case AM_AUTOMATIC:
            MYINFO << "Acquisition mode: automatic";
            ui->rbAutomatic->setChecked(true);
            break;
    }


    i = as->getInterval();
    ui->sbInterval->setMinimum(TIME_BASE);
    ui->sbInterval->setValue(i);

    i = as->getShots();
    ui->sbMaxShots->setValue(i);

    i = as->getRecTime();
    ui->sbRecTime->setValue(i);

    i = as->getAfter();
    ui->sbAfter->setValue(i);

    slotSetSbThresholdsParam();

    d = as->getUpThreshold();
    ui->sbUpThreshold->setValue(static_cast<double>(d));

    d = as->getDnThreshold();
    ui->sbDnThreshold->setValue(static_cast<double>(d));

    i = as->getDetectionRange();
    ui->sbTrange->setValue(i);

    k = static_cast<Qt::CheckState>( as->getDumps() );
    ui->chkDumps->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getScreenshots() );
    ui->chkSShots->setChecked(k);


    i = as->getNoiseLimit();
    ui->sbNoiseLimit->setValue(i);

    d = as->getDbfsOffset();
    ui->sbDbOff->setValue(d);

    d = as->getDbfsGain();
    ui->sbDbGain->setValue(d);

    i = as->getAvgdScans();
    ui->sbAvg->setValue(i);

    i = as->getJoinTime();
    ui->sbJoinTime->setValue(i);

    i = as->getMinFree();
    ui->sbMinFree->setValue(i);

    i = as->getPalette();
    ui->cbPalette->setCurrentIndex(i);

    slotUpdateSysInfo();


    //preferences
    k = static_cast<Qt::CheckState>( as->getSec() );
    ui->chkTticks->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getHz() );
    ui->chkFticks->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getDbfs() );
    ui->chkPticks->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getBoundaries() );
    ui->chkBoundaries->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getTooltips() );
    ui->chkTooltips->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getPing() );
    ui->chkEnablePing->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getFaultSound() );
    ui->chkEnableFaultSound->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getEraseLogs() );
    ui->chkEraseOldLogs->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getEraseShots() );
    ui->chkEraseOldShots->setChecked(k);

    k = static_cast<Qt::CheckState>( as->getDATdumps() );
    ui->chkEnableDAT->setChecked(k);

    i = as->getThresholdsBehavior();
    ui->cbThresholds->setCurrentIndex(i);

    i = as->getServerPort();
    ui->sbServerPort->setValue(i);

    i = as->getDatagramDelay();
    ui->sbUDPdelay->setValue(i);

    i = as->getDataLasting();
    ui->sbDataLasting->setValue(i);

    i = as->getImgLasting();
    ui->sbImgLasting->setValue(i);

    i = as->getDBsnap();
    ui->sbDBsnap->setValue(i);

    i = as->getMaxEventLasting();
    ui->sbMaxEvLast->setValue(i);

    k = static_cast<Qt::CheckState>( as->getOverwriteSnap() );
    ui->chkOverwriteSnap->setChecked(k);

    s = as->getServerAddress().toString();
    ui->leServerIP->setText(s);

    i = as->getScanMode();
    switch(i)
    {
    default:
    case SCAN_LAST:
        ui->rbLast->setChecked(true);
        break;

    case SCAN_AVG:
        ui->rbAverage->setChecked(true);
        break;

    case SCAN_MAX:
        ui->rbMax->setChecked(true);
        break;
    }

    wf->show(); 

    //restore geometry
    setGeometry( as->getMainGeometry() );

    blockAllSignals(false);

    //Initialize subwindows
    emit notifyReady();

    //Configuration applied. Any further change will
    //enable the autosave feature
    if(init && !deviceChanged)
    {
        as->clearChanged();
    }

    as->updateRTSformatRev();

    setTitle();

    init = false;
}




void MainWindow::GUIrefresh()
{
    MYDEBUG << "refreshing GUI" << MY_ENDL;

    blockAllSignals(true);

    slotUpdateSysInfo();

    blockAllSignals(false);
}


void MainWindow::GUIcontrolsLogic()
{
    bool ok = false;
    bool refreshDsValues = false;

    int sr = -1, bb = -1, bbr = -1, i = 0;
    QString srrS, srS, bbrS, bbS;
    QStringList sl;

    blockAllSignals(true);

    sr = ui->sbSR->value();
    srS = loc.toString( sr );

    bb = ui->sbBB->value();
    bbS = loc.toString( bb );

    srrS = ui->cbSRrange->currentText();

    bbr = ui->cbBBrange->currentIndex();
    bbrS = ui->cbBBrange->currentText();

    if(SRRchanged)
    {
        sl.append(srrS);
        i = as->isIntValueInList(sl, srS);
        if(i == -1)
        {
            MYINFO << "Sample rate " << sr << " is out of range: " << srrS << ", fixing...";
            sl = srrS.split(":");
            int min = loc.toInt(sl.at(0), &ok);
            MY_ASSERT(ok == true)
            int max = min;
            if(!as->isAudioDevice() && sl.count() == 2)
            {
                //gets the max only if srrS is a range, not a single number
                max = loc.toInt(sl.at(1), &ok);
                MY_ASSERT(ok == true)
            }

            //takes the highest possible SR for the new range
            sr = max;
            srS = loc.toString(max); //sl.at(1);
            ui->sbSR->setMinimum(min);
            ui->sbSR->setMaximum(max);
            ui->sbSR->setValue(sr);
            qApp->processEvents();
            SRchanged = true;
            MYINFO << "mainwindow-Sample rate set to " << sr;
        }


    }


    if(SRchanged)
    {

        if(sr != bb)
        {
            sl = bbrS.split(':');
            int min = loc.toInt(sl.at(0), &ok);
            MY_ASSERT(ok == true)
            int max = min;
            if(!as->isAudioDevice() && sl.count() > 1)
            {
                //range
                max = loc.toInt(sl.at(1), &ok);
            }

            MY_ASSERT(ok == true)
            bb = max;
            bbS = loc.toString(bb);
            ui->sbBB->setMinimum(min);
            ui->sbBB->setMaximum(max);
            ui->sbBB->setValue(bb);
            qApp->processEvents();
            BBchanged = true;

        }
        else
        {
            ui->sbBB->setMaximum(sr);
        }

    }

    if(BBRchanged)
    {
        sl.append(bbrS);
        i = as->isIntValueInList(sl, bbS);
        if(i == -1)
        {
            MYINFO << "bandwidth " << bb << " is out of range " << bbrS << ", fixing...";
            sl = bbrS.split(':');
            int min = 0, max = 0;
            if(sl.count() == 2)
            {
                //range full expressed min:max
                min = loc.toInt(sl.at(0), &ok);
                MY_ASSERT(ok == true)
                max = loc.toInt(sl.at(1), &ok);
                MY_ASSERT(ok == true)
                max = (max > sr)? sr : max;
            }
            else
            {
                //range expressed as single scalar, the minimum is equal to maximum
                min = loc.toInt(sl.at(0), &ok);
                MY_ASSERT(ok == true)
                max = min;
            }

            bb = max;

            bbS = loc.toString(bb);
            ui->sbBB->setMinimum(min);
            ui->sbBB->setMaximum(max);
            ui->sbBB->setValue(bb);
            qApp->processEvents();
            BBchanged = true;
            MYINFO << "bandwidth set to " << bb;
        }

    }

    blockAllSignals(false);

    //updating settings must be done when all changes have been defined
    //since they fire up notification signals
    if(SRRchanged)
    {
        delocalizeRange(srrS);
        as->setSampleRateRange( srrS );
        SRRchanged = false;
    }

    if(SRchanged)
    {

        QString s;
        s = as->getFreqRange();
        MYINFO << "Extract tuning limits" << s;
        sl = s.split(':');
        s = sl.at(0);

        if(as->isAudioDevice())
        {
            //audio spectra are represented as tuned to zero Hz
            //starting from the left, frequency raising rightwards
            minTuneHz = 0;
            maxTuneHz = 0;
            tune = 0;
            as->setTune(0);
        }
        else
        {
            minTuneHz = s.toLongLong(&ok) * 1000;
            MY_ASSERT(ok == true)
            if( minTuneHz < (sr/2) )
            {
                minTuneHz = sr / 2;
            }

            s = sl.at(1);
            maxTuneHz = s.toLongLong( &ok ) * 1000;
            MY_ASSERT(ok == true)
            tune = as->getTune();
            if(tune < minTuneHz || tune > maxTuneHz)
            {
                tune = minTuneHz;
                as->setTune(tune);
            }
        }

        ui->lcdTune->display(loc.toString(tune));
        as->setSampleRate( sr );
        refreshDsValues = true;
        SRchanged = false;
    }


    if(BBRchanged)
    {
        delocalizeRange(bbrS);
        as->setBandwidthRange(bbrS);
        BBRchanged = false;
    }

    if(BBchanged)
    {
        as->setBandwidth(bb);
        BBchanged = false;
    }

    if(refreshDsValues)
    {
        calcDownsamplingFreqs();
        slotAdjustResolution();
        as->setOffset(DEFAULT_OFFSET);
        as->setZoom(DEFAULT_ZOOM);
    }

}

bool MainWindow::calcDownsamplingFreqs()
{
    //The downsampled bandwidth is set equal to SR
    //unless a valid value has been set before.

    //The number of downsampling rates is calculated in base to
    //the maximum resolution alloweed by the current device,
    //based on its streamMTU

    bool done = false;
    QStringList dsValues;

    dsBw = as->getDsBandwidth(); //current value

    int dsFreq = as->getSampleRate();
    int FFTbins = as->getFFTbins();
    int current = -1;

    MYINFO << "New downsampling frequencies ";
    MYINFO << " looking for: " << dsBw;;
    for(int index = 0; index < MAX_DOWNSAMPLING_VALUES; index++)
    {
        MYINFO << "index=" << index << dsFreq;
        if(FFTbins > as->getMaxResMTU())
        {
            MYINFO << "Discarding " << dsFreq << "Hz because " << FFTbins << " exceeds maxMTU";
            break;
        }
        dsValues.append( loc.toString(dsFreq) );
        if(dsFreq == dsBw)
        {
            current = index;
            done = true;
        }
        dsFreq /= 2;
        FFTbins *= 2;

    }

    if(current == -1 || as->getSsBypass() == Qt::Checked)
    {
        //downsampling out of alloweed range (> SR)
        //realign dsBw setting to SR
        MYINFO << "New dsBw == SR";
        as->setDsBandwidth(as->getSampleRate(), true);
        current = 0;
    }

    blockAllSignals(true);
    ui->cbDownsampler->clear();
    ui->cbDownsampler->addItems( dsValues );
    MYINFO << "current index=" << current;
    ui->cbDownsampler->setCurrentIndex(current);
    ui->cbDownsampler->setEnabled(as->getSsBypass() != Qt::Checked);
    blockAllSignals(false);

    return done;
}





//buttons handling:

void MainWindow::slotQuit()
{
     MYDEBUG << "Quit";

    emit acquisitionAbort(ASC_MANUAL_REQ);

    //closes the application
    qApp->closeAllWindows();
}


void MainWindow::slotAboutDialog()
{
    QString desc;
    QTextStream ts(&desc);
    XQDir wd = ac->workingDir();
    QString gpl = "Copyright (C) 2018 --> 2024  Giuseppe Massimo Bertani "
        "gmbertani(a)users.sourceforge.net "
        "This program is free software: you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation, version 3 of the License."
        "This program is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details. "
        "You should have received a copy of the GNU General Public License "
        "along with this program.  If not, http://www.gnu.org/copyleft/gpl.html ";


    ts << "ECHOES - " << tr(TARGET_DESC) << MY_ENDL;
    ts << "V." << APP_VERSION << "." << MY_ENDL;
    ts << "Build date:" << BUILD_DATE << "." << MY_ENDL;

#ifdef _DEBUG
     ts << "UNOFFICIAL-COMPILED FOR DEBUGGING PURPOSES" << MY_ENDL;
#endif
    ts << "Build architecture: " << QSysInfo::buildAbi() << MY_ENDL;
    ts << "Qt version: " << QT_VERSION_STR << MY_ENDL;
    ts << "Compiler brand: " << COMPILER_BRAND << MY_ENDL;
    ts << MY_ENDL << gpl << MY_ENDL << MY_ENDL;
    ts << tr("Working directory: ") << MY_ENDL ;
    ts << wd.absolutePath() << MY_ENDL << MY_ENDL;

    ts << "Currently running on " << QSysInfo::productType() << " v." << QSysInfo::productVersion()
        << " (" <<  QSysInfo::currentCpuArchitecture() << ")" << MY_ENDL;

    QMessageBox::about
    (
        this,
        tr("Informations about Echoes"),
        desc
    );

}


void MainWindow::slotReopen(const QString* dev)
{
    MYINFO << __func__ << "(" << dev << ")" << MY_ENDL;

    bool ok = false;
    showBusy();

    ac->slotStopAcquisition(ASC_MANUAL_REQ);

    wf->slotConfigChanged();

    setTitle();

    //load combobox values first
    DeviceMap dm, curr;
    
    ui->cbDevice->blockSignals(true);
    ui->cbDevice->clear();

    //If dev is nullptr,
    //opens the first SDR device found.
    //if none is found, the dongle client will be opened, but
    //if no servers are available, as fallback opens the test patterns device

    foreach(dm, r->getDevices(false))   // was true
    {
        MYINFO << "adding " << dm.name;
        ui->cbDevice->addItem( dm.name );
        if (dev != nullptr && dm.name.contains(*dev))
        {
            ok = true;
            ui->cbDevice->setCurrentText(*dev);
            curr = dm;
        }
    }

    if(!ok)
    {
        dm = r->getDevices().at(0);
        curr = dm;
    }

    MY_ASSERT( ui->cbDevice->count() == r->getDevices().count() ) //paranoia: the max nr.of elements has been set in designer

    int idx = -1;

    /*
     * I have to discriminate the case of when i select an audio device and i had loaded a NON audio device from rts
     * because the tune must be shifted to zero and samplerate and dsBw must assume default values.
     * This should NOT happen if the rts loaded was about an audio device.
     */
    bool toAudioDevice = false;

    //when operating on device selector, the user wants to change the device in use with another:
    QString rtsDev = as->getDevice();
    QString selDev = ui->cbDevice->currentText();
    if(init)
    {
        //but the first time the program is started, the device to use must always be the one
        //described by rts file just loaded, regardless the device selector that will be
        //adjusted afterwards
        rtsDev = as->getDevice();
        selDev = ui->cbDevice->currentText();
        MYINFO << "required device=" << rtsDev;
    }

    if( rtsDev != selDev || init )
    {
        if(!init)
        {
            if ( selDev.contains("audio") && !rtsDev.contains("audio"))
            {
                toAudioDevice = true;
                MYINFO << "switching to audio device, tune is set to zero and SR and dsBw will take default values";
            }

            //if not in init, the selected device is sure it exists
            MYINFO << "opening " << selDev;
            idx = ui->cbDevice->findText( selDev );
        }
        else
        {
            //otherwise tries to open the device specified in rts
            MYINFO << "try open " << rtsDev;
            idx = ui->cbDevice->findText( rtsDev, Qt::MatchContains);
            if (idx < 0)
            {
                //The device specified in rts file is not listed in the combobox or has a different index number
                //
                //Retry to find it, after removing the index number
                QString deviceInCb;
                QString deviceNoIdx = as->getDeviceNoIdx(rtsDev);
                MYINFO << "The device specified in rts file is not listed in the combobox or has a different index number";
                MYINFO << "Retry to find it, after removing the index number = " << deviceNoIdx;
                for(idx = 0; idx <  ui->cbDevice->count(); idx++)
                {
                    deviceInCb = ui->cbDevice->itemText(idx);
                    if( deviceInCb.contains( deviceNoIdx ))
                    {
                        MYINFO << "Found " << deviceNoIdx << " at index " << idx;
                        break;
                    }
                    MYINFO << "Device " << deviceNoIdx << " not in " << deviceInCb;
                }

                if(idx >= ui->cbDevice->count())
                {
                    showFree();
                    QMessageBox::warning(this, QObject::tr("Warning"),
                        QObject::tr("Mismatched dongle in configuration file.\n"
                                    "Please select an available device\n"
                                    "before starting acquisition")
                    );

                    ast = AST_ERROR;
                    slotShowStatus( tr("Please select another input device"), ast);
                    curr = r->getDevices().at( r->getDevices().length() - 1 );
                    ui->cbDevice->setCurrentIndex(curr.index + 1); //forces wrong index
                    ui->cbDevice->blockSignals(false);


                    idx = 0; //fails back to device 0 in list that must exist, at least as test patterns
                    MYINFO << "Fails back to device index 0" ;

                }
            }
        }

        if (idx >= 0)
        {
            curr = r->getDevices().at(idx);
            MYINFO << "Device " << curr.name << " found";
        }
    }


    ui->cbDevice->setCurrentIndex(idx);    
    ui->cbDevice->blockSignals(false);

    while(idx < r->getDevices().count())
    {
        ast = r->slotSetDevice( curr.index );
        if(ast == AST_OPEN)
        {
            as->setDevice( r->getDeviceName(), false );
            slotShowStatus( tr("Input device opened"), ast);

            if( r->slotSetAntenna() == true )
            {
                if( r->slotSetSampleRate() == true )
                {
                    if( r->slotSetBandwidth() == true )
                    {
                        if( r->slotSetDsBandwidth() == true ) //note: this control is in FFT tab
                        {
                            if(r->slotSetResolution() == true)
                            {
                                if(r->slotSetFreqRange() == true)
                                {
                                    if(r->slotSetGain() == true)
                                    {
                                        if(r->slotSetError() == true)
                                        {
                                            if( as->getDevice().contains("audio", Qt::CaseInsensitive) )
                                            {
                                                //for audio devices, the frequency controls are inhibited
                                                //and the tuning frequency is set to zero
                                                as->setAudioDevice(true, toAudioDevice);
                                                as->setTune(0);
                                            }
                                            else if( as->getDevice().contains("UDP", Qt::CaseInsensitive) )
                                            {
                                                as->setNetDevice(true);
                                            }
                                            else if( as->getDevice().contains("test patterns", Qt::CaseInsensitive) )
                                            {
                                                as->setTestDevice(true);
                                            }
                                            else
                                            {
                                               as->setAudioDevice(false);
                                               as->setNetDevice(false);
                                               as->setTestDevice(false);
                                            }

                                            if(r->slotSetTune() == true)
                                            {
                                                ok = true;
                                                slotShowStatus( tr("Input device initialized"), ast);
                                                MYINFO << "Device " << as->getDevice() << " opened";
                                                if (as->getFFTbins() < 128)
                                                {
                                                    as->setResBins(DEFAULT_FFT_BINS);
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        idx++;
    }

    if(ok == false && ast <= AST_OPEN)
    {
        MYCRITICAL << "Failed input device configuration, ast=" << ast;
        showFree();
        slotQuit();
        exit(0);
    }

    slotGUIinit();


    sysCheck->setInterval(SYS_CHECK_INTERVAL);
    sysCheck->setSingleShot(false);
    sysCheck->start();

    showFree();

    MYINFO << "Reopen end" << MY_ENDL;
}

void MainWindow::slotOpenConfig()
{

#ifdef NO_NATIVE_DIALOGS
    //native dialogs buggy under linux
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open configuration file"),
        ac->workingDir().absolutePath(),
        tr("Echoes configuration file (*.rts *.RTS)"),
        0,
        QFileDialog::DontUseNativeDialog);

#else

    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open configuration file"),
        ac->workingDir().absolutePath(),
        tr("Echoes configuration file (*.rts *.RTS)"));
#endif

    if( fileName.isEmpty() )
    {
        //no file specified
        return;
    }

    slotShowStatus(tr("Loading configuration ")+fileName );
    //init = true;
    as->loadConfig(fileName);
    ac->updateRootDir();
    ui->lbConfig->setToolTip(tr("Configuration file loaded: ") + fileName);
    slotShowStatus(tr("Ready."));
    setTitle();

}


void MainWindow::slotSaveAs()
{
#ifdef NO_NATIVE_DIALOGS
    //native dialogs buggy under linux
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save configuration as"),
        ac->workingDir().absolutePath(),
        tr("Echoes configuration file (*.rts *.RTS)"),
        0,
        QFileDialog::DontUseNativeDialog);
#else
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save configuration as"),
        ac->workingDir().absolutePath(),
        tr("Echoes configuration file (*.rts *.RTS)"));
#endif

    if( filePath.isEmpty() )
    {
        //nessun nome di file specificato.
        return;
    }

    QFileInfo fi(filePath);
    QString baseName = fi.baseName();
    baseName.truncate(MAX_NAMES_SIZE);
    as->setConfigName( fi.dir(), baseName );
    slotShowStatus(tr("Saved configuration as ") + baseName);   
    init = true;
    as->loadConfig(filePath);
    ac->updateRootDir();
    ui->lbConfig->setToolTip(tr("Configuration file loaded: ") + baseName);
    setTitle();
    slotShowStatus(tr("Ready."));
}

void MainWindow::slotSave()
{

    slotShowStatus(tr("Saving configuration..."));
    XQDir wd = ac->workingDir();
    QString cfg = QString("%1%2").arg( as->getConfigName() ).arg(CONFIG_EXT);
    MYINFO << "Saving " << cfg ;
    QSettings ini( wd.absoluteFilePath(cfg), QSettings::IniFormat );
    as->save(ini);

    setTitle();
    slotShowStatus(tr("Ready."));
}


void MainWindow::slotStart()
{
    MYINFO << "Pressed Start";
    if(ast == AST_INIT)
    {
        slotReopen(nullptr);
    }

    if(ast == AST_OPEN)
    {
        //acquisition start
        emit acquisitionStatus(true);
        slotShowStatus( tr("Acquisition running"), AST_STARTED );
    }
}


void MainWindow::slotStop()
{
    MYINFO << "Pressed Stop";

    if(ast == AST_STARTED)
    {
        sysCheck->stop();
        emit acquisitionStatus(false);
    }
}


void MainWindow::slotAcqStopped()
{
    //called by Control to notify about an
    //automatic acquisition stop

    slotShowStatus( tr("Acquisition stopped"), AST_OPEN );
    showFree(true);
}


void MainWindow::slotAcqStarted()
{
    //called by Control to notify about an
    //automatic acquisition restart (after midnight UTC)

    slotShowStatus( tr("Acquisition restarted"), AST_STARTED );
    showFree();
}


void MainWindow::slotEnterTune(const QPoint& pos)
{
    bool ok;

    MYINFO << "Entering new tune: " << pos;


    QString freqStr = loc.toString(tune);

    freqStr = QInputDialog::getText( this, tr("Enter tune"),
        tr("Frequency [Hz]:"), QLineEdit::Normal, freqStr, &ok  );

    if (ok == true)
    {
        int val = freqStr.toLongLong(&ok);
        if(ok == true)
        {
            if(val >= minTuneHz && val <= maxTuneHz)
            {
                tune = val;
                ui->lcdTune->display(loc.toString(tune));
                as->setTune(tune);
            }
            else
            {
                MYWARNING << "Value entered: " << val
                    << " exceeds the range limits " << minTuneHz
                    << " : " << maxTuneHz;
            }
        }
    }
}

void MainWindow::slotSetSsBypass( int value )
{
    as->setSsBypass( value  );

    if(value == Qt::Checked)
    {
        //downsampler bypassed
        ui->cbDownsampler->setCurrentIndex(0);
        as->setDsBandwidth(as->getSampleRate());
        ui->cbDownsampler->setEnabled(false);
    }
    else
    {
        ui->cbDownsampler->setEnabled(true);
    }

}

//---------------------------

void MainWindow::slotTuneAdd10M()
{

    //+10Mhz steps:
    tune = as->getTune();
    if((tune+10e6) <= maxTuneHz)
    {
        tune += 10e6;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneAdd1M()
{
    //+1MHz steps
    tune = as->getTune();
    if((tune+1e6) <= maxTuneHz)
    {
        tune += 1e6;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneAdd100k()
{

    //+100kHz steps:
    tune = as->getTune();
    if((tune+100e3) <= maxTuneHz)
    {
        tune += 100e3;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneAdd10k()
{

    //+10kHz steps:

    tune = as->getTune();
    if((tune+10000) <= maxTuneHz)
    {
        tune += 10000;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneAdd1k()
{

    //+1kHz steps:

    tune = as->getTune();
    if((tune+1000) <= maxTuneHz)
    {
        tune += 1000;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneSub10M()
{
    //-10MHz steps:

    tune = as->getTune();

    if(tune >= 10e6 &&
       (tune-10e6 > minTuneHz) )
    {
        tune -= 10e6;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneSub1M()
{
    //-1MHz steps
    tune = as->getTune();
    if(tune >= 1e6 &&
       (tune-1e6) > minTuneHz )
    {
        tune -= 1e6;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneSub100k()
{

    // -100kHz steps:

    tune = as->getTune();
    if(tune >= 100e3 &&
       (tune-100e3) > minTuneHz )
    {
        tune -= 100e3;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }
}

void MainWindow::slotTuneSub10k()
{

    //-10kHz steps:
    tune = as->getTune();

    if(tune >= 10000 &&
       (tune-10000) > minTuneHz )
    {
        tune -= 10000;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }

}

void MainWindow::slotTuneSub1k()
{

    //-1kHz steps:
    tune = as->getTune();

    if(tune >= 1000 &&
       (tune-1000) > minTuneHz )
    {
        tune -= 1000;
        as->setTune(tune);
        tune = as->getTune();
        ui->lcdTune->display(loc.toString(tune));
    }

}

//--------------------------
bool MainWindow::slotSetDevice( const QString& text )
{
    MYINFO << __func__ << "(" << text << ")";
    int value = ui->cbDevice->findText( text, Qt::MatchFixedString );
    if(value == -1)
    {
        MYDEBUG << "string " << text << " not found in ui->cbDevice" << MY_ENDL;
        return false;
    }

    //resets any unsaved parameter change and restore the
    //settings from rts file before opening the new device.
    QString currentConfig = as->getConfigFullPath();
    as->loadConfig( currentConfig, false );

    as->setDevice(text, false);
    as->setZoom(DEFAULT_ZOOM);
    as->setOffset(DEFAULT_OFFSET);
    as->setSsBypass(Qt::Checked, false);
    ui->cbDownsampler->setEnabled(false);
    deviceChanged = true;

    slotShowStatus( tr("Device switched"), AST_INIT );
    slotReopen(&text);
    return true;
}

void MainWindow::slotSetAntenna( const QString& text )
{
    int value = ui->cbAntenna->findText( text, Qt::MatchFixedString );
    if(value == -1)
    {
        MYDEBUG << "string " << text << " not found in ui->cbAntenna" << MY_ENDL;
    }
    else
    {
        QString valueString = ui->cbAntenna->itemText(value);
        if(valueString.isEmpty())
        {
            MYDEBUG << "string " << valueString << " not found in ui->cbAntenna" << MY_ENDL;
        }
        else
        {
            as->setAntenna( valueString, false );
        }
    }
}

void MainWindow::slotSetSampleRateRange( const QString& text )
{
    if(text.length() == 0)
    {
        return;
    }
    slotStop();
    SRRchanged = true;
    GUIcontrolsLogic();
}


void MainWindow::slotSetSampleRate( int val )
{
    Q_UNUSED(val)
    slotStop();
    SRchanged = true;
    GUIcontrolsLogic();
}



void MainWindow::slotSetBandwidthRange( const QString& text )
{
    if(text.length() == 0)
    {
        return;
    }
    BBRchanged = true;
    GUIcontrolsLogic();
}



void MainWindow::slotSetBandwidth(int val)
{    
    Q_UNUSED(val)
    BBchanged = true;
    GUIcontrolsLogic();
}


void MainWindow::slotSetFreqRange( const QString& text )
{
    if(text.length() == 0)
    {
        return;
    }

    bool ok = false;
    QString freq;
    freq.setNum( as->getTune() );
    QString fRange = text;
    QStringList sl;
    sl.append(fRange);
    int i = as->isIntValueInList(sl, freq);
    if(i == -1)
    {
        MYINFO << "Tuning frequency " << freq << " doesn't match this range: " << fRange;
        sl = fRange.split(":");
        freq = sl.at(0);
        minTuneHz = freq.toInt(&ok);
        MY_ASSERT(ok == true)
        freq = sl.at(1);
        maxTuneHz = freq.toInt(&ok);
        MY_ASSERT(ok == true)
        tune = minTuneHz;
        as->setTune(tune, false);
        ui->lcdTune->display(loc.toString(tune));
        qApp->processEvents();

        MYINFO << "Taking the highest valid tuning frequency for this range: " << tune;
    }

    as->setFreqRange(fRange, false);

}



void MainWindow::slotSetAutoGain( int autoGain )
{
    if( autoGain == Qt::Checked )
    {
        MYDEBUG << "selected AUTO gain" << MY_ENDL;
        as->setGain(AUTO_GAIN);
        ui->sbGain->setEnabled(false);
        ui->sbGain->setValue(100);
    }
    else
    {
        int gain = ui->sbGain->value();
        as->setGain(gain);
        ui->sbGain->setEnabled(true);
        MYDEBUG << "selected MANUAL gain: " << gain << MY_ENDL;
    }

}

void MainWindow::slotSetGain( int newGain )
{
    as->setGain(newGain);
}




void MainWindow::slotAdjustResolution()
{
    //automatically sets the label value after
    //change by the +/- pushbuttons

    double res = static_cast<double>( as->pround( as->getResolution(), 2 ) );
    ui->lbResolution->setNum( res );

    int bins = as->getFFTbins();
    ui->lbPoints->setNum( bins  );
    //refreshDownsamplingFreqs(false);
}


void MainWindow::slotValidateUpThreshold()
{

}

void MainWindow::slotValidateDnThreshold()
{
}

void MainWindow::slotShowStatus(const QString& status, int appState )
{
    //this slot is called by Control and Postproc thread
    QMutexLocker ml(&protectionMutex);

    QWidget* w = nullptr;
    if(appState != AST_NONE && appState != AST_ERROR)
    {
        ast = appState;
        emit stateChanged(ast);
    }

    if(appState == AST_ERROR)
    {
        //temporary error, the device status remains unaffected
        //this status is not stored, neither notified
        ui->lbStatus->setStyleSheet("background-color: red; color: yellow;");
    }
    else if(ast == AST_CANNOT_OPEN)
    {
        /*
         * Device found, but error opening it
         * in this situation the user must be able to change device only
         */
        for(int i = GT_GLOBAL; i < GT_TOTAL; i++)
        {
            foreach(w, guiWidgetsList[i])
            {
                w->setEnabled(false);
            }
        }
        ui->pbQuit->setEnabled(true);
        ui->cbDevice->setEnabled(true);
        ui->lbStatus->setStyleSheet("background-color: red; color: yellow;");
    }
    else if(ast > AST_CANNOT_OPEN)
    {
        //Device detected and opened, all controls are enabled
        //is responsability of each device to consider or not
        //the actions on these controls.
        QString deviceClass;
        if( r->deviceOpened() )
        {
            deviceClass = r->deviceOpened()->className();
        }

        for(int i = GT_GLOBAL; i < GT_TOTAL; i++)
        {
            bool enable = true;
            bool multipleAntennas = false;
            bool multipleSRR = false;
            bool multipleBBR = false;
            bool multipleFR = false;
            bool multipleDiscreteSRs = false;
            bool noBBR = false;

            foreach(w, guiWidgetsList[i])
            {
                w->setEnabled(enable);
            }

            if(i == GT_DEVICE)
            {
                if( deviceClass == "DongleClient" )
                {
                    //on running client sessions the controls in device tab are all disabled
                    enable = false;
                }
                else if( deviceClass == "SyncRx")
                {
                    //some devices can't set a frequency error correction value
                    ui->lbPpm->setEnabled(r->allowsFrequencyCorrection());
                    ui->sbPpm->setEnabled(r->allowsFrequencyCorrection());

#ifndef __DEBUG //remove a leading underscore to inhibit unused controls hiding in debug mode

                    //the SDR has selectable multiple antennas
                    if(r->deviceOpened()->getAntennas().length() > 1)
                    {
                        multipleAntennas = true;
                    }

                    //the SDR has selectable sample rate ranges
                    if(r->deviceOpened()->getSRranges().length() > 1)
                    {
                        multipleSRR = true;
                        QString testRange = r->deviceOpened()->getSRranges().at(0);
                        QStringList rangeItem = testRange.split(':');
                        if(rangeItem.count() < 2)
                        {
                            //SRR is not a list of ranges but
                            //a list of discrete SR values
                            multipleDiscreteSRs = true;
                        }
                    }

                    //the SDR has selectable band filters
                    if(r->deviceOpened()->getBBranges().length() > 1)
                    {
                        multipleBBR = true;
                    }


                    //the SDR has selectable frequency ranges
                    if(r->deviceOpened()->getFreqRanges().length() > 1)
                    {
                        multipleFR = true;
                    }

                    //SDR devices have no bandwidth control in hw
                    if(as->getDevice().contains("RTL", Qt::CaseInsensitive))
                    {
                        noBBR = true;
                    }
#else
                    multipleAntennas = true;
                    multipleSRR = true;
                    multipleBBR = true;
                    multipleFR = true;
                    multipleDiscreteSRs = true;
#endif //_DEBUG
                }
                else if( deviceClass == "FuncGen" )
                {
                    noBBR = true;
                    ui->lbPpm->setEnabled(false);
                    ui->sbPpm->setEnabled(false);
                }

                if( as->isAudioDevice() != 0 )
                {
                    //for audio devices, the tune, gain and error correction controls are inhibited
                    //SoapyAudio do not provide input level (gain) facility - it's totally dummy.
                    //the audio input level must be managed via operating system
                    ui->lbGain->setVisible(false);
                    ui->sbGain->setVisible(false);
                    ui->chkAGC->setVisible(false);

                    ui->lbPpm->setVisible(false);
                    ui->sbPpm->setVisible(false);

                    ui->lbAntenna->setVisible(false);
                    ui->cbAntenna->setVisible(false);

                    ui->lbBBrange->setVisible(false);
                    ui->cbBBrange->setVisible(false);

                    ui->lbBB->setVisible(false);
                    ui->sbBB->setVisible(false);

                    ui->lbSR->setVisible(enable);
                    ui->sbSR->setVisible(enable);

                    if(!multipleDiscreteSRs)
                    {
                        ui->lbSR->setVisible(enable);
                        ui->sbSR->setVisible(enable);
                    }
                    else
                    {
                        ui->lbSR->setVisible(false);
                        ui->sbSR->setVisible(false);
                    }

                    ui->lbFreqRange->setVisible(false);
                    ui->cbFreqRange->setVisible(false);

                    ui->gBoxTuning->setVisible(false);
                }
                else
                {
                    ui->lbGain->setVisible(enable);
                    ui->sbGain->setVisible(enable);
                    ui->chkAGC->setVisible(enable);

                    ui->lbPpm->setVisible(enable);
                    ui->sbPpm->setVisible(enable);

                    if(multipleAntennas)
                    {
                        ui->lbAntenna->setVisible(enable);
                        ui->cbAntenna->setVisible(enable);
                    }
                    else
                    {
                        ui->lbAntenna->setVisible(false);
                        ui->cbAntenna->setVisible(false);
                    }

                    if(multipleSRR)
                    {
                        ui->lbSRrange->setVisible(enable);
                        ui->cbSRrange->setVisible(enable);

                        if(!multipleDiscreteSRs)
                        {
                            ui->lbSR->setVisible(enable);
                            ui->sbSR->setVisible(enable);
                        }
                        else
                        {
                            ui->lbSR->setVisible(false);
                            ui->sbSR->setVisible(false);
                        }
                    }
                    else
                    {
                        ui->lbSRrange->setVisible(false);
                        ui->cbSRrange->setVisible(false);
                    }

                    if(multipleBBR)
                    {
                        ui->lbBBrange->setVisible(enable);
                        ui->cbBBrange->setVisible(enable);
                    }
                    else
                    {
                        ui->lbBBrange->setVisible(false);
                        ui->cbBBrange->setVisible(false);
                    }

                    if(noBBR)
                    {
                        ui->lbBB->setVisible(false);
                        ui->sbBB->setVisible(false);
                    }
                    else
                    {
                        ui->lbBB->setVisible(enable);
                        ui->sbBB->setVisible(enable);
                    }

                    ui->lbSR->setVisible(enable);
                    ui->sbSR->setVisible(enable);

                    ui->cbFreqRange->setVisible(enable);
                    ui->lbFreqRange->setVisible(enable);
                    if(multipleFR)
                    {
                        ui->cbFreqRange->setEnabled(enable);
                    }
                    else
                    {
                        ui->cbFreqRange->setEnabled(false);
                    }

                    ui->gBoxTuning->setVisible(enable);
                }

            }

            if(i == GT_FFT)
            {          
                if( deviceClass == "FuncGen" )
                {
                    ui->chkBypass->setChecked(enable);
                    as->setSsBypass(Qt::Checked);
                    ui->chkBypass->setEnabled(false);
                    ui->cbDownsampler->setEnabled(false);
                }
                else if( deviceClass == "DongleClient" )
                {
                    ui->chkBypass->setEnabled(enable);
                    ui->pbDecRes->setEnabled(false);
                    ui->pbIncRes->setEnabled(false);
                    ui->cbDownsampler->setEnabled(false);
                }
                else
                {
                    ui->chkBypass->setEnabled(enable);
                    ui->cbDownsampler->setEnabled( as->getSsBypass() != Qt::Checked );
                }


            }

        }
        //the device tab is enabled when acquisition is stopped
        //to allow change device
        ui->cbDevice->setEnabled(true);

        //acquisition started, locks the device tab controls
        //because in order to change the settings, some devices
        //requires acquisition stopped. This is the dumbest way to
        //ensure that condition
        if(ast == AST_STARTED)
        {
            //The reporting controls are never locked because they
            //must apply on PostProc

            QList<GUItabs> toBeDisabled = {GT_GLOBAL};

            //The device tab controls are disabled while running
            //as UDP client or through an audio device

            if(as->getDevice().contains("UDP server"))
            {
               //only these two must be disabled in client's FFT tab
               //in case working as UDP client
               ui->pbIncRes->setEnabled(false);
               ui->pbDecRes->setEnabled(false);
            }


            GUItabs i;
            foreach (i, toBeDisabled)
            {
                foreach(w, guiWidgetsList[i])
                {
                    w->setEnabled(false);
                }
            }
            ui->pbQuit->setEnabled(true);
            ui->pbStart->setEnabled(true);
            //update windows titlebars
            setTitle();
        }
        ui->lbStatus->setStyleSheet("background-color: #282828; color: #00ff00");

    }

    if( QString::compare(ui->lbStatus->text(), status, Qt::CaseInsensitive) != 0 )
    {
        MYINFO << "Status bar: from<" << ui->lbStatus->text() << ">"
                << " to <" << status << ">" ;
        ui->lbStatus->setText(status);
    }

    //workaround for tooltips bug to avoid text truncation
    //see: http://www.qtcentre.org/threads/43959-QToolTip-doesnt-wrap-text
    QString filler;
    filler.fill(' ', status.size());
    QString tt = status+filler;

    ui->lbStatus->setToolTip(tt);
}

void MainWindow::slotUpdateSysInfo()
{
    if(init)
    {
        //no measurement while initializing
        return;
    }

    if(outputEnabled == false)
    {
        slotSetAcqMode( static_cast<E_ACQ_MODE>( as->getAcqMode() ) );
        outputEnabled = true;
    }

    ui->lbMeasuredInterval->setText( QString("%1 mS").arg( ac->measuredInterval() ) );

    XQDir archiveDir(
            QString("%1/%2")
            .arg( ac->workingDir().absolutePath() )
            .arg( as->getConfigName() )
    );

    ui->lbFreeStorage->setText("0 MB");
    QStorageInfo sti(archiveDir);
    if( sti.isValid() )
    {
        double gigaFree = sti.bytesFree();
        gigaFree /= 0x40000000L;
        QString s = QString("%L1 GB").arg( gigaFree, 0, 'g', 3 );
        ui->lbFreeStorage->setText( s );

        if( ast == AST_STARTED &&
            static_cast<double>(as->getMinFree()) > gigaFree )
        {
            MYWARNING << "Available space on drive has fallen below " << as->getMinFree() << " GB, stopping acquisition.";
            slotStop();
            QMessageBox::warning(this, tr("Warning"), tr("Available space on drive has fallen\n below %1 GB limit.\nAcquisition stopped.")
                .arg( static_cast<double>(as->getMinFree()) ), QMessageBox::Ok);
        }
    }
    else
    {
        //fixing BUG CALINA 1: this folder must be created by updateRootDir() only
        //archiveDir.mkpath( archiveDir.absolutePath() );
    }


}



void MainWindow::slotFocusChanged(QWidget*old, QWidget*now)
{
    Q_UNUSED(now)
    if(old == ui->leServerIP)
    {
        QHostAddress ip( ui->leServerIP->text() );
        if(ip != as->getServerAddress())
        {
            as->setServerAddress(ip.toString());

            //update device selector combobox
            DeviceMap dm, curr;

            ui->cbDevice->blockSignals(true);
            ui->cbDevice->clear();
            foreach(dm, r->getDevices(true))
            {
                MYINFO << "adding " << dm.name;
                ui->cbDevice->addItem( dm.name );
                if(dm.name == ui->cbDevice->currentText())
                {
                    curr = dm;
                }
            }
        }
    }
}


bool MainWindow::blockAllSignals(bool block)
{
    bool wereBlocked = false;
    QWidget* w = nullptr;
    for(int i = GT_GLOBAL; i < GT_TOTAL; i++)
    {
        foreach(w, guiWidgetsList[i])
        {
            if( w->signalsBlocked() )
            {
                wereBlocked = true;
            }
            w->blockSignals(block);

        }
    }

    wf->blockAllSignals(block);
    return wereBlocked;
}


void MainWindow::setTitle()
{
    QString title;
    QTextStream ts(&title);
    ts << "Echoes v." << APP_VERSION ;
    setWindowTitle(title);
    title.clear();
    ts << "Echoes waterfall window - (" << as->getConfigName() << ")";
    wf->setWindowTitle(title);
    ui->lbConfig->setText( as->getConfigName() );

}

void MainWindow::moveEvent ( QMoveEvent * event )
{
    Q_UNUSED(event)
    MYINFO << "event->pos=" << event->pos() << " geometry()=" << geometry();
    if(ast != AST_INIT)
    {
        MYINFO << "geometry set";
        as->setMainGeometry( geometry() );
    }
}

void MainWindow::resizeEvent ( QResizeEvent* event )
{
    Q_UNUSED(event)  //avoids warning unused-parameter
    MYINFO << "event->pos=" << event->size() << " geometry()=" << geometry();

    if(ast != AST_INIT)
    {
        MYINFO << "geometry set";
        as->setMainGeometry( geometry() );

    }
}


void MainWindow::showBusy()
{
    qApp->setOverrideCursor(*busyCur);
    MY_ASSERT(busyInst >= 0)
    busyInst++;

    MYINFO << __func__ << "busyInst=" << busyInst;
    update();
}


void MainWindow::showFree(bool force)
{
    MYINFO << __func__  << "(" << force << ")";
    if (force)
    {
        busyInst = 1;
    }

    if(busyInst < 1)
    {
        MYINFO << __func__ << "return due to busyInst=" << busyInst;
        return;
    }

    busyInst--;

    if(busyInst == 0)
    {
        MYINFO << __func__ << " freeing cursor";
        qApp->setOverrideCursor(*freeCur);
        //qApp->processEvents();
        update();
    }

    MYINFO << __func__ << "end, busyInst=" << busyInst;
}

void MainWindow::slotSetSbThresholdsParam()
{

    //sets the thresholds spinboxes intervals
    //depending of the nature differential (S-N)
    //or relative (S) of these thresholds

    E_THRESHOLDS_BEHAVIOR tb = static_cast<E_THRESHOLDS_BEHAVIOR>( as->getThresholdsBehavior() );
    if(tb == TB_ABSOLUTE)
    {
        //absolute
        ui->sbUpThreshold->setMinimum(as->getMinDbfs());
        ui->sbUpThreshold->setMaximum(as->getMaxDbfs());


        ui->sbDnThreshold->setMinimum(as->getMinDbfs());
        ui->sbDnThreshold->setMaximum(as->getMaxDbfs());


    }
    else
    {
        //differential and automatic
        ui->sbUpThreshold->setMinimum(0);
        ui->sbUpThreshold->setMaximum(as->getMaxDbfs() - as->getMinDbfs());

        ui->sbDnThreshold->setMinimum(0);
        ui->sbDnThreshold->setMaximum(as->getMaxDbfs() - as->getMinDbfs());
    }
}


void MainWindow::slotSetScanMode( E_SCAN_INTEG si )
{
    as->setScanMode(si);

    switch(si)
    {

        default:
        case SCAN_LAST:
            MYINFO << "Selected no scans integration (last scan)";
            ui->rbLast->setChecked(true);
            break;

        case SCAN_AVG:
            MYINFO << "Selected integration by scans averaging";
            ui->rbAverage->setChecked(true);
            break;

        case SCAN_MAX:
            MYINFO << "Selected scans integration by max holding";
            ui->rbMax->setChecked(true);
            break;

    }
}

void MainWindow::slotSetAcqMode( E_ACQ_MODE am )
{
    as->setAcqMode(am);

    switch(am)
    {

        default:
        case AM_CONTINUOUS:
            MYINFO << "Selected continuous acquisition mode";
            ui->rbContinuous->setChecked(true);
            break;

        case AM_PERIODIC:
            MYINFO << "Selected periodic acquisition mode";
            ui->rbPeriodic->setChecked(true);
            break;

        case AM_AUTOMATIC:
            MYINFO << "Selected automatic acquisition mode";
            ui->rbAutomatic->setChecked(true);
            break;

    }
}

void MainWindow::slotSetDsBandwidth( const QString& text )
{
    int value = ui->cbDownsampler->findText( text, Qt::MatchFixedString );
    if(value == -1)
    {
        MYDEBUG << "string " << text << " not found in ui->cbDownsampler" << MY_ENDL;
    }
    else
    {
        QString valueString = ui->cbDownsampler->itemText(value);
        if(valueString.isEmpty())
        {
            MYDEBUG << "string " << valueString << " not found in ui->cbDownsampler" << MY_ENDL;
        }
        else
        {
            bool ok = false;
            int newValue = loc.toInt(valueString, &ok);
            if(ok)
            {
                if (as->setDsBandwidth( newValue , true ) == 0)
                {
                    MYWARNING << "failed setting dsBw " << newValue;
                }
                else
                {
                    //with direct buffers is needed to restart acquisition
                    //after such changes
                    /*
                    if(as->getDirectBuffers() == Qt::Checked && ac->isAcquisitionRunning())
                    {
                        emit acquisitionStatus(false);
                    }
                    */
                }
            }
        }
    }
    ui->cbDownsampler->setEnabled(as->getSsBypass() != Qt::Checked);
}


void MainWindow::slotIncResolution()
{
    if(as->incResolution())
    {
        calcDownsamplingFreqs();
    }

    //with direct buffers is needed to restart acquisition
    //after such changes
    /*
    if(as->getDirectBuffers() == Qt::Checked && ac->isAcquisitionRunning())
    {
        emit acquisitionStatus(false);
    }
    */
}


void MainWindow::slotDecResolution()
{
    if(as->decResolution())
    {
        calcDownsamplingFreqs();
    }

    //with direct buffers is needed to restart acquisition
    //after such changes
    /*
    if(as->getDirectBuffers() == Qt::Checked && ac->isAcquisitionRunning())
    {
        emit acquisitionStatus(false);
    }
    */
}

void MainWindow::localizeRanges( QStringList& sl )
{
    QStringList nsl;
    QList<QString> l;
    QString s, s1, s2, s3;
    qlonglong n0, n1;
    bool ok;
    foreach(s, sl)
    {
        l = s.split(":");
        if(l.length() > 1)
        {
            n0 = l.at(0).toLongLong(&ok);
            MY_ASSERT(ok == true)
                    n1 = l.at(1).toLongLong(&ok);
            MY_ASSERT(ok == true)
                    s1 = loc.toString(n0);
            s2 = loc.toString(n1);
            s3 = s1+":"+s2;
            nsl.append(s3);
        }
        else
        {
            n0 = l.at(0).toLongLong(&ok);
            MY_ASSERT(ok == true)
                    s1 = loc.toString(n0);
            nsl.append(s1);
        }
    }

    sl = nsl;
}


void MainWindow::localizeRange( QString& s )
{
    QList<QString> l;
    QString s1, s2, s3;
    qlonglong n0, n1;
    bool ok;

    l = s.split(":");
    if(l.length() > 1)
    {
        n0 = l.at(0).toLongLong(&ok);
        MY_ASSERT(ok == true)
                n1 = l.at(1).toLongLong(&ok);
        MY_ASSERT(ok == true)
                s1 = loc.toString(n0);
        s2 = loc.toString(n1);
        s3 = s1+":"+s2;
    }
    else
    {
        n0 = l.at(0).toLongLong(&ok);
        MY_ASSERT(ok == true)
                s3 = loc.toString(n0);
    }
    s = s3;
}


void MainWindow::delocalizeRange( QString& s )
{
    QList<QString> l;
    QString s1, s2, s3;
    qlonglong n0, n1;
    bool ok;

    l = s.split(":");
    if(l.length() > 1)
    {
        n0 = loc.toLongLong(l.at(0), &ok);
        MY_ASSERT(ok == true)
                n1 = loc.toLongLong(l.at(1), &ok);
        MY_ASSERT(ok == true)
                s1.setNum(n0);
        s2.setNum(n1);
        s3 = s1+":"+s2;
    }
    else
    {
        n0 = loc.toLongLong(l.at(0), &ok);
        MY_ASSERT(ok == true)
                s3.setNum(n0);
    }
    s = s3;
}
