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






*******************************************************************************/

#ifndef DBIF_H
#define DBIF_H


#include <QtCore>
#include <QtSql>


///
/// \brief The E_SESS_END_CAUSE enum
///
enum E_SESS_END_CAUSE
{
    SEC_NONE,
    SEC_MANUAL_REQ,
    SEC_SHOTS_FINISHED,
    SEC_TOO_NOISY,
    SEC_FAULT
};

///
/// \brief The AutoData class
/// to enter automatic acquisition data in db
/// unsigned data must be converted to signed since the DB doesn't discriminate
/// between signed or unsigned
class AutoData
{
public:
    /**
     * @brief AutoData constructor
     */
    AutoData();

    /**
     * @brief AutoData copy constructor
     * @param ad object to be copied
     */
    AutoData(const AutoData& ad);

    /**
     * @brief AutoData::operator =
     * @return
     */
    //AutoData& operator=(const AutoData& ad);

    /**
     * @brief clear
     * zero/empty string initializer
     */
    void clear();

    int dailyNr;
    QString eventStatus;
    QDate utcDate;
    QTime utcTime;
    QString timestampMs;
    int revision;
    float upThr;
    float dnThr;
    float S;
    float avgS;
    float N;
    float diff;
    float avgDiff;
    int topPeakHz;
    float stdDev;
    int lastingMs;
    int lastingScans;
    int freqShift;
    int echoArea;
    int intervalArea;
    int peaksCount;
    float LOSspeed;
    int scanMs;
    float diffStart;
    float diffEnd;
    QString classification;
    QString shotName;
    QString dumpName;
};



struct BlobTuple
{
    int id;
    QString name;
};


class Settings;
class XQDir;
class DBif : public QObject
{
    Q_OBJECT

    QSqlDatabase db;

    QQueue<QSqlQuery> qq;           ///queries queue

    Settings*   as;
    int         id;                 ///latest progressive event identifier along the entire DB
    QString     lastEventStatus;    ///last event status, to detect status change and increment id
    QString     dbFilePath;         ///full path to the sqlite3 opened file
    int         sessID;             ///current session record
    bool        sessionOpened;      ///true when a session is open



protected:

    /**
     * @brief updateId
     *
     * retrieves the id of the latest event recorded in
     * automatic_data
     */
    int updateId();



public:
    explicit DBif(QString dbName, Settings* appSettings, QObject *parent = nullptr);

    ~DBif();


    /**
     * @brief DBif::setWALmode
     *
     * set SQLITE DB to WAL mode to allow concurrency with ebrow.
     * This mode is persistent, so it should be set only once,
     * when a DB file is created brand new.
     *
     */
    void setWALmode();

    /**
     * @brief getLastDailyNr
     * @return
     */
    int getLastDailyNr();

    /**
     * @brief getLastEventDate
     * @return
     */
    QDate getLastEventDate();

    /**
     * @brief setConfigData
     * @param appSettings
     * @return true if a record has been added successfully
     * in configuration table
     */
    bool setConfigData(Settings* appSettings);

    /**
     * @brief updateCfgDevices
     * @param appSettings
     * @return
     */
    bool updateCfgDevices(Settings* appSettings);

    /**
     * @brief updateCfgFFT
     * @param appSettings
     * @return
     */
    bool updateCfgFFT(Settings* appSettings);

    /**
     * @brief updateCfgNotches
     * @param appSettings
     * @return
     */
    bool updateCfgNotches(Settings* appSettings);

    /**
     * @brief updateCfgOutput
     * @param appSettings
     * @return
     */
    bool updateCfgOutput(Settings* appSettings);


    /**
     * @brief updateCfgStorage
     * @param appSettings
     * @return
     */
    bool updateCfgStorage(Settings* appSettings);

    /**
     * @brief updateCfgPrefs
     * @param appSettings
     * @return
     */
    bool updateCfgPrefs(Settings* appSettings);

    /**
     * @brief updateCfgReport
     * @param appSettings
     * @return
     */
    //bool updateCfgFilters(Settings* appSettings);

    /**
     * @brief updateCfgSite
     * @param appSettings
     * @return
     */
    //bool updateCfgSite(Settings* appSettings);

    /**
     * @brief updateCfgWaterfall
     * @param appSettings
     * @return
     */
    bool updateCfgWaterfall(Settings* appSettings);


    /**
     * @brief getConfigData
     * @param dt current revision creation datetime
     * @return current revision id
     */
    int getConfigData(QString *name, QDateTime* dt);

    /**
     * @brief appendAutoData
     * @param ad
     * @return
     */
    bool appendAutoData(AutoData* ad);

    /**
     * @brief appendShotData
     * @param id
     * @param shotName
     * @param shotData
     * @return
     */
    bool appendShotData(int id, QString& shotName, QByteArray& shotData);

    /**
     * @brief appendDumpData
     * @param id
     * @param shotName
     * @param dumpData
     * @return
     */
    bool appendDumpData(int id, QString& dumpName, QByteArray& dumpData);

