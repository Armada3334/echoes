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
$Rev:: 268                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-07-19 07:45:48 +02#$: Date of last commit
$Id: syncrx.cpp 268 2018-07-19 05:45:48Z gmbertani $
*******************************************************************************/

#include "setup.h"
#include "syncrx.h"


#define STEP_US 1000

static QList<DeviceMap> deviceList;

bool SyncRx::lookForDongleStatic( QList<DeviceMap>* devices )
{
    MYINFO << __func__;

    MY_ASSERT( nullptr !=  devices)

    QString buf;
    uint i;
    uint64_t deviceCount = 0;
    DeviceMap dm;
    SoapySDR::KwargsList detected;

    foreach(dm, *devices)
    {
        if(!dm.name.contains("Test", Qt::CaseSensitive) &&
           !dm.name.contains("UDP", Qt::CaseSensitive) )
        {
            //considers Soapy devices only, excluding virtual ones
            //like test patterns and udp server.
            deviceCount++;
        }
    }

    MYINFO << "SOAPY_SDR_ROOT_PATH=" << qEnvironmentVariable("SOAPY_SDR_ROOT_PATH");
    MYINFO << "SOAPY_SDR_PLUGIN_PATH=" << qEnvironmentVariable("SOAPY_SDR_PLUGIN_PATH");

    std::vector<std::string> searchPaths = SoapySDR::listSearchPaths();
    std::string sPath;
    foreach(sPath, searchPaths)
    {
        MYINFO << "searching path: " << QString(sPath.c_str());
    }

    MYINFO << "Configured devices:" << deviceCount;
    if(deviceCount == 0)
    {
        //if the dongles are already known
        //avoids to rescan the usb bus
        MYINFO << "Looking for new devices...";

        detected = SoapySDR::Device::enumerate();
        deviceCount = detected.size();

        if (deviceCount > 0)
        {        
            QString driver, label, serial;
            std::string tmp;
            SoapySDR::Kwargs::iterator it;
            SoapySDR::Kwargs current;

            MYINFO << "Found " << deviceCount << "device(s):";

            for (i = 0; i < deviceCount; i++)
            {
                current = detected.at(i);

                MYINFO << "Device #" << i;
                tmp = current.at("driver");
                driver = tmp.c_str();
                tmp = current.at("label");
                label = tmp.c_str();
                if( current.count("serial") == 1 )
                {
                    tmp = current.at("serial");
                    serial = tmp.c_str();
                }
                else
                {
                    //no serial number present, invent one
                    serial.setNum(i);
                }

                buf=QString("%1:%2 %3 SN%4")
                        .arg(i)
                        .arg(driver)
                        .arg(label)
                        .arg(serial);

                dm.name = buf;
                dm.index = static_cast<int>(i);
                devices->append(dm);
                MYINFO << buf;

                //dumps all the informations detected by Soapy
                for( it = detected.at(i).begin(); it != detected.at(i).end(); ++it )
                {
                    MYINFO << it->first.c_str() << " = " << it->second.c_str();
                }


            }
        }
        else
        {
            MYINFO << "none found.";
        }
    }
    deviceList = *devices;
    return (devices->count() > 0);
}



