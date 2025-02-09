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
$Rev:: 380                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-12-20 08:39:47 +01#$: Date of last commit
$Id: postproc.cpp 380 2018-12-20 07:39:47Z gmbertani $
*******************************************************************************/

#include "dbif.h"
#include "control.h"
#include "waterfall.h"
#include "mainwindow.h"
#include "postproc.h"



PostProc::PostProc(Settings* appSettings, XQDir workDir)
{

    setObjectName("PostProc thread");

    MY_ASSERT( nullptr !=  appSettings )
    as = appSettings;
    wd = workDir;

    stopProc = false;

    /*
     * TODO: tricky system folders management
     * can be improved with QStandardPath but
     * there will be impacts on program installation
     * on various platforms.
     */

#ifdef NIX
    extResourcesPath = QString(AUX_PATH);
#else
    extResourcesPath = qApp->applicationDirPath();
#endif

    MYINFO << "extResourcePath=" << extResourcesPath;
}

PostProc::~PostProc()
{
}



void PostProc::run()
{
    MYINFO << "+++++ PostProc thread started";
    MYINFO << "opening db " << dbPath;

    db = new DBif(dbPath, as);
    MY_ASSERT( nullptr !=  db )

    emit status( tr("Postprocessing daily data"));
    swappingTasks();

    QDate dt = as->getLastDBsnapshot();
    int diff = dt.daysTo( as->currentUTCdate() );
    if(diff >= as->getDBsnap())
    {
        //if dbSnap = 0 no backup is made

        emit status( tr("Backing up database"));

        //db->backup(wd);
        startBackupProcess();
        as->setLastDBsnapshot();
    }
    stopProc = false;
    MYINFO << "+++++ PostProc thread terminated";

}

void PostProc::startBackupProcess()
{

    QProcess vacuumProcess;
    QString program = QCoreApplication::applicationFilePath();

    MYINFO << "Starting vacuum external process...";
    vacuumProcess.start(program, QStringList() << "-x" << "-s" << as->getConfigName());

    if (!vacuumProcess.waitForStarted())
    {
        MYWARNING << "Vacuum process failed to start:" << vacuumProcess.errorString();
        return;
    }

    MYINFO << "Vacuum process launched, PID=" << vacuumProcess.processId();
    as->setVacuuming(true);

    if (!vacuumProcess.waitForFinished(-1))  //unlimited wait for vacuum process ending
    {
        MYWARNING << "Vacuum process failed:" << vacuumProcess.errorString();
    }

    MYINFO << "Vacuum process finished.";
    as->setVacuuming(false);

}

