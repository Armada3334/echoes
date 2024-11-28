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
$Id: iqbuf.cpp 23 2018-01-17 16:28:16Z gmbertani $
*******************************************************************************/

#include "settings.h"
#include "iqbuf.h"

int IQbuf::idGen = 0;

IQbuf::IQbuf(int maxDoublets, int szDbl)
{
    imported = false;
    physDoubletSize = maxDoublets;
    doubletsSize = physDoubletSize;
    sizeofDoublet = szDbl;
    byteSize = doubletsSize * sizeofDoublet;
    if(sizeofDoublet == sizeof(liquid_int16_complex))
    {
        buf16ptr = new liquid_int16_complex [ doubletsSize ];
        MY_ASSERT( nullptr != buf16ptr )
        idGen++;
        uid = idGen;
        MYDEBUG << "creating 16-bit buffer [" << uid << "], " << doubletsSize << " IQ doublets, " << byteSize << " bytes total.";

    }
    else
    {
        buf8ptr = new liquid_int8_complex [ doubletsSize ];
        MY_ASSERT( nullptr != buf8ptr )
        idGen++;
        uid = idGen;
        MYDEBUG << "creating 8-bit buffer [" << uid << "], " << doubletsSize << " IQ doublets, " << byteSize << " bytes total.";
    }

    clear();
}


IQbuf::IQbuf(int maxDoublets, int id, liquid_int16_complex* bPtr)
{
    imported = true;
    MY_ASSERT( nullptr !=   bPtr )
    physDoubletSize = maxDoublets;
    doubletsSize = physDoubletSize;
    sizeofDoublet = sizeof(liquid_int16_complex);
    byteSize = doubletsSize * sizeofDoublet;
    buf16ptr = bPtr;

    if(id > idGen)
    {
        idGen = id+1;
    }

    uid = id;
    MYINFO << "importing 16-bit buffer [" << uid << "]=" << MY_HEX << buf16ptr
           << MY_DEC << " size: " << doubletsSize << " IQ doublets, "
               << sizeofDoublet << " bytes each, " << byteSize << " bytes total.";
    clear();
}

IQbuf::IQbuf(int maxDoublets, int id, liquid_int8_complex* bPtr)
{
    imported = true;
    MY_ASSERT( nullptr !=   bPtr )
    physDoubletSize = maxDoublets;
    doubletsSize = physDoubletSize;
    sizeofDoublet = sizeof(liquid_int8_complex);
    byteSize = doubletsSize * sizeofDoublet;
    buf8ptr = bPtr;

    if(id > idGen)
    {
        idGen = id+1;
    }

    uid = id;
    MYINFO << "importing 8-bit buffer [" << uid << "]=" << MY_HEX << buf8ptr
           << MY_DEC << " size: " << doubletsSize << " IQ doublets, "
               << sizeofDoublet << " bytes each, " << byteSize << " bytes total.";
    clear();
}


IQbuf::IQbuf()
{
    imported = false;
    byteSize = 0;
    physDoubletSize = 0;
    doubletsSize = physDoubletSize;
    sizeofDoublet = sizeof(liquid_int16_complex);
    byteSize = doubletsSize * sizeofDoublet;
    buf16ptr = new liquid_int16_complex [ doubletsSize ];
    MY_ASSERT( nullptr != buf16ptr )
    idGen++;
    uid = idGen;

    MYDEBUG << "creating 16-bit buffer [" << uid << "], " << doubletsSize << " IQ doublets, " << byteSize << " bytes total.";
}


IQbuf::~IQbuf()
{
    if(!imported && buf16ptr != nullptr)
    {
        MYDEBUG << "deleting 16-bit buffer, " << physDoubletSize << " doublets wide";
        delete[] buf16ptr;
    }
    buf16ptr = nullptr;

    //imported buffers can't be deleted here
}



IQbuf& IQbuf::operator=(const IQbuf& other)
{
    // Guard self assignment
    if(this == &other)
        return *this;

    //resizing the buffer
    if(!imported)
    {
        if(buf16ptr != nullptr)
        {
            delete[] buf16ptr;
            buf16ptr = nullptr;
        }
        if(buf8ptr != nullptr)
        {
            delete[] buf8ptr;
            buf8ptr = nullptr;
        }
        byteSize = other.bufByteSize();
        physDoubletSize = other.maxSize();
        doubletsSize = physDoubletSize;
        sizeofDoublet = other.getSizeofDoublet();
        if(sizeofDoublet == sizeof(liquid_int16_complex))
        {
            buf16ptr = new liquid_int16_complex [ doubletsSize ];
            MY_ASSERT( nullptr != buf16ptr )
            buf8ptr = nullptr;
        }
        else if(sizeofDoublet == sizeof(liquid_int8_complex))
        {
            buf8ptr = new liquid_int8_complex [ doubletsSize ];
            MY_ASSERT( nullptr != buf8ptr )
            buf16ptr = nullptr;
        }

    }
    else
    {
        MYCRITICAL << "Cannot resize an imported buffer";
    }


    //copying data
    if(other.getSizeofDoublet() == sizeof(liquid_int16_complex))
    {
        std::copy(other.data16ptr(), other.data16ptr()+other.maxSize(), buf16ptr);
    }
    else if(sizeofDoublet == sizeof(liquid_int8_complex))
    {
        std::copy(other.data8ptr(), other.data8ptr()+other.maxSize(), buf8ptr);
    }
    return *this;
}


