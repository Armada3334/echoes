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


********************************************************************************/

#include "settings.h"
#include "xqdir.h"
#include "dbif.h"

static int connProg = 0;

AutoData::AutoData()
{
    clear();

}

AutoData::AutoData(const AutoData& ad)
{
    dailyNr         = ad.dailyNr;
    eventStatus     = ad.eventStatus;
    utcDate         = ad.utcDate;
    utcTime         = ad.utcTime;
    timestampMs     = ad.timestampMs;
    revision        = ad.revision;
    upThr           = ad.upThr;
    dnThr           = ad.dnThr;
    S               = ad.S;
    avgS            = ad.avgS;
    N               = ad.N;
    diff            = ad.diff;
    avgDiff         = ad.avgDiff;
    topPeakHz       = ad.topPeakHz;
    stdDev          = ad.stdDev;
    lastingMs         = ad.lastingMs;
    freqShift       = ad.freqShift;
    echoArea        = ad.echoArea;
    intervalArea    = ad.intervalArea;
    peaksCount      = ad.peaksCount;
    LOSspeed        = ad.LOSspeed;
    diffStart       = ad.diffStart;
    diffEnd         = ad.diffEnd;
    classification  = ad.classification;
    shotName        = ad.shotName;
    dumpName        = ad.dumpName;
}

void AutoData::clear()
{
    dailyNr = 0;
    eventStatus.clear();
    utcDate.setDate(1970,1,1);
    utcTime.setHMS(0,0,0);
    timestampMs.clear();

    revision        = 0;
    upThr           = 0.0F;
    dnThr           = 0.0F;
    S               = 0.0F;
    avgS            = 0.0F;
    N               = 0.0F;
    diff            = 0.0F;
    avgDiff         = 0.0F;
    topPeakHz       = 0;
    stdDev          = 0.0F;
    lastingMs         = 0;
    freqShift       = 0;
    echoArea        = 0;
    intervalArea    = 0;
    peaksCount      = 0;
    diffStart       = 0.0F;
    diffEnd         = 0.0F;
    LOSspeed        = 0.0F;



    classification.clear();
    shotName.clear();
    dumpName.clear();
}

DBif::DBif(QString dbPath, Settings *appSettings, QObject *parent) : QObject(parent)
{
    as = appSettings;

    QString connName = QString("ECHOES_CONN#%1").arg(++connProg);

    db = QSqlDatabase::addDatabase("QSQLITE", connName);

    db.setDatabaseName( dbPath );
    bool ok = db.open();
    if(!ok)
    {
        MYFATAL("Unable to open database %s", dbPath.toLatin1().constData());
    }

    MYINFO << "DBif connection = " << db;

    dbFilePath = dbPath;
    lastEventStatus = "Fall";
    id = updateId();  
    sessID = 0;
    sessionOpened = false;
    /*
     * these views should already be present in db model. If not, set force to true
     * and run once, then turn it to false again
    */
    bool force = false;

    //these will be replaced later 
    createAutoDataUnclassifiedView(force);
    createArchivedDaysView(force);
    createAutoDataTotalsByClass(force);

}

DBif::~DBif()
{
    db.commit();
    db.close();
}

void DBif::setWALmode()
{
    MYINFO << __func__ << "()";
    QSqlQuery r(db);

    bool result =  r.exec("PRAGMA journal_mode=WAL;");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << "1-PRAGMA journal_mode=WAL returned: " << e.text();
    }
    else
    {
        r.first();
        if(r.isValid())
        {
            QString ret = r.value(0).toString();
            if ( ret != "wal")
            {
                 MYWARNING << "DB is working in " << ret << " mode, instead of wal - this could cause cuncurrency issues when ebrow runs";
            }
            else
            {
                MYINFO << "DB in WAL (cuncurrent) mode";
            }
        }
        else
        {
            QSqlError e = r.lastError();
            MYCRITICAL << "2-PRAGMA journal_mode=WAL returned: " << e.text();
        }
    }

    MYINFO << "Setting WAL size to " << MAX_DB_WAL_PAGES << " pages max";
    QString s;
    s = QString("PRAGMA wal_autocheckpoint = %1;").arg(MAX_DB_WAL_PAGES);
    result = r.exec(QString(s));
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << "3-" << s << " returned: " << e.text();
    }
    else
    {
        r.first();
        if(r.isValid())
        {
            bool ok;
            int ret = r.value(0).toInt(&ok);
            if ( ret != MAX_DB_WAL_PAGES )
            {
                MYWARNING << "wal_autocheckpoint is still " << ret << " pages!";
            }
            else
            {
                MYINFO << "wal_autocheckpoint is now " << ret << " pages";
            }
        }
        else
        {
            QSqlError e = r.lastError();
            MYCRITICAL << "4-" << s << " returned: " << e.text();
        }
    }


/*
    MYINFO << "Setting auto_vacuum by 10 pages steps";
    result = r.exec("PRAGMA auto_vacuum = INCREMENTAL;");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << "3-PRAGMA auto_vacuum = INCREMENTAL returned: " << e.text();
    }

    result = r.exec("PRAGMA incremental_vacuum(10);");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << "4-PRAGMA incremental_vacuum(10) returned: " << e.text();
    }
*/
}

int DBif::updateId()
{
    QSqlQuery r(db);
    r.prepare( "SELECT MAX(ROWID), id, event_status FROM automatic_data" );
    bool result = r.exec();
    if (result)
    {

        r.first();
        if(r.isValid() == true)
        {
            id = r.value(1).toInt(&result);
            lastEventStatus = r.value(2).toString();
            if(result)
            {
                return id;
            }
        }
    }
    return 1;
}

