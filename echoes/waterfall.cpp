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
$Id: waterfall.cpp 369 2018-11-24 09:01:36Z gmbertani $
*******************************************************************************/
#include "ui_waterfall.h"
#include "control.h"
#include "ruler.h"
#include "dbmpalette.h"
#include "mainwindow.h"
#include "waterfall.h"

#define _RECOVER_SCROLLING_BUG_EVERY_SCANS 10000

Waterfall::Waterfall(Settings* appSettings, Control* appControl, MainWindow* mainWnd, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Waterfall)
{
    setObjectName("Waterfall widget");
    ui->setupUi(this);

    init = true;

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    MY_ASSERT( nullptr !=  appControl)
    ac = appControl;

    MY_ASSERT( nullptr !=  mainWnd)
    m = mainWnd;

    qScanList.clear();
    busyScanList.clear();
    storedList.clear();

    historyCleanNeeded = false;
    tTicks = WF_TIME_TICKS;

    yScans = 0;
    scanLen = 0;
    sigBlock = false;
    samplerate = as->getSampleRate();   
    maxBW = samplerate;
    dsBandwidth = as->getBandwidth();

    zoomCoverage = dsBandwidth / 2;
    tuneZoom = DEFAULT_ZOOM;
    tuneOffset = 0;
    maxOff = 0;
    minOff = 0;
    lastTune = 0;
    intervalHz = 0;
    tilt = {0, 40, 20, 10, 10, 10, 10, 5, 5, 5, 5};

    lowest = as->getTune() - (as->getDsBandwidth() / 2);
    highest = as->getTune() + (as->getDsBandwidth() / 2);

    zoomLo = lowest;
    zoomHi = highest;

    //resetEccentricity();

    powBeg = as->getMinDbfs();
    powEnd = as->getMaxDbfs();

    storedS = MIN_DBFS;
    storedN = MAX_DBFS;
    storedDiff = 0;
    storedAvgDiff = 0;
    storedLowThr = MAX_DBFS;
    storedUpThr = MIN_DBFS;
    pixPowerSum = 0.0F;
    pixRemainder = 0.0F;
    binRemainder = 0.0F;
    leftBinDbfs = 0.0F;
    bpp = 0.0F;
    scanPixel = 0;


    //avgNow = 0;
    QString frameStyle = "border-radius: 4px;  border: 1px solid white;";

    //create a 200 colors matching a 120 dBfs range palette
    ui->subFramePalette->setFrameStyle(QFrame::Box);
    ui->subFramePalette->setStyleSheet(frameStyle);
    dp = new DbmPalette(as, DEFAULT_DBFS_STEP, ui->subFramePalette);
    MY_ASSERT( nullptr !=  dp)

    //create waterfall diagram
    //the parent is a subframe because I needed to leave some
    //space (top margin) above the diagram to align it to the power graph at right
    ui->subFrameWf->setFrameStyle(QFrame::Box);
    ui->subFrameWf->setStyleSheet(frameStyle);
    ww = new WaterfallWidget(as, dp, ui->subFrameWf);
    MY_ASSERT( nullptr !=  ww)

    //frequency ruler is horizontal not scrolling
    ui->frameFreq->setFrameStyle(QFrame::Box);
    ui->frameFreq->setStyleSheet(frameStyle);
    hr = new Ruler(as, RO_DOWN, ui->frameFreq);
    MY_ASSERT( nullptr !=  hr)

    //instantaneous spectra graph (formerly InstantSpectra class)

    hcv = new QChartView();
    MY_ASSERT( nullptr !=  hcv)

    //vertical power axis
    haY = new QValueAxis;
    MY_ASSERT( nullptr !=  haY)

    //horizontal frequency axis
    haX = new QValueAxis;
    MY_ASSERT( nullptr !=  haX)

    hSerie = new QLineSeries();
    MY_ASSERT( nullptr !=  hSerie)

    hcv->setRenderHint(QPainter::Antialiasing);
    hcv->setSceneRect(ui->wPanoramic->rect());

    hChart = hcv->chart();
    hChart->legend()->hide();

    QMargins marH(20,0,0,0);
    hChart->setMargins(marH);
    hChart->setTheme( QChart::ChartThemeDark );
    hChart->addSeries(hSerie);

    //power axis (Y)
    haY->setRange(powBeg, powEnd);    
    haY->setLabelFormat("%- 3d    ");

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
    haY->setTickType(QValueAxis::TicksDynamic);
    haY->setTickAnchor(0);
    haY->setTickInterval(tilt.at(1));
    //haY->setTickCount(5);
#else
    haY->setTickCount(5);
    haY->applyNiceNumbers();
#endif


    hChart->addAxis(haY, Qt::AlignLeft);
    hSerie->attachAxis(haY);
    //hChart->setAxisY(haY, hSerie); deprecated

    //frequency axis (X)
    int points = static_cast<int>(hChart->plotArea().width());
    haX->setRange( 0, points - 1);
    //haX->applyNiceNumbers();
    haX->hide();
    hChart->addAxis(haX, Qt::AlignBottom);
    hSerie->attachAxis(haX);

    //average power and peak power vs. time graph
    vcv = new QChartView();
    MY_ASSERT( nullptr !=  vcv)

    vSerieS = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieS)

    vSerieN = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieN)

    vSerieD = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieD)

    vSerieA = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieA)

    vSerieU = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieU)

    vSerieL = new QLineSeries();
    MY_ASSERT( nullptr !=  vSerieL)

    //vertical time axis
    vaY = new QValueAxis;
    MY_ASSERT( nullptr !=  vaY)

    //horizontal power axis
    vaX = new QValueAxis;
    MY_ASSERT( nullptr !=  vaX)

    vcv->setRenderHint(QPainter::Antialiasing);
    vcv->setSceneRect(ui->wPower->rect());

    vChart = vcv->chart();
    vChart->legend()->hide();

    vChart->addSeries(vSerieS);
    vChart->addSeries(vSerieN);
    vChart->addSeries(vSerieD);
    vChart->addSeries(vSerieA);
    vChart->addSeries(vSerieU);
    vChart->addSeries(vSerieL);
    QMargins marV(10,0,0,10);
    vChart->setMargins(marV);
    vChart->setTheme( QChart::ChartThemeDark );

    //time axis (Y) in seconds. It must scroll
    //side-by-side with the waterfall
    points = static_cast<int>( vChart->plotArea().height() );
    vaY->setRange(-points, 0);
    vaY->setLabelFormat("%-03d ");
    vaY->hide();
    vChart->addAxis(vaY, Qt::AlignLeft);
    vSerieN->attachAxis(vaY);
    vSerieS->attachAxis(vaY);
    vSerieD->attachAxis(vaY);
    vSerieA->attachAxis(vaY);
    vSerieU->attachAxis(vaY);
    vSerieL->attachAxis(vaY);

    //power axis (X)
    vaX->setRange(powBeg, powEnd);
    vaX->setLabelFormat("  %-03d ");

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
    vaX->setTickType(QValueAxis::TicksDynamic);
    vaX->setTickAnchor(0);
    vaX->setTickInterval(tilt.at(1));
