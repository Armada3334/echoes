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
$Rev:: 261                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-07-17 13:57:32 +02#$: Date of last commit
$Id: circbuf.cpp 261 2018-07-17 11:57:32Z gmbertani $
*******************************************************************************/

#include "settings.h"
#include "circbuf.h"

CircBuf::CircBuf(bool keepFull, int maxRecords,  bool binFormat, QString logFilePath)
{    
    setMax(maxRecords);
    keepFull = keepFull;
    outPath = logFilePath;
    isBinary = binFormat;

    full = false;
    MYINFO << "Circbuf " << this << " keepFull: " << keepFull << "binary: " << isBinary << " created on file " << outPath;
}


CircBuf::~CircBuf()
{
    if(txtStr.device() != nullptr)
    {
        txtStr.flush();
        outFile.flush();
        outFile.close();
        if(keepFull == false)
        {
            q.clear();
            curr.clear();
            scan.clear();
        }
    }

}


int CircBuf::setFile(QString& path, bool append)
{
    QMutexLocker ml(&protectionMutex);

    if(isBinary)
    {
        MYCRITICAL << __func__ << "() can't be used on binary dumps" << outPath << MY_ENDL;
        return -2;
    }

    if(outPath != "")
    {
        txtStr.flush();
        outFile.flush();
        outFile.close();
    }

    //The file is closed but the data are kept in queue
    //To flush the queue, unsetFile() must be called first
    outPath = path;
    outFile.setFileName(outPath);

    if(append == false)
    {
        //create a brand new file
        //if another one already exists, it will be overwritten
        //Note it never uses QIODevice::Text so both binary and text buffers can be written
        if ( outFile.open(QIODevice::WriteOnly|QIODevice::Truncate) == 0 )
        {
            MYCRITICAL << "cannot create a new binary file " << outPath << MY_ENDL;
            return -1;
        }
    }
    else
    {
        //reopen an existing file if exists, otherwise creates it
        if ( outFile.open(QIODevice::WriteOnly|QIODevice::Append) == 0 )
        {
            MYCRITICAL << "cannot create a new file " << outPath << MY_ENDL;
            return -1;
        }
    }

    MYINFO << "opened circular no-binary buffer file " << outFile;
    txtStr.setDevice(&outFile);
    return 0;
}


int CircBuf::setFile(QString& path, QByteArray& header)
{
    QMutexLocker ml(&protectionMutex);

    if(!isBinary)
    {
        MYCRITICAL << __func__ << "() can't be used on ascii dumps" << outPath << MY_ENDL;
        return -2;
    }

    if(outPath != "")
    {
        outFile.flush();
        outFile.close();
    }

    //The file is closed but the data are kept in queue
    //To flush the queue, unsetFile() must be called first
    outPath = path;
    outFile.setFileName(outPath);

    //reopen an existing file if exists, otherwise creates it
    if ( outFile.open(QIODevice::WriteOnly|QIODevice::Append) == 0 )
    {
        MYCRITICAL << "cannot create a new file " << outPath << MY_ENDL;
        return -1;
    }
    MYINFO << "opened circular binary buffer file " << outFile;
    outFile.write(header);
    return 0;
}


int CircBuf::unSetFile()
{
    QMutexLocker ml(&protectionMutex);

    if(outPath != "")
    {
        MYDEBUG << "Circbuf " << this << " unset file " << outPath;
        txtStr.flush();
        outFile.flush();
        outFile.close();


        outPath = "";
        if(keepFull == false)
        {
            q.clear();
            curr.clear();
            scan.clear();
        }

        return 0;
    }
    return -1;
}




void CircBuf::setMax(int maxRecords)
{
    QMutexLocker ml(&protectionMutex);

    q.clear();
    if (maxRecords < CIRCBUF_MIN)
    {
        maxRecs = CIRCBUF_MIN;
    }
    else
    {
        maxRecs = maxRecords;
    }
    full = false;
    MYINFO << "Circbuf " << this << " resized to max records: " << maxRecs;
}

