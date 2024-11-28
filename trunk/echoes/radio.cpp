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
$Rev:: 267                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-07-19 07:43:11 +02#$: Date of last commit
$Id: radio.cpp 267 2018-07-19 05:43:11Z gmbertani $
*******************************************************************************/


#include "funcgen.h"
#include "dclient.h"
#include "syncrx.h"
#include "radio.h"



Radio::Radio(Settings* appSettings, Pool<Scan*> *sPool, XQDir workDir)
{
    setObjectName("Radio thread");

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    MY_ASSERT( nullptr !=  sPool)
    ps = sPool;

    wd = workDir;

    //zeroes pointers
    fftIn = nullptr;
    fftOut = nullptr;
    rrIn = nullptr;
    rrOut = nullptr;
    windowCoefs = nullptr;
    iqBuf = nullptr;

    rr = nullptr;
    devObj = nullptr;

    iqBufSize = 0;
    outputSamples = 0;
    decim = 0;
    binSize = 0;
    scanIndex = -1;
    dsBw = 0;
    norm = 0;
    wFunc = FTW_RECTANGLE;
    acqTime = 0;
    nans = 0;
    waitScanMs = 0;
    waitIQms = 0;
    elapsedLoops = 0;
    droppedScans = 0;
    resampMs = 0;
    FFTms = 0;
    devices.clear();
    ready = false;
    isCfgChanged = false;
    stopRx = false;
    skipResample = true;
    resamplerBypass = true;

    FFTbins = 0;
    avgPower = MY_MAX_FLOAT;

    //frequency domain scans pool
    Scan* buf;
    for (int i = 0; i < SCAN_POOL_SIZE ; i++)
    {
        buf = new Scan(MAX_FFT_BINS);
        MY_ASSERT( nullptr !=  buf)
        ps->insert(i, buf);
    }

    //time domain samples pool
    pb = new Pool<IQbuf*>;
    MY_ASSERT( nullptr !=  pb)

    connect( as, SIGNAL( notifySetDevice() ),       this, SLOT( slotSetDevice() ));
    connect( as, SIGNAL( notifySetTune() ),         this, SLOT( slotSetTune() ));
    connect( as, SIGNAL( notifySetSampleRate() ),   this, SLOT( slotSetSampleRate() ));
    connect( as, SIGNAL( notifySetDsBandwidth() ),  this, SLOT( slotSetDsBandwidth() ));
    connect( as, SIGNAL( notifySsBypass() ),        this, SLOT( slotSetDsBandwidth() ));
    connect( as, SIGNAL( notifySetGain() ),         this, SLOT( slotSetGain() ));
    connect( as, SIGNAL( notifySetWindow() ),       this, SLOT( slotSetWindow() ) );
    connect( as, SIGNAL( notifySetError() ),        this, SLOT( slotSetError() ) );
    connect( as, SIGNAL( notifySetResolution() ),   this, SLOT( slotSetResolution() ));

    connect( as, SIGNAL( notifySetAntenna() ),      this, SLOT( slotSetAntenna() ));
    connect( as, SIGNAL( notifySetBandwidth() ),    this, SLOT( slotSetBandwidth() ));
    connect( as, SIGNAL( notifySetFreqRange() ),    this, SLOT( slotSetFreqRange() ));


    deviceSearch();
 }

Radio::~Radio()
{
    /*
     * Note: never call reset() here. The device's threads must
     * be destroyed by the radio thread itself with a  reset() call
     * while the destroyer can be called by any other thread
    */

    if(ps != nullptr)
    {
        ps->clear();
    }

    if(pb != nullptr)
    {
        delete pb;
        pb = nullptr;
    }

    if( isDeviceOpen() )
    {
        delete devObj;
        devObj = nullptr;
    }

}


void Radio::stop()
{
    MYINFO << "stopping Radio";
    stopRx = true;
    //wait();
}


