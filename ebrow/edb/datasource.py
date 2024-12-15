"""
******************************************************************************

    Echoes Data Browser (Ebrow) is a data navigation and report generation
    tool for Echoes.
    Echoes is a RF spectrograph for SDR devices designed for meteor scatter.

    Both copyright (C) 2018-2023
    Giuseppe Massimo Bertani gm_bertani(a)yahoo.it

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

"""
import json
import os
import stat
import time
import calendar
from collections import Counter
from pathlib import Path
from datetime import datetime, timedelta

import pandas as pd
import numpy as np
from math import isnan
from PyQt5.QtSql import QSqlDatabase, QSqlQuery, QSqlError, QSqlTableModel
from PyQt5.QtWidgets import QFileDialog, qApp
from PyQt5.QtCore import QDir, QDate, qUncompress, QMetaType, QResource, QFile, QByteArray

from .utilities import fpCmp, fuzzyCompare, castFloatPrecision, timestamp2sideral
from .logprint import print


class CustomDataFrame(pd.DataFrame):

    def __init__(self, df):
        super().__init__(df)
        self.numberformat = self.format

    def format(self, column, format_):
        return pd.DataFrame(self[column]).astype(float).applymap(format_.format)


class DataSource:
    connectionProg = 0

    DBFS_RANGE_MIN = -150.0
    DBFS_RANGE_MAX = 20.0

    PROGRESS_STEP = 1

    def __init__(self, parent, settings):
        self._parent = parent
        self._dsPath = None
        self._adPath = None
        self._connectName = None
        self._workingDir = None
        self._db = None  # database handle
        self._adf = None  # automatic_data dataframe
        self._sdf = None  # automatic_sessions dataframe
        self._dataChangedInMemory = False
        self._settings = settings
        self.dataReady = False

        # pd.set_option('display.precision', 4)
        # Configure Pandas to show 4-decimal floats
        # pd.options.display.float_format = '{:.4f}'.format

    def _getDailyNrFromID(self, eventId: int):
        """
        Returns the date and the daily number of the given
        event ID

        @param eventId:
        @return: dailyNr, utcDate
        """
        if self._adf is not None:
            dailyNr = \
                self._adf.loc[
                    (self._adf['id'] == eventId) & (self._adf['event_status'] == 'Fall'), 'daily_nr'].to_list()[0]
            utcDate = \
                self._adf.loc[
                    (self._adf['id'] == eventId) & (self._adf['event_status'] == 'Fall'), 'utc_date'].to_list()[0]
            if dailyNr is not None and utcDate is not None:
                # skips the rowid
                return dailyNr, utcDate
        return None, None

    def _loadAutoDataTable(self):
        """
        Loads the CSV automatic_data table as dataframe
        self._adf. If the file doesn't exist, the
        data will be taken from the DB.
        If the file exists but it's older, its data
        will be loaded first. Then, the newer events
        data will be taken from DB.
        The time reference for this is the timestamp_ms
        column.
        Possible fails:
        1-data in DB are older, despite the file date
        2-presence of duplicate IDs with different timestamps
        3-there are in json data related to expired events
          that have been removed from DB

        """
        print("_loadAutoDataTable()")
        self.dataReady = False
        if os.path.exists(self._adPath):
            # self._parent.busy(True)
            self._parent.updateStatusBar("Reading existing cache file {}".format(self._adPath))
            df = pd.read_json(self._adPath, orient='records', convert_dates=False)
            oldestRecordID = df['id'].min()
            oldestRecordDate = df['utc_date'].min()
            newestRecordID = df['id'].max()
            newestRecordDate = df['utc_date'].max()
            self._adf = df
            self._parent.updateStatusBar("Reading newer events from DB")
            # self._parent.busy(False)
            return self._loadPartialAutoDataTableFromDB(oldestRecordID, oldestRecordDate, newestRecordID,
                                                        newestRecordDate)
        self._parent.updateStatusBar("Cache file {} not existing, loading the DB to rebuild it".format(self._adPath))
        return self._loadAutoDataTableFromDB()

    def _loadPartialAutoDataTableFromDB(self, oldestRecordID: int, oldestRecordDate: str, newestRecordID: int,
                                        newestRecordDate: str):
        """
        Integrates the missed records in self._adf with fresh data from DB's automatic_data table

        @return: the id to the first new data records added
        """
        result = False

        # if false, the db is not open
        if self._db is not None:
            self._parent.busy(True)
            print("_loadPartialAutoDataTableFromDB()")
            q = QSqlQuery(self._db)
            select = "SELECT id, timestamp_ms FROM automatic_data WHERE id={} AND utc_date='{}'".format(
                oldestRecordID, oldestRecordDate)
            result = q.exec(select)
            if result:
                q.first()
                if q.isValid():
                    # if false, the first cached record is not present in DB,
                    # otherwise checks for the latest one.
                    select = "SELECT id, timestamp_ms FROM automatic_data WHERE id={} AND utc_date='{}'".format(
                        newestRecordID, newestRecordDate)
                    result = q.exec(select)
                    if result:
                        q.first()
                        if q.isValid():
                            # if false, the last cached record is not present in DB
                            # otherwise first and last event matches self._adf
                            print("automatic data table matches, ok to load the newer data")
                            select = "SELECT COUNT(*) FROM automatic_data where id > {}".format(newestRecordID)
                            result = q.exec(select)
                            if result:
                                q.first()
                                if q.isValid():
                                    # if true, there are newer events in db, otherwise not
                                    rec = q.record()
                                    totalRows = rec.field('count(*)').value()
                                    if totalRows > 0:
                                        self._parent.updateStatusBar(
                                            "loading new {} rows from database".format(totalRows))
                                        select = "SELECT * FROM automatic_data WHERE id > {} ORDER BY id ASC, event_status DESC".format(
                                            newestRecordID)
                                        result = q.exec(select)
                                        if result:
                                            self._parent.updateProgressBar(0)
                                            currentRow = 0
                                            dataDict = dict()
                                            q.first()
                                            columnList = list()
                                            firstRaiseFound = False
                                            while q.isValid():
                                                currentRow += 1
                                                qApp.processEvents()
                                                rec = q.record()
                                                if len(dataDict.keys()) == 0:
                                                    # the first row is the header
                                                    for col in range(0, rec.count()):
                                                        qApp.processEvents()
                                                        columnName = rec.fieldName(col)
                                                        columnList.append(columnName)
                                                        dataDict[columnName] = list()

                                                for col in range(0, rec.count()):
                                                    qApp.processEvents()
                                                    columnName = columnList[col]
                                                    field = rec.field(col)
                                                    val = field.value()
                                                    varType = field.type()
                                                    if varType == QMetaType.Double or varType == QMetaType.Float:
                                                        try:
                                                            val = castFloatPrecision(val, 4)
                                                        except TypeError:
                                                            print(
                                                                "fixing invalid value: id={}, col={}, value={} as zero".format(
                                                                    rec.field('id').value(), columnName, val))
                                                            val = 0

                                                    # discard the first records until finds a raising front
                                                    if columnName == 'event_status':
                                                        if firstRaiseFound is False and val != 'Raise':
                                                            continue
                                                        else:
                                                            firstRaiseFound = True
                                                    if val == '0':
                                                        val = 0
                                                    dataDict[columnName].append(val)
                                                    self._parent.updateProgressBar(currentRow, totalRows)
                                                q.next()
                                            # end while
                                            adfUpdate = pd.DataFrame(dataDict)
                                            if not adfUpdate.empty:
                                                # joins the new events to adf table
                                                # adfUpdate.set_index('id', inplace=True)
                                                # adfUpdate.sort_index(inplace=True, ascending=True)

                                                # calculate the sidereal times for the new events
                                                adfUpdate['sidereal_utc'] = adfUpdate.apply(
                                                    lambda x: timestamp2sideral(x['timestamp_ms']), axis=1)

                                                self._adf = pd.concat([self._adf, adfUpdate], ignore_index=True)
                                                self._adf.reset_index(inplace=True, drop=True)
                                            self._stripIncompleteEvents()
                                            self._parent.updateStatusBar(
                                                "Cache file data loaded successfully and integrated with new "
                                                "events from DB")
                                            self._parent.updateProgressBar()  # hide progressbar
                                            self._parent.busy(False)
                                            self.dataReady = True
                                            return True

                        self._parent.updateStatusBar("The cache file is aligned with DB, data loaded successfully.")
                        self._parent.updateProgressBar()  # hide progressbar
                        self._parent.busy(False)
                        self.dataReady = True
                        return True
                else:
                    # the JSON contains expired events that must be removed from JSON.
                    q = QSqlQuery(self._db)
                    select = "SELECT min(id), utc_date FROM automatic_data"
                    result = q.exec(select)
                    if result:
                        q.first()
                        if q.isValid():
                            # retrieve the first ID in db
                            oldestRecordID = q.record().field('min(id)').value()
                            oldestRecordDate = q.record().field('utc_date').value()
                            select = "SELECT count(id) FROM automatic_data where id={}".format(oldestRecordID)
                            result = q.exec(select)
                            if result:
                                q.first()
                                if q.isValid() and q.record().field("count(id)").value() == 3:
                                    # the event must be valid, with all the three rows
                                    # now removes from df any row with ID lower than minId
                                    df = self._adf.query('id >= {}'.format(oldestRecordID))
                                    if len(df) > 0:
                                        self._adf = df
                                        # TODO: prevent further recursions
                                        self._parent.busy(False)
                                        return self._loadPartialAutoDataTableFromDB(oldestRecordID, oldestRecordDate,
                                                                                    newestRecordID, newestRecordDate)
                                    else:
                                        print(
                                            "Failed removing items with id < {} from dataframe".format(oldestRecordID))
                                else:
                                    print("first ID record in DB is not valid (less than 3 rows)")

                        else:
                            print("unlikely error happened. Failed finding the minimum ID")

        # every error falls here
        if self._db is None:
            self._parent.infoMessage("Warning", "Database not opened, choose one to work with")
        else:
            e = QSqlError(q.lastError())
            print("DataSource._loadPartialAutoDataTableFromDB() ", e.text())
            self._parent.infoMessage("Error",
                                     "\nThis database and its related cache file don't match. The cache contains "
                                     "events that aren't present in database.\nPlease choose another DB to "
                                     "work with, or delete the cache file manually first.")
        self._adf = None
        self._parent.updateProgressBar()  # hide progressbar
        return False

    def _loadAutoDataTableFromDB(self):
        """
        Extracts automatic_data from DB to a dataframe
        self._adf
        @return:
        """
        if self._db is not None:
            self._parent.busy(True)
            print("_loadAutoDataTableFromDB()")
            q = QSqlQuery(self._db)
            totalRows = 0
            select = "SELECT COUNT(*) FROM automatic_data "
            result = q.exec(select)
            if result:
                q.first()
                if q.isValid():
                    rec = q.record()
                    totalRows = rec.field('count(*)').value()
                    self._parent.updateStatusBar("loading {} rows from database".format(totalRows))

            select = "SELECT * FROM automatic_data "
            result = q.exec(select)
            if result:
                self._parent.updateProgressBar(0)
                currentRow = 0
                dataDict = dict()
                q.first()
                columnList = list()
                firstRaiseFound = False
                while q.isValid():
                    currentRow += 1
                    qApp.processEvents()
                    rec = q.record()
                    if len(dataDict.keys()) == 0:
                        # the first row is the header
                        for col in range(0, rec.count()):
                            qApp.processEvents()
                            columnName = rec.fieldName(col)
                            columnList.append(columnName)
                            dataDict[columnName] = list()

                    for col in range(0, rec.count()):
                        qApp.processEvents()
                        columnName = columnList[col]
                        field = rec.field(col)
                        val = field.value()
                        varType = field.type()
                        if varType == QMetaType.Double or varType == QMetaType.Float:
                            try:
                                val = castFloatPrecision(val, 4)
                            except TypeError:
                                print("fixing invalid value: id={}, col={}, value={} as zero".format(
                                    rec.field('id').value(), columnName, val))
                                val = 0

                        # discard the first records until finds a raising front
                        if columnName == 'event_status':
                            if firstRaiseFound is False and val != 'Raise':
                                continue
                            else:
                                firstRaiseFound = True
                        if val == '0':
                            val = 0
                        dataDict[columnName].append(val)
                        self._parent.updateProgressBar(currentRow, totalRows)
                    q.next()

                # for debugging only:
                # for key in dataDict.keys():
                #    print("column: {}, len: {}".format(key, len(dataDict[key])))

                self._adf = pd.DataFrame(dataDict)
                rows = self._adf.shape[0]
                if rows > 0:
                    self._idOffset = self._adf.loc[0, 'id']

                    self._parent.updateProgressBar(0, rows)

                    e = QSqlError(q.lastError())
                    self._stripIncompleteEvents()

                    # adds a new column with the UTC Sidereal Time
                    if 'sidereal_utc' not in self._adf.columns:
                        self._parent.updateStatusBar("Calculating sidereal time for each event in DB")
                        self._adf = self._adf.assign(sidereal_utc='')
                        self._adf['sidereal_utc'] = self._adf.apply(lambda x: self._makeSideral(x), axis=1)

                    # adds the attributes column
                    if 'attributes' not in self._adf.columns:
                        self._adf = self._adf.assign(attributes='')

                    # if not self._adf.empty:
                    #    self._adf.set_index('id', inplace=True)
                    #    self._adf.sort_index(inplace=True, ascending=True)

                    print("DataSource._loadAutoDataTableFromDB() ", e.text())
                    self._parent.busy(False)
                    return True

        self._parent.infoMessage("Warning", "This database is empty, choose another one to work with")
        self._adf = None
        self._parent.updateProgressBar()  # hide progressbar
        self._parent.busy(False)
        return False

    def _makeSideral(self, record):
        currentRow = ((record['id'] - self._idOffset) * 3)
        timestamp2sideral(record['timestamp_ms'])
        self._parent.updateProgressBar(currentRow)

    def _stripIncompleteEvents(self):
        """
        Remove from self._adf any broken event
        that is not defined by all the three rows
        Raise/Peak/Delete
        """

        raiseRecords = self._adf.loc[self._adf['event_status'] == 'Raise']
        peakRecords = self._adf.loc[self._adf['event_status'] == 'Peak']
        fallRecords = self._adf.loc[self._adf['event_status'] == 'Fall']

        print("total raise records:{}, peak records:{}. fall records:{}".format(len(raiseRecords), len(peakRecords),
                                                                                len(fallRecords)))

        # raise rows without peak
        raiseNoPeakEvents = self._xorEvents(peakRecords, raiseRecords, 'id')

        # peak rows without raise
        peakNoRaiseEvents = self._xorEvents(raiseRecords, peakRecords, 'id')

        # raise rows without fall
        raiseNoFallEvents = self._xorEvents(raiseRecords, fallRecords, 'id')

        # fall rows without raise
        fallNoRaiseEvents = self._xorEvents(fallRecords, raiseRecords, 'id')

        # peak rows without fall
        peakNoFallEvents = self._xorEvents(peakRecords, fallRecords, 'id')

        # fall rows without peak
        fallNoPeakEvents = self._xorEvents(fallRecords, peakRecords, 'id')

        print("raise records without peak:{}, raise records without fall: {}. peak records without fall:{}".format(
            len(raiseNoPeakEvents), len(fallNoRaiseEvents),
            len(peakNoFallEvents)))

        print("peak records without raise:{}, fall records without raise: {}. fall records without peak:{}".format(
            len(peakNoRaiseEvents), len(raiseNoFallEvents),
            len(fallNoPeakEvents)))

        brokenEvents = pd.concat([raiseNoPeakEvents, raiseNoFallEvents, peakNoFallEvents, peakNoRaiseEvents,
                                  raiseNoFallEvents, fallNoPeakEvents])
        brokenEvents.drop_duplicates(subset='id', inplace=True)

        brokenIDs = ""
        brokenList = brokenEvents['id'].tolist()

        for bid in brokenList:
            brokenIDs += str(bid) + ','
        totalBrokenIDs = brokenIDs.count(',')
        self._parent.updateStatusBar(
            "Found {} incomplete events, will be ignored. ID list follows:".format(totalBrokenIDs))
        self._parent.updateStatusBar(brokenIDs, logOnly=True)

        self._adf.drop(self._adf[self._adf['id'].isin(brokenList)].index, inplace=True, axis='index')

    def _xorEvents(self, df1, df2, onColumn):
        # prepare list of columns to be deleted after merging
        colNames = self._adf.columns.values.tolist()
        colNames.remove(onColumn)

        colToDelete = [name + '_x' for name in colNames]
        '''
        for name in colNames:
            colToDelete.append(name + '_x')
        '''
        colToDelete.append('_merge')

        colToRename = dict()
        for name in colNames:
            colToRename[name + '_y'] = name

        xorDf = df1.merge(df2, on=onColumn, how='right', indicator=True)
        xorDf = xorDf[xorDf['_merge'] == 'right_only']
        xorDf.drop(columns=colToDelete, inplace=True)
        xorDf.rename(columns=colToRename, inplace=True)
        return xorDf

    def _saveAutoDataTable(self):
        """
        Saves the self._adf dataframe as a cache file, json formatted.

        TBD: If the file already exists and has the same number of
        rows and columns, only the classification and attributes
        columns will be updated, to speed-up writing.

        Since v.0.1.71 the sqlite database generated by Echoes
        is not changed anymore
        """
        return self._saveClassifications()

    def _saveClassifications(self):
        """
        Updates the classifications and attributes
        on the cache file.

        Currently rewrites the entire csv file,
        TBD something smarter in case of speed concerns
        """
        if self._db is not None and self._adf is not None and self._dataChangedInMemory is True:
            if os.path.exists(self._adPath):
                bakPath = self._adPath.replace('.json', '.bak')
                self._copyFile(self._adPath, bakPath)
            adJson = self._adf.to_json(orient='records')
            with (open(self._adPath, 'w')) as file:
                file.write(adJson)
            return True
        return False

    def _loadAutoSessionTable(self):
        """
        Extracts automatic_sessions from DB to a dataframe
        @return:
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            select = "SELECT * FROM automatic_sessions "
            result = q.exec(select)
            if result:
                dataDict = dict()
                q.first()
                columnList = list()
                while q.isValid():
                    qApp.processEvents()
                    rec = q.record()
                    if len(dataDict.keys()) == 0:
                        # the first row is the header
                        for col in range(0, rec.count()):
                            qApp.processEvents()
                            columnName = rec.fieldName(col)
                            columnList.append(columnName)
                            dataDict[columnName] = list()
                    for col in range(0, rec.count()):
                        qApp.processEvents()
                        columnName = columnList[col]
                        val = rec.field(col).value()
                        if val == '0':
                            val = 0
                        dataDict[columnName].append(val)
                    q.next()
                df = pd.DataFrame(dataDict)
                df.set_index('id')
                df.sort_index(inplace=True, ascending=True)
                # creates datetime columns from string datetimes
                df['start_DT'] = pd.to_datetime(df['start_dt'])
                df['end_DT'] = pd.to_datetime(df['living_dt']).fillna(df['start_DT'])

                # then calculate timedeltas on all supported time resolutions
                df = df.drop(columns=['start_dt', 'living_dt', 'end_dt', 'delta_min'])
                # hide the active session (cause 0)
                # df = df[df.cause != 0]

                df['delta_min'] = ((df['end_DT'] - df['start_DT']) / pd.Timedelta(minutes=1)).astype(int)
                df['delta_10min'] = ((df['end_DT'] - df['start_DT']) / pd.Timedelta(minutes=10)).astype(int)
                df['delta_hour'] = ((df['end_DT'] - df['start_DT']) / pd.Timedelta(hours=1)).astype(int)
                df['delta_days'] = ((df['end_DT'] - df['start_DT']) / pd.Timedelta(days=1)).astype(int)

                self._sdf = df
            e = QSqlError(q.lastError())
            print("DataSource._loadAutoSessionTable() ", e.text())

    def _copyFile(self, srcPath: str, destPath: str):
        """

        """
        sourceSize = os.stat(srcPath).st_size
        copied = 0
        self._parent.updateProgressBar(0)
        with open(srcPath, "rb") as source, open(destPath, "wb") as target:
            while True:
                chunk = source.read(16384)
                if not chunk:
                    break

                target.write(chunk)
                copied += len(chunk)
                self._parent.updateProgressBar(copied, sourceSize)

    def _copyTable(self, srcDb: QSqlDatabase, srcTable: str, destDb: QSqlDatabase, destTable: str,
                   fromId: int = -1, toId: int = -1):
        """
        srcDb and destDb are opened DBs. The target table must already exist empty in
        destDb
        """
        print("copying table {}".format(srcTable))
        destDb.transaction()
        currentRow = 0
        self._parent.updateProgressBar(0)
        destDb.transaction()
        q1 = QSqlQuery(srcDb)

        if '_data' in srcTable:
            if fromId != -1 and toId != -1:
                cmd = "SELECT * FROM {} WHERE id >= {} AND id <= {} ORDER BY id ASC, event_status DESC".format(srcTable,
                                                                                                               fromId,
                                                                                                               toId)

            else:
                cmd = "SELECT * FROM {} ORDER BY id ASC, event_status DESC".format(srcTable)
            totalRows = ((toId - fromId) + 1) * 3  # 3 rows per ID (Raise/Peak/Fall)
        else:
            if fromId != -1 and toId != -1:
                cmd = "SELECT * FROM {} WHERE id >= {} AND id <= {} ORDER BY id ASC".format(srcTable, fromId, toId)

            else:
                cmd = "SELECT * FROM {} ORDER BY id ASC".format(srcTable)

            totalRows = (toId - fromId) + 1

        result = q1.exec(cmd)
        if result:
            print("totalRows=", totalRows)
            q1.first()
            while q1.isValid():
                colValues = dict()
                localRecord = q1.record()
                # print("table {} has the following columns:".format(srcTable))
                for col in range(0, localRecord.count()):
                    qApp.processEvents()
                    name = localRecord.fieldName(col)
                    if '_data' in name:
                        # BLOBs must never be converted to strings
                        colValues[name] = QByteArray(q1.value(name))
                    else:
                        colValues[name] = str(q1.value(name))
                    # print("{}={}".format(name, colValues[name]))

                q2 = QSqlQuery(destDb)
                names = ','.join(colValues.keys())
                values = len(colValues.keys()) * '?,'
                values = values[0:-1]  # chops final comma
                cmd = "INSERT INTO {} ({}) VALUES ({})".format(destTable, names, values)
                # print("currentRow=",currentRow)

                q2.prepare(cmd)
                for val in colValues.values():
                    qApp.processEvents()
                    q2.addBindValue(val)

                result = q2.exec()
                if not result:
                    e = QSqlError(q2.lastError())
                    print("DataSource.copyTable() ", e.text())
                    destDb.rollback()
                    return False

                currentRow += 1
                if totalRows > -1:
                    self._parent.updateProgressBar(currentRow, totalRows)

                q1.next()
        destDb.commit()
        return True

    def _getFirstDumpId(self) -> int:
        """
        Returns the oldest event Id having a dump file
        still stored in automatic_dumps. This means
        return the lowest ID number in automatic_dumps
        @return:
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            select = "SELECT MIN(id) FROM automatic_dumps"
            result = q.exec(select)
            if result:
                q.first()
                if q.isValid():
                    did = q.value(0)
                    if did == '':
                        did = 0
                    return did
            e = QSqlError(q.lastError())
            print("DataSource.getFirstDumpId() ", e.text())
        return 0

    def timestamp2datetime(self, timestamp_ms: str):
        return datetime.strptime(timestamp_ms, '%d/%m/%Y;%H:%M:%S.%f')

    def name(self):
        filePath = Path(self._dsPath)
        return filePath.name

    def stem(self):
        filePath = Path(self._dsPath)
        return filePath.stem

    def fullPath(self):
        return self._dsPath

    def connection(self):
        return self._connectName

    def reopenFile(self):
        self.closeFile()
        DataSource.connectionProg += 1
        print("creating new database connection")
        self._connectName = "EBROW_CONN#{}".format(DataSource.connectionProg)
        self._db = QSqlDatabase.addDatabase("QSQLITE", self._connectName)
        self._db.setDatabaseName(self._dsPath)
        print("reopening database")
        if self._db.open():
            self._loadAutoDataTable()
            self._loadAutoSessionTable()
            return self._db

        print("error reopening database ", self._dsPath)
        self.closeFile()
        return None

    def openFile(self, fileDBpath):
        """
        @param fileDBpath:
        @return:
        """
        if fileDBpath is not None:
            self.closeFile()
            self._workingDir = QDir.current()
            self._dsPath = fileDBpath
            dbSuffix = Path(fileDBpath).suffix
            self._adPath = self._dsPath.replace(dbSuffix, ".json")
            DataSource.connectionProg += 1
            self._connectName = "EBROW_CONN#{}".format(DataSource.connectionProg)
            self._db = QSqlDatabase.addDatabase("QSQLITE", self._connectName)
            self._db.setDatabaseName(self._dsPath)
            if self._db.open():
                if self._loadAutoDataTable():
                    self._loadAutoSessionTable()
                    return self._db

            print("error opening database ", self._dsPath)
            self.closeFile()
            return None

    def openFileDialog(self):
        """

        @return: -1 : failed new DB file opening, the current DB has been closed anyway
                 1 : DB file opened successfully
                 0 = pressed Cancel, nothing done
        """

        self._workingDir = QDir.current()
        fileDialog = QFileDialog()
        fileDialog.setWindowTitle("Open Data Source")
        fileDialog.setNameFilter("Echoes database (*.sqlite3)")
        fileDialog.setFileMode(QFileDialog.ExistingFile)
        fileDialog.setDirectory(self._workingDir)
        if fileDialog.exec():
            # pressed open
            self.closeFile()
            qApp.processEvents()
            self._parent.busy(True)
            ret = self.openFile(fileDialog.selectedFiles()[0])
            self._parent.busy(False)
            if ret is None:
                print("error opening database ", self._dsPath)
                return -1
            else:
                return 1
        # pressed cancel
        return 0

    def getSubsetFilename(self):
        """

        @return:
        """
        self._workingDir = QDir.current()
        subsetDBpath = QFileDialog.getSaveFileName(
            self._parent, caption="Save data subset", directory=self._workingDir.path(),
            filter="SQLite 3 database file (*.sqlite3)")
        return subsetDBpath[0]

    def createSubset(self, subPath: str, fromDate: str, toDate: str) -> bool:
        """

        """
        print("createSubset(from {} to {} in {})".format(fromDate, toDate, subPath))

        covIds = self.idCoverage(fromDate, toDate, True)
        startIndex = self._adf.index[(self._adf['id'] == covIds[2]) & (self._adf['event_status'] == 'Raise')].to_list()[
            0]
        endIndex = self._adf.index[(self._adf['id'] == covIds[3]) & (self._adf['event_status'] == 'Fall')].to_list()[0]
        subAdf = self._adf.iloc[startIndex:(endIndex + 1)]  # includes endIndex

        self._parent.updateStatusBar("Creating subset from {}(id={}) to {}(id={}) total {} rows:"
                                     .format(covIds[0], covIds[2], covIds[1], covIds[3], len(subAdf.index)))

        if subAdf is not None:
            self._parent.busy(True)
            # cloning the empty database model as base for this subset
            emptyDB = QResource("emptyDB")
            src = QFile(emptyDB.absoluteFilePath())
            if QFile.exists(subPath):
                print("overwriting existing file ", subPath)
                QFile.remove(subPath)
            if src.copy(subPath):
                os.chmod(subPath, stat.S_IWRITE)
                # then connects it
                DataSource.connectionProg += 1
                connectName = "EBROW_CONN#{}".format(DataSource.connectionProg)
                subsetDb = QSqlDatabase.addDatabase("QSQLITE", connectName)
                subsetDb.setDatabaseName(subPath)

                if subsetDb.open():

                    # only the selected ids will be copied in these tables:
                    subTables = [
                        "automatic_data",
                        "automatic_shots",
                        "automatic_dumps"
                    ]

                    for tableName in subTables:
                        self._parent.updateStatusBar("Copying selection of {}".format(tableName))
                        result = self._copyTable(self._db, tableName, subsetDb, tableName, covIds[2],
                                                 covIds[3])
                        if not result:
                            subsetDb.close()
                            os.remove(subPath)
                            self._parent.busy(False)
                            return False

                    # while all the other tables, that are quite small, will be fully cloned
                    otherTables = list((Counter(self._db.tables()) - Counter(subTables)).elements())
                    for tableName in otherTables:
                        self._parent.updateStatusBar("Copying {}".format(tableName))
                        result = self._copyTable(self._db, tableName, subsetDb, tableName)
                        if not result:
                            subsetDb.close()
                            os.remove(subPath)
                            self._parent.busy(False)
                            return False

                    # finally the views are cloned
                    self._parent.updateStatusBar("Copying views...")
                    subsetDb.exec('''
                        CREATE VIEW v_archived_days AS
                            SELECT DISTINCT utc_date
                                FROM automatic_data
                                WHERE utc_date <> DATE('now')
                    ''')
                    subsetDb.exec('''
                        CREATE VIEW v_automatic_data_unclassified AS
                            SELECT DISTINCT *
                                FROM automatic_data
                                WHERE classification = ""
                    ''')
                    subsetDb.exec('''
                        CREATE VIEW v_automatic_totals_by_classification AS
                            SELECT DISTINCT (
                                SELECT count( * ) 
                                    FROM automatic_data
                                    WHERE classification LIKE '%UNDER%' AND event_status = 'Fall'
                            )
                        AS underdense,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%OVER%' AND event_status = 'Fall'
                        )
                        AS overdense,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE CAR1%' AND event_status = 'Fall'
                        )
                        AS fake_carrier1,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE CAR2%' AND event_status = 'Fall'
                        )
                        AS fake_rfi,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE RFI%' AND event_status = 'Fall'
                        )
                        AS fake_esd,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE ESD%' AND event_status = 'Fall'
                        )
                        AS fake_esd,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE SAT%' AND event_status = 'Fall'
                        )
                        AS fake_saturation,
                        (
                            SELECT count( * ) 
                                FROM automatic_data
                                WHERE classification LIKE '%FAKE LONG%' AND event_status = 'Fall'
                        )
                        AS fake_too_long
                        FROM automatic_data;
                    ''')
                    subsetDb.close()
                self._parent.updateStatusBar("Subset created successfully")
                self._parent.busy(False)
                return True
        return False

    def updateFile(self):
        if self._adf is not None and self._dataChangedInMemory:
            # TODO: ask for full saving or only the classifications
            print("updating automatic_data table")
            self._saveClassifications()
            return True
        return False

    def closeFile(self):
        if self._db is not None:
            print("closing database and connection")
            self._db.close()
            self._db.removeDatabase(self._dsPath)
            self._db = None
            self._connectName = None
            return True
        return False

    def QDateCoverage(self) -> (QDate, QDate):
        """

        @return:
        """
        qDateFrom = None
        qDateTo = None
        if self._adf is not None:
            dateSeries = self._adf['utc_date']

            dateFrom = dateSeries.min()
            dateTo = dateSeries.max()
            qDateFrom = QDate.fromString(dateFrom, "yyyy-MM-dd")
            qDateTo = QDate.fromString(dateTo, "yyyy-MM-dd")
        return qDateFrom, qDateTo

    def idCoverage(self, dateFrom: str, dateTo: str, selfAdjust: bool = False) -> (str, str, str, str):
        idFrom = 0
        idTo = 0
        if self._adf is not None:
            idsFrom = self._adf.loc[(self._adf['utc_date'] == dateFrom), 'id']
            idsTo = self._adf.loc[(self._adf['utc_date'] == dateTo), 'id']
            idFrom = idsFrom.min()
            idTo = idsTo.max()
            if selfAdjust:
                # automatically fix exceeding bounds
                if isnan(idFrom) or isnan(idTo):
                    dateFrom = min(self._adf['utc_date'])
                    dateTo = max(self._adf['utc_date'])
                    idsFrom = self._adf.loc[(self._adf['utc_date'] == dateFrom), 'id']
                    idsTo = self._adf.loc[(self._adf['utc_date'] == dateTo), 'id']
                    idFrom = idsFrom.min()
                    idTo = idsTo.max()
        return dateFrom, dateTo, idFrom, idTo

    def dailyCoverage(self, date: str) -> int:
        maxDaily = 0
        if self._adf is not None:
            dailySeries = self._adf.loc[(self._adf['utc_date'] == date), 'daily_nr']
            maxDaily = dailySeries.max()
        return maxDaily

    def tableModel(self, name):
        """
        Returns the QSqlTableModel of the given DB table
        @param name:
        @return:
        """
        m = QSqlTableModel(None, self._db)
        m.setTable(name)
        m.select()
        return m

    def dailyCountsByClassification(self, df, filters, dateFrom=None, dateTo=None, totalRow=False, totalColumn=False,
                                    compensate=False, considerBackground=False):
        return self._dailyAggregationByClassification(df, filters, dateFrom, dateTo, metric='count',
                                                     totalRow=totalRow, totalColumn=totalColumn,
                                                     compensate=compensate, considerBackground=considerBackground)

    def dailyPowersByClassification(self, df, filters, dateFrom=None, dateTo=None, highestAvgRow=False,
                                    highestAvgColumn=False):
        return self._dailyAggregationByClassification(df, filters, dateFrom, dateTo, metric='power',
                                                     highestAvgRow=highestAvgRow, highestAvgColumn=highestAvgColumn)

    def dailyLastingsByClassification(self, df, filters, dateFrom=None, dateTo=None, highestAvgRow=False,
                                      highestAvgColumn=False):
        return self._dailyAggregationByClassification(df, filters, dateFrom, dateTo,  metric='lasting', dtDec = 0,
                                                     highestAvgRow=highestAvgRow, highestAvgColumn=highestAvgColumn)

    def _dailyAggregationByClassification(self, df: pd.DataFrame, filters: str, dateFrom: str = None, dateTo: str = None,
                                         metric: str = 'count', dtDec: int = 1, totalRow: bool = False, totalColumn: bool = False,
                                         compensate: bool = False, considerBackground: bool = False,
                                         highestAvgRow: bool = False, highestAvgColumn: bool = False) -> pd.DataFrame:
        """
        Aggregates daily metrics (count, mean of 'diff', or mean of 'lasting_ms') by classification.

        @param df: Input dataframe
        @param filters: Filter string for classification
        @param dateFrom: Start date (inclusive)
        @param dateTo: End date (inclusive)
        @param metric: Aggregation metric ('count', 'power', 'lasting')
        @param dtDec: number of decimals to show, zero=integer
        @param totalRow: Add a total row summing all columns
        @param totalColumn: Add a total column summing all rows
        @param compensate: If True, adjust counts based on background values
        @param considerBackground: If True, subtract background values
        @param highestAvgRow: Add a row with the average of each column
        @param highestAvgColumn: Add a column with the average of each row
        @return: DataFrame with daily aggregated metrics by classification
        """
        avgDailyDict = self._parent.tabStats.avgDailyDict if hasattr(self._parent, 'tabStats') else {}

        df.set_index('id')

        # Keeps only "Fall" records
        df = df.loc[(df['event_status'] == 'Fall')].copy()

        # Generate daily date range
        dtRange = pd.date_range(dateFrom, dateTo, freq='D')
        dtList = [x.strftime('%Y-%m-%d') for x in dtRange]

        # Filters by classification
        filterList = [item.strip() for item in filters.split(',')]
        classList = filterList
        if 'ACQ ACT' in classList:
            classList.remove('ACQ ACT')

        resultsList = []
        for cl in classList:
            cl = cl.strip()
            qApp.processEvents()
            fdf = df.loc[(df['classification'] == cl)].copy()
            metricDict = {}

            for dt in dtList:
                qApp.processEvents()
                tempDf = fdf.loc[(fdf['utc_date'] == dt)].copy()

                if metric == 'count':
                    value = tempDf.shape[0]
                elif metric == 'power':
                    value = tempDf['diff'].mean() if tempDf.shape[0] > 0 else 0
                elif metric == 'lasting':
                    value = round(tempDf['lasting_ms'].mean()) if tempDf.shape[0] > 0 else 0
                else:
                    raise ValueError(f"Unsupported metric: {metric}")

                if value < 0:
                    value = 0
                if value == 0 and 'ACQ ACT' in filterList and not self.acqWasRunning(dt, 86400):
                    value = np.nan

                # Apply background adjustments
                if metric == 'count' and avgDailyDict:
                    background = avgDailyDict.get(cl, 0)
                    if compensate and value < background:
                        value = background
                    if considerBackground:
                        value -= background
                    if value < 0:
                        value = 0

                if dtDec > 0:
                    metricDict[dt] = round(value, dtDec) if not pd.isna(value) else np.nan
                else:
                    metricDict[dt] = round(value) if not pd.isna(value) else -1

            resultsList.append(pd.Series(metricDict))

        # Combine results into a DataFrame
        newDf = pd.concat(resultsList, axis=1)

        # Controlla se tutti i tipi di dati originali sono interi
        all_integers = all(dtype.kind == 'i' for dtype in newDf.dtypes)

        # Add totals or averages
        if totalRow:
            total_row = newDf.sum(numeric_only=True)
            newDf.loc['Total'] = total_row.astype(int) if all_integers else total_row  # Interi solo se tutto è intero

        if totalColumn:
            total_column = newDf.sum(axis=1)
            newDf['Total'] = total_column.astype(int) if all_integers else total_column
            classList.append('Total')

        # Calcolo della colonna Average
        if highestAvgColumn:
            averages = newDf.apply(lambda row: row[row != 0].mean(), axis=1).round(1)
            newDf['Average'] = averages.astype(int) if all_integers else averages
            classList.append('Average')

        # Calcolo della riga Average
        if highestAvgRow:
            averages = newDf.apply(lambda col: col[col != 0].mean(), axis=0).round(1)
            newDf.loc['Average'] = averages.astype(int) if all_integers else averages

        # Evitare conflitti tra Average row e Average column
        if highestAvgColumn and highestAvgRow:
            newDf.at['Average', 'Average'] = -1 if all_integers else None

        # Riassegna i nomi delle colonne
        newDf.columns = classList
        return newDf

    def totalsByClassification(self, filters: str, considerBackground: bool = False,
                               compensate: bool = False) -> pd.DataFrame:
        """

        @param filters
        @return:
        """
        if considerBackground or compensate:
            print("WARNING - considerBackground NOT YET IMPLEMENTED")
        totalsDf = None
        if self._adf is not None:
            ids = dict()
            counts = dict()
            # filters fdf by class filters, removing spaces in between
            filterList = [item.strip() for item in filters.split(',')]
            for cat in filterList:
                qApp.processEvents()
                ids[cat] = self._adf.loc[self._adf['classification'].isin([cat]), 'id'].unique()
                counts[cat] = ids[cat].size
            totalsDf = pd.DataFrame(counts)
        return totalsDf

    def totalsUnclassified(self, dateFrom: str = None, dateTo: str = None) -> int:
        """
        @return:
        """
        if self._adf is not None:
            if dateFrom is not None and dateTo is not None:
                # returns the number of IDs recorded in the given date interval
                (adjDateFrom, adjDateTo, idFrom, idTo) = self.idCoverage(dateFrom, dateTo, True)
                return (int(idTo) - int(idFrom)) + 1
            else:
                # returns the count of all IDs in the table
                ids = self._adf.loc[:, 'id'].unique()
                return ids.size
        return 0

    def loadTableConfig(self, name: str, rev: int = -1) -> pd.DataFrame:
        """
        Extracts a configuration table
        @param name:
        @param rev:
        @return: dataframe
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            if rev != -1:
                select = "SELECT * FROM {} WHERE id={}".format(name, rev)
            else:
                select = "SELECT * FROM {}".format(name)
            result = q.exec(select)
            if result:
                dataDict = dict()
                q.first()
                columnList = list()
                if rev != -1 and not q.isValid():
                    # if no record indexed by rev is present, takes the youngest record
                    select = "SELECT * FROM {0} WHERE ID = (SELECT MAX(ID) FROM {0})".format(name)
                    q.exec(select)
                    q.first()

                while q.isValid():
                    qApp.processEvents()
                    rec = q.record()
                    if len(dataDict.keys()) == 0:
                        # the first row is the header
                        for col in range(0, rec.count()):
                            qApp.processEvents()
                            columnName = rec.fieldName(col)
                            columnList.append(columnName)
                            dataDict[columnName] = list()
                    for col in range(0, rec.count()):
                        qApp.processEvents()
                        columnName = columnList[col]
                        val = rec.field(col).value()
                        if val == '0':
                            val = 0
                        dataDict[columnName].append(val)
                    q.next()
                return pd.DataFrame(dataDict)

            e = QSqlError(q.lastError())
            print("DataSource.tableConfig() ", e.text())

    def getCfgRevisionFromID(self, eventId: int):
        """
        Returns the RTS configuration revision of the given
        event ID

        @param eventId:
        @return: RTS configuration revision number
        """
        revision = None
        if self._adf is not None:
            eventId = self._adf.loc[0, 'id'] if eventId == 0 else eventId
            revision = \
                self._adf.loc[(self._adf['id'] == eventId) & (self._adf['event_status'] == 'Fall'), ['revision']].iloc[
                    0, 0]
        return revision

    def getCfgRevisions(self) -> list:
        """
        Returns a list of RTS configuration revisions occurred in the selected period
        @return: list of revision numbers
        """
        revisions = None
        if self._adf is not None:
            revisions = self._adf['revision'].unique()
        return revisions

    def getEchoesVersion(self, rev: int):
        df = self.loadTableConfig('cfg_prefs', rev)
        df.set_index('id', inplace=True)
        if len(df) > 1:
            return df.loc[rev, 'echoes_ver']
        return df.iloc[0]['echoes_ver']

    def getIDsOfTheDay(self, date: str):
        """
        Returns a list of event IDs happened in the given day
        @param date:
        @return:
        """
        ids = None
        if self._adf is not None:
            # TODO: ordinare per ID
            ser = (self._adf['utc_date'].isin([date]))
            ids = self._adf.loc[ser, 'id'].unique().tolist()
        return ids

    def getFilteredIDsOfTheDay(self, date: str, filtClass: str = '', withImagesOnly: bool = False):
        """
        Returns a list of event IDs happened in the given day
        filtered by classification.  Two lists are returned:
        ids and daily numbers
        @param date:
        @param filtClass:
        @return:
        """
        ids = []
        dailies = []
        if len(filtClass) > 0 and self.isDateInRange(date):
            filtList = filtClass.split(',')
            df = self._adf.loc[
                (self._adf['utc_date'] == date) & (self._adf['classification'].isin(filtList)), ['id', 'daily_nr']]

            # TODO: ordinare per ID
            if df is not None:
                ids = df['id'].unique().tolist()
                dailies = df['daily_nr'].unique().tolist()

            if withImagesOnly:
                # discards event and daily ids of events having no image data associated
                toDiscard = list()
                for idx in range(0, len(ids)):
                    cId = int(ids[idx])
                    cDaily = int(dailies[idx])
                    shotName, shotBytes, dailyNr, utcDate = self.extractShotData(cId)
                    if shotName is None:
                        dumpName, dumpBytes, dailyNr, utcDate = self.extractDumpData(cId)
                        if dumpName is None:
                            toDiscard.append((cId, cDaily))
                            continue
                if len(toDiscard) > 0:
                    print("getFilteredIDsOfTheDay() discarding {} events without images".format(len(toDiscard)))
                    for cId, cDaily in toDiscard:
                        ids.remove(cId)
                        dailies.remove(cDaily)
        ids.sort()
        dailies.sort()
        return ids, dailies

    def extractShotData(self, eventId: int):
        """
        returns the filename and screenshot of the given event ID
        returns also its daily number and date when happened
        @param eventId:
        @return: shotName, shotBytes, dailyNr, utcDate
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            select = "SELECT shot_name, shot_data FROM automatic_shots WHERE id={}".format(eventId)
            result = q.exec(select)
            if result:
                q.first()
                if q.isValid():
                    shotName = q.value(0)
                    shotBytes = q.value(1)
                    dailyNr, utcDate = self._getDailyNrFromID(eventId)
                    return shotName, shotBytes, dailyNr, utcDate
            e = QSqlError(q.lastError())
            print("DataSource.extractShotData() ", e.text())
        dailyNr, utcDate = self._getDailyNrFromID(eventId)
        return None, None, dailyNr, utcDate
        # return None

    def extractShotsData(self, ids: list):
        """
        returns a list of tuples containing the filename and screenshot of all the events listed in ids
        the tuple contains also its daily number and date when happened
        @param ids: list of event id
        @return: list of tuple(id, shotName, shotBytes, dailyNr, utcDate)
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            select = "SELECT id, shot_name, shot_data FROM automatic_shots WHERE id IN ("
            for dId in ids:
                qApp.processEvents()
                select += "{}, ".format(dId)
            select = select[0:-2] + ')'
            result = q.exec(select)
            if result:
                tupleList = list()
                q.first()
                while q.isValid():
                    qApp.processEvents()
                    dId = q.value(0)
                    shotName = q.value(1)
                    shotBytes = q.value(2)
                    dailyNr, utcDate = self._getDailyNrFromID(dId)
                    tupleList.append((dId, shotName, shotBytes, dailyNr, utcDate))
                    q.next()
                return tupleList

            e = QSqlError(q.lastError())
            print("DataSource.extractShotsData() ", e.text())
        return None

    def extractDumpData(self, eventId: int):
        """
        returns the filename and  dump qbytearray of the given event ID
        returns also its daily number and date when happened
        @param eventId:
        @return:
        """
        if self._db is not None:
            q = QSqlQuery(self._db)
            select = "SELECT dump_name, dump_data FROM automatic_dumps WHERE id={}".format(eventId)
            result = q.exec(select)
            if result:
                q.first()
                if q.isValid():
                    dumpName = q.value(0)
                    dumpData = qUncompress(q.value(1))
                    '''
                    dumpData = qUncompress(q.value(1)).data().decode()
                    dumpLines = dumpData.splitlines()
                    dumpList = []
                    for line in dumpLines:
                        dumpList.append( line.split() )
                    '''
                    dailyNr, utcDate = self._getDailyNrFromID(eventId)
                    # df = pd.DataFrame(dumpList, columns=['bintime', 'frequency', 'power', 'scantime',
                    # 'S', 'N', 'S-N'])
                    return dumpName, dumpData, dailyNr, utcDate
            e = QSqlError(q.lastError())
            print("DataSource.extractDumpData() ", e.text())
        dailyNr, utcDate = self._getDailyNrFromID(eventId)
        return None, None, dailyNr, utcDate

    def dataChanged(self):
        return self._dataChangedInMemory

    def classifyEvents(self, fromID: int, toID: int, overwrite: bool = False):
        """
        Note: this algorythm has been taken from Echoes PostProc class and pythonized.

        :param fromID:
        :param toID:
        :param overwrite: False=process only unclassified records, True=process all records
        :return:
        """
        df = self.getADpartialFrame(idFrom=fromID, idTo=toID)
        self._parent.updateProgressBar(0)
        currentRow = 0

        if df is not None:
            if overwrite:
                print("overwriting all current classifications")
                df['classification'] = None

            # the S, N, avgS, diff, avgDiff related to Raise status are stored
            raiseRecords = df.loc[(df['event_status'] == 'Raise') & (df['classification'] == "")]
            subsetRaise = raiseRecords.loc[:,
                          ['id', 'S', 'avgS', 'N', 'diff', 'avg_diff', 'diff_start', 'diff_end', 'std_dev',
                           'freq_shift']]

            # the S, N, avgS, diff, avgDiff to be considered for filtering are related to Peak status
            peakRecords = df.loc[(df['event_status'] == 'Peak') & (df['classification'] == "")]
            subsetPeak = peakRecords.loc[:,
                         ['id', 'up_thr', 'S', 'avgS', 'N', 'diff', 'avg_diff', 'diff_start', 'diff_end', 'std_dev']]

            # while other fields are considered in Fall status
            fallRecords = df.loc[(df['event_status'] == 'Fall') & (df['classification'] == "")]
            subsetFall = fallRecords.loc[:,
                         ['id', 'N', 'lasting_ms', 'lasting_scans', 'echo_area', 'interval_area', 'peaks_count',
                          'freq_shift',
                          'std_dev']]

            manRefN = self._settings.readSettingAsBool('enableRefN')
            if manRefN:
                referenceN = self._settings.readSettingAsFloat('refNlevel')
            else:
                referenceN = self.DBFS_RANGE_MAX

            nextReferenceN = referenceN

            RFIfilter = self._settings.readSettingAsBool('RFIfilter')
            ESDfilter = self._settings.readSettingAsBool('ESDfilter')
            SATfilter = self._settings.readSettingAsBool('SATfilter')
            CAR1filter = self._settings.readSettingAsBool('CAR1filter')
            CAR2filter = self._settings.readSettingAsBool('CAR2filter')
            RFIfilterThreshold = self._settings.readSettingAsFloat('RFIfilterThreshold')
            ESDfilterThreshold = self._settings.readSettingAsFloat('ESDfilterThreshold')
            SATfilterThreshold = self._settings.readSettingAsFloat('SATfilterThreshold')
            CAR1filterThreshold = self._settings.readSettingAsFloat('CAR1filterThreshold')
            CAR2filterThreshold = self._settings.readSettingAsFloat('CAR2filterThreshold')
            underdenseMs = self._settings.readSettingAsInt('underdenseMs')
            overdenseSec = self._settings.readSettingAsInt('overdenseSec')
            carrierSec = self._settings.readSettingAsInt('carrierSec')

            cfgOutput = self.loadTableConfig('cfg_output', 0xffff)
            if cfgOutput is None:
                print("classifyEvents(): no output settings table found, doing nothing")
                return False

            elem = cfgOutput.iloc[-1:].loc[:, 'shot_freq_range']
            detectionRange = elem.iloc[0].astype(int)

            # fallRecords.set_index('id', drop=False, inplace=True)
            # peakRecords.set_index('id', drop=False, inplace=True)
            # raiseRecords.set_index('id', drop=False, inplace=True)
            r = 0
            configRevision = 0
            odf = None
            rows = fallRecords.shape[0]
            self._parent.updateStatusBar("updating classifications on {} events".format(rows))
            for idx in fallRecords.index:
                currentRow += 1
                self._parent.updateProgressBar(currentRow, rows)
                qApp.processEvents()
                if nextReferenceN == self.DBFS_RANGE_MAX:
                    # initialize the reference N value
                    nextReferenceN = subsetFall.loc[idx, 'N']

                if manRefN:
                    referenceN = self._settings.readSettingAsFloat('refNlevel')
                else:
                    referenceN = nextReferenceN

                cla = fallRecords.loc[idx, 'classification']
                myId = fallRecords.loc[idx, 'id']
                if cla == '':
                    print("Classifying eventID#", myId)

                    # gets some Echoes setup parameters useful for fakes detection
                    cr = self.getCfgRevisionFromID(myId)
                    if cr != configRevision or odf is None:
                        configRevision = cr
                        odf = self.loadTableConfig('cfg_output', configRevision)
                        recTime = odf.loc[0, 'rec_time']

                    # idx indexes the fall event - to browse peak events, idx must be decremented by 1
                    idp = idx - 1

                    # while to browse the raise event, idx must be decremented by 2
                    idr = idx - 2

                    N = subsetPeak.loc[idp, 'N']
                    diff = subsetPeak.loc[idp, 'diff']
                    avgDiff = subsetPeak.loc[idp, 'avg_diff']
                    stdDev = [subsetRaise.loc[idr, 'std_dev'], subsetPeak.loc[idp, 'std_dev'],
                              subsetFall.loc[idx, 'std_dev']]

                    begDiffPeak = subsetPeak.loc[idp, 'diff_start']
                    endDiffPeak = subsetPeak.loc[idp, 'diff_end']

                    begDiffRaise = subsetRaise.loc[idr, 'diff_start']
                    endDiffRaise = subsetRaise.loc[idr, 'diff_end']

                    upperThreshold = subsetPeak.loc[idp, 'up_thr']

                    # result = fpCmp(referenceN, N, 1)
                    # if result != 0:
                    #    referenceN = N

                    peaksCount = subsetFall.loc[idx, 'peaks_count']
                    echoArea = subsetFall.loc[idx, 'echo_area']
                    intervalArea = subsetFall.loc[idx, 'interval_area']
                    lastingMs = subsetFall.loc[idx, 'lasting_ms']
                    lastingScans = subsetFall.loc[idx, 'lasting_scans']
                    freqShift = subsetFall.loc[idx, 'freq_shift'] - subsetRaise.loc[idr, 'freq_shift']
                    begN = subsetRaise.loc[idr, 'N']
                    endN = subsetFall.loc[idx, 'N']

                    # fakes filter suited for lightings and electrostatic discharges ESD
                    if ESDfilter != 0:
                        # ESD appear on waterfall like an horizontal stripe covering the entire bandwidth
                        # with variable duration, depending of its cause: human (i.e. electric motors) or natural
                        # (lightings).
                        # The reference band S-N,  expressed as the average of S-N values read at the beginning (left)
                        # and at the end (right) of the scan, is calculated on Raise and Peak times.
                        # If the difference between the peak and raise references exceeds the upper threshold
                        # we have an ESD disturbance

                        avg = (begDiffRaise + endDiffRaise) / 2
                        refRaise = avg if avg > 0 else 0
                        avg = (begDiffPeak + endDiffPeak) / 2
                        refPeak = avg if avg > 0 else 0
                        refDiff = refPeak - refRaise
                        cmp = fuzzyCompare(refDiff, upperThreshold, (ESDfilterThreshold / 10))
                        if cmp == 1:
                            # refDiff exceeds upperThreshold
                            print("eventID#{} is ESD fake".format(myId))
                            self.setEventClassification(myId, "FAKE ESD")
                            continue

                    # filter against rx saturations artifacts
                    if SATfilter > 0:

                        # Strong signals near the receiver site, even on different frequencies
                        # than Echoes tuned frequency, can cause receiver saturations, that
                        # are detected by checking the N level falling down or raising up suddendly.
                        # The filter works by storing the N recorded at any event.
                        # That stored value becomes the reference level for the next event. 
                        # If in a new event,
                        # the N level falls below or jumps above that reference exceeding the given tolerance
                        # threshold, the event is classified as saturation

                        fuzzyRatio = fuzzyCompare(begN, endN, (SATfilterThreshold / 10))
                        if fuzzyRatio != 0:
                            print("eventID#{} is a saturation fake (sudden N variation - middle)".format(myId))
                            self.setEventClassification(myId, "FAKE SAT")
                            continue

                        fuzzyRatio = fuzzyCompare(referenceN, begN, (SATfilterThreshold / 10))
                        if fuzzyRatio != 0:
                            print("eventID#{} is a saturation fake (sudden N variation - begin)".format(myId))
                            self.setEventClassification(myId, "FAKE SAT")
                            continue

                        fuzzyRatio = fuzzyCompare(referenceN, endN, (SATfilterThreshold / 10))
                        if fuzzyRatio != 0:
                            print("eventID#{} is a saturation fake (sudden N variation - end)".format(myId))
                            self.setEventClassification(myId, "FAKE SAT")
                            continue

                    # filter against RF interferences artifacts
                    if RFIfilter > 0:
                        # RFI cause quick and strong variations of S
                        # since N is obtained by averaging S in order to
                        # obtain a stable reference, RFI affects
                        # also N, that shows variations proportional to
                        # the disturbance and the AveragedScans setting.
                        # The standard deviation indicates how strong are
                        # this N variations
                        # print("stdDev=", stdDev)
                        halfThr = RFIfilterThreshold / 2

                        if stdDev[1] > RFIfilterThreshold or (
                                stdDev[0] > halfThr and stdDev[1] > halfThr and stdDev[2] > halfThr):
                            print("eventID#{} is a fake due to radio interferences (too high stdev: raise:{}, "
                                  "peak: {}, fall: {}, halfThr={}".format(myId, stdDev[0], stdDev[1], stdDev[2],
                                                                          halfThr))
                            self.setEventClassification(myId, "FAKE RFI")
                            continue

                        # Another criteria to detect RFIs is the ratio between the capture area and the area covered
                        # by the echo. Real echoes usually don't fill up the entire intervalArea
                        if echoArea > (intervalArea * RFIfilterThreshold):
                            print("eventID#{} is a fake due to radio interferences (area too wide: echoArea: {}, "
                                  "intervalArea: {}, threshold={})".format(myId, echoArea, intervalArea,
                                                                           RFIfilterThreshold))
                            self.setEventClassification(myId, "FAKE RFI")
                            continue

                        # Third criteria is the ratio between the nr. of peaks detected in a scan
                        # and the total number of scans covered by the echo
                        if lastingScans > 10 and peaksCount > (lastingScans * RFIfilterThreshold):
                            print("eventID#{} is a fake due to radio interferences (too many peaks: peaksCount: {}, "
                                  "lastingScans: {}, threshold={})".format(myId, peaksCount, lastingScans,
                                                                           RFIfilterThreshold))
                            self.setEventClassification(myId, "FAKE RFI")
                            continue

                    # fakes filters suited for radio carriers tuned at waterfall's center frequency

                    if CAR1filter > 0:
                        # the CAR filter 1 works on the ratio between the echo area and the interval area covered
                        # that must be similar, within a given tolerance.
                        # Fixed carriers tend to have very small echo area, being narrow and regular.
                        # In order to discriminate them from real echoes, thay must last at least the given number of seconds.

                        estimatedArea = lastingScans * freqShift
                        if lastingMs > (carrierSec * 1000) and freqShift == 0 and fuzzyCompare(estimatedArea, echoArea,
                                                                                               CAR1filterThreshold) == 0:
                            print("eventID#{} is fake,  due to a carrier signal, estimatedArea={}, echoArea={}, "
                                  "lastingMs={}, freqShift={}".format(myId, estimatedArea, echoArea, lastingMs,
                                                                      freqShift))
                            self.setEventClassification(myId, "FAKE CAR1")
                            continue

                    if CAR2filter > 0:
                        # The filt 2 finds carriers by checking the istantaneous difference S-N against its average.
                        # If they are too close, into a certain percentage, the event is a carrier
                        fuzzyRatio = fuzzyCompare(diff, avgDiff, CAR2filterThreshold)
                        if lastingMs > (carrierSec * 1000) and fuzzyRatio == 0 and lastingMs > underdenseMs:
                            print(
                                "eventID#{} is fake,  due to a carrier signal, lastingMs={}, fuzzyRatio={}".format(myId,
                                                                                                                   lastingMs,
                                                                                                                   fuzzyRatio))
                            self.setEventClassification(myId, "FAKE CAR2")
                            continue

                    # simple fake filter, lasting based
                    if (lastingMs / 1000) > overdenseSec:
                        print("eventID#{} is fake,  it lasts too long={}ms".format(myId, lastingMs))
                        self.setEventClassification(myId, "FAKE LONG")
                        continue

                    # the event is good
                    if lastingMs <= underdenseMs:
                        print("eventID#{} is underdense, it lasts {}ms".format(myId, lastingMs))
                        self.setEventClassification(myId, "UNDER")
                    elif (lastingMs / 1000) <= overdenseSec:
                        print("eventID#{} is overdense, it lasts {}ms".format(myId, lastingMs))
                        self.setEventClassification(myId, "OVER")
            return True

    def attributeEvents(self, fromID: int, toID: int, overwrite: bool = False):
        """

        :param fromID:
        :param toID:
        :param overwrite: False=process only records without attributes, True=process all records
        :return:

        runs enabled attribute filters on all events between fromId and toId. Unlike classifications -
        which are generated based only on the data present in the automatic_data table - the attributes
        can also be calculated by reading the DATB files present in the database, or even external files.

        Since DATBs have an expiry date, it follows that once the attributes of an event have been calculated,
        it may no longer be possible to recalculate it in the following days. In this case, the last calculated
        value will be retained even if a complete recalculation was requested.
        """

        lowestID = self._getFirstDumpId()
        afDict = self._parent.tabPrefs.afDict()

        if fromID < lowestID:
            fromID = lowestID
        if toID < lowestID:
            toID = lowestID

        df = self.getADpartialFrame(idFrom=fromID, idTo=toID, wantFakes=False)

        self._parent.updateProgressBar(0)

        currentId = 0
        currentRow = 0

        if df is not None:
            raiseRecords = df.loc[(df['event_status'] == 'Raise') & (df['attributes'] == '')]
            peakRecords = df.loc[(df['event_status'] == 'Peak') & (df['attributes'] == '')]
            fallRecords = df.loc[(df['event_status'] == 'Fall') & (df['attributes'] == '')]
            r = 0

            totalRows = len(fallRecords.index)
            self._parent.updateStatusBar(
                "updating attributes on {} events provided with dump files (fakes are ignored)".format(totalRows))

            for idx in fallRecords.index:
                currentRow += 1
                self._parent.updateProgressBar(currentRow, totalRows)

                attribs = fallRecords.loc[idx, 'attributes']
                myId = fallRecords.loc[idx, 'id']

                if attribs == '' or overwrite:
                    print("Calculating attributes for eventID#", myId)
                    # idx indexes the fall event - to browse peak events, idx must be decremented by 1
                    idp = idx - 1
                    # while to browse raise event, idx must be decremented by 2
                    idr = idx - 2
                    mask = (self._adf['id'] == myId)
                    self._adf.loc[mask, 'attributes'] = ''
                    attrDict = dict()
                    for afName in afDict.keys():
                        print("Executing {} on event#{}".format(afName, myId))
                        af = afDict[afName]
                        if af.isFilterEnabled():
                            resultsJson = af.evalFilter(myId)
                            resultDict = json.loads(resultsJson)
                            attrDict = {key: value for (key, value) in
                                        (list(attrDict.items()) + list(resultDict.items()))}

                    # string dump of the attributeDict
                    if len(attrDict.keys()) > 0:
                        self._adf.loc[mask, 'attributes'] = json.dumps(attrDict)

                        # the attributes returned by each filter are joined in a single json string
                        # and stored in adf['attributes'] in the Fall record

                        try:
                            self._parent.eventDataChanges[myId] = True
                            self._dataChangedInMemory = True
                        except IndexError:
                            print("BUG! id=", myId)

            # reordering columns
            cols = ['id', 'daily_nr', 'event_status', 'utc_date', 'utc_time',
                    'timestamp_ms', 'sidereal_utc',
                    'revision', 'up_thr', 'dn_thr', 'S', 'avgS', 'N', 'diff', 'avg_diff',
                    'top_peak_hz', 'std_dev', 'lasting_ms', 'lasting_scans', 'freq_shift',
                    'echo_area', 'interval_area', 'peaks_count', 'LOS_speed', 'scan_ms',
                    'diff_start', 'diff_end', 'classification', 'attributes', 'shot_name', 'dump_name']
            self._adf = self._adf[cols]
        return True

    def getEventClassifications(self, fromID: int, toID: int):
        """
        @param fromID:
        @param toID:
        @return: dataframe
        """
        if self._adf is not None:
            df = self.getADpartialFrame(idFrom=fromID, idTo=toID)
            fallEdges = df.loc[df['event_status'] == 'Fall']
            classifications = fallEdges.loc[:, ['id', 'classification']]
            classifications = classifications.set_index('id')
            return classifications

    def setEventClassification(self, eventID: int, classif: str):
        """
        @param eventID:
        @param classif:
        @return:
        """
        if self._adf is not None:
            mask = (self._adf['id'] == eventID)
            self._adf.loc[mask, 'classification'] = classif
            try:
                self._parent.eventDataChanges[eventID] = True
                self._dataChangedInMemory = True
                return True
            except IndexError:
                print("BUG! eventId=", eventID)
        return False

    def resetClassifications(self):
        """
        Clears the cache file and rebuilds it by reloading the DB
        :return:
        """
        try:
            os.remove(self._adPath)
            print("Cache file {} deleted".format(self._adPath))
        except FileNotFoundError:
            print("no cache file to be deleted")

        if self._loadAutoDataTableFromDB():
            self._dataChangedInMemory = True
            return True
        return False

    def getEventData(self, eventID: int):
        """
        Returns a dataframe with event data, having raise, peak and fall data as columns

        @param eventID:
        @return:
        """
        print("getEventData({})".format(eventID))
        df = self.getADpartialFrame(idFrom=eventID, idTo=eventID)
        if df is not None:
            dfr = df[['utc_time', 'up_thr', 'dn_thr', 'S', 'avgS', 'N', 'diff', 'avg_diff', 'top_peak_hz', 'std_dev',
                      'lasting_ms', 'lasting_scans', 'freq_shift', 'echo_area', 'interval_area', 'peaks_count',
                      'LOS_speed', 'scan_ms', 'diff_start', 'diff_end', 'classification', 'shot_name', 'dump_name']]

            dfc = dfr.round(
                {'up_thr': 3, 'dn_thr': 3, 'S': 3, 'avgS': 3, 'N': 3, 'diff': 3, 'avg_diff': 3, 'std_dev': 3,
                 'diff_start': 3, 'diff_end': 3})
            dft = dfc.transpose()
            dft.columns = ['RAISE', 'PEAK', 'FALL']
            return dft
        return None

    def getADframe(self):
        return self._adf

    def getASframe(self):
        return self._sdf

    def getADfilteredFrame(self, filters: str = ''):
        """
        :param filters:
        :return:
        """
        if len(filters) > 0:
            # filters fdf by class filters, removing spaces in between
            filterList = [item.strip() for item in filters.split(',')]
            df = self._adf.loc[(self._adf['classification'].isin(filterList))].copy()
        else:
            df = self._adf.copy()
        return df

    def getADpartialCompositeFrame(self, dateFrom: str = None, dateTo: str = None, filters: str = '', idFrom: int = 0,
                                   idTo: int = 0):
        """
        Extracts portions of automatic_data in the given dates interval or ID interval
        packing raise, peak and fall edges in a single row
        @param dateFrom:
        @param dateTo:
        @param idFrom:
        @param idTo:
        @return:
        """
        df = self.getADfilteredFrame(filters)

        # creates a dataframe nf from automatic_data with separate
        # time columns for raise, peak, fall states
        nf = None
        if dateFrom is not None and dateTo is not None:
            nf = df.loc[(df['utc_date'] >= dateFrom) & (df['utc_date'] <= dateTo)]
        elif idFrom > 0 and idTo > 0:
            nf = df.loc[(idFrom <= df['id']) & (idTo >= df['id']),]

        if nf is not None and df.empty is False:
            nf = nf.convert_dtypes(convert_integer=True, convert_floating=True)

        # from nf 3 separate df are created, one for raises,
        # one for peaks and one for falls
        raiseDf = nf.loc[(nf['event_status'] == 'Raise'), :].copy()
        raiseDf.rename(columns={'utc_time': 'utc_raise'}, inplace=True)
        raiseDf.drop(columns=['event_status'], inplace=True)

        peakDf = nf.loc[(nf['event_status'] == 'Peak'), :].copy()
        peakDf.rename(columns={'utc_time': 'utc_peak'}, inplace=True)
        peakDf.drop(columns=['event_status'], inplace=True)

        fallDf = nf.loc[(nf['event_status'] == 'Fall'), :].copy()
        fallDf.rename(columns={'utc_time': 'utc_fall'}, inplace=True)
        fallDf.drop(columns=['event_status'], inplace=True)

        # merge the 3 df in a composite dataframe
        cf = fallDf
        cf = cf.merge(raiseDf[['id', 'utc_raise']], on='id', how='left')
        cf = cf.merge(peakDf[['id', 'utc_peak']], on='id', how='left')
        # reordering columns, the utc_raise and utc_peak columns are moved between utc_date and utc_fall
        cols = ['id', 'daily_nr', 'utc_date', 'utc_raise', 'utc_peak', 'utc_fall', 'timestamp_ms', 'sidereal_utc',
                'revision', 'up_thr', 'dn_thr', 'S',
                'avgS', 'N', 'diff', 'avg_diff', 'top_peak_hz', 'std_dev', 'lasting_ms', 'lasting_scans', 'freq_shift',
                'echo_area', 'interval_area', 'peaks_count', 'LOS_speed', 'scan_ms', 'diff_start', 'diff_end',
                'classification', 'attributes', 'shot_name', 'dump_name']
        cf = cf[cols]

        if filters == '':
            # everything has been filtered out
            cf = cf.head(0)
        cf.reset_index(drop=True, inplace=True)
        rcf = cf.round(
            {'up_thr': 3, 'dn_thr': 3, 'S': 3, 'avgS': 3, 'N': 3, 'diff': 3, 'avg_diff': 3, 'std_dev': 3,
             'diff_start': 3, 'diff_end': 3})
        return rcf

    def getADpartialFrame(self, dateFrom: str = None, dateTo: str = None, idFrom: int = 0, idTo: int = 0,
                          wantFakes: bool = True):
        """
        Extracts portions of automatic_data in the given dates interval or ID interval
        @param dateFrom:
        @param dateTo:
        @param idFrom:
        @param idTo:
        @param wantFakes:
        @return:
        """
        df = self._adf.copy()
        nf = None
        if wantFakes:
            if dateFrom is not None and dateTo is not None:
                nf = df.loc[(df['utc_date'] >= dateFrom) & (df['utc_date'] <= dateTo)]
            elif idFrom > 0 and idTo > 0:
                nf = df.loc[(idFrom <= df['id']) & (idTo >= df['id']),]
        else:
            if dateFrom is not None and dateTo is not None:
                nf = df.loc[(df['utc_date'] >= dateFrom) & (df['utc_date'] <= dateTo) & (
                    ~df['classification'].str.contains('FAKE'))]
            elif idFrom > 0 and idTo > 0:
                nf = df.loc[(idFrom <= df['id']) & (idTo >= df['id']) & (~df['classification'].str.contains('FAKE')),]

        if nf is not None and df.empty is False:
            nf = nf.convert_dtypes(convert_integer=True, convert_floating=True)
        return nf

    def getASpartialFrame(self, dateFrom: str = None, dateTo: str = None):
        """
        Extracts portions of automatic_sessions in the given dates interval or ID interval
        Rows are filtered by start_dt field
        @param dateFrom:
        @param dateTo:
        @return:
        """
        df = self._sdf.copy()
        subDf = df
        if dateFrom is not None and dateTo is not None:
            dtFrom = pd.to_datetime(dateFrom)
            dtTo = pd.to_datetime(dateTo)
            dtTo = dtTo + timedelta(days=2) - timedelta(seconds=1)
            # selects the rows with start_dt within the given dates range
            subDf = df[(df['start_DT'] >= dtFrom) & (df['end_DT'] < dtTo)]
        return subDf

    def acqWasRunning(self, dtFrom, deltaMins):
        """
        :param dtFrom: datetime as string
        :param deltaMins.
        :return: true if acq was active all along that period

        the minimum resolution is 10 minutes.
        """
        if dtFrom is not None and deltaMins is not None:
            dtTo = dtFrom + np.timedelta64(deltaMins, 'm')
            dateTo = dtTo.strftime("%Y-%m-%d")
            dateFrom = dtFrom.strftime("%Y-%m-%d")
            df = self.getASpartialFrame(dateFrom, dateTo)
            subDf = df[(df['start_DT'] >= dtFrom) & (df['end_DT'] <= dtTo)]
            totalMins = subDf['delta_min'].sum()
            return totalMins >= (deltaMins / 2)

    def tableTimeSeries(self, df: pd.DataFrame, index: list = None, rows: list = None,
                        columns: list = None):
        """
        Gets a dataframe and joins its rows, column by column,  in a Series
        @param df:       source dataframe
        @param index:   dataframe column to be used as index, containing a date or datetime
        @param rows:    dataframe rows list to be converted to series
                        defaults to None meaning all rows
        @param columns:    dataframe columns list to be converted to series
                        defaults to None meaning all columns


        @return:        pandas series

        """

        # TODO: check if [[]] and/or df.explode() could improve this code:

        newDf = df
        # print(df)
        totalColumns = len(df.columns)
        totalRows = len(df.index)
        if rows is None:
            rows = range(0, totalRows)
        if columns is None:
            columns = range(0, totalColumns)
        if index is not None:
            newDf = df.set_index(index)

        seriesList = list()

        for row in rows:
            qApp.processEvents()
            if index is None:
                serie = newDf.iloc[row, columns].reset_index(drop=True).squeeze()
            else:
                serie = newDf.iloc[row, columns].squeeze()
            # print(serie)
            seriesList.append(serie)

        newSerieList = pd.concat(seriesList).tolist()
        index = newDf.index
        indexList = list()
        lenSer = len(newSerieList)
        lenIdx = newDf.shape[0]
        steps = int(lenSer / lenIdx)

        # build index: by joining multiple rows in a single series
        # the original dataframe index won't cover all the series elements.
        # Here a new index is created, in order to cover all the elements.
        # The new labels are formed by a combination of the original index labels
        # plus a step number
        for label in index:
            qApp.processEvents()
            st = time.strptime(str(label), "%Y-%m-%d")
            for step in range(0, steps):
                qApp.processEvents()
                if steps <= 24:
                    hours = int(step * (24 / steps))
                    fullSt = (st.tm_year, st.tm_mon, st.tm_mday, hours, 0, 0, 0, 0, 0)
                else:
                    stepByMins = step * ((24 * 60) / steps)
                    hours = int(stepByMins / 60)
                    mins = int(stepByMins % 60)
                    fullSt = (st.tm_year, st.tm_mon, st.tm_mday, hours, mins, 0, 0, 0, 0)

                dt = calendar.timegm(fullSt)
                indexList.append(dt)

        newSerie = pd.Series(data=newSerieList, index=indexList)
        # print(newSerie)
        return newSerie

    def splitAndStackDataframe(self, df: pd.DataFrame, maxColumns=24):
        """

        :param df:
        :param maxColumns:
        :return:
        """
        stacked = pd.DataFrame()
        slices = list()
        step = maxColumns
        last = False
        cols = len(df.columns)
        for index in range(0, cols, step):
            qApp.processEvents()
            if (cols - (index + step)) < step:
                step = cols - index
                last = True
            # print("{}..{}".format(index, index + step))
            ds = df.iloc[:, index:index + step].copy()
            ds.reset_index(inplace=True)
            ds = ds.rename(columns={'index': 'utc_date'})
            h = pd.DataFrame(ds.columns)
            h = h.T
            ds.columns = range(ds.shape[1])
            ds = pd.concat([h, ds])
            # ds.to_csv('c:/temp/slice_{}.csv'.format(len(slices)), sep=';', index=False)
            stacked = pd.concat([stacked, ds])
            slices.append(ds)
            if last:
                break
        return stacked      #.fillna('-')

    def makeRMOB(self, df: pd.DataFrame, lastOnly: bool = False):
        dfRMOB = None
        if df is not None and df.shape[0] > 0:
            # RMOB data format is similar to hourly counts table, but covers the latest month in selection
            # and without the rightmost column (row totals) and bottom row (column totals)
            # the date index is split to 3 separate columns year-day-month
            df['date'] = df.index
            df['year'] = df['date'].str.slice(0, 4)
            df['month'] = df['date'].str.slice(5, 7)
            df['day'] = df['date'].str.slice(8, 10)
            elements = ['1']
            coveredYM = df[['year', 'month']].drop_duplicates()
            coveredYM['YM'] = coveredYM['month'] + '_' + coveredYM['year']
            coveredYM.drop(columns=['year', 'month'], inplace=True)
            df = df.set_index('day')
            dfToGenerate = coveredYM['YM'].to_list()
            if lastOnly:
                dfToGenerate = dfToGenerate[-1:]
            dfMonth = None
            for dfMonthName in dfToGenerate:
                qApp.processEvents()
                elements = dfMonthName.split('_')
                month = int(elements[0])
                year = int(elements[1])
                r = range(0, 31)
                dfRMOB = pd.DataFrame(
                    columns=['00h', '01h', '02h', '03h', '04h', '05h', '06h', '07h', '08h', '09h', '10h', '11h', '12h',
                             '13h', '14h', '15h', '16h', '17h', '18h', '19h', '20h', '21h', '22h', '23h'],
                    index=[f'{n + 1:02}' for n in r])
                dfRMOB.fillna(value=-1, inplace=True)

                dfMonth = df[df.year == elements[1]]
                dfMonth = dfMonth[dfMonth.month == elements[0]]
                dfMonth.drop(columns=['year', 'month', 'date'], inplace=True)

            dfRMOB.loc[dfMonth.index] = dfMonth
            monthNum = int(elements[0])
            year = int(elements[1])
            monthName = calendar.month_abbr[monthNum].lower()
            dfRMOB.index.name = monthName
            return dfRMOB, monthNum, year

    import pandas as pd
    from datetime import datetime, timedelta
    import pandas as pd
    from datetime import datetime, timedelta

    def makeCountsDf(
            self, df: pd.DataFrame, dtStart: str, dtEnd: str, dtRes: str, filters: str = '',
            compensate: bool = False, considerBackground: bool = False, totalRow: bool = False,
            totalColumn: bool = False, placeholder: int = 0):

        """
        Computes event counts grouped by day and time resolution.

        :param df: DataFrame containing event data.
        :param dtStart: Start date in 'YYYY-MM-DD' format.
        :param dtEnd: End date in 'YYYY-MM-DD' format.
        :param dtRes: Time resolution ('D', 'h', '10T').
        :param filters: Filters to apply on the 'classification' column.
        :param compensate: Whether to compensate counts using the background.
        :param considerBackground: Whether to subtract the background from counts.
        :param totalRow: Adds a row with totals for each column.
        :param totalColumn: Adds a column with totals for each row.
        :param placeholder: value to replace NaNs
        :return: DataFrame with the requested counts.
        """
        # Filter the DataFrame for relevant events
        df = df[df['event_status'] == 'Fall'].copy()
        if filters:
            filterList = [item.strip() for item in filters.split(',')]
            df = df[df['classification'].isin(filterList)].copy()

        # Create a 'datetime' column combining date and time
        df['datetime'] = pd.to_datetime(df['utc_date'] + ' ' + df['utc_time'])
        dtEndInclusive = (datetime.strptime(dtEnd, '%Y-%m-%d') + timedelta(days=1)).strftime('%Y-%m-%d')

        # Generate time intervals and column names
        dtRange = pd.date_range(dtStart, dtEndInclusive, freq=dtRes)
        columns = self._generateColumns(dtRange, dtRes)
        odf = pd.DataFrame(columns=columns, dtype='int32')

        # Iterate over time intervals to calculate counts
        for i in range(len(dtRange) - 1):
            dtFrom = dtRange[i]
            dtTo = dtRange[i + 1]
            subDf = df[(df['datetime'] >= dtFrom) & (df['datetime'] < dtTo)]
            count = len(subDf)

            hole = False
            if count == 0 and 'ACQ ACT' in filters:
                if dtRes == 'D' and (not self.acqWasRunning(dtFrom, 86400)):
                    count = placeholder
                    hole = True

                if dtRes == 'h' and (not self.acqWasRunning(dtFrom, 3600)):
                    count = placeholder
                    hole = True

                if dtRes == '10T' and (not self.acqWasRunning(dtFrom, 600)):
                    count = placeholder
                    hole = True

                if not hole:
                    # Adjust counts based on background or compensation rules
                    count = self._adjustCount(count, dtRes, compensate, considerBackground)

            row = dtFrom.date().strftime('%Y-%m-%d')
            column = self._formatColumnName(dtFrom, dtRes)

            if row not in odf.index:
                odf.loc[row] = placeholder
            odf.at[row, column] = count

        # Add totals for rows and columns
        if totalColumn:
            odf['Total'] = odf.sum(axis=1)
        if totalRow:
            odf.loc['Total'] = odf.sum()

        return odf

    def _adjustCount(self, count, dtRes, compensate, considerBackground):
        """Adjusts the count based on background and compensation rules."""
        background = 0  # Add logic to calculate the background if needed
        if compensate and count < background:
            count = background
        if considerBackground:
            count -= background
        return max(count, 0)

    def _formatColumnName(self, dtFrom, dtRes):
        """
            Formats the column name based on the time resolution.

            :param dtFrom: Start datetime of the interval.
            :param dtRes: Time resolution ('D', 'h', '10T').
            :return: Formatted column name.
            """
        if dtRes == 'D':
            return dtFrom.strftime('%Y-%m-%d')
        elif dtRes == 'h':
            return f"{dtFrom.hour:02}h"
        elif dtRes == '10T':
            return f"{dtFrom.hour:02}h{dtFrom.minute:02}m"
        else:
            raise ValueError(f"Resolution {dtRes} not supported.")

    def _generateColumns(self, dtRange, dtRes):
        """
           Generates column names based on the time resolution.

           :param dtRange: Range of datetime intervals.
           :param dtRes: Time resolution ('D', 'h', '10T').
           :return: List of formatted column names.
        """
        if dtRes == 'D':
            return [dt.strftime('%Y-%m-%d') for dt in dtRange[:-1]]
        elif dtRes == 'h':
            return [f'{hour:02}h' for hour in range(24)]
        elif dtRes == '10T':
            return [f'{hour:02}h{minute:02}m' for hour in range(24) for minute in range(0, 60, 10)]
        else:
            raise ValueError(f"Resolution {dtRes} not supported.")

    def _makeAverageDf(self, df: pd.DataFrame, dtStart: str, dtEnd: str, dtRes: str, targetColumn: str, dtDec: int = 1,
                       filters: str = '', highestAvgRow: bool = True, highestAvgColumn: bool = True):
        """
        Generates a DataFrame with averages of the specified column at the required resolution.

        @param df: Source DataFrame.
        @param dtStart: Start date in 'YYYY-MM-DD' format.
        @param dtEnd: End date in 'YYYY-MM-DD' format.
        @param dtRes: Resolution ('D' = daily, 'h' = hourly, '10T' = every 10 minutes).
        @param targetColumn: Column to calculate averages on ('diff' or 'lasting_ms').
        @param dtDec: number of decimals to show, zero=integer
        @param filters: Classification filters (comma-separated).
        @param highestAvgRow: If True, appends a row with the max of each column.
        @param highestAvgColumn: If True, adds a column with the max of each row.
        @return: DataFrame with average values.
        """
        df.set_index('id', inplace=True, drop=False)
        df = df.loc[df['event_status'] == 'Fall'].copy()

        # Apply classification filters
        filterList = [item.strip() for item in filters.split(',')]
        fdf = df.loc[df['classification'].isin(filterList)].copy()

        # Add datetime column
        fdf['datetime'] = pd.to_datetime(fdf['utc_date'] + ' ' + fdf['utc_time'])
        dtEndInclusive = datetime.strptime(dtEnd, '%Y-%m-%d') + timedelta(days=1)

        # Initialize output DataFrame
        columns = []
        intvlRange = None
        dtRange = pd.date_range(dtStart, dtEndInclusive, freq='D')
        intvlRange = pd.date_range(dtRange[0], dtRange[-1], freq=dtRes)
        if dtRes == 'D':
            columns = [f"{dt.day:02}" for dt in dtRange]
            odf = pd.DataFrame(columns=columns, dtype='float')

        elif dtRes == 'h':
            columns = [f"{hour:02}h" for hour in range(24)]
            odf = pd.DataFrame(columns=columns, dtype='float', index=dtRange.strftime('%Y-%m-%d'))

        elif dtRes == '10T':
            columns = [f"{hour:02}h{minute:02}m" for hour in range(24) for minute in range(0, 60, 10)]
            odf = pd.DataFrame(columns=columns, dtype='float', index=dtRange.strftime('%Y-%m-%d'))

        else:
            raise ValueError(f"Resolution '{dtRes}' not implemented.")

        # Fill output DataFrame with averages
        # for start, end in zip(dtRange[:-1], dtRange[1:]):
        for start, end in zip(intvlRange[:-1], intvlRange[1:]):
            qApp.processEvents()
            subdf = fdf[(fdf['datetime'] >= start) & (fdf['datetime'] < end)]
            avg = subdf[targetColumn].mean() if not subdf.empty else float('nan')
            avg = round(avg, dtDec) if not pd.isna(avg) else None

            if dtRes == 'D':
                odf.at[start.strftime('%Y-%m-%d'), f"{start.day:02}"] = avg
            elif dtRes == 'h':
                odf.at[start.strftime('%Y-%m-%d'), f"{start.hour:02}h"] = avg
            elif dtRes == '10T':
                odf.at[start.strftime('%Y-%m-%d'), f"{start.hour:02}h{start.minute:02}m"] = avg

        odf = odf.drop(odf.index[-1])

        # Add highest average row/column if required
        if highestAvgColumn:
            odf['Highest average'] = odf.max(axis=1)
        if highestAvgRow:
            highestRow = odf.max(axis=0)
            odf.loc['Highest average'] = highestRow
            if highestAvgColumn:
                odf.at['Highest average', 'Highest average'] = None

        # NOTE: when plotting, floats (dtDec > 0) are better because NaN values aren't plotted
        # while on a table they appear as 'NaN'
        # Integers (dtDec=0) instead can't contain NaN so it will be replaced with -1
        # but this is clean in a table but are nasty when plotting.

        return odf if dtDec != 0 else odf.fillna(-1).astype(int)

    def makePowersDf(self, df: pd.DataFrame, dtStart: str, dtEnd: str, dtRes: str, filters: str = '',
                     highestAvgRow: bool = False, highestAvgColumn: bool = False):
        return self._makeAverageDf(df, dtStart, dtEnd, dtRes, 'diff', 1, filters, highestAvgRow, highestAvgColumn)

    def makeLastingsDf(self, df: pd.DataFrame, dtStart: str, dtEnd: str, dtRes: str, filters: str = '',
                       highestAvgRow: bool = False, highestAvgColumn: bool = False):
        return self._makeAverageDf(df, dtStart, dtEnd, dtRes, 'lasting_ms', 0, filters, highestAvgRow, highestAvgColumn)






    def dbCommit(self):
        if self._db is not None:
            q = QSqlQuery(self._db)
            cmd = "COMMIT"
            result = q.exec(cmd)
            if result > 0:
                return True
            e = QSqlError(q.lastError())
            print("DataSource.dbCommit() ", e.text())
        return False

    def isDateInRange(self, target: str):
        dtFrom = datetime.fromisoformat(self._parent.fromDate)
        dtTo = datetime.fromisoformat(self._parent.toDate)
        dtTarget = datetime.fromisoformat(target)
        return dtFrom <= dtTarget <= dtTo
