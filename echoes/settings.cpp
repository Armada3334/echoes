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
$Id: settings.cpp 367 2018-11-23 17:22:34Z gmbertani $
*******************************************************************************/

#include <float.h>
#include "settings.h"




Settings::Settings(QObject *parent) :
    QObject(parent)
{
    setObjectName("Settings object");
    reset();

    //not saved on disk
    ww = nullptr;
    mw = nullptr;
}

Settings:: ~Settings()
{
    //save local settings before quit
    saveLocal();
    delete localSettings;
}

void Settings::reset()
{
    rtsRevision = 0;
    rtsFormatRev = CURRENT_RTS_FORMAT_REV;
    echoesVersion = APP_VERSION;

    //main window
    configName          = DEFAULT_CONFIG_NAME;
    geom                = DEFAULT_MAINWND_GEOM;

    //device settings:
    device              = DEFAULT_DEVICE;
    sampleRate          = DEFAULT_RATE;
    bandwidth           = DEFAULT_BW;
    tune                = DEFAULT_TUNE;
    gain                = DEFAULT_GAIN;
    ppmError            = DEFAULT_PPMERR;
    ssBypass            = DEFAULT_SSBYPASS;
    antenna             = DEFAULT_ANTENNA;
    fRange              = DEFAULT_FREQ_RANGE;
    bRange              = DEFAULT_BW_RANGE;
    sRange              = DEFAULT_SR_RANGE;

    //FFT settings:
    dsBw                = DEFAULT_BW;
    ftw                 = FTW_RECTANGLE;
    resolution          = (DEFAULT_BIN_SIZE * 10e8);
    fftBins             = DEFAULT_FFT_BINS;
    maxResHz            = DEFAULT_BIN_SIZE;
    maxResMTU           = DEFAULT_FFT_BINS;
    dsRatio             = 1;

    //output settings:
    acqMode             = DEFAULT_ACQ_MODE;
    interval            = DEFAULT_INTVL;
    shots               = DEFAULT_MAX_SHOTS;
    recTime             = DEFAULT_RECTIME;
    after               = DEFAULT_AUTO_AFTER;
    thresholdsBehavior  = DEFAULT_THRESHOLDS;
    upThreshold         = DEFAULT_AUTO_UP_THRESHOLD;
    dnThreshold         = DEFAULT_AUTO_DN_THRESHOLD;
    detectionRange      = DEFAULT_AUTO_TRANGE;
    genDumps            = DEFAULT_DUMPS;
    genScreenshots      = DEFAULT_SCREENSHOTS;
    avgdScans           = DEFAULT_N_SCANS;
    joinTime            = DEFAULT_JOIN_TIME;
    noiseLimit          = DEFAULT_NOISE_LIMIT;

    //storage settings
    overwriteSnap       = DEFAULT_OVERWRITE_SNAP;
    DBsnap              = DEFAULT_DB_SNAP;
    minFree             = DEFAULT_FREE_SPACE;
    eraseLogs           = DEFAULT_ERASE_LOGS;
    eraseShots          = DEFAULT_ERASE_SHOTS;
    dataLasting         = DEFAULT_DATA_LASTING;
    imgLasting          = DEFAULT_IMG_LASTING;

    //preferences
    maxEventLasting     = DEFAULT_MAX_EVENT_LASTING;
    hzTickMarks         = DEFAULT_HZ_TICKS;
    dBfsTickMarks       = DEFAULT_DBFS_TICKS;
    wfSecTickMarks      = DEFAULT_WSEC_TICKS;
    wfRangeBoundaries   = DEFAULT_RANGE_BOUNDARIES;
    tooltips            = DEFAULT_TOOLTIPS;
    pingOn              = DEFAULT_PING;
    faultSoundOn        = DEFAULT_FAULT_SOUND;
    palette             = DEFAULT_WF_PALETTE;
    dbFSoffset          = DEFAULT_DBFS_OFFSET;
    dbFSgain            = DEFAULT_DBFS_GAIN;
    serverPort          = DEFAULT_SERVER_PORT;
    serverAddr          = QHostAddress(DEFAULT_SERVER_ADDRESS);
    createDATdumps      = DEFAULT_DAT_DUMPS;
    datagramDelay       = DEFAULT_DGRAM_DELAY;
    scanMode     = DEFAULT_SCAN_INTEGRATION;

#ifdef WANT_FULL_UTC_DELTA
    UTCdelta            = 0;
#else
    UTCdelta            = QTime().fromString(DEFAULT_UTC_DELTA);
#endif

    //waterfall window:
    wfOffset            = DEFAULT_OFFSET;
    wfZoom              = DEFAULT_ZOOM;
    wfEccentricity      = DEFAULT_ECCENTRICITY;
    wfEccentricityBak   = DEFAULT_ECCENTRICITY;
    brightness          = DEFAULT_BRIGHTNESS;
    contrast            = DEFAULT_CONTRAST;
    powerZoom           = DEFAULT_POWERZOOM;
    powerOffset         = DEFAULT_POWEROFFSET;
    wwGeom              = DEFAULT_WATERFALL_GEOM;
    wwPaneGeom          = DEFAULT_WATERFALL_GEOM; //must be calculated at runtime
    plotS               = DEFAULT_PLOT_S;
    plotN               = DEFAULT_PLOT_N;
    plotD               = DEFAULT_PLOT_D;
    plotA               = DEFAULT_PLOT_A;
    plotU               = DEFAULT_PLOT_U;
    plotL               = DEFAULT_PLOT_L;

    //these values are calculated by waterfall window
    //but to allow working in console mode, they must
    //also be stored in configuration:
    zoomLo              = 0;
    zoomHi              = DEFAULT_BW;

    //local settings
    lastDBsnapshot      = "1970-01-01";
    resetCounter        = 0;
    logo                = DEFAULT_LOGO;

    //not saved on disk
    changed = 0;
    srvChanged = 0;
    hzPerPixel = 0;
    intervalHz = 0;
    localSettings = nullptr;

    consoleMode         = false;
    dumbMode            = false;
    audioDevice         = false;
    netDevice           = false;
    testDevice          = false;
    vacuumStatus        = false;
}

/**
 * WARNING!!!
 * in case a change in groups/items, this method must be updated later.
 * In this way it will be possible to convert existing rts files in
 * the new format by simply running the program and opening/saving them.
 * After conversion, this method can be updated in order
 * to load correctly the rts files with the new format.
 */
