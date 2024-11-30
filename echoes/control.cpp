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
$Rev:: 370                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-27 00:17:03 +01#$: Date of last commit
$Id: control.cpp 370 2018-11-26 23:17:03Z gmbertani $
*******************************************************************************/



#include "setup.h"
#include "settings.h"
#include "circbuf.h"
#include "radio.h"
#include "waterfall.h"
#include "postproc.h"
#include "mainwindow.h"
#include "control.h"



Control::Control(Settings* appSettings, XQDir wDir)
{
    setObjectName("Control object");

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    statPlot = nullptr;
    console = as->isConsoleMode();

    XQDir d = qApp->applicationDirPath();

    QString wav;

    pingSound = nullptr;
    faultSound = nullptr;

    if(as->getPing() == Qt::Checked)
    {
        wav = d.absoluteFilePath(NOTIFY_SOUND);
        pingSound = new QSound(wav);
        MY_ASSERT( nullptr !=  pingSound)
    }

    if(as->getFaultSound() == Qt::Checked)
    {
        wav = d.absoluteFilePath(FAULT_SOUND);
        faultSound = new QSound(wav);
        MY_ASSERT( nullptr !=  faultSound)
    }

    //frequency domain scans pool
    ps = new Pool<Scan*>;
    MY_ASSERT( nullptr !=  ps)


    dumpScans = 0;
    cbDAT = nullptr;
    cbCSV = nullptr;
    samplingTimer = nullptr;
    sShotCoverage = 0;

    //the file names are reinitialized each time "Start" is pressed
    sessionName = "";
    csvFileName = "";
    cmdFileName = "";
    dumpFileName = "";
    wfShotFileName = "";
    timeStamp = "";
    dateTimeStamp = "";

    am = AM_INVALID;
    loop = 0;
    scans = 1;
    stddev = 0;
    avgStddev = 0;
    autoCount = 0;
    manCount = 0;
    prog = 0;
    lastEventLasting = 0;
    tRange = 100;   //100%


    //loops countdown stopped
    waitingShots.clear();
    waitingDumps.clear();

    //acquisition stopped
    init = false;
    overload = true;
    newHdr = false;
    testMode = false;
    autoStopReq = false;
    waterfallChange = false;
    enableCapture = false;
    swapping = false;
    binDumps = false;
    updCfgReq = false;

    //other temporary vars
    debounceManShotReqs = 0;
    fromHz      = 0;
    toHz      = DEFAULT_RATE;
    zoomLo      = 0;
    zoomHi      = DEFAULT_RATE;
    rangeLo     = 0;
    rangeHi     = DEFAULT_RATE;
    centerFreq  = 0;
    maxFreq     = 0;
    FFTbins     = DEFAULT_FFT_BINS;
    loggedBins  = FFTbins;
    wfZoom      = DEFAULT_ZOOM;
    wfOffset    = DEFAULT_OFFSET;
    wfEccentricity = DEFAULT_ECCENTRICITY;
    wfTune      = DEFAULT_TUNE;
    hzShift     = 0;
    restart     = false;

    binSize     = DEFAULT_BIN_SIZE;
    avgDbfs     = 0;

    maxDbfs     = as->getMinDbfs();
    minDbfs     = as->getMaxDbfs();
    maxDiff     = 0.0;
    avgN        = as->getMinDbfs();
    avgS        = as->getMinDbfs();
    avgDiff     = 0;

    peakS       = 0;
    peakN       = 0;
    peakDiff    = 0;
    peakFreq    = 0;

    diffStart   = 0.0F;
    diffEnd     = 0.0F;

    autoUpThr = 0.0;
    autoDnThr = 0.0;

    totalTime = 0;
    scanTime = 0;
    avgScanTime = 0;
    scanDelta = 0;
    dBfsDelta = 0;
    csvDelta = 0;
    outDelta = 0;

    eventDetected = 0;
    wf     = nullptr;

    firstLoggedBin = 0;
    lastLoggedBin  = loggedBins - 1;

    scanIdxBak    = -1;

    //NaNs and infinites statistic counter
    nans        = 0;
    infinite    = 0;
    noDataBuf   = 0;

    //forces device reopening
    scc = 1;

    db = nullptr;

    //radio FFT manager
    r = new Radio(as, ps, wDir);

    wd = wDir;
    wd.makeAbsolute();
    updateRootDir();

    MY_ASSERT( nullptr !=  r)

    //data post processor
    pp = new PostProc( as, dbPath, rd );
    MY_ASSERT( nullptr !=  pp)



    //connects the available slots
    connect( r,                 SIGNAL( fault(int) ),           this, SLOT( slotStopAcquisition(int)), Qt::QueuedConnection );

    connect( as,                SIGNAL( notifySetInterval() ),  this, SLOT( slotSetInterval() ), Qt::QueuedConnection );
    connect( as,                SIGNAL( notifySetShots() ),     this, SLOT( slotSetShots() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetRecTime() ),   this, SLOT( slotSetRecTime() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetAfter() ),     this, SLOT( slotSetAfter() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetUpThreshold() ),       this, SLOT( slotSetUpThreshold() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetDnThreshold() ),       this, SLOT( slotSetDnThreshold() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetDetectionRange() ),    this, SLOT( slotSetDetectionRange() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySaved() ),              this, SLOT( slotUpdateConfiguration() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySavedAs() ),                this, SLOT( slotStopTimeChange() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifySetError() ),             this, SLOT( slotSetError() ), Qt::QueuedConnection  );
    connect( as,                SIGNAL( notifyOpened(const QString*) ), this, SLOT( slotStopTimeChange()), Qt::QueuedConnection  );

}

Control::~Control()
{
    r->quit();
    delete r;
    r = nullptr;
    delete cbCSV;
    cbCSV = nullptr;
    delete cbDAT;
    cbDAT = nullptr;
    delete ps;
    ps = nullptr;
    delete pingSound;
    pingSound = nullptr;
    delete faultSound;
    faultSound = nullptr;
    if(statPlot != nullptr)
    {
        statPlot->close();
        delete statPlot;
        statPlot = nullptr;
    }

    if(samplingTimer != nullptr)
    {
        delete samplingTimer;
        samplingTimer = nullptr;
    }

    if(db != nullptr)
    {
        delete db;
        db = nullptr;
    }
}



QString Control::getDateTime()
{
    QString s = as->currentUTCdateTime().toString(Qt::TextDate);
    return s;
}

QString Control::getDateTimeStamp()
{
    QString s = as->currentUTCdateTime().toString(Qt::ISODate);
    s.replace(':', '-');
    return s;
}


QString Control::getDateStamp()
{
    QString s = cal.date().toString(Qt::ISODate);
    return s;
}


//---slots----------------