SyncRx::SyncRx(Settings* appSettings, Pool<IQbuf*>* bPool, QThread *parentThread, int devIndex) :
    IQdevice(appSettings, bPool, parentThread)
{
    setObjectName("SyncRx thread");
    owner = parentThread;
    MYINFO << "SyncRx constructor" << MY_ENDL;
    MYINFO << "parentThread=" << parentThread << " parent()=" << parent();

    ready = false;
    isAudioDevice = false;
    dev = nullptr;
    stream = nullptr;
    channel = 0;
    sentBuffersCount = 0;
    forwardedBuffersCount = 1;
    sysFaultCount = 0;
    offsetGain = MIN_DBFS;
    rxStreamFS = 0;
    overflows = 0;
    underflows = 0;
    droppedSamples = 0;
    dbDelayMs = 1;
    timeAcq = 0;
    avgTimeAcq = 0;
    iqBufRoundSize = 0;
    streamChannel = 0;
    timeNs = 0;
    check8.real = 0;
    check8.imag = 0;
    check16.real = 0;
    check16.imag = 0;

    cfgChangesCount = as->changes();
    serverSocket = nullptr;
    clients.clear();

    //experimental
    iqRecordOn = false;

    minTune = 0;
    maxTune = 0;

    MYDEBUG << "invoking enumerate()";
    SoapySDR::KwargsList detected = SoapySDR::Device::enumerate();
    MYDEBUG << "detected[] size=" << detected.size();
    MYDEBUG << "getting devInfos at devIndex=" << devIndex;
    SoapySDR::Kwargs devInfo = detected.at(devIndex);


    //workaround for SoapyAirspy bug
    //https://github.com/pothosware/SoapySDR/issues/433
    SoapySDR_setLogLevel(SOAPY_SDR_INFO);

    MYINFO << "invoking make()";
    dev = SoapySDR::Device::make( devInfo );
    MYDEBUG << "exiting make()";
    if (dev == nullptr)
    {
        MYCRITICAL << "SoapySDR::Device::make() failed on " << devInfo.at("label").c_str();
        emit status( tr("Failed opening SoapySDR device."), AST_CANNOT_OPEN );
        return;
    }
    MYINFO << "SoapySDR device opened";

    //end workaround
    SoapySDR_setLogLevel(SOAPY_SDR_DEBUG);

    if(!devInfo["driver"].compare("audio"))
    {
        isAudioDevice = true;
    }

    std::vector< std::string > strVec;
    strVec = dev->listAntennas( SOAPY_SDR_RX, channel );
    if(strVec.size() != 0)
    {
        antennas = as->fromStrVect(strVec);
    }

    SoapySDR::RangeList rangeVec;
    rangeVec = dev->getSampleRateRange( SOAPY_SDR_RX, channel );
    if(rangeVec.size() != 0)
    {
        if(isAudioDevice)
        {
            srRanges = as->fromRangeVect(rangeVec, 0, 1, true);
        }
        else
        {
            srRanges = as->fromRangeVect(rangeVec);
        }

    }
    MYINFO << "total " << srRanges.count() << " sample rates ranges: " << srRanges;

    rangeVec = dev->getFrequencyRange( SOAPY_SDR_RX, channel );
    if(rangeVec.size() != 0)
    {
        frRanges = as->fromRangeVect(rangeVec, 0, 1000);
    }

    //patch: SoapyAudio returns range 0:6Mhz, senseless for audio devices
    //set 0 to 192 khz
    if(isAudioDevice)
    {
        frRanges[0]="0 : 192000";
    }

    MYINFO << "total " << frRanges.count() << " frequency ranges: " << frRanges;

    rangeVec = dev->getBandwidthRange( SOAPY_SDR_RX, channel );
    if(rangeVec.size() != 0)
    {
        bbRanges = as->fromRangeVect(rangeVec);
    }
    else
    {
        //if no band filter ranges are defined, takes the sample rate ranges
        bbRanges = srRanges;
    }
    MYINFO << "total " << bbRanges.count() << " band filter ranges: " << bbRanges;

    QString gainName = (isAudioDevice == false)? "TUNER" : "AUDIO";

    gainRange = dev->getGainRange( SOAPY_SDR_RX, channel, gainName.toStdString() );

    MYINFO << "Gain range: 0% == " << gainRange.minimum() << " : 100% == " << gainRange.maximum() << " step=" << gainRange.step();

    //Echoes tries to use the device's native stream format. By default is CS16 but CS8 is supported
    //as well. Even if resolution is lower than CS16, wth CS8 the waterfall can scroll faster.
    //In case of other native formats, the CS16 will be used.
    dataFormat = QString( dev->getNativeStreamFormat( SOAPY_SDR_RX, channel, rxStreamFS ).c_str() );
    MYINFO << "Native rx stream format: " << dataFormat;
    if(dataFormat != SOAPY_SDR_CS8 && dataFormat != SOAPY_SDR_CS16)
    {
        dataFormat = SOAPY_SDR_CS16;
    }

    //creates a stream with the unique purpose of reading its MTU and publish it
    SoapySDR::Stream* testStream = dev->setupStream( SOAPY_SDR_RX, dataFormat.toStdString() );
    if(testStream == nullptr)
    {
        MYCRITICAL << "Cannot create test RX stream";
        return;
    }

    rxStreamMTU = static_cast<uint>( dev->getStreamMTU( testStream ) );
    MYINFO << " fullscale: " << rxStreamFS << " MTU: " << rxStreamMTU;
    dev->closeStream( testStream );    
    //the GUI must be informed of this limit
    as->setMaxRes( static_cast<double>( rxStreamMTU ));


    if(as->getRTSrevision() == 0 && as->getConfigName() == "default")
    {
        //initialize the settings appropriate for the device

        //by default initialize the dongle with the highest frequency range supported
        frRange = frRanges.last();
        as->setFreqRange(frRange);

        bool ok = false;
        int val = 0;

        //the default band filter is the highest supported by the device
        bbRange = bbRanges.last();
        if(bbRange.contains(':'))
        {
            //range is a real range
            //the default band filter is set to the highest value
            val = bbRange.split(':').at(1).toInt(&ok);
        }
        else
        {
            //range is a discrete value
            //the default band filter is set as the given value
            val = bbRange.toInt(&ok);
            MY_ASSERT(ok == true)
        }
        as->setBandwidth(val);

        srRange = srRanges.last();
        if(srRange.contains(':'))
        {
            //range is a real range
            //the default sample rate is set to the highest value
            val = srRange.split(':').at(1).toInt(&ok);
            MY_ASSERT(ok == true)
        }
        else
        {
            //range is a discrete value
            //the default sample rate is set as the given value
            val = srRange.toInt(&ok);
            MY_ASSERT(ok == true)
        }

        as->setSampleRate(val);
        as->setDsBandwidth(val);
        if(isAudioDevice)
        {
            //the default tuning frequency is a quarter of SR (==BW)
            val /= 4;
        }
        else
        {
            //the default tuning frequency is in first quarter of the range
            //(not the middle to avoid the problem with E4000 tuning hole)
            val = frRange.split(':').at(0).toInt(&ok);
            val += frRange.split(':').at(1).toInt(&ok);
            val /= 4;
            MY_ASSERT(ok == true)
        }
        as->setTune(val * 1000); //khz to hz
    }

   //
    SoapySDR::ArgInfoList si = dev->getSettingInfo();
    MYINFO <<  "----------------------";
    MYINFO <<  "Driver settings:";
    for(uint i = 0; i < si.size(); i++)
    {
        SoapySDR::ArgInfo info = si.at(i);
        MYINFO << "name: " << QString::fromStdString(info.name);
        MYINFO << "desc: " <<  QString::fromStdString(info.description);
        MYINFO << "type: " << info.type;
        MYINFO << "key: " <<  QString::fromStdString(info.key);
        MYINFO << "value: " <<  QString::fromStdString(info.value);
        MYINFO << "range: " << info.range.minimum() << ", " << info.range.maximum() << ", " << info.range.step();
        MYINFO << "unit: " <<  QString::fromStdString(info.units);

        MYINFO << "----------------------";
    }

    MYINFO <<  "----------------------";
    MYINFO << "Stream arguments:";
    hasBufflen = false, hasBuffers = false;
    si = dev->getStreamArgsInfo( SOAPY_SDR_RX, channel );
    for(uint i = 0; i < si.size(); i++)
    {
        SoapySDR::ArgInfo info = si.at(i);
        MYINFO << "name: " << QString::fromStdString(info.name);
        MYINFO << "desc: " <<  QString::fromStdString(info.description);
        MYINFO << "type: " << info.type;
        MYINFO << "key: " <<  QString::fromStdString(info.key);
        MYINFO << "value: " <<  QString::fromStdString(info.value);
        MYINFO << "range: " << info.range.minimum() << ", " << info.range.maximum() << ", " << info.range.step();
        MYINFO << "unit: " <<  QString::fromStdString(info.units);

        MYINFO << "----------------------";

        if(info.key == "bufflen")
        {
            hasBufflen = true;
        }

        if(info.key == "buffers")
        {
            hasBuffers = true;
        }

    }
    //The device is now partially initialized. The GUI now can show the device capabilities
    //and allow the user to set them singularly or in group by loading a setup file (rts).
    //The settings are applied to the device while pressing the start button

}

SyncRx::~SyncRx()
{
     MYINFO << "SyncRx destructor" << MY_ENDL;
    //freeing clients references
    clients.clear();

    //closing local stream
    if(stream != nullptr)
    {
        MYINFO << "Closing opened stream";
        dev->deactivateStream(stream);
        dev->closeStream(stream);
        stream = nullptr;
    }

    //destroying server
    if(serverSocket != nullptr)
    {
        serverSocket->deleteLater();
        //serverSocket = nullptr;
    }

    if(dev != nullptr)
    {
        SoapySDR::Device::unmake( dev );
        MYINFO << "called unmake() - device closed";
        dev = nullptr;
    }
}

int SyncRx::calcBufSize()
{
    //by enabling direct buffers, the user can select any combination
    //of FFT resolution vs. bandwidth, even senseless ones, since the MTU
    //will be determined by the <bufflen> stream parameter, that is
    //assigned equal to iqBufRoundSize

    //resolution = dsBW / FFTbins
    //iqBufSize = SR x (FFTbins / dsBW) = SR / resolution = FFTbins x dsRatio



    int iqBufSize = static_cast<int>( as->getFFTbins() * as->getDSratio() );
    MYINFO << "temporary iqBufSize = fftBins(" << as->getFFTbins() << ") * dsRatio(" << as->getDSratio() << ")";
    iqBufRoundSize = as->roundUp(iqBufSize, BUFFER_BLOCKSIZE);

    //without direct buffers, the FFT resolution the user can set will be limited
    //by the stream MTU returned by Soapy
    if( iqBufRoundSize > static_cast<int>(rxStreamMTU) )
    {
        iqBufSize = static_cast<int>(rxStreamMTU);
        iqBufRoundSize = as->roundUp(iqBufSize, BUFFER_BLOCKSIZE);
    }

    //the GUI must be informed of this limit
    as->setMaxRes( static_cast<double>( rxStreamMTU ));

    MYINFO << "calcBufSize() = " << iqBufRoundSize;
    return iqBufRoundSize;
}