void Settings::load( QSettings& ini, bool enableNotifications )
{
    MYINFO << "loading config";
    loading = true;
    configFullPath =  ini.fileName();
    configName = QFileInfo( configFullPath ).baseName();

    //the local settings are placed in the same place
    //platform-dependent, the user has no way to change them
    //
    QString targetInstance;
    targetInstance = QString("%1-%2").arg(TARGET_NAME).arg(configName);

    localSettings = new QSettings(COMPANY_NAME, targetInstance);
    MY_ASSERT( nullptr !=  localSettings)

    loadLocal();

    //general informations
    ini.beginGroup( "General" );
    rtsFormatRev = ini.value("RTSformatRev", 0).toInt();
    rtsRevision = ini.value("RTSrevision", 0).toInt();
    ini.endGroup();

    //device settings:
    ini.beginGroup( "Device settings" );
    device          = ini.value("Device", DEFAULT_DEVICE).toString();
    device.truncate(MAX_DEVICE_ID_LEN);

    sampleRate      = ini.value("SampleRate", DEFAULT_RATE).toInt();
    bandwidth       = ini.value("Bandwidth", DEFAULT_BW).toInt();
    tune            = ini.value("Tune",DEFAULT_TUNE).toInt();
    gain            = ini.value("Gain", DEFAULT_GAIN).toInt();
    ppmError        = ini.value("PpmError", DEFAULT_PPMERR).toInt();
    antenna         = ini.value("Antenna", DEFAULT_ANTENNA).toString();
    sRange          = ini.value("SampleRateRange", DEFAULT_SR_RANGE).toString();
    fRange          = ini.value("FrequencyRange", DEFAULT_FREQ_RANGE).toString();
    bRange          = ini.value("BandwidthRange", DEFAULT_BW_RANGE).toString();
    ini.endGroup();

    //FFT settings:
    ini.beginGroup( "FFT settings" );
    dsBw            = ini.value("DownsampledBand", DEFAULT_DSBW).toInt();
    ftw             = static_cast<FFT_WINDOWS>( ini.value("FFTwindow", FTW_RECTANGLE).toInt() );

    bool ok;
    QString sRes    = ini.value("Resolution", DEFAULT_RESOLUTION).toString();
    float fRes  = sRes.toFloat(&ok) * 10e8;
    MY_ASSERT(ok == true)
    resolution = static_cast<qint64>(fRes);

    fftBins         = ini.value("FFTbins", DEFAULT_FFT_BINS).toInt();
    ssBypass        = ini.value("SsBypass", DEFAULT_SSBYPASS).toInt();
    ini.endGroup();

    //output settings:
    ini.beginGroup( "Output settings" );
    acqMode         = ini.value("AcquisitionMode", DEFAULT_ACQ_MODE).toInt();
    interval        = ini.value("SamplingInterval", DEFAULT_INTVL).toInt();
    shots           = ini.value("MaxShots", DEFAULT_MAX_SHOTS).toInt();
    recTime         = ini.value("RecordingTime", DEFAULT_RECTIME).toInt();
    after           = ini.value("ShotsAfter", DEFAULT_AUTO_AFTER).toInt();
    thresholdsBehavior   = ini.value("ThresholdsBehavior", DEFAULT_THRESHOLDS).toInt();
    upThreshold     = ini.value("ShotUpThreshold",DEFAULT_AUTO_UP_THRESHOLD).toInt();
    dnThreshold     = ini.value("ShotDnThreshold",DEFAULT_AUTO_DN_THRESHOLD).toInt();
    detectionRange  = ini.value("ShotFrequencyRange", DEFAULT_AUTO_TRANGE).toInt();
    genScreenshots  = ini.value("GenerateScreenshots", DEFAULT_SCREENSHOTS).toInt();
    genDumps        = ini.value("GenerateDumps", DEFAULT_DUMPS).toInt();
    avgdScans       = ini.value("AveragedScans", DEFAULT_N_SCANS).toInt();
    joinTime        = ini.value("JoinTime", DEFAULT_JOIN_TIME).toInt();
    noiseLimit          = ini.value("NoiseLimit", DEFAULT_NOISE_LIMIT).toInt();
    palette             = ini.value("Palette", DEFAULT_WF_PALETTE).toInt();
    ini.endGroup();


    //storage settings:
    ini.beginGroup( "Storage settings" );
    minFree             = ini.value("MinFree", DEFAULT_FREE_SPACE).toInt();
    dataLasting         = ini.value("DataLasting", DEFAULT_DATA_LASTING).toInt();
    imgLasting          = ini.value("ImgLasting", DEFAULT_IMG_LASTING).toInt();
    DBsnap              = ini.value("DBsnap", DEFAULT_DB_SNAP).toInt();
    overwriteSnap       = ini.value("OverwriteSnap", DEFAULT_OVERWRITE_SNAP).toInt();
    eraseLogs           = ini.value("EraseLogs", DEFAULT_ERASE_LOGS).toInt();
    eraseShots          = ini.value("EraseShots", DEFAULT_ERASE_SHOTS).toInt();
    ini.endGroup();

    //preferences settings:
    ini.beginGroup( "Preferences" );
    maxEventLasting     = ini.value("MaxEventLasting", DEFAULT_MAX_EVENT_LASTING).toInt();
    wfSecTickMarks      = ini.value("WaterfallSecTickMarks", DEFAULT_WSEC_TICKS).toInt();
    hzTickMarks         = ini.value("WaterfallHzTickMarks", DEFAULT_HZ_TICKS).toInt();
    dBfsTickMarks       = ini.value("PowergraphDbmTickMarks", DEFAULT_DBFS_TICKS).toInt();
    wfRangeBoundaries   = ini.value("RangeBoundaries", DEFAULT_RANGE_BOUNDARIES).toInt();
    tooltips            = ini.value("TooltipsShown", DEFAULT_TOOLTIPS).toInt();
    pingOn              = ini.value("EnablePing", DEFAULT_PING).toInt();
    faultSoundOn        = ini.value("EnableFaultSound", DEFAULT_FAULT_SOUND).toInt();
    createDATdumps      = ini.value("DATdumps", DEFAULT_DAT_DUMPS).toInt();

    QString sDbfsOffset = ini.value("DbfsOffset", DEFAULT_DBFS_OFFSET).toString();
    dbFSoffset          = sDbfsOffset.toFloat(&ok);
    MY_ASSERT(ok == true)

    QString sDbfsGain   = ini.value("DbfsGain", DEFAULT_DBFS_GAIN).toString();
    dbFSgain            = sDbfsGain.toFloat(&ok);
    MY_ASSERT(ok == true)

    serverPort          = ini.value("ServerPort", DEFAULT_SERVER_PORT).toInt();

    QString serverAddrAsStr =  ini.value("ServerAddress", DEFAULT_SERVER_ADDRESS).toString();
    serverAddr          = QHostAddress( serverAddrAsStr );

    datagramDelay       = ini.value("DatagramDelay", DEFAULT_DGRAM_DELAY).toInt();
    scanMode     = ini.value("ScanIntegration", DEFAULT_SCAN_INTEGRATION).toInt();


    geom                = ini.value("MainGeom", DEFAULT_MAINWND_GEOM).toRect();

#ifdef WANT_FULL_UTC_DELTA
    QTime midnight(0,0);
    QTime delta         = QTime::fromString( ini.value( "UTCdelta", DEFAULT_UTC_DELTA ).toString() );
    UTCdelta            = midnight.secsTo(delta);
#else
    UTCdelta         = QTime::fromString( ini.value( "UTCdelta", DEFAULT_UTC_DELTA ).toString() );
#endif

    dsRatio = sampleRate / dsBw;
    ini.endGroup();

    //waterfall window
    ini.beginGroup( "Waterfall settings" );
    wfOffset        = ini.value("WaterfallOffset", DEFAULT_OFFSET).toInt();
    wfZoom          = ini.value("WaterfallZoom", DEFAULT_ZOOM).toInt();
    wfEccentricity  = ini.value("WaterfallEccentricity", DEFAULT_ECCENTRICITY).toInt();
    brightness      = ini.value("WaterfallBrightness", DEFAULT_BRIGHTNESS).toInt();
    contrast        = ini.value("WaterfallContrast", DEFAULT_CONTRAST).toInt();
    powerOffset     = ini.value("WaterfallPowerOffset", DEFAULT_POWEROFFSET).toInt();
    powerZoom       = ini.value("WaterfallPowerZoom", DEFAULT_POWERZOOM).toInt();    
    wwGeom          = ini.value("WaterfallGeom", DEFAULT_WATERFALL_GEOM).toRect();
    wwPaneGeom      = ini.value("WaterfallPaneGeom", DEFAULT_WATERFALL_GEOM).toRect();
    zoomLo          = ini.value("WaterfallZoomLowerBound", 0).toInt();
    zoomHi          = ini.value("WaterfallZoomHigherBound", DEFAULT_BW).toInt();
    plotS           = ini.value("WaterfallPlotS", DEFAULT_PLOT_S).toBool();
    plotN           = ini.value("WaterfallPlotN", DEFAULT_PLOT_N).toBool();
    plotD           = ini.value("WaterfallPlotD", DEFAULT_PLOT_D).toBool();
    plotA           = ini.value("WaterfallPlotA", DEFAULT_PLOT_A).toBool();
    plotU           = ini.value("WaterfallPlotU", DEFAULT_PLOT_U).toBool();
    plotL           = ini.value("WaterfallPlotL", DEFAULT_PLOT_L).toBool();
    ini.endGroup();

    //fix zoom values < 1.0 (==10)
    wfZoom = (wfZoom > DEFAULT_ZOOM)? wfZoom : DEFAULT_ZOOM;

    //notches
    ini.beginGroup( "Notch filters" );
    notches.clear();
    int sz = ini.beginReadArray("Notches");
    Notch nt;
    for (int i = 0; i < sz; ++i)
    {
         ini.setArrayIndex(i);
         nt.freq = ini.value("Frequency").toInt();
         nt.width = ini.value("Bandwidth").toInt();
         nt.begin = ini.value("BeginHz").toInt();
         nt.end = ini.value("EndHz").toInt();
         notches.append(nt);
    }

    ini.endArray();
    ini.endGroup();

    //recalc the resolution after config file loading
    updateResolution();

    MYINFO << "load(" << ini.fileName() << ")" << MY_ENDL;

    if(enableNotifications == true)
    {
        emit notifyOpened(&device);
    }
    changed = 0;
    srvChanged = 0;
    loading = false;
    MYINFO << "config loaded";

}