void Radio::run()
{
    MYINFO << "Radio thread started" ;

    stopRx = false;
    //QThread* current = currentThread();
    if(isDeviceOpen())
    {
        //starting acquisition thread
        devObj->start();
    }

    int bufIdx = -1;

    iqBuf = nullptr;

    statClock.start();
    waitIQms = 0;
    acqTime = (1000 * iqBufSize) / as->getSampleRate();

    while( stopRx == false && devObj != nullptr )
    {

        bufIdx = pb->getData();

        if(bufIdx == -1)
        {
            if(devObj->isFinished())
            {
                //device thread stopped, stops this thread too
                stopRx = true;
            }
            else
            {
                //no fresh samples to process, retries after a short pause
                QThread::usleep(100);
                MYDEBUG << "no IQ buffer to read";
            }
            continue;
        }



        if(devObj->isGUIupdateNeeded())
        {
            MYINFO << "The device notified a configuration change not required by GUI, so the GUI must be refreshed";
            emit GUIupdateNeeded();
        }

        iqBuf = pb->getElem( bufIdx );
        MY_ASSERT( nullptr !=  iqBuf )
        MYDEBUG << "processing bufIdx " << bufIdx;
        timeStamp = iqBuf->timeStamp();

        if(iqBuf->data16ptr() == nullptr && iqBuf->data8ptr() == nullptr)
        {
            //invalid buffer or buffer length changed, discards it
            MYINFO << "invalid buffer, releasing bufIdx=" << bufIdx;
            pb->release(bufIdx);
            bufIdx = -1;
            continue;
        }

        waitIQms = static_cast<int>( statClock.restart() );

        if( iqBufSize != iqBuf->bufSize() )
        {
            isCfgChanged = true;
        }


        if( as->fpCmpf( binSize, static_cast<float>(as->getResolution()), 5 ) )
        {
            isCfgChanged = true;
        }


        if( FFTbins != as->getFFTbins() )
        {
            isCfgChanged = true;
        }


        if( resamplerBypass != as->getSsBypass() )
        {
            isCfgChanged = true;
        }


        if(dsBw != as->getDsBandwidth())
        {
            isCfgChanged = true;
        }



        if( isCfgChanged == true  )
        {
            isCfgChanged = false;

            dsBw = as->getDsBandwidth();
            iqBufSize = iqBuf->bufSize();
            binSize = static_cast<float>(as->getResolution());
            resamplerBypass = as->getSsBypass();
            FFTbins = as->getFFTbins();
            outputSamples = static_cast<int>(dsBw / binSize); //FFTbins;
            inputSamples = iqBufSize;

            MYINFO << "IQ buffers size " << iqBufSize << " doublets";
            MYINFO << "Input samples " << inputSamples << " doublets";
            MYINFO << "Output samples " << outputSamples << " doublets";

            //bandwidth, resolution  or sample rate changed: rebuilds the downsampler and fft plan
            if(rr != nullptr)
            {
                rresamp_crcf_destroy(rr);
                rr = nullptr;
            }
            double ratio = outputSamples;
            ratio /= inputSamples;
            if(as->getSampleRate() != as->getDsBandwidth())
            {
                MYINFO << "Creating rational resampler with ratio " << inputSamples << " ==> " << outputSamples
                       << " = " <<  ratio;
                skipResample = false;
            }
            else
            {
                MYINFO << "No resampling required (SR==dsBw)";
                skipResample = true;
            }
            rr = rresamp_crcf_create_default(static_cast<uint>(outputSamples), static_cast<uint>(inputSamples));  //note order output,input
            MY_ASSERT(rr != nullptr)
            decim = rresamp_crcf_get_decim(rr);
            MYINFO << "Decimation factor Q: " << decim;
            uint interp = rresamp_crcf_get_interp(rr);
            MYINFO << "Interpolation factor P: " << interp;
            uint blockLen = rresamp_crcf_get_block_len(rr);
            MYINFO << "Block len Q: " << blockLen;

            if(rrOut != nullptr)
            {
                delete[] rrOut;
                rrOut = nullptr;
            }
            rrOut = new liquid_float_complex[outputSamples];
            MY_ASSERT(rrOut != nullptr)
            memset(rrOut, 0, static_cast<size_t>(outputSamples) * sizeof(liquid_float_complex));

            if(rrIn != nullptr)
            {
                delete[] rrIn;
                rrIn = nullptr;
            }
            rrIn = new liquid_float_complex[inputSamples];
            MY_ASSERT(rrIn != nullptr)
            memset(rrIn, 0, static_cast<size_t>(inputSamples) * sizeof(liquid_float_complex));

            if(fftIn != nullptr)
            {
                delete []fftIn;
                fftIn = nullptr;
            }
            if(fftIn == nullptr)
            {
                fftIn = new liquid_float_complex[FFTbins];
                MY_ASSERT(fftIn != nullptr)
            }


            if(fftOut != nullptr)
            {
                delete []fftOut;
                fftOut = nullptr;
            }
            if(fftOut == nullptr)
            {
                fftOut = new liquid_float_complex[FFTbins];
                MY_ASSERT(fftOut != nullptr)
            }

            slotSetWindow();

            MYINFO << "Creating FFT plan, FFT size = " << FFTbins << " points";
            plan = fft_create_plan( static_cast<uint>(FFTbins), fftIn, fftOut, LIQUID_FFT_FORWARD, 0 );

            acqTime = (1000 * iqBufSize) / as->getSampleRate();

            //array init must be done after the plan
            memset(fftIn, 0, static_cast<size_t>(FFTbins) * sizeof(liquid_float_complex));
            memset(fftOut, 0, static_cast<size_t>(FFTbins) * sizeof(liquid_float_complex));

            nans = 0;
            waitIQms = 0;
            waitScanMs = 0;
            elapsedLoops = 0;
            resampMs = 0;
            FFTms = 0;

            norm = 1.0F / static_cast<float>(FFTbins);
            ready = true;
            MYINFO << "Radio ready";
        }

        performFFT();
        iqBuf = nullptr;

        MYDEBUG << "releasing bufIdx=" << bufIdx;
        pb->release(bufIdx);
        bufIdx = -1;

        elapsedLoops++;
    }

    MYINFO << "Radio thread stopping..." ;
    if( isDeviceOpen() )
    {
        MYINFO << "stopping " << devObj->className() << " first";
        devObj->stop();
        //devObj->wait();
    }

    stopRx = false;

    pb->flush();
    MYINFO << "Radio thread stopped" ;
    reset();

}

