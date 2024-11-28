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
$Rev:: 65                       $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-02-06 00:18:10 +01#$: Date of last commit
$Id: iqbuf.h 65 2018-02-05 23:18:10Z gmbertani $
*******************************************************************************/


#ifndef IQBUF_H
#define IQBUF_H

#include <QtCore>

#undef _GLIBCXX_COMPLEX
#include "liquid.h"
#include "settings.h"

LIQUID_DEFINE_COMPLEX(int16_t,  liquid_int16_complex);
LIQUID_DEFINE_COMPLEX(int8_t, liquid_int8_complex);


/**
 * @brief The IQbuf class
 * an IQbuf object contains a scan got from SDR in
 * a format closest possible to native one.
 * A scan is an array of I+Q doublets. int16_t specifies the base type
 * of each doublet, the doublet is expressed by a liquid_int16_complex
 * The array is sized to the maximum allowed (physical size)
 * It can be used partially, in order to cover narrower
 * bandwidths.
 * doubletsSize is the used part and is called logical size.
 *
 * While using imported buffers, the size of a doublet must be
 * specified in order to manage correctly the memory in case
 * the imported buffers have a lower resolution than liquid_int16_complex
 * like liquid_int8_complex
 *
 * 8-bit buffers can only be imported, not created.
 *
 */

class IQbuf
{
    liquid_int16_complex*  buf16ptr = nullptr;
    liquid_int8_complex*   buf8ptr  = nullptr;

    QString     tsString;                                           //< capture timestamp
    bool        imported = false;                                   //< the data buffer has been imported
    int         physDoubletSize = 0;                                //< maximum physical I+Q doublets allocated size [bytes]
    int         doubletsSize = 0;                                   //< logical number of I+Q doublets in buffer [doublets]
    int         sizeofDoublet = sizeof(liquid_int16_complex);           //< byte size of a doublet,- normally sizeof(int16_t) but could be smaller
    int         byteSize = 0;                                       //< logical size of I+Q doublets in buffer [bytes]
    int         uid   = 0;                                          //< unique identifier in pool
    QRecursiveMutex mx;



    static int  idGen;                      //< unique identifiers generator

public:
    ///
    /// \brief IQbuf
    /// \param maxDoublets     physical number of iq doublets
    ///
    /// the memory allocated (physical size) for each iqbuf remains maxDoublets
    /// while the logical size (portion containing valid data) can be
    /// changed at runtime, reflecting the sample rate and
    /// bandwidth chosen
    ///
    explicit IQbuf(int maxDoublets, int szDbl=sizeof(liquid_int16_complex));

    ///
    /// \brief IQbuf
    /// constructor for imported 16bit buffers (allocated externally)
    ///
    /// \param maxDoublets  physical number of iq doublets
    /// \param id           buffer identifier / handle
    /// \param bPtr         data buffer pointer
    ///
    IQbuf(int maxDoublets, int id, liquid_int16_complex* bPtr);

    ///
    /// \brief IQbuf
    /// constructor for imported 8bit buffers (allocated externally)
    ///
    /// \param maxDoublets  physical number of iq doublets
    /// \param id           buffer identifier / handle
    /// \param bPtr         data buffer pointer
    ///
    IQbuf(int maxDoublets, int id, liquid_int8_complex* bPtr);


    ///
    /// \brief IQbuf
    /// empty constructor
    IQbuf();

    ///
    virtual ~IQbuf();


    ///
    /// \brief operator []
    /// \param idx
    /// \return
    ///
    liquid_int16_complex&  operator[](size_t idx)
    {
        return buf16ptr[idx];
    }

    ///
    /// \brief operator =
    /// \param other
    /// \return
    ///
    IQbuf& operator=(const IQbuf& other);

    ///
    /// \brief getReal16
    /// \param doubletIdx
    /// \return
    ///
    int16_t getReal16(int doubletIdx)
    {
        return buf16ptr[doubletIdx].real;
    }


    ///
    /// \brief getImag16
    /// \param doubletIdx
    /// \return
    ///
    int16_t getImag16(int doubletIdx)
    {
        return buf16ptr[doubletIdx].imag;
    }

    ///
    /// \brief getReal8
    /// \param doubletIdx
    /// \return
    ///
    int8_t getReal8(int doubletIdx)
    {
        return buf8ptr[doubletIdx].real;
    }


    ///
    /// \brief getImag8
    /// \param doubletIdx
    /// \return
    ///
    int8_t getImag8(int doubletIdx)
    {
        return buf8ptr[doubletIdx].imag;
    }

