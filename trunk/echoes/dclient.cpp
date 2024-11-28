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
$Id: dclient.cpp 268 2018-07-19 05:45:48Z gmbertani $
*******************************************************************************/

#include "setup.h"

#include "dclient.h"



int DongleClient::clearUDPinputSocketStatic(QUdpSocket* socket)
{
    int recv = MAX_DGRAM_SIZE;
    int wastedBytes = 0;
    QHostAddress sender;
    quint16 senderPort = 0;
    MY_ASSERT( nullptr !=  socket)
    QByteArray baDgram(MAX_DGRAM_SIZE, 0);
    while(recv == MAX_DGRAM_SIZE)
    {
        recv = static_cast<int>( socket->readDatagram(baDgram.data(), baDgram.size(),
                        &sender, &senderPort) );
        if (recv > 0)
        {
            wastedBytes += recv;
        }
        else
        {
            wastedBytes = recv;
        }
    }
    MYINFO << "Cleared UDP input buffer, wasted: " << wastedBytes << " bytes";
    return wastedBytes;
}


bool DongleClient::lookForServerStatic(Settings* appSettings, QList<DeviceMap>* devices)
{
    MYINFO << __func__;

    MY_ASSERT( nullptr !=  devices)
    MY_ASSERT( nullptr !=  appSettings)

    bool found = false;
    QString netDev;
    QHostAddress serverAddr = appSettings->getServerAddress();
    quint16 serverPort = static_cast<quint16>( appSettings->getServerPort() );
    if( serverAddr.toIPv4Address() != 0 && serverPort != 0 )
    {
        int idx = 0;
        if(devices->count() > 0)
        {
            idx = devices->last().index + 1;
        }

        DeviceMap dm;

        for(int j = 0; j < devices->count(); j++)
        {
            dm = devices->at(j);
            if(dm.name.contains("UDP"))
            {
                //dongle server already scanned
                netDev = QString("%1:UDP server %2:%3").arg(idx).arg(serverAddr.toString()).arg(serverPort);
                MYINFO << "### Updated dongle server: " << netDev;
                dm.name = netDev;
                devices->replace(j, dm);
                found = true;
            }
        }

        if(found == false)
        {

            //checks if a dongle server has been defined
            netDev = QString("%1:UDP server %2:%3").arg(idx).arg(serverAddr.toString()).arg(serverPort);
            MYINFO << "### Looking for dongle server at " << netDev;

            QByteArray baMyConfigName, baCommand;
            qint64 sent = 0;

            baMyConfigName.append( appSettings->getConfigName().toUtf8() );
            baMyConfigName.append( "." );
            baMyConfigName.append( appSettings->getConfigName().toUtf8() );
            QUdpSocket* socket = new QUdpSocket();
            MY_ASSERT( nullptr !=  socket)

            //stops eventual incoming data flux from server previously activated
            //to avoid flooding
            baCommand.append( appSettings->getConfigName().toUtf8() );
            baCommand.append( "." );
            baCommand.append( "STOP" );
            sent = socket->writeDatagram(baCommand, serverAddr, serverPort);
            if ( clearUDPinputSocketStatic(socket) < 0 )
            {
                MYWARNING << "FAILED clearing UDP input socket";
            }

            sent = socket->writeDatagram(baMyConfigName, serverAddr, serverPort);
            if(sent != baMyConfigName.size())
            {
                MYCRITICAL << "### Sending not complete, sent (=" << sent << ") bytes < baMyConfigName.size()(="
                     << baMyConfigName.size() << ") bytes : " << socket->errorString();
            }

            //waits answer from server
            int attempts = 1;
            if( socket->waitForReadyRead(10000) )
            {
                while (socket->hasPendingDatagrams())
                {
                    qint64 size = socket->pendingDatagramSize();
                    if(size <= 0)
                    {
                        //discard broken data and retrying after one second
                        socket->readAll();
                        msleep(1000);
                        attempts--;
                        if(attempts == 0)
                        {
                            break;
                        }
                        continue;
                    }

                    QByteArray baSrvConf(static_cast<int>(size), '\0');

                    socket->readDatagram(baSrvConf.data(), baSrvConf.size(),
                                         &serverAddr, &serverPort);
                    QDataStream hdr(baSrvConf);
                    qint16 syncWord = -1;
                    hdr >> syncWord;
                    if(syncWord == SYNC_SRVCONF)
                    {
                        //the configuration got is stored but not applied
                        //because this is a static function, it must be used
                        //to check if there is a server that can be connected
                        //but real connection (intended as handshake)
                        //is done by constructor.

                        found = true;

                        if(devices->count() > 0)
                        {
                            dm = devices->last();
                            dm.index++;
                        }
                        else
                        {
                            dm.index = 0;
                        }

                        dm.name = netDev;
                        devices->append(dm);
                        MYINFO << "### Server radio configuration received";
                    }
                    else
                    {
                        MYWARNING << "### Discarded data packet arrived is not a server configuration, its sync is " << syncWord
                                  << " instead of " << SYNC_SRVCONF;
                    }
                }

            }
        }
    }

    if(found == false)
    {
        MYWARNING << "### no dongle servers configured in " << appSettings->getConfigName();
    }

    return found;
}