#else
    vaX->setTickCount(5);
    vaX->applyNiceNumbers();
#endif

    vaX->setLabelsAngle(90);
    vChart->addAxis(vaX, Qt::AlignBottom);
    vSerieS->attachAxis(vaX);
    vSerieN->attachAxis(vaX);
    vSerieD->attachAxis(vaX);
    vSerieA->attachAxis(vaX);
    vSerieU->attachAxis(vaX);
    vSerieL->attachAxis(vaX);

    vSerieS->setVisible( as->getPlotS() );
    vSerieN->setVisible( as->getPlotN() );
    vSerieD->setVisible( as->getPlotD() );
    vSerieA->setVisible( as->getPlotA() );
    vSerieU->setVisible( as->getPlotU() );
    vSerieL->setVisible( as->getPlotL() );

    //plugging signals, only once

    //notifications
    connect( as,    SIGNAL( notifySetDbfs() ),        this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetHz() ),          this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetSec() ),         this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetTune() ),        this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetSampleRate() ),  this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetBandwidth() ),   this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetResolution() ),  this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySsBypass() ),       this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetDsBandwidth() ), this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetOffset() ),      this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetZoom() ),        this,         SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifySetEccentricity()), this,         SLOT( slotSetEccentricity() ) );
    connect( as,    SIGNAL( notifyOpened(const QString*) ), this,   SLOT( slotConfigChanged()) );

    //pushbuttons
    connect( ui->pbShot,    SIGNAL( pressed() ), ac,  SLOT( slotTakeWfManShot() ), Qt::QueuedConnection );
    connect( ui->pbReset,   SIGNAL( pressed() ), this,  SLOT( slotResetEccentricity() ) );

    //sliders
    connect( ui->hsTuneOffset,  SIGNAL( valueChanged(int) ), this,  SLOT( slotSetOffset(int) ) );
    connect( ui->hsTuneZoom,    SIGNAL( valueChanged(int) ), this,  SLOT( slotSetZoom(int) ) );
    connect( ui->hsBrightness,  SIGNAL( valueChanged(int) ), this,  SLOT( slotSetBrightness(int) ) );
    connect( ui->hsContrast,    SIGNAL( valueChanged(int) ), this,  SLOT( slotSetContrast(int) ) );
    connect( ui->hsPowerOffset,  SIGNAL( valueChanged(int) ), this,  SLOT( slotSetPowerOffset(int) ) );
    connect( ui->hsPowerZoom,    SIGNAL( valueChanged(int) ), this,  SLOT( slotSetPowerZoom(int) ) );
    //connect( ui->hsPowerOffset,  SIGNAL( sliderReleased() ), this,  SLOT( slotCleanCharts() ) );
    //connect( ui->hsPowerZoom,    SIGNAL( sliderReleased() ), this,  SLOT( slotCleanCharts() ) );

    connect( ui->pbPeak,    SIGNAL( toggled(bool) ), as, SLOT( setPlotS(bool) ) );
    connect( ui->pbAvg,     SIGNAL( toggled(bool) ), as, SLOT( setPlotN(bool) ) );
    connect( ui->pbDiff,    SIGNAL( toggled(bool) ), as, SLOT( setPlotD(bool) ) );
    connect( ui->pbAvgDiff, SIGNAL( toggled(bool) ), as, SLOT( setPlotA(bool) ) );
    connect( ui->pbUpperThr,SIGNAL( toggled(bool) ), as, SLOT( setPlotU(bool) ) );
    connect( ui->pbLowerThr,SIGNAL( toggled(bool) ), as, SLOT( setPlotL(bool) ) );
    connect( ui->pbLogo,    SIGNAL( clicked() ),    this,  SLOT( slotUpdateLogo() ) );

    connect( m,     SIGNAL( notifyReady() ),             this,   SLOT( slotMainWindowReady() ) );
    connect( ac,    SIGNAL( setShotCounters(int,int) ),  this,   SLOT( slotStartAcq(int,int) ) );
    //connect( vaY,   SIGNAL( rangeChanged(qreal,qreal) ), this,   SLOT( slotRangeChanged(qreal, qreal) ) );

    connect( as,    SIGNAL( notifySplot()), this,   SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifyNplot()), this,   SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifyDplot()), this,   SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifyAplot()), this,   SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifyUplot()), this,   SLOT( slotRuntimeChange() ) );
    connect( as,    SIGNAL( notifyLplot()), this,   SLOT( slotRuntimeChange() ) );

}

Waterfall::~Waterfall()
{
    delete vcv;
    delete hcv;
    delete dp;
    delete ww;
    delete hr;
    delete ui;
}


