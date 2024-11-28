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
$Rev:: 351                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-10-22 00:13:47 +02#$: Date of last commit
$Id: funcgen.cpp 351 2018-10-21 22:13:47Z gmbertani $
*******************************************************************************/

#include <time.h>
#include "setup.h"
#include "funcgen.h"

FuncGen::FuncGen(Settings* appSettings, Pool<IQbuf*> *bPool, QThread *parentThread) :
    IQdevice(appSettings, bPool, parentThread)
{
    setObjectName("FuncGen thread");

    MYINFO << "parentThread=" << parentThread << " parent()=" << parent();

    ready = false;

    iqBufSize = 0;
    stored = nullptr;
    acqTime = 0;
    amplitude = 0.0;
    actualFreq = 0.0;

    sweepFreq = 0;
    loopsToNextSweep = 0;
    highBound = 0;
    lowBound = 0;
    dsBw = 0;

    testMode = TM_NONE;
    meteor = FG_NONE;
    countDown = 0;
    burning = 0;
    ablation = 0;
    ionization = 0;

    buildingNewWave = false;

    clearQ = 1200;
    clearI = 1200;


    stored = nullptr;

    deviceName = "Test patterns";

    srRanges.clear();
    srRanges.append( "1000 : 100000" );

    bbRanges.clear();
    bbRanges.append( "1000 : 100000" );

    frRanges.clear();
    frRanges.append( "1000 : 50000" );

    antennas.clear();
    antennas.append("None");
    bufInit = true;

}

FuncGen::~FuncGen()
{
    //waits for the end of owned thread ( run() termination )
    if( wait(2000) == false )
        MYWARNING << className() << " destructor - can't stop the thread";
}


