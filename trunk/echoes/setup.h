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
$Rev:: 381                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-12-20 08:42:10 +01#$: Date of last commit
$Id: setup.h 381 2018-12-20 07:42:10Z gmbertani $
*******************************************************************************/


#ifndef SETUP_H
#define SETUP_H

#include <QtCore>

enum LOG_LEVELS
{
    ECHOES_LOG_NONE,               // do not create a program log
    ECHOES_LOG_FATAL,              // only fatal messages (crashes) are logged
    ECHOES_LOG_CRITICAL,           // logs fatal and critical messages (alerts about possible crashes)
    ECHOES_LOG_WARNINGS,           // logs warnings too (the program does not behave as expected)
    ECHOES_LOG_INFO,               // logs info messages too

    ECHOES_LOG_ALL                 // logs everything including debug messages (huge logs)
};


//final application name
#define TARGET_NAME             "echoes"
#define TARGET_DESC             "Echoes is a RF spectrograph for SDR devices designed for meteor scatter"
#define COMPANY_NAME            "GABB"

#define FULL_CONSOLE_APPNAME    "console_echoes"

#ifdef _DEBUG
#undef APP_VERSION
#undef BUILD_DATE
#undef COMPILER_BRAND
#endif


#ifndef BUILD_DATE
#define BUILD_DATE "2065/12/08 00:00:00"
#endif

#ifndef APP_VERSION
#define APP_VERSION "00.00"
#endif 


#ifndef COMPILER_BRAND
#define COMPILER_BRAND "unknown"
#endif 


//online manual url
//#define ONLINE_MANUAL_URL       "https://echoes.sourceforge.io"
#define ONLINE_MANUAL_URL       "http://www.gabb.it/echoes"

//seconds before terminating stale threads
#define KILL_TIMEOUT           3

//CSV columns separator
#define CSV_SEP                 ';'

//Automatic shots prefix
#define AUTOSHOT_PREFIX        "autoshot_"
//Manual shots prefix
#define MANSHOT_PREFIX         "manshot_"
//Dump file prefix
#define DUMP_PREFIX         "dump_"

//Continuous data file prefix
#define DATA_PREFIX         "data_"

//CSV statistic data file prefix
#define CSV_PREFIX              "scan_"

//configuration files extension
#define CONFIG_EXT              ".rts"

//table files extension
#define TABLE_EXT               ".csv"

//dump files extension, ASCII gnuplot format
#define GP_DATA_EXT              ".dat"

//dump files extension, binary format
#define DUMP_DATA_EXT            ".datb"


//screenshot images extensions
#define SSHOT_EXT               ".png"

//echoes internals statistic plot
#define INNER_STATISTIC_PLOT    TARGET_NAME+QString("-")+as->getConfigName()+TABLE_EXT //GP_DATA_EXT

//sqlite3 model database
#define DB_MODEL                "echoes.sqlite3"

//maximum noise alloweed for automatic capturing (0=unlimited)
#define DEFAULT_NOISE_LIMIT     (0)

//dBm calibration: offset applied to power values from FFT
#define DEFAULT_DBFS_OFFSET     (MIN_DBFS)

//dBm calibration: gain applied to power values from FFT
#define DEFAULT_DBFS_GAIN       (1.0)

//direct buffers acquisition delay calibration interval
#define DB_DELAY_CALIB_COUNT    (1000)

//direct buffers acquisition delay calibration
//maximum number of accepted undeflows every DB_DELAY_CALIB_COUNT forwarded buffers
#define DB_DELAY_ACCEPT_LIMIT   (10)

//dongle server port
#define DEFAULT_SERVER_PORT     (12345)

//dongle server address
#define DEFAULT_SERVER_ADDRESS  "0.0.0.0"

//max.UDP datagram size
#define MAX_DGRAM_SIZE 512

//max length of client / rts file names
#define MAX_NAMES_SIZE 30

//min time resolution in mS
#define TIME_BASE               (1)

//disk space checking cadence mS
#define SYS_CHECK_INTERVAL      (30000)

//minimum storage for circular buffers
#define CIRCBUF_MIN             (15)

//samples array pool size:
#define DEFAULT_POOLS_SIZE      (2)

//scans array pool size
#define SCAN_POOL_SIZE          (2)

//maximum bandwidth of the samples coming from dongle (bandwidth)
#define MAX_BANDWIDTH_HZ         (10000000)

//maximum number of FFT bins = 2^20
#define MAX_FFT_BINS            (1048576)

//maximum input buffer size
#define MAX_SAMPLE_BUF_SIZE     (MAX_FFT_BINS)

//max alloweed downsamplig rates
#define MAX_DOWNSAMPLING_VALUES (5)

//speed of light [m/s]
#define SPEED_OF_LIGHT          (299792458)

//step increment for frequency rulers [hz]
#define HORZ_MIN_TICKS_STEP     (100)

//dBfs values scale start at
#define MIN_DBFS                (-130.0)

//dBfs scale excursion
#define DBFS_SCALE              (150.0)