void Waterfall::GUIinit(void)
{
    MYINFO << __func__ << "() wf";
    blockAllSignals(true);

    init = true;

    //parametrize instant spectra graph ----------------------

    //hcv->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ));
    ui->saPanoramic->setWidget(hcv);
    hSerie->clear();

    //parametrize S/N vs. time graph -----------------------

    ui->saPower->setWidget(vcv);

    //line colors must be the same used in legend
    hSerie->setColor( Qt::white );
    vSerieS->setColor( Qt::red );               //peak power            (S)
    vSerieN->setColor( Qt::cyan );              //average power         (N)
    vSerieD->setColor( QColor("orange") );      //difference            (D)
    vSerieA->setColor( QColor("brown") );       //average difference    (A)
    vSerieU->setColor( Qt::yellow );            //upper threshold       (U)
    vSerieL->setColor( QColor("palegreen") );   //lower threshold       (L)


    //inizializes the status of every control
    //starting from sliders
    int b = as->getBrightness();
    ui->hsBrightness->setValue(b);
    ui->lbBrightness->setNum(b);

    int c = as->getContrast();
    ui->hsContrast->setValue(c);
    ui->lbContrast->setNum(c);

    ui->hsPowerZoom->setMinimum(1);
    ui->hsPowerZoom->setMaximum(10);
    int pg = as->getPowerZoom();
    ui->hsPowerZoom->setValue(pg);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
    haY->setTickInterval(tilt.at(pg));
    vaX->setTickInterval(tilt.at(pg));
#endif

    ui->hsPowerOffset->setMinimum(static_cast<int>(as->getMinDbfs()));
    ui->hsPowerOffset->setMaximum(static_cast<int>(as->getMaxDbfs()));
    int po = as->getPowerOffset();
    ui->hsPowerOffset->setValue(po);


    //these controls are independent from radio settings
    slotSetBrightness(b);
    slotSetContrast(c);
    slotSetPowerZoom(pg);
    slotSetPowerOffset(po);
    ui->lbBrightness->setNum(b);
    ui->lbContrast->setNum(c);
    ui->lbPzoom->setNum(pg);
    ui->lbPoff->setNum(po);

    setGeometry( as->getWwGeometry() );

    //trick to avoid the offset and zoom self-reset when setting bw
    int bw = as->getDsBandwidth();
    tuneOffset = as->getOffset();

    int zoom = 0;
    int zoomCoverage = 0;

    QString s = loc.toString(bw);
    ui->lbBW->setText(s);

    ui->hsTuneZoom->setMinimum(DEFAULT_ZOOM);
    zoom = as->getZoom();
    //resetEccentricity();

    ui->lbFoff->setNum(tuneOffset);
    ui->hsTuneZoom->setValue(zoom);
    MYINFO << "Tune offset set to: " << tuneOffset;
    MYINFO << "Tune zoom set to: " << tuneZoom;
    slotSetZoom(zoom);
    slotSetOffset( tuneOffset );

    slotTakeShot(0,0,0,0,0,0,0, "");

    ui->lbCapturing->hide();
    ui->lbNoCap->hide();
    ui->lbInit->show();

    s = QString("%1.%2").arg(zoom/10).arg(zoom%10);
    ui->lbFzoom->setText(s);
    ui->lbFzoomHz->setText( loc.toString(zoomCoverage * 2) );

    QString logoFile = as->getLogo();
    QPixmap logoPix(logoFile);
    QIcon logoIcon(logoPix);
    ui->pbLogo->setText("");
    ui->pbLogo->setIcon(logoIcon);

    blockAllSignals(false);

    //GUI init terminates once slotRuntimeChange() is called.
}