int Radio::performFFT()
{
    int j, f;
    float power = 0.0;
    QString ts;


    if(iqBuf != nullptr)
    {
        //getting a free fft output buffer
        scanIndex = ps->take();
        if(scanIndex != -1)
        {
            MYDEBUG << "took scan#" << scanIndex << " free=" << ps->peekFree() << " busy=" << ps->peekBusy() << " written=" << ps->peekWritten();
            droppedScans = 0;
            Scan* s = ps->getElem(scanIndex);
            MY_ASSERT( nullptr !=  s)

                    float* bins = s->data();
            MY_ASSERT( nullptr !=  bins)
                    s->setBufSize(FFTbins);

            waitScanMs = static_cast<int>(statClock.restart());

            //fresh samples to process, the fifo must be full

            if( as->getSsBypass() == false &&
                    !skipResample )
            {
                MYDEBUG << "performing resample...";
                if(iqBuf->getSizeofDoublet() == sizeof(liquid_int8_complex))
                {
                    for ( f = 0; f < inputSamples ; f++  )
                    {
                        if(pb->useExternalBuffers())
                        {
                            //direct buffers access, raw data are always unsigned (?)
                            //rrIn[f].real( iqBuf->getUReal8(f) - 128 );
                            //rrIn[f].imag( iqBuf->getUImag8(f) - 128 );
                            rrIn[f].real = iqBuf->getUReal8(f) - 128 ;
                            rrIn[f].imag = iqBuf->getUImag8(f) - 128 ;

                        }
                        else
                        {
                            //stream access
                            //rrIn[f].real( iqBuf->getReal8(f) );
                            //rrIn[f].imag( iqBuf->getImag8(f) );
                            rrIn[f].real = iqBuf->getReal8(f) ;
                            rrIn[f].imag = iqBuf->getImag8(f) ;
                        }
                    }
                }
                else
                {
                    for ( f = 0; f < inputSamples ; f++  )
                    {
                        if(pb->useExternalBuffers())
                        {
                            //direct buffers access, raw data are always unsigned (?)
                            rrIn[f].real = iqBuf->getUReal16(f) - 32768;
                            rrIn[f].imag = iqBuf->getUImag16(f) - 32768;
                        }
                        else
                        {
                            //stream access
                            rrIn[f].real = iqBuf->getReal16(f) ;
                            rrIn[f].imag = iqBuf->getImag16(f) ;
                        }
                    }
                }

                rresamp_crcf_execute(rr, rrIn, rrOut);   //note order rr, inputs, outputs

                for ( f = 0 ; f < outputSamples ; f++ )
                {
                    fftIn[f].real = rrOut[f].real * windowCoefs[f];
                    fftIn[f].imag = rrOut[f].imag * windowCoefs[f];
                }
            }
            else
            {
                if(iqBuf->getSizeofDoublet() == sizeof(liquid_int8_complex))
                {
                    for ( f = 0 ; f < outputSamples ; f++ )
                    {
                        if(pb->useExternalBuffers())
                        {
                            //direct buffers access
                            fftIn[f].real = ( iqBuf->getUReal8(f) - 128 ) * windowCoefs[f] ;
                            fftIn[f].imag = ( iqBuf->getUImag8(f) - 128 ) * windowCoefs[f] ;
                        }
                        else
                        {
                            //stream access
                            fftIn[f].real = iqBuf->getReal8(f) * windowCoefs[f] ;
                            fftIn[f].imag = iqBuf->getImag8(f) * windowCoefs[f] ;

                        }
                    }
                }
                else
                {
                    for ( f = 0 ; f < outputSamples ; f++ )
                    {
                        if(pb->useExternalBuffers())
                        {
                            //direct buffers access
                            fftIn[f].real = ( iqBuf->getUReal16(f) - 32768 ) * windowCoefs[f] ;
                            fftIn[f].imag = ( iqBuf->getUImag16(f) - 32768 ) * windowCoefs[f] ;
                        }
                        else
                        {
                            //stream access
                            fftIn[f].real = iqBuf->getReal16(f) * windowCoefs[f] ;
                            fftIn[f].imag = iqBuf->getImag16(f) * windowCoefs[f] ;
                        }
                    }
                }

            }

            resampMs = static_cast<int>(statClock.restart());

            MYDEBUG << "performing FFT...";
            fft_execute(plan);

            MYDEBUG << "normalizing FFT output...";


            float avgp = 0;

            if( as->isAudioDevice() )
            {
                //Audio spectra:
                //DC elem is [0] and the first ((FFTbins-1) / 2) elements are positive frequencies bins
                //since negative frequencies are not used, an implicit binning x 2 is applied in order to fill
                //all the bins with valid data.
                //This means that the displayed spectra will cover half of the selected sample rate
                for (j=0; j < FFTbins/2; j++)
                {
                    power = toLinearPower( fftOut[j] );
                    if(power == FLT_MIN)
                    {
                        break;
                    }

                    avgp = avgp + power;

                    f = j*2;
                    MY_ASSERT(f >= 0)

                            //binning x 2
                            bins[f] =  power;
                    bins[f+1] =  power;

                    fftIn[j].real = 0;
                    fftIn[j].imag = 0;

                    fftIn[j+1].real = 0;
                    fftIn[j+1].imag = 0;
                }
            }
            else
            {
                //DC elem is [0] so i must shift and mirror
                //in this way:
                //-------------------------------------------------------->| (   positive frequencies bins [1]...[FFTbins/2-1]    )
                //         ((FFTbins-1) / 2)                             +         ((FFTbins-1) / 2)
                // (negative frequencies bins [FFTbins]...[FFTbins/2]) |<---------------------------------------------------------
                //in order to set it at center and have positive
                //frequences at right and negative at left.

                int offset = int (FFTbins/2);
                for (j=0; j < FFTbins; j++)
                {
                    power = toLinearPower( fftOut[j] );
                    if(power == FLT_MIN)
                    {
                        break;
                    }

                    avgp = avgp + power;


                    if (j < offset)
                    {
                        f = j + offset;
                    }
                    else
                    {
                        //f = (j - offset) + 1; //+1 to skip DC at [0]
                        f = (j - offset);

                    }
                    MY_ASSERT(f >= 0)
                            bins[f] =  power;

                    fftIn[j].real = 0;
                    fftIn[j].imag = 0;
                }
            }

            if(power == FLT_MIN)
            {
                //releases the unused scan
                MYINFO << "releasing invalid scan " << scanIndex << " timestamp=" << timeStamp;
                ps->release(scanIndex);
                return 0;
            }

            avgPower = avgp / FFTbins;
            s->setBufSize(FFTbins);
            s->setBinSize(binSize);
            s->setTimeStamp(timeStamp);
            MYDEBUG << "forwarding true scan to Control::wfDbfs(), timestamp=" << timeStamp;
            ps->forward(scanIndex);
            FFTms = static_cast<int>(statClock.restart());
            return 1;

        }
        else
        {
            droppedScans++;
            MYDEBUG << "no free output scan available";
        }
    }

    return 0;
}