void SyncRx::run()
{
    //dev must be opened before starting this thread
    MY_ASSERT(dev != nullptr)

    MYINFO << "SDR RX channel #" << channel << " acquisition thread started";

    int bufIdx = -1;
    int retVal = 0;
    int nextHandle = 0xffff;
    IQbuf* buf = nullptr;
    unsigned long tmax = (1200 * static_cast<unsigned long>(as->getInterval()));
    unsigned long tmin = STEP_US, tmid = tmax / 2;

    //int tmid = 1000 * static_cast<unsigned long>(as->getInterval());

    sysFaultCount = 0;
    stopThread = false;

    acqTimer.start();

    //recording stuff
    //the purpose is to record iq buffers in a file with real meteor echoes inside
    //in order to be replayed later by funcgen without dongle for easier parameters tuning
    XQDir recDir( XQDir::temp() );
    QString recName = recDir.absoluteFilePath("echoesIQsamples.dat");
    QFile rec( recName );
    QDataStream ds(&rec);
    uint count  = 0;
    const uint magic = IQDUMP_MAGIC;

    if(iqRecordOn)
    {
        if (rec.open(QIODevice::WriteOnly) == false)
        {
            MYWARNING << "IQ recording required but unable to open file " << recName;
            iqRecordOn = false;
        }
        else
        {
            MYINFO << "IQ recording active on file " << recName;
        }
    }

    if(stream != nullptr)
    {
        MYFATAL("BUG: attempt to setup an already existing stream %p", static_cast<void*>(stream));
    }



    calcBufSize();
    stream = dev->setupStream( SOAPY_SDR_RX,
                               dataFormat.toStdString(),
                               std::vector< size_t >(),
                               streamArgs );
    if(stream == nullptr)
    {
        MYCRITICAL << "Cannot create RX stream";
        return;
    }

    DCspikeRemoval(true);

    rxStreamMTU = static_cast<uint>( dev->getStreamMTU( stream ) );
    MYINFO << " fullscale: " << rxStreamFS << " MTU: " << rxStreamMTU;

    int numBufs = static_cast<int>( dev->getNumDirectAccessBuffers(stream) );
    IQbuf* iqBuf = nullptr;

    if( pb->size() == 0 )
    {
        //IQbuf for stream access
        for (int i = 0; i < DEFAULT_POOLS_SIZE; i++)
        {
            if(dataFormat == SOAPY_SDR_CS16)
            {
                iqBuf = new IQbuf( MAX_SAMPLE_BUF_SIZE, sizeof(liquid_int16_complex) );
            }
            else
            {
                iqBuf = new IQbuf( MAX_SAMPLE_BUF_SIZE, sizeof(liquid_int8_complex) );
            }
            MY_ASSERT( nullptr !=  iqBuf)
            pb->insert(i, iqBuf);
        }
        MYINFO << "created " << numBufs << " IQbufs";
    }

    retVal = dev->activateStream(stream);
    MYINFO << "activating stream, returned code " << retVal << " : " << SoapySDR_errToStr(retVal);
    if(retVal == 0)
    {

        //this instance is a dongle server, so it can read the samples directly from
        //a SDR dongle and forward them - besides the instance threads,
        //to other instances via UDP
        if(serverSocket == nullptr)
        {
            startServer();
        }

        if(as->isDumbMode())
        {
            MYINFO << "Dumb mode selected, this instance runs as pure server, no events catching";
        }

        stopThread = false;
        unsigned latency = 0;
        int fragCounter = 0;
        forever
        {
            qint64 lastTimeNs = 0;


            if( /* isFinished() == true || isInterruptionRequested() == true || */ stopThread == true)
            {
                MYINFO << "SyncRx thread stopped";
                if(iqRecordOn)
                {
                    rec.close();
                }
                break;
            }

            if(serverSocket->isValid())
            {
                readPendingClientDatagrams();
            }

            //TODO: only changes in device+fft tabs should be notified
            if(as->serverChanges() != cfgChangesCount)
            {
                cfgChangesCount = as->serverChanges();

                avgTimeAcq = 0;

                shareDeviceConfig();
                MYINFO << "sharing done";
            }
            
            timeAcq = acqTimer.elapsed();
            latency = timeAcq * 1000;
            avgTimeAcq = (avgTimeAcq + static_cast<int>( acqTimer.restart() )) / 2;
            MYDEBUG << "timeAcq=" << timeAcq;

            // use Soapy streaming, easier to manage but a bit slower because the data
            // stream blocks must be copied in IQbufs before forwarding to Radio


            bufIdx = pb->take();
            if(bufIdx != -1)
            {
                droppedSamples = 0;

                buf = pb->getElem(bufIdx);

                if(buf != nullptr)
                {
                    if( buf->bufSize() != iqBufRoundSize )
                    {
                        MYINFO << "setting buffer " << bufIdx << " size to " << iqBufRoundSize;
                        buf->setBufSize( iqBufRoundSize );
                        MY_ASSERT(iqBufRoundSize % BUFFER_BLOCKSIZE == 0)
                    }

                    tsGen = as->currentUTCdateTime();
                    QString tsStr = tsGen.toString( "dd/MM/yyyy;hh:mm:ss.zzz");
                    buf->setTimeStamp( tsStr );
                    if(dev != nullptr && stopThread == false)
                    {
                        //dongle server
                        int streamFlags = 0;
                        int stillToRead = iqBufRoundSize;
                        int readBytes = 0;
                        qint64 timeNs = 0;    //not used
                        void* vptr[] = { buf->voidPtr() };
                        MYDEBUG << "*vptr =" << *vptr << " sizeof(liquid_int16_complex)=" << sizeof(liquid_int16_complex);

                        do
                        {
                            usleep(tmid) ;
                            MYDEBUG << "about to read " << stillToRead << " bytes, stopThread=" << stopThread;
                            retVal = dev->readStream(stream,
                                                     vptr,
                                                     static_cast<size_t>(stillToRead),
                                                     streamFlags,
                                                     lastTimeNs,
                                                     1e6);


                            if(retVal >= 0)
                            {
                                stillToRead -= retVal;
                                readBytes += retVal;
                                if(stillToRead > 0)
                                {
                                    liquid_int16_complex* ptr = static_cast<liquid_int16_complex*>(vptr[0]);
                                    MYDEBUG << "ptr before=" << ptr;
                                    ptr += retVal;
                                    MYDEBUG << "ptr after=" << ptr;
                                    vptr[0] = static_cast<void*>(ptr);
                                    MYINFO << "readStream() returned " << retVal << "/"
                                            << iqBufRoundSize << " bytes, total " << readBytes << " still " << stillToRead << " pending.";
                                    MYDEBUG << "streamFlags=" << MY_HEX << streamFlags;  //0x04=SOAPY_SDR_HAS_TIME
                                }
                                else
                                {
                                    MYDEBUG << "OK, buffer ready for FFT, data buffer len=" << iqBufRoundSize;
                                    MYDEBUG << "streamFlags=" << MY_HEX << streamFlags;  //0x20=SOAPY_SDR_MORE_FRAGMENTS
                                    if(streamFlags & SOAPY_SDR_MORE_FRAGMENTS)
                                    {
                                        //what to do in this case? nothing for now
                                        fragCounter++;
                                        MYDEBUG << "fragment " << fragCounter << " timeNs=" << lastTimeNs;

                                    }
                                    else
                                    {
                                        MYDEBUG << "ok after " << fragCounter+1 << " fragments, iqBuf size=" << iqBufRoundSize << " timeNs=" << lastTimeNs;
                                        fragCounter = 0;
                                    }
                                }
                                tsGen = as->currentUTCdateTime();
                                QString s = tsGen.toString( "dd/MM/yyyy;hh:mm:ss.zzz" );
                                if (lastTimeNs <= timeNs && (streamFlags & SOAPY_SDR_HAS_TIME))
                                {
                                    //underflow detected
                                    MYINFO << "data out of time sequence while filling buffer: " << bufIdx << " timestamp: " << s;
                                    MYINFO << "retVal=" << retVal << " timeNs=" << timeNs << " lastTimeNs=" << lastTimeNs;
                                    pb->discard(bufIdx);
                                    bufIdx = -1;
                                }
                                else
                                {
                                    timeNs = lastTimeNs;

                                    if(false /*streamFlags & SOAPY_SDR_HAS_TIME*/)
                                    {
                                        //FIXME: time is zero
                                        QTime fromMidnight = QTime::fromMSecsSinceStartOfDay( lastTimeNs/1000 );
                                        tsGen = QDateTime(QDate::currentDate(), fromMidnight, Qt::UTC);
                                    }
                                    else
                                    {
                                        tsGen = as->currentUTCdateTime();
                                    }

                                    s = tsGen.toString( "dd/MM/yyyy;hh:mm:ss.zzz");
                                    buf->setTimeStamp( s );
                                    MYDEBUG << "retVal=" << retVal << " timestamp=" << s << " timeNs=" << timeNs;
                                }
                            }
                        }
                        while (retVal > 0 && stillToRead > 0 && stopThread == false);


                        if(retVal < 0)
                        {
                            sysFaultCount++;
                            MYWARNING << "readStream() returned " << retVal
                                      << " (" << SoapySDR_errToStr(retVal)
                                      << ") flags:" << streamFlags
                                      << " progressive: " << sysFaultCount;


                            pb->discard(bufIdx);
                            bufIdx = -1;
                        }
                        else
                        {


                            if(serverSocket != nullptr && clients.count() > 0)
                            {
                                shareSamples(buf);
                            }

                            if(iqRecordOn)
                            {
                                //for debug only: recording IQ buffers to a file
                                //each record starts with magic and progressive counter
                                //then the complete buffer follows
                                ds << magic;
                                ds << count;
                                ds.writeRawData( reinterpret_cast<char*>( buf->data16ptr()), buf->bufByteSize() );
                                ds << buf->data16ptr();
                                count++;
                            }

                            if(as->isDumbMode())
                            {
                                //in dumb mode, the samples are sent to external clients only
                                //no captures are performed locally
                                pb->discard(bufIdx);
                                bufIdx = -1;
                            }
                            else
                            {
                                //forwards the samples to the Radio class for events processing
                                MYDEBUG << "forwarding buffer " << bufIdx << " timestamp:" << buf->timeStamp();

                                pb->forward(bufIdx);
                                forwardedBuffersCount++;
                                sysFaultCount = 0;
                            }
                        } // if(retVal <= 0)
                    }
                }
            } // if(bufIdx != -1)
            else
            {
                droppedSamples++;
            }

            if(retVal == SOAPY_SDR_OVERFLOW)
            {

                if(tmid > (tmin + STEP_US))
                {
                    tmid -= STEP_US;
                    MYDEBUG << "Overflow, Decreasing tmid, " << tmax << " > " << tmid << " > " << tmin << " uS, latency=" << latency;
                }

                //too slow reading
                overflows++;
            }
            else if(latency > (2*tmid))
            {
                if((tmid + STEP_US) < tmax)
                {
                    tmid += STEP_US;
                    MYDEBUG << "latency=" << latency << " Increasing tmid, " << tmax << " > " << tmid << " > " << tmin << " uS";
                }
                
                //too fast reading
                underflows++;
            }

            if(sysFaultCount >= MAX_SYS_ERRORS)
            {
                break;
            }

            QThread::yieldCurrentThread();



        }  //forever

        stopThread = false;

        //deactivate and close stream
        if(stream != nullptr)
        {
            MYINFO << "deactivating stream";
            retVal = dev->deactivateStream(stream);
            if (retVal < 0)
            {
                MYINFO << "deactivating stream, returned code " << retVal << " : " << SoapySDR_errToStr(retVal);
                sysFaultCount++;
                emit fault(retVal);
            }
            MYINFO << "deactivated";
        }
    }
    else
    {
        //activateStream() failure
        emit fault(retVal);
    }

    if(sysFaultCount >= MAX_SYS_ERRORS)
    {
        emit fault(retVal);
    }

    if(stream != nullptr)
    {
        dev->closeStream(stream);
        stream = nullptr;
        MYINFO << "stream closed";
        pb->dump();
    }

    //releasing pending buffers
    if (bufIdx != -1)
    {
        MYWARNING << "discard pending buffer " << bufIdx;
        pb->discard(bufIdx);
        bufIdx = -1;
        pb->flush();
    }
    pb->unregisterReleaseCallback();
    pb->clear();
}