void Waterfall::slotRuntimeChange()
{
    MYINFO << __func__ ;

    QString s;
    int sr = -1, dsBw = -1, off = 0, ecc = 0, zoom = -1, tune = INT_MAX;
    E_ACQ_MODE currAm = AM_INVALID;

    blockAllSignals(true);
    setGeometry( as->getWwGeometry() );


    sr = as->getSampleRate();
    dsBw = as->getDsBandwidth();
    zoom = as->getZoom();
    off = as->getOffset();
    tune = as->getTune();
    ecc = as->getEccentricity();
    currAm = static_cast<E_ACQ_MODE>( as->getAcqMode() );
    fTicks = (as->isAudioDevice()) ? WF_AUDIO_FREQ_TICKS : WF_FREQ_TICKS;

    bool srChanged = (sr != samplerate);
    bool dsBwChanged = (dsBw != dsBandwidth);
    bool zoomChanged = (zoom != tuneZoom);
    bool offChanged = (off != tuneOffset);
    //bool eccChanged = (ecc != eccentricity);
    bool tuneChanged = (tune != lastTune);


    if(currAm == AM_AUTOMATIC)
    {
        vSerieS->setVisible( as->getPlotS() );
        vSerieN->setVisible( as->getPlotN() );
        vSerieD->setVisible( as->getPlotD() );
        vSerieA->setVisible( as->getPlotA() );
        vSerieU->setVisible( as->getPlotU() );
        vSerieL->setVisible( as->getPlotL() );
        ui->pbPeak->setChecked( as->getPlotS() );
        ui->pbAvg->setChecked( as->getPlotN() );
        ui->pbDiff->setChecked( as->getPlotD() );
        ui->pbAvgDiff->setChecked( as->getPlotA() );
        ui->pbUpperThr->setChecked( as->getPlotU() );
        ui->pbLowerThr->setChecked( as->getPlotL() );

    }
    else
    {
        //thresholds are senseless in continuous or periodic mode
        vSerieS->setVisible( as->getPlotS() );
        vSerieN->setVisible( as->getPlotN() );
        vSerieD->setVisible( as->getPlotD() );
        vSerieA->setVisible( as->getPlotA() );
        vSerieU->setVisible( false );
        vSerieL->setVisible( false );
        ui->pbPeak->setChecked( as->getPlotS() );
        ui->pbAvg->setChecked( as->getPlotN() );
        ui->pbDiff->setChecked( as->getPlotD() );
        ui->pbAvgDiff->setChecked( as->getPlotA() );
        ui->pbUpperThr->setChecked( false );
        ui->pbLowerThr->setChecked( false );

    }


    if(srChanged)
    {
        //SR changed: if lower than BW, the latter is reset to maximum value, equal to SR
        maxBW = sr;
        if(dsBandwidth > maxBW)
        {
            dsBw = maxBW;
            dsBwChanged = true;
        }
        samplerate = sr;
    }


    if(dsBwChanged)
    {
        QString s = loc.toString(dsBw);
        ui->lbBW->setText(s);
        dsBandwidth = dsBw;
    }


    if(init || srChanged || dsBwChanged || zoomChanged || offChanged || /* eccChanged || */ tuneChanged)
    {
        //adjusts the offset and zoom controls accordingly
        if( !as->isAudioDevice() && (srChanged || dsBwChanged || tuneChanged) )
        {
            //recalc the LUT (for radio devices only)
            //

            float fsr = static_cast<float>(samplerate);
            float dsRatio = 1.0F;
            for (int i = 0; i < MAX_DOWNSAMPLING_VALUES; i++)
            {
                dsLUT[i].dsBw = fsr / dsRatio;
                dsLUT[i].ecc = as->getEccentricity();
                dsLUT[i].lowest = tune-(fsr/2.0);
                dsLUT[i].highest = dsLUT[i].lowest + dsLUT[i].dsBw;
                dsRatio *= 2.0;
            }

            lastTune = tune;
        }


        if( as->isAudioDevice() )
        {
            //compensate the binning x2 applied in Radio::performFFT()
            //for audio spectra, where negative frequencies have been
            //discarded.

            //The left side of the spectra, if not zoomed, must always show 0 Hz regardless
            //SR and dsBw
            lowest = 0;
            highest = dsBw/2;

            //the zoom value is expressed as _tenths_ of X
            zoomCoverage = static_cast<int>((dsBandwidth/( static_cast<float>(zoom)/10.0 )) / 4.0);
            minOff = lowest+zoomCoverage;
            maxOff = highest-zoomCoverage;
            offsetCoverage = ((maxOff - minOff)/2) > 1 ? ((maxOff - minOff)/2) : 2;
            zoomLo = off - zoomCoverage + (dsBandwidth/4);
            zoomHi = off + zoomCoverage + (dsBandwidth/4);
            MYINFO << "Audio device wants zoomLo=" << zoomLo << " zoomHi=" << zoomHi;
        }
        else
        {
            int dsRatio = static_cast<int>(samplerate / dsBandwidth);
            dsRatio = (dsRatio == 0)? 1 : dsRatio;
            int idx = log2(dsRatio);
            lowest = dsLUT[idx].lowest;
            highest = dsLUT[idx].highest;
            ecc = dsLUT[idx].ecc;

            MYINFO << idx << ") dsBw=" << dsBw << " ecc=" << ecc << ", centerHz=" << tune+ecc << ", lowest=" << lowest << ", highest=" << highest;
            //the zoom value is expressed as _tenths_ of X
            zoomCoverage = static_cast<int>((dsBandwidth/( static_cast<float>(zoom)/10.0 )) / 2.0);

            minOff = lowest+zoomCoverage;
            maxOff = highest-zoomCoverage;
            offsetCoverage = ((maxOff - minOff)/2) > 1 ? ((maxOff - minOff)/2) : 2;
            zoomLo = tune + off - zoomCoverage;
            zoomHi = tune + off + zoomCoverage;
            MYINFO << "Radio device wants zoomLo=" << zoomLo << " zoomHi=" << zoomHi;

        }

        ui->hsTuneOffset->setMaximum( offsetCoverage );
        ui->hsTuneOffset->setMinimum( -offsetCoverage );
        ui->hsTuneOffset->setTickInterval( dsBw / 10 );
        ui->hsTuneOffset->setValue( off );
        s.setNum(off);
        ui->lbFoff->setText(s);
        tuneOffset = off;

        //the zoom slider min/max values are fixed
        //only the value can change
        ui->hsTuneZoom->setValue( zoom );
        s = QString("%1.%2").arg(zoom/10).arg(zoom%10);
        ui->lbFzoom->setText(s);
        ui->lbFzoomHz->setText( loc.toString(zoomCoverage * 2) );
        tuneZoom = zoom;

        eccentricity = ecc;
        as->setEccentricity(eccentricity, false);
    }

    if(minOff == maxOff)
    {
        //when zoom = DEFAULT_ZOOM
        ui->hsTuneOffset->setEnabled(false);
    }
    else
    {
        ui->hsTuneOffset->setEnabled(true);
    }



    double res = static_cast<double>( as->pround( as->getResolution(), 2 ) );
    ui->lbRes->setNum( static_cast<double>( res ));
    res = as->getResolution();
    ui->lbPoints->setNum( as->getFFTbins() );
    int visiblePoints = static_cast<int>((zoomCoverage * 2) / res);
    ui->lbVisiblePoints->setNum( visiblePoints );

    if(as->getDbfs() != 0)
    {
        //shows the power grid in both graphs
        haY->show();
        vaX->show();

    }
    else
    {
        haY->hide();
        vaX->hide();
    }

    as->setZoomedRange(zoomLo, zoomHi);
    haX->setRange(zoomLo, zoomHi);

    int step = calcSliderStep( hr->getMinHz(), offsetCoverage );

    ui->hsTuneOffset->setSingleStep( step );
    ui->hsTuneOffset->setPageStep( step * 10 );
    ui->hsTuneOffset->setMaximum( offsetCoverage );
    ui->hsTuneOffset->setMinimum( -offsetCoverage );

    if(!init)
    {
        if(srChanged || dsBwChanged)
        {
            //the zoom is set back to default while offset and eccentricity are set to zero
            MYINFO << "srChanged=" << srChanged << " dsBwChanged=" << dsBwChanged;
            zoom = DEFAULT_ZOOM;
            as->setZoom(zoom);
            if(as->isAudioDevice())
            {
                off = -offsetCoverage;
            }
            else
            {
                off = 0;
            }
            as->setOffset(off);
            slotResetEccentricity();
        }

        if(zoomChanged && !as->isLoading())
        {
            MYINFO << "zoomChanged ?";
            //the offset and eccentricity are set back to zero
            if(as->isAudioDevice())
            {
                off = -offsetCoverage;
            }
            else
            {
                off = 0;
            }
            as->setOffset(off);
        }

        if(offChanged)
        {
            MYINFO << "offChanged ?";
        }
    }

    intervalHz = as->getPeakIntervalFreq();
    ui->lbFreq->setText( loc.toString(intervalHz) );

    ui->hsPowerOffset->setMinimum(as->getMinDbfs());
    ui->hsPowerOffset->setMaximum(as->getMaxDbfs());
    int po = as->getPowerOffset();
    ui->hsPowerOffset->setValue(po);

    float displayedBins = static_cast<float>(as->getFFTbins() * 10.0F) / as->getZoom();
    bpp = getBpp( displayedBins );
    slotUpdateGrid();

    blockAllSignals(false);
    MYINFO << "exiting " << __func__ ;

}