void Radio::reset()
{
    MYINFO << "Resetting device..." ;

    devObj->stop();
    //devObj->wait();

    if(fftIn != nullptr)
    {
        delete []fftIn;
        fftIn = nullptr;
    }

    if(fftOut != nullptr)
    {
        delete []fftOut;
        fftOut = nullptr;
    }


    if(scanIndex != -1)
    {
        MYDEBUG << "releasing binsIndex=" << scanIndex;
        ps->release(scanIndex);
    }


    if(windowCoefs != nullptr)
    {
        delete[] windowCoefs;
        windowCoefs = nullptr;
    }

    //everything set to default values

    iqBuf = nullptr;

    isCfgChanged = true;
    FFTbins = DEFAULT_FFT_BINS;
    binSize = DEFAULT_BIN_SIZE;
    scanIndex = -1;
    norm = 0;   //scaling factor for fft output values

    wFunc = FTW_RECTANGLE;

    ready = false;
}

QString Radio::getDeviceName()
{
    if( !isDeviceOpen() )
    {
        return "";
    }

    DeviceMap elem = {-1,""};
    foreach(elem, devices)
    {
        if(elem.index == devIndex)
        {
            break;
        }
    }

    return elem.name;
}


//device slots--------------------------------------------

APP_STATE Radio::slotSetDevice(int deviceIndex)
{
    MYINFO << __func__ << "(" << deviceIndex << ")" << MY_ENDL;
    APP_STATE retVal = AST_NONE;

    if(devObj != nullptr)
    {
        MYINFO << "slotSetDevice() stopping and killing device before rebirth";
        devObj->stop();
        //devObj->wait();
        delete devObj;
        devObj = nullptr;
    }

    DeviceMap elem;

    if( deviceIndex < 0 )
    {
        QString s = as->getDevice();
        QStringList sl = s.split(":");
        foreach (elem, devices)
        {
            if( elem.name.contains( sl[1] ) )
            {
                deviceIndex = elem.index;
            }
        }
    }

    foreach(elem, devices)
    {
        if( deviceIndex == elem.index )
        {
            ps->flush();
            pb->flush();

            if( elem.name.contains("Test"))
            {
                //no RTLSDR devices plugged in and no dongle server found
                MYINFO << "opens the test device";
                devObj = new FuncGen(as, pb);
            }
            else if( elem.name.contains("UDP")  )
            {
                MYINFO << "opens the dongle client";
                devObj = new DongleClient(as, pb);
            }
            else //physical device managed by SoapySDR library
            {
                MYINFO << "opens the dongle server";
                devObj = new SyncRx(as, pb, nullptr, deviceIndex);
            }

            if (devObj != nullptr && devObj->isDeviceOpen())
            {
                retVal = AST_OPEN;

                //forwards device faults
                connect( devObj, SIGNAL( fault(int) ), this, SIGNAL( fault(int) ) );
            }
            else
            {
                retVal =  AST_ERROR;
            }
        }
    }


    devIndex = deviceIndex;
    return static_cast<APP_STATE>(retVal);
}