bool DBif::setConfigData(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO configuration (id, name, creation) VALUES (?,?, datetime('now', 'utc'))" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getConfigName() );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING << __func__ << "() returned: " << e.text();
    }
    return result;
}

bool DBif::updateCfgDevices(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO cfg_devices ( id, antenna, bandwidth, bw_range, device, freq_range, gain, ppm_error, sample_rate, sr_range, tune)"
               "VALUES (?,?,?,?,?,?,?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getAntenna() );
    r.addBindValue( as->getBandwidth() );
    r.addBindValue( as->getBandwidthRange() );
    r.addBindValue( as->getDevice() );
    r.addBindValue( as->getFreqRange() );
    r.addBindValue( as->getGain() );
    r.addBindValue( as->getError() );
    r.addBindValue( as->getSampleRate() );
    r.addBindValue( as->getSampleRateRange() );
    r.addBindValue( as->getTune() );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING << __func__ << "() returned: " << e.text();
    }
    return result;
}

bool DBif::updateCfgFFT(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO cfg_fft (id, ds_bandwidth, resolution, bypass, fft_window)"
               "VALUES (?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getDsBandwidth() );
    r.addBindValue( as->getResolution() );
    r.addBindValue( as->getSsBypass() );
    r.addBindValue( as->getWindow() );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING<< __func__ << "() returned: " << e.text();
    }
    return result;
}

bool DBif::updateCfgNotches(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    QList<Notch> notchList = as->getNotches();
    bool result = false;
    int notchId = 1;
    for(Notch& n : notchList)
    {
        r.prepare( "INSERT INTO cfg_notches (id, notchId, bandwidth, begin_hz, end_hz, frequency)"
                   "VALUES (?,?,?,?,?,?)" );
        r.addBindValue( as->getRTSrevision() );
        r.addBindValue( notchId );
        r.addBindValue( n.width );
        r.addBindValue( n.begin );
        r.addBindValue( n.end );
        r.addBindValue( n.freq );
        result =  r.exec();

        if(!result)
        {
            QSqlError e = r.lastError();
            MYWARNING << __func__ << "() returned: " << e.text() << " while inserting notchId#" << notchId;
        }
        notchId++;
    }
    return result;
}

bool DBif::updateCfgOutput(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO cfg_output (id, acq_mode, avgd_scans, gen_dumps, gen_sshots, join_time, max_shots, "
               "rec_time, interval, dn_threshold, up_threshold, thr_behavior, shot_freq_range, shot_after, noise_limit, palette)"
               "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getAcqMode() );
    r.addBindValue( as->getAvgdScans() );
    r.addBindValue( as->getDumps() );
    r.addBindValue( as->getScreenshots() );
    r.addBindValue( as->getJoinTime() );
    r.addBindValue( as->getShots() );
    r.addBindValue( as->getRecTime() );
    r.addBindValue( as->getInterval() );
    r.addBindValue( as->getDnThreshold() );
    r.addBindValue( as->getUpThreshold() );
    r.addBindValue( as->getThresholdsBehavior() );
    r.addBindValue( as->getDetectionRange() );
    r.addBindValue( as->getAfter() );
    r.addBindValue( as->getNoiseLimit() );
    r.addBindValue( as->getPalette() );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING << __func__ << "() returned: "  << e.text();
    }
    return result;
}


bool DBif::updateCfgStorage(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO cfg_storage ( id, data_lasting, blob_lasting, min_free, erase_logs, erase_shots, db_snap, overwrite_snap)"
               "VALUES (?,?,?,?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getDataLasting() );
    r.addBindValue( as->getImgLasting() );
    r.addBindValue( as->getMinFree() );
    r.addBindValue( as->getEraseLogs() );
    r.addBindValue( as->getEraseShots() );
    r.addBindValue( as->getDBsnap() );
    r.addBindValue( as->getOverwriteSnap() );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING << __func__ << "() returned: "  << e.text();
    }
    return result;
}

bool DBif::updateCfgPrefs(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    r.prepare( "INSERT INTO cfg_prefs ( id, dbfs_gain, dbfs_offset, direct_buffers, fault_sound, ping_sound, "
               "main_geometry, db_ticks, hz_ticks, sec_ticks, show_tooltips, utc_delta, server_addr, server_port, max_event_lasting, echoes_ver )"
               "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getDbfsGain() );
    r.addBindValue( as->getDbfsOffset() );
    r.addBindValue( false ); //was getDirectBuffers()
    r.addBindValue( as->getFaultSound() );
    r.addBindValue( as->getPing() );
    r.addBindValue( as->getMainGeometry() );
    r.addBindValue( as->getDbfs() );
    r.addBindValue( as->getHz() );
    r.addBindValue( as->getSec() );
    r.addBindValue( as->getTooltips() );
    r.addBindValue( as->getUTCdelta() );
    r.addBindValue( as->getServerAddress().toString() );
    r.addBindValue( as->getServerPort() );
    r.addBindValue( as->getMaxEventLasting() );
    r.addBindValue( APP_VERSION );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING << __func__ << "() returned: "  << e.text();
    }
    return result;
}