void Waterfall::slotMainWindowReady()
{
    GUIinit();
    slotRuntimeChange();

    init = false;
    MYINFO << "*** END of GUI init***" << MY_ENDL;

}


void Waterfall::slotResetEccentricity()
{
    MYINFO << "resetEccentricity()";
    ui->hsTuneZoom->setValue(1);
    int ecc = 0;
    as->setEccentricity(ecc);
    eccentricity = ecc;
    as->getPeakIntervalFreq();
}

void Waterfall::slotUpdateLogo()
{
    MYDEBUG << "slotUpdateLogo()";   
    QString logoFile = QFileDialog::getOpenFileName(
        this,
        tr("Open configuration file"),
        ac->workingDir().absolutePath(),
        tr("PNG image file (*.png *.PNG)"));

    if( logoFile.isEmpty() )
    {
        //no file specified
        return;
    }

    QPixmap logoPix(logoFile);
    QIcon logoIcon(logoPix);
    ui->pbLogo->setText("");
    ui->pbLogo->setIcon( logoIcon );
    as->setLogo(logoFile);
}

void Waterfall::slotConfigChanged()
{
    //init = true;
}


void Waterfall::slotSetCapturing(bool isInhibited, bool isCapturing)
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);


    if(!init)
    {
        ui->lbInit->setVisible(false);
        if(!isInhibited)
        {
            ui->lbNoCap->setVisible(false);
            ui->lbCapturing->setVisible(isCapturing);
        }
        else
        {
            ui->lbCapturing->setVisible(false);
            ui->lbNoCap->setVisible(true);
        }
    }
    else
    {
        ui->lbInit->setVisible(true);
    }
}

void Waterfall::slotSetScreenshotCoverage(int secs)
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);


    if(secs == 0)
    {
        ui->lbSsCov->setText(tr("measuring..."));
    }
    else
    {
        ui->lbSsCov->setNum(secs);
    }
}



void Waterfall::slotStartAcq(int shotNr, int totalShots)
{

    yScans = 0;
    vSerieS->clear();
    vSerieN->clear();
    vSerieD->clear();
    vSerieA->clear();
    vSerieU->clear();
    vSerieL->clear();
    int points = static_cast<int>(vChart->plotArea().height());
    vaY->setRange(-points, 0);

    QString s;

    if(shotNr == 0)
    {
        ui->lbShot->setText("00/00");
        return;
    }

    if(totalShots == 0)
    {
        //manual shots
        //s.sprintf("%02i", shotNr);
        s = QString("%1").arg(shotNr, 2, 10, QChar('0'));
        ui->lbShot->setText(s);
        ui->lbTaken->setText( tr("Manual shots taken:") );
    }
    else
    {
        //s.sprintf("%02i/%02i", shotNr,totalShots);
        s = QString("%1/%2").arg(shotNr, 2, 10, QChar('0')).arg(totalShots, 2, 10, QChar('0'));
        ui->lbShot->setText(s);
        ui->lbTaken->setText( tr("Automatic shots taken:") );
    }

    ui->lbTaken->show();
    ui->lbShot->show();
    ui->lbInit->show();

    slotRuntimeChange();
}



void Waterfall::slotSetOffset( int value )
{
    //rounds value by single step
    int sStep = ui->hsTuneOffset->singleStep();

    int mod = value % sStep;
    if (mod != 0)
    {
        value = ( mod > (sStep / 2) )?
                (value - mod + sStep) :
                (value - mod);
    }

    as->setOffset(value);
    slotRuntimeChange();
}



void Waterfall::slotSetEccentricity()
{
    as->roundEccentricity();
    slotRuntimeChange();
}



void Waterfall::slotSetZoom( int value )
{
    //zoom is a value 1..100x
    //rounds value by single step
    int sStep = ui->hsTuneZoom->singleStep();
    int mod = value % sStep;
    if (mod != 0)
    {
        value = ( mod > (sStep / 2) )?
                (value - mod + sStep) :
                (value - mod);
    }

    //resets the offset each time the zoom changes

    as->setZoom(value);
    slotRuntimeChange();
}

void Waterfall::slotSetBrightness( int value )
{
    MYDEBUG << "linear input value=" << value;
    as->setBrightness(value);
    dp->setDbmPalette(value, as->getContrast());
    //dp->updatePalette();
    //slotRuntimeChange();
}


void Waterfall::slotSetContrast( int value )
{
    MYDEBUG << "linear input value=" << value;
    as->setContrast(value);
    dp->setDbmPalette(as->getBrightness(), value);
    //dp->updatePalette();
    //slotRuntimeChange();
}

void Waterfall::slotSetPowerZoom( int zoom )
{
    MYDEBUG << "linear input value=" << zoom;
    as->setPowerZoom(zoom);
    int off = as->getPowerOffset();
    int max = as->getMaxDbfs();
    int min = as->getMinDbfs();
    int delta = static_cast<int>(fabs(max - min) / zoom);
    powBeg = off - (delta/2);
    powEnd = off + (delta/2);

    vaX->setRange(powBeg, powEnd);

    powBeg = storedN - (delta/2);
    powEnd = storedN + (delta/2);
    haY->setRange(powBeg, powEnd);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
    vaX->setTickInterval(tilt.at(zoom));
    haY->setTickInterval(tilt.at(zoom));
#endif

    slotCleanCharts();
    MYDEBUG << "powBeg=" << powBeg << " off= " << off << " powEnd=" << powEnd
     << " max=" << max << " min=" << min << " delta=" << delta << " storedN=" << storedN;
}

void Waterfall::slotSetPowerOffset(int dbFs)
{
    MYDEBUG << "linear input value=" << dbFs;
    as->setPowerOffset(dbFs);
    int zoom = as->getPowerZoom();
    int delta = static_cast<int>( fabs(as->getMaxDbfs() - as->getMinDbfs()) / zoom );
    powBeg = dbFs - (delta/2);
    powEnd = dbFs + (delta/2);
    vaX->setRange(powBeg, powEnd);

    if(storedN != MAX_DBFS)
    {
        //Instant spectra graph
        //Centers the N value on Y axis
        //N must be a plausible value
        powBeg = storedN - (delta/2);
        powEnd = storedN + (delta/2);
    }
    haY->setRange(powBeg, powEnd);
    slotSetPowerZoom(zoom);

    MYDEBUG << "powBeg=" << powBeg << " powEnd=" << powEnd;
}