void Settings::save( QSettings& ini, bool enableNotifications  )
{
    if(localSettings == nullptr)
    {
        configFullPath =  ini.fileName();
        configName = QFileInfo( configFullPath ).baseName();

        //the local settings are placed in the same place
        //platform-dependent, the user has no way to change them
        //
        QString targetInstance;
        targetInstance = QString("%1-%2").arg(TARGET_NAME).arg(configName);

        localSettings = new QSettings(COMPANY_NAME, targetInstance);
        MY_ASSERT( nullptr !=  localSettings)
    }

    if(changed > 0)
    {
        rtsRevision++;
    }


    ini.beginGroup( "General" );
    ini.setValue( "RTSformatRev", rtsFormatRev );
    ini.setValue( "RTSrevision", rtsRevision );
    ini.setValue( "EchoesVersion", echoesVersion );
    ini.endGroup();

    //device settings:
    ini.beginGroup( "Device settings" );
    ini.setValue( "Device", device );
    ini.setValue( "SampleRate", sampleRate );
    ini.setValue( "Bandwidth", bandwidth );
    ini.setValue( "Tune", tune );
    ini.setValue( "Gain", gain );
    ini.setValue( "PpmError", ppmError );
    ini.setValue( "Antenna", antenna );
    ini.setValue( "FrequencyRange", fRange );
    ini.setValue( "BandwidthRange", bRange );
    ini.setValue( "SampleRateRange", sRange );
    ini.endGroup();

    //FFT settings:
    ini.beginGroup( "FFT settings" );
    ini.setValue( "DownsampledBand", dsBw );
    ini.setValue( "FFTwindow", static_cast<int>( ftw ) );
    ini.setValue( "FFTbins", fftBins );

    QString sRes;
    float fRes = static_cast<float>(resolution) / 10e8F;
    sRes.setNum(fRes);
    ini.setValue( "Resolution", sRes );
    ini.setValue( "SsBypass", ssBypass );
    ini.endGroup();

    //output settings:
    ini.beginGroup( "Output settings" );
    ini.setValue( "AcquisitionMode", acqMode );
    ini.setValue( "SamplingInterval", interval );
    ini.setValue( "MaxShots", shots );
    ini.setValue( "RecordingTime", recTime );
    ini.setValue( "ShotsAfter", after );
    ini.setValue( "ThresholdsBehavior", thresholdsBehavior );
    ini.setValue( "ShotUpThreshold", upThreshold );
    ini.setValue( "ShotDnThreshold", dnThreshold );
    ini.setValue( "ShotFrequencyRange", detectionRange );
    ini.setValue( "GenerateScreenshots", genScreenshots );
    ini.setValue( "GenerateDumps", genDumps );
    ini.setValue( "AveragedScans", avgdScans );
    ini.setValue( "JoinTime", joinTime );
    ini.setValue( "NoiseLimit", noiseLimit );
    ini.setValue( "Palette", palette );
    ini.endGroup();

    //storage settings:
    ini.beginGroup( "Storage settings" );
    ini.setValue( "EraseLogs", eraseLogs );
    ini.setValue( "EraseShots", eraseShots );
    ini.setValue( "OverwriteSnap", overwriteSnap );
    ini.setValue( "DBsnap", DBsnap );
    ini.setValue( "MinFree", minFree );
    ini.setValue( "DataLasting", dataLasting );
    ini.setValue( "ImgLasting", imgLasting );
    ini.endGroup();

    //preferences
    ini.beginGroup( "Preferences" );
    ini.setValue( "MaxEventLasting", maxEventLasting );
    ini.setValue( "WaterfallSecTickMarks", wfSecTickMarks );
    ini.setValue( "WaterfallHzTickMarks", hzTickMarks );
    ini.setValue( "PowergraphDbmTickMarks", dBfsTickMarks );
    ini.setValue( "RangeBoundaries", wfRangeBoundaries );
    ini.setValue( "TooltipsShown", tooltips );
    ini.setValue( "EnablePing", pingOn );
    ini.setValue( "EnableFaultSound", faultSoundOn );
    ini.setValue( "DATdumps", createDATdumps );
    ini.setValue( "DatagramDelay", datagramDelay );
    ini.setValue( "ServerPort", serverPort );
    ini.setValue( "ServerAddress", serverAddr.toString() );
    ini.setValue( "ScanIntegration", scanMode );
    ini.setValue( "MainGeom", geom );
#ifdef WANT_FULL_UTC_DELTA
    QTime delta(0,0);
    delta = delta.addSecs(UTCdelta);
    ini.setValue( "UTCdelta", delta.toString());
#else
    ini.setValue( "UTCdelta", UTCdelta.toString() );
#endif

    QString sDbfsGain;
    sDbfsGain.setNum(dbFSgain);
    ini.setValue( "DbfsGain", sDbfsGain );

    QString sDbfsOffset;
    sDbfsOffset.setNum(dbFSoffset);
    ini.setValue( "DbfsOffset", sDbfsOffset );

    ini.endGroup();

    //waterfall window
    ini.beginGroup( "Waterfall settings" );
    ini.setValue( "WaterfallOffset", wfOffset );
    ini.setValue( "WaterfallZoom", wfZoom );
    ini.setValue( "WaterfallEccentricity", wfEccentricity );
    ini.setValue( "WaterfallBrightness", brightness );
    ini.setValue( "WaterfallContrast", contrast );
    ini.setValue( "WaterfallPowerZoom", powerZoom );
    ini.setValue( "WaterfallPowerOffset", powerOffset );    
    ini.setValue(  "WaterfallGeom", wwGeom );
    ini.setValue(  "WaterfallPaneGeom", wwPaneGeom );
    ini.setValue(  "WaterfallZoomLowerBound", zoomLo );
    ini.setValue(  "WaterfallZoomHigherBound", zoomHi );
    ini.setValue(  "WaterfallPlotS", plotS );
    ini.setValue(  "WaterfallPlotN", plotN );
    ini.setValue(  "WaterfallPlotD", plotD );
    ini.setValue(  "WaterfallPlotA", plotA );
    ini.setValue(  "WaterfallPlotU", plotU );
    ini.setValue(  "WaterfallPlotL", plotL );
    ini.endGroup();

    //notches
    ini.beginGroup( "Notch filters" );
    int sz = notches.count();
    ini.beginWriteArray("Notches");
    Notch nt;
    for (int i = 0; i < sz; ++i)
    {
         ini.setArrayIndex(i);
         nt = notches.at(i);
         ini.setValue("Frequency", nt.freq);
         ini.setValue("Bandwidth", nt.width);
         ini.setValue("BeginHz", nt.begin);
         ini.setValue("EndHz", nt.end);
    }

    ini.endArray();
    ini.endGroup();

    ini.sync();

    MYINFO << "save(" << ini.fileName() << ")" << MY_ENDL;
    if(enableNotifications == true && changed > 0)
    {
        emit notifySaved();
    }

    if(ini.fileName() != DEFAULT_CONFIG_NAME)
    {
        if(enableNotifications == true)
        {
            emit notifySavedAs();
        }
    }

    changed = 0;
    srvChanged = 0;
}


