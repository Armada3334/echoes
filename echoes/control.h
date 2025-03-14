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
$Rev:: 330                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-10-02 15:19:36 +02#$: Date of last commit
$Id: control.h 330 2018-10-02 13:19:36Z gmbertani $
*******************************************************************************/

#ifndef CONTROL_H
#define CONTROL_H

#include <cmath>
#include <time.h>
#include <QtCore>
#include <QtGui>
#include <QtMultimedia>

#include "scan.h"
#include "pool.h"
#include "dbif.h"

extern XQDir*  prgDir;

class Settings;
class CircBuf;
class Radio;
class Waterfall;
class Powergraph;
class PostProc;


// columns for CSV statistic file
enum  E_CSV_FIELDS
{
    CF_PROG,
    CF_DATE_UTC,
    CF_TIME_UTC,
    CF_UNIX_TIMESTAMP,
    CF_TUNE,
    CF_DSBW_HZ,
    CF_CENTRAL_HZ,
    CF_LOW_HZ,
    CF_HIGH_HZ,
    CF_ZOOMED_BW,
    CF_STEP_HZ,
    CF_THR_BEHAVIOR,
    CF_UP_THR_DBFS,
    CF_DN_THR_DBFS,
    CF_UP_THR_PERC,
    CF_DN_THR_PERC,
    CF_RANGE_LOW,
    CF_RANGE_HI,
    CF_PEAK_DBFS,
    CF_AVG_PEAK_DBFS,
    CF_AVG_DBFS,
    CF_DIFF_DBFS,
    CF_AVG_DIFF,
    CF_PEAK_HZ,

    CF_RESERVED,
    CF_STDDEV,
    CF_LASTING_MS,
    CF_SHIFT_HZ,
    CF_ECHO_AREA,
    CF_INTERVAL_AREA,
    CF_PEAKS_COUNT,
    CF_LOS_SPEED,
    CF_EVENT_STATUS,
    CF_SHOT_NAME,
    CF_TOTAL
};

enum E_ACQ_STOP_CAUSE
{
    ASC_UNKNOWN,
    ASC_MANUAL_REQ,
    ASC_SHOTS_FINISHED,
    ASC_GUI_WRONG_SETTING,
    ASC_RADIO_FAULT_DETECTED,
    ASC_KILLED_BY_SIGNAL
};

///
/// \brief The HzTuple struct
/// For each scan triggering an event, the following data are stored
/// in order to calculate the echo extension area (Hz x mS)
/// and its speed (doppler)
struct HzTuple
{
    int hzFrom;         ///lowest point above the lower threshold and contiguous to peak
    int hzPeak;         ///peak frequency
    int hzTo;           ///highest point above the lower threshold and contiguous to peak
    int count;          ///number of peaks (crossings of the upper threshold) in the same scan
};

///
/// \brief The EvTuple struct
///
struct EvTuple
{
    int     prog;            ///progressive event ID
    int     scan;            ///scan number where the event has been detected
    time_t  timeMs;          ///system time of the event [ms]
};


//Binary dump file structures mustn't have padding bytes, since their purpose is
//to be compact, and make it simple to be read from Python
#pragma pack(push,1)

/// \brief The binary dump file starts with a header
struct DumpHeader
{
    char startMarker[4];    ///constant string "DUMP"
    double resolution;      ///waterfall resolution (Hz per FFT point)
    uint32_t startHz;       ///waterfall starting Hz (leftmost)
    uint32_t points;        ///waterfall coverage in FFT points
    bool isAudio;           ///audio spectrum (startHz is always zero)
};

/// \brief then each scan is prepended with its header
struct ScanHeader
{
    char scanMarker[2];     ///constant string "$$"
    double timestamp_us;    ///scan timestamp, seconds.microseconds
    float firstCell;        ///first scan cell
};

/// \brief then a bunch of floats compose each scan. When scans are ended, the footer follows
/// and close the file:
struct ScanFooter
{
    float lastCell;        ///last scan cell
    char endMarker[2];      ///constant string "&&"
    float avgN;             ///noise N level in this scan
    float maxDbfs;          ///highest signal S level in this scan
    float maxDiff;          ///maximum S-N level reached in this scan