void Waterfall::slotCleanCharts()
{
    //power/time series reset
    yScans = 0;
    vSerieS->clear();
    vSerieN->clear();
    vSerieD->clear();
    vSerieA->clear();
    vSerieU->clear();
    vSerieL->clear();
    int points = static_cast<int>(vChart->plotArea().height());
    vaY->setRange(-points, 0);
    points = static_cast<int>(hChart->plotArea().width());
    haX->setRange( 0, points - 1);
}



void Waterfall::slotShowStatus( int appState )
{
    if(appState <= AST_CANNOT_OPEN)
    {
        ui->hsTuneOffset->setEnabled(false);
        ui->hsTuneZoom->setEnabled(false);
    }
    else
    {
        //ui->hsBW->setEnabled(true);
        ui->hsTuneOffset->setEnabled(true);
        ui->hsTuneZoom->setEnabled(true);
    }
}

void Waterfall::slotCleanAll()
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);

    ww->clean();
    dp->updatePalette();
}


void Waterfall::slotRefresh()
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);
    QList<QPointF> readyScanList;
    ScanInfos readyScanInfos;
    const uint atLeastScans = 5;

    //max number of visible points
    int vp = vChart->plotArea().height();

    bool raisingEdge = false;
    int S = 0;
    int N = 0;
    int iDiff = 0;
    int iAvgDiff = 0;
    qreal upThr = 0.0;
    qreal lowThr = 0.0;

    QString timeStamp;

    while(!qScanList.isEmpty())
    {
        readyScanList = qScanList.dequeue();
        MYDEBUG << timeStamp << " scan len=" << readyScanList.length();

        if (!qScanInfos.isEmpty())
        {
            readyScanInfos = qScanInfos.dequeue();
            timeStamp = readyScanInfos.timeStamp;
            raisingEdge = readyScanInfos.raisingEdge;

            S = static_cast<int>( readyScanInfos.maxDbm );
            N = static_cast<int>( readyScanInfos.avgDbm );
            upThr = static_cast<qreal>( readyScanInfos.upThr );
            lowThr = static_cast<qreal>( readyScanInfos.lowThr );
            iDiff = S-N;
            iAvgDiff = static_cast<int>( readyScanInfos.avgDiff );
        }

        //update the instantaneous spectra graph (the lower one)
        hSerie->replace( readyScanList );
        haX->setRange(0, readyScanList.size()-1);
        if(raisingEdge == true || yScans <= atLeastScans)
        {
            storedList = readyScanList;
            storedS = S;
            storedDiff = iDiff;
            storedAvgDiff = iAvgDiff;
            storedLowThr = lowThr;
            storedUpThr = upThr;

            if(yScans == atLeastScans)
            {
                //wait some scans in order to get a quite stable N value to
                //initialize storedN and adjust the vertical centering
                //of instantaneous spectra
                if(as->fuzzyCompare(N, storedN, 2))
                {
                    //update the instantaneous spectra graph offset
                    //if N varies more than 2%
                    storedN = N;
                    slotSetPowerZoom( as->getPowerZoom() );
                }

                storedN = N;
            }
        }

        //update the labels
        ui->lbSval->setText( QString("%1 dBfs").arg(S) );
        ui->lbNval->setText( QString("%1 dBfs").arg(N) );
        ui->lbDiffVal->setText( QString("%1 dBfs").arg(iDiff) );
        ui->lbAvgDiffVal->setText( QString("%1 dBfs").arg(iAvgDiff) );
        ui->lbUpThr->setText( QString("%1 dBfs").arg(upThr, 0, 'f', 1) );
        ui->lbLowThr->setText( QString("%1 dBfs").arg(lowThr, 0, 'f', 1) );

        //update the waterfall
        ww->refresh(timeStamp, readyScanList);


        /*
         * QtCharts scrolling issue (feature or bug?)
         * Each scroll() causes a rangeChanged() signal().
         * The range scroll amount is not exactly the same
         * given to scroll(), I don't know why, maybe a cumulative
         * rounding error.
         * The result is that the range loses the alignment
         * to yScans and the resulting graph, after half hour
         * of work becomes clearly no more aligned to waterfall.
         *
         */

        vSerieS->append( S, yScans );
        vSerieN->append( N, yScans );
        vSerieD->append( iDiff, yScans );
        vSerieA->append( iAvgDiff, yScans );
        vSerieU->append( upThr, yScans );
        vSerieL->append( lowThr, yScans );

        yScans++;
        vChart->scroll(0.0, 1.0);

        if(false) //yScans % _RECOVER_SCROLLING_BUG_EVERY_SCANS == 0)
        {
            historyCleanNeeded = true;
        }
        else
        {

            //the oldest points, when no more visible, are deleted
            //from series

            if( yScans >  static_cast<uint>(vp) )
            {
                vSerieN->remove(0);
                vSerieS->remove(0);
                vSerieD->remove(0);
                vSerieA->remove(0);
                vSerieU->remove(0);
                vSerieL->remove(0);
            }
            vaY->setRange(yScans-vp, yScans);

            MYDEBUG << "yScans=" << yScans << " vSerieN.count=" << vSerieN->count();

        }

        if(historyCleanNeeded && ui->lbCapturing->isHidden())
        {
            /*
             * Workaround for the progressive shortening bug mentioned above
             */

            MYINFO << "workaround";
            QList<QPointF> tmp;

            tmp = vSerieS->points();
            vSerieS->clear();
            vSerieS->append(tmp);

            tmp = vSerieN->points();
            vSerieN->clear();
            vSerieN->append(tmp);

            tmp = vSerieD->points();
            vSerieD->clear();
            vSerieD->append(tmp);

            tmp = vSerieA->points();
            vSerieA->clear();
            vSerieA->append(tmp);

            tmp = vSerieU->points();
            vSerieU->clear();
            vSerieU->append(tmp);

            tmp = vSerieL->points();
            vSerieL->clear();
            vSerieL->append(tmp);
            vaY->setRange(yScans-vp, yScans);

            historyCleanNeeded = false;

        }
    }

    ui->lbUTC->setText( ac->getDateTime() );

    if(qScanList.length() != qScanInfos.length())
    {
        MYINFO << "Queues qScanList holds " << qScanList.length() << " elements and qScanInfo holds " << qScanInfos.length() << " elements, resyncing";
        while(!qScanInfos.isEmpty())
        {
            MYINFO << "dequeuing and freeing elements";
            readyScanInfos = qScanInfos.dequeue();
        }

    }

    if(as->signalsBlocked())
    {
        MYWARNING << "Settings signals blocked";
    }
}