void Control::slotAcquisition(bool go)
{
    QMutexLocker ml(&protectionMutex);

    //circular buffers
    //scans are the maximum number of records that can be stored
    //in cb without overwriting. Resulting DATs will be longer than
    //expected because the interval is only the GUI refreshing interval
    //but the samples could take more time to be extracted/decoded/written
    //on disk.

    dumpScans = 0;
    binDumps = !as->getDATdumps();

    if (cbDAT != nullptr)
    {
        delete cbDAT;
        cbDAT = nullptr;
    }

    if(cbDAT == nullptr)
    {
        cbDAT = new CircBuf(true, dumpScans, binDumps);
        MY_ASSERT( nullptr !=  cbDAT)
    }

    //statistic CSV is opened in append mode

    if(cbCSV == nullptr)
    {
        cbCSV = new CircBuf(false, dumpScans);
        MY_ASSERT( nullptr !=  cbCSV)
    }

    if(db == nullptr)
    {
       /*
         * When application is started, the DB is opened
         * again if not yet done, to have a DB connection
         * owned by Control thread and keep the main GUI thread
         * without DB involvements
         */
       MYINFO << "Reopening DB";
       db = new DBif(dbPath, as);
       MY_ASSERT(db != nullptr)
       if (updCfgReq)
       {
           slotUpdateConfiguration();
           updCfgReq = false;
       }
    }

    if(samplingTimer == nullptr)
    {

        MYINFO << "Create sampling timer";
        samplingTimer = new QTimer();
        if(samplingTimer != nullptr)
        {
            //posticipate the initialization in order to wait for main window coming up
            connect( samplingTimer, SIGNAL( timeout() ), this, SLOT( slotControlLoop() ), Qt::QueuedConnection );
            samplingTimer->setSingleShot(false);
            samplingTimer->setInterval( as->getInterval() );
            samplingTimer->setTimerType( Qt::PreciseTimer );
            //samplingTimer->moveToThread(this->thread());

        }
         MYINFO << "sampling timer created";
    }


    if(go == true)
    {
        init = true;
        restart = true;
        loop = 0;
        scans = 1;
        stddev = 0;
        noDataBuf = 0;
        infinite = 0;
        nans = 0;
        avgN = as->getMinDbfs();
        avgS = avgN;
        avgDbfs = avgN;
        Nfifo.clear();
        Sfifo.clear();
        avgDiff = 0;
        eventDetected = 0;
        autoStopReq = false;
        manStopReq = false;
        waterfallChange = false;
        autoCount = 0;
        manCount = 0;
        enableCapture = false;

        statClock.start();
        scanTimer.start();

        autoUpThr = as->getUpThreshold();
        autoDnThr = as->getDnThreshold();

        //initialize UTC calendar
        cal = as->currentUTCdateTime();

        //acquisition running since
        lastSessionCheck = as->currentUTCdateTime();
        db->sessionStart();

        if(console == false)
        {
            MY_ASSERT(wf != nullptr)
            //clean the WF before acquisition starts
            emit wfClean();
        }


        if(scc > 0)
        {
            if(r->isDeviceOpen() == false)
            {
                //restarting after daily swap
                r->slotSetDevice();
            }

            if( r->slotSetSampleRate() == true )
            {
                if( r->slotSetDsBandwidth() == true ) //note: this control is in FFT tab
                {
                    if(r->slotSetResolution() == true)
                    {
                        if(r->slotSetGain() == true)
                        {
                            if(r->slotSetError() == true)
                            {
                                if(r->slotSetTune() == true)
                                {
                                    scc = 0;
                                }
                            }
                        }
                    }

                }
            }
            if(scc > 0)
            {
                MYCRITICAL << "failed restart, scc=" << scc;
            }
        }

        // events delayers to take shots and dumps
        waitingShots.clear();
        waitingDumps.clear();

        //applying preferences:

        rd.refresh();
        if(as->getEraseLogs() != 0)
        {
            //erase old statistic data records
            QStringList files = rd.entryList();
            QString file;
            int goDelete;
            foreach (file, files)
            {
                goDelete = 0;

                goDelete += file.endsWith(TABLE_EXT, Qt::CaseInsensitive);
                if(goDelete == 0)
                {
                    continue;
                }

                file = rd.absoluteFilePath(file);
                if(rd.remove( file ) == false)
                {
                    MYWARNING << "cannot remove file " << file;
                }
            }

            //flushes the automatic_data table
            db->flushAutoData();

        }

        if(as->getEraseShots() != 0)
        {
            //erase old shots, feeds and plots
            QStringList files = rd.entryList();
            QString file;
            int goDelete;
            foreach (file, files)
            {
                goDelete = 0;

                goDelete += file.endsWith(DUMP_DATA_EXT, Qt::CaseInsensitive);
                goDelete += file.endsWith(GP_DATA_EXT, Qt::CaseInsensitive);
                goDelete += file.endsWith(SSHOT_EXT, Qt::CaseInsensitive);
                if(goDelete == 0)
                {
                    continue;
                }
                file = rd.absoluteFilePath(file);
                if( rd.remove(file) == false )
                {
                    MYWARNING << "cannot remove file " << file;
                }
            }

            db->flushShots();
            db->flushDumps();
        }

        //theorical number of scans stored in the given time
        dumpScans = static_cast<int>( ceil( static_cast<float>(as->getRecTime() * 1000) / as->getInterval() ) );
        cbDAT->setMax(dumpScans);

        //dataDumps.clear();

        //assigns output file names - the actual datetime must be embedded in filenames
        cmdFileName.clear();
        datFileName.clear();
        dumpFileName.clear();

        //determine the acquisition mode
        emit status( tr("Starting acquisition...") );
        am = static_cast<E_ACQ_MODE>( as->getAcqMode() );
        MYINFO << "am=" << am;
        switch( am )
        {
            case AM_CONTINUOUS:
                if ( as->getShots() == 0 && as->getUpThreshold() == 0.0F )
                {
                    //continuous mode
                    MYINFO << "*** Continuous acquisition mode (shots=0, upThreshold=0, recTime=0) ";

                    cbCSV->setMax(dumpScans);
                    makeSessName( getDateTimeStamp() );

                }
                else
                {
                    am = AM_INVALID;
                    faultStatus = tr("Please set: shots=0 and upThreshold=0");
                    MYINFO << "FAULT STATUS";
                }
                break;

            case AM_PERIODIC:
                if ( as->getShots() > 0 && as->getUpThreshold() == 0.0F )
                {
                    //periodic shots mode
                    MYINFO << "*** Periodic acquisition mode ( shots>0, upThreshold=0, recTime>0 )";
                    cbCSV->setMax(dumpScans);
                    makeSessName( getDateTimeStamp() );
                }
                else
                {
                    am = AM_INVALID;
                    faultStatus = tr("Please set: shots>0 and upThreshold=0");
                }
                break;

            case AM_AUTOMATIC:
                if ( as->getShots() > 0
                     && as->getDnThreshold() >= 0.0F
                     && as->getUpThreshold() > 0.0F  )
                {
                    //automatic events detection, uses database in place of cbCSV
                    MYINFO << "*** Automatic peaks detection mode ( shots>0, upThreshold>0, recTime>0 ) date stamp: " << getDateStamp();
                    makeSessName( getDateStamp() );
                }
                else
                {
                    am = AM_INVALID;
                    faultStatus = tr("Please set: shots>0 and upThreshold > 0 and dnTreshold >= 0");
                }
                break;

            default:
                break;
        }

        csvFileName = makeFileName(CSV_PREFIX, TABLE_EXT);

        if( as->getDumps() == Qt::Checked )
        {
            //dump files contain numerical coordinates suited for plotting
            //their structure is
            //similar to waterfall output, with time on Y
            //frequency in X and the dBfs signal represented with
            //a color scale. At the end of each horizontal scan
            //the S, N and S-N values are stored.

            //If shots or threshold are set, a new output
            //data file will be created each time a new event
            //is catched.


            //continuous dump data file
            //should never be loaded in DB
            datFileName =  makeFileName(DATA_PREFIX, GP_DATA_EXT);

            //The data file is marked with
            //the same datetime and progressive number of the
            //relative screenshot.
            if(binDumps)
            {
                dumpFileName =  makeFileName(DUMP_PREFIX, DUMP_DATA_EXT);
            }
            else
            {
                dumpFileName =  makeFileName(DUMP_PREFIX, GP_DATA_EXT);
            }

            //the shot number will be appended later, with extension
        }

        MYINFO << "Starting acquisition." ;
        MYINFO << "One read every " << as->getInterval() << " ms." ;
        samplingTimer->setInterval( as->getInterval() );
        samplingTimer->start();


        if(r->isRunning())
        {
            MYWARNING << "Cannot start radio thread: it's still running";
        }

        r->start();

        emit notifyStart();

        if(console == false)
        {
            MainWindow* mw = as->getMainWindow();
            MY_ASSERT( nullptr !=  mw)
            connect(pp, SIGNAL(status(QString)), mw, SLOT(slotShowStatus(QString)), Qt::QueuedConnection);
            connect(mw, SIGNAL(acquisitionAbort(int)), this, SLOT(slotStopAcquisition(int)), Qt::QueuedConnection);
        }

        //change working directory to selected one
        bool ok = XQDir::setCurrent( rd.currentPath() );
        MY_ASSERT(ok == true)

        MY_ASSERT( csvFileName.isEmpty() == false )
        QString csvAbsPath = rd.absoluteFilePath( csvFileName );

        //the shots acquisition should continue if
        //the previous session was not completed
        //(shots taken less than expected) and the
        //date didn't change

        int result = 0;
        if( cal.date() == db->getLastEventDate() )
        {
            result += 1;
        }
        if( db->getLastDailyNr() < as->getShots() )
        {
            result += 2;
        }
        if( QFile::exists(csvAbsPath) == true || am == AM_AUTOMATIC )
        {
            result += 4;
        }
        if( as->getEraseLogs() == Qt::Unchecked && as->getEraseShots() == Qt::Unchecked )
        {
            result += 8;
        }
        MYINFO << "append/restart check = " << result;
        if(result == 0x0f)
        {
            prog = static_cast<uint>( db->getLastDailyNr() );        //continues from the last event known (for automatic mode only)
            autoCount = static_cast<int>(prog);
            MYINFO << "Events progressive initialized to #" << prog;
            result = 0;
            if(am != AM_AUTOMATIC)
            {
                MYINFO << " statistic CSV opened in append";
                result = cbCSV->setFile( csvAbsPath, true );
            }
        }
        else
        {
            prog = 0;
            autoCount = 0;
            result = 0;

            //CSV statistic file not needed in automatic mode
            if(am != AM_AUTOMATIC)
            {
                newHdr = true;
                MYINFO << "Events progressive zeroed, creating a new statistic CSV";

                //creates a new CSV file
                cbCSV->unSetFile();
                result = cbCSV->setFile( csvAbsPath );
            }
        }
        MY_ASSERT(result == 0)


        //CSV column header
        csvFields.clear();
        csvFields.append( tr("Event nr.") );                        //field #1
        csvFields.append( tr("Date") );                             //field #2
        csvFields.append( tr("UTC Time") );                         //field #3
        csvFields.append( tr("Unix Timestamp [ms]") );              //field #4
        csvFields.append( tr("Tune") );                             //field #5 *
        csvFields.append( tr("Downsampled bw") );                   //field #6 *
        csvFields.append( tr("Central Hz") );                       //field #7 *
        csvFields.append( tr("Lowest Hz") );                        //field #8
        csvFields.append( tr("Highest Hz") );                       //field #9
        csvFields.append( tr("zoomed BW Hz") );                     //field #10
        csvFields.append( tr("step Hz") );                          //field #11
        csvFields.append( tr("Threshold mode") );                   //field #12
        csvFields.append( tr("Up threshold [dBfs]") );              //field #15
        csvFields.append( tr("Dn threshold [dBfs]") );              //field #16
        csvFields.append( tr("Auto up threshold %") );              //field #13
        csvFields.append( tr("Auto dn threshold %") );              //field #14
        csvFields.append( tr("Range low Hz") );                     //field #17
        csvFields.append( tr("Range high Hz") );                    //field #18
        csvFields.append( tr("Top peak (S) [dBfs]") );              //field #19
        csvFields.append( tr("Average peak [dBfs]") );              //field #20
        csvFields.append( tr("Average noise (N) [dBfs]") );         //field #21
        csvFields.append( tr("Difference (S-N) [dBfs]") );          //field #22
        csvFields.append( tr("Average difference (S-N) [dBfs]") );  //field #23
        csvFields.append( tr("Top peak[Hz]") );                     //field #24

        emit setShotCounters( static_cast<int>(prog), as->getShots() );
    }
    else
    {
        MYINFO << "manual acquisition stop request" ;
        if(eventDetected)
        {
            emit status( QString(tr("Acquisition will stop in some seconds, be patient...")));
        }
        else
        {
            emit status( tr("Stopping acquisition...") );
        }

        emit GUIbusy();
        manStopReq = true;
    }
}