bool DBif::updateCfgWaterfall(Settings* appSettings)
{
    QSqlQuery r(db);

    MY_ASSERT(appSettings != nullptr)
    as = appSettings;
    int zoomLowBound, zoomHighBound;

    as->getZoomedRange( zoomLowBound, zoomHighBound );

    r.prepare( "INSERT INTO cfg_waterfall ( id, brightness, contrast, eccentricity, window_geometry, pane_geometry, freq_offset, "
               "plot_a, plot_d, plot_l, plot_n, plot_s, plot_u, power_offset, power_zoom, freq_zoom, zoom_high_bound, zoom_low_bound )"
               "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)" );
    r.addBindValue( as->getRTSrevision() );
    r.addBindValue( as->getBrightness() );
    r.addBindValue( as->getContrast() );
    r.addBindValue( as->getEccentricity() );
    r.addBindValue( as->getWwGeometry() );
    r.addBindValue( as->getWwPaneGeometry() );
    r.addBindValue( as->getOffset() );
    r.addBindValue( as->getPlotA() );
    r.addBindValue( as->getPlotD() );
    r.addBindValue( as->getPlotL() );
    r.addBindValue( as->getPlotN() );
    r.addBindValue( as->getPlotS() );
    r.addBindValue( as->getPlotU() );
    r.addBindValue( as->getPowerOffset() );
    r.addBindValue( as->getPowerZoom() );
    r.addBindValue( as->getZoom() );
    r.addBindValue( zoomHighBound );
    r.addBindValue( zoomLowBound );
    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYWARNING<< __func__ << "() returned: "  << e.text();
    }
    return result;
}



int DBif::getConfigData(QString* name, QDateTime* dt)
{
    MYINFO << __func__;
    Q_CHECK_PTR(dt);
    QSqlQuery r(db);
    r.prepare( "SELECT max(id), name, creation FROM configuration" );
    bool result = r.exec();
    if (result)
    {
        QList<QList<QVariant>*>* rows = new QList<QList<QVariant>*>();
        Q_CHECK_PTR(rows);

        r.first();
        if(r.isValid() == true)
        {
            *dt =  r.value(2).toDateTime();
            *name = r.value(1).toString();
            int revision = r.value(0).toInt(&result);
            if(result)
            {
                return revision;
            }
        }
    }
    return -1;
}

bool DBif::peakRollback()
{
    QSqlQuery r(db);

    r.prepare( "DELETE FROM automatic_data WHERE ROWID=(SELECT MAX(ROWID) FROM automatic_data) AND event_status <> \"Raise\" " );
    qq.push_back(r);
    bool result = executePendingQueries();
    return result;
}

bool DBif::flushAutoData()
{
    QSqlQuery r(db);

    r.prepare( "DELETE FROM automatic_data" );
    qq.push_back(r);
    bool result = executePendingQueries();
    return result;
}


bool DBif::flushShots()
{
    QSqlQuery r(db);

    r.prepare( "DELETE FROM automatic_shots" );
    qq.push_back(r);
    bool result = executePendingQueries();
    return result;
}


bool DBif::flushDumps()
{
    QSqlQuery r(db);

    r.prepare( "DELETE FROM automatic_dumps" );
    qq.push_back(r);
    bool result = executePendingQueries();
    return result;
}


bool DBif::executePendingQueries()
{
    QSqlQuery r(db);

    bool result = false;
    MYINFO << __func__ << "() there are " << qq.count() << " queued queries to execute";

    if(as->isVacuuming())
    {
        MYWARNING << "vacuuming in progress, this query will be retried later";
        return result;
    }

    while(qq.count() > 0)
    {
        r = qq.first();
        QString rs = r.lastQuery();
        int argsIndex = rs.indexOf('?');
        rs = rs.left(argsIndex-1);
        MYINFO << __func__ << "() trying: " << rs;
        rs = "(";
        QList<QVariant> list = r.boundValues().values();
        for (int i = 0; i < list.size(); ++i)
        {
            rs.append(list.at(i).toString().toUtf8().data());
            if (i < (list.size() - 1))
            {
                rs.append(", ");
            }
        }
        MYINFO << rs ;

        result =  r.exec();
        if(!result)
        {
            QSqlError e = r.lastError();

            //in case of temporary unavailability of the DB
            //the entry is queued for a later retry.
            //If the queue grows over MAX_QUEUED_QUERIES
            //the acquisition must stop

            if(qq.count() > MAX_QUEUED_QUERIES)
            {
                MYWARNING << "Max number of queued queries exceeded - TODO: stop acquisition";
                break;
            }


            if(e.text().contains("database is locked"))
            {
                MYWARNING << __func__ << "() returned: "  << e.text() << " this query will be retried later";
                break;
            }
            else
            {
                MYWARNING << __func__ << "() query returned an unrecoverable error "  << e.text();
                qq.removeFirst();
            }
        }
        else
        {
            MYINFO << __func__ << "() query successful";
            qq.removeFirst();
        }

    }

    //once loaded all the pending events in DB, flushes the wal file to free some drive space
    result = r.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }

    return result;
}


bool DBif::appendAutoData(AutoData* ad)
{
    QSqlQuery r(db);

    MY_ASSERT(ad != nullptr)

    r.prepare( "INSERT INTO automatic_data (id, daily_nr, event_status, utc_date, utc_time, timestamp_ms, revision, up_thr, dn_thr, S, avgS, N, diff, avg_diff, "
               "top_peak_hz, std_dev, lasting_ms, lasting_scans, freq_shift, echo_area, interval_area, peaks_count, LOS_speed, scan_ms, "
               "diff_start, diff_end, classification, shot_name, dump_name )"
               "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)" );

    if(ad->eventStatus == "Raise" && lastEventStatus == "Fall")
    {
        id++;
    }
    r.addBindValue( id );
    r.addBindValue( ad->dailyNr );
    r.addBindValue( ad->eventStatus );
    r.addBindValue( ad->utcDate );
    r.addBindValue( ad->utcTime );
    r.addBindValue( ad->timestampMs );
    r.addBindValue( ad->revision );
    r.addBindValue( ad->upThr );
    r.addBindValue( ad->dnThr );
    r.addBindValue( ad->S );
    r.addBindValue( ad->avgS );
    r.addBindValue( ad->N );
    r.addBindValue( ad->diff );
    r.addBindValue( ad->avgDiff );
    r.addBindValue( ad->topPeakHz );
    r.addBindValue( ad->stdDev );
    r.addBindValue( ad->lastingMs );
    r.addBindValue( ad->lastingScans );
    r.addBindValue( ad->freqShift );
    r.addBindValue( ad->echoArea );
    r.addBindValue( ad->intervalArea );
    r.addBindValue( ad->peaksCount );
    r.addBindValue( ad->LOSspeed );
    r.addBindValue( ad->scanMs );
    r.addBindValue( ad->diffStart );
    r.addBindValue( ad->diffEnd );
    r.addBindValue( ad->classification );
    r.addBindValue( ad->shotName );
    r.addBindValue( ad->dumpName );

    qq.push_back(r);
    lastEventStatus = ad->eventStatus;
    return executePendingQueries();

}

