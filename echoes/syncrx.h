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
$Id: syncrx.h 149 2018-03-13 23:08:31Z gmbertani $
*******************************************************************************/

#ifndef SYNCRX_H
#define SYNCRX_H

#include <QtCore>
#include <QtNetwork>
#include <cmath>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Errors.hpp>
#include "iqdevice.h"

///
/// \brief The ClientDescriptor struct
///
struct ClientDescriptor
{
    QString         name;       ///<descriptive only, needs not to be unique
    QHostAddress    address;    ///<address+port must be unique in clients list
    quint16         port;
    bool            active;     ///<true = the client is ready to receive the samples flux
    int             timeout;    ///scans left before client disconnection
};



///
/// \brief The SyncRx class
///
class SyncRx : public IQdevice
{
    Q_OBJECT

protected:
    bool bufReady;
    bool iqRecordOn;                            ///<iq samples recording active
    bool isAudioDevice;                         ///<not a SDR dongle
    bool hasBufflen;                            ///<true if the device allows to set its internal buffers length
    bool hasBuffers;                            ///<true if the device allows to set its internal buffers number

    int overflows;                              ///<progressive count of consecutive overflows
    int underflows;                             ///<progressive count of duplicated buffers for direct buffers reading delay calculation
    int droppedSamples;                         ///<progressive count of unavailable buffers
    int dbDelayMs;                              ///<direct buffers reading calculated delay [ms]
    int timeAcq;                                ///<time spent in acquisition loop
    int avgTimeAcq;                             ///<average time spent in acquisition loop
    int cfgChangesCount;                        ///<configuration changes counter for clients notification
    int forwardedBuffersCount;                  ///<progressive count of data buffers correctly processed and forwarded to Radio
    int sentBuffersCount;                       ///<progressive count of buffers sent to clients, if any
    int sysFaultCount;                          ///<progressive nr. of errors returned by Soapy API functions
    int iqBufRoundSize;                         ///<rx buffers size
    double offsetGain;                          ///<difference between 0dB set as gain and the gain read back afterwards

    qint64 timeNs;

    size_t channel;                             ///<Soapy RX channel
    SoapySDR::Device *dev;                      ///<Soapy opened device SDR
    QString dataFormat;                         ///<Soapy stream format (CS8 or CS16)

    QElapsedTimer acqTimer;                     ///<statistic timer to measure the time needed to read and forward one buffer
    QUdpSocket* serverSocket;                   ///<UDP socket for dongle client connections
    QList<ClientDescriptor*> clients;           ///<connected clients list

    uint rxStreamMTU;                           ///<Soapy stream MTU in doublets
    double rxStreamFS;                          ///<Soapy stream data fullscale value
    SoapySDR::Stream* stream;                   ///<Soapy stream pointer
    SoapySDR::Kwargs streamArgs;                ///<Soapy stream parameters (arguments)
    SoapySDR::Range gainRange;                  ///<Soapy device gains range

    liquid_int16_complex check16;               ///<Check value of the latest scan forwarded - 16 bit format
    liquid_int8_complex check8;                 ///<Check value of the latest scan forwarded - 8 bit format

    ///
    /// \brief startServer
    ///
    bool startServer();

    ///
    /// \brief sendDeviceConfig
    /// \param client
    ///
    void sendDeviceConfig(ClientDescriptor* client);

    ///
    /// \brief stopAllClients
    ///
    void stopAllClients();

    ///
    /// \brief stopClient
    /// \param client
    ///
    void stopClient( ClientDescriptor* client );

    ///
    /// \brief broadcastSamples
    /// \param buf
    ///
    void shareSamples(IQbuf* buf);

    ///
    /// \brief readPendingClientDatagrams
    ///
    void readPendingClientDatagrams();

    ///
    /// \brief broadcastDeviceConfig
    ///
    void shareDeviceConfig();

    ///
    /// \brief calcBufSize
    /// \return
    ///
    int calcBufSize();

    ///
    /// \brief checkForScanFrozen
    /// \param buf
    /// \return
    ///
    ///  detects acquisition faults not reported by Soapy
    ///
    bool checkForScanFrozen(IQbuf* buf);

public:

    ///
    /// \brief SyncRx
    /// \param appSettings
    /// \param bPool
    /// \param parent
    ///
    explicit SyncRx(Settings* appSettings, Pool<IQbuf*> *bPool, QThread *parentThread = nullptr, int devIndex = 0);

    ~SyncRx();

    ///
    /// \brief lookForDongleStatic
    /// \param devices
    /// \return
    ///
    static bool lookForDongleStatic(QList<DeviceMap> *devices );

    ///
    /// \brief run
    ///
    virtual void run() override;

    ///
    /// \brief className
    /// \return
    ///
    virtual const char* className() const override
    {
        return static_cast<const char*>( "SyncRx" );
    }

    ///
    /// \brief deviceOpen
    /// \return
    ///
    virtual bool isDeviceOpen() override;   


    ///
    /// \brief getStats
    /// \param stats
    /// \return
    ///
    virtual bool getStats( StatInfos* stats ) override;

    ///
    /// \brief DCspikeRemoval
    ///
    void DCspikeRemoval(bool status);

    ///
    /// \brief getDataFormat
    /// \return
    ///
    QString getDataFormat() const
    {
        return dataFormat;
    }


public slots:

    ///
    /// \brief paramsChange
    ///
    virtual void slotParamsChange() override;

    ///
    /// \brief slotSetTune
    /// \return
    ///
    virtual bool slotSetTune() override;
    ///
    /// \brief slotSetSampleRate
    /// \return
    ///
    virtual bool slotSetSampleRate() override;

    ///
    /// \brief slotSetGain
    /// \return
    ///
    virtual bool slotSetGain() override;

    ///
    /// \brief slotSetError
    /// \return
    ///
    virtual bool slotSetError() override;

    ///
    /// \brief slotSetDsBandwidth
    /// \return
    ///
    virtual bool slotSetDsBandwidth() override;


    ///
    /// \brief slotSetAntenna
    /// \return
    ///
    virtual bool slotSetAntenna() override;

    ///
    /// \brief slotSetBandwidth
    /// \return
    ///
    virtual bool slotSetBandwidth() override;

    ///
    /// \brief slotSetFreqRange
    /// \return
    ///
    virtual bool slotSetFreqRange() override;

};

#endif // SYNCRX_H