void FuncGen::run()
{
    bool init = true;
    stopThread = false;

    MYINFO << "Function generator thread started";
    int bufIdx = -1;

    IQbuf* iqBuf = nullptr;
    if( pb->size() == 0 )
    {
        for (int i = 0; i < DEFAULT_POOLS_SIZE; i++)
        {
            iqBuf = new IQbuf( MAX_SAMPLE_BUF_SIZE );
            MY_ASSERT( nullptr !=  iqBuf)
            pb->insert(i, iqBuf);
        }
    }

    MYINFO << "I/Q buffer size set to " << MAX_SAMPLE_BUF_SIZE << " I/Q doublets ("
           << MAX_SAMPLE_BUF_SIZE * 2 << " bytes)";


    iqBufSize = DEFAULT_BW;

    //static IQ buffer for periodic waveforms
    stored = new IQbuf( iqBufSize );
    MY_ASSERT( nullptr !=  stored )

    //enough small to be fast and enough big to allow
    //down resampling:
    iqBufSize = static_cast<int>(as->getSampleRate() / as->getResolution());

    //acquisition time in ms:
    acqTime = (1000 * iqBufSize) / as->getSampleRate();

    countDown = 0;
    burning = 0;
    ablation = 0;
    ionization = 0;
    amplitude = 0.0;
    actualFreq = 0.0;
    meteor = FG_NONE;
    lowBound   =  as->getTune() - (dsBw / 2);
    highBound  =  as->getTune() + (dsBw / 2);

    if(as->getShots() > 0 && as->getUpThreshold() > 0)
    {
        MYINFO << "shots required and up threshold set, entering meteor simulation to test the automatic capture";
        testMode = TM_METEOR;
    }
    else if(as->getShots() > 0)
    {
        MYINFO << "Shots required without thresholds, entering sweeping sinewave generation to test periodic mode";
        testMode = TM_SWEEP;
    }
    else
    {
        MYINFO << "no shots required, entering sinewave generation to test continuous mode";
        testMode = TM_SINE;
    }


    forever
    {
        if( isFinished() == true || isInterruptionRequested() == true || stopThread == true )
        {
            MYDEBUG << "FuncGen: exiting and terminating thread.";
            break;
        }

        IQbuf* buf = nullptr;

        bufIdx = pb->take();
        if(bufIdx != -1)
        {

            buf = pb->getElem(bufIdx);
            if(buf != nullptr)
            {

                iqBufSize = static_cast<int>(as->getSampleRate() / as->getResolution());

                if( buf->bufSize() != iqBufSize || init == true )
                {
                    buf->setBufSize( iqBufSize );
                    stored->setBufSize( iqBufSize );
                    acqTime = (1000 * iqBufSize) / as->getSampleRate();
                    init = false;
                }

                tsGen = as->currentUTCdateTime();
                QString s = tsGen.toString( "dd/MM/yyyy;hh:mm:ss.zzz");
                buf->setTimeStamp( s );

                switch(testMode)
                {
                case TM_NONE:
                case TM_DC:
                    break;

                case TM_SINE:
                    getWave(
                        static_cast<float>(as->getTune()),
                        static_cast<float>(as->getGain()),
                        buf );
                    break;

                case TM_SWEEP:
                    if(loopsToNextSweep == 0)
                    {
                        loopsToNextSweep = 50;
                        if(sweepFreq >= as->getTune())
                        {
                            sweepFreq = 0;
                        }
                        sweepFreq += static_cast<int>(as->getResolution() * 10);

                    }
                    else
                    {
                        loopsToNextSweep--;
                    }

                    getWave(
                        static_cast<float>(sweepFreq),
                        static_cast<float>(as->getGain()),
                        buf );

                    break;

                case TM_METEOR:

                    getEvent(
                        static_cast<float>(as->getTune()),
                        static_cast<float>(as->getGain()),
                        buf );

                    switch(meteor)
                    {
                    case FG_NONE:
                        //no events - the test produces a fixed pattern
                        //controlled by the caller
                        //goSine();
                        break;

                    case FG_SET:
                        //previous event concluded
                        //triggers a new one
                        //reloads casual countdown
                        srand( static_cast<uint>( time(nullptr) ) );
                        if(as->getRecTime() > 0)
                        {
                            countDown = ((rand() % 3) + 1) * //countdown is in 50ms units
                                    (as->getRecTime() * 20); //20 times the rec time
                        }
                        else
                        {
                            countDown = ((rand() % 3) + 1) * 40;
                        }
                        meteor = FG_COUNTING;
                        break;

                    case FG_COUNTING:
                        //waits countdown. Counters are decremented in getEvent() to get
                        //synced with radio thread
                        if(countDown <= 0)
                        {
                            //trigger
                            //calculate burning scans
                            burning = (rand() % 10) + 2;
                            actualFreq = 1 + as->getSampleRate() / ((rand() % 100) + 1);

                            //produces the short vertical strip with a fixed sinewave
                            goSine();
                            meteor = FG_BURNING;
                        }
                        else
                        {
                            makeNoise();
                        }
                        break;

                    case FG_BURNING:
                        //burning phase
                        if(burning <= 0)
                        {
                            //prepares ablation sweep width
                            //looks better with lowest SR
                            ablation = static_cast<int>(
                                        actualFreq + (rand() % static_cast<int>(actualFreq / 100))
                                        +1 );

                            meteor = FG_ABLATION;
                        }
                        break;

                    case FG_ABLATION:
                        //ablation is very fast - it lasts one scan only and at full scale amplitude
                        {
                            amplitude = 100;
                            goSweep();
                            amplitude = (rand() % 30);
                            ionization = (rand() % 10) + 2;
                            meteor = FG_IONIZATION;
                        }
                        break;

                    case FG_IONIZATION:
                        //ionization phase
                        if(ionization > 0)
                        {
                            //approximate decreasing repetition of ablation scan
                            //actualFreq += (rand() % 10000);
                            //ablation -= (rand() % 5000);
                            amplitude = (amplitude > 0)? amplitude + (rand() % 2) : 0;
                            goSweep();

                        }
                        else
                        {
                            //event's end
                            //clears output wave
                            makeNoise();
                            meteor = FG_SET;

                        }
                        break;
                    }

                }

                memcpy( buf->data16ptr(), stored->data16ptr(), static_cast<size_t>(buf->bufByteSize()) );
                pb->forward(bufIdx);

            }

        }

        QThread::yieldCurrentThread();

    }

    //releasing pending buffers
    if(bufIdx != -1)
    {
        pb->discard(bufIdx);
        bufIdx = -1;
        pb->flush();
    }
    pb->clear();

    if(stored != nullptr)
    {
        delete stored;
        stored = nullptr;
    }

}




bool FuncGen::isDeviceOpen()
{
    return true;
}

void FuncGen::getWave(float freq, float amp, IQbuf* buf)
{
    QMutexLocker ml(&mt);

    //comparing floats in this way is not safe but
    //they are in fact integers boxed in floats
    if( as->fpCmpf(freq, actualFreq) != 0 || as->fpCmpf(amp, amplitude) != 0 )
    {
        //changed parameters: zeroes the buffer

        actualFreq = freq;
        amplitude = amp;
        //makeNoise();
        buildingNewWave = true;
    }

    goSine();

    //outputs the stored wave
    memcpy(buf->data16ptr(), stored->data16ptr(), static_cast<size_t>( buf->bufByteSize() ) );
}