/*
 * detects acquisition faults not reported by Soapy
 */
bool SyncRx::checkForScanFrozen(IQbuf* toCheck)
{
    bool isBad = true;
    int quarter = iqBufRoundSize / 4;
    int num = 0;

    if(dataFormat == SOAPY_SDR_CS16)
    {
        liquid_int16_complex temp16;
        temp16.real = 0;
        temp16.imag = 0;
        for(num = 0; num < iqBufRoundSize; num += quarter)
        {
            temp16.real ^= toCheck->getReal16(num);
            temp16.imag ^= toCheck->getImag16(num);
        }

        if(temp16.real != check16.real || temp16.imag != check16.imag)
        {
            //good scan, it's not a clone of the latest one read
            isBad = false;
        }
    }
    else if(dataFormat == SOAPY_SDR_CS8)
    {
        liquid_int8_complex temp8;
        temp8.real = 0;
        temp8.imag = 0;
        for(num = 0; num < iqBufRoundSize; num += quarter)
        {
            temp8.real ^= toCheck->getReal8(num);
            temp8.imag ^= toCheck->getImag8(num);
        }

        if(temp8.real != check8.real || temp8.imag != check8.imag)
        {
            //good scan, it's not a clone of the latest one read
            isBad = false;
        }
    }

    return isBad;
}