void Control::slotControlLoop()
{
    bool peak = false;

    scanDelta = 0;
    dBfsDelta = 0;
    csvDelta = 0;
    outDelta = 0;
    deadTime = statClock.restart();
    MYDEBUG << "loop: " << loop;

    //the scanning lifesignal goes direcly to stdout
    if(logLevel < ECHOES_LOG_INFO)
    {
        switch( loop % 8 )
        {
        case 0:
        case 1:
            printf("\r\\");
            break;

        case 2:
        case 3:
            printf("\r|");
            break;

        case 4:
        case 5:
            printf("\r/");
            break;

        case 6:
        case 7:
            printf("\r-");
            break;

        }
        fflush(stdout);
    }
    debounceManShotReqs -= (debounceManShotReqs > 0)? 1 : 0;

   if(
      as->fpCmpf(binSize, as->getResolution(), 5) ||
      wfZoom != as->getZoom() ||
      wfBw != as->getDsBandwidth() ||
      wfOffset != as->getOffset() ||
      wfTune != as->getTune() ||
      wfEccentricity != as->getEccentricity() ||
      waterfallChange == true )
    {
        restart = true;
    }

    peak = wfdBfs();

    if (!r->isRunning())
    {
        //In case wfDbfs() detected a radio fault and stopped the sampling timer,
        //must return immediately.
        MYINFO << "acquisition loop aborted";
        return;
    }

    EvTuple et;

    if( r->isFinished() && r->isReady() )
    {
        MYINFO << "Acquisition stopping request due to radio thread termination" ;
        autoStopReq = true;
        slotStopAcquisition(ASC_RADIO_FAULT_DETECTED);
    }
    else
    {
        switch (am)
        {
            case AM_CONTINUOUS:
                if (init == true)
                {
                    //forgets about the last progressive
                    if(prog > 0)
                    {
                        prog = 0;
                        autoCount = 0;
                    }

                    if( as->getDumps() == Qt::Checked )
                    {
                        cbCSV->unSetFile();
                        QString csvAbsPath = rd.absoluteFilePath( csvFileName );
                        cbCSV->setFile( csvAbsPath );
                        cbCSV->setKeepMode(false);
                    }

                    //continuous acquisition (no shots) each scan line is immediately
                    //dumped to file
                    dumpScans = 1;
                }

                cbCSV->dump();

                //continuous mode can be stopped only manually or from server
                if(manStopReq == true)
                {
                    slotStopAcquisition(ASC_MANUAL_REQ);
                }
                break;

            case AM_PERIODIC:
                //produces screenshots and/or dumps at regular intervals, that is the shorter
                //between the screenshot coverage and dump coverage
                if (init == true)
                {
                    //Starts the countdown to next shot
                    if( waitingDumps.count() == 0 )
                    {
                        prog = 0;   //forgets about last event number, these are scans now
                        autoCount = 0;
                        QTime now = as->currentUTCtime();
                        et.scan = scans;
                        et.timeMs = now.msecsSinceStartOfDay();
                        et.prog = static_cast<int>(prog);
                        waitingDumps.enqueue( et );
                        MYINFO << "init ==> Enqueued et.prog=" << et.prog << " et.timeMs=" << et.timeMs;
                        cbCSV->setKeepMode(true);
                    }

                }

                if( waitingDumps.count() > 0  )
                {
                    //please take in count that dumps coverage can be longer
                    //than screenshot coverage, so if both are enabled,
                    //in the screenshots could result time holes because
                    //the time between shots is the same of dumps.
                    //For this reason, only the dumps queue is used here:
                    et = waitingDumps.head();
                    int delayMs = (as->getRecTime() * 1000);
                    QTime now =  as->currentUTCtime();

                    int elapsedMs = now.msecsSinceStartOfDay();
                    elapsedMs -= et.timeMs;
                    MYDEBUG << "Period ms: " << delayMs << " now: " << now << " elapsedMs: " << elapsedMs;
                    int msToShot = static_cast<int>(delayMs - elapsedMs);
                    MYDEBUG << "next shot in: " << msToShot  << " ms";

                    if( msToShot <= 0 )
                    {
                        if( autoCount < as->getShots() && swapping == false)
                        {
                            if(init == false)   //skips the first empty shot
                            {
                                autoCount++;
                                if( as->getScreenshots() == Qt::Checked )
                                {
                                    slotTakeWfAutoShot();
                                }

                                //never produces dump files in periodic mode

                            }
                            //the prog counter in this case does not count events but scans
                            if(console == false)
                            {
                                cbCSV->dump(wfShotFileName);
                            }
                            else
                            {
                                cbCSV->dump(dumpFileName);
                            }
                            MYINFO << "Triggering time reached, taking shot#" << autoCount << " of "  << as->getShots();

                            et = waitingDumps.dequeue();
                            MYINFO << "<== Dequeued et.prog=" << et.prog << " et.timeMs=" << et.timeMs;

                            QTime now = as->currentUTCtime();
                            et.prog = autoCount+1;
                            et.scan = scans;
                            et.timeMs = now.msecsSinceStartOfDay();
                            waitingDumps.enqueue( et );
                            MYINFO << "==> Enqueued et.prog=" << et.prog << " et.timeMs=" << et.timeMs;
                        }
                        else
                        {
                            waitingDumps.dequeue();
                            MYINFO << "Stopping acquisition, max shots/dumps exceeded: (" << as->getShots() << ")." ;
                            autoStopReq = true;
                        }
                    }
                }



                //periodic mode can be stopped in any moment
                if(autoStopReq)
                {
                    slotStopAcquisition(ASC_SHOTS_FINISHED);
                }
                else if(manStopReq)
                {
                    slotStopAcquisition(ASC_MANUAL_REQ);
                }
                break;

            case AM_AUTOMATIC:
                if (init == true)
                {
                    if( as->getDumps() == Qt::Checked )
                    {
                        cbDAT->setKeepMode(true);
                    }

                    //initialization terminates when measureScanTime() has been called the first time
                }

                if( autoStopReq == false &&
                    prog > static_cast<uint>( as->getShots() ) )
                {
                    MYINFO << "Acquisition stopping request due to  max shots exceeded: (" << as->getShots() << ")." ;
                    autoStopReq = true;
                }

                if( peak == true &&
                    autoStopReq == false &&
                    manStopReq == false )
                {
                    //new event detected (raising edge)

                    //Starts the countdown
                    //with afterPeak == 0 the trigger will be immediate
                    QTime now = as->currentUTCtime();
                    et.scan = scans;
                    et.timeMs = now.msecsSinceStartOfDay();
                    et.prog = static_cast<int>(prog);
                    waitingShots.enqueue( et );
                    waitingDumps.enqueue( et );
                    MYINFO << "Event #" << prog << " detected at scan #" << scans << " pushing countdown" ;
                    MYINFO << "Now there are still " << waitingShots.count() << " events waiting a shot" ;
                    MYINFO << "and " << waitingDumps.count() << " events waiting a dump" ;
                }

                //remember: "shots" means screenshots png while "dump" means a spectral data file for plotting.

                if( waitingShots.count() > 0  && console == false )
                {
                    MYDEBUG << "events waiting for shot: " << waitingShots.count();

                    et = waitingShots.head();
                    int wfScans = wf->scansNumber();
                    int delayScans = (as->getAfter() * wfScans) / 100;
                    int leftScans = delayScans - (scans - et.scan);
                    MYDEBUG << "left scans left before shot: " << leftScans;

                    //waits left scans expiring before triggering the shot
                    //the as->getAfter() gives a percentage. For shots, it means
                    //percentage of the waterfall,
                    // 0% = shot immediately when the event is detected
                    // 100% = shot when the peak reaches the last visible row in waterfall
                    if( leftScans <= 0 && swapping == false)
                    {
                        //shot time!
                        if( et.prog <= as->getShots() )
                        {
                            waitingShots.dequeue();

                            autoCount = et.prog;
                            MYINFO << "Countdown expired, triggering shot#" << autoCount << " of "  << as->getShots() ;
                            if( as->getScreenshots() == Qt::Checked )
                            {
                                slotTakeWfAutoShot();
                            }
                        }
                    }
                }

                if( waitingDumps.count() > 0  )
                {
                    MYDEBUG << "events waiting for dump: " << waitingDumps.count();

                    et = waitingDumps.head();

                    //calculates how many scans are in rec time
                    int dumpScans = (as->getRecTime() * 1000) / as->getInterval();

                    //the as->getAfter() gives a percentage. For dumps, it means percentage of the rec time.
                    int delayScans = (as->getAfter() * dumpScans) / 100;
                    int leftScans = delayScans - (scans - et.scan);
                    MYDEBUG << "left scans left before dump: " << leftScans;


                    //waits left scans expiring AND event closed before triggering the dump
                    if(leftScans < 0 && eventDetected == 0 )
                    {
                        //dump time!
                        if( et.prog <= as->getShots() && swapping == false)
                        {
                            waitingDumps.dequeue();

                            autoCount = et.prog;
                            MYINFO << "Countdown expired, triggering dump#" << autoCount << " of "  << as->getShots() ;
                            if( as->getDumps() == Qt::Checked )
                            {
                                slotTakeDump();
                            }
                        }
                    }
                }

                //in automatic mode, the acquisition
                //waits until the pending events are closed and all the data
                //rows stored in circbufs
                //have been written on disk before stopping.

                if(autoStopReq == true || manStopReq == true)
                {

                    //wait for pending events closing

                    if( waitingShots.count() == 0 && waitingDumps.count() == 0 && eventDetected == 0 )
                    {
                        emit status( tr("Stopping acquisition...") );
                        MYINFO << "stopping radio thread to skip further events detections";
                        r->stop();
                        if(autoStopReq)
                        {
                            slotStopAcquisition(ASC_SHOTS_FINISHED);
                        }
                        else if (manStopReq)
                        {
                            slotStopAcquisition(ASC_MANUAL_REQ);
                        }
                    }


                }
                break;

            default:                
                MYWARNING << "Wrong parameters combination for required configuration, please fix and restart acquisition, am=" << am;
                if(samplingTimer->isActive())
                {
                    autoStopReq = true;
                    slotStopAcquisition(ASC_GUI_WRONG_SETTING);
                    emit status( faultStatus );
                }
                //retriggers MainWindow::slotSetAcqMode()
                am = static_cast<E_ACQ_MODE>( as->getAcqMode() );
                as->setAcqMode(am);
                break;

        }

        //checks for swap time (by default midnight UTC): when it comes,
        //closes the csv files
        //then opens two brand new ones

        if(
            as->isSwapTime() &&
            swapping == false &&
            manStopReq == false &&
            eventDetected == 0 &&
            autoStopReq == false &&
            pp->isRunning() == false)
        {
            swapping = true;
            emit GUIbusy();

            MYINFO << "*********** SWAP TIME UTC ******************";

            //there must be no files opened while archiving

            pp->setWdir(rd);
            pp->start( QThread::IdlePriority );
            while (pp->isRunning() == false)
            {
                QThread::msleep(100);
            }

            //reset progressive counter
            MYINFO << "automatic progressive number zeroed (was " << prog << ")";
            prog = 0;
            autoCount = 0;

            MYINFO << "updating session name, date stamp: " << getDateStamp();
            makeSessName( getDateStamp() );
            emit GUIfree();

            MYINFO << "************* daily swapping launched ****************";
        }

        //ensures swapping only once, if it lasts less than one minute
        if(swapping == true && as->isSwapTime() == false)
        {
            swapping = false;
        }
    }

    if(autoStopReq == false)
    {
        if( totalTime > as->getInterval() && overload == false)
        {
            MYDEBUG << "last scan took " << totalTime
                << " ms with interval set to " << as->getInterval() << " ms";

            overload = true;
        }
        else if(init == true && manStopReq == false && loop == 0)
        {
            //run only once after start acquisition
            overload = false;
            switch(am)
            {
            case AM_CONTINUOUS:
            default:
                emit status( tr("Acquisition running continuous mode") );
                break;

            case AM_PERIODIC:
                emit status( tr("Acquisition running periodic mode") );
                break;

            case AM_AUTOMATIC:
                emit status( tr("Acquisition running automatic mode") );
                break;
            }

        }

    }

    loop++;

    printStats();

    if((autoStopReq||manStopReq) &&
            samplingTimer->isActive() == false)
    {
        autoStopReq = false;
        manStopReq = false;
    }


    totalTime = deadTime + scanDelta + dBfsDelta  + csvDelta + outDelta;
    if(scanTime > 0)
    {
        avgScanTime = (scanTime+avgScanTime) / 2;
        scanTime = 0;
    }

    measureScanTime();

}


void Control::slotStopAcquisition(int what)
{
    QMutexLocker ml(&protectionMutex);

    if(samplingTimer != nullptr &&  samplingTimer->isActive())
    {
        bool acqFault = false;
        E_SESS_END_CAUSE cause = SEC_NONE;

        switch(what)
        {
        case ASC_MANUAL_REQ:
            MYINFO << "Acquisition stop requested by user" ;
            if(console)
            {
                as->setResetCounter(true);
                as->saveLocal();
            }

            cause = SEC_MANUAL_REQ;
            break;

        case ASC_SHOTS_FINISHED:
            MYINFO << "Acquisition stop due to shots counter zeroed" ;
            cause = SEC_SHOTS_FINISHED;
            break;

        case ASC_GUI_WRONG_SETTING:
            MYINFO << "Acquisition stop due to settings incompatible with the selected operating mode" ;
            cause = SEC_FAULT;
            break;

        case ASC_RADIO_FAULT_DETECTED:
            MYCRITICAL << "Acquisition stop due to error detected on data coming from radio" ;
            cause = SEC_FAULT;
            autoStopReq = true;
            acqFault = true;
            audioNotify(faultSound);
            break;

        case ASC_KILLED_BY_SIGNAL:
            MYCRITICAL << "Acquisition stop due to exception" ;
            cause = SEC_FAULT;
            break;

        default:
            MYCRITICAL << "Acquisition stop for unknown reason" ;
            cause = SEC_FAULT;
            break;
        }



#ifdef _DEBUG
        printStats();
#endif

        emit status( tr("Stopping acquisition...") );

        samplingTimer->stop();
        waitingShots.clear();
        waitingDumps.clear();

        db->executePendingQueries();

        MYINFO << "Closing circular buffer files";
        cbCSV->unSetFile();
        cbDAT->unSetFile();
        pp->stop();

        //stops radio acquisition (or test pattern generator)
        //if not yet stopped
        if (r->isRunning() == true)
        {
            r->stop();
        }


        if(autoStopReq || !acqFault)
        {
            MYINFO << "Notifies stop";

            //resets the Start pushbutton
            emit notifyStop();
        }
        else
        {
            emit status( tr("Acquisition FAULT") );
        }
        emit GUIfree();

        db->sessionEnd(cause);

        //forces device reopening
        scc = 1;
    }
}



QString Control::makeFileName(const QString& prefix, const QString& ext, uint progId)
{
    QString fileName;

    if(progId == 0)
    {
        fileName = QString("%1%2%3")
                .arg(prefix)
                .arg(sessionName)
                .arg(ext);

    }
    else
    {
        fileName = QString("%1%2_%3%4")
                .arg(prefix)
                .arg(sessionName)
                .arg(progId, 5, 10, QChar('0'))
                .arg(ext);

    }

    return fileName;
}


void Control::makeSessName(QString dateTime)
{
    QString ams;
    switch(am)
    {
    case AM_CONTINUOUS:
    default:
        ams = tr("continuous") ;
        break;

    case AM_PERIODIC:
        ams = tr("periodic") ;
        break;

    case AM_AUTOMATIC:
        ams = tr("automatic") ;
        break;
    }

    sessionName = QString("%1_%2_%3")
            .arg( as->getConfigName() )
            .arg( ams )
            .arg( dateTime );
}

void Control::slotSetInterval()
{
    MYDEBUG << "forwarding to slotSetRecTime()...";
    slotSetRecTime();
    slotStopTimeChange();
}


void Control::slotSetShots()
{
    slotStopTimeChange();
}


void Control::slotSetRecTime()
{
    MYDEBUG << "scans = " << dumpScans << " calling slotFFTchanged()...";

    //slotFFTchanged();
    slotStopTimeChange();
}


void Control::slotSetAfter()
{
    slotStopTimeChange();
}


void Control::slotSetUpThreshold()
{
    slotStopTimeChange();
}

void Control::slotSetDnThreshold()
{
    slotStopTimeChange();
}

void Control::slotSetDetectionRange()
{
    tRange = as->getDetectionRange();
    slotStopTimeChange();
}





void Control::slotTakeWfAutoShot()
{
    QMutexLocker ml(&protectionMutex);

    if(console == false && wf != nullptr)
    {
        wfShotFileName.clear();
        wfShotFileName = makeFileName(AUTOSHOT_PREFIX, SSHOT_EXT, static_cast<uint>(autoCount));
        emit wfTakeShot( autoCount, as->getShots(),  peakS, peakN, avgDiff,
                         autoUpThr, autoDnThr, rd.absoluteFilePath(wfShotFileName) );
        emit status( tr("Automatic shot taken") );
        audioNotify(pingSound);
    }
}