    /**
     * @brief sessionStart
     * appends/opens a new session record in automatic_session table
     * A session is intended as a period of time when the acquisition is active
     * with automatic captures enabled.
     *
     * @return true if ok
     */
    bool sessionStart();

    /**
     * @brief sessionUpdate
     * updates the session counters
     * @param upTime up time in minutes
     * @return true if ok
     */
    bool sessionUpdate();

    /**
     * @brief sessionEnd
     * closes a session record. This can happen due to:
     * 1-manual request from GUI
     * 2-shots counter reached zero
     * 3-too noisy (acq remains active but session is closed)
     *
     * @return true if ok
     */
    bool sessionEnd(E_SESS_END_CAUSE ec);

    /**
     * @brief flushSessions
     * clears all the session data
     *
     * @return true if ok
     */
    bool deleteExpiredSessions(int sessionsLastingDays);

    /**
     * @brief flushAutoData
     * @return
     */
    bool flushAutoData();

    /**
     * @brief flushShots
     * @return
     */
    bool flushShots();

    /**
     * @brief flushDumps
     * @return
     */
    bool flushDumps();

    /**
     * @brief peakRollback
     * @return
     * deletes the last record in automatic_data
     * because the last recorded event has been prolonged
     * and the peak and fall records need to be recomputed
     */
    bool peakRollback();

    /**
     * @brief getFirstUnclassifiedAutoData
     * @param id
     * @param ad
     * @return
     */
    int getFirstUnclassifiedAutoData(int &id, QString &evStatus, AutoData *ad);

    /**
     * @brief getShotsToArchive
     * @param shotList
     * @return
     */
    int getShotsToArchive( QList<BlobTuple>& shotList );

    /**
     * @brief getDumpsToArchive
     * @param dumpList
     * @return
     */
    int getDumpsToArchive( QList<BlobTuple>& dumpList );

    /**
     * @brief deleteExpiredBlobs
     * @param blobsLastingDays
     * @return
     */
    bool deleteExpiredBlobs(int blobsLastingDays);

    /**
     * @brief deleteExpiredData
     * @param dataLastingDays
     * @return
     */
    bool deleteExpiredData(int dataLastingDays);

    /**
     * @brief classifyEvent
     * @param id
     * @param classification
     * @return
     */
    bool classifyEvent(int id, QString& classification);

    /**
     * @brief fixShotName
     * @param id
     * @param newName
     * @return
     */
    bool fixShotName(int id, QString& newName);

    /**
     * @brief fixDumpName
     * @param id
     * @param newName
     * @return
     */
    bool fixDumpName(int id, QString& newName);

    /**
     * @brief createArchivedDaysView
     * @return
     */
    bool createArchivedDaysView(bool force);

    /**
     * @brief createHourlyPartialViews
     *
     * creates 24 views to split automatic_data in
     * 1hr sections
     *
     */
    bool createHourlyPartialViews(QStringList& kindList, bool force);

    /**
     * @brief createDailyByHourHourRaw
     * @return
     */
    bool createDailyByHourView(QStringList& kindList, bool force);

    /**
     * @brief createTenMinPartialViews
     *
     * creates 24 views to split automatic_data in
     * 10min sections
     *
     */
    bool createTenMinPartialViews(QStringList& kindList, bool force);

    /**
     * @brief createDailyByTenMinView
     * @param kind
     * @return
     */
    bool createDailyByTenMinView(QStringList& kindList, bool force);

    /**
     * @brief createDailyTotalsView
     * @param kindList
     * @param force
     * @return
     */
    bool createDailyTotalsView(QStringList& kindList, bool force);

    /**
     * @brief createAutoDataUnclassifiedView
     * @return
     */
    bool createAutoDataUnclassifiedView(bool force);

    /**
     * @brief createAutoDataTotalsByClass
     * @param force
     * @return
     */
    bool createAutoDataTotalsByClass(bool force);

    /**
     * @brief extractShotData
     * @param id
     * @param shotName
     * @param shotData
     * @return
     */
    bool extractShotData(int id, QString& shotName, QByteArray& shotData);


    /**
     * @brief extractDumpData
     * @param id
     * @param dumpName
     * @param dumpData
     * @return
     */
    bool extractDumpData(int id, QString& dumpName, QByteArray& dumpData);

    /**
     * @brief getShotsFromArchive
     * @param from
     * @param to
     * @param shotList
     * @return
     */
    int getShotsFromArchive( QDate& from, QDate& to, QList<BlobTuple>& shotList );

    /**
     * @brief getDumpsFromArchive
     * @param from
     * @param to
     * @param dumpList
     * @return
     */
    int getDumpsFromArchive( QDate& from, QDate& to, QList<BlobTuple>& dumpList );

    /**
     * @brief backup
     * creates a backup snapshot of the current database
     * WARNING: must be executed in the same thread of the
     * constructor!
     * @return
     */
    bool backup(XQDir workDir);

    /**
     * @brief executePendingQueries
     * @return
     * Tries to execute all the queries stored in the queue.
     *
     * The queries that will be queued are only those
     * that are executed while acquisition is running.
     */
    bool executePendingQueries();

};

#endif // DBIF_H
