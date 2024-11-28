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
$Rev:: 356                      $: Revision of last commit
$Author:: gmbertani             $: Author of last commit
$Date:: 2018-11-02 19:12:07 +01#$: Date of last commit
$Id: postproc.h 356 2018-11-02 18:12:07Z gmbertani $
*******************************************************************************/

#ifndef POSTPROC_H
#define POSTPROC_H

#include <QtCore>
#include <QtGui>
//#include <QtPrintSupport>
#include "settings.h"

namespace Ui {
    class MainWindow;
}

extern bool console;

class Control;
class DBif;



class PostProc : public QThread
{
    Q_OBJECT

protected:

    Settings*       as;

    bool stopProc;

    XQDir           wd;                 ///working directory
    QDate           processDT;          ///yesterday UTC
    DBif*           db;                 ///database interface pointer

    QString dbPath;                 ///location of DB file
    QString extResourcesPath;       ///location of css file and other auxiliary files


    ///
    /// \brief parseDateFromName
    /// \param fileName
    /// \param prefix
    /// \param cfg
    /// \param start
    /// \return event nr. if defined, otherwise 1 if ok and 0 for failure
    ///
    uint parseEventDateFromName(QString fileName, const QString& prefix, const QString& ext, const QString& cfg, QDate &start);


    ///
    /// \brief swappingTasks
    /// \return
    ///
    bool swappingTasks();

    ///
    /// \brief startBackupProcess
    ///
    void startBackupProcess();

public:
    explicit PostProc(Settings* appSettings, QString& dPath, XQDir workDir);

    ~PostProc();

    void run();

    ///
    /// \brief setWdir
    /// \param wdir
    ///
    void setWdir(const XQDir& wdir)
    {
        wd = wdir;
    }

    ///
    /// \brief stop
    ///
    void stop()
    {
        stopProc = true;
    }


signals:
    ///
    /// \brief status
    /// \param msg
    /// updates status bar in MainWindow
    void status(const QString& msg);

};

#endif // POSTPROC_H