bool SyncRx::isDeviceOpen()
{
    if(dev != nullptr)
    {
        return true;
    }
    return false;
}


bool SyncRx::getStats( StatInfos* stats )
{
    stats->ss.droppedSamples = droppedSamples;
    stats->ss.timeAcq = timeAcq;
    stats->ss.avgTimeAcq = avgTimeAcq;
    stats->ss.overflows = overflows;
    stats->ss.underflows = underflows;
    return true;
}


void SyncRx::slotParamsChange()
{
    MYINFO << __func__ << "()";

    if(ready == false)
    {
        dsBw = as->getDsBandwidth();
        if( isAudioDevice )
        {
            //the band from 0 to dsBw/2 are negative frequencies not displayed
            lowBound   = dsBw/2;
            highBound  = dsBw;
            MYINFO << "Subsampled band positive frequencies from" << lowBound << " to " << highBound << " Hz, full dsBw =" << dsBw << " Hz";
        }
        else
        {
            lowBound   =  as->getTune() - (dsBw / 2);
            highBound  =  as->getTune() + (dsBw / 2);
            MYINFO << "Subsampled band from" << lowBound << " to " << highBound << " Hz, dsBw=" << dsBw << " Hz";
        }


        emit status( tr("Ready.") );

        //end of parameter change sequence
        ready = true;
    }
}

bool SyncRx::slotSetTune()
{
    MYINFO << __func__ << "()";

    int fails = 0;

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }
    slotSetFreqRange();
    qint64 newTune = as->getTune();

    if(newTune != tune && !as->isAudioDevice())
    {
        if(isRunning() == true)
        {
            if(pb != nullptr)
            {
                //flushes the unprocessed iq buffers
                pb->flush();
            }
        }

        if(newTune < minTune || newTune > maxTune)
        {
            MYINFO << "Changing tuning frequency to fit in current range " << frRange;
            MYINFO << "From " << newTune << " to " << maxTune;
            newTune = static_cast<int>(maxTune);
        }

        //note: the tune is ineffective with audio devices. It is set as SR/2 fixed
        //just to avoid the Radio class to deal with negative frequencies.

        ready = false;
        dev->setFrequency( SOAPY_SDR_RX, 0,  static_cast<double>( newTune ) );
        tune = static_cast<int>( dev->getFrequency(SOAPY_SDR_RX, channel) );
        if( tune != newTune )
        {
            MYCRITICAL << "Tuning failed, set " << newTune << " got " << tune << " instead.";
            emit status( tr("Failed setting tuner frequency."), AST_ERROR );
            fails++;
        }
        as->setTune( static_cast<int>(tune), false);

    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return (fails == 0);
}

bool SyncRx::slotSetSampleRate()
{   
    MYINFO << __func__ << "()";

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    int fails = 0;
    int retVal = 1;

    if(as->getSampleRate() != sr)
    {
        retVal = 0;
        bool ok = false;
        QString newSr;
        newSr.setNum( as->getSampleRate() );

        if( as->isIntValueInList(srRanges, newSr) < 0 )
        {
            srRange = srRanges.last();
            QString dflt = srRange.split(":").last();
            MYWARNING << "Sample rate " << newSr << " not supported, defaulting to: " << dflt;
            newSr = dflt;
        }

        ready = false;
        double dSr = newSr.toDouble(&ok);
        MY_ASSERT(ok == true)

        MYINFO << "Sample rate to set: " << dSr;
        dev->setSampleRate( SOAPY_SDR_RX, channel, dSr );
        MY_ASSERT(ok == true)
        sr = static_cast<int>( dev->getSampleRate( SOAPY_SDR_RX, channel ) );
        if( sr == newSr.toInt(&ok) )
        {
            MY_ASSERT(ok == true)
            as->setSampleRate(sr);
            MYINFO << "Sample rate set to " << sr << " Hz";

        }
        else
        {
            MY_ASSERT(ok == true)
            MYCRITICAL << "Failed setting sample rate to " << newSr << " Hz, dev->getSampleRate() returned: " << sr;
            emit status( tr("Failed setting sample rate."), AST_ERROR );
            fails++;
        }

        if(ready == false)
        {
            slotParamsChange();
        }
    }
    return (retVal >= 0);
}

bool SyncRx::slotSetGain()
{
    MYINFO << __func__ << "()";

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    int fails = 0;
    ready = false;
    gain = as->getGain();
    if( as->fpCmpf( static_cast<float>(gain), AUTO_GAIN, 0 ) == 0 )
    {
        //automatic gain
        dev->setGainMode( SOAPY_SDR_RX, channel, true );
        if( dev->getGainMode( SOAPY_SDR_RX, channel ) == false )
        {
            MYCRITICAL << "Failed setting automatic tuner gain " ;
            emit status( tr("Failed setting automatic tuner gain."), AST_ERROR );
            fails++;
        }
    }
    else
    {
        //manual gain

        //Specify the name of the gain to modify: AUDIO for audio device, TUNER for all others.
        //For SDRs equipped with multiple gain settings (IF, RF..) the set value is equally distributed by Soapy

        dev->setGainMode( SOAPY_SDR_RX, channel, false );
        if( dev->getGainMode( SOAPY_SDR_RX, channel ) != false )
        {
            MYCRITICAL << "Failed setting manual tuner gain " ;
            emit status( tr("Failed setting manual tuner gain."), AST_ERROR );
            fails++;
        }
        else
        {
            if(offsetGain == MIN_DBFS)
            {
                //measure the offset gain
                //After some tests with RSP1 it seems there is an offset between
                //the gain set and the gain read back. This offset is measured here
                //and subtracted at each further read back.

                dev->setGain( SOAPY_SDR_RX, channel, 0.0 );
                offsetGain = static_cast<double>( dev->getGain(SOAPY_SDR_RX, channel) );
                MYINFO << "Measured gain offset: " << offsetGain;
            }


            if( gain < 0 )         // >= 0 %
            {
                gain = 0;
                as->setGain( gain );
            }

            if( gain > 100 )      // <= 100%
            {
                gain = 100;
                as->setGain( gain );
            }

            QString gainName = (isAudioDevice == false)? "TUNER" : "AUDIO";
            double dbGain = gainRange.minimum() + (gain / (100.0 / (gainRange.maximum()-gainRange.minimum())));
            MYINFO << "Setting tuner gain to " << gain << "% == " << dbGain << " dB" ;


            //dev->setGain( SOAPY_SDR_RX, channel, gainName.toStdString(), dbGain );
            dev->setGain( SOAPY_SDR_RX, channel, dbGain );


            //dbGain = static_cast<double>( dev->getGain(SOAPY_SDR_RX, channel, gainName.toStdString()) );
            dbGain = static_cast<double>( dev->getGain(SOAPY_SDR_RX, channel) );
            MYINFO << "Read back gain: " << dbGain << " dB" ;
            dbGain = dbGain - offsetGain;
            MYINFO << "Compensated gain now: " << dbGain << " dB" ;
            if ( dbGain >= gainRange.minimum() && dbGain <= gainRange.maximum() )
            {
                as->setGain(gain);
            }
            else
            {
                MYWARNING << "Failed setting gain, got " << dbGain << " dbGain instead." ;
                emit status( tr("Failed setting manual tuner gain."), AST_ERROR );
                fails++;
            }
        }
    }

    if(ready == false)
    {
        slotParamsChange();
    }

    return (fails == 0);
}