//dBfs values scale ends at
#define MAX_DBFS                (MIN_DBFS+DBFS_SCALE)

//invalid dBfs value
#define INVALID_DBFS            (-1000.0F)

//the device ceased to stream samples
#define MAX_MISSED_SCANS        (150)


//internal I/Q buffers base type
//librtlsdr: use uint8_t
//soapy: CS16 ( liquid_int16_complex )
//#define int16_t           int16_t
#define STREAMTYPE          SOAPY_SDR_CS16

//handling deprecations added in Qt5.14
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
#define MY_ENDL Qt::endl
#define MY_HEX  Qt::hex
#define MY_DEC  Qt::dec
#define MY_POSF position()
#else
#define MY_ENDL endl
#define MY_HEX hex
#define MY_DEC dec
#define MY_POSF posF()
#endif

//dBfs color scale delta between values [dBfs]
#define DEFAULT_DBFS_STEP       (15)

//Waterfall widget frequency axle max number of major ticks - radio spectra
#define WF_FREQ_TICKS           (5)

//Waterfall widget frequency axle max number of major ticks - audio spectra
#define WF_AUDIO_FREQ_TICKS     (6)

//Waterfall widget time axle ticks
#define WF_TIME_TICKS           (20)

//scrolling rulers: min nr. of ticks between lines
#define MIN_SCROLLING_RULER_TICKS   (10)

//max number of items that can be inserted in a combobox values list
#define MAX_ITEMS_IN_LISTS      (40)

//session update cadence [s]
#define SESS_UPD_CADENCE        (300)

//offset from scan begin and scan end to detect ESD fake events
#define SCAN_ESD_OFFSET         (20)

//magic number for IQbuf files
#define IQDUMP_MAGIC            (0xAA55F00F)

//sync word for IQbuf UDP transfers
#define SYNC_IQBUF              (0x2015)

//sync word for server configuration UDP transfers
#define SYNC_SRVCONF            (0x6a21)

//dclient send ALIVE every x scans
#define KEEPALIVE_RATE          20

//the server waits the ALIVE from clients for scans
#define KEEPALIVE_TOUT          600

//limits to x characters the device ID string length
#define MAX_DEVICE_ID_LEN       48

//max queued queries
#define MAX_QUEUED_QUERIES      1000

//minimal iqBuf size
#define BUFFER_BLOCKSIZE        512

//minimum lasting for join [s]
#define MIN_JOIN_LASTING        1

//limits the WAL file size (default is 1000)
#define MAX_DB_WAL_PAGES        100

//hardcoded default settings:

//RTS file format revision: 0 = older than version 0.51
#define CURRENT_RTS_FORMAT_REV  2

#define DEFAULT_CONFIG_NAME     "default"
#define DEFAULT_MAINWND_GEOM    QRect(100,100,340,750)

//device settings
#define DEFAULT_DEVICE          "Test patterns"
#define DEFAULT_RATE            100000              //generic value supported by most devices
#define DEFAULT_BW              DEFAULT_RATE
#define DEFAULT_TUNE            (50000)             //hz
#define DEFAULT_GAIN            (100)               //default RF gain: 100% manual
#define DEFAULT_PPMERR          (0)                 //ppm/100
#define AUTO_GAIN               (-1)                //placeholder for AGC on


//FFT settings:
#define DEFAULT_DSBW                DEFAULT_RATE    //hz
#define DEFAULT_WINDOW              (0)               //FTW_RECTANGLE
#define DEFAULT_FFT_BINS            (4096)            // default FFT points
#define DEFAULT_RESOLUTION          "24.4140625"           //Hz
#define DEFAULT_BIN_SIZE            (24.4140625F)          //Hz

#define DEFAULT_SSBYPASS            (Qt::Checked)
#define DEFAULT_ANTENNA             "None"
#define DEFAULT_FREQ_RANGE          "1000 : 100000"
#define DEFAULT_BW_RANGE            "1000 : 100000"
#define DEFAULT_SR_RANGE            "1000 : 100000"

//output settings:
#define DEFAULT_ACQ_MODE            (1)               //continuous
#define DEFAULT_INTVL               (100)             //ms
#define DEFAULT_MAX_SHOTS           (0)               //0=continuous mode
#define DEFAULT_RECTIME             (0)               //sec
#define DEFAULT_AUTO_AFTER          (0)               //sec
#define DEFAULT_THRESHOLDS          (0)               //diffential thresholds (default for Echoes <= 0.25)
#define DEFAULT_AUTO_UP_THRESHOLD   (0)               //power peaks detection threshold S-N [dBfs/100]
#define DEFAULT_AUTO_DN_THRESHOLD   (0)               //below this threshold, the peak is considered terminated [dBfs/100]
#define DEFAULT_AUTO_TRANGE         (20)              //power peaks detection frequency range [%]
#define DEFAULT_DUMPS               Qt::Unchecked     //do not produce dump files
#define DEFAULT_SCREENSHOTS         Qt::Checked       //always produce screenshots
#define DEFAULT_AI_FEEDS            Qt::Unchecked     //AI input files generation must be enabled explicitly
#define DEFAULT_N_SCANS             (50)              //number of averaged scans to calculate N
#define DEFAULT_JOIN_TIME           (1000)            //minimum time between consecutive events in ms