void FuncGen::getEvent(float freq, float amp, IQbuf *buf)
{
    QMutexLocker ml(&mt);

    if(meteor == FG_NONE)
    {
        makeNoise();
        memcpy(buf->data16ptr(), stored->data16ptr(), static_cast<size_t>( buf->bufByteSize() ) );
        meteor = FG_SET;
    }

    if(meteor == FG_SET)
    {     
        //changed parameters:
        actualFreq = freq;
        amplitude = amp;

        //aborts pending events and zeroes the buffer
        makeNoise();
    }

    //ms counting
    msleep(50);
    if(countDown > 0)
    {
        countDown--;
    }
    else if(burning > 0)
    {
        burning--;
    }
    else if(ionization > 0)
    {
        ionization--;
    }

    memcpy(buf->data16ptr(), stored->data16ptr(), static_cast<size_t>( buf->bufByteSize() ));

}



void FuncGen::goSweep()
{
    QElapsedTimer clock;

    clock.start();

    //fills the input buffer with a sine wave sweeping from actualFreq
    //to actualFreq+ablation
    //Please note that one sample is TWO BASETYPEs (I+Q)
    //only I (real) samples are generated
    //the Q (imaginary) samples are all zeroed

    const float twopi = 6.28318530F;
    float t = 0, z = 0, x = 0, y = 0, oldY = 0, dt = 0, df = 0, currFreq = 0;

    dt = 1.0F / as->getSampleRate();
    df = (2 * ablation) / static_cast<float>( iqBufSize );
    currFreq = actualFreq;

    MYDEBUG << "Calculating sine wave table, sweeping from " << actualFreq << " to " << ablation << " Hz";
    MYDEBUG << "Amplitude=" << amplitude;
    MYDEBUG << "dt = " << dt;

    int periods = 0;
    int phase = 0;
    x = 0;

    //QString wave = "sweeptab.dat";
    //QFile f(wd.absoluteFilePath( wave ));
    //f.open(QIODevice::WriteOnly);
    //QTextStream s(&f);

    for (int n = 0; n < stored->bufSize(); n++)
    {
        t = x * dt;                 // 1/sample_rate = time between samples
        z = twopi*currFreq*t + phase;
        oldY = y;
        y = amplitude * sinf( z );

        stored->setReal(n, static_cast<int16_t>(y));
        stored->setImag(n, static_cast<int16_t>(0));

        if( oldY <= 0 && y > 0 ) //zero-cross
        {
            periods++;

        }
        currFreq += df;
        x++;
    }

    MYDEBUG << "Sweeping wave generation took " << clock.elapsed() << " ms";
}

void FuncGen::goSine()
{
    QElapsedTimer clock;

    clock.start();

    //fills the input buffer with a sine wave that covers
    //with precise periods the entire buffer.
    //The number of periods to store in buffer can be specified,
    //while the resulting frequency is calculated accordingly.
    //Please note that one sample is TWO BASETYPEs (I+Q)
    //only I (real) samples are generated
    //the Q (imaginary) samples are all zeroed

    const float twopi = 6.28318530F;
    float t = 0, z = 0, x = 0, y = 0, oldY = 0, lasting = 0, ppp = 0, dt = 0, tp = 0;
    QTextStream s;
    QFile f;
    dt = 1.0F / as->getSampleRate();
    lasting = 1.0F / actualFreq;
    ppp = static_cast<float>(as->getSampleRate() / actualFreq);
    tp = acqTime / lasting;

    if(bufInit)
    {
        MYINFO << "Calculating sine wave table, actualFreq=" << actualFreq;
        MYINFO << "Amplitude=" << amplitude;
        MYINFO << "Points per period = " << ppp;
        MYINFO << "Period lasting [s] =" << lasting;
        MYINFO << "dt = " << dt;
        MYINFO << "Expected total periods = " << tp;

        QString wave = "sinetab.dat";
        QFileInfo fi(as->getConfigFullPath());
        QDir wd( fi.absolutePath() );
        f.setFileName( wd.absoluteFilePath( wave ) );
        f.open(QIODevice::WriteOnly|QIODevice::Truncate);
        s.setDevice(&f);
    }

    int periods = 0;
    float phase = 0.0;
    x = 0;

    for (int n = 0; n < stored->bufSize(); n++)
    {
        t = x * dt;                 // 1/sample_rate = time between samples
        z = (twopi * actualFreq * t) + phase;
        oldY = y;
        y = amplitude * sinf( z );

        //convert to ints
        y = (sizeof(int16_t) == 2)? y * 65536 : y * 256;

        stored->setReal(n, static_cast<int16_t>(y));
        stored->setImag(n, static_cast<int16_t>(0));

        if(bufInit)
        {
            s << "(" << y << ", 0)," << MY_ENDL;
        }

        if( oldY <= 0 && y > 0 ) //zero-cross
        {
            periods++;
        }

        x++;
    }

    if(bufInit)
    {
        f.close();
        MYINFO << "Sine wave generation took " << clock.elapsed() << " ms";
        MYINFO << "actualFreq=" << actualFreq << " Hz, " << periods << " full periods in buffer.";
        bufInit = false;
    }
}