void Control::slotTakeWfManShot()
{
    QMutexLocker ml(&protectionMutex);

    if( debounceManShotReqs == 0 )
    {
        if(console == false && wf != nullptr)
        {
            audioNotify(pingSound);

            QString wfManShotFileName;

            manCount++;
            wfManShotFileName = makeFileName(MANSHOT_PREFIX, SSHOT_EXT, static_cast<uint>(manCount));
            emit wfTakeShot( manCount, 0,  0, 0, 0, 0, 0, rd.absoluteFilePath(wfManShotFileName) );
            emit status( tr("Manual shot taken") );
        }

        debounceManShotReqs = MANSHOT_DEBOUNCE_TICKS;
    }


}


void Control::audioNotify(QSound* sound)
{
    if (sound == pingSound)
    {
        if(as->getPing() != Qt::Checked)
        {
            //ping sound disabled
            return;
        }
    }

    if (sound == faultSound)
    {
        if(as->getFaultSound() != Qt::Checked)
        {
            //fault sound disabled
            return;
        }
    }


    if(  QAudioDeviceInfo::availableDevices(QAudio::AudioOutput).isEmpty() == true )
    {
        //alternative commands to play a wav file if Qt reference sound system is not present
        //this is a linux problem, since it presents a wide choice of audio drivers

        //It should not happen, except in case the Qt libraries have been built
        //with sound system disabled

        QString wav = sound->fileName();

#if WANT_PULSEAUDIO
        MYWARNING << "Default sound system not available, trying alternatives";

        QStringList sl;
        sl.append(SOUNDS_PATH);
        sl.append(wav);
        QProcess::startDetached( "/usr/bin/paplay", sl );

#elif WANT_SOX
        MYWARNING << "Default sound system not available, trying alternatives";
        wav = QString("/usr/bin/play %1/%2").arg(SOUNDS_PATH).arg(wav);
        QProcess::startDetached( wav );
#else
        MYDEBUG << "silently playing " << wav;
#endif

    }
    else
    {
        sound->play();
    }
}

void Control::slotTakeDump()
{
    //completes the dump filename
    //with the autoshot number then dumps the data
    //on disk

    QMutexLocker ml(&protectionMutex);

    dumpFileName.clear();
    if(binDumps)
    {
        dumpFileName = makeFileName(DUMP_PREFIX, DUMP_DATA_EXT, static_cast<uint>(autoCount));
    }
    else
    {
        dumpFileName = makeFileName(DUMP_PREFIX, GP_DATA_EXT, static_cast<uint>(autoCount));
    }


    QString absPath = rd.absoluteFilePath( dumpFileName );

    cbDAT->unSetFile();

    if(binDumps)
    {
        //binary dump header:
        DumpHeader dh;

        //the dump format version 2 includes the thresholds data
        memcpy(&dh.startMarker, "DMP2", sizeof(dh.startMarker));
        dh.resolution = as->getResolution();
        dh.startHz = zoomLo;
        dh.points =  lastLoggedBin - firstLoggedBin + 1;
        dh.isAudio = as->isAudioDevice();

        QByteArray ba((char*)&dh, sizeof(dh));

        cbDAT->setFile( absPath, ba );

        int written = cbDAT->bdump();
        if (written == 0)
        {
            MYINFO << "bdump() failed";
        }

    }
    else
    {
        cbDAT->setFile( absPath );
        cbDAT->dump();
    }


    //updates the counter counting the dumps instead of the shots
    //when shots are not wanted
    if( as->getScreenshots() != Qt::Checked )
    {
        emit wfUpdateCounter(autoCount, as->getShots());
    }
    MYINFO << "Created dump file: " << dumpFileName;


}

void Control::slotStopTimeChange()
{
    //configuration change occurred while acquisition was stopped
    autoCount = 0;          //shots counter are reset
    manCount = 0;           //shots counter are reset
    scc++;
    if(console == false && wf != nullptr)
    {
        emit wfUpdateGrid();
    }
    MYDEBUG << "scc = " << scc;
}


void Control::measureScanTime()
{
    bool initDone = false;

    MYDEBUG << "init=" << init;
    if(!console)
    {
        /*
         * The screenshot number of scans is fixed because it matches the waterfall widget's height.
         * is the time coverage that needs to be calculated at runtime (unless we're in console mode)
         */

        //required number of scans displayed in the waterfall
        //(waterfall's widget height)
        int wfScans = wf->scansNumber();
        if((scans % wfScans) == 0)
        {
            sShotCoverage = (wfScans * as->getInterval()) ;
            MYDEBUG << "scans=" << scans << " wfScans=" << wfScans;
            MYDEBUG << "Required screenshots coverage: " << sShotCoverage << " ms for " << wfScans << " scans";

            cal.fromString( dateTimeStamp );
            sShotCoverage = static_cast<int>( calOld.msecsTo(cal) );
            MYDEBUG << "Effective screenshot coverage: " << cal << " - " << calOld << " = " << sShotCoverage << " ms";
            calOld = cal;
            initDone = true;
        }
    }

    if(as->getDumps() == Qt::Checked && init == true)
    {
        /*
         * The dumps duration is set in mainwindow as parameter, but the number of scans
         * that a dump can store depends of the acquisition speed, that is dynamic and is
         * calculated here once, each time the acquisition starts
         */


        initDone = false;

        //required number of scans stored in a shot
        int requiredDumpScans = static_cast<int>( ceil( static_cast<float>(as->getRecTime() * 1000) / as->getInterval() ) );
        MYDEBUG << "Required dump coverage (shot duration) in seconds=" << as->getRecTime() << " for " << dumpScans << " scans";

        if(static_cast<int>(scans) > requiredDumpScans)
        {
            //effective number of scans stored in a shot, must wait at least the required
            //scans before calculation, to allow the average scan time to stabilize
            dumpScans = static_cast<int>( ceil( static_cast<float>(as->getRecTime() * 1000) / avgScanTime ) );

            MYINFO << "Effective scans per dump: " << dumpScans;

            cbDAT->setMax(dumpScans);
            if(am != AM_AUTOMATIC)
            {
                //in automatic mode the statistic CSV stores events data, not scans data.
                cbCSV->setMax(dumpScans);
            }

            initDone = true;
        }
    }

    if(init  && initDone)
    {
        init = false;
        MYINFO << "Initialization terminated at scan#" << scans;
        joinTimer.start();
        slotInitParams();
    }

    if(!init)
    {
        QDateTime now = as->currentUTCdateTime();
        if(lastSessionCheck.secsTo(now) > SESS_UPD_CADENCE)
        {
            lastSessionCheck = now;
            db->sessionUpdate();
        }
    }
}

bool Control::slotSetError()
{

    float err = as->getError();
    float fracErr = 0.0;
    float fracPpmToHz = 0.0;
    float intErr = 0.0; //not used

    //the fractional part of ppm error is used to apply a transparent offset
    //to the waterfall, in order to correct the frequency more accurately
    //than dongle does
    fracErr = modf(err, &intErr);

    //converts the fractional ppm error to Hz, intErr is not used
    fracPpmToHz = (as->getTune() / 1000000) * fracErr;

    //recalculate the band's start and the end frequencies
    int rawFromHz = r->getLowBound();
    fromHz = static_cast<int>(rawFromHz+fracPpmToHz);

    int rawToHz = r->getHighBound();
    toHz = static_cast<int>(rawToHz+fracPpmToHz);

    MYINFO << "raw: fromHz=" << rawFromHz << "  toHz=" << rawToHz;
    MYINFO << "fracErr=" << fracErr << " fracPpmToHz=" << fracPpmToHz;
    MYINFO << "corrected: fromHz=" << fromHz << " toHz=" << toHz;


    //peak detection range (tRange) from % to Hz:
    as->getZoomedRange( zoomLo, zoomHi );
    int interval = 0;

    int zoomRange = (zoomHi - zoomLo);
    interval = (zoomRange * as->getDetectionRange()) / 100;

    MYINFO << "zoomLo=" << zoomLo << " zoomHi=" << zoomHi << " zoomRange=" << zoomRange;

    rangeLo = zoomLo + (zoomRange / 2) - (interval / 2);
    rangeHi = rangeLo + interval;
    MYINFO << "range " << as->getDetectionRange() << " % = " << rangeLo << "..." << rangeHi << " = " << interval << " Hz ";


    //zoom is in _tenths_ of hz
    int binsInZoom = static_cast<int>( ((10*wfBw) / wfZoom) / binSize );

    //when scan starts or any upper slider changes on waterfall
    //the loggedbins and firstloggedbins are recalculated
    if(as->isAudioDevice())
    {
        firstLoggedBin = static_cast<int>( zoomLo / binSize ) * 2;
    }
    else
    {
        firstLoggedBin = static_cast<int>( (zoomLo - fromHz) / binSize );
    }

    if(firstLoggedBin < 0)
    {
        firstLoggedBin = 0;
    }

    lastLoggedBin = firstLoggedBin+binsInZoom-1;


    MYINFO << "binsInZoom=" << binsInZoom << " firstLoggedBin=" << firstLoggedBin
           << " lastLoggedBin=" << lastLoggedBin
           << " loggedBins=" << loggedBins;

    return true;
}



//---- end slots ---------------------

void Control::slotInitParams()
{


    MYINFO << __func__ << " initializing variables";
    //runtime change

    wfZoom = as->getZoom();
    wfBw = as->getDsBandwidth();
    wfOffset = as->getOffset();
    wfTune = as->getTune();
    wfEccentricity = as->getEccentricity();
    binSize = as->getResolution();
    loggedBins =  static_cast<int>(wfBw / binSize);


    //retrieves the waterfall window pointer
    if (console == false && wf == nullptr)
    {
        MYINFO << "connecting Control's signals";

        wf = as->getWaterfallWindow();
        connect( wf,    SIGNAL(waterfallResized()),                         this,   SLOT( slotWaterfallResized() ),                     Qt::QueuedConnection );
        connect( this,  SIGNAL(wfClean()),                                  wf,     SLOT( slotCleanAll() ),                             Qt::QueuedConnection );
        connect( this,  SIGNAL(wfUpdateGrid()),                             wf,     SLOT( slotUpdateGrid() ),                           Qt::QueuedConnection );
        connect( this,  SIGNAL(wfSetCapturing(bool, bool)),                 wf,     SLOT( slotSetCapturing(bool, bool) ),               Qt::QueuedConnection );
        connect( this,  SIGNAL(wfSetScreenshotCoverage(int)),               wf,     SLOT( slotSetScreenshotCoverage(int) ),             Qt::QueuedConnection );

        connect( this,  SIGNAL(wfUpdateCounter(int, int)),
                 wf,     SLOT( slotUpdateCounter(int, int)),Qt::QueuedConnection );

        connect( this,  SIGNAL(wfRefresh()),                                wf,     SLOT( slotRefresh() ),   Qt::QueuedConnection );

        connect( this,  SIGNAL(wfTakeShot(int, int, float, float, float, float, float, QString)),
                 wf,     SLOT( slotTakeShot(int, int, float, float, float, float, float, QString )),Qt::QueuedConnection );


    }

    //applies the error correction's fractional part
    slotSetError();

    eventDetected = 0;

    //note: the central frequency of the waterfall and the central frequency of the peak detection interval
    //are now two different things. What really matters here is the second one, that will be called
    //centerFreq even if it's no more sticky at the waterfall's centre.

    if(as->isAudioDevice())
    {
        centerFreq = (zoomHi - zoomLo) / 2;
    }
    else
    {
        centerFreq = wfTune + wfOffset + wfEccentricity;
    }

    rangeLo += wfEccentricity;
    rangeHi += wfEccentricity;

    if(console == false && wf != nullptr)
    {
        emit wfUpdateGrid();
        wf->appendInfos(timeStamp, maxDbfs, avgN, avgDiff, autoUpThr, autoDnThr, true);
        MYDEBUG << "calling slotRefresh";
        emit wfRefresh();
        MYDEBUG << "slotRefresh called";
    }

    if( as->getThresholdsBehavior() == TB_AUTOMATIC )
    {
        autoDnThr = avgDiff + as->getDnThreshold();
        autoUpThr = autoDnThr + as->getUpThreshold();
    }

    int minMs = static_cast<int>(as->getSampleRate() / as->getFFTbins());
    MYINFO << "Minimum scan lasting = SR/FFTpoints = " << minMs << " ms";

    waterfallChange = false;

}

