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
$Rev:: 367                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-23 18:22:34 +01#$: Date of last commit
$Id: settings.h 367 2018-11-23 17:22:34Z gmbertani $
*******************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QtCore>
#include <QtNetwork>
#include <QtGui>

#include <SoapySDR/Types.hpp>

#include "xqdir.h"
#include "setup.h"


extern uint logLevel; //from main.cpp

#define MYDEBUG             if(logLevel >= ECHOES_LOG_ALL) qDebug()
#define MYINFO              if(logLevel >= ECHOES_LOG_INFO) qInfo()
#define MYWARNING           if(logLevel >= ECHOES_LOG_WARNINGS) qWarning()
#define MYCRITICAL          if(logLevel >= ECHOES_LOG_CRITICAL) qCritical()
#define MYFATAL(str,arg)    if(logLevel >= ECHOES_LOG_FATAL) qFatal(str,arg)


///
/// \brief The StatInfos class
/// container for program's statistics
class StatInfos
{
public:

    struct StatControl
    {
        uint loop;                  // progressive loops count
        uint scans;                 // progressive scans count
        uint noDataBufs;            // progressive count of missing FFT data to display
        uint nans;                  // progressive count of NANs
        uint infinite;              // progressive count of infinites
        int totalTime;              // ms spent in last scan
        int deadTime;               // ms spent since quitting wfdBfs til next tick
        int scanDelta;              // ms spent in wfdBfs() waiting a scan to analyze
        int dBfsDelta;              // ms spent in wfdBfs() producing a scan line for waterfall
        int csvDelta;               // ms spent in wfdBfs() when saving statistic data to csv
        int outDelta;               // ms spent in wfdBfs() when dumping gnuplot data to file

    } sc;

    struct StatRadio
    {
        uint nans;                                    /// progressive count of NANs
        uint elapsedLoops;                            /// progressive counter of radio thread loops

        int inputSamples;                               /// resampler input samples
        int outputSamples;                              /// resampler output samples
        int FFTbins;                                    /// total FFT  bins (N)
        float binSize;                                  /// size of each bin

        int acqTime;                                  /// minimum ms needed to get a sample from dongle
        int waitScanMs;                               /// ms lost while waiting for a free scan bufs
        int waitIQms;                                 /// ms lost while waiting for IQ samples
        int resampMs;                                 /// ms taken by resampling IQ data
        int FFTms;                                    /// ms spent in FFT
        float avgPower;                               /// average power of this scan
        int droppedScans;                             /// number of wasted scans due to buffer unavailability
    } sr;

    struct StatIQdevice
    {
        int droppedSamples;                           /// number of acquisition fails due to buffer unavailability
        int overflows;                                /// progressive count of overflows
        int underflows;                                /// progressive count of underflows
        int timeAcq;                                  /// time spent getting a scan from the dongle
        int avgTimeAcq;                               /// average time spent getting a scan from the dongle
    } ss;

    StatInfos()
    {
        memset(&sc, 0, sizeof(sc));
        memset(&sr, 0, sizeof(sr));
        memset(&ss, 0, sizeof(ss));
    }

};


///
/// \brief The Notch class
///
class Notch
{
public:
    Notch()
    {
        freq = 0;
        width = 0;
        begin = 0;
        end = 0;
    }

    int operator ==(const Notch& n2)
    {
        return (freq == n2.freq &&
                width == n2.width &&
                begin == n2.begin &&
                end == n2.end);
    }

    int operator !=(const Notch& n2)
    {
        return !(freq == n2.freq &&
                width == n2.width &&
                begin == n2.begin &&
                end == n2.end);
    }

    int freq;           //notch frequency center
    int width;          //notch band width (= notchEnd-notchBeg)
    int begin;          //notch band begin [Hz]
    int end;            //notch band end [Hz]
};




class Waterfall;
class MainWindow;

///
//Initialization states
enum APP_STATE
{
    AST_NONE,           //nothing changed
    AST_INIT,           //app just started
    AST_ERROR,          //temporary error
    AST_CANNOT_OPEN,    //app and device found but could not open it

    AST_OPEN,           //device opening ok, waiting for acquisition start
    AST_STOPPED,        //acquisition stopped
    AST_STARTED,        //acquisition started

    AST_TOTAL_STATES    //final stub
};

