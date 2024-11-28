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
$Id: scan.h 23 2018-01-17 16:28:16Z gmbertani $
*******************************************************************************/


#ifndef SCAN_H
#define SCAN_H

#include <QtCore>



class Scan
{
    int         physSize;       //< buffer physical size
    int         bufSize;        //< buffer logical size (floats)
    float*      buf;
    float       binSize;        //< buffer resolution (size of each bin in hz)
    QString     timeStamp;      //< capture timestamp

    QRecursiveMutex      mx;

    int         uid;             //< unique identifier in pool
    static int  idGen;           //< unique identifiers generator

public:
    ///
    /// \brief Scan
    /// \param maxSize
    ///
    Scan(int maxSize);

    ~Scan();

    ///
    /// \brief constData
    /// \return
    ///
    const float* constData() const
    {
        return buf;
    }

    ///
    /// \brief data
    /// \return
    ///
    float* data() const
    {
        return buf;
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
        Scan::idGen = 0;
    }

    ///
    /// \brief clear
    ///
    void clear();

    ///
    /// \brief integrate
    /// \param add
    ///
    ///
    ///
    void average( Scan* add );

    ///
    /// \brief peaks
    /// \param add
    ///
    void peaks( Scan* add );


    /*
     * the following methods are meant for
     * auxiliary data storage
     */

    ///
    /// \brief setBufSize
    /// \param bufSz
    /// \return
    ///
    bool setBufSize(int bufSz)
    {
        QMutexLocker ml(&mx);
        if(bufSize <= physSize)
        {
            bufSize = bufSz;
            return true;
        }
        return false;
    }

    ///
    /// \brief setBinSize
    /// \param bs
    ///
    void setBinSize(float bs)
    {
        QMutexLocker ml(&mx);
        binSize = bs;
    }

    ///
    /// \brief setTimeStamp
    /// \param ts
    ///
    void setTimeStamp(QString& ts)
    {
        QMutexLocker ml(&mx);
        timeStamp = ts;
    }

    ///
    /// \brief getBufSize
    /// \return
    ///
    int getBufSize() const
    {
        return bufSize;
    }

    ///
    /// \brief getBinSize
    /// \return
    ///
    float getBinSize() const
    {
        return binSize;
    }

    ///
    /// \brief getTimeStamp
    /// \return
    ///
    QString getTimeStamp() const
    {
        return timeStamp;
    }



};

#endif // SCAN_H