void Settings::loadLocal()
{

    MYINFO << "loadLocal(" << localSettings->fileName() << ")" << MY_ENDL;
    localSettings->beginGroup( "LocalSettings" );
    resetCounter = localSettings->value("ResetCounter", 0).toInt();
    lastDBsnapshot = localSettings->value("LastDBsnapshot", DEFAULT_DATE).toString();
    logo = localSettings->value("Logo", DEFAULT_LOGO).toString();
    localSettings->endGroup();

    if(resetCounter > 0)
    {
        MYWARNING << "Echoes has been interrupted abnormally for " << resetCounter << " times. Quit the program pressing Exit then restart it to get rid of this message.";
    }

}



void Settings::saveLocal()
{
    MYINFO << "saveLocal(" << localSettings->fileName() << ")" << MY_ENDL;
    localSettings->beginGroup( "LocalSettings" );

    localSettings->setValue( "ResetCounter", resetCounter );
    localSettings->setValue( "LastDBsnapshot", lastDBsnapshot );
    localSettings->setValue( "Logo", logo );
    localSettings->endGroup();
}



//main window settings


void Settings::loadConfig( QString& name, bool enableNotifications )
{
    //opens a new configuration

    if (true /*name != configName*/)
    {
        configFullPath = name;
        MYINFO << "getConfig(" << configFullPath << ")" << MY_ENDL;
        configName = QFileInfo( configFullPath ).baseName();
        reset();
        QSettings ini( configFullPath, QSettings::IniFormat );
        load(ini, enableNotifications);
    }
}

void Settings::setConfigName(XQDir wd, QString& name  )
{
    //perform a SaveAs
    configName = name;
    MYINFO << "setConfigName(" << configName << ")" << MY_ENDL;
    QString rts = configName+CONFIG_EXT;
    QSettings ini( wd.absoluteFilePath(rts), QSettings::IniFormat );
    rtsRevision = 0;
    save(ini, true);
}



//device settings

void Settings::setDevice( const QString& dev, bool enableNotifications )
{
    if (dev.size() > 0)
    {
        QString chopDev = dev.left(MAX_DEVICE_ID_LEN);
        if(device != chopDev)
        {
            device = chopDev;
            MYINFO << "**** DEVICE IS CHANGED ****";
            MYINFO << "setDevice(" << device << ")" << MY_ENDL;

            if(enableNotifications == true)
            {
                emit notifySetDevice();
            }
            changed++;
            srvChanged++;
        }
    }
}

QString Settings::getDeviceNoIdx(QString& dev)
{
    if (dev.contains(":"))
    {
        int end = device.indexOf("SN");
        QString chopped = device.section(':', 1);
        chopped = chopped.left(end);
        MYINFO << "getDeviceNoIdx(" << device << ") = " << chopped << MY_ENDL;
        return chopped;
    }
    return dev;
}


void Settings::setSampleRate(int value  , bool enableNotifications)
{
    if (value != sampleRate)
    {       
        sampleRate = value;
        MYINFO << "setSampleRate(" << value << ")" << MY_ENDL;

        //the resolution changes to mantain FFT bins unchanged
        double dRes = static_cast<double>(dsBw) / fftBins;
        MYDEBUG << "resolution is " << dRes << " Hz" << MY_ENDL;
        resolution = static_cast<qint64>(dRes * 10e8);
        if(enableNotifications == true)
        {
            emit notifySetSampleRate();
            emit notifySetResolution();
        }

        changed++;
        srvChanged++;
    }
}


void Settings::setBandwidth( int value, bool enableNotifications )
{
    if (value != bandwidth)
    {
        bandwidth = value;
        MYINFO << "setBandwidth(" << value << ")" << MY_ENDL;

        if(enableNotifications == true)
        {
            emit notifySetBandwidth();
        }
        changed++;
    }
}


void Settings::setTune(qint64 value, bool enableNotifications)
{
    if (value != tune)
    {
        MY_ASSERT(tune >= 0)
        tune = value;
        MYINFO << "setTune(" << value << ")" << MY_ENDL;

        if(enableNotifications == true)
        {
            emit notifySetTune();
        }
        changed++;
        srvChanged++;
    }
}



void Settings::setGain( int value, bool enableNotifications  )
{
    if (value != gain)
    {
        gain = value;
        MYINFO << "setGain(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetGain();
        }

        changed++;
        srvChanged++;
    }
}

void Settings::setError(double value, bool enableNotifications)
{
    //max excursion: +/- 100.0
    int iValue = static_cast<int>(value);
    if ( iValue != ppmError )
    {
        ppmError = iValue;
        MYINFO << "setError(" << iValue << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetError();
        }
        changed++;
        srvChanged++;
    }
}


void Settings::setAntenna( QString value, bool enableNotifications  )
{
    if (value != antenna)
    {
        antenna = value;
        MYDEBUG << "setAntenna(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetAntenna();
        }
        changed++;
        srvChanged++;
    }
}


void Settings::setSampleRateRange( QString value, bool enableNotifications )
{
    if (value != sRange)
    {
        sRange = value;
        MYDEBUG << "setSampleRateRange(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetSampleRateRange();
        }
        changed++;
        srvChanged++;
    }
}

void Settings::setBandwidthRange( QString value, bool enableNotifications )
{
    if (value != bRange)
    {
        bRange = value;
        MYDEBUG << "setBandwidthRange(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetBandwidthRange();
        }
        changed++;
        srvChanged++;
    }
}


void Settings::setFreqRange( QString value, bool enableNotifications )
{
    if (value != fRange)
    {
        fRange = value;
        MYDEBUG << "setFreqRange(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetFreqRange();
        }
        changed++;
        srvChanged++;
    }
}


//FFT settings

bool Settings::setDsBandwidth( int value, bool enableNotifications )
{
    bool done = false;
    if (value > 0 && value != dsBw)
    {
        int ratio = sampleRate / value;

        //if downsampling is active (ratio > 1)
        //the data required for downsampling is ratio times the fftBins.
        //The overall data requested must not exceed the device's MTU
        //unless direct buffers are active
        if((fftBins * ratio) <= maxResMTU)
        {
            dsRatio = ratio;
            dsBw = value;
            MYINFO << "setDsBandwidth(" << dsBw << "), dsRatio=" << dsRatio << MY_ENDL;

            //the FFT resolution must be recalc mantaining the same fftBins:
            double dRes = dsBw;
            dRes /= fftBins;

            MYDEBUG << "adjusted resolution after bandwidth change =" << dRes << " - fftBins=" << fftBins << MY_ENDL;
            resolution = static_cast<qint64>(dRes * 10e8);

            if(enableNotifications == true)
            {
                emit notifySetDsBandwidth();
                emit notifySetResolution();
            }
            changed++;
            srvChanged++;
            done = true;
        }
    }
    return done;
}


void Settings::setWindow( int value, bool enableNotifications  )
{
    if ( value != static_cast<int>(ftw) )
    {
        ftw = static_cast<FFT_WINDOWS>( value );
        MYDEBUG << "setWindow(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetWindow();
        }
        changed++;
    }
}