///
/// \brief The FFT_WINDOWS enum
///
enum FFT_WINDOWS
{
    FTW_RECTANGLE,
    FTW_HAMMING,
    FTW_BLACKMAN,
    FTW_BLACKMAN_HARRIS,
    FTW_HANN_POISSON,
    FTW_YOUSSEF,
    FTW_BARTLETT,

    FTW_HANN,
    FTW_BLACKMAN_HARRIS_7,
    FTW_FLATTOP,
};


///
/// \brief The E_THRESHOLDS_BEHAVIOR enum
///
enum E_THRESHOLDS_BEHAVIOR
{
    TB_DIFFERENTIAL,
    TB_ABSOLUTE,
    TB_AUTOMATIC
};

///
/// \brief The E_ACQ_MODE enum
///
enum E_ACQ_MODE
{
    AM_INVALID,
    AM_CONTINUOUS,      //< continuous S/N acquisition
    AM_PERIODIC,        //< periodic acquisition, with dumps taken every X seconds
    AM_AUTOMATIC        //< automatic acquisition, with dumps taken when S-N exceeds a threshold
};

///
/// \brief The E_PALETTE enum
///
enum E_PALETTE
{
    PALETTE_BW,         //< black & white
    PALETTE_CLASSIC     //< colors
};

///
/// \brief The E_SCAN_INTEG enum
///
enum E_SCAN_INTEG
{
    SCAN_INVALID,
    SCAN_LAST,
    SCAN_AVG,
    SCAN_MAX
};


///
/// \brief The Settings class
///
class Settings : public QObject
{
    Q_OBJECT

    QLocale     loc;
    QRecursiveMutex  protectionMutex;    ///<generic mutex to protect against concurrent calls

    bool loading;               //< set high when load() is in execution

    int changed;                //< if >0 settings changed, save() must be called before exiting program
    int srvChanged;             //< counts the configuration changes that must be notified to UDP clien

    int rtsRevision;            //< number of revision of the rts file loaded
    int rtsFormatRev;           //< number of revision of the rts file format
    QString echoesVersion;      //< version of Echoes that generated this file

    //settings:

    //main window settings:
    QRect       geom;               //< mainwindow geometry
    QString     configName;         //< configuration file name without extension
    QString     configFullPath;     //<  configuration file's absolute path

    //device settings:
    QString     device;         //< SDR dongle active
    int         sampleRate;     //< dongle sample rate [Hz]
    int         bandwidth;      //< dongle bandwidth [Hz]
    qint64      tune;           //< SDR tuning in Hz
    int         gain;           //< RF gain [%]    -1 == AGC on
    int         ppmError;       //< tuner error compensation [ppm]
    int         ssBypass;       //< subsampler bypass
    QString     antenna;        //< antenna
    QString     fRange;         //< frequency range
    QString     bRange;         //< bandwidth range
    QString     sRange;         //< sample rate range

    //FFT settings:
    int         dsBw;           //< downsampled bandwidth [Hz]
    FFT_WINDOWS ftw;            //< FFT window function
    qint64      resolution;     //< FFT resolution stored as int, expressed as [Hz * 1e9] to avoid precision loss
    int         fftBins;        //< FFT resolution in points
    int         dsRatio;        //< SR/dsBw

    //this value is set by the device itself, the GUI must react consequently:
    double      maxResHz;         //< the minimum Hz resolution allowed by device (0=no limits)
    int         maxResMTU;        //< the maximum FFT points resolution allowed by device


    //output settings:
    int         acqMode;        //< acquisition mode
    int         interval;       //< waterfall display interval [mS]
    int         shots;          //< max number of shots to take before self-stop
    int         recTime;        //< size of each shot [sec] = before+after
    int         after;          //< number of seconds to record after trigger
    int         upThreshold;    //< peaks detection S-N threshold [dBfs / 100]
    int         dnThreshold;    //< peaks termination S-N threshold [dBfs / 100]
    int         detectionRange; //< peaks detection S-N frequency range
    int         genDumps;       //< generate dump files for plotting
    int         genScreenshots; //< generate screenshots
    int         avgdScans;      //< Noise filter slope
    int         joinTime;       //< minimum time between consecutive events
    int         palette;        //< 0=B/W, 1=color
    int         noiseLimit;     //< maximum noise alloweed for automatic capturing