DongleClient::DongleClient(Settings* appSettings, Pool<IQbuf*>* bPool, QThread *parentThread) :
     IQdevice(appSettings, bPool, parentThread)
{
    setObjectName("DongleClient thread");
    owner = parentThread;

    MYINFO << "parentThread=" << parentThread << " owner= " << owner << " parent()=" << parent();

    droppedSamples = 0;
    timeAcq = 0;
    recvBufferCount = 0;
    clientSocket = nullptr;
    serverConnected = false;
    tempBuf.setMaxSize( MAX_SAMPLE_BUF_SIZE );
    iqBufSize = 0;
    srvIqBufSize = 0;
    srvBandwidth = 0;
    srvDsBandwidth = 0;
    sizeofDoublet = sizeof (liquid_int16_complex);

    QHostAddress serverAddress = as->getServerAddress();
    MYINFO << "Connecting to " << serverAddress.toString();

    bool ok = false;
    quint32 numAddr = serverAddress.toIPv4Address(&ok);
    if(ok && numAddr != 0)
    {
        //this instance is a client, so it tries to connect the given
        //dongle server
        //waiting for server's configuration
        serverConnected = waitServerConfig();

        //enlarges the receive buffer to 16MB
        clientSocket->setSocketOption( QAbstractSocket::ReceiveBufferSizeSocketOption, 0x100000 );
        MYINFO << " client socket opened, ReceiveBufferSizeSocketOption = " << clientSocket->socketOption( QAbstractSocket::ReceiveBufferSizeSocketOption );

        //QString s;
        //rates.append( s.setNum( as->getSampleRate() )  );
    }

    IQbuf* iqb = nullptr;
    if( pb->size() == 0 )
    {
        for (int i = 0; i < DEFAULT_POOLS_SIZE; i++)
        {
            iqb = new IQbuf( MAX_SAMPLE_BUF_SIZE, sizeofDoublet );
            MY_ASSERT( nullptr !=  iqb)
            pb->insert(i, iqb);
        }
    }

    MYINFO << "I/Q buffer size set to " << MAX_SAMPLE_BUF_SIZE << " I/Q doublets ("
           << MAX_SAMPLE_BUF_SIZE * sizeofDoublet << " bytes)";


}

DongleClient::~DongleClient()
{
    sendCommand("QUIT");

    //destroying client
    if(clientSocket != nullptr)
    {
        MYINFO << "Client socket closed";
        serverConnected = false;
        delete clientSocket;
        clientSocket = nullptr;
    }
}