bool Waterfall::appendInfos(QString timeStamp, float maxDbm, float avgDbm, float avgDiff, float upThr, float lowThr, bool raisingEdge)
{
    //this slot is called directly as a function by Control thread
    QMutexLocker ml(&protectionMutex);

    ScanInfos si;

    si.timeStamp = timeStamp;
    si.maxDbm = maxDbm;
    si.avgDbm = avgDbm;
    si.avgDiff = avgDiff;
    si.upThr = upThr;
    si.lowThr = lowThr;
    si.raisingEdge = raisingEdge;
    qScanInfos.append(si);
    if(qScanInfos.length() > 10)
    {
        MYINFO << "waiting scaninfos:" << qScanInfos.length();
    }
    return true;
}


bool Waterfall::appendPixels(float binDbfs, bool begin, bool end)
{
    //this slot is called directly as a function by Control thread
    QMutexLocker ml(&protectionMutex);

    if(begin)
    {
        scanPixel = 0;
        pixPowerSum = 0.0F;
        binRemainder = 0.0F;
        pixRemainder = 0.0F;
        busyScanList.clear();
    }

    float pixDbfs = 0.0F;
    MY_ASSERT(bpp != 0.0F);
    if(bpp < 1.0F)
    {
        //1:N
        //less than one bin per pixel: the same dBfs value is set in
        //consecutive pixels
        binRemainder = binRemainder + (1.0F/bpp);
        while (binRemainder >= 1.0F)
        {
            pixDbfs = binDbfs;
            busyScanList.append( QPointF( scanPixel, pixDbfs ) );
            scanPixel++;
            binRemainder = binRemainder - 1.0F;
        }
    }
    else if(bpp == 1.0F)
    {
        //1:1
        //one pixel per bin - the simplest case
        pixDbfs = binDbfs;
        busyScanList.append( QPointF( scanPixel, pixDbfs ) );
        scanPixel++;
    }
    else
    {
        //N:1
        //more bins per pixel: the resulting pixel is the average of consecutive bins,
        //obtained by summing them and dividing by bpp.
        //If the last bin fits partially a pixel (bpp is not integer), its remainder is saved and summed to the
        //following pixel power

        if(pixRemainder == 0.0F)
        {
            //there is no remainder, let's start processing a new pixel
            pixRemainder = bpp;

        }

        if(pixRemainder >= 1.0F)
        {
            //processing the integer part of pixel power
            //adds the full bin power
            pixPowerSum = pixPowerSum + binDbfs;
            pixRemainder = fabs(pixRemainder - 1.0F);
            binRemainder = 0.0F;
        }
        else
        {
            //processing the fractional part of pixel power
            //the power is added in proportion to remainder
            leftBinDbfs = binDbfs * pixRemainder;
            pixPowerSum = pixPowerSum + leftBinDbfs;
            binRemainder = 1.0F - pixRemainder;
            pixRemainder = (bpp - binRemainder);

            //pixel done, calculating its power by averaging
            pixDbfs = pixPowerSum / bpp;
            busyScanList.append( QPointF( scanPixel, pixDbfs ) );
            scanPixel++;

            //unprocessed power fraction left to the following pixel
            leftBinDbfs = binDbfs - leftBinDbfs;
            pixPowerSum = leftBinDbfs;
        }

    } //end if(bpp < 1.0F)

    //MYDEBUG << "busyScanList len= " << busyScanList.length() << " scanPixel=" << scanPixel << " binRemainder="
           // << binRemainder << " pixRemainder=" << pixRemainder;

    if(end)
    {
        int missing = scanLen - busyScanList.length();
        for(int n = 0; n < missing; n++)
        {
            //fixes last remaining pixel
            busyScanList.append( QPointF( scanPixel, pixDbfs ) );
        }

        qScanList.append(busyScanList);
        if(qScanList.length() > 10)
        {
            MYINFO << "waiting scans:" << qScanList.length();
        }
        return true;
    }
    return false;
}

void Waterfall::slotUpdateGrid()
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);

    MYDEBUG << "slotUpdateGrid()";

    dp->refresh();

    if(zoomLo < zoomHi)
    {
        hr->setGrid( fTicks, zoomLo, zoomHi );
        ww->setGrid(hr->getMinPix(), tTicks, hr->getMajHz(), hr->getMinHz());
    }

    palBeg = 0;
    palEnd = 0;
    palExc = 0;
    dp->paletteExtension(palBeg, palEnd, palExc);
    as->setWfGeometry( geometry(), ww->geometry() );

}


float Waterfall::getBpp(int displayedBins)
{
    MYINFO << "getBpp( displayedBins = " << displayedBins << " )";

    QRect wSize = as->getWwPaneGeometry();
    MYINFO << "getBpp wfWidth = " << wSize.width() << " pixels";

    float dsRatio = dsBandwidth;
    dsRatio /= samplerate;

    MYINFO << "dsRatio = " << dsRatio;

    bpp = (static_cast<float>(displayedBins) /* * dsRatio */) / wSize.width();
    scanLen = static_cast<int>( ceil((1.0/bpp) * displayedBins) );


    MYINFO << "FFT points/bins per pixel (bpp)= " << bpp << " pixel per bins:" << 1.0/bpp << " total pixels per scan:" << scanLen;
    return bpp;
}