    //storage settings
    int     eraseLogs;           //< erase all log files before acquisition restarts
    int     eraseShots;          //< erase all shots before acquisition restarts
    int     overwriteSnap;       //< new DB snapshot overwrites the latest
    int     DBsnap;              //< creates a DB snapshot every days
    int     dataLasting;         //< tabular data lasting in days
    int     imgLasting;          //< screenshots and dumps lasting in days
    int     minFree;             //< minimum space to keep free on working drive [GB]

    //preferences:
    int     maxEventLasting;     //< lasting in secs of the longest valid event
    int     hzTickMarks;         //<  show tick marks on X axis (Frequency Hz - one tick every 1000 Hz)
    int     dBfsTickMarks;       //<  show tick marks on Y axis (total power dBfs)
    int     wfSecTickMarks;      //<  show tick marks on waterfall's Y axis (absolute time GMT - one tick every second)
    int     wfRangeBoundaries;   //<  show the peak detection interval boundaries on waterfall

    int     tooltips;            //<  tooltips visible if nonzero
    int     pingOn;              //< ping sound enabling (events notification)
    int     faultSoundOn;        //< acquisition fault notification sound enabling
    int     thresholdsBehavior;  //< thresholds behavior: absolute, differential, automatic
    int     dbFSoffset;          //< dBm calibration: offset applied to power values from FFT
    float   dbFSgain;            //< dBb calibration: gain applied to power values from FFT
    int     serverPort;          //< dongle server port
    int     createDATdumps;      //< create ASCII DAT dumps instead of binary ones (DATB)
    int     datagramDelay;       //< delay added after sending an UDP datagram [us]
    int     scanMode;     //< specifies how to deal with scans when more data than needed are available

#ifdef WANT_FULL_UTC_DELTA
    int      UTCdelta;    //< time difference to UTC, converted to seconds.
                          // allows to make the program work as in another timezone
                          // without involve the OS
#else
    QTime    UTCdelta;    //< hour when the swap will happen, by default midnight UTC (00:00)
                          // Set nonzero to debug daily swapping problems
#endif

    QHostAddress    serverAddr;  //< dongle server IP address


    //waterfall window  settings:
   //<
    int     wfOffset;           //< when band is zoomed, the frequency offset control browses up/dn the spectral portion showed [Hz]
    int     wfZoom;             //< magnification factor of band displayed in waterfall, in tenths of X
    int     wfEccentricity;     //< position of the event detection interval related to waterfall's centre [Hz]
    int     wfEccentricityBak;  //< previous position of the event detection interval for rounding purposes [Hz]
    int     brightness;         //< palette settings - brightness percentage 0..100%
    int     contrast;           //< palette settings - contrast percentage 0..100%
    int     powerZoom;          //< power graphs: gain of power axle
    int     powerOffset;        //< power graphs: offset of power plotting
    QList<Notch> notches;       //< notch filters list
    bool      plotS;              //< enable plotting peak power (S)
    bool      plotN;              //< enable plotting average power (N)
    bool      plotD;              //< enable plotting difference
    bool      plotA;              //< enable plotting average difference
    bool      plotU;              //< enable plotting upper threshold
    bool      plotL;              //< enable plotting lower threshold

    //local settings
    int     resetCounter;       //< number of times the program terminated abnormally
    QString lastDBsnapshot;     //< date of the last DB snapshot created

    QRect   wwGeom;             //< waterfall window geometry
    QRect   wwPaneGeom;         //< waterfall widget pane geometry
    QString     GNUplotPath;    //< path to the gnuplot executable
    QString     logo;           //< path to PNG file displayed as logo in waterfall window
    QSettings*  localSettings;  //< nonportable machine-dependent configuration

    //temporary storage (not saved on RTS)
    int    vacuumStatus;         //< DB vacuum in progress
    int    consoleMode;        //< application started in console mode (-c)
    int    dumbMode;           //< application started in dumb mode (-d)
    int    audioDevice;        //< no SDR device connected, using audio input
    int    netDevice;          //< no SDR device connected, showing samples from UDP server
    int    testDevice;         //< no SDR device connected, running self test patterns