bool DBif::appendShotData(int id, QString& shotName, QByteArray& shotData)
{
    QSqlQuery r(db);
    r.prepare( "INSERT INTO automatic_shots (id, shot_name, shot_data) VALUES (?,?,?)" );
    r.addBindValue(id);
    r.addBindValue(shotName);
    r.addBindValue(shotData);
    qq.push_back(r);
    return executePendingQueries();
}


bool DBif::appendDumpData(int id, QString& dumpName, QByteArray& dumpData)
{
    QSqlQuery r(db);
    r.prepare( "INSERT INTO automatic_dumps (id, dump_name, dump_data) VALUES (?,?,?)" );
    r.addBindValue(id);
    r.addBindValue(dumpName);
    r.addBindValue(dumpData);
    qq.push_back(r);
    return executePendingQueries();
}

bool DBif::extractShotData(int id, QString& shotName, QByteArray& shotData)
{
    QSqlQuery r(db);
    r.prepare( "SELECT id, shot_name, shot_data FROM automatic_shots WHERE id=? OR shot_name=?" );
    r.addBindValue(id);
    r.addBindValue(shotName);
    bool result = r.exec();
    if (result)
    {
        r.first();
        if(r.isValid())
        {
            bool ok = false;
            int myId = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            QString myShotName = r.value(1).toString();
            shotData = r.value(2).toByteArray();

            //once extracted, the record is removed from automatic_dumps
            r.prepare( "DELETE FROM automatic_shots WHERE id=?" );
            r.addBindValue(myId);
            qq.push_back(r);
            result = executePendingQueries();
        }
    }
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
    }
    return result;
}


bool DBif::extractDumpData(int id, QString& dumpName, QByteArray& dumpData)
{
    QSqlQuery r(db);
    r.prepare( "SELECT id, dump_name, dump_data FROM automatic_dumps WHERE id=? OR dump_name=?" );
    r.addBindValue(id);
    r.addBindValue(dumpName);
    bool result = r.exec();
    if (result)
    {
        r.first();
        if(r.isValid())
        {
            bool ok = false;
            int myId = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            QString myDumpName = r.value(1).toString();
            dumpData = r.value(2).toByteArray();

            //once extracted, the record is removed from automatic_dumps
            r.prepare( "DELETE FROM automatic_dumps WHERE id=?" );
            r.addBindValue(myId);
            qq.push_back(r);
            result = executePendingQueries();
            return result;
        }
    }
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
    }
    return result;
}



int DBif::getShotsToArchive( QList<BlobTuple>& shotList )
{
    QSqlQuery r(db);

    QString query=QString("SELECT id,shot_name FROM automatic_data "
           "WHERE shot_name IS NOT NULL "
           "AND event_status='Fall' "
           "AND utc_date >= DATE('now','-%1 days')"
           " EXCEPT SELECT id,shot_name FROM automatic_shots").arg(as->getDataLasting());

    bool result = r.exec(query);
    if (result)
    {
        r.first();
        while(r.isValid())
        {
            bool ok = false;
            struct BlobTuple bt;
            bt.id = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            bt.name = r.value(1).toString();
            shotList.append(bt);
            r.next();
        }
    }
    return shotList.count();
}


int DBif::getDumpsToArchive( QList<BlobTuple>& dumpList )
{
    QSqlQuery r(db);

    QString query=QString("SELECT id,dump_name FROM automatic_data "
           "WHERE dump_name IS NOT NULL "
           "AND event_status='Fall' "
           "AND utc_date >= DATE('now','-%1 days')"
           " EXCEPT SELECT id,dump_name FROM automatic_dumps").arg(as->getDataLasting());
    r.exec(query);

    bool result = r.exec();
    if (result)
    {
        r.first();
        while(r.isValid())
        {
            bool ok = false;
            struct BlobTuple bt;
            bt.id = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            bt.name = r.value(1).toString();
            dumpList.append(bt);
            r.next();
        }
    }

    return dumpList.count();
}

int DBif::getShotsFromArchive( QDate& from, QDate& to, QList<BlobTuple>& shotList )
{
    QSqlQuery r(db);

    QString query=QString(
           "SELECT A.id,  B.utc_date, A.shot_name "
           "FROM automatic_shots A, automatic_data B "
           "WHERE B.shot_name IS NOT NULL "
           "AND A.id = B.id "
           "AND B.event_status = 'Fall' "
           "AND B.utc_date >= DATE('%1') "
           "AND B.utc_date <= DATE('%2')").arg(from.toString("yyyy-MM-dd")).arg(to.toString("yyyy-MM-dd"));


    bool result = r.exec(query);
    if (result)
    {
        r.first();
        while(r.isValid())
        {
            bool ok = false;
            struct BlobTuple bt;
            bt.id = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            bt.name = r.value(2).toString();
            shotList.append(bt);
            r.next();
        }
    }
    return shotList.count();
}

