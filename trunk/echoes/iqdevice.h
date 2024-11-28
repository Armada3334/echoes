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
    along with this program.  If not, see http://www.gnu.org/copyleft/gpl.html




*******************************************************************************
$Rev:: 373                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-27 00:22:13 +01#$: Date of last commit
$Id: iqdevice.h 373 2020-08-25 23:22:13Z gmbertani $
*******************************************************************************/



#ifndef IQDEVICE_H
#define IQDEVICE_H


#include <QtCore>
#include <QtNetwork>
#include "setup.h"

#include "settings.h"
#include "iqbuf.h"
#include "scan.h"
#include "pool.h"

///
/// \brief The DeviceMap struct
///
struct DeviceMap
{
    int index;      ///driver index - it's the index returned by librtlsdr and required by rtlsdr_open()
    QString name;   ///device string displayed on main window
};

///
/// \brief The IQdevice class
///
/// it's the base class for a device able to produce
/// a stream of I/Q samples.
/// Each device is a singleton, since an Echoes instance can represent data
/// coming from a single device only.
/// To display data coming from more devices,
/// multiple Echoes running instances are needed.

class IQdevice : public QThread
{
    Q_OBJECT


protected:

    QThread* owner;
    QRecursiveMutex mt;
    Settings* as;

    Pool<IQbuf*>* pb;                    ///IQ buffers pool (to send data to FFT)

    QString deviceName;                           /// IQ device name

    //human-readable device capabilities lists

    QStringList srRanges;                         /// available sample rate ranges
    QStringList bbRanges;                         /// available bandwidth for opened device [Hz]
    QStringList frRanges;                         /// available tuning ranges for opened device [Hz]
    QStringList antennas;                         /// available antennas for opened device [Hz]

    QString antenna;                              /// selected antenna
    QString srRange;                              /// selected sample rate range [Hz]
    QString bbRange;                              /// selected bandwidth range    [Hz]
    QString frRange;                              /// selected tuning range      [kHz]

    QDateTime tsGen;                              /// timestamps generator

    int streamChannel;                            /// Soapy RX channel number (zero)

    int64_t tune;                                 /// tuned frequency [Hz]
    int64_t minTune;                              /// minumum tunable frequency in current range [Hz]
    int64_t maxTune;                              /// maximum tunable frequency in current range [Hz]
    int lowBound;                                 /// lowest frequency in downsampled band
    int highBound;                                /// highest frequency in downsampled band

    int sr;                                       /// sample rate [Hz]
    int bw;                                       /// hw filtered bandwidth [Hz] (called "bandwidth" in SoapySDR)
    int dsBw;                                     /// bandwidth - downsampler output rate [Hz]
                                                  /// if resamplerBypass is true, it matches the bandwidth

    int gain;                                     ///RF gain in % of the gain range or -1 for automatic gain
    float ppmError;                               ///tuner error compensation [ppm]

    bool hasFreqErrorCorr;                        /// the device allows manual correction for tune frequency error
    bool myCfgChanged;                            /// the device changed the settings by its own
    bool resamplerBypass;                         /// true = skip resampler (like older Echoes versions did)
    bool ready;                                   /// nesting flag to avoid multiple calls of slotParamsChange()
    bool stopThread;                              /// request to stop gracefully this thread  


public:
    ///
    /// \brief IQdevice
    /// \param appSettings
    /// \param bPool
    /// \param parentThread
    ///
    IQdevice(Settings* appSettings, Pool<IQbuf*> *bPool, QThread *parentThread = nullptr);

    virtual ~IQdevice();

    ///
    /// \brief run
    ///
    virtual void run() = 0;

    ///
    /// \brief className
    /// \return
    ///
    virtual const char* className() const = 0;


    ///
    /// \brief deviceOpen
    /// \return
    ///
    virtual bool isDeviceOpen() = 0;

    ///
    /// \brief getStats
    /// \param stats
    /// \return
    ///
    virtual bool getStats( StatInfos* stats ) = 0;

    ///
    /// \brief isReady
    /// \return
    ///
    bool isReady() const
    {
        return ready;
    }

    ///
    /// \brief isConfigChanged
    /// \return
    /// note: destructive reading
    bool isGUIupdateNeeded()
    {
        bool changed = myCfgChanged;
        myCfgChanged = false;
        return changed;
    }

    ///
    /// \brief stop
    /// 
    void stop()
    {
        MYINFO << "stop() called";
        if(isRunning() && !stopThread)
        {
            stopThread = true;
            MYINFO << "stopThread = true (no waits)";
            //wait();
        }
    }

    ///
    /// \brief getSRranges
    /// \return
    ///
    QStringList getSRranges() const
    {
        return srRanges;
    }

    ///
    /// \brief getBBranges
    /// \return
    ///
    QStringList getBBranges() const
    {
        return bbRanges;
    }

    ///
    /// \brief getFreqRanges
    /// \return
    ///
    QStringList getFreqRanges() const
    {
        return frRanges;
    }

    ///
    /// \brief getAntennas
    /// \return
    ///
    QStringList getAntennas() const
    {
        return antennas;
    }

    ///
    /// \brief allowsDCspikeRemoval
    ///
    bool allowsDCspikeRemoval() const
    {
        return false; //placeholder
    }

    ///
    /// \brief allowsFrequencyCorrection
    ///
    bool allowsFrequencyCorrection() const
    {
        return hasFreqErrorCorr;
    }

    ///
    /// \brief getHighBound
    /// \return
    ///
    int getHighBound() const
    {
        return highBound;
    }

    ///
    /// \brief getLowBound
    /// \return
    ///
    int getLowBound() const
    {
        return lowBound;
    }

public slots:

    ///
    /// \brief paramsChange
    ///
    virtual void slotParamsChange() {}

    ///
    /// \brief slotSetTune
    /// \return
    ///
    ///
    virtual bool slotSetTune()
    {
        return true;
    }

    ///
    /// \brief slotSetSampleRate
    /// \return
    ///
    virtual bool slotSetSampleRate()
    {
        return true;
    }

    ///
    /// \brief slotSetGain
    /// \return
    ///
    virtual bool slotSetGain()
    {
        return true;
    }

    ///
    /// \brief slotSetError
    /// \return
    ///
    virtual bool slotSetError()
    {
        return true;
    }

    ///
    /// \brief slotSetDsBandwidth
    /// \return
    ///
    virtual bool slotSetDsBandwidth()
    {
        return true;
    }


    ///
    /// \brief slotSetResolution
    /// \return
    ///
    virtual bool slotSetResolution()
    {
        return true;
    }


    ///
    /// \brief slotSetAntenna
    /// \return
    ///
    virtual bool slotSetAntenna()
    {
        return true;
    }

    ///
    /// \brief slotSetBandwidth
    /// \return
    ///
    virtual bool slotSetBandwidth()
    {
        return true;
    }

    ///
    /// \brief slotSetFreqRange
    /// \return
    ///
    virtual bool slotSetFreqRange()
    {
        return true;
    }



signals:
    ///
    /// \brief status
    /// \param msg
    /// \param ast
    ///
    void status(const QString& msg, int ast = AST_NONE);  //updates status bar in MainWindow

    ///
    /// \brief rtlsdr_fault
    /// \param val
    ///
    void fault(int val);


};

#endif // IQDEVICE_H