int CircBuf::append(const char* format, ...)
{
    QMutexLocker ml(&protectionMutex);
    MY_ASSERT(format != nullptr)

    QString buf;
    va_list args;
    va_start(args, format);

    buf = QString::vasprintf(format, args);
    curr.append(buf.toUtf8());

    va_end(args);
    return curr.length();
}

int CircBuf::append(QString& buf)
{
    QMutexLocker ml(&protectionMutex);
    //MYDEBUG << "appending to CSV";
    curr.append(buf.toUtf8());
    return curr.length();
}

int CircBuf::append(QByteArray& ba)
{
    QMutexLocker ml(&protectionMutex);
    //MYDEBUG << "appending to DATB";
    curr.append(ba);
    return curr.length();
}

int CircBuf::insert(const char* format, ...)
{
    QMutexLocker ml(&protectionMutex);

    QString buf;
    int currLen = 0;

    va_list args;
    va_start(args, format);


    buf = QString::vasprintf(format, args);
    curr.append(buf.toUtf8());
    currLen = curr.length();

    MYDEBUG << "Circbuf " << this << " inserting a record " << currLen << " bytes long";

    commit();

    va_end(args);
    return currLen;
}





void CircBuf::commit(bool prepend)
{
    QMutexLocker ml(&protectionMutex);

    if(prepend == true)
    {
        q.prepend(curr);
    }
    else
    {
        q.enqueue(curr);
    }

    curr.clear();

    MYDEBUG << "Circbuf " << this << " committed scan# metadata" << q.size();

    if(q.size() >= maxRecs)
    {
        full = true;

        int excess = q.size() - maxRecs;
        while(excess > 0)
        {
            if(prepend == true)
            {
                //discards the newest record
                q.takeLast();
                excess = q.size() - maxRecs;
                MYDEBUG << "Circbuf " << this << " full, discarding newest scan (prepend=" << prepend << "), left: " << excess;
            }
            else
            {
                //discards the oldest record
                q.dequeue();
                excess = q.size() - maxRecs;
                MYDEBUG << "Circbuf " << this << " full, discarding oldest scan, left: " << excess;
            }
        }
    }

}


void CircBuf::rollback(uint recs)
{
    QMutexLocker ml(&protectionMutex);

    for(uint i = 0; i < recs; i++)
    {
        if(q.count() >= 1)
        {
            q.removeLast();
        }
    }

    MYDEBUG << "Circbuf " << this << " removed latest " << recs << " scans, now " << q.size() << " scans are still in queue";

    if(q.size() < maxRecs)
    {
        full = false;
    }

}



int CircBuf::dump(QString shotName, int numRecs)
{
    QMutexLocker ml(&protectionMutex);

    MYDEBUG << this << ":dump( " << shotName << ", " << numRecs << " )";


    int written = 0;
    if(isBinary)
    {
        MYWARNING << "Circbuf " << this << " dump() don't work for binary files, use bdump() instead.";
        return written;
    }


    if(outFile.isOpen() == false)
    {
        MYDEBUG << "Circbuf " << this << " dump() requested with file not yet assigned, doing nothing";
        return written;
    }

    if(keepFull == true)
    {
        //with kpFull, the records in queue are copied to disk
        //but the queue is not flushed. This allows to keep the
        //dump files size almost equal, once the queue reaches
        //its maximum size.
        //This behavior is good for plot files, not for CSV
        //Please note, a single spectra scan line is composed by many lines


        foreach (scan, q)
        {

            if(shotName.isEmpty() == true)
            {
                if(isBinary)
                {
                    txtStr << scan << MY_ENDL;
                }
                else
                {
                    txtStr << QString::fromUtf8(scan) << MY_ENDL;
                }
                MYDEBUG << "Circbuf " << this << " copied record# " << written << " to file " << outPath;
            }
            else
            {
                if(isBinary)
                {
                    txtStr << scan << shotName << MY_ENDL;
                }
                else
                {
                    txtStr << QString::fromUtf8(scan) << shotName << MY_ENDL;

                }
                MYDEBUG << "Circbuf " << this << " copied record# " << written << " to file " << outPath << " shotName: " << shotName;
            }

            written++;
            if(numRecs > 0 &&
               written >= numRecs)
            {
                break;
            }
            
            QThread::yieldCurrentThread();
        }

    }
    else
    {
        //without kpFull, the records in queue are flushed on the disk
        //leaving the queue empty. This behavior is good for CSV files
        while(q.empty() == false)
        {
            scan = q.dequeue();
            if(shotName.isEmpty() == true)
            {
                if (isBinary)
                {
                    txtStr << scan << MY_ENDL;
                }
                else
                {
                    txtStr << QString::fromUtf8(scan) << MY_ENDL;
                }

                MYDEBUG << "Circbuf " << this << " extracted oldest record# " << written << " to file " << outPath;
            }
            else
            {
                //the shot name is appended as last field of the record
                if (isBinary)
                {
                    txtStr << scan << shotName << MY_ENDL;
                }
                else
                {
                    txtStr << QString::fromUtf8(scan) << shotName << MY_ENDL;
                }

                MYDEBUG << "Circbuf " << this << " extracted oldest record# " << written << " to file " << outPath << " shotName: " << shotName;
            }
            written++;
            if(numRecs > 0 &&
               written >= numRecs)
            {
                break;
            }
            QThread::yieldCurrentThread();
        }
        MYDEBUG << "Circbuf " << this << " queue empty";
        full = false;
    }
    txtStr.flush();
    return written;
}