int DBif::getDumpsFromArchive( QDate& from, QDate& to, QList<BlobTuple>& dumpList )
{
    QSqlQuery r(db);

    QString query=QString(
           "SELECT A.id,  B.utc_date, A.dump_name "
           "FROM automatic_dumps A, automatic_data B "
           "WHERE B.dump_name IS NOT NULL "
           "AND A.id = B.id "
           "AND B.event_status = 'Fall' "
           "AND B.utc_date >= DATE('%1') "
           "AND B.utc_date <= DATE('%2')").arg(from.toString("yyyy-MM-dd")).arg(to.toString("yyyy-MM-dd"));


    bool result = r.exec(query);
    if (result)
    {
        r.first();
        while(r.isValid())
        {
            bool ok = false;
            struct BlobTuple bt;
            bt.id = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            bt.name = r.value(2).toString();
            dumpList.append(bt);
            r.next();
        }
    }
    return dumpList.count();
}





int DBif::getFirstUnclassifiedAutoData(int& id, QString& evStatus, AutoData *ad)
{
    int retval = 0;
    Q_CHECK_PTR(ad);
    QSqlQuery r(db);
    r.prepare( "SELECT * FROM v_automatic_data_unclassified WHERE event_status=?" );
    r.addBindValue(evStatus);
    bool result = r.exec();
    if (result)
    {
        bool ok = false;
        r.first();
        if(r.isValid() == true)
        {
            id =  r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            ad->dailyNr = r.value(1).toInt(&ok);
            MY_ASSERT(ok)
            ad->eventStatus = r.value(2).toString();
            ad->utcDate = r.value(3).toDate();
            ad->utcTime = r.value(4).toTime();
            ad->timestampMs = r.value(5).toString();
            ad->revision = r.value(6).toInt(&ok);
            MY_ASSERT(ok)
            ad->upThr = r.value(7).toFloat(&ok);
            MY_ASSERT(ok)
            ad->dnThr = r.value(8).toFloat(&ok);
            MY_ASSERT(ok)
            ad->S = r.value(9).toFloat(&ok);
            MY_ASSERT(ok)
            ad->avgS = r.value(10).toFloat(&ok);
            MY_ASSERT(ok)
            ad->N = r.value(11).toFloat(&ok);
            MY_ASSERT(ok)
            ad->diff = r.value(12).toFloat(&ok);
            MY_ASSERT(ok)
            ad->avgDiff = r.value(13).toFloat(&ok);
            MY_ASSERT(ok)
            ad->topPeakHz = r.value(14).toInt(&ok);
            MY_ASSERT(ok)
            ad->stdDev = r.value(15).toFloat(&ok);
            MY_ASSERT(ok)
            ad->lastingMs = r.value(16).toInt(&ok);
            MY_ASSERT(ok)
            ad->lastingScans = r.value(17).toInt(&ok);
            MY_ASSERT(ok)
            ad->freqShift = r.value(18).toInt(&ok);
            MY_ASSERT(ok)
            ad->echoArea  = r.value(19).toInt(&ok);
            MY_ASSERT(ok)
            ad->intervalArea = r.value(20).toInt(&ok);
            MY_ASSERT(ok)
            ad->peaksCount  = r.value(21).toInt(&ok);
            MY_ASSERT(ok)
            ad->LOSspeed = r.value(22).toFloat(&ok);
            MY_ASSERT(ok)
            ad->scanMs = r.value(23).toInt(&ok);
            MY_ASSERT(ok)
            ad->diffStart = r.value(24).toFloat(&ok);
            MY_ASSERT(ok)
            ad->diffEnd = r.value(25).toFloat(&ok);
            MY_ASSERT(ok)
            ad->classification = r.value(26).toString();
            ad->shotName = r.value(27).toString();
            ad->dumpName = r.value(28).toString();
            retval = ok;
        }
    }
    return retval;
}



bool DBif::deleteExpiredBlobs(int blobsLastingDays)
{
    QSqlQuery r(db);
    int id = -1;

    QString query= QString(
        "SELECT mid FROM ( "
            "SELECT max(id) as mid, shot_name, utc_date, julianday('now') - julianday(utc_date) - 1 as age "
            "FROM automatic_data "
            "WHERE age > ? AND event_status = \'Fall\'"
        ")"
    );
    r.prepare(query);
    r.addBindValue(blobsLastingDays);

    bool result = r.exec();
    if (result)
    {

        if(r.first())
        {
            bool ok = false;
            id = r.value(0).toInt(&ok);
            MY_ASSERT(ok)
            MYINFO << "About to delete the expired blobs having ID <= " << id;

            r.clear();

            r.prepare("DELETE FROM automatic_shots WHERE id<=?");
            r.addBindValue(id);
            qq.push_back(r);
            result = executePendingQueries();
            MYINFO << "done";

            r.clear();

            r.prepare("DELETE FROM automatic_dumps WHERE id<=?");
            r.addBindValue(id);
            qq.push_back(r);
            result = executePendingQueries();
            MYINFO << "done";
        }
        else
        {
            MYCRITICAL << "query validity=" << r.isValid() << " no rows returned";
        }
    }
    else
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }

    return result;
}


bool DBif::deleteExpiredData(int dataLastingDays)
{
    QSqlQuery r(db);
    QDateTime oldest = as->currentUTCdateTime();
    oldest = oldest.addDays(-dataLastingDays);

    r.prepare("DELETE FROM automatic_data WHERE utc_date < ?");

    QString oldestStr = oldest.date().toString(Qt::ISODate);
    MYINFO << "deleting data older than " << oldestStr;
    r.addBindValue( oldestStr );
    qq.push_back(r);
    bool result = executePendingQueries();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }
    return result;
}