bool Control::wfdBfs()
{
    static int integs = 0;
    bool backingUp = false;
    int i, scanIndex;
    double binFreq;
    float power;
    float dBfs = 0;
    Scan* scan;
    QString str;


    cal = as->currentUTCdateTime();
    double msTimeT = static_cast<double>(cal.toMSecsSinceEpoch()) / 1000.0;
    QString ts;
    ts = ts.asprintf("%.3f", msTimeT);
    //MYDEBUG << "wfDbfs() cal=" << cal.toString(Qt::ISODate) << "ts=" << ts;
    scanIndex = ps->getData();

    if(scanIndex == -1)
    {
        noDataBuf++;
        if(noDataBuf > MAX_MISSED_SCANS)
        {
            //stops updating waterfall
            MYWARNING << "No incoming scans";
            slotStopAcquisition(ASC_RADIO_FAULT_DETECTED);
            return 0;
        }


        if(scanIdxBak != -1)
        {
#ifdef SCAN_REPEATING
            //data not ready: reuses the last scan already processed
            backingUp = true;
            scanIndex = scanIdxBak;
            scanIdxBak = -1;
            QThread::yieldCurrentThread();
            MYDEBUG << "No data - reusing last data from scan#" << scanIndex;
#else
            return 0;
#endif
        }
        else
        {
            //no data to show available yet
            //MYDEBUG << "No data at all";
            return 0;
        }
    }
    else
    {
        MYDEBUG << "got scan#" << scanIndex;
        scanTime = scanTimer.restart();
        noDataBuf = 0;
    }

    //at least one data scan available
    scan = ps->getElem( scanIndex );
    MY_ASSERT( nullptr != scan )
    if(scan->getBufSize() == 0)
    {
        //invalid data, discard it
        MYINFO << "invalid data, releasing scanIndex#" << scanIndex;
        ps->release(scanIndex);
        scanIndex = -1;
        scanIdxBak = -1;
        return 0;
    }

    HzTuple ht = {0,0,0,0};

    if(!backingUp)
    {
        if(as->getScanMode() == SCAN_LAST)
        {
            MYDEBUG << "integration by last scan#" << scanIndex;
        }
        else
        {
            //reads all the written elements to hold the highest or to average them
            for(i = 1;;i++)
            {
                int toAddIdx = ps->getData();
                if(toAddIdx == -1)
                {
                    //no more scans in queue
                    if(i > 1 && i != integs)
                    {
                        MYDEBUG << "no more scans to get, integration by " << i;
                        integs = i;
                    }
                    break;
                }
                MYDEBUG << ">got scan#" << toAddIdx;
                Scan* toAdd = ps->getElem( toAddIdx );
                MY_ASSERT( nullptr != toAdd )
                if(toAdd->getBufSize() == 0 || scan->getBufSize() != toAdd->getBufSize())
                {
                    //invalid data, discard it
                    MYINFO << "invalid data, releasing toAddIdx#" << toAddIdx;
                    ps->release(toAddIdx);
                    toAddIdx = -1;
                    return 0;
                }

                //integrates all the pending scans before displaying them
                if(as->getScanMode() == SCAN_MAX)
                {
                    scan->peaks( toAdd );
                    MYDEBUG << "max hold integration, releasing toAddIdx#" << toAddIdx;
                    ps->release(toAddIdx);
                }
                else if(as->getScanMode() == SCAN_AVG)
                {
                    scan->average( toAdd );
                    MYDEBUG << "averaged integration, releasing toAddIdx#" << toAddIdx;
                    ps->release(toAddIdx);
                }

            } //for()

        } //if not (as->getScanMode() == SCAN_LAST)
    } //if !backingUp

    const float* scanData = nullptr;
    scanData = scan->constData();
    MY_ASSERT( nullptr !=  scanData)

    scanDelta = statClock.restart();

    //date+time used in CSV files
    dateTimeStamp = scan->getTimeStamp();

    //separe date from time - only time is shown on waterfall
    QStringList sl = dateTimeStamp.split(CSV_SEP);
    timeStamp = sl.at(1);


    avgDbfs = as->getMinDbfs();
    maxDbfs = as->getMinDbfs();

    maxFreq = 0;
    maxDiff = 0.0;

    if( restart == true && eventDetected == 0 )
    {
        if ( as->fpCmpf( scan->getBinSize(), as->getResolution(), 5 ) != 0 )
        {
            //the buffer size must match the resolution chosen
            //otherwise it means it's an old buffer that cannot
            //be processed anymore
            MYDEBUG << __func__ << " discarding old buffer, each bin is "
                   << scan->getBinSize()
                   << " Hz wide, while actual resolution is "
                   <<  as->getResolution() << " Hz";

            if(scanIdxBak != -1)
            {
                //releases the backupped data and
                //backs up the current data
                MYDEBUG << "releasing avgIdxBak#" << scanIdxBak;
                ps->release(scanIdxBak);
            }

            scanIdxBak = scanIndex;

            return false;
        }

        FFTbins = scan->getBufSize();
        binSize = scan->getBinSize();

        slotInitParams();
        restart = false;
    }

    if(newHdr == true)
    {
        if(as->getDumps() == Qt::Checked)
        {
            MYINFO << "Appending new header in CSV";
            QString header = csvFields.join(CSV_SEP);
            cbCSV->insert( qPrintable(header) );
            cbCSV->dump();
        }
        newHdr = false;
    }

    //the dump files record all the logged bins, while the
    //waterfall widget shows only the portion (bandwidth and offset)
    //required.
    //The dump files don't care of pixels

    int valuesFixed = 0;
    int notchIndex = -1;
    int avgBins = 0;
    bool notched = false;
    bool inPeak = false;
    bool scanReady = false;
    float lastDiff = 0.0F;
    float sumDbfs = 0.0F;
    int overdenseMs = (as->getMaxEventLasting() * 1000);


    Notch n;
    QList<Notch> notches = as->getNotches();

    binFreq = zoomLo;

    if(notches.count() > 0)
    {
        //one or more notches are defined
        notchIndex = 0;
        n = notches.at(notchIndex);
    }


    for ( i = firstLoggedBin ; i <= lastLoggedBin; i++ )
    {

        if(notchIndex != -1)
        {
            if(binFreq >= n.begin &&
               notched == false)
            {
                //entering in a notched band
                notched = true;
            }
            else if(binFreq > n.end &&
               notched == true)
            {
                //exiting a notched band
                notched = false;
                notchIndex++;
                if(notchIndex < notches.count())
                {
                    n = notches.at(notchIndex);
                }
                else
                {
                    //end of notches
                    notchIndex = -1;
                }

            }
        }

        power = scanData[i];

        if(i >= loggedBins)
        {
            //end of scan line
            break;
        }

        if (powerToDbfs( power, dBfs ) == false)
        {
            //fixes the data with default value
            dBfs = avgN;
            valuesFixed++;
            MYDEBUG << "ERROR: dBfs is NaN or infinite (scanData["
            << i
            << "]="
            << scanData[i]
            << ") forcing dBfs to " << dBfs ;

/*
            MYINFO << "discarding this broken scan# " << scanIndex;
            ps->release(scanIndex);
            scanIndex = -1;

            if(scanIdxBak != -1)
            {

                //releases this broken scan and takes
                //the last good backup scan
                scanIndex = scanIdxBak;
                MYINFO << "replacing it with the last backup scan# " << scanIdxBak;
            }
*/
        }

        if(notched == true)
        {
            //avgDbfs = avgN;
            dBfs = avgN;
        }

        //audio devices need resolution compensation due to binning x2 in Radio.cpp
        binFreq = binFreq + ((as->isAudioDevice()) ?
                    static_cast<double>( as->getResolution() / 2 ) :
                    static_cast<double>( as->getResolution() ));

        //MYDEBUG << "bin#" << i << " binFreq=" << binFreq;
        //the S-N difference is calculated also on two arbitrary bins
        //at beginning and at end of displayed scan, to help with detection
        //of ESD fakes
        if(i == firstLoggedBin + SCAN_ESD_OFFSET)
        {
            diffStart = dBfs - avgN;
        }

        if(i == lastLoggedBin - SCAN_ESD_OFFSET)
        {
            diffEnd = dBfs - avgN;
        }

        if(console == false)
        {
            scanReady = wf->appendPixels(dBfs, i == firstLoggedBin, i == lastLoggedBin);
            if(scanReady)
            {
                //MYDEBUG << timeStamp << "Scan ready";
            }
        }


        if((autoStopReq == true || manStopReq == true) && lastingTimer.elapsed() > overdenseMs)
        {
            //in automatic mode, if a carrier has been detected as an echo, the capturing
            //mode can be interrupted by pressing stop and wait until the maximum time allowed for an
            //overdense event expires. After that limit, each bin is set to avgN causing a
            //forced event close
            dBfs = avgN;
        }


        if(newHdr == false && scanIdxBak != -1) //avoids to append duplicate records
        {
            if( as->getDumps() == Qt::Checked )
            {
                if(i == firstLoggedBin)
                {
                    if(binDumps)
                    {
                        ScanHeader sh;
                        memcpy(&sh.scanMarker, "$$", sizeof(sh.scanMarker));
                        sh.timestamp_us = msTimeT;
                        sh.firstCell = dBfs;

                        QByteArray ba((char*)&sh, sizeof(sh));
                        cbDAT->append(ba);

                    }
                    else
                    {
                        cbDAT->append("%s    %i   %.2f\n", qPrintable(ts), static_cast<int>(binFreq), static_cast<double>(dBfs));
                    }
                }

                if(i > firstLoggedBin && i < lastLoggedBin)
                {
                    //skips the last record because it will be written later, after calculating
                    //the min max and average dBfs to mark the end of scan line
                    if(binDumps)
                    {
                        QByteArray ba((char*)&dBfs, sizeof(dBfs));
                        cbDAT->append(ba);
                    }
                    else
                    {
                        cbDAT->append("%s    %i   %.2f\n", qPrintable(ts), static_cast<int>(binFreq), static_cast<double>(dBfs));
                    }
                }
            }

        }

        int bf = static_cast<int>(binFreq);

        //statistics

        //the average noise is calculated on each entire scan
        //as a running average after summing all the bins displayed
        sumDbfs = sumDbfs + dBfs;

        //the average difference can never go below zero
        avgDiff = avgS - avgN;
        avgDiff = (avgDiff < 0) ? 0 : avgDiff;

        avgBins++;

        //the peaks and the S value are calculated only inside the detection range
        //the same for stddev
        if(bf >= rangeLo && bf <= rangeHi)
        {
            //MYDEBUG << "rangeLo=" << rangeLo << " rangeHi=" << rangeHi << " bf=" << bf << "Hz";
            if( dBfs > maxDbfs )
            {
                maxFreq = static_cast<int>(binFreq);
                maxDbfs = dBfs;
            }

            if( dBfs < minDbfs )
            {
                minDbfs = dBfs;
            }

            //detects the first and the last frequency in the scan that crosses
            //the lower threshold. Considering these pixels for the contiguous scans
            //interested by the event, it will be possible to calculate the number of
            //bins (FFT points) involved (Area)

            float diff = 0;


            if(as->getThresholdsBehavior() == TB_ABSOLUTE)
            {
                diff = dBfs;    //absolute thresholds
            }
            else if(as->getThresholdsBehavior() == TB_DIFFERENTIAL)
            {
                diff = dBfs - avgN;     //difference against fixed thresholds
            }
            else if (as->getThresholdsBehavior() == TB_AUTOMATIC)
            {
                diff = dBfs - avgN;     //difference against automatic thresholds

            }

            //takes the first lower threshold positive crossing in this scan
            if (diff > autoDnThr)
            {
                //MYDEBUG << "diff=" << diff << " autoDnThr=" << autoDnThr;
                if(lastDiff <= autoDnThr)
                {
                    //MYDEBUG << "lastDiff=" << lastDiff;
                    if(ht.hzFrom == 0)
                    {
                        //MYDEBUG << "ht.hzFrom == 0";
                        ht.hzFrom = static_cast<int>(binFreq);
                        ht.hzPeak = 0;
                        ht.hzTo = 0;
                        ht.count = 0;
                        inPeak = false;
                    }
                }
            }
            //counts each time the signal reaches a peak above the upper
            //threshold in this scan
            if (diff > autoUpThr)
            {
                //MYDEBUG << "diff=" << diff << " autoUpThr=" << autoUpThr;
                if(ht.hzFrom > 0)
                {
                    //MYDEBUG << "ht.hzFrom =" << ht.hzFrom;
                    if(inPeak == false)
                    {
                        //upper threshold positive crossing, increments the counter only
                        ht.count++;
                        inPeak = true;
                        //MYDEBUG << "inPeak = true, ht.count =" << ht.count;
                    }
                }
            }

            if(inPeak == true)
            {

                if(diff < autoUpThr)
                {
                    //upper threshold negative crossing
                    inPeak = false;
                    //MYDEBUG << "inPeak = false";
                }

                //Temporary maxDiff, calculated on the average noise of the previous scan.
                maxDiff = (maxDbfs - avgN);
                if(diff >= maxDiff )
                {
                    //stores the frequency of the peak
                    ht.hzPeak = static_cast<int>(binFreq);
                }


            }

            //takes the last lower threshold negative crossing in this scan
            if (diff <= autoDnThr &&
                lastDiff > autoDnThr &&
                ht.hzFrom > 0)
            {
                ht.hzTo = static_cast<int>(binFreq);
            }
            lastDiff = diff;
        }

        if(ht.hzFrom != 0 && ht.hzTo <= ht.hzFrom)
        {
            //the signal is still high at the end of range, force closing
             ht.hzTo = rangeHi;
        }

    }  //end of for() loop



    dBfsDelta = statClock.restart();

    //only at the end of the scan it's possible to determinate
    //the average N then the maximum S-N difference
    //and be sure the threshold has been crossed

    //the N value calculated on this scan is inserted in a FIFO
    avgDbfs = sumDbfs / avgBins;
    Nfifo.enqueue(avgDbfs);
    if( Nfifo.size() > as->getAvgdScans() )
    {
        //discards the oldest element to keep the
        //FIFO length constant.
        Nfifo.dequeue();
        if(isnan(avgN))
        {
            avgN = MIN_DBFS;
        }
        int intN = static_cast<int>(avgN);
        if(enableCapture == false &&
                intN < as->getNoiseLimit())
        {
            MYINFO << "Average N value available =" << avgN << " dBfs, value alloweed for capturing";
            enableCapture = true;
            db->sessionStart();
        }
        else if(
                am == AM_AUTOMATIC &&
                as->getNoiseLimit() < 0 &&
                intN > as->getNoiseLimit() &&
                enableCapture == true
                )
        {
            MYINFO << "Noise level " << avgN << "dBfs exceeds the maximum level alloweed for capturing" << as->getNoiseLimit() << " dBfs";
            MYWARNING << "Capture inhibited until the noise level will fall below the limit";
            emit status( tr("Band too noisy, capture inhibited") );
            enableCapture = false;
            db->sessionEnd(SEC_TOO_NOISY);
            eventDetected = 0;
        }
    }



    maxDiff = maxDbfs - avgN;           //canonical S-N calculated inside the peak detection range

    if(console == false)
    {
        MYDEBUG << scans  << ") avgN=" << avgN << " maxDbfs=" << maxDbfs << " maxDiff=" << maxDiff  << " maxFreq=" << maxFreq ;
    }
    else
    {
        MYINFO << "avgN=" << avgN << " maxDbfs=" << maxDbfs << " maxDiff=" << maxDiff << " maxFreq=" << maxFreq ;
    }
    //the S values are averaged too, these values
    //are used in case of automatic thresholds
    Sfifo.enqueue(maxDbfs);
    if( Sfifo.size() > as->getAvgdScans() )
    {
        //discards the oldest element to keep the
        //FIFO length constant.
        Sfifo.dequeue();
    }

    bool raisingEdge = false;

    if( init == false )
    {
        //doesn't record anything during initialization

        //updating CSV scan file
        if(am == AM_CONTINUOUS)
        {
            //continuous mode: the <prog> counter counts the scans.
            //one CSV row is produced at each scan
            if(newHdr == false)
            {
                //in continuous mode, the prog counts the scans, not the events
                //despite the "eventNr" in the header
                prog++;

                //does nothing if dumps are not required
                if( as->getDumps() == Qt::Checked )
                {
                    //prints the statistic values at each scan
                    //note that floats are localized, in order to use the local decimal separator
                    str = QString("%2%1%3%1%4%1%5%1%6%1%7%1%8%1%9%1%10%1%L11%1")
                            .arg(CSV_SEP)
                            .arg(prog)
                            .arg(dateTimeStamp)
                            .arg(qPrintable(ts))
                            .arg(as->getTune())
                            .arg(as->getDsBandwidth())
                            .arg(centerFreq)
                            .arg(zoomLo)
                            .arg(zoomHi)
                            .arg(zoomHi-zoomLo)
                            .arg(static_cast<double>(binSize));


                    str += QString("%2%1%L3%1%L4%1%L5%1%L6%1%7%1%8%1")
                            .arg(CSV_SEP)
                            .arg("-")
                            .arg(0)
                            .arg(0)
                            .arg(static_cast<double>(as->getUpThreshold()))
                            .arg(static_cast<double>(as->getDnThreshold()))
                            .arg(rangeLo)
                            .arg(rangeHi);

                    str += QString("%L2%1%L3%1%L4%1%L5%1%L6%1%7%1")
                            .arg(CSV_SEP)
                            .arg(static_cast<double>(maxDbfs))
                            .arg(static_cast<double>(avgS))
                            .arg(static_cast<double>(avgN))
                            .arg(static_cast<double>(maxDiff))
                            .arg(static_cast<double>(avgDiff))
                            .arg(maxFreq);

                    cbCSV->insert( qPrintable(str) );

                    if(prog % 30 == 0)
                    {
                        //dumps the data on file every 30 scans
                        cbCSV->dump();
                        MYINFO << "dumping scan metadata on CSV";
                    }
                }
            }
        }
        else if ( am == AM_PERIODIC )
        {
            //in periodic mode, the prog counts the scans, not the events.
            //the CSV record when the shot is made must carry its filename
            prog++;

            peakN = avgN;
            peakS = maxDbfs;
            peakDiff = (peakS - peakN);
            peakFreq = maxFreq;

            //does nothing if dumps are not required
            if( as->getDumps() == Qt::Checked )
            {
                //prints the statistic values at each scan
                //note that floats are localized, in order to use the local decimal separator
                str = QString("%2%1%3%1%4%1%5%1%6%1%7%1%8%1%9%1%10%1%L11%1")
                        .arg(CSV_SEP)
                        .arg(prog)
                        .arg(dateTimeStamp)
                        .arg(qPrintable(ts))
                        .arg(as->getTune())
                        .arg(as->getDsBandwidth())
                        .arg(centerFreq)
                        .arg(zoomLo)
                        .arg(zoomHi)
                        .arg(zoomHi-zoomLo)
                        .arg(static_cast<double>(binSize));

                str += QString("%2%1%L3%1%L4%1%L5%1%L6%1%7%1%8%1")
                        .arg(CSV_SEP)
                        .arg("-")
                        .arg(0)
                        .arg(0)
                        .arg(static_cast<double>(as->getUpThreshold()))
                        .arg(static_cast<double>(as->getDnThreshold()))
                        .arg(rangeLo)
                        .arg(rangeHi);

                str += QString("%L2%1%L3%1%L4%1%L5%1%L6%1%7%1")
                        .arg(CSV_SEP)
                        .arg(static_cast<double>(maxDbfs))
                        .arg(static_cast<double>(avgS))
                        .arg(static_cast<double>(avgN))
                        .arg(static_cast<double>(maxDiff))
                        .arg(static_cast<double>(avgDiff))
                        .arg(maxFreq);


                cbCSV->insert( qPrintable(str) );

                if(prog % 30 == 0)
                {
                    //dumps the data on file every 30 scans
                    cbCSV->dump();
                    MYINFO << "dumping scan metadata on CSV";
                }
            }
        }


        //event detection raising edge (automatic mode)
        //peaks that cross the upper threshold
        //are considered only within the detection range
        if( am == AM_AUTOMATIC &&
            enableCapture == true  &&
            scanIdxBak != -1 &&
            init == false ) //avoids first false event at acquisition start
        {

            if( eventDetected == 0 &&
                (
                    ( as->getThresholdsBehavior() == TB_DIFFERENTIAL && maxDiff > autoUpThr ) ||
                    ( as->getThresholdsBehavior() == TB_AUTOMATIC && maxDiff > autoUpThr) ||
                    ( as->getThresholdsBehavior() == TB_ABSOLUTE && maxDbfs > autoUpThr )
                )
              )
            {
                prog++;
                MYWARNING << "Event " << prog << " started at " << maxFreq << " Hz";
                MYINFO << "maxDbfs (S)=" << maxDbfs << " maxDiff (S-N)=" << maxDiff << " autoUpThr=" << autoUpThr;
                eventBorder.clear();
                eventBorder.append(ht);

                eventDetected = maxFreq;
                lastingTimer.start();

                hzShift = centerFreq - maxFreq;

                if(newHdr == false)
                {
                    //records the raising front
                    AutoData ad;
                    ad.dailyNr          = static_cast<int>(prog);
                    ad.eventStatus      = "Raise";
                    ad.utcDate          = cal.date();
                    ad.utcTime          = cal.time();
                    ad.timestampMs      = dateTimeStamp;
                    ad.revision         = as->getRTSrevision();
                    ad.upThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoUpThr : as->getUpThreshold();
                    ad.dnThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoDnThr : as->getDnThreshold();
                    ad.S                = maxDbfs;
                    ad.avgS             = avgS;
                    ad.N                = avgN;
                    ad.diff             = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : maxDiff;   //when TB_ABSOLUTE is set; the difference is not reported
                    ad.avgDiff          = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : avgDiff;   //when TB_ABSOLUTE is set; the average difference is not reported
                    ad.topPeakHz        = maxFreq;
                    ad.stdDev           = stddev;
                    ad.lastingMs        = 0;
                    ad.lastingScans     = 0;
                    ad.freqShift        = hzShift;
                    ad.echoArea         = 0;
                    ad.intervalArea     = 0;
                    ad.peaksCount       = 0;
                    ad.LOSspeed         = 0;
                    ad.scanMs           = avgScanTime;
                    ad.diffStart        = diffStart;
                    ad.diffEnd          = diffEnd;
                    ad.classification   = "";
                    ad.shotName        = "";
                    ad.dumpName         = "";

                    db->appendAutoData(&ad);
                    lad[0].clear();
                    lad[1].clear();
                    lad[2].clear();

                    lad[0] = ad;

                    //by default, the next record (the peak) is initialized as the raising edge.
                    //if a higher peak will be detected later, the peak record will be updated

                    //initialize event stats
                    peakN = avgN;
                    peakS = maxDbfs;
                    peakDiff = (peakS - peakN);
                    peakFreq = maxFreq;
                    ad.eventStatus = "Peak";
                    ad.S = peakS;
                    ad.N = peakN;
                    ad.diff = peakDiff;
                    ad.topPeakHz = peakFreq;
                    ad.stdDev = stddev;
                    ad.scanMs = avgScanTime;
                    db->appendAutoData(&ad);
                    lad[1] = ad;
                    raisingEdge = true;

                    //MYDEBUG << "stddev(N)=" << stddev;

                }
            }
            else if( eventDetected != 0 &&
                     manStopReq == false &&
                     autoStopReq == false &&
                     lastingTimer.elapsed() < overdenseMs &&  //force event closing if lasting too long
                     (
                        ( as->getThresholdsBehavior() == TB_DIFFERENTIAL && maxDiff > autoUpThr ) ||
                        ( as->getThresholdsBehavior() == TB_AUTOMATIC && maxDiff > autoUpThr ) ||
                        ( as->getThresholdsBehavior() == TB_ABSOLUTE && maxDbfs > autoUpThr )
                     )
                   )

            {
                //event running, continue detection of contour
                MYDEBUG << "Event " << prog << " continues...";
                MYDEBUG << "maxDbfs (S)=" << maxDbfs << " maxDiff (S-N)=" << maxDiff << " autoUpThr=" << autoUpThr << " autoDnThr=" << autoUpThr;
                if( eventDetected < 0 )
                {
                    if(
                        ( as->getThresholdsBehavior() == TB_DIFFERENTIAL && maxDiff > lad[1].diff ) ||
                        ( as->getThresholdsBehavior() == TB_AUTOMATIC && maxDiff > lad[1].diff ) ||
                        ( as->getThresholdsBehavior() == TB_ABSOLUTE && maxDbfs > lad[1].S )
                     )
                     {
                        //this event is joined to a previous peak
                        //so I must discard the latest 2 records
                        //(the peak and fall)
                        //and continue the detection

                        db->peakRollback();
                        db->peakRollback();

                        lad[1].clear();
                        lad[2].clear();

                        eventDetected = maxFreq;
                        peakDiff = 0;
                        MYINFO << "Joining to event " << prog << " at freq: " << maxFreq;
                    }
                }

                eventBorder.append(ht);

                //update event stats
                if(maxDiff >= peakDiff)
                {
                    if(lad[1].dailyNr > 0 && lad[1].eventStatus == "Peak")
                    {
                        //found a higher peak, updates the Peak record
                        db->peakRollback();
                    }
                    peakN = avgN;
                    peakS = maxDbfs;
                    peakDiff = (peakS - peakN);
                    peakFreq = maxFreq;

                    int msLasting = lastingTimer.elapsed();

                    int lastingScans = msLasting / avgScanTime;
                    if (lastingScans == 0)
                    {
                        lastingScans = 1;
                    }

                    AutoData ad;
                    ad.dailyNr          = static_cast<int>(prog);
                    ad.eventStatus      = "Peak";
                    ad.utcDate          = cal.date();
                    ad.utcTime          = cal.time();
                    ad.timestampMs      = dateTimeStamp;
                    ad.revision         = as->getRTSrevision();
                    ad.upThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoUpThr : as->getUpThreshold();
                    ad.dnThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoDnThr : as->getDnThreshold();
                    ad.S                = maxDbfs;
                    ad.avgS             = avgS;
                    ad.N                = avgN;
                    ad.diff             = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : maxDiff;   //when TB_ABSOLUTE is set; the difference is not reported
                    ad.avgDiff          = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : avgDiff;   //when TB_ABSOLUTE is set; the average difference is not reported
                    ad.topPeakHz        = maxFreq;
                    ad.stdDev           = stddev;
                    ad.lastingMs        = msLasting;
                    ad.lastingScans     = lastingScans;
                    ad.freqShift        = hzShift;
                    ad.echoArea         = 0;
                    ad.intervalArea     = 0;
                    ad.peaksCount       = 0;
                    ad.LOSspeed         = 0;
                    ad.scanMs           = avgScanTime;
                    ad.diffStart        = diffStart;
                    ad.diffEnd          = diffEnd;
                    ad.classification   = "";
                    ad.shotName        = "";
                    ad.dumpName         = "";

                    //MYDEBUG << "stddev(N)=" << stddev;

                    db->appendAutoData(&ad);
                    lad[1] = ad;
                }

            }
            else if( eventDetected > 0 &&
                     (
                        ( as->getThresholdsBehavior() == TB_DIFFERENTIAL && avgDiff < autoDnThr && maxDiff < autoDnThr  ) ||
                        ( as->getThresholdsBehavior() == TB_AUTOMATIC && avgDiff < autoDnThr && maxDiff < autoDnThr && stddev < (2 * avgStddev)) ||
                        ( as->getThresholdsBehavior() == TB_ABSOLUTE && maxDbfs < autoDnThr ) ||
                        ( lastingTimer.elapsed() > overdenseMs ) || //force event closing if lasting too long
                        ( restart == true )                   //force event closing when waterfall params change
                     )
                   )
            {
                //event detection falling edge:
                //when maxDiff falls below the lower threshold
                //after a peak has been detected
                MYWARNING << "Event " << prog << " stopped at " << maxFreq << " Hz";
                MYINFO << "stdev=" << stddev << " avgStdev=" << avgStddev;
                eventDetected = -maxFreq;
                int msLasting = lastingTimer.elapsed();

                //The doppler calculation assumes that the central frequency in waterfall
                //is the same of transmitter.
                //TODO: the calculation must keep in count the position of the
                //trasmitter vs. receiver

                const int C = SPEED_OF_LIGHT;
                float freqShift = (hzShift * C) / centerFreq;


                eventBorder.append(ht);

                //calculates area as nr. of bins interested in the echo
                int echoArea = 0;

                //and the entire interval area, a rectangle covering
                //the interval range x event lasting
                int intervalArea = 0;
                int peaksCount = 0;
                if (msLasting > overdenseMs)
                {
                    MYINFO << "Event closing forced because its lasting= " << msLasting
                     << "ms exceeds the maximum alloweed for overdense events: " << overdenseMs;
                    restart = true;
                }
                else if(restart)
                {
                    MYINFO << "Event closing forced because waterfall window params have been changed by the user";
                }

                foreach(ht, eventBorder)
                {
                    if(ht.hzTo == 0)
                    {
                        ht.hzTo = ht.hzPeak;
                    }
                    float hzCoverage = ht.hzTo - ht.hzFrom;
                    if(hzCoverage < 0)
                    {
                        MYDEBUG << "--------BAD echoArea -------------";
                        MYDEBUG << "ht.hzFrom: " << ht.hzFrom;
                        MYDEBUG << "ht.hzPeak: " << ht.hzPeak;
                        MYDEBUG << "ht.hzTo: " << ht.hzTo;
                        MYDEBUG << "ht.count: " << ht.count;
                    }
                    else if(hzCoverage == 0.0F)
                    {
                        //this happens when hzTo == hzFrom
                        //a correction is needed:
                        hzCoverage = binSize;
                        MYDEBUG << "fixing hzCoverage = 0 with hzCoverage=" << binSize;
                    }

                    echoArea += static_cast<int>(hzCoverage / binSize)  ;
                    MYDEBUG << "hzCoverage = " << hzCoverage << " Hz, echoArea = " << echoArea << " FFT points (bins)";
                    int hzFull = (rangeHi - rangeLo);
                    if(hzFull < 0)
                    {
                        MYDEBUG << "--------BAD intervalArea -------------";
                        MYDEBUG << "ht.hzFrom: " << ht.hzFrom;
                        MYDEBUG << "ht.hzPeak: " << ht.hzPeak;
                        MYDEBUG << "ht.hzTo: " << ht.hzTo;
                        MYDEBUG << "ht.count: " << ht.count;
                    }
                    intervalArea += (hzFull / binSize);
                    MYDEBUG << "hzFull = " << hzFull << "Hz, intervalArea = " << intervalArea << " FFT points (bins)";
                    peaksCount += ht.count;
                }
                MYINFO << "Resulting echo area: " << echoArea << " of " << intervalArea << " [points]";

                if(newHdr == false &&
                   scanIdxBak != -1 )
                {

                    wfShotFileName = makeFileName(AUTOSHOT_PREFIX, SSHOT_EXT, prog);
                    if(binDumps)
                    {
                        dumpFileName = makeFileName(DUMP_PREFIX, DUMP_DATA_EXT, prog);
                    }
                    else
                    {
                        dumpFileName = makeFileName(DUMP_PREFIX, GP_DATA_EXT, prog);
                    }

                    int lastingScans = msLasting / avgScanTime;
                    if (lastingScans == 0)
                    {
                        lastingScans = 1;
                    }

                    AutoData ad;
                    ad.dailyNr          = static_cast<int>(prog);
                    ad.eventStatus      = "Fall";
                    ad.utcDate          = cal.date();
                    ad.utcTime          = cal.time();
                    ad.timestampMs      = dateTimeStamp;
                    ad.revision         = as->getRTSrevision();
                    ad.upThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoUpThr : as->getUpThreshold();
                    ad.dnThr            = as->getThresholdsBehavior() == TB_AUTOMATIC ? autoDnThr : as->getDnThreshold();
                    ad.S                = maxDbfs;
                    ad.avgS             = avgS;
                    ad.N                = avgN;
                    ad.diff             = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : maxDiff;   //when TB_ABSOLUTE is set; the difference is not reported
                    ad.avgDiff          = as->getThresholdsBehavior() == TB_ABSOLUTE ? 0.0F : avgDiff;   //when TB_ABSOLUTE is set; the average difference is not reported
                    ad.topPeakHz        = maxFreq;
                    ad.stdDev           = stddev;
                    ad.lastingMs        = msLasting;
                    ad.lastingScans     = lastingScans;
                    ad.freqShift        = hzShift;
                    ad.echoArea         = echoArea;
                    ad.intervalArea     = intervalArea;
                    ad.peaksCount       = peaksCount;
                    ad.LOSspeed         = freqShift;
                    ad.scanMs           = avgScanTime;
                    ad.diffStart        = diffStart;
                    ad.diffEnd          = diffEnd;
                    ad.classification   = "";
                    ad.shotName         = "";

                    //MYDEBUG << "stddev(N)=" << stddev;

                    if(as->getScreenshots() == Qt::Checked)
                    {
                        // inserts the screenshot name only if a screenshot has been generated
                        ad.shotName = wfShotFileName;
                    }

                    ad.dumpName         = "";
                    if(as->getDumps() == Qt::Checked)
                    {
                        // inserts the dump name only if a dump has been generated
                        ad.dumpName = dumpFileName;
                    }

                    db->appendAutoData(&ad);
                    lad[2] = ad;

                    lastEventLasting = ad.lastingMs / 1000;
                }

                joinTimer.restart();
            }
            else if(joinTimer.elapsed() > as->getJoinTime() &&
                    eventDetected < 0)
            {
                //last peak event completely closed
                MYWARNING << "Event# " << prog << " closed";
                eventDetected = 0;
                eventBorder.clear();
            }

            if(eventDetected == 0 && joinTimer.elapsed() > as->getJoinTime())
            {
                if( as->getThresholdsBehavior() == TB_AUTOMATIC && eventDetected == 0 )
                {
                    //MYDEBUG << "no event pending, recalculate automatic thresholds";
                    MYDEBUG << "=> avgDiff=" << avgDiff;
                    MYDEBUG << "=> autoDnThr= avgDiff  + " << as->getDnThreshold();
                    MYDEBUG << "=> autoUpThr= autoDnThr+ " << as->getUpThreshold();
                    autoDnThr = avgDiff + as->getDnThreshold();
                    autoUpThr = autoDnThr + as->getUpThreshold();

                    MYDEBUG << "=> autoUpThr=" << autoUpThr << " autoDnThr=" << autoDnThr;
                }
            }
        }

    }

    if(console == false && wf != nullptr)
    {
        if(init)
        {
            //first drawing for graphs freq/power (instantaneous) and time/power (historic)
            //while waiting for init end
            wf->appendInfos(timeStamp, maxDbfs, avgN, avgDiff, autoUpThr, autoDnThr, (scans == 1));
        }

        if(!init)
        {
            wf->appendInfos(timeStamp, maxDbfs, avgN, avgDiff, autoUpThr, autoDnThr,raisingEdge);
            emit wfSetCapturing(!enableCapture, eventDetected);                             //update the capturing / too noisy labels
            emit wfSetScreenshotCoverage(sShotCoverage);                                    //update the screenshot coverage label
        }

        MYDEBUG << timeStamp << ") NOINIT calling slotRefresh() scanReady=" << scanReady;
        emit wfRefresh();                                                                   //update the waterfall and the sides graphs
    }

    csvDelta = statClock.restart();

    if(newHdr == false && scanIdxBak != -1) //avoids to append duplicate records
    {
        if( as->getDumps() == Qt::Checked )
        {
            //end of scan line
            if(binDumps)
            {
                float md = (as->getThresholdsBehavior() == TB_ABSOLUTE)? 0.0F : maxDiff;

                ScanFooter sf;
                sf.lastCell = dBfs;
                memcpy(&sf.endMarker, "&&", sizeof(sf.endMarker));
                sf.avgN = avgN;
                sf.maxDbfs = maxDbfs;
                sf.maxDiff = md;
                sf.avgDiff = avgDiff;
                sf.upThr = autoUpThr;
                sf.dnThr = autoDnThr;
                QByteArray ba((char*)&sf, sizeof(sf));
                cbDAT->append(ba);

            }
            else
            {

                cbDAT->append( "%s    %i   %.2f    %s   %.2f   %.2f   %.2f\n", qPrintable(ts),  static_cast<int>(binFreq), static_cast<double>(dBfs),
                               qPrintable(ts), static_cast<double>(avgN), static_cast<double>(maxDbfs),
                               (as->getThresholdsBehavior() == TB_ABSOLUTE)? 0.0 : static_cast<double>(maxDiff),
                               static_cast<double>(avgDiff), static_cast<double>(autoUpThr), static_cast<double>(autoDnThr)
                                                                             );
                                //when TB_ABSOLUTE is set, the difference is not reported
                cbDAT->append("");   //end of scan line (for surface plots) is marked with an empty line
            }

            cbDAT->commit();
        }
    }

    scan = nullptr;
    scanData = nullptr;

    //the averaged N value to be used in next scan
    //is calculated by averaging all the
    //values stored in FIFO
    avgN = avgDbfs;
    foreach (avgDbfs, Nfifo)
    {
        avgN += avgDbfs;
    }
    avgN /= (Nfifo.count()+1);

    //calculate the variance and stddev

    double dist = 0.0L;
    double sum = 0.0L;
    foreach (avgDbfs, Nfifo)
    {
        dist = pow(static_cast<double>(avgDbfs - avgN), 2.0L);
        sum = sum + dist;
    }

    double variance = sum / (Nfifo.count() - 1);
    stddev = sqrt(variance);
    //MYDEBUG << "variance(N)=" << variance << ", stddev(N)=" << stddev;

    if(eventDetected == 0 && init == false)
    {
        avgStddev = avgStddev + stddev;
        avgStddev = avgStddev / 2.0;
        MYDEBUG << "stdev=" << stddev << " avgStdev=" << avgStddev;
    }

    //the averaged S value to be used in next scan
    //is calculated by averaging all the
    //values stored in FIFO
    avgS = maxDbfs;
    foreach (maxDbfs, Sfifo)
    {
        avgS += maxDbfs;
    }
    avgS /= (Sfifo.count()+1);


    if(scanIdxBak != -1)
    {
        //releases the backupped data and
        //backs up the current data
        MYDEBUG << "releasing scanIdxBak#" << scanIdxBak;
        ps->release(scanIdxBak);
    }

    scanIdxBak = scanIndex;
    scans++;
    outDelta = statClock.restart();
    QThread::yieldCurrentThread();
    return raisingEdge;
}

