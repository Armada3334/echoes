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
$Id: radio.h 373 2018-11-26 23:22:13Z gmbertani $
*******************************************************************************/



#ifndef RADIO_H
#define RADIO_H

#include <cmath>
#include <limits>
#include <float.h>
#include <QtCore>
#include "setup.h"
#include "settings.h"
#include "iqbuf.h"
#include "scan.h"
#include "pool.h"
#include "iqdevice.h"
//#include "c:\Program Files\PothosSDR\include\liquid\liquid.h"


class FuncGen;
class DongleClient;
class SyncRx;



///
/// \brief The Radio class
///
class Radio : public QThread
{
    Q_OBJECT

protected:

    Settings* as;

    QRecursiveMutex mx;

    Pool<IQbuf*>* pb;                     ///<IQ buffers pool (for FFT input)
    Pool<Scan*>* ps;                               ///<spectral scans pool (for FFT output)

    QElapsedTimer statClock;                        ///<inner statistics timer

    int             devIndex;                      ///<opened device index
    IQdevice*       devObj;                        ///<opened device object

    XQDir    wd;

    QList<DeviceMap> devices;                    ///< available devices found

    bool ready;                                   ///< true = ready to start sampling
    bool isCfgChanged;
    bool stopRx;                                  ///< set to true to stop radio thread
    bool skipResample;                            ///< no resampling required (SR==dsBw)
    int resamplerBypass;                          ///< resampler bypass active is true

    IQbuf *iqBuf;                          ///< iq buffer to be procesed by FFT input

    QString timeStamp;                            ///< current buffer timestamp

    int inputSamples;                             ///< resampler input samples (depending of SR and resolution)
    int outputSamples;                            ///< resampler output samples (depending of BW)
    int FFTbins;                                  ///< FFT input samples == FFT output bins (N) (depending of resolution)
    int iqBufSize;                                ///< number of samples contained in a IQ buffer (doublets) (depending of inputSamples)

    float binSize;                                 ///< size of each FFT bin [Hz] (resolution)
    int   dsBw;                                    ///< downsampled bandwidth [Hz]

    float norm;                                   ///< scaling factor for fft output values
    float *windowCoefs;                           ///< FFT input window coefs
    float avgPower;                               ///< power of the weakest bin in the scan (but still higher than zero)

    FFT_WINDOWS  wFunc;                           ///< selected window function index

    liquid_float_complex* fftIn;                    ///< iq (complex) input buffer for fft
    liquid_float_complex* fftOut;                   ///< complex data output buffer for fft

    fftplan plan;                                   ///< liquid fftw plan

    liquid_float_complex* rrIn;                   ///< iq (complex) input buffer for resampler
    liquid_float_complex* rrOut;                  ///< iq (complex) output buffer for resampler
    rresamp_crcf  rr;                             ///< resampler SR-->BW
    uint decim;                                   ///< rr internal decimation factor
    int scanIndex;                                ///< index of *bins in the pool
    int acqTime;                                  ///< minimum time needed to fill-in the IQ buffer [mS]

    uint nans;                                    ///< progressive count of NANs
    uint elapsedLoops;                            ///< progressive counter of radio thread loops

    int waitScanMs;                               ///< ms lost while waiting for a free scan bufs
    int waitIQms;                                 ///< ms lost while waiting for IQ samples
    int resampMs;                                 ///< ms taken by resampling IQ data
    int FFTms;                                    ///< ms spent in FFT
    int droppedScans;                             ///< progressive number of lost scans due to buffer unavailability
     //FFT window functions

    ///
    /// \brief rectangle
    /// \param i
    /// \param length
    /// \return
    ///
    float rectangle(int i, int length);

    ///
    /// \brief blackman
    /// \param i
    /// \param length
    /// \return
    ///
    float blackman(int i, int length);

    ///
    /// \brief hannPoisson
    /// \param i
    /// \param length
    /// \return
    ///
    float hannPoisson(int i, int length);

    ///
    /// \brief youssef
    /// \param i
    /// \param length
    /// \return
    ///
    float youssef(int i, int length);