bool DBif::deleteExpiredSessions(int sessionsLastingDays)
{
    QSqlQuery r(db);

  	//flushes the wal file to free some drive space
    bool result = r.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }

    QDateTime oldest = as->currentUTCdateTime();
    oldest = oldest.addDays(-sessionsLastingDays);
    r.prepare("DELETE FROM automatic_sessions WHERE end_dt < ?");

    r.addBindValue( oldest.toString() );
    qq.push_back(r);
    result = executePendingQueries();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }
    return result;
}

bool DBif::classifyEvent(int id, QString& classification )
{
    QSqlQuery r(db);

    r.prepare( "UPDATE automatic_data SET classification=? WHERE id=? " );
    r.addBindValue( classification );
    r.addBindValue( id );

    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }
    return result;
}


bool DBif::fixShotName(int id, QString& newName)
{
    QSqlQuery r(db);

    r.prepare( "UPDATE automatic_data SET shot_name=? WHERE id=? AND event_status='Fall'" );
    r.addBindValue( newName );
    r.addBindValue( id );

    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }
    return result;
}

bool DBif::fixDumpName(int id, QString& newName)
{
    QSqlQuery r(db);

    r.prepare( "UPDATE automatic_data SET dump_name=? WHERE id=? AND event_status='Fall'" );
    r.addBindValue( newName );
    r.addBindValue( id );

    bool result =  r.exec();
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL<< __func__ << "() returned: "  << e.text();
    }
    return result;
}

bool DBif::createArchivedDaysView(bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query;
    view = QString("v_archived_days");
    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS SELECT DISTINCT utc_date FROM automatic_data WHERE utc_date <> DATE('now')").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT DISTINCT utc_date FROM automatic_data WHERE utc_date <> DATE('now')").arg(view);
    }
    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating " << view;
    }
    return result;
}



bool DBif::createHourlyPartialViews(QStringList& kindList, bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString dataView, countView, query;
    QString kinds = kindList.join("_");
    for(int h=0; h<24; h++)
    {
        dataView = QString("v_automatic_data_h%1_%2").arg(h,2,10,QChar('0')).arg(kinds);
        if(force)
        {
            query = QString("DROP VIEW IF EXISTS ") + dataView;
            result =  r.exec(query);
            if(!result)
            {
                QSqlError e = r.lastError();
                MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << dataView;
                break;
            }

            query = QString("CREATE VIEW %1 AS "
              "SELECT * "
              "FROM automatic_data "
              "WHERE "
              "utc_time >= time('%2:00:00') AND "
              "utc_time < time('%2:59:59') AND "
              "event_status =\'Fall\' AND "
              "utc_date < DATE(\'now\') AND (" ).arg(dataView).arg(h,2,10,QChar('0'));
        }
        else
        {
            query = QString("CREATE VIEW IF NOT EXISTS %1 AS "
              "SELECT * "
              "FROM automatic_data "
              "WHERE "
              "utc_time >= time('%2:00:00') AND "
              "utc_time < time('%2:59:59') AND "
              "event_status =\'Fall\' AND "
              "utc_date < DATE(\'now\') AND (" ).arg(dataView).arg(h,2,10,QChar('0'));
        }


        for(QString& kind : kindList)
        {
            query += "classification LIKE \'"+kind+"%\' OR ";
        }
        query.chop(3); //cuts away the last OR
        query += ");";

        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << dataView;
            break;
        }

        countView = QString("v_automatic_data_counts_h%1_%2").arg(h,2,10,QChar('0')).arg(kinds);
        if(force)
        {
            query = QString("DROP VIEW IF EXISTS ") + countView;
            result =  r.exec(query);
            if(!result)
            {
                QSqlError e = r.lastError();
                MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << countView;
                break;
            }

            query = QString("CREATE VIEW %1 AS "
              "SELECT utc_date, %3 AS hour, count(*) AS cnt "
              "FROM %2 "
              "GROUP BY utc_date; " ).arg(countView).arg(dataView).arg(h,2,10,QChar('0'));
        }
        else
        {
            query = QString("CREATE VIEW IF NOT EXISTS %1 AS "
              "SELECT utc_date, %3 AS hour, count(*) AS cnt "
              "FROM %2 "
              "GROUP BY utc_date; " ).arg(countView).arg(dataView).arg(h,2,10,QChar('0'));
        }

        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << countView;
            break;
        }

    }

    return result;
}


bool DBif::createDailyTotalsView(QStringList& kindList, bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query;
    QString kinds = kindList.join("_");
    view = QString("v_daily_totals_%1").arg(kinds);
    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS SELECT utc_date, total ").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT utc_date, total ").arg(view);
    }

    query += QString("FROM v_daily_by_hour_%1").arg(kinds);

    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << view;
        return result;
    }

    return result;
}


bool DBif::createDailyByHourView(QStringList& kindList, bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query;
    QString kinds = kindList.join("_");
    view = QString("v_daily_by_hour_%1").arg(kinds);
    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS SELECT ad.utc_date,").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT ad.utc_date,").arg(view);
    }

    for(int h=0; h<24; h++)
    {
        query += QString("IFNULL(v%1.cnt, '0') AS h%1, ").arg(h,2,10,QChar('0'));
    }
    query += QString("(");
    for(int h=0; h<24; h++)
    {
        query += QString("IFNULL(v%1.cnt, '0')+").arg(h,2,10,QChar('0'));
    }
    query.chop(1); //cuts away the last +
    query += QString(") AS total");

    query += " FROM v_archived_days ad ";
    for(int h=0; h<24; h++)
    {
        query += QString("LEFT OUTER JOIN v_automatic_data_counts_h%1_%2 v%1 ON ad.utc_date = v%1.utc_date ").arg(h,2,10,QChar('0')).arg(kinds);
    }

    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << view;
        return result;
    }

    return result;
}