int CircBuf::bdump(int numRecs)
{
    QMutexLocker ml(&protectionMutex);

    MYDEBUG << this << ":bdump( " << numRecs << " )";
    int written = 0;
    QByteArray buf;

    if(!isBinary)
    {
        MYWARNING << "Circbuf " << this << " bdump() don't work for ASCII files, use dump() instead.";
        return written;
    }

    if(outFile.isOpen() == false)
    {
        MYDEBUG << "Circbuf " << this << " dump() requested with file not yet assigned, doing nothing";
        return written;
    }

    if(keepFull == true)
    {
        //with kpFull, the records in queue are copied to disk
        //but the queue is not flushed. This allows to keep the
        //dump files size almost equal, once the queue reaches
        //its maximum size.
        //This behavior is good for plot files, not for CSV
        //Please note, a single spectra scan line is composed by many lines
        quint16 crc16 = 0;
        foreach (scan, q)
        {
            if(isBinary)
            {
                MYDEBUG << "scan length: " << scan.length();
                buf.append(scan);
            }
            else
            {
                buf.append(QString::fromUtf8(scan).toStdString().c_str());
            }
            MYDEBUG << "Circbuf " << this << " copied record# " << written << " to file " << outPath;

            written++;
            if(numRecs > 0 &&
               written >= numRecs)
            {
                crc16 = qChecksum(buf.constData(), static_cast<uint>(buf.size()));
                MYINFO << "final checksum of " << written << " scans, total " << buf.size() << "  is: " << crc16;
                outFile.write(buf);
                outFile.write((char*)&crc16, sizeof(crc16));
                break;
            }

            QThread::yieldCurrentThread();
        }

        if(numRecs == 0)
        {
            crc16 = qChecksum(buf.constData(), static_cast<uint>(buf.size()));
            outFile.write(buf);
            outFile.write((char*)&crc16, sizeof(crc16));
        }

    }
    else
    {
        //without kpFull, the records in queue are flushed on the disk
        //leaving the queue empty. This behavior is good for CSV files
        while(q.empty() == false)
        {
            scan = q.dequeue();
            if(isBinary)
            {
                buf.append(scan);
            }
            else
            {
                buf.append(QString::fromUtf8(scan).toStdString().c_str());
            }

            MYDEBUG << "Circbuf " << this << " extracted oldest record# " << written << " to file " << outPath;
            written++;
            if(numRecs > 0 &&
               written >= numRecs)
            {
                quint16 crc16 = qChecksum(buf.constData(), static_cast<uint>(buf.size()));
                outFile.write(buf);
                outFile.write((char*)&crc16, sizeof(crc16));
                break;
            }
            QThread::yieldCurrentThread();
        }
        MYDEBUG << "Circbuf " << this << " queue empty";
        full = false;
    }

    outFile.flush();
    return written;
}