    float avgDiff;          ///average difference
    float upThr;            ///up threshold
    float dnThr;            ///down threshold
};
#pragma pack(pop)


class Control : public QObject
{
    Q_OBJECT

    QRecursiveMutex protectionMutex;

    QFile *statPlot;            ///internal statistics file
    Settings* as;               ///application settings
    Radio* r;                   ///RTLSDR interface
    CircBuf* cbCSV;             ///circular buffer for CSV tables output
    CircBuf* cbDAT;             ///circular buffer for spectra data DAT/DATB output
    Pool<Scan*>* ps;            ///data array pool
    PostProc* pp;               ///data postprocessing thread
    DBif* db;                   ///database interface pointer
    QTimer*  samplingTimer;     ///acquisition timer
    QSharedMemory* shm;         ///shared memory for realtime data sharing

    QQueue<EvTuple> waitingShots;    ///peaks revealed and waiting for a screenshot (periodic+automatic mode)
    QQueue<EvTuple> waitingDumps;    ///peaks revealed and waiting for a dump (automatic mode)

    QQueue<float> Nfifo;        ///N values related to latest scans, for sliding N average calculation
    QQueue<float> Sfifo;        ///S values related to latest scans, for sliding S-N average calculation

    E_ACQ_MODE  am;             ///acquisition mode

    AutoData lad[3];            ///last data entered in database [0]raise [1]peak [2]fall

    int dumpScans;              ///number of FFT scans to fit a dump
    int sShotCoverage;          ///screenshot coverage in ms
    int autoCount;              ///progressive number of automatic shots since acquisition started
    int manCount;               ///progressive number of manual shots since acquisition started
    int scanIdxBak;              ///progressive of last data scan processed


    uint loop;                  ///progressive loop count (scanTimer triggers count)
    uint scans;                 ///progressive displayed scans count

    uint scc;                   ///stop time changes count
    bool init;                  ///executing first acquisition loops

    bool console;               ///console mode if true (no graphics window)
    bool testMode;              ///no input device - input buffer is filled with a self produced signal.
    bool newHdr;                ///runtime params changed
    bool overload;              ///time consumed for a loop is higher than requested
    bool waterfallChange;       ///set to true when the waterfall visualization needs to be recalculated
    bool restart;               ///set to true when acquisition has just restarted to recalc the waterfall params
    bool enableCapture;         ///automatic shots are inhibited until the avgN value becomes reliable
    bool swapping;              ///daily swapping in progress
    bool binDumps;              ///when true, generates binary dumps
    bool updCfgReq;             ///the cfg tables in db must be updated

    ///processing scan lines:
    int fromHz;                 ///lowest frequency in band [Hz]
    int toHz;                   ///highest frequency in band [Hz]
    int zoomLo;                 ///lowest frequency showed at waterfall's left [Hz]
    int zoomHi;                 ///highest frequency showed at waterfall's right [Hz]
    int rangeLo;                ///lowest frequency falling into the peak detection range
    int rangeHi;                ///highest frequency falling into the peak detection range
    int tRange;                 ///current peak detection range in percentage
    int centerFreq;             ///waterfall's centered on this frequency
    int FFTbins;                ///nr.of FFT output bins (aka FFT points)
    int loggedBins;             ///nr. of bins showed in waterfall and logged to disk (depends from zoom control)
    int wfBw;                   ///subsample rate (bandwidth)
    int wfZoom;                 ///waterfall's showed portion
    int wfOffset;               ///waterfall's frequency offset
    qint64 wfTune;              ///waterfall's frequency tuned
    int wfEccentricity;         ///central frequency of peak detection range

    int firstLoggedBin;         ///first bin showed at leftmost pixel in waterfall
    int lastLoggedBin;          ///last bin showed at rightmost pixel in waterfall
    float binSize;              ///bins size [Hz]

    int totalTime;              /// ms spent in last loop (with or without displaying a scan)
    int scanTime;               /// total count of ms since the last displayed scan
    int avgScanTime;            /// average ms spent displaying a scan
    int deadTime;               /// ms spent since quitting wfdBfs til next loop
    int scanDelta;              /// ms spent in wfdBfs() waiting a scan to analyze
    int dBfsDelta;              /// ms spent in wfdBfs() producing a scan line for waterfall
    int csvDelta;               /// ms spent in wfdBfs() when saving statistic data to csv
    int outDelta;               /// ms spent in wfdBfs() when dumping gnuplot data to file
    int debounceManShotReqs;    /// debounce manual shot requests