void DongleClient::run()
{
    MYINFO << "Dongle client thread started";
    int bufIdx = -1;
    IQbuf* buf = nullptr;
    stopThread = false;
    unsigned liveCount = KEEPALIVE_RATE;

    //reconnects at each acq restart
    if ( clearUDPinputSocketStatic(clientSocket) < 0 )
    {
        MYWARNING << "FAILED clearing UDP input socket";
    }

    if(pb->size() == 0)
    {

        IQbuf* iqb = nullptr;
        if( pb->size() == 0 )
        {
            for (int i = 0; i < DEFAULT_POOLS_SIZE; i++)
            {
                iqb = new IQbuf( MAX_SAMPLE_BUF_SIZE, sizeofDoublet );
                MY_ASSERT( nullptr !=  iqb)
                pb->insert(i, iqb);
            }
        }

        MYINFO << "I/Q buffer size set to " << MAX_SAMPLE_BUF_SIZE << " I/Q doublets ("
           << MAX_SAMPLE_BUF_SIZE * sizeofDoublet << " bytes)";
    }

    sendCommand("START");
    acqTimer.start();

    forever
    {

        if( isFinished() == true || isInterruptionRequested() == true || stopThread == true )
        {
            MYINFO << "DongleClient thread stopped";
            sendCommand("STOP");
            if ( clearUDPinputSocketStatic(clientSocket) < 0 )
            {
                MYWARNING << "FAILED clearing UDP input socket";
            }
            break;
        }

        if(serverConnected)
        {
            msleep(as->getInterval());
            bufIdx = pb->take();

            if(bufIdx != -1)
            {
                timeAcq = static_cast<int>(acqTimer.restart());

                buf = pb->getElem(bufIdx);
                if(buf != nullptr)
                {
                    int retVal = readSamples(buf);
                    if(retVal > 0)
                    {
                        pb->forward(bufIdx);
                    }
                    else
                    {
                        //no data to forward, frees the buffer
                        pb->discard(bufIdx);

                        if(retVal == -1)
                        {
                            //the server stopped acquisition
                            MYWARNING << "Server stopped acquisition, stopping DongleClient thread";
                            if ( clearUDPinputSocketStatic(clientSocket) < 0 )
                            {
                                MYWARNING << "FAILED clearing UDP input socket";
                            }
                            break;
                        }
                    }
                    liveCount--;
                    if(liveCount <= 0)
                    {
                        liveCount = KEEPALIVE_RATE;
                        sendCommand("ALIVE");
                    }
                }

                else
                {
                    //cannot forward data to radio
                    //due to missing free buffers
                    droppedSamples += buf->bufSize();
                }
                acqTimer.restart();
            }
        }
        else
        {
            MYWARNING << "Unrequested server disconnection, stopping DongleClient thread";
            if ( clearUDPinputSocketStatic(clientSocket) < 0 )
            {
                MYWARNING << "FAILED clearing UDP input socket";
            }
            break;
        }
    }

    //releasing pending buffers
    if(bufIdx != -1)
    {
        pb->discard(bufIdx);
        pb->flush();
    }
    pb->clear();

}


bool DongleClient::getStats( StatInfos* stats )
{
    stats->ss.droppedSamples = droppedSamples;
    stats->ss.timeAcq = timeAcq;
    return true;
}