bool SyncRx::slotSetError()
{
    MYINFO << __func__ << "()";

    int fails = 0;
    float err = 0.0F;

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    //the integer part of the ppm error is used to set the frequency correction into the dongle
    //if supported

    if( dev->hasFrequencyCorrection( SOAPY_SDR_RX, channel ) )
    {
        ready = false;
        hasFreqErrorCorr = true;
        float newErr = as->getError();

        MYINFO << "Setting tuning error to " << newErr << " ppm";
        dev->setFrequencyCorrection( SOAPY_SDR_RX, channel, newErr );
        MYINFO << "Set done, reading it back...";
        err =  static_cast<float>( dev->getFrequencyCorrection( SOAPY_SDR_RX, channel ) );

        if ( as->fpCmpf(err, newErr, 0) == 0 )
        {
            MYINFO << "Tuning error set to " << newErr << " ppm";
            as->setError(err);
        }
        else
        {
            MYCRITICAL << "Failed setting tuning error to " << newErr
                << " ppm, getFrequencyCorrection() returned: " << err;
            emit status( tr("Failed setting tuning error ppm."), AST_ERROR );
            fails++;
        }
    }

    if(ready == false)
    {
        slotParamsChange();
    }
    return (fails == 0);
}

bool SyncRx::slotSetDsBandwidth()
{
    //note: here is set the downsampler output bandwidth
    //while the bandwidth filter is set with slotSetBandwidth()
    //Soapy is not involved here
    MYINFO << __func__ << "()";

    int newBw = as->getDsBandwidth();
    MYINFO << "newBw=" << newBw;
    if( dsBw != newBw && newBw <= bw )
    {
        ready = false;
        dsBw = newBw;
        slotSetFreqRange();
        if(isAudioDevice)
        {
           tune = 0;
           as->setTune(tune);
           MYINFO << "audio device, retuning to " << tune;
        }
    }
    if(ready == false)
    {
        slotParamsChange();
    }
    return true;
}


bool SyncRx::slotSetAntenna()
{
    MYINFO << __func__ << "()";

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    int fails = 0;

    QString newAnt;
    newAnt = as->getAntenna();

    if( !antennas.contains( newAnt ))
    {
        MYWARNING << "Antenna " << newAnt << " not supported, defaulting to: " << antennas.first();
        newAnt = antennas.first();
    }

    ready = false;

    dev->setAntenna( SOAPY_SDR_RX, channel, newAnt.toStdString() );
    antenna = QString( dev->getAntenna( SOAPY_SDR_RX, channel ).c_str() );

    if( antenna == newAnt )
    {
        as->setAntenna(antenna);
        MYINFO << "Antenna set to " << antenna;
    }
    else
    {
        MYCRITICAL << "Failed setting antenna " << newAnt << " Hz, dev->getAntenna() returned: " << antenna;
        emit status( tr("Failed setting antenna."), AST_ERROR );
        fails++;
    }

    if(ready == false)
    {
        slotParamsChange();
    }

    return (fails == 0);
}

bool SyncRx::slotSetBandwidth()
{
    MYINFO << __func__ << "()";
    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    //for audio devices, bw = SR
    int fails = 0;

    //currently, bandwidths are not supported by SoapyRTLSDR
    if(!isAudioDevice && !deviceName.contains("RTL"))
    {
        bool ok = false;
        QLocale loc;

        int bandWidth = as->getBandwidth();
        QString bbr = as->getBandwidthRange();
        int min=0, max=0;
        if (bbr.contains(':'))
        {
            //range expressed as interval - from_value : to_value
            QStringList sl = bbr.split(':');
            max = loc.toInt(sl.at(1), &ok);
            MY_ASSERT(ok == true)
            min = loc.toInt(sl.at(0), &ok);
            MY_ASSERT(ok == true)
        }
        else
        {
            //range expressed as discrete value
            max = loc.toInt(bbr, &ok);
            ok = (bandWidth <=max);
        }

        if(bandWidth > max)
        {
            //fixes the value to stay into the range
            //the fix won't be saved in settings
            bandWidth = max;
        }

        if (ok)
        {
            dev->setBandwidth( SOAPY_SDR_RX, channel, bandWidth );
            bw = static_cast<int>( dev->getBandwidth( SOAPY_SDR_RX, channel ) );
            MYINFO << "bandwidth read back: " << bw;
            if( bandWidth != bw )
            {
                MYWARNING << "This device doesn't implement band filters";
            }
            else
            {
                MYINFO << "bandwidth " << bandWidth << " fits in range " << bbr << " Hz as " << bw;
                ready = false;
                slotParamsChange();
            }
        }
        else
        {
            fails++;
            MYWARNING << "The required bandwidth " << bandWidth << " doesn't fit the required range " << bbr;
        }
    }

    return (fails == 0);
}

