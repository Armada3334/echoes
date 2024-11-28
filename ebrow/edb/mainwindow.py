"""
******************************************************************************

    Echoes Data Browser (Ebrow) is a data navigation and report generation
    tool for Echoes.
    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
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

import os
import time
from platform import uname
from datetime import datetime
from pathlib import Path

# from pyqtspinner import WaitingSpinner
from PyQt5.QtCore import Qt, QDate, QEvent, QTimer
from PyQt5.QtGui import QColor, QFontDatabase
from PyQt5.QtWidgets import QMainWindow, QApplication, QMessageBox, qApp

# from splash import SplashWindow
from .ui_mainwindow import Ui_ebrowWindow
from .settings import Settings
from .datasource import DataSource
from .screenshots import ScreenShots
from .rtsconfig import RTSconfig
from .stats import Stats
from .prefs import Prefs
from .utilities import createCustomColormapsDict, cleanFolder
from .reports import Report
from .logprint import print, logPrintVerbose, removeLogFile

APPVERSION = "0.1.83"


class MainWindow(QMainWindow):
    TWMAIN_DB = 0
    TWMAIN_EVENTS = 1
    TWMAIN_STATISTICS = 2
    TWMAIN_RTS = 3
    TWMAIN_REPORT = 4
    TWMAIN_PREFS = 5
    WINDOW_TITLE = "Echoes Data Browser"
    DEFAULT_PROGRESS_STEP = 1

    def __init__(self, app: QApplication, dbFile: str=None, isBatchRMOB: bool = False, isBatchReport: bool = False, isBatchXLSX = False, verboseLog: bool = True):
        super(MainWindow, self).__init__()

        self.app = app
        self.dbFile = dbFile
        self.version = APPVERSION
        self.toDate = None
        self.fromDate = None
        self.dataSource = None
        self.filteredIDs = []
        self.filteredDailies = []
        self.currentDate = None
        self.covDays = None
        self.dbOk = False

        self.covID = 0
        self.currentIndex = -1  # index in filteredIDs to get the currentID
        self.busyCount = 0
        self.currentID = 0
        self.currentDailyNr = 0
        self.toDailyNr = 0
        self.fromId = 0
        self.toId = 0
        self.coverage = 0
        self.isQuitting = False
        self.isReporting = False
        self.isAutoExport = False
        logPrintVerbose(verboseLog)

        self.cmapDict = createCustomColormapsDict()
        self.isBatchRMOB = isBatchRMOB
        self.isBatchReport = isBatchReport
        self.isBatchXLSX = isBatchXLSX

        self._fontDB = QFontDatabase()
        self._fontDB.addApplicationFont(":/resources/fonts/Gauge-Regular.ttf");
        self._fontDB.addApplicationFont(":/resources/fonts/Gauge-Heavy.ttf");
        self._ui = Ui_ebrowWindow()
        self._ui.setupUi(self)
        self._splash = None
        self.tabScreenshots = None
        self.tabStats = None
        self.tabRTS = None
        self.tabReport = None
        self.tabPrefs = None
        self._cleanExportReq = False
        self._progress = 0
        self._progressTarget = 1
        self._progressPercent = 100

        # list of events which class will be updated by pressing the DB update button
        self.eventDataChanges = list()
        self.classifications = None

        self._ui.pbSave.setEnabled(False)
        self._ui.pbSubset.setEnabled(False)
        self._ui.pbLastMonth.setEnabled(False)
        self._ui.pbLastYear.setEnabled(False)
        self._ui.pbClassReset.setEnabled(False)

        self.appPath = os.path.dirname(os.path.realpath(__file__))
        print("appPath=", self.appPath)
        if not os.path.isdir(self.appPath):
            print("invalid appPath")
            if uname().system == 'Windows':
                # this is the exe version of ebrow
                # so, its appPath matches with Echoes one
                self.appPath = Path(os.environ['programfiles']) / "GABB" / "echoes"
                print("new appPath=", self.appPath)
            else:
                print("Unable to generate report, assets directory missing")
                return

        self._spinner = None
        '''
        self._spinner = WaitingSpinner(
            self,
            roundness=52,
            fade=12,
            radius=90,
            lines=70,
            line_length=40,
            line_width=4,
            speed=0.3,
            color=QColor("powderblue")
        )
        '''
        self.filterTags = ['OVER', 'UNDER', 'FAKE RFI', 'FAKE ESD', 'FAKE CAR1', 'FAKE CAR2', 'FAKE SAT', 'FAKE LONG',
                           'ACQ ACT']
        self.filterChecks = [self._ui.chkOverdense, self._ui.chkUnderdense, self._ui.chkFakeRfi, self._ui.chkFakeEsd, self._ui.chkFakeCar1,
                             self._ui.chkFakeCar2, self._ui.chkFakeSat, self._ui.chkFakeLong, self._ui.chkFakeLong]
        # last one is unused but these 2 lists must have same length

        self.filterCheckStats = [self._ui.chkOverdense_2, self._ui.chkUnderdense_2, self._ui.chkFakeRfi_2, self._ui.chkFakeEsd_2,
                                 self._ui.chkFakeCar1_2, self._ui.chkFakeCar2_2, self._ui.chkFakeSat_2,
                                 self._ui.chkFakeLong_2, self._ui.chkAcqActive_2]

        self.filterCheckReport = [self._ui.chkOverdense_3, self._ui.chkUnderdense_3, self._ui.chkFakeRfi_3, self._ui.chkFakeEsd_3,
                                  self._ui.chkFakeCar1_3, self._ui.chkFakeCar2_3, self._ui.chkFakeSat_3,
                                  self._ui.chkFakeLong_3]

        self._ui.lbVersion.setText(self.version)
        self._ui.gbSrvExport.setVisible(False)
        self.workingDir = Path(Path.home(), Path("ebrow"))
        self.exportDir = Path(self.workingDir, "exports")
        self._iniFilePath = str(Path(self.workingDir, "ebrow.ini"))
        self._settings = Settings(self._iniFilePath)
        self._settings.load()
        windowGeometry = self._settings.readSettingAsObject('geometry')
        self.setGeometry(windowGeometry)

        # the displayed coverage matches the last values from ebrow.ini
        (self.fromDate, self.toDate) = self._settings.coverage()
        self.fromQDate = QDate().fromString(self.fromDate, 'yyyy-MM-dd')
        self.toQDate = QDate().fromString(self.toDate, 'yyyy-MM-dd')
        self._ui.dtFrom.setDate(self.fromQDate)
        self._ui.dtTo.setDate(self.toQDate)

        Path.mkdir(self.workingDir, parents=True, exist_ok=True)
        self.updateStatusBar("working directory {} ok ".format(self.workingDir))
        Path.mkdir(self.exportDir, parents=True, exist_ok=True)

        os.chdir(self.workingDir)
        self._ui.pbOpen.clicked.connect(self._openDataSource)
        self._ui.pbSave.clicked.connect(self._updateDataSource)
        self._ui.pbSubset.clicked.connect(self._createSubset)
        self._ui.pbClassReset.clicked.connect(self._resetClassifications)
        self._ui.pbQuit.clicked.connect(self._quit)
        self._ui.pbLastMonth.clicked.connect(self.coverLastMonth)
        self._ui.pbLastYear.clicked.connect(self.coverLastYear)
        self._ui.dtFrom.dateChanged.connect(self.getCoverage)
        self._ui.dtTo.dateChanged.connect(self.getCoverage)
        self._ui.twMain.currentChanged.connect(self._handleTabChanges)

        # the following tabs remain hidden until a db is opened:
        self._ui.twMain.setTabVisible(1, False)  # Screenshots
        self._ui.twMain.setTabVisible(2, False)  # Statistics
        self._ui.twMain.setTabVisible(3, False)  # RTS config
        self._ui.twMain.setTabVisible(4, False)  # Report

        self._initTimer = QTimer()
        self._initTimer.timeout.connect(self.postInit)
        self._initTimer.start(500)
        if not (isBatchRMOB or isBatchReport or isBatchXLSX):
            # self._splash = SplashWindow(self, ":/splashscreen")
            # self._splash.show()
            pass

    # public methods:

    def checkExportDir(self, checkThisDir):
        if not self._cleanExportReq:
            self._cleanExportReq = True
            if not os.listdir(checkThisDir):
                self.updateStatusBar("export directory {} ok".format(checkThisDir))
            else:
                self.updateStatusBar("export directory {} not empty".format(checkThisDir))
                if not (self.isBatchRMOB or self.isBatchReport or self.isBatchXLSX):
                    if self.busyCount > 0:
                        # hides temporarily the wait cursor if visible
                        QApplication.restoreOverrideCursor()

                    result = QMessageBox.question(self, self.WINDOW_TITLE,
                                                  "Export directory not empty, delete content before usage?")
                    if self.busyCount > 0:
                        # shows the wait cursor again
                        QApplication.setOverrideCursor(Qt.WaitCursor)

                    if result == QMessageBox.StandardButton.Yes:
                        if cleanFolder(str(checkThisDir)):
                            self.updateStatusBar("export directory {} cleaned".format(checkThisDir))
                            return True
                else:
                    if cleanFolder(str(checkThisDir)):
                        self.updateStatusBar("export directory {} cleaned".format(checkThisDir))
                        return True

        return False

    def eventFilter(self, obj, event):
        if self._settings.readSettingAsBool('tooltipDisabled'):
            if event.type() == QEvent.ToolTip:
                return True

        return super(MainWindow, self).eventFilter(obj, event)

    def postInit(self):
        """
        initialization tasks executed after the main window has been shown
        @return:
        """
        removeLogFile()
        print("================== STARTING EBROW =====================")
        print("post-initialization")
        self._initTimer.stop()
        self.busy(True)
        # the following tabs remain hidden until a db is opened:
        self._ui.twMain.setTabVisible(1, False)  # Screenshots
        self._ui.twMain.setTabVisible(2, False)  # Statistics
        self._ui.twMain.setTabVisible(3, False)  # RTS config
        self._ui.twMain.setTabVisible(4, False)  # Report
        self.updateStatusBar("Creating tabs")
        self.tabScreenshots = ScreenShots(self, self._ui, self._settings)
        self.tabStats = Stats(self, self._ui, self._settings)
        self.tabRTS = RTSconfig(self, self._ui, self._settings)
        self.tabPrefs = Prefs(self, self._ui, self._settings)
        self.tabReport = Report(self, self._ui, self._settings)

        self._ui.lbVersion.setText(APPVERSION)
        self.currentDate = self._settings.readSettingAsString('currentDate')

        '''
        INUTILE FINCHE IL DB NON E' CARICATO
        # updates the selected range
        # to cover the current month until today
        today = datetime.today()
        if self.isBatchRMOB:
            self.coverLastMonth()
            # self._ui.dtFrom.setDate(QDate(today.year, today.month, 1))
        else:
            dateFrom = self._settings.readSettingAsString('dateFrom')
            qdateFrom = QDate().fromString(dateFrom, 'yyyy-MM-dd')
            dateTo = self._settings.readSettingAsString('dateTo')
            qdateTo = QDate().fromString(dateTo, 'yyyy-MM-dd')
            # self._ui.dtFrom.setDate(qdateFrom)
            # self._ui.dtTo.setDate(QDate(today.year, today.month, today.day))
        '''
        print("isBatchXLSX=",self.isBatchXLSX)
        if self.isBatchRMOB or self.isBatchReport or self.isBatchXLSX or self.dbFile is not None:
            self.updateStatusBar("Loading database file")
            self._openDataSource(noGUI=True)
            if self._ui.chkAutosave.isChecked() is True:
                self.updateStatusBar("Self saving the database file after classifications")
                self._updateDataSource()

            if self.isBatchRMOB:
                self.updateStatusBar("Self updating and sending RMOB files (--rmob specified)")
                self.coverLastMonth()
                self.getRMOBdata()
                self.close()

            elif self.isBatchReport or self.isBatchXLSX:
                self.updateStatusBar("Self generating report (--report and/or --xlsx specified)")
                self.coverLastMonth()
                self.tabReport.updateTabReport()
                self.close()

        else:
            if self._splash:
                self._splash.end()
        self.updateProgressBar()  # hide the progressbar
        # self._parent.checkExportDir(self._exportDir)
        self.busy(False, force=True)

    def getRMOBdata(self, sendOk: bool = True):
        # automatic export of RMOB file
        # for runtime and reporting
        return self.tabStats.updateRMOBfiles(sendOk)

    def getSummaryPlot(self, filters):
        # automatic creation of summary plot
        # for reporting
        return self.tabStats.updateSummaryPlot(filters)

    def closeEvent(self, event):
        # closing the window with the X button
        self._quit()
        event.accept()

    def resizeEvent(self, event):
        QMainWindow.resizeEvent(self, event)
        '''
        if self.tabScreenshots is not None:
            self.tabScreenshots.updateThumbnails()
        '''

    def busy(self, wantBusy: bool, force: bool = False, spinner: bool = True):
        if wantBusy:
            if self.busyCount == 0:
                # disable all pushbuttons when waiting
                self.updateStatusBar("Busy")
                self._ui.pbShotExp.setEnabled(False)
                self._ui.pbStatTabExp.setEnabled(False)
                self._ui.pbCfgTabExp.setEnabled(False)
                self._ui.pbRMOB.setEnabled(False)
                self._ui.pbRefresh.setEnabled(False)
                self._ui.pbRefresh_2.setEnabled(False)
                self._ui.pbRefresh_3.setEnabled(False)
                self._ui.pbReportXLSX.setEnabled(False)
                self._ui.pbReportHTML.setEnabled(False)
                self._ui.pbReset.setEnabled(False)
                self._ui.pbSave.setEnabled(False)
                self._ui.pbSubset.setEnabled(False)
                self._ui.pbOpen.setEnabled(False)
                self._ui.pbClassReset.setEnabled(False)
                self._ui.pbEditParms.setEnabled(False)
                self._ui.pbAdd.setEnabled(False)
                self._ui.pbDel.setEnabled(False)
                self._ui.pbCalc.setEnabled(False)
                self._ui.dtFrom.setEnabled(False)
                self._ui.dtTo.setEnabled(False)
                if spinner and self._spinner is not None:
                    self._spinner.start()
                    self._spinner.raise_()
                else:
                    QApplication.setOverrideCursor(Qt.WaitCursor)
            self.busyCount += 1
            print("Busy [{}]".format(self.busyCount))
        else:
            if self.busyCount > 0:
                self.busyCount -= 1
                if self.busyCount == 0 or force:
                    self.busyCount = 0
                    self.updateProgressBar()  # hide progress bar
                    self.updateStatusBar("Ready")
                    # self.updateStatusBar("Ready (force={})".format(force))
                    self._ui.pbOpen.setEnabled(True)
                    if self.dbOk:
                        self._ui.pbShotExp.setEnabled(True)
                        self._ui.pbStatTabExp.setEnabled(True)
                        self._ui.pbCfgTabExp.setEnabled(True)
                        self._ui.pbRMOB.setEnabled(True)
                        self._ui.pbRefresh.setEnabled(True)
                        self._ui.pbRefresh_2.setEnabled(True)
                        self._ui.pbRefresh_3.setEnabled(True)
                        self._ui.pbReportXLSX.setEnabled(True)
                        self._ui.pbReportHTML.setEnabled(True)
                        self._ui.pbReset.setEnabled(True)
                        self._ui.pbLastMonth.setEnabled(True)
                        self._ui.pbLastYear.setEnabled(True)
                        self._ui.pbSave.setEnabled(True)
                        self._ui.pbSubset.setEnabled(True)
                        self._ui.pbClassReset.setEnabled(True)
                        self._ui.pbEditParms.setEnabled(True)
                        self._ui.pbAdd.setEnabled(True)
                        self._ui.pbDel.setEnabled(True)
                        self._ui.pbCalc.setEnabled(True)
                        self._ui.dtFrom.setEnabled(True)
                        self._ui.dtTo.setEnabled(True)
                    if spinner and self._spinner is not None:
                        self._spinner.stop()
                    QApplication.restoreOverrideCursor()
                else:
                    print("Still busy [{}]".format(self.busyCount))
        self.app.processEvents()
        time.sleep(0.1)

    def updateStatusBar(self, text: str, logOnly: bool = False):
        sText = text
        if self.isReporting:
            if self.isAutoExport:
                sText = "AUTOEXPORT IN PROGRESS: {}".format(text)
            else:
                sText = "REPORT IN PROGRESS: {}".format(text)
        elif self.isBatchRMOB:
            sText = "RMOB EXPORT IN PROGRESS: {}".format(text)

        if not logOnly:
            self._ui.lbStatus.setText(sText)
        print(sText)
        fullText = "{} - {}".format(datetime.now(), sText)
        self._ui.pteStatus.appendPlainText(fullText)
        if self.dataSource and self.dataSource.dataReady:
            todayIDs = self.dataSource.getIDsOfTheDay(self.currentDate)
            if todayIDs is not None and todayIDs is not []:
                self._ui.lbTotal.setNum(self.covID)
                self._ui.lbDate.setText(self.currentDate)
                self._ui.lbCurDaily.setNum(len(todayIDs))
                self._ui.lbCurFiltered.setNum(len(self.filteredIDs))
        if self.app is not None:
            self.app.processEvents()

    def updateProgressBar(self, doneItems: int = None, totalItems: int = None, step: int = DEFAULT_PROGRESS_STEP):
        if totalItems is not None:
            self._progressTarget = int(totalItems)

        if doneItems is None:
            # hide progressbar
            self._ui.pbDB.setVisible(False)
            self._progressPercent = 0
            self._ui.pbDB.setValue(self._progressPercent)
        else:
            self._ui.pbDB.setVisible(True)
            doneItems = int(doneItems)
            step = int(step)
            self._progress = (doneItems / self._progressTarget) * 100
            self._progressPercent = int(self._progress)
            if self._progressPercent < 100:
                self._ui.pbDB.setValue(self._progressPercent)
                self._ui.pbDB.setVisible(True)
            else:
                self._ui.pbDB.setVisible(False)
                self._progressPercent = 0
                self._ui.pbDB.setValue(self._progressPercent)
        if qApp is not None:
            qApp.processEvents()

    def infoMessage(self, notice: str, msg: str):
        self.busy(False, force=True)
        errorbox = QMessageBox()
        errorbox.setWindowTitle(self.WINDOW_TITLE)
        errorbox.setText(str(notice) + '\n' + str(msg))
        errorbox.setIcon(QMessageBox.Information)
        errorbox.exec_()

    def confirmMessage(self, notice: str, msg: str):
        self.busy(False, force=True)
        errorbox = QMessageBox()
        errorbox.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
        errorbox.setWindowTitle(self.WINDOW_TITLE)
        errorbox.setText(str(notice) + '\n' + str(msg))
        errorbox.setIcon(QMessageBox.Question)
        button = errorbox.exec_()
        return button == QMessageBox.Ok

    # protected methods:

    def getCoverage(self, recursion: int = 0):
        if self.dataSource is None or self.dbOk is False:
            return False

        # gets the DB date coverage
        (qDateDBfrom, qDateDBto) = self.dataSource.QDateCoverage()
        if self.dbOk and (qDateDBfrom is None or qDateDBto is None):
            self.dbOk = False

        else:
            # DB date coverage ok
            self._ui.lbDBfilename.setText("{} ".format(self.dataSource.name()))
            if self.fromQDate >= qDateDBfrom and self.toQDate <= qDateDBto:
                # the required coverage in compatible with DB coverage
                pass

            elif self.fromQDate >= qDateDBfrom and self.toQDate > qDateDBto:
                # the required coverage overlaps the DB's coverage
                # exceeding post its end, so the end date must be fixed
                self._ui.dtTo.setDate(qDateDBto)

            elif self.fromQDate < qDateDBfrom:
                # if the data start after the 1st of the month, the beginning date
                # is taken from the database
                self._ui.dtFrom.setDate(qDateDBfrom)

            else:
                # in any other case
                # the interval is reinitialized selecting the last month
                # present in database. Note: this call generates one recursion
                self.coverLastMonth()

            qDateFrom = self._ui.dtFrom.date()
            qDateTo = self._ui.dtTo.date()
            defaultDate = self._settings.readSettingAsString('currentDate')
            defaultQDate = QDate().fromString(defaultDate, 'yyyy-MM-dd')

            self.fromDate = qDateFrom.toString("yyyy-MM-dd")
            self.toDate = qDateTo.toString("yyyy-MM-dd")
            self.coverage = qDateFrom.daysTo(qDateTo) + 1
            self.covDays = list()
            date = qDateFrom
            for day in range(0, self.coverage):
                dateStr = date.toString("yyyy-MM-dd")
                self.filteredIDs, self.filteredDailies = self.dataSource.getFilteredIDsOfTheDay(dateStr)
                if len(self.filteredIDs) > 0:
                    self.covDays.append(dateStr)
                    if defaultDate == '' or not qDateFrom <= defaultQDate <= qDateTo:
                        # if default date is invalid
                        self.currentDate = dateStr  # takes by default the most recent date with images
                date = date.addDays(1)

            # self.updateStatusBar("coverage {} days, from {} to {}:".format(self.coverage, self.fromDate, self.toDate))
            print("coverage {} days, from {} to {}".format(self.coverage, self.fromDate, self.toDate))
            self.filteredIDs, self.filteredDailies = self.dataSource.getFilteredIDsOfTheDay(self.currentDate)
            self._ui.lbCovFrom.setText(self.fromDate)
            self._ui.lbCovTo.setText(self.toDate)
            self._settings.writeSetting('dateFrom', self.fromDate)
            self._settings.writeSetting('dateTo', self.toDate)
            if self.tabScreenshots.getCoverage() is False:
                self.tabScreenshots.getCoverage(selfAdjust=True)
                dt = time.strptime(self.fromDate, "%Y-%m-%d")
                self._ui.dtFrom.setDate(QDate(dt.tm_year, dt.tm_mon, dt.tm_mday))
                dt = time.strptime(self.toDate, "%Y-%m-%d")
                self._ui.dtTo.setDate(QDate(dt.tm_year, dt.tm_mon, dt.tm_mday))
                if recursion == 0:
                    recursion += 1
                    self.getCoverage(recursion)
                # self.updateStatusBar("Fixed day coverage from {} to {}, total {} IDs, from {} to {}:".format(
                #    self.fromDate, self.toDate, self.covID, self.fromId, self.toId))
                print("Fixed day coverage from {} to {}, total {} IDs, from {} to {}".format(
                    self.fromDate, self.toDate, self.covID, self.fromId, self.toId))
                return True

            self.covID = (self.toId - self.fromId) + 1
            self.updateStatusBar("coverage {} IDs, from {} to {}".format(self.covID, self.fromId, self.toId))
        return True

    def coverLastMonth(self):
        (qDateFrom, qDateTo) = self.dataSource.QDateCoverage()
        self._ui.dtTo.setDate(qDateTo)
        (y, m, d) = qDateTo.getDate()
        qDateFrom.setDate(y, m, 1)
        self._ui.dtFrom.setDate(qDateFrom)

    def coverLastYear(self):
        (qDateFrom, qDateTo) = self.dataSource.QDateCoverage()
        self._ui.dtTo.setDate(qDateTo)
        (y, m, d) = qDateTo.getDate()
        qDateFrom.setDate(y, 1, 1)
        self._ui.dtFrom.setDate(qDateFrom)


    def _quit(self):
        if not self.isQuitting:
            self.isQuitting = True
            if self.dataSource is not None:
                if not (self.isBatchRMOB or self.isBatchReport or self.isBatchXLSX):
                    self._updateDataSource()
                self.dataSource.closeFile()
            self._settings.writeSetting('geometry', self.geometry())
            qDateFrom = self._ui.dtFrom.date()
            qDateTo = self._ui.dtTo.date()
            self.fromDate = qDateFrom.toString("yyyy-MM-dd")
            self.toDate = qDateTo.toString("yyyy-MM-dd")
            self._settings.writeSetting('dateFrom', self.fromDate)
            self._settings.writeSetting('dateTo', self.toDate)
            self._settings.save()
            # if not (self.isBatchRMOB or self.isBatchReport):
            self.close()

    def _handleTabChanges(self):
        idx = self._ui.twMain.currentIndex()
        if self.dataSource is None:
            # only 2 tabs visible if no DB opened
            if idx == 1:  # self.TWMAIN_PREFS:
                self.tabPrefs.updateTabPrefs()
        else:
            if idx == self.TWMAIN_EVENTS:
                self.tabScreenshots.updateTabEvents()
            if idx == self.TWMAIN_STATISTICS:
                self.tabStats.updateTabStats()
            if idx == self.TWMAIN_RTS:
                self.tabRTS.updateTabRTSconfig()
            if idx == self.TWMAIN_PREFS:
                self.tabPrefs.updateTabPrefs()
            if idx == self.TWMAIN_REPORT:
                self.tabReport.updateTabReport()
        self._settings.save()

    def _updateDataSource(self):
        print("_updateDataSource()")
        if self.dataSource is not None:
            if True in self.eventDataChanges:
                if (self._settings.readSettingAsBool('autosaving')
                        or
                        self.confirmMessage("About to saving the changes on cache file.",
                                            "Press OK to confirm, Cancel to skip")):
                    self.busy(True)
                    if self._overrideClassifications():
                        self.updateStatusBar("Saving changes to cache...")
                        if self.dataSource.updateFile():
                            self.updateStatusBar("Changes saved")
                            self.eventDataChanges = [False] * (self.fromId + self.covID)
                            self.busy(False)
                            return True
                    self.busy(False)
            self.updateStatusBar("Cache file doesn't need updating")
        return False

    def _createSubset(self):
        """

        """
        print("_createSubset()")
        if self.dataSource is not None:
            # open save dialog
            subsetDBpath = self.dataSource.getSubsetFilename()
            if len(subsetDBpath) > 0:
                self.busy(True)
                self.updateStatusBar("Creating a DB subset into {}".format(subsetDBpath))
                result = self.dataSource.createSubset(subsetDBpath, self.fromDate, self.toDate)

                self.busy(False)


    def _resetClassifications(self):
        if self.dataSource is not None:
            if self.confirmMessage("Warning",
                                   """
               Pressing OK will rebuild the cache file with data 
               taken from the database, losing any manual changes 
               made through Ebrow.
               Press Cancel to abort this operation.
                                   """):
                self.busy(True)
                self._settings.save()
                self.updateStatusBar("Clearing classifications and attributes...")
                if self.dataSource.resetClassifications():
                    self.eventDataChanges = [False] * (self.fromId + self.covID)
                    self.updateStatusBar("Performing classifications...")
                    self.dataSource.classifyEvents(self.fromId, self.toId)
                    self.updateStatusBar("Calculating attributes...")
                    self.dataSource.attributeEvents(self.fromId, self.toId)
                self.busy(False)

    def _overrideClassifications(self):
        """
        updates the classifications in automatic_data dataframe
        with manual overrides, if any
        :return:
        """
        self.busy(True)
        self.updateStatusBar("Updating classifications on cache file...")
        idx = 0
        updated = 0
        total = len(self.eventDataChanges)
        self.updateProgressBar(0)
        for xchg in self.eventDataChanges:
            if xchg is True:
                self.dataSource.setEventClassification(idx, self.classifications.loc[idx, 'classification'])
            idx += 1
            updated += 1
            self.updateProgressBar(idx, total, step=5)

        self.updateStatusBar("{} classifications ready, including manual overrides".format(updated - 1))
        # self._ui.pbSave.setEnabled(False)
        # self._ui.pbClassReset.setEnabled(False)
        self.busy(False)
        return True

    def _openDataSource(self, noGUI: bool = False):
        """

        @param noGUI:
        @return:
        """

        '''
        
        try:
            self._ui.dtFrom.dateChanged.disconnect()
            self._ui.dtTo.dateChanged.disconnect()
        except TypeError:
            pass
        '''

        if self.dbFile is None:
            name = self._settings.readSettingAsString('lastDBfilePath')
        else:
            name = self.dbFile

        if name != 'None' and noGUI:
            # automatic database opening at startup
            self.dataSource = DataSource(self, self._settings)
            self.dbOk = False
            name = self._settings.readSettingAsString('lastDBfilePath')
            dbFile = self.dataSource.openFile(name)
            if dbFile is None:
                self.updateStatusBar("Failed opening stored database file {}".format(name))
            else:
                self.dbOk = True

        if not noGUI:
            # database opening required by the user via GUI
            if self.dataSource is None:
                self.dataSource = DataSource(self, self._settings)

            result = self.dataSource.openFileDialog()

            self.busy(True)
            if result == -1:
                self.updateStatusBar("Failed opening new database file, no DB opened")
                self.dbOk = False
            elif result == 0:
                # pressed cancel, doing nothihg
                self.busy(False)
                return
            else:
                self._ui.lbDBfilename.setText("loading...")
                self.dbOk = True

        if self.dbOk:
            self.getCoverage()

        if self.dbOk:
            self._ui.twMain.setTabVisible(1, True)  # Screenshots
            self._ui.twMain.setTabVisible(2, True)  # Plots
            self._ui.twMain.setTabVisible(3, True)  # Statistics
            self._ui.twMain.setTabVisible(4, True)  # Report
            self.tabPrefs.updateTabPrefs()


            if len(self.filteredIDs) > 0:
                self.currentID = self.filteredIDs[self.currentIndex]
                self.currentDailyNr = self.filteredDailies[self.currentIndex]
            self.updateStatusBar("DB: {}".format(self.dataSource.fullPath()))
            self._settings.writeSetting('lastDBfilePath', self.dataSource.fullPath())
            self.updateStatusBar("Opening ok, performing classifications...")
            self.eventDataChanges = [False] * (self.fromId + self.covID)
            self.dataSource.classifyEvents(self.fromId, self.toId)
            self.dataSource.attributeEvents(self.fromId, self.toId)
            self._ui.pbSave.setEnabled(True)
            self._ui.pbSubset.setEnabled(True)
            self._ui.pbClassReset.setEnabled(True)
            self._ui.pbLastMonth.setEnabled(False)
            self._ui.pbLastYear.setEnabled(False)

            # initialize the editable classifications list
            self.classifications = self.dataSource.getEventClassifications(self.fromId, self.toId)
            self.updateStatusBar("Classifications loaded")

        else:
            # the following tabs remain hidden until a db is opened:
            self._ui.twMain.setTabVisible(1, False)  # Screenshots
            self._ui.twMain.setTabVisible(2, False)  # Statistics
            self._ui.twMain.setTabVisible(3, False)  # RTS config

        self.busy(False)