int DongleClient::readSamples(IQbuf *buf)
{
    QMutexLocker ml(&mutex);

    QHostAddress sender;
    quint16 senderPort = 0;
    int retVal = 0;
    bool skipBuffer = false;
    int sliceSize = MAX_DGRAM_SIZE;
    int totSlices = 0;
    int sliceIndex = 0;
    QByteArray baDgram(sliceSize, '\0');
    qint64 recv = 0;

    MYDEBUG << __func__;
    try
    {
        while(retVal == 0)
        {
            if( clientSocket->hasPendingDatagrams() == false )
            {
                msleep(1);
                MYDEBUG << "no pending datagrams";
                return retVal;
            }

            if( clientSocket->pendingDatagramSize() == 0 )
            {
                msleep(1);
                MYDEBUG << "pending datagrams sized zero";
                return retVal;
            }

            baDgram.resize( static_cast<int>(MAX_DGRAM_SIZE) );

            //?? --> QSocketNotifier: Socket notifiers cannot be enabled or disabled from another thread
            recv = clientSocket->readDatagram(baDgram.data(), baDgram.size(),
                        &sender, &senderPort);

            if(recv > 0)
            {
                //datagram received
                //the datagram size matches the size expected
                if( recv == strlen("STOP") )
                {
                    //the datagram contains a stop acquisition command
                    if(baDgram.contains("STOP"))
                    {
                        MYINFO << "### The server has stopped the acquisition";
                        sliceIndex = 0;
                        tempBuf.clear();
                        retVal = -1;
                    }

                }
                else if(recv < MAX_DGRAM_SIZE)
                {
                     //must read the datagram content to understand its structure
                    QDataStream ds(baDgram);
                    qint16 syncWord = -1;
                    ds >> syncWord;

                    if(syncWord == SYNC_IQBUF)
                    {
                        //it's an iqbuf header
                        hdr.timeStamp.clear();

                        //extracts the header data
                        ds >> hdr.uid;
                        ds >> hdr.timeStamp;

                        buf->setTimeStamp(hdr.timeStamp);
                        buf->setBufSize( iqBufSize );
                        totSlices = static_cast<int>(ceil( (iqBufSize * sizeofDoublet) / sliceSize ));
                        sliceIndex = 0;
                    }
                    else if(syncWord == SYNC_SRVCONF)
                    {

                        //it's a configuration change
                        QString     srvDeviceName;
                        QString     srvSRR;
                        QString     srvBBR;
                        QString     srvFR;
                        QString     srvAntenna;
                        int         srvSampleRate;
                        int         srvGain;
                        float       srvPpmError;
                        int         srvTune;

                        ds >> srvDeviceName    ; MYINFO << "srvDeviceName=" << srvDeviceName;   //simply stored
                        ds >> srvSRR           ; MYINFO << "       srvSRR=" << srvSRR;          as->setSampleRateRange(srvSRR);
                        ds >> srvBBR           ; MYINFO << "       srvBBR=" << srvBBR;          as->setBandwidthRange(srvBBR);
                        ds >> srvFR            ; MYINFO << "        srvFR=" << srvFR ;          as->setFreqRange(srvFR);
                        ds >> srvAntenna       ; MYINFO << "   srvAntenna=" << srvAntenna;      as->setAntenna(srvAntenna);
                        ds >> srvSampleRate    ; MYINFO << "srvSampleRate=" << srvSampleRate;   as->setSampleRate(srvSampleRate);
                        ds >> srvBandwidth     ; MYINFO << " srvBandwidth=" << srvBandwidth;    //set later
                        ds >> srvGain          ; MYINFO << "      srvGain=" << srvGain;         as->setGain(srvGain);
                        ds >> srvPpmError      ; MYINFO << "  srvPpmError=" << srvPpmError;     as->setError(srvPpmError);
                        ds >> srvTune          ; MYINFO << "      srvTune=" << srvTune;         as->setTune(srvTune);
                        ds >> isAudioDevice    ; MYINFO << "isAudioDevice=" << isAudioDevice;   as->setAudioDevice(isAudioDevice);
                        ds >> srvDsBandwidth   ; MYINFO << " srvDsBandwidth=" << srvDsBandwidth;
                        ds >> srvIqBufSize     ; MYINFO << " srvIqBufSize=" << srvIqBufSize;       //simply stored
                        ds >> sizeofDoublet    ; MYINFO << "sizeofDoublet=" << sizeofDoublet;   //simply stored

                        ds >> isDirectBuffersEnabled; MYINFO << "isDirectBuffersEnabled=" << isDirectBuffersEnabled;   //simply stored
                        iqBufSize = srvIqBufSize;
                        as->setBandwidth(srvBandwidth);
                        int ratio = as->getBandwidth() / as->getDsBandwidth();
                        as->setResBins(iqBufSize / ratio);
                        if(tempBuf.getSizeofDoublet() != sizeofDoublet)
                        {
                            MYINFO << "Rebuilding IQ buffer pool with samples size " << sizeofDoublet << " bytes";
                            rebuildIQpool();
                            tempBuf = IQbuf(tempBuf.bufSize(), sizeofDoublet);
                        }

                        myCfgChanged = true;
                        MYINFO << "Server radio configuration changed at runtime, received " << recv << " bytes";
                        slotParamsChange();

                    }
                    tempBuf.clear();
                }
                else if(sliceIndex < totSlices)
                {
                    //it's an iqbuf slice

                    if(recv == sliceSize )
                    {
                        int from = (sliceIndex * sliceSize);
                        int to = from + sliceSize;
                        tempBuf.fromByteArray(from, to, baDgram);
                        sliceIndex++;

                        if( sliceIndex == totSlices &&
                            tempBuf.bufByteSize() != (iqBufSize * sizeofDoublet) )
                        {
                            MYWARNING << "expected buffer size and received data don't match "
                                      << "expected = " << (iqBufSize * sizeofDoublet)
                                      << "received bytes =" << tempBuf.bufByteSize();
                            skipBuffer = true;
                            tempBuf.clear();
                            retVal = -2;
                        }
                    }
                    else
                    {

                        MYWARNING << "unexpected shorter slice#" << sliceIndex
                                  << " = " << recv << " bytes, discarding buffer";
                        tempBuf.clear();
                        QDataStream ds(baDgram);
                        qint16 syncWord = -1;
                        ds >> syncWord;
                        if(syncWord == SYNC_IQBUF)
                        {
                            MYWARNING << "unexpected header at slice#" << sliceIndex
                                  << " = " << recv << " bytes, discarding buffer";
                            retVal = -3;
                        }
                        else
                        {
                            MYWARNING << "unexpected shorter slice#" << sliceIndex
                                  << " = " << recv << " bytes, discarding buffer";
                            retVal = -4;
                        }
                    }

                    if(sliceIndex == totSlices && skipBuffer == false)
                    {
                        if(buf->getSizeofDoublet() == sizeof(liquid_int8_complex))
                        {
                            for ( int f = 0; f < iqBufSize ; f++  )
                            {
                                if(isDirectBuffersEnabled)
                                {
                                    //direct buffers access, raw data are always unsigned (?)
                                    buf->setReal( f, static_cast<int8_t>( tempBuf.getUReal8(f) - 128 ));
                                    buf->setImag( f, static_cast<int8_t>( tempBuf.getUImag8(f) - 128 ));
                                }
                                else
                                {
                                    //stream access
                                    buf->setReal( f, static_cast<int8_t>( tempBuf.getReal8(f) ));
                                    buf->setImag( f, static_cast<int8_t>( tempBuf.getImag8(f) ));
                                 }
                            }
                        }
                        else
                        {
                            for ( int f = 0; f < iqBufSize ; f++  )
                            {
                                if(isDirectBuffersEnabled)
                                {
                                    //direct buffers access, raw data are always unsigned (?)
                                    buf->setReal( f, static_cast<int16_t>( tempBuf.getUReal16(f) - 32768 ));
                                    buf->setImag( f, static_cast<int16_t>( tempBuf.getUImag16(f) - 32768 ));
                                }
                                else
                                {
                                    //stream access
                                    buf->setReal( f, static_cast<int16_t>( tempBuf.getReal16(f) ));
                                    buf->setImag( f, static_cast<int16_t>( tempBuf.getImag16(f) ));
                                }

                            }
                        }

                        recvBufferCount++;
                        MYDEBUG << "Received buffer #" << recvBufferCount;

                        retVal = tempBuf.bufByteSize();
                        tempBuf.clear();
                    }

                }

            }
        }
    }
    catch ( QException& e )
    {
        MYCRITICAL << "Exception " << e.what();
    }

    return retVal;
}