bool Settings::decResolution( bool enableNotifications )
{
    bool done = false;

    //increments FFT resolution (less Hz per point)

    //actual FFT resolution in points:
    int checkBins = 0;

    int exp = static_cast<int>( ceilf( log2f( fftBins ) ) );
    exp++;
    checkBins = (1 << exp);
    double dRes = 0;

    if(ssBypass)
    {
        dRes = static_cast<double>(sampleRate) / checkBins;
    }
    else
    {
        dRes = static_cast<double>(dsBw) / checkBins;
    }

    //remember: dRes and maxRes are in Hz, the smaller is Hz, the higher is resolution
    //so dRes must not be smaller than maxRes, unless direct buffers are used
    MYINFO << "maxRes=" << maxResHz << " dRes=" << dRes;
    if(checkBins <= MAX_FFT_BINS)
    {
        if((checkBins * dsRatio) <= maxResMTU)
        {
            fftBins = checkBins;
            MYINFO << "decResolution+(" << dRes << ") new fftBins=" << fftBins << MY_ENDL;
            resolution = static_cast<qint64>(dRes * 10e8);

            changed++;
            srvChanged++;
            if(enableNotifications == true)
            {
                emit notifySetResolution();
            }
            done = true;
        }
    }

    return done;
}

bool Settings::setResBins( int FFTbins, bool enableNotifications )
{
    bool isFail = true;

    //actual FFT resolution in points:
    int checkBins = 0;

    int exp = static_cast<int>( ceilf( log2f( FFTbins ) ) );
    checkBins = (1 << exp);
    double dRes = 0;
    if(ssBypass == Qt::Checked)
    {
        dRes = static_cast<double>(sampleRate) / checkBins;
    }
    else
    {
        dRes = static_cast<double>(dsBw) / checkBins;
    }
    //remember: dRes and maxRes are in Hz, the smaller is Hz, the higher is resolution
    //so dRes must not be smaller than maxRes, unless direct buffers are used
    if(checkBins <= MAX_FFT_BINS &&  dRes >= maxResHz)
    {
        if((checkBins * dsRatio) <= maxResMTU)
        {
            fftBins = checkBins;
            MYDEBUG << "setResBins+(" << fftBins << ") new resolution=" << dRes << " Hz" << MY_ENDL;
            resolution = static_cast<qint64>(dRes * 10e8);
            changed++;
            srvChanged++;
            if(enableNotifications == true)
            {
                emit notifySetResolution();
            }
            isFail = false;
        }
    }
    return isFail;
}

bool Settings::incResolution( bool enableNotifications )
{
    bool done = false;

    //decrements FFT resolution (more Hz per point)

    //actual FFT resolution in points:
    int checkBins = 0;

    int exp = static_cast<int>( ceilf( log2f( fftBins ) ) );
    if(exp > 2)
    {
        exp--;
    }
    checkBins = (1 << exp);
    double dRes = 0;

    if(ssBypass)
    {
        dRes = static_cast<double>(sampleRate) / checkBins;
    }
    else
    {
        dRes = static_cast<double>(dsBw) / checkBins;
    }

    fftBins = checkBins;
    MYINFO << "incResolution+(" << dRes << ") new fftBins=" << fftBins << MY_ENDL;
    resolution = static_cast<qint64>(dRes * 10e8);

    changed++;
    srvChanged++;
    if(enableNotifications == true)
    {
        emit notifySetResolution();
    }
    done = true;
    return done;
}

bool Settings::updateResolution( bool enableNotifications )
{
    //fixes the FFT resolution when it exceeds the maximum
    //alloweed by device and current settings
    bool fixed = false;

    //actual FFT resolution in points:
    int checkBins = 0;

    int exp = static_cast<int>( ceilf( log2f( fftBins ) ) );
    checkBins = (1 << exp);
    double dRes = 0;
    if(ssBypass != 0)
    {
        dRes = static_cast<double>(sampleRate) / checkBins;
    }
    else
    {
        dRes = static_cast<double>(dsBw) / checkBins;
    }

    //remember: dRes and maxRes are in Hz, the smaller is Hz, the higher is resolution
    //so dRes must not be smaller than maxRes, unless direct buffers are used
    if(checkBins <= MAX_FFT_BINS && dRes >= maxResHz)
    {
        if((checkBins * dsRatio) <= maxResMTU)
        {
            fftBins = checkBins;
        }
    }
    else
    {
        MYINFO << "Required resolution " << dRes
               << " exceeds the device's MTU, fixing to " << maxResHz;
        fftBins = static_cast<double>(sampleRate) / maxResHz;
        return fixed;
    }

    MYDEBUG << "updateResolution() fftBins=" << fftBins << " resolution=" << dRes << MY_ENDL;
    resolution = static_cast<qint64>(dRes * 10e8);
    if(enableNotifications == true)
    {
        emit notifySetResolution();
    }
    return fixed;
}

void Settings::setMaxRes( double mtu )
{
    maxResMTU = static_cast<int>(mtu);
    maxResHz = static_cast<double>(dsBw) / mtu;
    MYINFO << "setting maxRes: " << maxResHz << " hz, streamMTU:" << maxResMTU;
}

void Settings::setSsBypass( int value, bool enableNotifications )
{
    if (value != ssBypass)
    {
        ssBypass = value;
        MYDEBUG << "ssBypass(" << value << ")" << MY_ENDL;

        double dRes = 0;
        if(ssBypass)
        {
            dRes = static_cast<double>(sampleRate) / fftBins;
        }
        else
        {
            dRes = static_cast<double>(dsBw) / fftBins;
        }

        resolution = static_cast<qint64>(dRes * 10e8);

        MYINFO << "adjusted resolution after bypass change =" << dRes << "Hz = fftBins=" << fftBins << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetResolution();
            emit notifySsBypass();
        }

        changed++;
    }
}




void Settings::setEccentricity( int value, bool enableNotifications )
{
    if (value != wfEccentricity)
    {
        wfEccentricityBak = wfEccentricity;
        wfEccentricity = value;
        MYINFO << "setEccentricity(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetEccentricity();
        }
        changed++;
    }
}


//output settings

void Settings::setAcqMode( int value, bool enableNotifications )
{
    QMutexLocker ml(&protectionMutex);
    if (value != acqMode)
    {
        acqMode = value;
        MYDEBUG << "notifySetAcqMode(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetAcqMode();
        }
        changed++;
    }
}

void Settings::setInterval( int value, bool enableNotifications  )
{
    if (value != interval)
    {
        interval = value;
        MYDEBUG << "setInterval(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetInterval();
        }
        changed++;
    }
}

void Settings::setShots( int value, bool enableNotifications  )
{
    if (value != shots)
    {
        shots = value;
        MYDEBUG << "setShots(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetShots();
        }
        changed++;
    }
}


void Settings::setRecTime( int value, bool enableNotifications  )
{
    if (value != recTime)
    {
        recTime = value;
        MYDEBUG << "setRecTime(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetRecTime();
        }
        changed++;
    }
}


void Settings::setAfter( int value, bool enableNotifications  )
{
    if (value != after)
    {
        after = value;
        MYDEBUG << "setAfter(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetAfter();
        }
        changed++;
    }
}


void Settings::setThresholdsBehavior(int value, bool enableNotifications)
{
    if (value != thresholdsBehavior)
    {
        thresholdsBehavior = value;
        MYDEBUG << "setThresholdsBehavior(" << value << ")" << MY_ENDL;
        changed++;
        if(enableNotifications)
        {
            emit notifySetThresholdsBehavior();
        }
    }
}