void FuncGen::goDC()
{
    //fills the input buffer with a positive constant value.
    //Please note that one sample is TWO bytes (I+Q)
    //only I (real) samples are filled
    //the Q (imaginary) samples are all zeros

    MYDEBUG << "Inserting a constant DC offset in buffer, a=" << amplitude;

    int a = as->iround(amplitude);
    for (int n = 0; n < stored->bufSize(); n++)
    {
        stored->setReal(n, static_cast<int16_t>(a * 256));
        stored->setImag(n, static_cast<int16_t>(0));
    }

}

void FuncGen::makeNoise()
{
    //fills the input buffer with randomized low numbers
    //to simulate background noise, both I and Q
    MYDEBUG << "Background noise";

    for (int n = 0; n < stored->bufSize(); n++)
    {
        stored->setReal(n, static_cast<int16_t>(rand() % clearI));
        stored->setImag(n, static_cast<int16_t>(rand() % clearQ));
    }
}

void FuncGen::retune()
{
    //sweeping restarts from the lowest frequency
    //displayable (always 1hz)
    MYDEBUG << "retune()" ;
    sweepFreq = lowBound;
    loopsToNextSweep = 0;
}

bool FuncGen::getStats( StatInfos* stats )
{
    Q_UNUSED(stats)
    return false;
}

void FuncGen::slotParamsChange()
{
    if(ready == false)
    {
        dsBw = as->getDsBandwidth();

        lowBound   =  as->getTune() - (dsBw / 2);
        highBound  =  as->getTune() + (dsBw / 2);

        MYINFO << "Subsampled bandwidth" << lowBound << " to " << highBound << " Hz";

        retune();

        emit status( tr("Ready.") );
        ready = true;
    }
}

bool FuncGen::slotSetTune()
{
    if(isRunning() == true)
    {
        if(pb != nullptr)
        {
            //flushes the unprocessed iq buffers
            pb->flush();
        }
    }

    if(tune != as->getTune())
    {
        ready = false;
        if(as->getTune() <= 0 || as->getTune() > dsBw)
        {
           tune = dsBw/2;
           as->setTune(tune);
           MYWARNING << "Fixing device tune to " << tune;
        }
    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}

bool FuncGen::slotSetSampleRate()
{
    if( sr != as->getSampleRate() )
    {
        ready = false;
        QString newSr;
        newSr.setNum( as->getSampleRate() );
        if( as->isIntValueInList( srRanges, newSr ) != -1 )
        {
            sr = as->getSampleRate();
        }
        else
        {
            bool ok = false;
            newSr = srRanges.last().split(":").last();
            int intSr = newSr.toInt(&ok);
            MY_ASSERT(ok == true)
            sr = intSr;
            as->setSampleRate(sr);
        }
        slotSetBandwidth();
        slotSetDsBandwidth();
        slotSetFreqRange();
        tune = dsBw/2;
        as->setTune(tune);
    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}

bool FuncGen::slotSetGain()
{
    if(gain != as->getGain())
    {
        ready = false;
        gain = as->getGain();
        if( gain < 0 )         // >= 0 %
        {
            gain = 0;
        }

        if( gain > 100 )      // <= 100%
        {
            gain = 100;
        }
        as->setGain( gain );
    }

    if(ready == false)
    {
        slotParamsChange();
    }

    return true;
}

bool FuncGen::slotSetDsBandwidth()
{
    //function not implemented
    //ds bandwidth still same as sample rate

    if(dsBw != as->getDsBandwidth() ||
       dsBw != sr ||
       dsBw != bw)
    {
        ready = false;
        int newBw = as->getDsBandwidth();

        if(newBw != sr)
        {
            dsBw = sr;
        }
        else if(newBw != bw)
        {
            slotSetBandwidth();
        }
        as->setDsBandwidth(dsBw);
    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}



bool FuncGen::slotSetAntenna()
{
    antenna = "None";
    as->setAntenna(antenna);
    MYINFO << "Antenna set to " << antenna;
    return true;
}

bool FuncGen::slotSetBandwidth()
{
    if(bw != sr)
    {
        ready = false;
        bw = sr;
        as->setDsBandwidth(bw);
        MYINFO << "Base band set to " << bw;
        bbRanges.clear();
        QString bbs;
        bbs.setNum(bw);
        bbRanges.append( "1000 : " + bbs );
    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}

bool FuncGen::slotSetFreqRange()
{
    ready = false;
    frRange = QString("0:%1").arg(bw / 1000);
    as->setFreqRange(frRange);
    MYINFO << "Frequency range set to " << frRange << " kHz";
    QString bbs;
    bbs.setNum( (bw / 2000) );
    QString locRange = QString("1:%1").arg(bbs);
    frRanges.clear();
    frRanges.append(locRange);

    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}