bool DongleClient::sendMyConfigName()
{
    MYINFO << __func__;
    QByteArray baMyConfigName;
    qint64 sent = 0;

    if(serverConnected == false)
    {
        if(clientSocket == nullptr)
        {
            clientSocket = new QUdpSocket(/*no parent to avoid "Cannot create children..." */);
            MY_ASSERT( nullptr !=  clientSocket)
        }

        baMyConfigName.append( as->getConfigName().toUtf8() );
        baMyConfigName.append( "." );
        baMyConfigName.append( as->getConfigName().toUtf8() );

        sent = clientSocket->writeDatagram(baMyConfigName, as->getServerAddress(), static_cast<quint16>(as->getServerPort()));
        if(sent != baMyConfigName.size())
        {
            MYCRITICAL << "Sending not complete, sent (=" << sent << ") bytes < baMyConfigName.size()(=" << baMyConfigName.size() << ") bytes";
            return false;
        }
        MYINFO << "configuration sent to " << as->getServerAddress() << ":" << as->getServerPort();
    }
    return true;
}

void DongleClient::sendCommand(QString cmd)
{
    MYDEBUG << __func__ << "(" << cmd << ")";
    QByteArray baCommand;
    qint64 sent = 0;

    if(serverConnected)
    {
        baCommand.append( as->getConfigName().toUtf8() );
        baCommand.append( "." );
        baCommand.append( cmd.toUtf8() );
        sent = clientSocket->writeDatagram(baCommand, as->getServerAddress(), static_cast<quint16>(as->getServerPort()));
        if(sent != baCommand.size())
        {
            MYCRITICAL << "Sending not complete, sent (=" << sent << ") bytes < baCommand.size()(=" << baCommand.size() << ") bytes";
        }
    }
}