    ///
    /// \brief getUReal16
    /// \param doubletIdx
    /// \return
    ///
    uint16_t getUReal16(int doubletIdx)
    {
        return static_cast<uint16_t>(buf16ptr[doubletIdx].real);
    }


    ///
    /// \brief getUImag16
    /// \param doubletIdx
    /// \return
    ///

    uint16_t getUImag16(int doubletIdx)
    {
        return static_cast<uint16_t>(buf16ptr[doubletIdx].imag);
    }
    ///
    /// \brief getUReal8
    /// \param doubletIdx
    /// \return
    ///
    uint8_t getUReal8(int doubletIdx)
    {
        return static_cast<uint8_t>(buf8ptr[doubletIdx].real);
    }


    ///
    /// \brief getUImag8
    /// \param doubletIdx
    /// \return
    ///
    uint8_t getUImag8(int doubletIdx)
    {
        return static_cast<uint8_t>(buf8ptr[doubletIdx].imag);
    }


    ///
    /// \brief setReal
    /// \param doubletIdx
    /// \param value
    ///
    void setReal(int doubletIdx, const int16_t& value)
    {
        buf16ptr[doubletIdx].real = value;
    }


    ///
    /// \brief setImag
    /// \param doubletIdx
    /// \param value
    ///
    void setImag(int doubletIdx, const int16_t& value)
    {
        buf16ptr[doubletIdx].imag = value;
    }


    ///
    /// \brief setReal
    /// \param doubletIdx
    /// \param value
    ///
    void setReal(int doubletIdx, const int8_t& value)
    {
        buf8ptr[doubletIdx].real = value;
    }


    ///
    /// \brief setImag
    /// \param doubletIdx
    /// \param value
    ///
    void setImag(int doubletIdx, const int8_t& value)
    {
        buf8ptr[doubletIdx].imag = value;
    }

    ///
    /// \brief setPhysicalSize
    /// \param maxDoublets
    ///
    void setMaxSize( int maxDoublets );


    ///
    /// \brief setBufSize
    /// \param doublets
    /// \return
    ///
    bool setBufSize( int doublets );


    ///
    /// \brief clear
    ///
    void clear();


    ///
    /// \brief IQdataPtr
    /// \return a complex 16-bit pointer to the buffer
    ///
    liquid_int16_complex* data16ptr() const
    {
        return buf16ptr;
    }

    ///
    /// \brief IQdataPtr
    /// \return a complex 8-bit pointer to the buffer
    ///
    liquid_int8_complex* data8ptr() const
    {
        return buf8ptr;
    }

    ///
    /// \brief voidPtr
    /// \return a void pointer to the buffer

    void* voidPtr() const
    {
        if(sizeofDoublet == sizeof(liquid_int16_complex))
        {
            MY_ASSERT(buf16ptr != nullptr)
            return static_cast<void*>( buf16ptr );
        }
        MY_ASSERT(buf8ptr != nullptr)
        return static_cast<void*>( buf8ptr );
    }

    int getSizeofDoublet() const
    {
        return sizeofDoublet;
    }

    ///
    /// \brief toByteArray
    /// \param ba
    ///
    void toByteArray( int from, int to, QByteArray& ba );

    ///
    /// \brief fromByteArray
    /// \param ba
    ///
    void fromByteArray( int from, int to, QByteArray& ba );


    ///
    /// \brief setTimeStamp
    /// \param ts
    ///
    void setTimeStamp(QString& ts)
    {
        QMutexLocker ml(&mx);
        tsString = ts;
    }

    ///
    /// \brief myId
    /// \return
    ///
    int myUID() const
    {
        return uid;
    }

    ///
    /// \brief resetUIDgen
    /// \return
    ///
    void resetUIDgen()
    {
        IQbuf::idGen = 0;
    }

    ///
    /// \brief bufByteSize
    /// \return the logical size in bytes
    ///
    int bufByteSize() const
    {
        return byteSize;
    }

    ///
    /// \brief bufSize
    /// \return the logical size in doublets
    ///
    int bufSize() const
    {
        return doubletsSize;
    }

    ///
    /// \brief maxSize
    /// \return
    ///
    int maxSize() const
    {
        return physDoubletSize;
    }

    ///
    /// \brief getTimeStamp
    /// \return
    ///
    QString timeStamp() const
    {
        return tsString;
    }


};



#endif // IQBUF_H