void IQbuf::setMaxSize( int maxDoublets )
{
    if(!imported)
    {
        if(buf16ptr != nullptr)
        {
            delete[] buf16ptr;
            buf16ptr = nullptr;
        }

        byteSize = maxDoublets * 2 * sizeofDoublet;
        physDoubletSize = byteSize / 2;
        doubletsSize = physDoubletSize;

        buf16ptr = new liquid_int16_complex [ maxDoublets * 2 ];
        MY_ASSERT( nullptr !=   buf16ptr )
    }
    else
    {
        MYCRITICAL << "Cannot resize an imported buffer";
    }
}


bool IQbuf::setBufSize( int doublets )
{
    if(doublets <= physDoubletSize)
    {
        doubletsSize = doublets;
        byteSize = sizeofDoublet * doubletsSize;
        clear();
        return true;
    }

    //required a size bigger than physical
    return false;
}

void IQbuf::clear()
{
    QMutexLocker ml(&mx);


    for(int p=0; p < doubletsSize; p++)
    {
        if(sizeofDoublet == sizeof(liquid_int16_complex))
        {
            buf16ptr[p].real = 0;
            buf16ptr[p].imag = 0;
        }
        else if(sizeofDoublet == sizeof(liquid_int8_complex))
        {
            buf8ptr[p].real = 0;
            buf8ptr[p].imag = 0;
        }
    }

    MYDEBUG << "zeroing buffer (" << uid << "), " << doubletsSize << " doublets wide";
}

void IQbuf::toByteArray( int from, int to, QByteArray& ba )
{
    int retSize = to - from;
    MY_ASSERT(from >= 0 && to < byteSize && retSize <= byteSize && retSize > 0)
    ba.clear();
    ba.resize( retSize );

    from /= sizeofDoublet;
    to /= sizeofDoublet;
    if(sizeofDoublet == sizeof(liquid_int16_complex))
    {
        liquid_int16_complex* iqPtr = reinterpret_cast<liquid_int16_complex*>( ba.data() );
        for(int i=from, f=0; i < to; i ++, f ++)
        {
            iqPtr[f].real = buf16ptr[i].real ;
            iqPtr[f].imag = buf16ptr[i].imag ;
        }
    }
    else if(sizeofDoublet == sizeof(liquid_int8_complex))
    {
        liquid_int8_complex* iqPtr = reinterpret_cast<liquid_int8_complex*>( ba.data() );
        for(int i=from, f=0; i < to; i ++, f ++)
        {
            iqPtr[f].real = buf8ptr[i].real ;
            iqPtr[f].imag = buf8ptr[i].imag ;
        }

    }

}

void IQbuf::fromByteArray( int from, int to, QByteArray& ba )
{
    int addSize = to - from;
    MY_ASSERT(from >= 0 && to <= byteSize && addSize <= byteSize && addSize > 0)

    from /= sizeofDoublet;
    to /= sizeofDoublet;

    if(sizeofDoublet == sizeof(liquid_int16_complex))
    {
        liquid_int16_complex* iqPtr = reinterpret_cast<liquid_int16_complex*>( ba.data() );
        for(int i=from, f=0; i < to; i ++, f ++)
        {
            int16_t _i = iqPtr[f].real;
            int16_t _q = iqPtr[f].imag;
            buf16ptr[i].real = _i ;
            buf16ptr[i].imag = _q ;
        }
    }
    if(sizeofDoublet == sizeof(liquid_int8_complex))
    {
        liquid_int8_complex* iqPtr = reinterpret_cast<liquid_int8_complex*>( ba.data() );
        for(int i=from, f=0; i < to; i ++, f ++)
        {
            int16_t _i = iqPtr[f].real;
            int16_t _q = iqPtr[f].imag;
            buf8ptr[i].real = _i ;
            buf8ptr[i].imag = _q ;
        }
    }
}

