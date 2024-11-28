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
$Id: dclient.h 149 2018-03-13 23:08:31Z gmbertani $
*******************************************************************************/

#ifndef DCLIENT_H
#define DCLIENT_H

#include <QtCore>
#include <QtNetwork>

//#include "liquid.h"
#include "iqdevice.h"

struct IQbufHeader
{
    int uid;
    QString timeStamp;
};



///
/// \brief The DongleClient class
///
class DongleClient  : public IQdevice
{
    Q_OBJECT

protected:
    QRecursiveMutex mutex;                                 ///< GUI slots and readSamples() must be synced

    bool serverConnected;
    bool isDirectBuffersEnabled;                  ///< server is sending direct buffers (so uses CU8 or CU16 raw formats)
    bool isAudioDevice;                           ///< server device is not a SDR dongle
    int droppedSamples;                           ///< number of I/Q pairs missing
    int timeAcq;                                  ///< time spent in acquisition loop
    int recvBufferCount;                          ///< progressive count of sub buffers submitted to radio
    int iqBufSize;                                ///< IQbuffers size on client
    int srvIqBufSize;                             ///< IQbuffers size on server
    int srvBandwidth;                             ///< Server bandwidth
    int srvDsBandwidth;                           ///< downsampled bandwidth on server
    int sizeofDoublet;                            ///< server sample format:
                                                  /// (2=liquid_int8_complex, 4=liquid_int16_complex)

    QElapsedTimer acqTimer;                       ///< statistic timer
    IQbuf tempBuf;                                ///< sub buffer data storage

    QUdpSocket* clientSocket;                     ///< client socket

    IQbufHeader hdr;                              ///< incoming buffer header


    ///
    /// \brief sendMyConfigName
    ///
    bool sendMyConfigName();

    ///
    /// \brief readSamples
    /// \param buf
    /// \return
    ///
    int readSamples(IQbuf* buf);

    ///
    /// \brief waitServerConfig
    ///
    /// Performs stop-time handshake with the server.
    /// The client sends its name (the current rts file name)
    /// The server sends back its device configuration
    bool waitServerConfig();

    ///
    /// \brief sendCommand
    /// \param cmd
    ///
    void sendCommand(QString cmd);

protected slots:

    ///
    /// \brief paramsChange
    ///
    virtual void slotParamsChange() override;

public:

    ///
    /// \brief DongleClient
    /// \param appSettings
    /// \param bPool
    /// \param parent
    ///
    explicit DongleClient(Settings* appSettings, Pool<IQbuf*> *bPool, QThread *parentThread = nullptr);

    ~DongleClient();

    ///
    /// \brief lookForServerStatic
    /// \param appSettings
    /// \param devices
    /// \return
    ///
    static bool lookForServerStatic(Settings *appSettings, QList<DeviceMap>* devices);

    ///
    /// \brief clearUDPinputSocket
    /// \return wasted bytes
    ///
    static int clearUDPinputSocketStatic(QUdpSocket *socket);

    ///
    /// \brief run
    ///
    void run() override;

    ///
    /// \brief isDeviceOpen
    /// \return
    ///
    bool isDeviceOpen() override
    {
        return serverConnected;
    }

    ///
    /// \brief className
    /// \return
    ///
    virtual const char* className() const override
    {
        return static_cast<const char*>( "DongleClient" );
    }

    ///
    /// \brief getStats
    /// \param stats
    /// \return
    ///
    bool getStats( StatInfos* stats ) override;


    ///
    /// \brief rebuildIQpool
    /// \return
    ///
    bool rebuildIQpool();


public slots:


    ///
    /// \brief slotSetDsBandwidth
    /// \return
    ///
    virtual bool slotSetDsBandwidth() override;


    ///
    /// \brief slotSetResolution
    /// \return
    ///
    virtual bool slotSetResolution() override;
};

#endif // DCLIENT_H