    ///a scan peak is the maximum S-N found in a single scan:

    float stddev;               ///standard deviation of N
    float avgStddev;             ///average standard deviation of N
    float avgDbfs;              ///average background noise [dBfs] in the current scan inside the detection range
    float avgS;                 ///averaged peak S [dBfs] calculated on the latest scans
    float avgN;                 ///averaged background noise N [dBfs] calculated on the latest scans
    float avgDiff;              ///averaged S-N [dBfs] calculated by difference avgS-avgN (aka averaged SNR)
    float maxDbfs;              ///peak value (S)[dBfs] in the current scan, inside the peak detection range
    float minDbfs;              ///minimum value [dBfs] in the current scan, inside the peak detection range, for statistic purposes
    float maxDiff;              ///S-N: peak (S) - average (N) in the current scan, inside the peak detection range   
    int   maxFreq;              ///frequency [Hz] of the highest difference found in the current scan, inside the peak detection range
    int   hzShift;              ///frequency difference [Hz] between the center frequency and the peak

    int eventDetected;          ///when the difference crosses the up threshold this variable keeps its maxFreq
                                ///until the difference falls below the dn threshold. In this case, its maxFreq
                                ///is stored here with a minus sign.

    int lastEventLasting;       ///lasting of the last event [sec]

    ///an event peak is the maximum S-N found in a single event (only for automatic mode):

    float peakS;               ///peak value (S)[dBfs] of the event
    float peakN;               ///average background noise (N)[dBfs] at event peak
    float peakDiff;            ///S-N: peak (S) - average (N) at event peak
    int   peakFreq;            ///frequency [Hz] of the peakDiff

    float diffStart;            ///istantaneous S-N at the beginning of the scan
    float diffEnd;              ///istantaneous S-N at the end of the scan


    QList<HzTuple>  eventBorder;     ///keeps track of echo contour points to calculate its area in pixels

    float autoUpThr;           /// dynamic upper threshold
    float autoDnThr;           /// dynamic lower threshold

    uint nans;                  /// progressive count of NANs
    uint infinite;              /// progressive count of infinites
    uint noDataBuf;             /// progressive count of missing FFT data to display
    uint prog;                  /// progressive event counter

    bool autoStopReq;           ///acquisition self-stopping request
    bool manStopReq;            ///acquisition manual stop request

    QString sessionName;        ///self generated as <config name>+<GMT timestamp> without prefixes and extension
    QString cmdFileName;        ///self generated as <gpPrefix>+<sessionName>.<GP_CMD_EXT> (gnuplot command files)
    QString csvFileName;        ///self generated as <CSV_PREFIX>+<sessionName>.<TABLE_EXT>
    QString datFileName;        ///self generated as <gpPrefix>+<sessionName>.<GP_DATA_EXT> (for continuous plots)
    QString dumpFileName;       ///self generated as <gpPrefix>+<sessionName>+<progressive dump nr.>.<GP_DATA_EXT>
                                ///for (plot shots, automatic and periodic)
    QString wfShotFileName;     ///self generated as
                                ///<AUTOSHOT_PREFIX | MANSHOT_PREFIX>+<sessionName>+<progressive shot nr.>.<SSHOT_EXT>
                                ///(waterfall screenshots)
    QString faultStatus;        ///status error to display in case of AM_INVALID
    QString dbPath;             ///path to current sqlite database


    QDateTime cal;              ///current scan datetime object
    QString dateTimeStamp;      ///current scan datetime string
    QString timeStamp;          ///current scan time string
    QDateTime calOld;           ///last scan datetime object
    QDateTime lastSessionCheck;          ///acquisition start timestamp

    QElapsedTimer statClock;    ///stopwatch for statistics
    QElapsedTimer scanTimer;    ///stopwatch for scan time measure
    QElapsedTimer lastingTimer; ///stopwatch to measure event lasting (time between raising-->falling front)
    QElapsedTimer joinTimer;    ///stopwatch to measure the minimum time between consecutive separate events