//preferences
#define DEFAULT_MAX_EVENT_LASTING   (50)
#define DEFAULT_WSEC_TICKS          Qt::Checked      //showed
#define DEFAULT_HZ_TICKS            Qt::Checked      //showed
#define DEFAULT_DBFS_TICKS          Qt::Checked      //showed
#define DEFAULT_RANGE_BOUNDARIES    Qt::Checked      //showed
#define DEFAULT_TOOLTIPS            Qt::Unchecked    //tooltips showed
#define DEFAULT_PING                Qt::Checked      //ping sound enabled
#define DEFAULT_FAULT_SOUND         Qt::Checked      //fault sound enabled
#define DEFAULT_WF_PALETTE          0                //waterfall palette: 0=b/w, 1=color
#define DEFAULT_DAT_DUMPS           Qt::Unchecked    //create DATB binary dumps
#define DEFAULT_UTC_DELTA           "00:00:00"       //delta seconds added to UTC time (to debug date related bugs)
#define DEFAULT_DGRAM_DELAY         0                //no delay added after UDP datagram sending
#define DEFAULT_SCAN_INTEGRATION    (1)              //shows the newest data available per scan

//storage settings
#define DEFAULT_OVERWRITE_SNAP      Qt::Checked
#define DEFAULT_DB_SNAP             (7)
#define DEFAULT_FREE_SPACE          (0)               //minimum space to keep free on working drive in GB, 0=no limits
#define DEFAULT_ERASE_LOGS          Qt::Unchecked   //erase
#define DEFAULT_ERASE_SHOTS         Qt::Unchecked   //erase
#define DEFAULT_DATA_LASTING        (1096)             //delete from DB the tabular data oldest than days (3 years)
#define DEFAULT_IMG_LASTING         (60)              //delete from DB the images and dumps (blobs) oldest than days
#define DEFAULT_DATE                "1970-01-01"
#define DB_WRITE_ATTEMPTS           10

//waterfall settings:
#define DEFAULT_WATERFALL_GEOM QRect(300,300,1024,768)
#define DEFAULT_OFFSET         (0)                    //hz
#define DEFAULT_ECCENTRICITY   (0)                    //hz
#define DEFAULT_ZOOM           (10)                   // [X / 10]
#define DEFAULT_DBFS_ZOOM      (50)                   //percentage
#define DEFAULT_BRIGHTNESS     (30)                   //percentage
#define DEFAULT_CONTRAST       (90)                   //percentage
#define DEFAULT_POWEROFFSET    (DEFAULT_DBFS_OFFSET / 2)
#define DEFAULT_POWERZOOM      (1)
#define DEFAULT_NOTCH_WIDTH    (10)                   //Hz
#define MAX_NOTCH_WIDTH        (99999)                //Hz
#define MANSHOT_DEBOUNCE_TICKS  (50)

#define DEFAULT_PLOT_S          (1)
#define DEFAULT_PLOT_N          (1)
#define DEFAULT_PLOT_D          (1)
#define DEFAULT_PLOT_A          (1)
#define DEFAULT_PLOT_U          (1)
#define DEFAULT_PLOT_L          (1)
#define PG_ALIGNMENT_EVERY_SCANS    (300)


//site infos - some fake data
#define DEFAULT_LOGO           ":/logo"
#define DEFAULT_LATITUDE       (+45.516667)
#define DEFAULT_LONGITUDE      (-9.583333)
#define DEFAULT_ALTITUDE       (125)
#define DEFAULT_STATION_NAME   QString("GABB")
#define DEFAULT_CONTACT        QString("gmbertani@users.sourceforge.net")
#define DEFAULT_RXSETUP        QString("-")
#define DEFAULT_NOTES          QString("-")


#define GLOBAL_STYLESHEET_FILE  ":/qss"
#define WORKING_PATH           TARGET_NAME     //subdir under $HOME
#define NOTIFY_SOUND           ":/ping"
#define FAULT_SOUND            ":/fault"
#define APP_LANG               "en"
#define IMAGE_FORMAT            QImage::Format_RGB32
#define MAX_SYS_ERRORS          (5)

#ifdef NIX
    #define LOGFILE             "/dev/log"          //journal / syslog
#else
    #define LOGFILE             TARGET_NAME         //application log file
#endif

#ifdef _MSC_VER
 #define MY_MAX_FLOAT FLT_MAX
#endif

#ifdef __GNUG__
 #define MY_MAX_FLOAT __FLT_MAX__
#endif




#define MY_ASSERT(condition) \
    if( (condition) == false ) { MYFATAL("MY_ASSERT failed at %s()", __func__); }
//    if( (condition) == false ) { MYCRITICAL << "MY_ASSERT failed"; }

//comment to avoid the waterfall to redraw the latest scan in case of missing input data
#define SCAN_REPEATING  1



#endif // SETUP_H