bool Radio::slotSetSampleRate()
{   
    if(isDeviceOpen())
    {
        return devObj->slotSetSampleRate();
    }
    return false;
}

bool Radio::slotSetDsBandwidth()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetDsBandwidth();
    }
    return false;
}

bool Radio::slotSetTune()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetTune();
    }
    return false;
}


bool Radio::slotSetGain()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetGain();
    }
    return false;
}


bool Radio::slotSetError()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetError();
    }
    return false;
}

bool Radio::slotSetAntenna()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetAntenna();
    }
    return false;
}

bool Radio::slotSetBandwidth()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetBandwidth();
    }
    return false;
}

bool Radio::slotSetFreqRange()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetFreqRange();
    }
    return false;
}









//FFT slots:

bool Radio::slotSetResolution()
{
    if(isDeviceOpen())
    {
        return devObj->slotSetResolution();
    }
    return true;
}

bool Radio::slotSetWindow()
{

    if (windowCoefs != nullptr)
    {
        delete windowCoefs;
    }
    windowCoefs = new float[ FFTbins ];
    MY_ASSERT( nullptr !=  windowCoefs)

    wFunc = as->getWindow();
    for ( int i = 0 ; i < FFTbins ; i++ )
    {
        switch(wFunc)
        {
            case FTW_BARTLETT:
                windowCoefs[i] = bartlett(i, FFTbins);
                break;

            case FTW_BLACKMAN:
                windowCoefs[i] = blackman(i, FFTbins);
                break;

            case FTW_HANN_POISSON:
                windowCoefs[i] = hannPoisson(i, FFTbins);
                break;

            case FTW_YOUSSEF:
                windowCoefs[i] = youssef(i, FFTbins);
                break;

            //from liquidsdr lib:
#if defined(WINDOZ) && defined(COMPILER_BRAND_MSVC64)
            case FTW_HAMMING:
                windowCoefs[i] = liquid_hamming(static_cast<uint>(i), static_cast<uint>(FFTbins));
                break;

            case FTW_BLACKMAN_HARRIS:
                windowCoefs[i] = liquid_blackmanharris(static_cast<uint>(i), static_cast<uint>(FFTbins));
                break;

            case FTW_BLACKMAN_HARRIS_7:
                windowCoefs[i] =liquid_blackmanharris7(static_cast<uint>(i), static_cast<uint>(FFTbins));
                break;

            case FTW_HANN:
                windowCoefs[i] =liquid_hann(static_cast<uint>(i), static_cast<uint>(FFTbins));
                break;

            case FTW_FLATTOP:
                windowCoefs[i] =liquid_flattop(static_cast<uint>(i), static_cast<uint>(FFTbins));
                break;
#else
        case FTW_HAMMING:
            windowCoefs[i] = hamming(static_cast<uint>(i), static_cast<uint>(FFTbins));
            break;

        case FTW_BLACKMAN_HARRIS:
            windowCoefs[i] = blackmanharris(static_cast<uint>(i), static_cast<uint>(FFTbins));
            break;

        case FTW_BLACKMAN_HARRIS_7:
            windowCoefs[i] = blackmanharris7(static_cast<uint>(i), static_cast<uint>(FFTbins));
            break;

        case FTW_HANN:
            windowCoefs[i] = hann(static_cast<uint>(i), static_cast<uint>(FFTbins));
            break;

        case FTW_FLATTOP:
            windowCoefs[i] = flattop(static_cast<uint>(i), static_cast<uint>(FFTbins));
            break;
#endif

            case FTW_RECTANGLE:
                windowCoefs[i] = rectangle(i, FFTbins);
                break;
        }

    }

    switch(wFunc)
    {
        case FTW_BARTLETT:
            MYINFO << "Window set to BARTLETT";
            break;

        case FTW_HAMMING:
            MYINFO << "Window set to HAMMING";
            break;

        case FTW_BLACKMAN:
            MYINFO << "Window set to BLACKMAN";
            break;

        case FTW_BLACKMAN_HARRIS:
            MYINFO << "Window set to BLACKMAN-HARRIS";
            break;

        case FTW_HANN_POISSON:
            MYINFO << "Window set to POISSON";
            break;

        case FTW_YOUSSEF:
            MYINFO << "Window set to YOUSSEF";
            break;

        case FTW_HANN:
            MYINFO << "Window set to HANN";
            break;

        case FTW_BLACKMAN_HARRIS_7:
            MYINFO << "Window set to BLACKMAN-HARRIS-7";
            break;

        case FTW_FLATTOP:
            MYINFO << "Window set to FLAT TOP";
            break;

        case FTW_RECTANGLE:
            MYINFO << "Window set to RECTANGLE";
            break;
    }

    return true;
}