void Waterfall::slotReplay()
{
    if(storedList.count() > 0)
    {
        ui->lbSval->setText( QString("%1 dBfs").arg(storedS) );
        ui->lbNval->setText( QString("%1 dBfs").arg(storedN) );
        ui->lbDiffVal->setText( QString("%1 dBfs").arg(storedDiff) );
        ui->lbAvgDiffVal->setText( QString("%1 dBfs").arg(storedAvgDiff) );
        ui->lbUpThr->setText( QString("%1 dBfs").arg(storedUpThr, 0, 'f', 1, ' ') );
        ui->lbLowThr->setText( QString("%1 dBfs").arg(storedLowThr, 0, 'f', 1, ' ') );


        hSerie->replace( storedList );
        haX->setRange(0, storedList.size()-1);
        storedList.clear();
        storedList.clear();
    }
}

void Waterfall::resizeEvent ( QResizeEvent* event )
{
    if(init == true)
    {
        //skips the first resizeEvent() issued by the system
        MYINFO << "event->size=" << event->size() << " geometry()=" << geometry() << " IGNORED";
    }
    else
    {
        MYINFO << "event->size=" << event->size() << " geometry()=" << geometry();
        MYDEBUG << "widget waterfall : " << ww->size();
        MYDEBUG << "widget hChart: " << hChart->size();
        MYDEBUG << "plot area hChart: " << hChart->plotArea();


        ww->setGrid(hr->getMinPix(), tTicks, hr->getMajHz(), hr->getMinHz(), true);
        as->setWfGeometry( geometry(), ww->geometry() );

        int points = static_cast<int>(hChart->plotArea().width());
        haX->setRange(0, points-1);

        points = static_cast<int>(vChart->plotArea().height());
        vaY->setRange(-points, 0);

        yScans = 0;
        vSerieS->clear();
        vSerieN->clear();
        vSerieD->clear();
        vSerieA->clear();
        vSerieU->clear();
        vSerieL->clear();

        points = static_cast<int>( vChart->plotArea().height() );
        vaY->setRange(-points, 0);

        slotRuntimeChange();
        slotUpdateGrid();
        emit waterfallResized();

    }
}

void Waterfall::moveEvent ( QMoveEvent * event )
{
    if(init == true)
    {
        //skips the first moveEvent() issued by the system
        MYINFO << "event->pos=" << event->pos() << " geometry()=" << geometry() << " IGNORED";
    }
    else
    {
        MYINFO << "event->pos=" << event->pos() << " geometry()=" << geometry();
        as->setWfGeometry( geometry(), ww->geometry() );
    }
}


void Waterfall::blockAllSignals(bool block)
{
    MYINFO << "blockAllSignals(" << block << ")";
    sigBlock = block;

    ui->hsBrightness->blockSignals(block);
    ui->hsContrast->blockSignals(block);
    ui->hsPowerZoom->blockSignals(block);
    ui->hsPowerOffset->blockSignals(block);
    ui->hsTuneOffset->blockSignals(block);
    ui->hsTuneZoom->blockSignals(block);
    ui->pbShot->blockSignals(block);
    as->blockSignals(block);
}


void Waterfall::slotTakeShot(int shotNr, int totalShots, float maxDbm, float avgDbm, float avgDiff, float upThr, float lowThr, QString shotFileName )
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);

    QString s;

    if( ac->getAcquisitionMode() == AM_AUTOMATIC )
    {
        //redisplay the instantSpectra at moment of the event
        //for the sake of screenshot completeness
        slotReplay();
    }

    if(shotNr == 0)
    {
        ui->lbShot->setText("00/00");
        return;
    }

    if(totalShots == 0)
    {
        //manual shots
        //s.sprintf("%02i", shotNr);
        s = QString("%1").arg(shotNr, 2, 10, QChar('0'));
        ui->lbShot->setText(s);
        ui->lbTaken->setText( tr("Manual shots taken:") );
        MYINFO << "Manual shot taken: " << shotFileName;
    }
    else
    {        
        //automatic shots
        s = QString("%1/%2").arg(shotNr, 2, 10, QChar('0')).arg(totalShots, 2, 10, QChar('0'));
        ui->lbShot->setText(s);
        ui->lbTaken->setText( tr("Automatic shots taken:") );
        MYINFO << "Automatic shot taken: " << shotFileName;

        int S = static_cast<int>( maxDbm );
        int N = static_cast<int>( avgDbm );

        int iDiff = S-N;
        int iAvgDiff = static_cast<int>( avgDiff );

        ui->lbSval->setText( QString("%1 dBfs").arg(S) );
        ui->lbNval->setText( QString("%1 dBfs").arg(N) );
        ui->lbDiffVal->setText( QString("%1 dBfs").arg(iDiff) );
        ui->lbAvgDiffVal->setText( QString("%1 dBfs").arg(iAvgDiff) );

        ui->lbUpThr->setText( QString("%1 dBfs").arg(upThr, 0, 'f', 1) );
        ui->lbLowThr->setText( QString("%1 dBfs").arg(lowThr, 0, 'f', 1) );

    }
    ui->lbTaken->show();
    ui->lbShot->show();

    show();
    setFocus();

    QPixmap shot = grab();
    shot.save( shotFileName, "PNG"); //takes a shot of the entire window

}


void Waterfall::slotUpdateCounter(int shotNr, int totalShots)
{
    //this slot is called by Control thread
    QMutexLocker ml(&protectionMutex);

    QString s;

    if(shotNr == 0)
    {
        ui->lbShot->setText("00/00");
        return;
    }

    if(totalShots > 0)
    {
        s = QString("%1/%2").arg(shotNr, 2, 10, QChar('0')).arg(totalShots, 2, 10, QChar('0'));
        ui->lbShot->setText(s);
        ui->lbTaken->setText( tr("Automatic dumps taken:") );
    }
    ui->lbTaken->show();
    ui->lbShot->show();
}

QSize Waterfall::getWidgetSize()
{
    return ww->size();
}

int Waterfall::calcSliderStep( int base, int limit )
{
    int pow10 = 0, step = 1, retVal = 1;
    int test = static_cast<int>( pow(10.0, pow10) );

    for( ; pow10 < 5; pow10++ )
    {
        if( base % test == 0 )
        {
            step = test;
            test = 5 * step;

            if( base % test == 0 )
            {
                step = test;
                test = 2 * step;
            }
            else
            {
                retVal = step;
                break;
            }
        }
        else
        {
            retVal = step;
            break;
        }

        if(step >= limit)
        {
            break;
        }

    }

    return retVal;
}

