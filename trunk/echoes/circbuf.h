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
$Rev:: 205                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-05-31 23:41:02 +02#$: Date of last commit
$Id: circbuf.h 205 2018-05-31 21:41:02Z gmbertani $
*******************************************************************************/

#ifndef CIRCBUF_H
#define CIRCBUF_H

#include <QtCore>
#include "setup.h"


class CircBuf : public QObject
{
    Q_OBJECT

    QQueue<QByteArray> q;

    int  maxRecs;           ///<buffer records nr.

    bool isBinary;           ///<buffer format)
    bool keepFull;            ///<if true, dump() doesn't empty the buffer
    bool full;              ///<true when circular buffer content reached maxRecs, self-deleting oldest records at each insertion


    QByteArray curr;           ///<scan line under construction
    QByteArray scan;           ///<completed scan line

    QRecursiveMutex  protectionMutex;    ///<generic mutex to protect against concurrent calls from Control thread


    QFile outFile;          ///<output file
    QString outPath;        ///<output file full pathname

    QTextStream txtStr;

public:

    ///
    /// \brief CircBuf
    /// \param keepFull
    /// \param append
    /// \param maxRecords
    /// \param outPath
    ///
    explicit CircBuf(bool keepFull, int maxRecords = CIRCBUF_MIN, bool binFormat = false, QString outPath = "");

    ~CircBuf() override;

    ///
    /// \brief append
    /// \param buf
    /// \return
    ///  appends text to curr without inserting it in circular buffer
    int append(QString& buf);

    ///
    /// \brief append
    /// \param format
    /// \return
    /// formats and appends text to curr without inserting it in circular buffer
    int append(const char* format, ...);

    ///
    /// \brief append
    /// \param ba
    /// \return
    ///  appends binary data to curr without inserting it in circular buffer
    int append(QByteArray& ba);

    ///
    /// \brief insert
    /// \param format
    /// \return
    ///insert a full formatted record in circular buffer
    /// discarding the oldest record in case
    /// the circular buffer reached maxRecs size.
    /// like calling append() once then commit()
    int insert(const char* format, ...);

    ///
    /// \brief setFile
    /// \param path
    /// \param append
    /// \return
    ///creates or reopens a new ASCII dump file
    int setFile(QString& path, bool append = false);

    ///
    /// \brief setFile
    /// \param path
    /// \param header
    /// \return
    ///creates or reopens new binary dump file
    int setFile(QString& path, QByteArray &header);


    ///
    /// \brief unSetFile
    /// \return
    ///closes the dump file
    int unSetFile();

    ///
    /// \brief dump
    /// \param shotName
    /// \param numRecs
    /// \return
    ///saves the oldest <numRecs> records from buffer
    ///to dump file adding the shot file name at each record
    int dump(QString shotName = "", int numRecs = 0);

    ///
    /// \brief bdump
    /// \param numRecs
    /// \return
    ///saves the oldest <numRecs> records from buffer
    ///to binary dump file
    int bdump(int numRecs = 0);



    ///
    /// \brief commit
    /// \param prepend if true, the curr record is inserted at the beginning of the queue
    /// instead of end.
    ///appends the curr record at the end of circular buffer
    void commit( bool prepend = false );

    ///
    /// \brief setMax
    /// \param maxRecords
    ///sets the circular buffer size
    void setMax(int maxRecords);

    ///
    /// \brief rollback
    /// \param recs
    /// discards the recentmost #recs records
    /// from the buffer
    void rollback(uint recs);


    ///
    /// \brief setKeepMode
    /// \param full
    ///sets the circular buffer behavior
    void setKeepMode(bool full)
    {
        keepFull = full;
    }

    ///
    /// \brief getOutPath
    /// \return
    ///current file path
    QString getOutPath()
    {
        return outPath;
    }

signals:

public slots:
};

#endif // CIRCBUF_H
