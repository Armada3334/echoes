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
$Rev:: 149                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-03-14 00:08:31 +01#$: Date of last commit
$Id: funcgen.h 149 2018-03-13 23:08:31Z gmbertani $
*******************************************************************************/

#ifndef FUNCGEN_H
#define FUNCGEN_H

#include <QtCore>
#include "iqdevice.h"
///
/// \brief The TEST_MODE enum
/// function generator mode
enum TEST_MODE
{
    TM_NONE,
    TM_DC,
    TM_SINE,
    TM_SWEEP,
    TM_METEOR
};


///
/// \brief The FG_STATUS enum
///Function generator status
enum FG_STATUS
{
    FG_NONE,
    FG_SET,
    FG_COUNTING,
    FG_BURNING,
    FG_ABLATION,
    FG_IONIZATION
};


/**
 * @brief The FuncGen class
 * Generates a fixed frequency sinewave or simulates meteor events
 */
class FuncGen : public IQdevice
{
    Q_OBJECT

protected:
    bool buildingNewWave;
    bool waveReady;
    bool bufReady;
    bool bufInit;

    IQbuf* stored;                      /// static iq buffer array

    float amplitude;
    float actualFreq;
    int iqBufSize;
    float acqTime;

    int16_t clearQ;
    int16_t clearI;

    //TM_SINE:
    int sweepFreq;                       /// sweep frequency in TM_SINE
    int loopsToNextSweep;                /// TM_SINE: acquisition loops before increasing sweepFreq

    TEST_MODE testMode;                  /// test mode active (when no devices plugged in)

    //meteor mode:
    FG_STATUS meteor;                    ///meteor mode status
    int countDown;                       ///scans before next event (0=trigger)
    int burning;                         ///vertical stripe lasting [scans]
    int ablation;                        ///horizontal strip width [hz]
    int ionization;                      ///ionization phase lasting [scans]



    ///
    /// \brief goSine
    ///create a sinewave which fits the buf8[] buffer integer periods
    ///at given frequency and amplitude
    void goSine();

    ///
    /// \brief goDC
    /// if given frequency is zero
    void goDC();


    ///
    /// \brief goSweep
    /// simulate meteor (set freq to zero
    void goSweep();

    ///
    /// \brief makeNoise
    ///
    void makeNoise();

    ///
    /// \brief getWave
    /// \param freq
    /// \param amp
    /// \param buf
    /// read continuous sinewave
    void getWave(float freq, float amp, IQbuf *buf);

    ///
    /// \brief getEvent
    /// \param freq
    /// \param amp
    /// \param buf
    ///read simulated meteor event
    void getEvent(float freq, float amp, IQbuf *buf);

    ///
    /// \brief retune
    ///
    void retune();


public:    
    ///
    /// \brief FuncGen
    /// \param appSettings
    /// \param bPool
    /// \param parentThread
    ///
    explicit FuncGen(Settings* appSettings, Pool<IQbuf*> *bPool = nullptr, QThread *parentThread = nullptr);

    ~FuncGen();


    ///
    /// \brief run
    ///
    virtual void run();

    ///
    /// \brief className
    /// \return
    ///
    virtual const char* className() const
    {
        return static_cast<const char*>( "FuncGen" );
    }

    ///
    /// \brief deviceOpen
    /// \return
    ///
    virtual bool isDeviceOpen();

    ///
    /// \brief getStats
    /// \param stats
    /// \return
    ///
    virtual bool getStats( StatInfos* stats );

public slots:

    ///
    /// \brief paramsChange
    ///
    virtual void slotParamsChange();

    ///
    /// \brief slotSetTune
    /// \return
    ///
    virtual bool slotSetTune();
    ///
    /// \brief slotSetSampleRate
    /// \return
    ///
    virtual bool slotSetSampleRate();

    ///
    /// \brief slotSetGain
    /// \return
    ///
    virtual bool slotSetGain();

    ///
    /// \brief slotSetDsBandwidth
    /// \return
    ///
    virtual bool slotSetDsBandwidth();


    ///
    /// \brief slotSetAntenna
    /// \return
    ///
    virtual bool slotSetAntenna();

    ///
    /// \brief slotSetBandwidth
    /// \return
    ///
    virtual bool slotSetBandwidth();

    ///
    /// \brief slotSetFreqRange
    /// \return
    ///
    virtual bool slotSetFreqRange();

};

#endif // FUNCGEN_H