bool Control::powerToDbfs(float pixPower, float& dBfs)
{
    bool result = true;
    float avgPow = r->getAvgPower();
    float dbfsGain = as->getDbfsGain();
    float dbfsOffset = as->getDbfsOffset();

    dBfs  = 10 * log10( pixPower / avgPow );
    dBfs  *= static_cast<float>( dbfsGain );
    dBfs  += static_cast<float>( dbfsOffset );

    if(std::isnormal(dBfs) == 0)
    {
        if(std::isnan(dBfs) != 0)
        {
            nans++;
            result = false;
            MYDEBUG << "NaN! pixPower=" << pixPower << " avgPow=" << avgPow << " dbfsGain=" << dbfsGain << " dbfsOffset=" << dbfsOffset;
        }
        if(std::isfinite(dBfs) == 0)
        {
            infinite++;
            result = false;
            MYDEBUG << "INF! pixPower=" << pixPower << " avgPow=" << avgPow << " dbfsGain=" << dbfsGain << " dbfsOffset=" << dbfsOffset;
        }
    }
    return result;
}

bool Control::getStats( StatInfos* stats )
{
    stats->sc.deadTime = deadTime;
    stats->sc.scanDelta = scanDelta;
    stats->sc.dBfsDelta = dBfsDelta;
    stats->sc.csvDelta = csvDelta;
    stats->sc.outDelta = outDelta;

    stats->sc.totalTime = totalTime;
    stats->sc.infinite = infinite;
    stats->sc.nans = nans;
    stats->sc.noDataBufs = noDataBuf;
    stats->sc.scans = scans;
    stats->sc.loop = loop;

    return r->getStats(stats);

}

