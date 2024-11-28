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
$Rev:: 23                       $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-01-17 17:28:16 +01#$: Date of last commit
$Id: scan.cpp 23 2018-01-17 16:28:16Z gmbertani $
*******************************************************************************/

#include "settings.h"
#include "scan.h"

int Scan::idGen = 0;

Scan::Scan(int maxSize)
{

    buf = new float [ maxSize ];
    MY_ASSERT( nullptr !=   buf )

    physSize = maxSize;
    bufSize = physSize;
    idGen++;
    uid = idGen;
    MYDEBUG << "creating buffer [" << uid << "], " << maxSize << " floats wide max";

    clear();
}

Scan::~Scan()
{
    if(buf != nullptr)
    {
        MYDEBUG << "deleting buffer, " << bufSize << " floats wide";
        delete[] buf;
        buf = nullptr;
    }
}



void Scan::clear()
{
    QMutexLocker ml(&mx);

    MY_ASSERT( nullptr !=  buf)

    for(int i = 0; i < bufSize; i++)
    {
        buf[i] = 0.0;
    }
    MYDEBUG << "zeroing buffer, " << bufSize << " floats wide";

    binSize = 0;
    timeStamp = "";
}

void Scan::average( Scan* add )
{
    MY_ASSERT( add->bufSize ==  bufSize )


    for(int i = 0; i < bufSize; i++)
    {
        buf[i] += add->constData()[i];
        buf[i] /= 2.0F;
    }
}

void Scan::peaks( Scan* add )
{
    MY_ASSERT( add->bufSize ==  bufSize )


    for(int i = 0; i < bufSize; i++)
    {
        if(buf[i] < add->constData()[i])
        {
            buf[i] = add->constData()[i];
        }
    }
}
