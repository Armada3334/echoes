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
$Rev:: 369                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-24 10:01:36 +01#$: Date of last commit
$Id: waterfall.h 369 2018-11-24 09:01:36Z gmbertani $
*******************************************************************************/

#ifndef WATERFALL_H
#define WATERFALL_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtCharts>

#include "settings.h"
#include "ruler.h"
#include "waterfallwidget.h"

QT_CHARTS_USE_NAMESPACE

class DbmPalette;
class Preferences;
class Control;

namespace Ui {
    class Waterfall;
}


/**
 * @brief The LUTentry class
 * downsampling lookup table entry
 */
struct LUTentry
{
    float dsBw;
    float ecc;
    float lowest;
    float highest;

};

/**
 * @brief The ScanInfos class
 *
 * auxiliary information accompanying each scan
 *
 */
struct ScanInfos
{
    QString timeStamp;
    float maxDbm;
    float avgDbm;
    float avgDiff;
    float upThr;
    float lowThr;
    bool raisingEdge;
};



class MainWindow;

class Waterfall : public QWidget
{
    Q_OBJECT

private:
    Ui::Waterfall *ui;          ///Waterfall window form

protected:

    LUTentry dsLUT[MAX_DOWNSAMPLING_VALUES];

    QLocale loc;
    QRecursiveMutex  protectionMutex;    ///<generic mutex to protect against concurrent calls from Control thread

    Settings* as;               ///current application settings
    MainWindow* m;              ///parent window

    Control*    ac;             ///application control

    WaterfallWidget* ww;        ///Waterfall widget
    DbmPalette* dp;             ///color/value palette
    Ruler* hr;                  ///horizontal (fixed) frequency ruler

    bool init;                  ///set in constructor: some code must be executed only when called by constructor
    bool sigBlock;              ///signals blocked if true
    bool historyCleanNeeded;    ///workaround for history plot progressive shortening bug


    int samplerate;             ///last known sampling rate
    int dsBandwidth;            ///last known downsampled bandwidth
    int tuneOffset;             ///last known tune offset
    int tuneZoom;               ///last known tune zoom

    int maxBW;                  ///maximum bw alloweed = SR

    int zoomCoverage;           ///relative frequency coverage of waterfall at each side of central frequency

    int offsetCoverage;         ///relative excursion of offset slider at each side of tune frequency
    int eccentricity;           ///X position of the central frequency red marker

    int minOff;                 ///offset slider absolute frequency excursion start [hz]
    int maxOff;                 ///offset slider absolute frequency excursion end [hz]

    int zoomLo;                 ///minimum absolute frequency showed in waterfall [Hz]
    int zoomHi;                 ///maximum absolute frequency showed in waterfall [Hz]

    int lowest;                 ///minimum absolute frequency showable in waterfall with zoom = 1 [hz]  When BW == SR equals to fromHz
    int highest;                ///maximum absolute frequency showable in waterfall with zoom = 1 [hz]  When BW == SR equals to toHz

    int lastTune;               ///last tune showed
    int intervalHz;               ///peak detection area's central frequency

    int fTicks;                 ///frequency grid ticks
    int tTicks;                 ///time grid ticks
    uint yScans;                 ///number of rows plotted in the waterfall since acquisition start

    int palBeg;                 ///color palette begin value [dBfs]
    int palEnd;                 ///color palette end value [dBfs]
    int palExc;                 ///color palette extension [dBfs]

    int powBeg;                 ///lowest power value shown on graphs [dBfs]
    int powEnd;                 ///highest power value shown on graphs [dBfs]

    int storedS;                /// S value to show in replay
    int storedN;                /// N value to show in replay
    int storedDiff;             /// S-N value to show in replay
    int storedAvgDiff;          /// average S-N value to show in replay
    qreal storedLowThr;           /// low threshold to show in replay
    qreal storedUpThr;            /// up threshold to show in replay

    int scanPixel;              /// scan current pixel
    int scanLen;                /// number of pixels composing a scan

    float bpp;                  /// waterfall bins per pixel
    float binSize;              /// resolution in Hz of a bin coming from Control

    float leftBinDbfs;          /// residual power to be averaged with next bin
    float pixPowerSum;          /// case N:1 summing bins to fit a pixel, see appendPixels(),  case N:1
    float pixRemainder;         /// splitting last bin to fit a pixel - remainder of last processed pixel, see appendPixels(),  case N:1
    float binRemainder;         /// splitting last bin to fit multiple pixels, case 1:N


    QQueue<QList<QPointF>> qScanList;   ///queue of pixels scans composed by appendPixel()
    QQueue<ScanInfos> qScanInfos;        ///queue of scan infos structure composed by appendInfos()

    QList<QPointF> busyScanList;        ///scan currently under construction by appendPixel()

    QList<QPointF> storedList;  ///saved scanList to replay the peaks in shots

    QVector<int> tilt;          ///zoom vs. graphs tick interval lookup table

    ///instantaneous spectra graph
    QChartView* hcv;            ///istantaneous spectra graph view
    QChart *hChart;             ///istantaneous spectra graph
    QValueAxis *haX, *haY;      ///istantaneous spectra graph axis


    QChartView* vcv;            ///power history graph view
    QChart *vChart;             ///power history graph
    QValueAxis *vaX, *vaY;      ///power history graph axis