void Control::printStats()
{
    StatInfos s;
    getStats(&s);

    uint effPerc = 0;
    if(s.sc.loop > 0)
    {
        effPerc = 100 - ((s.sc.noDataBufs * 100) / s.sc.loop);
    }

#if 0
    MYDEBUG << MY_ENDL
            << "Control - loop:" << s.sc.loop << MY_ENDL
            << "Control - scans:" << s.sc.scans << MY_ENDL
            << " deadTime:" << s.sc.deadTime << MY_ENDL
            << " scanDelta:" << s.sc.scanDelta << MY_ENDL
            << " dBfsDelta:" << s.sc.dBfsDelta << MY_ENDL
            << " csvDelta:" << s.sc.csvDelta << MY_ENDL
            << " outDelta:" << s.sc.outDelta << MY_ENDL
            << " totalTime:" << s.sc.totalTime << MY_ENDL
            << " infinite:" << s.sc.infinite << MY_ENDL
            << " NaNs:" << s.sc.nans  << MY_ENDL
            << " noDataBufs:" << s.sc.noDataBufs << MY_ENDL
            << " efficiency " << effPerc << "%%";

    MYDEBUG << MY_ENDL
            << "Radio - waitIQms:" << s.sr.waitIQms << MY_ENDL
            << " inputSamples:" << s.sr.inputSamples << MY_ENDL
            << " outputSamples:" << s.sr.outputSamples << MY_ENDL
            << " FFTbins:" << s.sr.FFTbins << MY_ENDL
            << " binSize:" << s.sr.binSize << MY_ENDL
            << " NaNs:" << s.sr.nans << MY_ENDL
            << " waitScanMs:" << s.sr.waitScanMs << MY_ENDL
            << " elapsedLoops:" << s.sr.elapsedLoops << MY_ENDL
            << " acqTime:" << s.sr.acqTime << MY_ENDL
            << " resampMs:" << s.sr.resampMs << MY_ENDL
            << " FFTms:" << s.sr.FFTms << MY_ENDL
            << " avgPower:" << s.sr.avgPower << MY_ENDL;

    if (testMode == false)
    {
        MYDEBUG << "SyncRx - timeFreeBuf:" << s.ss.timeFreeBuf
                << " timeAcq:" << s.ss.timeAcq << MY_ENDL
                << " overflows:" << s.ss.overflows << MY_ENDL
                << " droppedSamples:" << s.ss.droppedSamples << " " << MY_ENDL;
    }
#endif
    QString ps;

#ifdef _DEBUG
    if(statPlot == nullptr)
    {
        QString pltPath = rd.absoluteFilePath(INNER_STATISTIC_PLOT);
        statPlot = new QFile(pltPath);
        MY_ASSERT( nullptr !=  statPlot)
        bool ok = statPlot->open(QIODevice::WriteOnly);
        if(ok == true)
        {
            MYINFO << "echoes inner stat file opened; " << pltPath;
            ps = "Control:loop;Control:scans;totalTime;deadTime;scanDelta;dBfsDelta;csvDelta;outDelta;infinite;nans;noDataBufs;;Radio:inputSamples;outputSamples;FFTbins;binSize;acqTime;nans;waitScanMs;waitIQms;elapsedLoops;resampMs;FFTms;avgPower;droppedScans;;Syncrx;timeAcq;avgTimeAcq;droppedSamples;overflows;underflows\n";
            statPlot->write(qPrintable(ps), ps.length());
            ps = ";;time spent in acq.loop;time spent waiting loop;time spent waiting scan; time spent displaying scan;time spent updating csv;time spent updating dumps;;;;;;;;;;;time spent in radio loop;;;;;\n";
            statPlot->write(qPrintable(ps), ps.length());
            statPlot->flush();

        }
        else
        {
            MYCRITICAL << "unable to open inner stat file " << pltPath
                       << "error=" << statPlot->errorString();
            delete statPlot;
            statPlot = nullptr;
        }
    }
    else
    {
        QString ps = QString("%1;%2;%3;%4;%5;%6;%7;%8;%9;%10;%11;%12;;%13;%14;%15;%16;%L17;%18;%19;%20;%21;%22;%23;%L24;;")
            .arg(s.sc.loop)
            .arg(s.sc.scans)
            .arg(s.sc.totalTime)
            .arg(s.sc.deadTime)
            .arg(s.sc.scanDelta)
            .arg(s.sc.dBfsDelta)
            .arg(s.sc.csvDelta)
            .arg(s.sc.outDelta)
            .arg(s.sc.infinite)
            .arg(s.sc.nans)
            .arg(s.sc.noDataBufs)
            .arg(s.sr.inputSamples)
            .arg(s.sr.outputSamples)
            .arg(s.sr.FFTbins)
            .arg(static_cast<double>(s.sr.binSize))
            .arg(s.sr.acqTime)
            .arg(s.sr.nans)
            .arg(s.sr.waitScanMs)
            .arg(s.sr.waitIQms)
            .arg(s.sr.elapsedLoops)
            .arg(s.sr.resampMs)
            .arg(s.sr.FFTms)
            .arg(s.sr.avgPower)
            .arg(s.sr.droppedScans);

        if (testMode == false)
        {
            QString ps2 = QString(";%1;%2;%3;%4;%5")
                    .arg(s.ss.timeAcq)
                    .arg(s.ss.avgTimeAcq)
                    .arg(s.ss.droppedSamples)
                    .arg(s.ss.overflows)
                    .arg(s.ss.underflows);
            ps.append(ps2);
        }
        MYDEBUG << "writing echoes inner stats: " << ps << " in statPlot: " << statPlot;
        statPlot->write(qPrintable(ps), ps.length());
        ps = "\n";
        statPlot->write(qPrintable(ps), ps.length());
        statPlot->flush();
        MYDEBUG << "..done";

    }
#endif //_DEBUG
}