void Settings::setUpThreshold(double value, bool enableNotifications )
{
    int iValue = static_cast<int>(value * 100.0);
    if ( iValue != upThreshold )
    {
        upThreshold = iValue;
        MYDEBUG << "setUpThreshold(" << iValue << ")" << MY_ENDL;

        if(enableNotifications == true)
        {
            emit notifySetUpThreshold();
        }
        changed++;
    }
}

void Settings::setDnThreshold(double value, bool enableNotifications  )
{
    int iValue = static_cast<int>(value * 100.0);
    if ( iValue != dnThreshold )
    {
        dnThreshold = iValue;
        MYDEBUG << "setDnThreshold(" << iValue << ")" << MY_ENDL;

        if(enableNotifications == true)
        {
            emit notifySetDnThreshold();
        }
        changed++;
    }
}


void Settings::setDetectionRange( int value, bool enableNotifications  )
{
    if (value != detectionRange)
    {
        detectionRange = value;
        MYDEBUG << "setTrange(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetDetectionRange();
        }
        changed++;
    }
}

void Settings::setDumps( int value  )
{
    if (value != genDumps)
    {
        genDumps = value;
        MYDEBUG << "setDumps(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setScreenshots( int value  )
{
    if (value != genScreenshots)
    {
        genScreenshots = value;
        MYDEBUG << "setScreenshots(" << value << ")" << MY_ENDL;
        changed++;
    }
}



void Settings::setAvgdScans( int value )
{
    if (value != avgdScans)
    {
        avgdScans = value;
        MYDEBUG << "setAvgdScans(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setJoinTime( int value )
{
    if (value != joinTime)
    {
        joinTime = value;
        MYDEBUG << "setJoinTime( " << value << " )" << MY_ENDL;
        changed++;
    }
}


void Settings::setScanMode( int value, bool enableNotifications )
{
    QMutexLocker ml(&protectionMutex);
    if (value != scanMode)
    {
        scanMode = value;
        MYDEBUG << "notifySetScanMode(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetScanMode();
        }
        changed++;
    }
}




//waterfall settings:

void Settings::setOffset( int value, bool enableNotifications  )
{
    if (value != wfOffset)
    {
        wfOffset = value;
        MYINFO << "setOffset(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetOffset();
        }
        changed++;
    }
}


void Settings::setZoom( int value, bool enableNotifications  )
{
    if (value != wfZoom)
    {
        wfZoom = value;
        MYDEBUG << "setZoom(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetZoom();
        }
        changed++;
    }
}


void Settings::setZoomedRange( int lo, int hi )
{
    //these values are calculated by waterfall window
    //but to allow working in console mode, they must
    //also be stored in configuration
    if(zoomLo <= zoomHi)
    {
        zoomLo = lo;
        zoomHi = hi;
    }
}


void Settings::setBrightness( int value )
{
    if (value != brightness)
    {
        brightness = value;
        MYDEBUG << "setBrightness(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setContrast( int value )
{
    if (value != contrast)
    {
        contrast = value;
        MYDEBUG << "setContrast(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setPowerOffset( int value )
{
    if (value != powerOffset)
    {
        powerOffset = value;
        MYDEBUG << "setPowerOffset(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setPowerZoom( int value )
{
    if (value != powerZoom)
    {
        powerZoom = value;
        MYDEBUG << "setPowerZoom(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setNotches( QList<Notch>& ntList )
{
    notches = ntList;
    MYDEBUG << "setNotches(" << ntList.count() << " notches )";
    changed++;
}


void Settings::setPlotS( bool value, bool enableNotifications  )
{
    if (value != plotS)
    {
        plotS = value;
        MYDEBUG << "setPlotS(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySplot();
        }
        changed++;
    }
}

void Settings::setPlotN( bool value, bool enableNotifications  )
{
    if (value != plotN)
    {
        plotN = value;
        MYDEBUG << "setPlotN(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifyNplot();
        }
        changed++;
    }
}

void Settings::setPlotD( bool value, bool enableNotifications  )
{
    if (value != plotD)
    {
        plotD = value;
        MYDEBUG << "setPlotD(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifyDplot();
        }
        changed++;
    }
}

void Settings::setPlotA( bool value, bool enableNotifications  )
{
    if (value != plotA)
    {
        plotA = value;
        MYDEBUG << "setPlotA(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifyAplot();
        }
        changed++;
    }
}

void Settings::setPlotU( bool value, bool enableNotifications  )
{
    if (value != plotU)
    {
        plotU = value;
        MYDEBUG << "setPlotU(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifyUplot();
        }
        changed++;
    }
}

void Settings::setPlotL( bool value, bool enableNotifications  )
{
    if (value != plotL)
    {
        plotL = value;
        MYDEBUG << "setPlotL(" << value << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifyLplot();
        }
        changed++;
    }
}

//storage settings

void Settings::setEraseLogs( int value )
{
    if (value != eraseLogs)
    {
        eraseLogs = value;
        MYDEBUG << "setEraseLogs(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setEraseShots( int value )
{
    if (value != eraseShots)
    {
        eraseShots = value;
        MYDEBUG << "setEraseShots(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setDataLasting( int value )
{
    if (value != dataLasting)
    {
        dataLasting = value;
        MYDEBUG << "setDataLasting(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setOverwriteSnap( int value )
{
    if (value != overwriteSnap)
    {
        overwriteSnap = value;
        MYDEBUG << "setOverwriteSnap(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setDBsnap( int value )
{
    if (value != DBsnap)
    {
        DBsnap = value;
        MYDEBUG << "setDBsnap(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setImgLasting( int value )
{
    if (value != imgLasting)
    {
        imgLasting = value;
        MYDEBUG << "setImgLasting(" << value << ")" << MY_ENDL;
        changed++;
    }
}



void Settings::setMinFree(int value )
{
    if (value != minFree)
    {
        minFree = value;
        MYDEBUG << "setMinFree(" << value << ")" << MY_ENDL;
        changed++;
    }
}




//preferences
void Settings::setMaxEventLasting( int value  )
{
    if (value != maxEventLasting)
    {
        maxEventLasting = value;
        MYDEBUG << "setMaxEventLasting(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setSec(int value , bool enableNotifications)
{
    if (value != wfSecTickMarks)
    {
        wfSecTickMarks = value;
        MYDEBUG << "setSec(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetSec();
        }
        changed++;
    }
}

void Settings::setHz(int value , bool enableNotifications)
{
    if (value != hzTickMarks)
    {
        hzTickMarks = value;
        MYDEBUG << "setHz(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetHz();
        }
        changed++;
    }
}

void Settings::setDbfs( int value, bool enableNotifications )
{
    if (value != dBfsTickMarks)
    {
        dBfsTickMarks = value;
        MYDEBUG << "setDbm(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifySetDbfs();
        }
        changed++;
    }
}


void Settings::setRangeBoundaries(int value , bool enableNotifications)
{
    if (value != wfRangeBoundaries)
    {
        wfRangeBoundaries = value;
        MYDEBUG << "setRangeBoundaries(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifyRangeBoundaries();
        }
        changed++;
    }
}

void Settings::setTooltips( int value )
{
    if (value != tooltips)
    {
        tooltips = value;
        MYDEBUG << "setTooltips(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setPing( int value )
{
    if (value != pingOn)
    {
        pingOn = value;
        MYDEBUG << "setPing(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setFaultSound( int value )
{
    if (value != faultSoundOn)
    {
        faultSoundOn = value;
        MYDEBUG << "setFaultSound(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setDATdumps( int value )
{
    if (value != createDATdumps)
    {
        createDATdumps = value;
        MYDEBUG << "setDATdumps(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setPalette( int value )
{
    if (value != palette)
    {
        palette = value;
        MYDEBUG << "setPalette(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setDbfsOffset(double value, bool enableNotifications)
{
    if (value != dbFSoffset)
    {
        //The value comes from a QDoubleSpinBox so it's a double
        //and must be converted to float
        dbFSoffset = static_cast<float>(value);;
        MYDEBUG << "setDbfsOffset(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifyDbfsOffset();
        }
        changed++;
    }
}

void Settings::setDbfsGain(double value, bool enableNotifications)
{
    if (value != dbFSgain)
    {
        //The value comes from a QDoubleSpinBox so it's a double
        //and must be converted to float
        dbFSgain = static_cast<float>(value);
        MYDEBUG << "setDbfsGain(" << value << ")" << MY_ENDL;
        if(enableNotifications == true)
        {
            emit notifyDbfsGain();
        }
        changed++;
    }
}

void Settings::setNoiseLimit(int value)
{
    if (value != noiseLimit)
    {
        noiseLimit = value;
        MYDEBUG << "setNoiseLimit(" << value << ")" << MY_ENDL;
        changed++;
    }
}


void Settings::setDatagramDelay( int value  )
{
    if (value != datagramDelay)
    {
        datagramDelay = value;
        MYDEBUG << "setDatagramDelay(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setServerPort( int value  )
{
    if (value != serverPort)
    {
        serverPort = value;
        MYDEBUG << "setServerPort(" << value << ")" << MY_ENDL;
        changed++;
    }
}

void Settings::setServerAddress(const QString& ipAddr)
{
    QHostAddress value(ipAddr);
    if (value != serverAddr)
    {
        serverAddr = value;
        MYDEBUG << "setServerAddress(" << ipAddr << ")" << MY_ENDL;
        changed++;
    }
}


//-------------------------------------------------------------------
// local settings
//-------------------------------------------------------------------


void Settings::setMainGeometry( QRect newGeom  )
{
    if(newGeom != geom)
    {
        geom = newGeom;
        MYDEBUG << "setGeometry(" << geom << ")" << MY_ENDL;
        //saveLocal();
    }
}

void Settings::setWfGeometry(QRect newWwGeom, QRect newWwPaneGeom , bool enableNotifications  )
{
    if(newWwGeom != wwGeom ||
       newWwPaneGeom != wwPaneGeom)
    {
        wwGeom = newWwGeom;
        wwPaneGeom = newWwPaneGeom;
        MYDEBUG << "setWfGeometry(" << wwGeom << ")";
        MYDEBUG << " waterfall pane widget geometry: " << wwPaneGeom << ")" << MY_ENDL;
        if(enableNotifications)
        {
            emit notifySetWfGeometry();
        }

    }
}

int Settings::getPeakIntervalFreq()
{
    qint64 newIntervalHz = 0;
    if(ww != nullptr)
    {
        int halfIntvl = (((zoomHi - zoomLo)/2) * detectionRange) / 100;
        newIntervalHz = zoomLo + ((zoomHi - zoomLo)/2) + wfEccentricity;
        if((newIntervalHz - halfIntvl) < zoomLo)
        {
            newIntervalHz = zoomLo + halfIntvl;
        }
        else if((newIntervalHz + halfIntvl) > zoomHi)
        {
            newIntervalHz = zoomHi - halfIntvl;
        }
    }
    intervalHz = newIntervalHz;
    MYDEBUG << "intervalHz=" << intervalHz;
    return intervalHz;
}

void Settings::setLogo( QString logoPix )
{
    logo = logoPix;
    MYDEBUG << "setLogo()" << MY_ENDL;
    changed++;
}


//-------------------------------------------------------------------
// utility methods
//-------------------------------------------------------------------


float Settings::lowestElem( QList<float>* elems )
{
    float lowVal = FLT_MAX;
    //int lowElem = 0;

    for (int i = 0; i < elems->count(); ++i)
    {
        if(elems->at(i) < lowVal)
        {
            lowVal = elems->at(i);
            //lowElem = i;
        }
    }
    return lowVal;
}

float Settings::highestElem( QList<float>* elems )
{
    float highVal = -FLT_MAX;
    //int highElem = 0;

    for (int i = 0; i < elems->count(); ++i)
    {
        if(elems->at(i) > highVal)
        {
            highVal = elems->at(i);
            //highElem = i;
        }
    }
    return highVal;
}


float Settings::funcGen( QMap<float,float>* func, float xInput)
{
    float  ratio = 0;
    float  sideX = 0;
    float  sideY = 0;
    float  yOutput = 0;

    // Ckeck the minimum numbers of items
    if(func->count() < 2)
    {
        // Impossible to interpolate
        yOutput = 0;
        return yOutput;
    }

    QList<float> yValues = func->values();
    QList<float> xValues = func->keys();

    // Check Lo Limit
    float lowest = lowestElem(&xValues);
    if(xInput < lowest)
    {
        //clipping underflows
        yOutput = func->value(lowest);
        return yOutput;
    }

    // Check Hi Limit
    float highest = highestElem(&xValues);
    if(xInput > highest)
    {
        //clipping overflows
        yOutput = func->value(highest);
        return yOutput;
    }

    for(int i = 0; i < func->count(); i++)
    {
        if(xInput >= xValues.at(i) && xInput <= xValues.at(i+1) )
        {
            ratio = (
                    (yValues.at(i+1) - yValues.at(i)) /
                    (xValues.at(i+1) - xValues.at(i))
                );

            sideX = xInput - xValues.at(i);
            sideY = (sideX * ratio);

            yOutput = sideY + yValues.at(i);
            return yOutput;
        }
    }

    /* Error */
    yOutput = 0;
    return yOutput;
}

float Settings::pround(float x, int precision)
{
    float power = powf(10, precision);
    float xpowered = x * power;
    float xlated;
    float frac = modff(xpowered, &xlated);
    bool minus = (xlated < 0) ? true : false;

    if(minus == false && frac > 0.555555F)
    {
        xlated += 1;
    }

    if(minus == true && frac < -0.555555F)
    {
        xlated -= 1;
    }
    xlated = xlated / power;
    return xlated;
}


QString Settings::thresholdBehaviorString()
{
    switch( thresholdsBehavior )
    {
        case TB_ABSOLUTE:
            return tr("absolute");

        case TB_DIFFERENTIAL:
            return tr("differential");

        case TB_AUTOMATIC:
            return tr("automatic");

    }

    return QString("FAILURE");
}


int Settings::fpCmp( double val1, double val2, int decimals )
{
    double multiplier = pow(10, decimals);
    int i1 = static_cast<int>( val1 * multiplier );
    int i2 = static_cast<int>( val2 * multiplier );
    return i2-i1;
}

int Settings::fpCmpf( float val1, float val2, int decimals )
{
    float multiplier = powf(10, decimals);
    int i1 = static_cast<int>( val1 * multiplier );
    int i2 = static_cast<int>( val2 * multiplier );
    return i2-i1;
}

int Settings::fuzzyCompare(float a, float b, float tolerance)
{
    float diff = fabs(a-b);
    float percDiff = fabs((diff * 100) / a);

    if(percDiff > tolerance)
    {
        return (a > b)? 1 : -1;
    }

    return 0;
}

QStringList Settings::fromStrVect(std::vector<std::string> &strVec )
{
    QStringList retList;
    for(uint8_t i = 0; i < strVec.size(); ++i)
    {
        retList.append( strVec.at(i).c_str() );
        if(i >= MAX_ITEMS_IN_LISTS)
        {
            //too many values, ignore others
            break;
        }
    }
    return retList;
}

QStringList Settings::fromRangeVect( SoapySDR::RangeList& rangeVec, int precision, int div, bool compact )
{
    QStringList retList;
    SoapySDR::Range r;
    uint values = 0, step = 0;
    QString numA, numB;
    div = (div < 1)? 1 : div;
    for(uint8_t i = 0; i < rangeVec.size(); ++i)
    {
        bool explode = compact;
        r = rangeVec.at(i);
        step = (r.step() == 0.0)? 1 : static_cast<uint>( r.step() );

        //for a limited number of values, returns a list set with
        //all the possible values
        values = static_cast<uint>((r.maximum() - r.minimum()) / step);
        if(values <= MAX_ITEMS_IN_LISTS)
        {
            explode = true;
            for(double d = r.minimum(); d <= r.maximum(); d += step)
            {
                double ddiv = d / div;
                numA.setNum(ddiv, 'f', precision);
                retList.append( QString("%1").arg(numA) );
            }
        }


        if(explode == false)
        {
            //if possible values are too many, the list returned contains
            //a string representation of the ranges
            double ddiv = r.minimum() / div;
            numA.setNum(ddiv, 'f', precision);
            ddiv = r.maximum() / div;
            numB.setNum(ddiv, 'f', precision);
            if(numB == numA)
            {
                //this range contains a scalar instead: creates a range
                //putting together the first and last of these "ranges"
                //then quits
                r = rangeVec.at( rangeVec.size() - 1 );
                ddiv = r.maximum() / div;
                numB.setNum(ddiv, 'f', precision);
                retList.clear();
                retList.append( QString("%1:%2").arg(numA).arg(numB) );
                break;
            }
            retList.append( QString("%1:%2").arg(numA).arg(numB) );
        }

        if(i >= MAX_ITEMS_IN_LISTS)
        {
            //too many values, ignore others
            break;
        }
    }
    return retList;
}

int Settings::getClosestIntValueFromList(QStringList& strList, QString target)
{
    QString curr;
    bool ok = false, found = false;
    int index = 0, closest = 0, currVal = 0, min = 0, max = 0;
    int intValue = loc.toInt(target, &ok);
    MY_ASSERT(ok == true)

    int closestVal = 0;
    foreach (curr, strList)
    {
        QStringList sl = curr.split(':');
        if(sl.count()> 1)
        {
            min = loc.toInt(sl.at(0), &ok);
            MY_ASSERT(ok == true)
            max = loc.toInt(sl.at(0), &ok);
            MY_ASSERT(ok == true)
            //int mid = min + ((max-min)/2);
            closest =  index;
            closestVal = loc.toInt(curr, &ok);
            found = true;
        }
        else
        {
            currVal = loc.toInt(curr, &ok);
            MY_ASSERT(ok == true)
            if (currVal >= intValue &&
                abs(intValue-currVal) < abs(closestVal - currVal))
            {
                found = true;
                closest = index;
                closestVal = currVal;
            }
        }
        index++;
    }
    return (found==true)? closest : index-1;
}

int Settings::getClosestFloatValueFromList(QStringList& strList, QString target)
{
    //works only with lists of discrete integer values (not ranges, not floats)
    QString curr;
    bool ok = false, found = false;
    int index = 0, closest = 0;
    float currVal = 0;
    float floatValue = loc.toFloat(target, &ok);
    MY_ASSERT(ok == true)

    float closestVal = 0;
    foreach (curr, strList)
    {
        currVal = loc.toFloat(curr, &ok);
        MY_ASSERT(ok == true)
       if (currVal >= floatValue &&
            fabs(floatValue-currVal) < fabs(closestVal - currVal))
        {
            found = true;
            closest = index;
            closestVal = currVal;
        }
        index++;
    }
    return (found==true)? closest : index-1;
}

 int Settings::roundUp(int numToRound, int multiple)
 {
     if (multiple == 0)
        return numToRound;

    int remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
 }

int Settings::isIntValueInList(QStringList& strList, QString target )
{
    bool ok = false;
    int rangeNum = 0;
    QString range;
    foreach (range, strList)
    {
        QStringList rangeItem = range.split(':');
        if(rangeItem.count() < 2)
        {
            //rangesList is not a list of ranges but
            //a list of values
            if( rangeItem.at(0) == target )
            {
                return rangeNum;
            }
        }
        else
        {
            int from = loc.toInt(rangeItem.at(0), &ok);
            if(ok == false)
            {
                MYINFO << "to=" << rangeItem.at(0);
            }
            MY_ASSERT(ok == true)
            int to = loc.toInt(rangeItem.at(1), &ok);
            if(ok == false)
            {
                MYINFO << "to=" << rangeItem.at(1);
            }
            MY_ASSERT(ok == true)
            int val = loc.toInt(target, &ok);
            if(ok == false)
            {
                MYINFO << "value=" << target;
            }
            MY_ASSERT(ok == true)
            if(val >= from && val <= to)
            {
                return rangeNum;
            }
        }
        rangeNum++;
    }
    return -1;
}


int Settings::isFloatValueInList(QStringList& strList, QString target )
{
    bool ok = false;
    int rangeNum = 0;
    QString range;
    foreach (range, strList)
    {
        QStringList rangeItem = range.split(':');
        if(rangeItem.count() < 2)
        {
            //rangesList is not a list of ranges but
            //a list of values
            if( rangeItem.at(0) == target )
            {
                return rangeNum;
            }
        }
        else
        {
            //Warning: this function does not fall back to the 'C' locale if the string cannot be interpreted in this locale.
            float from = loc.toFloat(rangeItem.at(0), &ok);
            MY_ASSERT(ok == true)
            float to = loc.toFloat(rangeItem.at(1), &ok);
            MY_ASSERT(ok == true)
            float val = loc.toFloat(target, &ok);
            MY_ASSERT(ok == true)
            if(val >= from && val <= to)
            {
                return rangeNum;
            }
        }
        rangeNum++;
    }
    return -1;
}



// returns the integer divider of numToDiv not less than divCeil
int Settings::getIntDivider( int numToDiv, int divCeil )
{
    int floor=0, ceil=0;
    for (int n = 2; n < numToDiv; n++)
    {
        int mod = numToDiv % n;
        if(mod == 0)
        {
            if(n >= divCeil)
            {
                ceil=n;
                break;
            }
            else
            {
                floor = n;
            }
        }
    }

    if(floor == 0 && ceil == 0)
    {
        //prime numbers
        return numToDiv;
    }

    if((divCeil-floor) <= (ceil-divCeil))
    {
        return floor;
    }
    return ceil;

}



int Settings::updateRTSformatRev()
{
    int currentFormat = rtsFormatRev;
    if(rtsFormatRev != CURRENT_RTS_FORMAT_REV)
    {
        MYWARNING << "The format revision of " << configName << ".rts is " << rtsFormatRev;
        MYWARNING << "while this program supports revision " << CURRENT_RTS_FORMAT_REV;
        MYWARNING << "some settings could be initialized with default values";
        MYWARNING << "or misinterpreted. Please check if everything is still OK";
        MYWARNING << "then save in order to update the rts format.";
        changed++;
        rtsFormatRev = CURRENT_RTS_FORMAT_REV;

    }

    return currentFormat;
}

void Settings::roundEccentricity()
{
    //rounds value by single resolution step
    int sStep = iround(getResolution());
    if(sStep <= 0)
    {
        sStep = 1;
    }

    int mod = wfEccentricity % sStep;
    if (mod != 0)
    {
        //rounding required
        if(wfEccentricity > wfEccentricityBak)
        {
            //when increasing eccentricity
            MYINFO << ">round up";
            wfEccentricity = (wfEccentricity - mod + sStep);
        }
        else if(wfEccentricity < wfEccentricityBak)
        {
            //when decreasing eccentricity
            MYINFO << ">round down";
            wfEccentricity = (wfEccentricity - mod - sStep);
        }
        else
        {
            //static rounding
            if( mod > (sStep / 2) )
            {
                MYINFO << ">round up";
                wfEccentricity = (wfEccentricity - mod + sStep);
            }
            else
            {
                MYINFO << ">round to " << mod;
                wfEccentricity = (wfEccentricity - mod);
            }
        }

    }
}