bool PostProc::swappingTasks()
{
    int recs = 0;

    //remove expired shots and dumps from db first,
    //to make room for new blobs


    int daysForData = as->getDataLasting();
    if (daysForData)
    {
        MYINFO << "Removing expired data and sessions (older than " << daysForData << " days)";
        db->deleteExpiredSessions(daysForData);
        db->deleteExpiredData(daysForData);
    }

    int daysForImages = as->getImgLasting();
    if (daysForImages)
    {
        MYINFO << "Removing expired shots and dumps (older than " << daysForImages << " days)";
        db->deleteExpiredBlobs(daysForImages);
    }



    wd.setCurrent( wd.path() );
    MYINFO << "current folder: " << wd.currentPath();

    //load new blobs in db
    QList<BlobTuple> blobList;
    bool ok = false;
    recs = db->getShotsToArchive(blobList);
    MYINFO << "got " << recs << " shots to store";
    for(BlobTuple bt : blobList)
    {
        if(bt.name.isEmpty() || !QFile::exists(bt.name))
        {
            continue;
        }

        MYINFO << "archiving#" << bt.id << " " << bt.name << " as blob in DB";
        QFile shot(bt.name);
        if (shot.open(QIODevice::ReadOnly))
        {
            QByteArray shotByteArray = shot.readAll();
            ok = db->appendShotData(bt.id, bt.name, shotByteArray);
            shot.close();
            if(ok)
            {
                //after archiving in DB, removes the original file
                ok = shot.remove();
                if(!ok)
                {
                    MYWARNING << "Failed removing file " << bt.name << " after archiving";
                }
            }
            else
            {
                MYWARNING << "Failed archiving " << bt.name;
            }
        }
        else
        {
            MYWARNING << "Failed opening " << bt.name;
        }
    }
    blobList.clear();
    recs = db->getDumpsToArchive(blobList);
    MYINFO << "got " << recs << " dumps to store";
    for(BlobTuple bt : blobList)
    {
        if(bt.name.isEmpty() || !QFile::exists(bt.name))
        {
            continue;
        }
        MYINFO << "archiving#" << bt.id << " " << bt.name << " as blob in DB";
        QFile dump(bt.name);
        if (dump.open(QIODevice::ReadOnly))
        {
            //the QByteArray is compressed before archiving
            QByteArray dumpByteArray = qCompress(dump.readAll());
            ok = db->appendDumpData(bt.id, bt.name, dumpByteArray);
            dump.close();
            if(ok)
            {
                //after archiving in DB, removes the original file
                ok = dump.remove();
                if(!ok)
                {
                    MYWARNING << "Failed removing file " << bt.name << " after archiving";
                }
            }
            else
            {
                MYWARNING << "Failed archiving " << bt.name;
            }
        }
        else
        {
            MYWARNING << "Failed opening " << bt.name;
        }
    }
    blobList.clear();

#if 0
    //only to test data extraction

    ok = false;
    QDate from(2022, 5, 1);
    QDate to = as->currentUTCdate();
    recs = db->getShotsFromArchive(from, to, blobList);
    MYINFO << "got " << recs << " stored shots to extract";
    for(BlobTuple bt : blobList)
    {
        MYINFO << "extracting#" << bt.id << " " << bt.name << " to PNG";
        QFile shot(bt.name);
        if (shot.open(QIODevice::WriteOnly))
        {
            QByteArray shotByteArray;
            ok = db->extractShotData(bt.id, bt.name, shotByteArray);
            if(ok)
            {
                //after extraction, saves the data as PNG file
                shot.write(shotByteArray);
                shot.close();
            }
            else
            {
                MYWARNING << "Failed extracting " << bt.name;
                shot.close();
                shot.remove();
            }
        }
        else
        {
            MYWARNING << "Failed opening " << bt.name;
        }
    }
    blobList.clear();

    ok = false;
    recs = db->getDumpsFromArchive(from, to, blobList);
    MYINFO << "got " << recs << " stored dumps to extract";
    for(BlobTuple bt : blobList)
    {
        MYINFO << "extracting#" << bt.id << " " << bt.name << " to DAT";
        QFile dump(bt.name);
        if (dump.open(QIODevice::WriteOnly))
        {
            QByteArray dumpByteArrayZ;
            ok = db->extractDumpData(bt.id, bt.name, dumpByteArrayZ);
            if(ok)
            {
                //after extraction, uncompress the data
                QByteArray dumpByteArray = qUncompress(dumpByteArrayZ);
                //saves the data as DAT file
                dump.write(dumpByteArray);
                dump.close();
            }
            else
            {
                MYWARNING << "Failed extracting " << bt.name;
                dump.close();
                dump.remove();
            }
        }
        else
        {
            MYWARNING << "Failed opening " << bt.name;
        }
    }
    blobList.clear();
#endif

    return true;
}


uint PostProc::parseEventDateFromName(QString fileName, const QString& prefix, const QString& ext,
    const QString& cfg, QDate& fileDate )
{
    int index = 0;
    uint eventNr = 0;
    QString sect;

    index = fileName.contains(prefix);
    if (index != 1)
    {
        return eventNr;
    }

    //remove the prefix:
    fileName.remove(prefix);

    //the CSV name must contain the actual config name
    index = fileName.contains(cfg);
    if (index < 1)
    {
        return eventNr;
    }

    //then removes the config name
    fileName.remove(cfg);

    //the _automatic_ flag
    fileName.remove(tr("_automatic_"));

    //and the extension
    fileName.remove(ext);

    //now splits by underscores:
    QStringList sects = fileName.split('_');
    if(sects.count() == 1)
    {
        //the filename carries no event nr. so let's assume 1
        eventNr = 1;
    }
    else if(sects.count() == 2)
    {
        //2 fields should remain: the acquisition start datetime and the event nr.

        //parsing event nr first
        bool ok = false;
        sect = sects.at(1);
        eventNr = sect.toUInt(&ok);
        MY_ASSERT(ok == true)
    }

    sect = sects.at(0);
    QStringList dateSects = sect.split('T');
    sect = dateSects.at(0);
    fileDate = QDate::fromString(sect, Qt::ISODate);
    MY_ASSERT(fileDate.isValid() == true)
    return eventNr;
}