void Control::updateRootDir()
{
    QString rootDir = wd.absoluteFilePath( as->getConfigName() );
    dbPath = QString("%1/%2.sqlite3").arg(rootDir, as->getConfigName() );
    bool brandNew = false;
    if( !wd.exists(rootDir) )
    {
        //create the configuration root directory
        MYINFO << "creating root directory " << rootDir;
        wd.mkdir(rootDir);

        //clones the database model
        QString model = QString("%1/%2").arg(prgDir->path(), DB_MODEL);
        if( prgDir->copy(model, dbPath) )
        {
            MYINFO << "created database " << dbPath;
            brandNew = true;
        }
        else
        {
            MYWARNING << "failed creating " << dbPath;
        }
    }

    if(db == nullptr)
    {
        /*
         * The first time the app is started, it opens the DB, sets it in WAL mode,
         * fills it with
         * known informations then closes it immediately
         * because the operative DB connection must be created
         * in Control thread, to avoid GUI freezing in long operations
         */
        MYINFO << "Initializing DB...";
        db = new DBif(dbPath, as);
        MY_ASSERT(db != nullptr)
        if(brandNew)
        {

            db->setConfigData(as);
            db->updateCfgDevices(as);
            db->updateCfgFFT(as);
            db->updateCfgNotches(as);
            db->updateCfgOutput(as);
            db->updateCfgStorage(as);
            db->updateCfgPrefs(as);
            db->updateCfgWaterfall(as);
        }


        QDateTime dt;
        QString name;
        int revInDB = db->getConfigData( &name, &dt );
        if(revInDB < as->getRTSrevision())
        {
            db->setConfigData(as);
            db->updateCfgDevices(as);
            db->updateCfgFFT(as);
            db->updateCfgNotches(as);
            db->updateCfgOutput(as);
            db->updateCfgStorage(as);
            db->updateCfgPrefs(as);
            db->updateCfgWaterfall(as);
        }

        if(as->getEchoesVersion() != APP_VERSION)
        {
            //Echoes version changed
            db->updateCfgPrefs(as);
            MYWARNING << "Echoes version changed from " << as->getEchoesVersion() << " to " << APP_VERSION;
        }

        db->setWALmode();
        delete db;
        db = nullptr;
        MYINFO << "DB initialized, closed connection";
    }
    rd = wd;
    rd.cd(rootDir);
}

void Control::slotUpdateConfiguration()
{
    if(db)
    {
        MYINFO << __func__;
        QDateTime dt;
        QString name;
        int revInDB = db->getConfigData( &name, &dt );
        if(revInDB < as->getRTSrevision())
        {
            db->setConfigData(as);
            db->updateCfgDevices(as);
            db->updateCfgFFT(as);
            db->updateCfgNotches(as);
            db->updateCfgOutput(as);
            db->updateCfgStorage(as);
            db->updateCfgPrefs(as);
            db->updateCfgWaterfall(as);
        }
    }
    else
    {
        //posticipate update when db will be created
        updCfgReq = true;
    }
}