//end slots ---------------------------------------

int Radio::deviceSearch()
{
    MYINFO << __func__ << "()" << MY_ENDL;
    /*
     * note: the devices list (so the relative indexes) is ordered in this way:
     *
     * the physical dongles come first,
     * then the UDP dongle server, only one if defined
     * last the test device, only one always present
     *
     * Each time a new configuration is loaded,
     * the search must be repeated
     *
    */


    bool testDevMissing = true;
    bool udpDevMissing = true;
    devices.clear();

    //lowest indexes are about dongles
    MYINFO << "looking for dongles...";
    SyncRx::lookForDongleStatic(&devices);

    //then the dongle server, if defined
    MYINFO << "looking for servers...";
    if( DongleClient::lookForServerStatic(as, &devices) )
    {
        udpDevMissing = false;
    }

    DeviceMap dm;
    if(devices.count() > 0)
    {
        dm = devices.last();
        dm.index++;
    }
    else
    {
        //no dongles neither dongle servers found
        dm.index = 0;
    }

    MYINFO << "adding test patterns...";
    foreach(dm, devices)
    {
        if(dm.name.contains( "Test patterns" ))
        {
            //add the test device avoiding duplicates
            testDevMissing = false;
        }
    }

    int delThis = -1;

    for(int i = 0; i < devices.count(); i++)
    {
        dm = devices.at(i);
        if(dm.name.contains( "UDP" ) && udpDevMissing)
        {
            //remove the udp device from the list
            delThis = i;
            break;
        }
    }

    if(delThis != -1)
    {
        devices.removeAt(delThis);
        dm.index--; //to shift down the index counter of test device
    }

    if(testDevMissing)
    {
        dm.index++;
        dm.name = QString( "%1: Test patterns" ).arg(dm.index);
        devices.append( dm );
    }

    return devices.count();
}