bool DongleClient::waitServerConfig()
{
    bool result = false;
    QHostAddress serverAddr;
    quint16 serverPort;

    sendMyConfigName();

    if(clientSocket->state() == QAbstractSocket::BoundState)
    {
        MYINFO << "Waiting server config...";
        if( clientSocket->waitForReadyRead() )
        {
            while (clientSocket->hasPendingDatagrams())
            {
                qint64 size = clientSocket->pendingDatagramSize();
                QByteArray baData(static_cast<int>(size), '\0');

                clientSocket->readDatagram(baData.data(), baData.size(),
                                     &serverAddr, &serverPort);
                QDataStream cfg(baData);
                qint16 syncWord = -1;
                cfg >> syncWord;
                if(syncWord == SYNC_SRVCONF)
                {
                    QString     srvDeviceName;
                    QString     srvSRR;
                    QString     srvBBR;
                    QString     srvFR;
                    QString     srvAntenna;
                    int         srvSampleRate;
                    int         srvGain;
                    float       srvPpmError;
                    qint64      srvTune;
                    uint        srvRxStreamMTU;

                    cfg >> srvDeviceName    ; MYINFO << "srvDeviceName=" << srvDeviceName;   //simply stored
                    cfg >> srvSRR           ; MYINFO << "       srvSRR=" << srvSRR;          as->setSampleRateRange(srvSRR);
                    cfg >> srvBBR           ; MYINFO << "       srvBBR=" << srvBBR;          as->setBandwidthRange(srvBBR);
                    cfg >> srvFR            ; MYINFO << "        srvFR=" << srvFR ;          as->setFreqRange(srvFR);
                    cfg >> srvAntenna       ; MYINFO << "   srvAntenna=" << srvAntenna;      as->setAntenna(srvAntenna);
                    cfg >> srvSampleRate    ; MYINFO << "srvSampleRate=" << srvSampleRate;   as->setSampleRate(srvSampleRate);
                    cfg >> srvBandwidth     ; MYINFO << " srvBandwidth=" << srvBandwidth;    //set later
                    cfg >> srvGain          ; MYINFO << "      srvGain=" << srvGain;         as->setGain(srvGain);
                    cfg >> srvPpmError      ; MYINFO << "  srvPpmError=" << srvPpmError;     as->setError(srvPpmError);
                    cfg >> srvTune          ; MYINFO << "      srvTune=" << srvTune;         as->setTune(srvTune);
                    cfg >> isAudioDevice    ; MYINFO << "isAudioDevice=" << isAudioDevice;   as->setAudioDevice(isAudioDevice);
                    cfg >> srvDsBandwidth   ; MYINFO << "srvDsBandwidth=" << srvDsBandwidth; //simply stored
                    cfg >> srvIqBufSize     ; MYINFO << " srvIqBufSize=" << srvIqBufSize;    //simply stored
                    cfg >> sizeofDoublet    ; MYINFO << "sizeofDoublet=" << sizeofDoublet;   //simply stored
                    cfg >> isDirectBuffersEnabled; MYINFO << "isDirectBuffersEnabled=" << isDirectBuffersEnabled;   //simply stored
                    cfg >> srvRxStreamMTU   ; MYINFO << "SRVrxStreamMTU=" << srvRxStreamMTU;
                    MYINFO << "sizeof CFG: " << baData.size();

                    iqBufSize = srvIqBufSize;
                    as->setMaxRes(srvRxStreamMTU);
                    as->setBandwidth(srvBandwidth);

                    int ratio = srvSampleRate / srvDsBandwidth;
                    MYINFO << "DS ratio = " << ratio;
                    int FFTbins = iqBufSize / ratio;
                    MYINFO << "FFTbins=" << FFTbins;
                    as->setResBins(FFTbins);
                    slotParamsChange();

                    tempBuf.clear();

                    if(tempBuf.getSizeofDoublet() != sizeofDoublet)
                    {
                        MYINFO << "Rebuilding IQ buffer pool with samples size " << sizeofDoublet << " bytes";
                        rebuildIQpool();
                        tempBuf = IQbuf(tempBuf.bufSize(), sizeofDoublet);
                    }

                    result = true;
                    MYINFO << "Server radio configuration received, client connected to server";
                }
                else
                {
                    MYWARNING << "Discarded unknown data packet a, its size is " << baData.size();
                }

            }

        }
    }
    return result;
}