    int     zoomLo;             //< lowest freq. showed in waterfall - NOT SAVED ON DISK
    int     zoomHi;             //< highest freq. showed in waterfall - NOT SAVED ON DISK
    qint64  intervalHz;         //< red needle frequency
    float  hzPerPixel;         //< Hz per pixel (waterfall resolution) - NOT SAVED ON DISK
    Waterfall*  ww;             //< waterfall window pointer - NOT SAVED ON DISK
    MainWindow* mw;             //< main window pointer - NOT SAVED ON DISK

public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    //restore setting to defaults
    void reset();

    //save/load configuration settings AND local settings files
    void load( QSettings& ini, bool enableNotifications = true );
    void save( QSettings& ini, bool enableNotifications = true );

    void loadConfig(QString& name , bool enableNotifications = true);

    int changes() const
    {
        return changed;
    }

    int serverChanges() const
    {
        return srvChanged;
    }

    void setResetCounter(bool clear = false)
    {
        resetCounter = (clear == true)? 0 : resetCounter+1;
        saveLocal();
    }




public slots:
    //getters


    QString getEchoesVersion()
    {
        return echoesVersion;
    }

    int getRTSrevision() const
    {
        return rtsRevision;
    }


    //mainwindow settings
    QString getConfigName()     const { return configName    ; }
    QString getConfigFullPath() const { return configFullPath; }


    //device settings:

    QString getDeviceNoIdx(QString &dev);     //< returns the device name without index and serial number

    QString getDevice()     const { return device        ; }
    int getSampleRate()     const { return sampleRate    ; }
    int getBandwidth()      const { return bandwidth      ; }
    qint64 getTune()        const { return tune          ; }
    int getGain()           const { return gain          ; }
    float getError()        const { return static_cast<float>(ppmError); }
    QString getAntenna()    const { return antenna       ; }

    QString getFreqRange()          const { return fRange        ; }
    QString getBandwidthRange()     const { return bRange        ; }
    QString getSampleRateRange()    const { return sRange        ; }

    //FFT settings:
    FFT_WINDOWS getWindow() const { return ftw           ; }
    double getResolution()   const
    {
        double res =  static_cast<double>(resolution);
        res /= 10e8 ;
        return res;
    }
    int getFFTbins()        const { return fftBins       ; }
    int getSsBypass()       const { return ssBypass      ; }
    int getDsBandwidth()    const { return dsBw;           }
    int getDSratio()        const { return dsRatio;        }

    //output settings
    int getAcqMode()        const { return acqMode       ; }
    int getInterval()       const { return interval      ; }
    int getShots()          const { return shots         ; }
    int getRecTime()        const { return recTime       ; }
    int getAfter()          const { return after         ; }
    float getUpThreshold()  const { return static_cast<float>(upThreshold) / 100.0F   ; }
    float getDnThreshold()  const { return static_cast<float>(dnThreshold) / 100.0F   ; }
    int getDetectionRange() const { return detectionRange        ; }
    int getDumps()          const { return genDumps    ; }
    int getScreenshots()    const { return genScreenshots; }
    int getAvgdScans()      const { return avgdScans   ; }
    int getJoinTime()       const { return joinTime      ; }

    //storage settings
    int getEraseLogs()      const { return eraseLogs     ; }
    int getEraseShots()     const { return eraseShots    ; }
    int getOverwriteSnap()  const { return overwriteSnap ; }
    int getDBsnap()         const { return DBsnap        ; }
    int getDataLasting()    const { return dataLasting   ; }
    int getImgLasting()     const { return imgLasting    ; }
    float getMinFree()      const { return minFree       ; }

    //preferences
    int getMaxEventLasting() const { return maxEventLasting; }
    int getHz()             const { return hzTickMarks   ; }
    int getDbfs()           const { return dBfsTickMarks  ; }
    int getSec()            const { return wfSecTickMarks ; }
    int getBoundaries()     const { return wfRangeBoundaries ; }
    int getTooltips()       const { return tooltips     ; }
    int getPing()           const { return pingOn   ; }
    int getFaultSound()     const { return faultSoundOn   ; }
    int getPalette()        const { return palette   ; }
    int getThresholdsBehavior()     const { return thresholdsBehavior; }
    int getNoiseLimit()             const { return noiseLimit   ; }
    int getDbfsOffset()             const { return dbFSoffset   ; }
    float getDbfsGain()             const { return dbFSgain   ; }
    int getServerPort()             const { return serverPort   ; }
    QHostAddress getServerAddress() const { return serverAddr; }
    int getDATdumps()       const { return createDATdumps; }
    int getDatagramDelay()  const { return datagramDelay; }
    int getScanMode()        const { return scanMode       ; }