float Radio::toLinearPower(liquid_float_complex cmp)
{  
    float power;

    if( std::isnan( cmp.real ) || std::isnan( cmp.imag ) )
    {
        nans++;
        MYINFO << "fft produced a NAN output";
        //MY_ASSERT(nans == 0);
        cmp.real = 0;
        cmp.imag = 0;
        return FLT_MIN;
    }

    //extracting the linear power
    //see https://dsp.stackexchange.com/questions/19615/converting-raw-i-q-to-db

    liquid_float_complex cmpNorm;
    cmpNorm.real =  cmp.real * norm ;
    cmpNorm.imag =  cmp.imag * norm ;

    power = cmpNorm.real * cmpNorm.real +  cmpNorm.imag * cmpNorm.imag;
    if(power == 0.0)
    {
        MYDEBUG << "FFT produced zero power, will result in infinite dB, fixing it";
        power = avgPower;

    }
    return power;
}



float Radio::rectangle(int i, int length)
{
    Q_UNUSED(i)
    Q_UNUSED(length)
    return 1.0;
}

/*
float Radio::hamming(int i, int length)
{
    double a, b, w, N1;
    a = 25.0/46.0;
    b = 21.0/46.0;
    N1 = static_cast<double>(length-1);
    w = a - b*cos(2*i*M_PI/N1);
    return static_cast<float>(w);
}
*/