    QLineSeries* vSerieS;       ///peak power (S) vs. time data serie
    QLineSeries* vSerieN;       ///average power (N) vs. time data serie
    QLineSeries* vSerieD;       ///difference vs. time data serie
    QLineSeries* vSerieA;       ///average difference vs. time data serie
    QLineSeries* vSerieU;       ///upper threshold vs. time data serie
    QLineSeries* vSerieL;       ///lower threshold vs. time data serie

    QLineSeries* hSerie;        ///power vs. frequency (scan) data serie


    virtual void resizeEvent ( QResizeEvent* event );
    virtual void moveEvent ( QMoveEvent * event );

    ///
    /// \brief calcSliderStep
    /// \param base
    /// \return
    ///
    int calcSliderStep(int base, int limit);

    ///
    /// \brief getBpp
    /// \param displayedBins
    /// \return
    ///
    float getBpp(int displayedBins);  ///retrieves the waterfall resolution: bins per pixel

public:
    explicit Waterfall(Settings* appSettings, Control* appControl, MainWindow* mainWnd, QWidget *parent = nullptr);
    ~Waterfall();



    ///
    /// \brief GUIinit
    ///
    void GUIinit();

    ///
    /// \brief blockAllSignals
    /// \param block
    ///
    void blockAllSignals(bool block);



    ///
    /// \brief scansNumber
    /// \return
    ///
    int scansNumber() const
    {
        return ww->scans();
    }


    ///
    /// \brief getWidgetSize
    /// \return
    ///
    QSize getWidgetSize();

    ///
    /// \brief appendPixels
    /// \param binDbfs
    /// compose a scan and pushes it into qScanList
    ///
    /// \return true = list complete
    ///
    bool appendPixels(float binDbfs, bool begin, bool end);

    ///
    /// \brief appendInfos
    /// \param timeStamp
    /// \param maxDbm
    /// \param avgDbm
    /// \param avgDiff
    /// \param upThr
    /// \param lowThr
    /// \param raisingEdge
    /// compose a ScanInfos structure and pushes it into qScanInfo;
    ///
    /// \return true = push ok
    ///
    bool appendInfos(QString timeStamp, float maxDbm, float avgDbm, float avgDiff, float upThr, float lowThr, bool raisingEdge);

public slots:

    ///
    /// \brief cleanAll
    ///
    void slotCleanAll();

    ///
    /// \brief slotSetBrightness
    /// \param value
    ///
    void slotSetBrightness( int value );

    ///
    /// \brief slotSetContrast
    /// \param value
    ///
    void slotSetContrast( int value );

    ///
    /// \brief slotRuntimeChange
    ///
    void slotRuntimeChange();

    ///
    /// \brief slotShowStatus
    /// \param appState
    ///
    void slotShowStatus( int appState );

    ///
    /// \brief slotReplay
    ///
    void slotReplay();

    ///
    /// \brief slotUpdateGrid
    void slotUpdateGrid();

    ///
    /// \brief slotSetOffset
    /// \param value
    ///
    void slotSetOffset( int value );

    ///
    /// \brief slotSetEccentricity
    /// \param value
    ///
    void slotSetEccentricity();

    ///
    /// \brief slotSetZoom
    /// \param value
    ///
    void slotSetZoom( int value );


    ///
    /// \brief slotRefresh
    ///
    void slotRefresh();

    ///
    /// \brief slotStartAcq
    /// \param shotNr
    /// \param totalShots
    ///
    void slotStartAcq(int shotNr, int totalShots);

    ///
    /// \brief slotSetPowerOffset
    /// \param dbm
    ///
    void slotSetPowerOffset(int dbFs );

    ///
    /// \brief slotSetPowerZoom
    /// \param zoom
    ///
    void slotSetPowerZoom(int zoom );

    ///
    /// \brief slotCleanCharts
    ///
    void slotCleanCharts();

    ///
    /// \brief slotConfigChanged
    ///
    void slotConfigChanged();

    ///
    /// \brief slotRangeChanged
    /// \param min
    /// \param max
    ///
    //void slotRangeChanged(qreal min, qreal max);

    ///
    /// \brief slotUpdateLogo
    ///
    void slotUpdateLogo();


    ///
    /// \brief slotSetCapturing
    /// \param isInhibited
    /// \param isCapturing
    ///
    void slotSetCapturing(bool isInhibited, bool isCapturing);

    ///
    /// \brief slotSetScreenshotCoverage
    /// \param secs
    ///
    void slotSetScreenshotCoverage(int secs);

    ///
    /// \brief slotTakeShot
    /// \param shotNr
    /// \param totalShots
    /// \param maxDbm
    /// \param avgDbm
    /// \param avgDiff
    /// \param upThr
    /// \param lowThr
    /// \param shotFileName
    ///
    void slotTakeShot(int shotNr, int totalShots, float maxDbm, float avgDbm, float avgDiff, float upThr, float lowThr,
                      QString shotFileName = "unnamed.png" );

    ///
    /// \brief slotUpdateCounter
    /// \param shotNr
    /// \param totalShots
    ///
    void slotUpdateCounter(int shotNr, int totalShots);

    ///
    /// \brief slotMainWindowReady
    ///
    void slotMainWindowReady();

    ///
    /// \brief slotResetEccentricity
    ///
    void slotResetEccentricity();

signals:

    ///
    /// \brief waterfallResized
    ///
    void waterfallResized();
};

#endif // WATERFALL_H