    //waterfall window
    int getOffset()         const { return wfOffset      ; }
    int getZoom()           const { return wfZoom        ; }
    int getEccentricity()   const { return wfEccentricity; }
    int getBrightness()     const { return brightness    ; }
    int getContrast()       const { return contrast      ; }
    int getPowerZoom()      const { return powerZoom     ; }
    int getPowerOffset()    const { return powerOffset   ; }
    bool getPlotS()          const { return plotS         ; }
    bool getPlotN()          const { return plotN         ; }
    bool getPlotD()          const { return plotD         ; }
    bool getPlotA()          const { return plotA         ; }
    bool getPlotU()          const { return plotU         ; }
    bool getPlotL()          const { return plotL         ; }

    QList<Notch> getNotches() const { return notches; }

    //volatile flags:
    int isVacuuming() const
    {
        return vacuumStatus;
    }

    int isConsoleMode() const
    {
        return consoleMode;
    }

    int isDumbMode() const
    {
        return dumbMode;
    }

    int isAudioDevice() const
    {
        return audioDevice;
    }

    int isNetDevice() const
    {
        return netDevice;
    }

    int isTestDevice() const
    {
        return testDevice;
    }


    bool isLoading() const
    {
        return loading;
    }

    //save/load local settings file only
    void loadLocal();
    void saveLocal();

    //local settings
    QDate getLastDBsnapshot() const { return QDate::fromString( lastDBsnapshot, "yyyy-MM-dd" );  }
    void setLastDBsnapshot()
    {
        lastDBsnapshot = currentUTCdate().toString("yyyy-MM-dd");
    }

    void setWfGeometry(QRect newWwGeom, QRect newWwPaneGeom, bool enableNotifications = true);
    QRect getWwGeometry()   const { return wwGeom        ; }

    QRect getWwPaneGeometry()   const { return wwPaneGeom     ; }

    void setMainGeometry    ( QRect newGeom);
    QRect getMainGeometry   () const { return geom          ; }

    QString getLogo         () const { return logo      ; }
    void  setLogo           ( QString logoPix );

    int getPeakIntervalFreq();



    //utilities

    ///
    /// \brief fpCmpf compares two floats up to the
    /// required decimals
    /// \param val1
    /// \param val2
    /// \param decimals
    /// \return 0: val1==val2, <0: val1<val2, >0: vali1>val2
    ///
    int fpCmpf( float val1, float val2, int decimals=3 );

    ///
    /// \brief fpCmp compares two doubles up to the
    /// required decimals
    /// \param val1
    /// \param val2
    /// \param decimals
    /// \return
    ///
    int fpCmp( double val1, double val2, int decimals=5 );


    ///
    /// \brief fuzzyCompare
    /// \param a
    /// \param b
    /// \param tolerance (percentage)
    /// \return 1 if a>b,
    ///         -1 if a<b,
    ///         0 if a-b is within +/-tolerance % of a,
    ///
    int fuzzyCompare(float a, float b, float tolerance);



    ///
    /// \brief funcGen
    /// \param func
    /// \param xInput
    /// \return
    /// table lookup function generator
    ///
    float funcGen( QMap<float,float>* func, float xInput);


    ///
    /// \brief lowestElem
    /// \param elems
    /// \return
    /// returns the lowest element in a list
    ///
    float lowestElem( QList<float>* elems );


    ///
    /// \brief highestElem
    /// \param elems
    /// \return
    /// returns the highest element in a list
    ///
    float highestElem( QList<float>* elems );

    ///
    /// \brief fromStrVect
    /// \param strvec
    /// \return
    ///
    QStringList fromStrVect(std::vector<std::string> &strVec );

    ///
    /// \brief fromRangeVect
    /// \param rangeVec
    /// \return
    ///
    QStringList fromRangeVect(SoapySDR::RangeList& rangeVec, int precision = 0, int div = 1, bool compact = false);


    ///
    /// \brief isIntValueInList
    /// \param strList - can be a list of ranges or a list of values
    /// \param target integer value (freq, bw...)
    /// \return the list index pointing a value or a range including the target
    ///
    int isIntValueInList(QStringList& strList, QString target );