float Radio::blackman(int i, int length)
{
    double a0, a1, a2, w, N1;
    a0 = 7938.0/18608.0;
    a1 = 9240.0/18608.0;
    a2 = 1430.0/18608.0;
    N1 = static_cast<double>(length-1);
    w = a0 - a1*cos(2*i*M_PI/N1) + a2*cos(4*i*M_PI/N1);
    return static_cast<float>(w);
}

/*
float Radio::blackmanHarris(int i, int length)
{
    double a0, a1, a2, a3, w, N1;
    a0 = 0.35875;
    a1 = 0.48829;
    a2 = 0.14128;
    a3 = 0.01168;
    N1 = static_cast<double>(length-1);
    w = a0 - a1*cos(2*i*M_PI/N1) + a2*cos(4*i*M_PI/N1) - a3*cos(6*i*M_PI/N1);
    return static_cast<float>(w);
}
*/

float Radio::hannPoisson(int i, int length)
{
    double a, N1, w;
    a = 2.0;
    N1 = static_cast<double>(length-1);
    w = 0.5 * (1 - cos(2*M_PI*i/N1)) *
        pow(M_E, (-a * static_cast<double>( abs( static_cast<int>(N1-1-2*i))) )/N1);
     return static_cast<float>(w);
}


float Radio::youssef(int i, int length)
{
    double a, a0, a1, a2, a3, w, N1;
    a0 = 0.35875;
    a1 = 0.48829;
    a2 = 0.14128;
    a3 = 0.01168;
    N1 = static_cast<double>(length-1);
    w = a0 - a1*cos(2*i*M_PI/N1) + a2*cos(4*i*M_PI/N1) - a3*cos(6*i*M_PI/N1);
    a = 0.0025;
    w *= pow(M_E, (-a * static_cast<double>( abs( static_cast<int>(N1-1-2*i))) )/N1);
    return static_cast<float>(w);
}


float Radio::bartlett(int i, int length)
{
    double N1, L, w;
    L = static_cast<double>( length );
    N1 = L - 1;
    w = (i - N1/2) / (L/2);
    if (w < 0)
    {
        w = -w;
    }
    w = 1 - w;
    return static_cast<float>(w);
}


bool Radio::getStats( StatInfos* stats )
{
    stats->sr.nans = nans;
    stats->sr.waitScanMs = waitScanMs;
    stats->sr.waitIQms = waitIQms;
    stats->sr.FFTbins = FFTbins;
    stats->sr.inputSamples = iqBufSize;
    stats->sr.outputSamples = outputSamples;
    stats->sr.binSize = binSize;
    stats->sr.acqTime = acqTime;
    stats->sr.elapsedLoops = elapsedLoops;
    stats->sr.resampMs = resampMs;
    stats->sr.FFTms = FFTms;
    stats->sr.avgPower = avgPower;
    stats->sr.droppedScans = droppedScans;

    return ( isDeviceOpen() )? devObj->getStats(stats) : false;
}

int Radio::getHighBound()
{
    if( !isDeviceOpen() )
    {
        return false;
    }
    return devObj->getHighBound();
}


int Radio::getLowBound()
{
    if( !isDeviceOpen() )
    {
        return false;
    }
    return devObj->getLowBound();
}

bool Radio::allowsFrequencyCorrection()
{
    if( !isDeviceOpen() )
    {
        return false;
    }
    return devObj->allowsFrequencyCorrection();
}