    QStringList csvFields;      ///CSV output: columns names



    XQDir rd;                   ///configuration root directory object  where the files generated by echoes are placed
    XQDir wd;                   ///working directory object, where the rts files are placed

    Waterfall*  wf;             ///Waterfall subwindow pointer

    QSound* pingSound;          ///Event notification sound
    QSound* faultSound;         ///Fault notification sound

protected:


    ///
    /// \brief wfdBfs
    ///updates waterfall, powergraph, log and dump files if required, returns true
    ///when a new event is detected
    ///
    /// \return
    ///
    bool    wfdBfs();


    ///
    /// \brief printStats
    ///
    void    printStats();           ///updates echoes.dat

    ///
    /// \brief makeSessName
    /// \param dateTime
    ///
    void makeSessName(QString dateTime);

    ///
    /// \brief makeFileName
    /// \param prefix
    /// \param ext
    /// \param progId
    /// \return
    ///
    QString makeFileName(const QString &prefix, const QString &ext, uint progId = 0);

    ///
    /// \brief getScansFromSeconds
    /// \param sec
    /// \return
    ///
    int getScansFromSeconds(int sec);


    ///
    /// \brief getStats
    /// \param stats pointer to zeroed structure
    /// \return false on failure
    ///
    bool getStats( StatInfos* stats );



    ///
    /// \brief measureScanTime
    /// calculate dynamically the scan time and the amount of scans
    /// needed to cover the recTime and produce a gnuplot dump
    ///
    void measureScanTime();

    ///
    /// \brief powerToDbfs
    /// \param pixPower
    /// \param dBfs
    /// \return
    ///
    bool powerToDbfs(float pixPower, float& dBfs);   ///linear power to dBfs

    ///
    /// \brief getDateTimeStamp
    /// \return a Qt::ISODate representation of current UTC datetime
    /// without colons inside, suited for data recording
    QString getDateTimeStamp();

    ///
    /// \brief getDateStamp
    /// \return a Qt::ISODate representation of current UTC date
    ///
    QString getDateStamp();

    ///
    /// \brief audioNotify
    /// plays a notification sound
    void audioNotify(QSound* sound);


public:
    ///
    /// \brief Control
    /// \param appSettings
    /// \param cMode
    /// \param wDir
    /// \param parent
    ///
    explicit Control(Settings* appSettings, XQDir wDir);

    ~Control();


    ///
    /// \brief getDateTime
    /// \return
    ///
    QString getDateTime();          ///Returns a representation of current UTC datetime for display purposes

    ///
    /// \brief getRadio
    /// \return the radio object
    ///
    Radio* getRadio() const
    {
        return r;
    }

    ///
    /// \brief postProcessor
    /// \return the postprocessor object
    ///
    PostProc* postProcessor() const
    {
        return pp;
    }

    ///
    /// \brief workingDir
    /// \return
    ///
    XQDir workingDir() const
    {
        return wd;
    }


    ///
    /// \brief getAcquisitionMode
    /// \return acquisition mode
    ///
    E_ACQ_MODE getAcquisitionMode() const
    {
        return am;
    }


    ///
    /// \brief UTCcalendarClock
    /// \return
    ///
    QDateTime UTCcalendarClock() const
    {
        return cal;
    }

    ///
    /// \brief isConsoleMode
    /// \return
    ///
    bool isConsoleMode() const
    {
        return console;
    }

    ///
    /// \brief isAcquisitionRunning
    /// \return
    ///
    bool isAcquisitionRunning() const
    {
        if(init == false && samplingTimer != nullptr)
        {
            return (samplingTimer->isActive() && (scc == 1));
        }
        return false;
    }

    ///
    /// \brief measuredInterval
    /// \return mS
    ///
    int measuredInterval() const
    {
        return avgScanTime;
    }

    ///
    /// \brief updateRootDir
    /// \return
    ///
    void updateRootDir();

    ///
    /// \brief getDBpath
    /// \return
    ///
    QString getDBpath() const
    {
        return dbPath;
    }

signals:
    ///
    /// \brief acquisition stopped notification / feedback
    ///
    void notifyStop();