    ///
    /// \brief isFloatValueInList
    /// \param strList - can be a list of ranges or a list of values
    /// \param target float value (gain...)
    /// \return the list index pointing a value or a range including the target
    ///
    int isFloatValueInList(QStringList& strList, QString target );

    ///
    /// \brief getClosestValueFromList
    /// \param strList - can be a list of ranges or a list of values
    /// \param target integer value (freq, bw...)
    /// \return the list index pointing a value closest to target and not higher than it
    ///
    int getClosestIntValueFromList(QStringList& strList, QString target);

    ///
    /// \brief getClosestValueFromList
    /// \param strList - can be a list of ranges or a list of values
    /// \param target float value (gain...)
    /// \return the list index pointing a value closest to target and not higher than it
    ///
    int getClosestFloatValueFromList(QStringList& strList, QString target);

    ///
    /// \brief thresholdBehaviorString
    /// \return
    ///
    QString thresholdBehaviorString();    ///absolute, differential, automatic

    ///
    /// \brief clearChanged
    /// resets the settings changed flag
    void clearChanged()
    {
        changed = 0;
        srvChanged = 0;
    }


    ///
    /// \brief updateRTSformatRev
    /// \return
    ///
    int updateRTSformatRev();

    ///
    /// \brief getWaterfallWindow
    /// \return
    /// The class Settings holds the pointer of windows (no disk storage)
    Waterfall* getWaterfallWindow() const
    {
        return ww;
    }

    ///
    /// \brief setWaterfallWindow
    /// \param w
    ///
    void setWaterfallWindow(Waterfall* w)
    {
        MY_ASSERT( nullptr !=  w)
        ww = w;
    }

    //The class Settings holds the pointer of windows (no disk storage)
    MainWindow* getMainWindow() const
    {
        return mw;
    }

    ///
    /// \brief setMainWindow
    /// \param w
    ///
    void setMainWindow(MainWindow* w)
    {
        MY_ASSERT( nullptr !=  w)
        mw = w;
    }


    ///
    /// \brief pround
    /// \param x
    /// \param precision
    /// \return
    /// generic rounding function
    ///
    float pround(float x, int precision = 1);

    //
    ///
    /// \brief iround
    /// \param x
    /// \return
    /// generic rounding function
    ///
    inline int iround(float x)
    {
        return static_cast<int>( pround(x,0) );
    }

    ///
    /// \brief roundUp
    /// \param numToRound
    /// \param multiple
    /// \return
    ///
    int roundUp(int numToRound, int multiple);


    ///
    /// \brief getIntDivider
    /// \param numToDiv
    /// \param divCeil
    /// \return the integer divider of numToDiv not less than divCeil
    ///
    int getIntDivider( int numToDiv, int divCeil );

    void setVacuuming(int status)
    {
        vacuumStatus = status;
    }

    ///
    /// \brief setConsoleMode
    /// \param c
    ///
    void setConsoleMode(int c)
    {
        consoleMode = c;
    }

    ///
    /// \brief setDumbMode
    /// \param d
    ///
    void setDumbMode(int d)
    {
        dumbMode = d;
    }


    ///
    /// \brief setAudioDevice
    /// \param d
    ///
    void setAudioDevice(int dev, bool newOne = false)
    {
        audioDevice = dev;
        netDevice = false;
        testDevice = false;

        if(audioDevice)
        {
            //also some default settings must be changed:
            tune = 0;

            if(newOne)
            {
                //just switched to audio device without loading
                //a rts
                sampleRate = 96000;
                dsBw = sampleRate;
                fftBins = 1024;
                resolution = qint64(172.266 * 1e8);
            }
        }
    }

    ///
    /// \brief setNetDevice
    /// \param d
    ///
    void setNetDevice(int d)
    {
        netDevice = d;
        //audioDevice = false;
        //testDevice = false;
    }


    ///
    /// \brief setTestDevice
    /// \param d
    ///
    void setTestDevice(int d)
    {
        testDevice = d;
        audioDevice = false;
        netDevice = false;
    }




    ///
    /// \brief setHpp
    /// \param hpp
    /// Hz per pixel (waterfall resolution)
    void setHpp(float hpp)
    {
        hzPerPixel = hpp;
    }