void DongleClient::slotParamsChange()
{
    if(ready == false)
    {
        sr = as->getSampleRate();
        bw = as->getBandwidth();
        dsBw = as->getDsBandwidth();

        if( as->isAudioDevice() )
        {
            lowBound   =  as->getTune() - (dsBw / 4);
            highBound  =  as->getTune() + (dsBw / 4);
        }
        else
        {
            lowBound   =  as->getTune() - (dsBw / 2);
            highBound  =  as->getTune() + (dsBw / 2);
        }
        MYINFO << "Subsampled band from" << lowBound << " to " << highBound << " Hz, width=" << dsBw << " Hz";
/*
        if(as->getSsBypass() == Qt::Checked)
        {
           iqBufSize = srvIqBufSize;
           as->setResBins(iqBufSize, false);
           as->setBandwidth(srvBandwidth, false);

        }
*/
        tempBuf.setBufSize(iqBufSize);
        //the next datagrams will be iqbuf slices
        //let's prepare to receive them

        emit status( tr("Ready.") );

        //ending of parameter change sequence
        ready = true;
    }
}

bool DongleClient::slotSetDsBandwidth()
{
    QMutexLocker ml(&mutex);

    int newBw = as->getDsBandwidth();
    if( dsBw != newBw && dsBw <= sr )
    {
        ready = false;
        dsBw = newBw;
        slotParamsChange();
    }

    return true;
}

bool DongleClient::slotSetResolution()
{
    QMutexLocker ml(&mutex);
    ready = false;
    slotParamsChange();
    return true;
}

bool DongleClient::rebuildIQpool()
{
    if(pb->size() > 0)
    {
        IQbuf* iqb = nullptr;
        pb->clear();
        for (int i = 0; i < DEFAULT_POOLS_SIZE; i++)
        {
            iqb = new IQbuf( MAX_SAMPLE_BUF_SIZE, sizeofDoublet );
            MY_ASSERT( nullptr !=  iqb)
            pb->insert(i, iqb);
        }
        return true;
    }
    return false;
}