bool SyncRx::slotSetFreqRange()
{
    MYINFO << __func__ << "()";

    if(dev == nullptr)
    {
        MYINFO << "device not opened, doing nothing" ;
        return false;
    }

    int fails = 0;
    QString newRange;
    newRange = as->getFreqRange();
    if( !frRanges.contains( newRange ))
    {
        MYWARNING << "Range " << newRange << " not supported, defaulting to: " << frRanges.first();
        newRange = frRanges.first();
    }

    ready = false;
    frRange = newRange;

    bool ok = false;
    QStringList limits = frRange.split(':');
    MYINFO << "limits: " << limits;
    QString s =  limits.first().trimmed();
    minTune = s.toLongLong(&ok) * 1000; //kHz to Hz
    MY_ASSERT(ok == true)
    s =  limits.last().trimmed();
    maxTune = s.toLongLong(&ok) * 1000;
    MY_ASSERT(ok == true)
    /*
    if(isAudioDevice)
    {
        as->setTune(0);
    }
    */

    MYINFO << "tune=" << tune << " minTune=" << minTune << " maxTune=" << maxTune;
    if(tune != as->getTune())
    {
        MYINFO << "getTune=" << as->getTune();
        if(isRunning() == true)
        {
            if(pb != nullptr)
            {
                //flushes the unprocessed iq buffers
                pb->flush();
            }
        }

        int64_t newTune  = as->getTune();
        if(newTune < minTune || newTune > maxTune)
        {
            MYINFO << "Changing tuning " << newTune;
            newTune = (maxTune-minTune)/2;
            MYINFO << " to " << newTune;
            MYINFO << " to fit in " << newRange;

            MYINFO << "changing tune " << tune << " to " << newTune << " hz";
        }
        else
        {
            tune = newTune;
        }

        /*
         * WARNING: exceptions can arise in case of tuning in a device's tuning hole,
         * i.e. E4000 devices have one between 975 and 1310 MHz. Soapy seems to ignore this
         * so only one frequency range is discovered.
         */
        dev->setFrequency( SOAPY_SDR_RX, 0,  static_cast<double>( newTune ) );
        tune = static_cast<int>( dev->getFrequency(SOAPY_SDR_RX, channel) );
        if(tune == newTune)
        {
            MYINFO << "Device correctly tuned to " << tune << " Hz";
            as->setTune( static_cast<int>(tune) );
            slotSetTune();
        }
        else
        {
            MYCRITICAL << "Tuning failed, set " << newTune << " got " << tune << " instead.";
            emit status( tr("Failed adjusting tuner frequency."), AST_ERROR );
            fails++;
        }
    }

    if(ready == false)
    {
        slotParamsChange();
    }

    return (fails == 0);
}


//------------

bool SyncRx::startServer()
{
    if(serverSocket != nullptr)
    {
        serverSocket->close();
        delete serverSocket;
        serverSocket = nullptr;
    }
    serverSocket = new QUdpSocket();
    bool result = serverSocket->bind(static_cast<quint16>(as->getServerPort()));
    MY_ASSERT( nullptr !=  serverSocket)
    if(result)
    {
        MYINFO << "### Ready to accept UDP connections on port " << as->getServerPort() << " state:" << serverSocket->state();
    }
    else
    {
        MYCRITICAL << "### Failed listening for UDP connections on port " << as->getServerPort() << ":" << serverSocket->errorString();
    }
    return result;
}

void SyncRx::readPendingClientDatagrams()
{
    qint64 result = 0;
    //MYDEBUG << "### Socket state: " << serverSocket->state();
    if(serverSocket->state() == QAbstractSocket::BoundState)
    {
        //MYDEBUG << "### Checking for clients requests...";
        while (serverSocket->hasPendingDatagrams())
        {
            qint64 size = serverSocket->pendingDatagramSize();
            if(size <= 0)
            {
                MYWARNING << "pendingDatagramSize <= 0 (QTBUG-49301) socket badly closed on client - "
                    << serverSocket->errorString();
                requestInterruption();
                break;
            }

            ClientDescriptor* candidateClient = new ClientDescriptor;
            QByteArray datagram(static_cast<int>(size),0);
            try
            {
                result = serverSocket->readDatagram(datagram.data(),
                                     static_cast<qint64>(datagram.size()),
                                     &candidateClient->address, &candidateClient->port);

                if(result < 0)
                {
                    MYCRITICAL << "### Failed reading " << datagram.size() << " bytes : " << serverSocket->errorString();
                    return;
                }

                ClientDescriptor* c = nullptr;

                //Expected data from clients are always in form
                //<client name>.<payload> for instance "perseids.START"
                //Unknown clients must annunce themselves first by sending
                //<client name>.<client name> for instance "perseids.perseids"

                QStringList data = QString( datagram.simplified() ).split(".");
                if(data.count() != 2)
                {
                    MYINFO << "Received and discarded unexpected datagram";
                    continue;
                }


                QString clientName = data.at(0);

                bool bad = false;
                for (const QChar &c : clientName)
                {
                    if (!c.isLetterOrNumber())
                    {
                         bad = true;
                         break;
                    }
                }

                if(bad)
                {
                    MYINFO << "Received and discarded datagram, clientName must contain only alphanumeric chars: " << clientName;
                    continue;
                }

                if(clientName.length() > MAX_NAMES_SIZE)
                {
                    MYINFO << "Received and discarded datagram, clientName is too long: " << clientName;
                    continue;
                }

                QString clientData = data.at(1);

                //checks if the client is already in distribution list
                bool alreadyKnown = false;
                foreach(c, clients)
                {
                    if(c->name == clientName)
                    {
                        MYINFO << "### Client " << c->name << " sent something";
                        c->address = candidateClient->address;
                        c->port = candidateClient->port;
                        alreadyKnown = true;
                    }
                }

                if(alreadyKnown == true)
                {
                    if(clientData.contains("QUIT"))
                    {
                        //the client requires disconnection
                        MYINFO << "### Releasing client " << c->name;
                        clients.removeAll(c);
                    }
                    else if(clientData.contains("START"))
                    {
                        MYINFO << "### Start sending samples to client " << c->name;
                        c->active = true;
                        c->timeout = KEEPALIVE_TOUT;
                    }
                    else if(clientData.contains("STOP"))
                    {
                        MYINFO << "### Stop sending samples to client " << c->name;
                        c->active = false;
                    }
                    else if(clientData.contains("ALIVE"))
                    {
                        //keepalive timeout retrigger
                        MYINFO << "### client " << c->name << " is alive";
                        c->timeout = KEEPALIVE_TOUT;
                    }

                    delete candidateClient;
                    candidateClient = nullptr;
                }

                if(clientData == clientName)//datagram contains a new client name
                {
                    //new client to be served
                    //the scan flux is closed by default
                    if(alreadyKnown == false)
                    {
                        candidateClient->name = clientName;
                        candidateClient->active = false;
                        clients.append(candidateClient);
                        MYINFO << "### Accepting UDP connection from new client " << candidateClient->name
                               << " at " << candidateClient->address.toString() << " : " << candidateClient->port;

                        //sends always the client the device configuration back to client
                        sendDeviceConfig(candidateClient);
                    }
                    else
                    {
                        //sends always the client the device configuration back to client
                        sendDeviceConfig(c);
                    }

                }


            }
            catch ( QException& e )
            {
                MYCRITICAL << "Exception " << e.what();
            }
        }
    }
}

void SyncRx::shareDeviceConfig()
{
    MYINFO << __func__;
    ClientDescriptor* client = nullptr;
    try
    {
        foreach(client, clients)
        {
            sendDeviceConfig(client);
        }
    }
    catch ( QException& e )
    {
        MYCRITICAL << "Exception " << e.what();
    }
}