    ///
    /// \brief bartlett
    /// \param i
    /// \param length
    /// \return
    ///
    float bartlett(int i, int length);


    ///
    /// \brief deviceSearch
    /// \return
    ///
    int deviceSearch();


    ///
    /// \brief toLinearPower
    /// \param cmp
    /// \return
    ///
    float toLinearPower(liquid_float_complex cmp);

    ///
    /// \brief showStatus
    /// \param ast
    /// \param s
    ///
    void showStatus(int ast, QString s);

    ///
    /// \brief showStatus
    /// \param s
    ///
    void showStatus(QString s);

    ///
    /// \brief reset
    /// is a sort of destructor for resources
    /// created into the run() thread
    ///
    void reset();

    ///
    /// \brief performFFT
    /// \return
    ///
    int performFFT();


    ///
    /// \brief run
    ///
    void run();


public:
    ///
    /// \brief Radio
    /// \param appSettings
    /// \param sPool
    /// \param workDir
    /// \param parent
    ///
    explicit Radio(Settings* appSettings, Pool<Scan*>* sPool, XQDir workDir);
    ~Radio();


    ///
    /// \brief isReady
    /// \return
    ///
    bool isReady() const
    {
        return ready;
    }

    ///
    /// \brief isDeviceOpen
    /// \return
    ///
    bool isDeviceOpen()
    {
        if(devObj != nullptr)
        {
            return devObj->isDeviceOpen();
        }
        return false;
    }

    ///
    /// \brief deviceOpened
    /// \return
    ///
    IQdevice* deviceOpened()
    {
        return devObj;
    }

    ///
    /// \brief testRetune
    /// \return
    ///
    bool testRetune();

    ///
    /// \brief getDevices
    /// \return
    ///
    QList<DeviceMap> getDevices(bool rescan = false)
    {
        if(rescan)
        {
            deviceSearch();
        }
        return devices;
    }

    ///
    /// \brief getDeviceName
    /// \return
    ///
    QString getDeviceName();

    ///
    /// \brief stopMe
    ///
    void stop();

    ///
    /// \brief getStats
    /// \param stats
    /// \return
    ///
    bool getStats( StatInfos* stats );

    ///
    /// \brief getHighBound
    /// \return
    ///
    int getHighBound();


    ///
    /// \brief getLowBound
    /// \return
    ///
    int getLowBound();

    ///
    /// \brief allowsFrequencyCorrection
    /// \return
    ///
    bool allowsFrequencyCorrection();

    ///
    /// \brief getAcquisitionTimeMs
    /// \return
    ///
    int getAcquisitionTimeMs() const
    {
        return acqTime;
    }

    ///
    /// \brief getAvgPower
    /// \return
    ///
    float getAvgPower() const
    {
        return avgPower;
    }


public slots:

    ///
    /// \brief slotSetDevice
    /// \return
    ///
    APP_STATE slotSetDevice(int deviceIndex = -1);

    ///
    /// \brief slotSetWindow
    /// \return
    ///
    bool slotSetWindow();

    ///
    /// \brief slotSetResolution
    /// \return
    ///
    bool slotSetResolution();


    ///
    /// \brief slotSetTune
    /// \return
    ///
    bool slotSetTune();
    ///
    /// \brief slotSetSampleRate
    /// \return
    ///
    bool slotSetSampleRate();

    ///
    /// \brief slotSetGain
    /// \return
    ///
    bool slotSetGain();

    ///
    /// \brief slotSetError
    /// \return
    ///
    bool slotSetError();

    ///
    /// \brief slotSetDsBandwidth
    /// \return
    ///
    bool slotSetDsBandwidth();

    ///
    /// \brief slotSetAntenna
    /// \return
    ///
    bool slotSetAntenna();

    ///
    /// \brief slotSetBandwidth
    /// \return
    ///
    bool slotSetBandwidth();

    ///
    /// \brief slotSetFreqRange
    /// \return
    ///
    bool slotSetFreqRange();


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

    ///
    /// \brief GUIupdateNeeded
    ///
    void GUIupdateNeeded();

};

#endif // RADIO_H
