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
$Date:: 2020-08-26 00:13:47 +02#$: Date of last commit
$Id: iqdevice.cpp 351 2020-08-26 00:13:47 gmbertani $
*******************************************************************************/

#include "setup.h"
#include "iqdevice.h"



IQdevice::IQdevice(Settings* appSettings, Pool<IQbuf*> *bPool, QThread *parentThread)
{
    //setObjectName("IQdevice thread");
    if(parentThread != nullptr)
    {
        setParent(parentThread);
    }
    //MY_ASSERT( nullptr !=  parentThread);
    //MYINFO << "parentThread=" << parentThread << " parent()=" << parent();

    MY_ASSERT( nullptr !=  appSettings)
    as = appSettings;

    MY_ASSERT( nullptr !=  bPool)
    pb = bPool;

    ready = false;
    resamplerBypass = false;
    myCfgChanged = false;
    stopThread = false;
    hasFreqErrorCorr = false;

    sr = 0;
    dsBw = 0;
    bw = 0;
    tune = 0;
    lowBound  = 0;
    highBound = 0;

    deviceName = as->getDevice();

    srRange = tr("N.A.");
    bbRange = tr("N.A.");
    frRange = tr("N.A.");
    antenna = tr("N.A.");

    srRanges.append(srRange);
    bbRanges.append(bbRange);
    frRanges.append(frRange);
    antennas.append(antenna);

    qRegisterMetaType<QAbstractSocket::SocketError>();

}

IQdevice::~IQdevice()
{

}