void SyncRx::shareSamples(IQbuf *buf)
{
    MYDEBUG << __func__;
    sentBuffersCount++;
    int datagramDelayMs = as->getDatagramDelay();

    ClientDescriptor* client = nullptr;
    QByteArray baIQbuf; //serialized IQbuf to be sent via UDP socket, to be broken down in datagrams, BUFFER_BLOCKSIZE bytes max each.
    QByteArray baDgram; //single datagram
    QByteArray baHeader; //serialized IQbuf header

    //preparing header
    QDataStream hdr(&baHeader, QIODevice::WriteOnly);
    qint16 syncWord = SYNC_IQBUF;

    hdr << syncWord;
    hdr << buf->myUID();
    hdr << buf->timeStamp();

    //preparing serialized IQbuf
    QDataStream ds(&baIQbuf, QIODevice::WriteOnly);
    if(dataFormat == SOAPY_SDR_CS16)
    {
        ds.writeRawData( reinterpret_cast<char*>( buf->data16ptr() ), buf->bufByteSize() );
    }
    else if(dataFormat == SOAPY_SDR_CS8)
    {
        ds.writeRawData( reinterpret_cast<char*>( buf->data8ptr() ), buf->bufByteSize() );
    }

    try
    {

        foreach(client, clients)
        {
            if(client->active == true)
            {
                client->timeout--;
                if(client->timeout <= 0)
                {
                    MYWARNING << "client " << client->address << " timeout";
                    stopClient(client);
                }

                //Sending header
                qint64 sent = serverSocket->writeDatagram(baHeader, client->address, client->port);
                if(sent != baHeader.size())
                {
                    MYCRITICAL << "Sending header to " << client->name << " (" << client->address.toString() << "/" << client->port << ")" << MY_ENDL
                               << " not complete, sent " << sent << " bytes instead of " << baHeader.size() << " bytes "
                               << MY_ENDL  << " error: " << serverSocket->errorString();
                    break;
                }

                //sending data samples
                int byteIndex = 0, sliceIndex = 0;
                int sliceSize = (baIQbuf.size() > MAX_DGRAM_SIZE)? MAX_DGRAM_SIZE : baIQbuf.size();
                MYDEBUG << "baIQbuf.size=" << baIQbuf.size() << " sliceSize=" << sliceSize;
                while(byteIndex < baIQbuf.size())
                {
                    baDgram = baIQbuf.mid(byteIndex, sliceSize);
                    usleep(datagramDelayMs);
                    sent = serverSocket->writeDatagram(baDgram, client->address, client->port);
                    if(sent < 0)
                    {
                        MYWARNING << "Sending to " << client->name << " (" << client->address.toString() << "/" << client->port << ")" << MY_ENDL
                                   << " not complete, sent " << sent << " bytes instead of " << baDgram.size()
                                   << MY_ENDL  << " error: " << serverSocket->errorString();

                        MYINFO << "Resending slice#" << sliceIndex << ", byteIndex = " << byteIndex;
                    }
                    else
                    {
                        //prepare to send the next slice
                        MYDEBUG << "Sent slice#" << sliceIndex;
                        byteIndex += baDgram.size();
                        sliceIndex++;
                    }

                }
            }
        }

        MYDEBUG << "Sent buffer #" << sentBuffersCount;
    }
    catch ( QException& e )
    {
        MYCRITICAL << "Exception " << e.what();
    }
}
void SyncRx::stopAllClients()
{
    MYINFO << __func__;
    QByteArray baStop;
    baStop.append("STOP");

    ClientDescriptor* client = nullptr;
    try
    {
        //notifies all the connected clients that the server is terminating
        foreach(client, clients)
        {
            stopClient(client);
        }
    }
    catch ( QException& e )
    {
        MYCRITICAL << "Exception " << e.what();
    }
}


void SyncRx::stopClient( ClientDescriptor* client )
{
    MYINFO << __func__;
    QByteArray baStop;
    baStop.append("STOP");

    MY_ASSERT(client != nullptr)

    try
    {
        serverSocket->writeDatagram(baStop,  client->address, client->port);
        MYINFO << "Sending STOP to client " << client->address.toString() << ":" << client->port;
        client->active = false;
    }
    catch ( QException& e )
    {
        MYCRITICAL << "Exception " << e.what();
    }
}



void SyncRx::sendDeviceConfig(ClientDescriptor* client)
{
    qint64 sent = 0;
    int sizeofDoublet = (dataFormat == SOAPY_SDR_CS16) ? sizeof(liquid_int16_complex) : sizeof(liquid_int8_complex);

    QByteArray baMyConfig;
    QDataStream cfg(&baMyConfig, QIODevice::WriteOnly);
    qint16 syncWord = SYNC_SRVCONF;
    cfg << syncWord;

    cfg << as->getDevice();             MYINFO << "srvDeviceName=" << as->getDevice();
    cfg << as->getSampleRateRange();    MYINFO << "       srvSRR=" << as->getSampleRateRange();
    cfg << as->getBandwidthRange();     MYINFO << "       srvBBR=" << as->getBandwidthRange();
    cfg << as->getFreqRange();          MYINFO << "        srvFR=" << as->getFreqRange();
    cfg << as->getAntenna();            MYINFO << "   srvAntenna=" << as->getAntenna();
    cfg << as->getSampleRate();         MYINFO << "srvSampleRate=" << as->getSampleRate();
    cfg << as->getBandwidth();          MYINFO << " srvBandwidth=" << as->getBandwidth();
    cfg << as->getGain();               MYINFO << "      srvGain=" << as->getGain();
    cfg << as->getError();              MYINFO << "  srvPpmError=" << as->getError();
    cfg << as->getTune();               MYINFO << "      srvTune=" << as->getTune();
    cfg << isAudioDevice;               MYINFO << "isAudioDevice=" << isAudioDevice;
    cfg << as->getDsBandwidth();        MYINFO << "srvDsBandwidth=" << as->getDsBandwidth();
    cfg << iqBufRoundSize;              MYINFO << "    iqBufSize=" << iqBufRoundSize;
    cfg << sizeofDoublet;               MYINFO << "sizeofDoublet=" << sizeofDoublet;
    cfg << rxStreamMTU;                 MYINFO << "rxStreamMTU=" << rxStreamMTU;
    MYINFO << "sizeof CFG: " << baMyConfig.size();


    MYINFO << "### Sending device config to " << client->address.toString() << ":" << client->port;

    sent = serverSocket->writeDatagram(baMyConfig,  client->address, client->port);

    if(sent != baMyConfig.size())
    {
        MYCRITICAL << "### Sending to " << client->address.toString() << " not complete, sent (=" << sent
                   << ") bytes < sizeof(dc)(=" << baMyConfig.size() << ") bytes";
    }
}


void SyncRx::DCspikeRemoval(bool status)
{
    //If possible, enables the automatic IQ balance mode
    //and DC offset mode to remove the DC component.
    //This method should be called once the dBfs scale
    //has been calibrated

    if ( dev->hasDCOffsetMode( SOAPY_SDR_RX, channel ) )
    {
        dev->setDCOffsetMode( SOAPY_SDR_RX, channel, status );
    }

    if ( dev->hasIQBalanceMode( SOAPY_SDR_RX, channel ) )
    {
        dev->setIQBalanceMode( SOAPY_SDR_RX, channel, status );
    }

    if(status)
    {
        dev->writeSetting("offset_tune", "true");
    }
    else
    {
        dev->writeSetting("offset_tune", "false");
    }
}