bool DBif::createTenMinPartialViews(QStringList& kindList, bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString dataView, countView, query;
    QString kinds = kindList.join("_");
    for(int h=0; h<24; h++)
    {
        for(int m=0; m<60; m += 10)
        {

            dataView = QString("v_automatic_data_h%1_m%2_%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(kinds);
            if(force)
            {
                query = QString("DROP VIEW IF EXISTS ") + dataView;
                result =  r.exec(query);
                if(!result)
                {
                    QSqlError e = r.lastError();
                    MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << dataView;
                    break;
                }

                query = QString("CREATE VIEW %1 AS "
                  "SELECT * "
                  "FROM automatic_data "
                  "WHERE "
                  "utc_time >= time('%2:%3:00') AND "
                  "utc_time < time('%2:%4:59') AND "
                  "event_status =\'Fall\' AND "
                  "utc_date < DATE(\'now\') AND (" ).arg(dataView).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(m+9,2,10,QChar('0'));
            }
            else
            {
                query = QString("CREATE VIEW IF NOT EXISTS %1 AS "
                  "SELECT * "
                  "FROM automatic_data "
                  "WHERE "
                  "utc_time >= time('%2:%3:00') AND "
                  "utc_time < time('%2:%4:59') AND "
                  "event_status =\'Fall\' AND "
                  "utc_date < DATE(\'now\') AND (" ).arg(dataView).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(m+9,2,10,QChar('0'));
            }

            for(QString& kind : kindList)
            {
                query += "classification LIKE \'"+kind+"%\' OR ";
            }
            query.chop(3); //cuts away the last OR
            query += ");";

            result =  r.exec(query);
            if(!result)
            {
                QSqlError e = r.lastError();
                MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << dataView;
                break;
            }


            countView = QString("v_automatic_data_counts_h%1_m%2_%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(kinds);
            if(force)
            {
                query = QString("DROP VIEW IF EXISTS ") + countView;
                result =  r.exec(query);
                if(!result)
                {
                    QSqlError e = r.lastError();
                    MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << countView;
                    break;
                }

                query = QString("CREATE VIEW %1 AS "
                  "SELECT utc_date, %3 AS hour, %4 AS min, count(*) AS cnt "
                  "FROM %2 "
                  "GROUP BY utc_date; " ).arg(countView).arg(dataView).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
            }
            else
            {
                query = QString("CREATE VIEW IF NOT EXISTS %1 AS "
                  "SELECT utc_date, %3 AS hour, %4 AS min, count(*) AS cnt "
                  "FROM %2 "
                  "GROUP BY utc_date; " ).arg(countView).arg(dataView).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
            }

            result =  r.exec(query);
            if(!result)
            {
                QSqlError e = r.lastError();
                MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << countView;
                break;
            }

        }
    }

    return result;
}



bool DBif::createDailyByTenMinView(QStringList& kindList, bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query, join;
    QString kinds = kindList.join("_");
    for(int h=0; h<24; h++)
    {
        view = QString("v_h%1_by_ten_min_%2").arg(h,2,10,QChar('0')).arg(kinds);
        if(force)
        {
            query = QString("DROP VIEW IF EXISTS ") + view;
            result =  r.exec(query);
            if(!result)
            {
                QSqlError e = r.lastError();
                MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
                return result;
            }

            query = QString("CREATE VIEW %1 AS SELECT ad.utc_date,").arg(view);
        }
        else
        {
            query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT ad.utc_date,").arg(view);
        }

        for(int m=0; m<60; m+=10)
        {
            query += QString("IFNULL(v%1_m%2.cnt, '0') AS h%1_m%2, ").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
        }

        query.chop(2); //cuts the last comma and space on latest argument
        query += " FROM v_archived_days ad ";

        for(int m=0; m<60; m+=10)
        {
            query += QString("LEFT OUTER JOIN v_automatic_data_counts_h%1_m%2_%3 v%1_m%2 ON ad.utc_date = v%1_m%2.utc_date ").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(kinds);
        }

        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << view;
            return result;
        }
    }

    view = QString("v_daily_by_ten_min_%1").arg(kinds);
    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS SELECT ad.utc_date,").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT ad.utc_date,").arg(view);
    }

    for(int h=0; h<24; h++)
    {
        for(int m=0; m<60; m+=10)
        {
            query += QString("v%1.h%1_m%2, ").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
        }
    }

    query += "(";
    for(int h=0; h<24; h++)
    {
        for(int m=0; m<60; m+=10)
        {
            query += QString("IFNULL(v%1.h%1_m%2, '0')+").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
        }
    }
    query.chop(1); //cuts away the last +
    query += ") as totals FROM v_archived_days ad ";

    for(int h=0; h<24; h++)
    {
        join = QString("v_h%1_by_ten_min_%2 v%1").arg(h,2,10,QChar('0')).arg(kinds);
        query += QString("LEFT OUTER JOIN %2 ON ad.utc_date = v%1.utc_date ").arg(h,2,10,QChar('0')).arg(join);
    }

    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating view " << view;
        return result;
    }


    return result;
}





bool DBif::createAutoDataUnclassifiedView(bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query;
    view = QString("v_automatic_data_unclassified");

    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS SELECT DISTINCT * FROM automatic_data WHERE classification=\"\"").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS SELECT DISTINCT * FROM automatic_data WHERE classification=\"\"").arg(view);
    }

    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating " << view;
    }
    return result;
}