    ///
    /// \brief acquisition started notification / feedback
    ///
    void notifyStart();

    ///
    /// \brief setShotCounters
    /// \param shotNr
    /// \param totalShots
    ///
    void setShotCounters( int shotNr, int totalShots );


    ///
    /// \brief status
    /// \param msg
    ///
    void status(const QString& msg);

    ///
    /// \brief GUIbusy
    ///
    void GUIbusy();

    ///
    /// \brief GUIfree
    ///
    void GUIfree();

    ///
    /// \brief waterfallClean
    ///
    void wfClean();

    ///
    /// \brief wfTakeShot
    /// \param shotNr
    /// \param totalShots
    /// \param maxDbm
    /// \param avgDbm
    /// \param avgDiff
    /// \param shotFileName
    ///
    void wfTakeShot(int shotNr, int totalShots,
                       float maxDbm, float avgDbm, float avgDiff,
                       float upThr, float dnThr,
                       QString shotFileName = "unnamed.png" );

    ///
    /// \brief wfUpdateCounter
    /// \param shotNr
    /// \param totalShots
    ///
    void wfUpdateCounter(int shotNr, int totalShots);

    ///
    /// \brief wfUpdateGrid
    ///
    void wfUpdateGrid();

    ///
    /// \brief wfRefresh
    ///
    void wfRefresh();

    ///
    /// \brief wfAppendPixels
    /// \param binDbfs
    ///
    void wfAppendPixels(float binDbfs);

    ///
    /// \brief wfSetCapturing
    /// \param isInhibited
    /// \param isCapturing
    ///
    void wfSetCapturing(bool isInhibited, bool isCapturing);

    ///
    /// \brief slotSetScreenshotCoverage
    /// \param secs
    ///
    void wfSetScreenshotCoverage(int secs);



protected slots:


    ///
    /// \brief slotControlLoop
    /// called at <interval> rate to record event data
    /// and scale the FFT output from radio thread and
    /// forward it to the waterfall (main thread)
    ///
    void slotControlLoop();
    ///
    /// \brief slotSetInterval
    ///
    void slotSetInterval();

    ///
    /// \brief slotSetShots
    ///
    void slotSetShots();

    ///
    /// \brief slotSetRecTime
    ///
    void slotSetRecTime();

    ///
    /// \brief slotSetAfter
    ///
    void slotSetAfter();

    ///
    /// \brief slotSetUpThreshold
    ///
    void slotSetUpThreshold();

    ///
    /// \brief slotSetDnThreshold
    ///
    void slotSetDnThreshold();

    ///
    /// \brief slotSetTrange
    ///
    void slotSetDetectionRange();

    ///
    /// \brief slotSetError
    /// \return
    ///
    bool slotSetError();

    ///
    /// \brief slotStopTimeChange
    ///posticipate the application of changed settings when acquisition starts
    void slotStopTimeChange();

    ///
    /// \brief slotTakeWfAutoShot
    /// takes an automatic waterfall screenshot
    void slotTakeWfAutoShot();


    ///
    /// \brief slotTakeDump
    ///generates a data dump
    void slotTakeDump();


public slots:

    ///
    /// \brief slotAcquisition
    /// \param go
    /// called from main() in console mode, otherwise
    /// from mainwindow thru Start/Stop pushbutton
    ///
    void slotAcquisition(bool go);

    ///
    /// \brief slotTakeWfManShot
    /// called by waterfall
    /// to take a manual waterfall screenshot
    /// by pressing the button
    void slotTakeWfManShot();

    ///
    /// \brief stopLoop
    /// E_ACQ_STOP_CAUSE what
    ///stops immediately the radio acquisition threads
    void slotStopAcquisition(int what);


    ///
    /// \brief initParams
    /// \return
    ///
    void slotInitParams();

    ///
    /// \brief slotWaterfallResized
    ///
    void slotWaterfallResized()
    {
        waterfallChange = true;
    }

    ///
    /// \brief slotUpdateConfiguration
    ///updates the cfg tables on DB each time
    ///the rts is saved
    void slotUpdateConfiguration();
};

#endif // CONTROL_H