    ///
    /// \brief getHpp
    /// \return
    ///
    float getHpp() const
    {
        return hzPerPixel;
    }

    ///
    /// \brief getFracPpmErr
    /// \return
    ///
    void getZoomedRange( int& lo, int& hi ) const
    {
        lo = zoomLo; hi = zoomHi;
    }

    ///
    /// \brief getMinDbfs
    /// \return
    ///
    float getMinDbfs()
    {
        return static_cast<float>( dbFSoffset * 1.5 );
    }

    ///
    /// \brief getMaxDbfs
    /// \return
    ///
    float getMaxDbfs()
    {
        return static_cast<float>( dbFSoffset+DBFS_SCALE );
    }

    bool isSwapTime()
    {
        QTime utc = currentUTCtime();

#ifdef WANT_FULL_UTC_DELTA
        //note: seconds MUST be ignored
        if (utc.hour() == 0 &&
            utc.minute() == 0)
#else
        if (utc.hour() == UTCdelta.hour() &&
            utc.minute() == UTCdelta.minute())
#endif
        {
            return true;
        }
        return false;
    }


    ///
    /// \brief currentUTCtime
    /// \return current time UTC added with UTCdelta
    ///
    QTime currentUTCtime()
    {
        QTime utc = currentUTCdateTime().time();
        return utc;
    }

    ///
    /// \brief currentUTCdate
    /// \return current date UTC added with UTCdelta
    ///
    QDate currentUTCdate()
    {
        QDate utc = currentUTCdateTime().date();
        return utc;
    }

    ///
    /// \brief currentUTCdateTime
    /// \return current datetime UTC added with UTCdelta
    ///
    QDateTime currentUTCdateTime()
    {
        QDateTime utc = QDateTime::currentDateTimeUtc();
#ifdef WANT_FULL_UTC_DELTA
        utc = utc.addSecs(UTCdelta);
#endif
        return utc;
    }

    ///
    /// \brief getUTCdelta
    /// \return UTCdelta as date string
    ///
    QString getUTCdelta()
    {
#ifdef WANT_FULL_UTC_DELTA
        QTime delta(0,0);
        return delta.addSecs(UTCdelta).toString();
#else
        return UTCdelta.toString();
#endif
    }

    ///
    /// \brief getMaxResHz
    /// \return
    ///
    double getMaxResHz() const
    {
        return maxResHz;
    }

    ///
    /// \brief getMaxResMTU
    /// \return
    ///
    int getMaxResMTU() const
    {
        return maxResMTU;
    }

    ///
    /// \brief roundEccentricity
    ///
    void roundEccentricity();

    //setters
    //note: the slots provided with <enableNotifications> argument have been called both as slots
    //and as methods. In the first case the notifications should be inhibited to avoid unvanted recursions


    //mainwindow settings
    void setConfigName( XQDir wd, QString& name );
    void setZoomedRange( int lo, int hi );

    //device settings
    void setDevice          ( const QString &dev, bool enableNotifications = true );
    void setFreqRange       ( QString value, bool enableNotifications = true );
    void setBandwidthRange  ( QString value, bool enableNotifications = true  );
    void setSampleRateRange ( QString value, bool enableNotifications = true  );
    void setSampleRate      ( int value, bool enableNotifications = true );
    void setBandwidth       ( int value, bool enableNotifications = true );
    void setAntenna         ( QString value, bool enableNotifications = true );
    void setTune            ( qint64 value, bool enableNotifications = true );
    void setGain            ( int value, bool enableNotifications = true  );
    void setError           ( double value, bool enableNotifications = true );

    //FFT settings:
    void setSsBypass        ( int value, bool enableNotifications = true );
    bool setDsBandwidth     ( int value, bool enableNotifications = true );
    void setWindow          ( int value, bool enableNotifications = true );
    bool setResBins         ( int FFTbins, bool enableNotifications = true );
    bool updateResolution   ( bool enableNotifications = true  );
    void setMaxRes          ( double mtu );
    
	//FFT resolution pushbuttons:
    bool incResolution      (bool enableNotifications = true);
    bool decResolution      (bool enableNotifications = true);