bool DBif::createAutoDataTotalsByClass(bool force)
{
    bool result = false;
    QSqlQuery r(db);
    QString view, query;
    view = QString("v_automatic_totals_by_classification");

    if(force)
    {
        query = QString("DROP VIEW IF EXISTS ") + view;
        result =  r.exec(query);
        if(!result)
        {
            QSqlError e = r.lastError();
            MYCRITICAL << __func__ << "() returned: " << e.text()  << " while dropping old view " << view;
            return result;
        }

        query = QString("CREATE VIEW %1 AS ").arg(view);
    }
    else
    {
        query = QString("CREATE VIEW IF NOT EXISTS %1 AS ").arg(view);
    }

    query += "SELECT DISTINCT ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%UNDER%' AND event_status = 'Fall') AS underdense, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%OVER%' AND event_status = 'Fall') AS overdense, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%FAKE CAR1%' AND event_status = 'Fall') AS fake_carrier1, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%FAKE CAR2%' AND event_status = 'Fall') AS fake_carrier2, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%FAKE ESD%' AND event_status = 'Fall') AS fake_esd, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%FAKE SAT%' AND event_status = 'Fall') AS fake_saturation, ";
    query += "(SELECT count( * ) FROM automatic_data WHERE classification LIKE '%FAKE LONG%' AND event_status = 'Fall') AS fake_too_long ";
    query += " FROM automatic_data";
    result =  r.exec(query);
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while creating " << view;
    }
    return result;
}


/*
 * v.0.58
 *
 * This task is now performed by a separate process to avoid acquisition freezings
 * (see -x command line option)
 */

bool DBif::backup(XQDir workDir)
{
    bool result = false;
    QDate today = as->currentUTCdate();
    QString dbSnapshot;
    if(as->getOverwriteSnap())
    {
        dbSnapshot = QString("snapshot_%1.sqlite3").arg(as->getConfigName());
    }
    else
    {
        dbSnapshot = QString("snapshot_%1_%2.sqlite3").arg(as->getConfigName()).arg(today.toString("yyyy-MM-dd"));
    }
    dbSnapshot = workDir.absoluteFilePath(dbSnapshot);

    QFile::remove(dbSnapshot); //delete the file in case already exists

    QSqlQuery r(db);
    MYINFO << __func__ << "vacuuming data into a database snapshot " << dbSnapshot;
    result =  r.exec( QString("VACUUM INTO '%1'").arg(dbSnapshot) );
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
        return false;
    }

    MYINFO << __func__ << "vacuuming the current database too";
    result =  r.exec("VACUUM");
    if(!result)
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: "  << e.text();
        return false;
    }
    MYINFO << __func__ << "vacuuming done";

    return true;
}


int DBif::getLastDailyNr()
{
    int dailyNr = 0;
    QSqlQuery r(db);
    QString query = QString("SELECT max(daily_nr) "
                            "FROM automatic_data "
                            "WHERE id = ( "
                            " SELECT max(id) "
                            " FROM automatic_data "
                            " WHERE utc_date == DATE('now'))");
    bool result =  r.exec(query);
    if (result)
    {
        r.first();
        if(r.isValid())
        {
            bool ok = false;
            dailyNr = r.value(0).toInt(&ok);
        }
    }
    else
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while reading last dailyNr";
    }
    return dailyNr;
}

QDate DBif::getLastEventDate()
{
    QDate qd;
    QString date = "1970-01-01";
    QSqlQuery r(db);
    QString query = QString("SELECT max(utc_date) "
                            "FROM automatic_data ");
    bool result =  r.exec(query);
    if (result)
    {
        r.first();
        if(r.isValid())
        {
            date = r.value(0).toString();
        }
    }
    else
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while reading last dailyNr";
    }

    return qd.fromString(date, "yyyy-MM-dd");
}

bool DBif::sessionStart()
{
    MYINFO << __func__;
    if(sessionOpened)
    {
        //session already opened
        return true;
    }
    QSqlQuery r(db);
    QString query = QString("SELECT max(id) FROM automatic_sessions");
    bool result =  r.exec(query);
    if (result)
    {
        r.first();
        if(r.isValid())
        {
            bool ok = false;
            sessID = r.value(0).toInt(&ok);
            if(ok)
            {
                sessID++;
            }
            else
            {
                sessID = 1;
            }
        }
    }
    else
    {
        QSqlError e = r.lastError();
        MYCRITICAL << __func__ << "() returned: " << e.text()  << " while reading last session ID";
    }

    r.prepare( "INSERT INTO automatic_sessions(id, start_dt, living_dt, end_dt, delta_min, cause) VALUES (?, datetime('now', 'utc'),NULL,NULL,0, 0)" );
    r.addBindValue(sessID);
    qq.push_back(r);
    result = executePendingQueries();

    MYINFO << __func__ << " created new session#" << sessID;
    sessionOpened = true;

    return result;

}

bool DBif::sessionUpdate()
{
    MYINFO << __func__ << " #" << sessID;
    if (!sessionOpened)
    {
        MYWARNING << "No sessions opened";
        return false;
    }

    QSqlQuery r(db);

    r.prepare( "UPDATE automatic_sessions SET living_dt=datetime('now', 'utc'), delta_min=(JULIANDAY(datetime('now', 'utc')) - JULIANDAY(start_dt)) * 1440 WHERE id=?" );
    r.addBindValue( sessID );
    qq.push_back(r);
    bool result = executePendingQueries();
    return result;
}

bool DBif::sessionEnd(E_SESS_END_CAUSE ec)
{
    sessionUpdate();
    MYINFO << __func__;
    if (!sessionOpened)
    {
        MYWARNING << "No sessions opened";
        return false;
    }

    QSqlQuery r(db);
    r.prepare( "UPDATE automatic_sessions SET end_dt=datetime('now', 'utc'), cause=? WHERE id=?" );
    r.addBindValue( static_cast<int>(ec) );
    r.addBindValue( sessID );

    qq.push_back(r);
    bool result = executePendingQueries();
    sessionOpened = false;
    return result;
}