    //output settings
    void setAcqMode         (int value , bool enableNotifications = true );
    void setInterval        (int value , bool enableNotifications = true );
    void setShots           (int value , bool enableNotifications = true );
    void setRecTime         (int value , bool enableNotifications = true );
    void setAfter           (int value , bool enableNotifications = true );
    void setThresholdsBehavior(int value , bool enableNotifications = true);
    void setUpThreshold     (double value, bool enableNotifications = true);
    void setDnThreshold     (double value, bool enableNotifications = true);
    void setDetectionRange  (int value , bool enableNotifications = true);
    void setDumps           ( int value );
    void setScreenshots     ( int value );
    void setAvgdScans       ( int value );
    void setJoinTime        ( int value );
    void setPalette         ( int value );
    void setNoiseLimit      ( int value  );

    //storage settings
    void setEraseLogs       ( int value );
    void setEraseShots      ( int value );
    void setOverwriteSnap   ( int value );
    void setDBsnap          ( int value );
    void setDataLasting     ( int value );
    void setImgLasting      ( int value );
    void setMinFree         ( int value );


    //preferences
    void setMaxEventLasting ( int value );
    void setHz              ( int value, bool enableNotifications = true );
    void setDbfs            ( int value, bool enableNotifications = true );
    void setSec             ( int value, bool enableNotifications = true );
    void setRangeBoundaries ( int value, bool enableNotifications = true );
    void setTooltips        ( int value );
    void setPing            ( int value );
    void setFaultSound      ( int value );
    void setDbfsOffset      ( int value,    bool enableNotifications = true   );
    void setDbfsGain        ( double value, bool enableNotifications = true   );
    void setServerPort      ( int value  );
    void setServerAddress   ( const QString& ipAddr  );
    void setDATdumps        ( int value );
    void setDatagramDelay   ( int value );
    void setScanMode        ( int value, bool enableNotifications = true );


    //waterfall window
    void setOffset          (int value , bool enableNotifications = true);
    void setZoom            (int value , bool enableNotifications = true);
    void setEccentricity    (int value , bool enableNotifications = true);
    void setBrightness      ( int value );
    void setContrast        ( int value );
    void setPowerZoom       ( int value );
    void setPowerOffset     ( int value );
    void setNotches         ( QList<Notch>& ntList);

    void setPlotS           (bool value , bool enableNotifications = true);
    void setPlotN           (bool value , bool enableNotifications = true);
    void setPlotD           (bool value , bool enableNotifications = true);
    void setPlotA           (bool value , bool enableNotifications = true);
    void setPlotU           (bool value , bool enableNotifications = true);
    void setPlotL           (bool value , bool enableNotifications = true);




signals:

    /*
     * notification signals are required only for controls having an immediate
     * feedback even when acquisition is running
     */

    void notifyOpened           (const QString*);       ///configuration loaded from disk, GUI needs refresh.
    void notifySavedAs          ();
    void notifySaved            ();
    void notifySetWfGeometry    ();
    void notifySetOffset        ();
    void notifySetZoom          ();
    void notifySetEccentricity  ();


    //device settings:
    void notifySetSampleRate    ();
    void notifySetBandwidth     ();
    void notifySetTune          ();
    void notifySetError         ();
    void notifySetDevice        ();
    void notifySetGain          ();
    void notifySsBypass         ();
    void notifySetAntenna       ();
    void notifySetFreqRange     ();
    void notifySetBandwidthRange ();
    void notifySetSampleRateRange ();

    //FFT settings
    void notifySetDsBandwidth   ();
    void notifySetResolution    ();
    void notifySetWindow        ();

    //output settings
    void notifySetAcqMode       ();
    void notifySetInterval      ();
    void notifySetUpThreshold   ();
    void notifySetDnThreshold   ();
    void notifySetDetectionRange();
    void notifySetAfter         ();
    void notifySetRecTime       ();
    void notifySetShots         ();
    void notifySetThresholdsBehavior();

    //preferences settings
    void notifySetHz            ();
    void notifySetDbfs          ();
    void notifySetSec           ();
    void notifyRangeBoundaries  ();
    void notifyDbfsOffset       ();
    void notifyDbfsGain         ();
    void notifySetScanMode      ();

    //waterfall settings
    void notifySplot            ();
    void notifyNplot            ();
    void notifyDplot            ();
    void notifyAplot            ();
    void notifyUplot            ();
    void notifyLplot            ();
};

#endif // SETTINGS_H
