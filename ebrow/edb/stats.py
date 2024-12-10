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
import calendar
import os
import re
import os.path
import time
import json
import platform
from pathlib import Path
from datetime import datetime

import matplotlib.pyplot as plt
import pandas as pd
from PyQt5.QtCore import QSize
from PyQt5.QtGui import QPainter, QPixmap, QFont
from PyQt5.QtWidgets import QHBoxLayout, QScrollArea, QInputDialog, qApp
from PyQt5.QtCore import Qt

# from .mainwindow import MainWindow
from .nestedpie import NestedPie
from .heatmap import Heatmap
from .hm_rmob import HeatmapRMOB
from .bg_rmob import BargraphRMOB
from .bargraph import Bargraph
from .statplot import StatPlot
from .pandasmodel import PandasModel
from .utilities import notice, cryptDecrypt, mkExportFolder
from .logprint import print, fprint


class Stats:
    STTW_TABLES = 0
    STTW_DIAGRAMS = 1

    TAB_COUNTS_BY_DAY = 0
    TAB_COUNTS_BY_HOUR = 1
    TAB_COUNTS_BY_10M = 2
    TAB_POWERS_BY_DAY = 3
    TAB_POWERS_BY_HOUR = 4
    TAB_POWERS_BY_10M = 5
    TAB_LASTINGS_BY_DAY = 6
    TAB_LASTINGS_BY_HOUR = 7
    TAB_LASTINGS_BY_10M = 8
    TAB_SESSIONS_REGISTER = 9
    TAB_RMOB_MONTH = 10
    TAB_SPORADIC_BG_BY_HOUR = 11
    TAB_SPORADIC_BG_BY_10M = 12

    GRAPH_PLOT = 0
    GRAPH_HEATMAP = 1
    GRAPH_BARS = 2
    GRAPH_PIE = 3

    filterDesc = {'OVER': "Overdense event", 'UNDER': "Underdense event", 'FAKE RFI': "Fake event (RFI)",
                  'FAKE ESD': "Fake event (ESD)", 'FAKE CAR1': "Fake event (Carrier#1)",
                  'FAKE CAR2': "Fake event (Carrier#2)", 'FAKE SAT': "Fake event (Saturation)",
                  'FAKE LONG': "Event lasting too long",
                  "ACQ ACT": "Acquisition active"}

    def __init__(self, parent, ui, settings):
        self._bakClass = None
        self._ui = ui
        self._parent = parent
        self._settings = settings
        self._dataSource = None
        self._diagram = None
        self._plot = None
        self._spacer = None
        self._RMOBupdating = False
        self._classFilter = ''
        self._classFilterRMOB = "OVER,UNDER,ACQ ACT"
        self._dataFrame = None
        self.avgDailyDict = dict()
        self.avgHourDf = None
        self.avg10minDf = None
        self._considerBackground = False
        self._compensation = False
        self._ui.chkSubSB.setEnabled(False)
        self._ui.chkCompensation.setEnabled(False)
        self._px = plt.rcParams['figure.dpi']  # from inches to pixels
        self._szBase = None
        self._maxCoverages = list()

        self._exportDir = Path(self._parent.exportDir, "statistics")
        mkExportFolder(self._exportDir)
        self._currentColormap = self._settings.readSettingAsString('currentColormapStat')
        self._RMOBclient = self._settings.readSettingAsString('RMOBclient')
        self._ui.cbCmaps_2.setCurrentText(self._currentColormap)

        self._showValues = self._settings.readSettingAsBool('showValues')
        self._ui.chkShowValues.setChecked(self._showValues)
        self._showGrid = self._settings.readSettingAsBool('showGridStat')
        self._ui.chkGrid_2.setChecked(self._showGrid)
        self._smoothPlots = self._settings.readSettingAsBool('smoothPlots')
        self._ui.chkSmoothPlots.setChecked(self._smoothPlots)
        self._linkedSliders = self._settings.readSettingAsBool('linkedSlidersStat')
        self._ui.chkLinked_3.setChecked(self._linkedSliders)

        self._hZoom = self._settings.readSettingAsFloat('horizontalZoomStat')
        self._vZoom = self._settings.readSettingAsFloat('verticalZoomStat')

        self._changeHzoom(int(self._hZoom * 10))
        self._changeVzoom(int(self._vZoom * 10))

        self._getClassFilter()
        self._ui.lwTabs.setCurrentRow(0)
        self._ui.lwDiags.setCurrentRow(0)

        self._ui.chkOverdense_2.clicked.connect(self._setClassFilter)
        self._ui.chkUnderdense_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeRfi_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeEsd_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeCar1_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeCar2_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeSat_2.clicked.connect(self._setClassFilter)
        self._ui.chkFakeLong_2.clicked.connect(self._setClassFilter)
        self._ui.chkAcqActive_2.clicked.connect(self._setClassFilter)
        self._ui.chkAll_2.clicked.connect(self._toggleCheckAll)
        self._ui.lwTabs.currentRowChanged.connect(self._tabChanged)
        self._ui.pbRefresh_2.clicked.connect(self._updateTabGraph)
        self._ui.pbReset_3.clicked.connect(self._resetPressed)

        self._ui.twStats.currentChanged.connect(self.updateTabStats)
        self._ui.cbCmaps_2.textActivated.connect(self._cmapChanged)
        self._ui.twStats.currentChanged.connect(self.updateTabStats)
        self._ui.pbStatTabExp.clicked.connect(self._exportPressed)
        self._ui.pbRMOB.clicked.connect(self.updateAndSendRMOBfiles)
        self._ui.chkSubSB.clicked.connect(self._toggleBackground)
        self._ui.chkCompensation.clicked.connect(self._toggleCompensation)
        self._ui.chkGrid_2.clicked.connect(self._toggleGrid)
        self._ui.chkShowValues.clicked.connect(self._toggleValues)
        self._ui.chkSmoothPlots.clicked.connect(self._toggleSmooth)

        self._ui.hsHzoom_3.valueChanged.connect(self._changeHzoom)
        self._ui.hsVzoom_3.valueChanged.connect(self._changeVzoom)
        self._ui.chkLinked_3.clicked.connect(self._toggleLinkedCursors)

        self._showDiagramSettings(False)
        self._showColormapSetting(False)

    def _get2DgraphsConfig(self):
        # 2D graphs (XY and bar graphs) configuration structure
        tableRowConfig = {
            self.TAB_COUNTS_BY_DAY: {
                "title": "Daily counts",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyCountsByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate,
                             "totalColumn": True,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
                "seriesFunction": lambda df: df['Total'].squeeze(),
                "seriesArgs": {},
                "yLabel": "Filtered daily counts",
                "fullScale": -1
            },
            self.TAB_POWERS_BY_DAY: {
                "title": "Average S-N, in the covered dates, daily totals",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyPowersByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate,
                             "highestAvgColumn": True
                             },
                "seriesFunction": lambda df: df['Average'].squeeze(),
                "seriesArgs": {},
                "yLabel": "Filtered average powers by hour [dBfs]",
                "fullScale": -1
            },
            self.TAB_LASTINGS_BY_DAY: {
                "title": "Average lastings in the covered dates, daily totals",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyLastingsByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate,
                             "highestAvgColumn": True},
                "seriesFunction": lambda df: df['Average'].squeeze(),
                "seriesArgs": {},
                "yLabel": "Filtered average lastings by day [ms]",
                "fullScale": -1
            },
            self.TAB_RMOB_MONTH: {
                "title": "RMOB hourly summary",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.toDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilterRMOB,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "",
                "fullScale": lambda df: df.max().max()
            },
            self.TAB_COUNTS_BY_HOUR: {
                "title": "Hourly counts",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered hourly counts",
                "fullScale": -1
            },
            self.TAB_POWERS_BY_HOUR: {
                "title": "Average S-N by hour",
                "resolution": "hour",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered average powers by hour [dBfs]",
                "fullScale": -1
            },
            self.TAB_LASTINGS_BY_HOUR: {
                "title": "Average lastings by hour",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeLastingsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered average lastings by hour [ms]",
                "fullScale": -1
            },
            self.TAB_COUNTS_BY_10M: {
                "title": "Counts by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered counts by 10min",
                "fullScale": -1
            },

            self.TAB_POWERS_BY_10M: {
                "title": "Average S-N by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered average powers by 10 min [dBfs]",
                "fullScale": -1
            },

            self.TAB_LASTINGS_BY_10M: {
                "title": "Average lastings by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makeLastingsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered average lastings by 10min. [ms]",
                "fullScale": -1
            },

            self.TAB_SPORADIC_BG_BY_HOUR: {
                "title": "Sporadic background by hour",
                "resolution": "hour",
                "dataFunction": lambda df: self._selectSporadicDf(self.avgHourDf),
                # note: the df parameter is intentionally ignored
                "dataArgs": {},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered counts",
                "fullScale": -1
            },
            self.TAB_SPORADIC_BG_BY_10M: {
                "title": "Sporadic background by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": lambda df: self._selectSporadicDf(self.avg10minDf),
                "dataArgs": {},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered counts",
                "fullScale": -1
            },
        }
        return tableRowConfig

    def updateTabStats(self):
        if self._ui.twMain.currentIndex() == self._parent.TWMAIN_STATISTICS:
            enableSB = 0
            avgDailyStr = self._settings.readSettingAsObject('sporadicBackgroundDaily')
            if len(avgDailyStr) > 0:
                self.avgDailyDict = json.loads(avgDailyStr)
                enableSB |= 1

            avgHourStr = self._settings.readSettingAsObject('sporadicBackgroundByHour')
            if len(avgHourStr) > 0:
                self.avgHourDf = pd.DataFrame.from_dict(json.loads(avgHourStr))
                enableSB |= 2

            avg10minStr = self._settings.readSettingAsObject('sporadicBackgroundBy10min')
            if len(avg10minStr) > 0:
                self.avg10minDf = pd.DataFrame.from_dict(json.loads(avg10minStr))
                enableSB |= 4

            # hides the sporadic background data if none are defined in ebrow.ini
            itemSBhour = self._ui.lwTabs.item(self.TAB_SPORADIC_BG_BY_HOUR)
            itemSBhour.setHidden(True)
            itemSB10m = self._ui.lwTabs.item(self.TAB_SPORADIC_BG_BY_10M)
            itemSB10m.setHidden(True)

            if enableSB == 7:
                itemSBhour.setHidden(False)
                itemSB10m.setHidden(False)
                self._ui.chkSubSB.setEnabled(True)
                self._ui.chkCompensation.setEnabled(True)
                self._considerBackground = self._settings.readSettingAsBool('subtractSporadicBackground')
                self._compensation = self._settings.readSettingAsBool('compensation')
                self._ui.chkSubSB.setChecked(self._considerBackground)
                self._ui.chkCompensation.setChecked(self._compensation)

            if self._ui.twStats.currentIndex() == self.STTW_TABLES:
                self._ui.gbDiagrams_2.hide()
                # self._ui.pbRMOB.setVisible(True)
                self._showDiagramSettings(False)

            if self._ui.twStats.currentIndex() == self.STTW_DIAGRAMS:
                self._ui.gbDiagrams_2.show()
                self._showDiagramSettings(True)
            # self._updateTabGraph()

    def updateSummaryPlot(self, filters: str):
        self._parent.busy(True)
        os.chdir(self._parent.workingDir)
        self._bakClass = self._classFilter
        self._classFilter = filters
        self._vZoom = 1
        self._hZoom = 1
        self._ui.lwTabs.setCurrentRow(self.TAB_COUNTS_BY_DAY)
        self._ui.lwDiags.setCurrentRow(self.GRAPH_PLOT)
        self._ui.twStats.setCurrentIndex(self.STTW_DIAGRAMS)
        self.showDataDiagram()
        self._classFilter = self._bakClass

        # title = self._ui.lwTabs.currentItem().text()
        # title = title.lower().replace(' ', '_')
        title = 'summary_by_day'
        className = type(self._plot).__name__
        pngName = 'stat-' + title + '-' + className + '.png'
        self._plot.saveToDisk(pngName)
        self._parent.updateStatusBar("Generated  {}".format(pngName))
        self._ui.lbStatFilename.setText(pngName)
        self._parent.busy(False)
        return pngName

    def calculateSporadicBackground(self):
        """
        Examines data collected in the specified date ranges and extracts
        two dataframes containing their averages:
        one dataframe with hourly resolution and
        the second one every ten minutes. Each dataframe contains 3 rows,
        the first for underdense counts, the second for overdense and
        the third is the sum of the two
        """
        self._parent.busy(True)
        calcDone = False

        # retrieve the intervals first
        self._dataSource = self._parent.dataSource
        if self._dataSource:
            self._parent.updateProgressBar(0)
            avgHdfUnder = None
            avg10dfUnder = None
            avgHdfOver = None
            avg10dfOver = None

            # to limit precision to 4 decimals
            pd.set_option('display.float_format', lambda x: '%.0f' % x
            if (x == x and x * 10 % 10 == 0) else ('%.1f' % x if (x == x and x * 100 % 10 == 0) else '%.2f' % x))

            prog = 0
            sdList = self._settings.readSettingAsObject('sporadicDates')
            if len(sdList) > 0:
                sporadicDatesList = json.loads(sdList)
                for intervalStr in sporadicDatesList:
                    qApp.processEvents()
                    dates = intervalStr.split(" -> ")
                    # extracts the events in the given date,
                    # ignoring the intervals that are not fully included into the database coverage
                    if self._parent.fromDate <= dates[0] and self._parent.toDate >= dates[1]:
                        dfEvents = self._dataSource.getADpartialFrame(dates[0], dates[1])
                        if dfEvents is not None:
                            dfCountsHourlyUnder = self._dataSource.makeCountsDf(dfEvents, dates[0], dates[1], dtRes='h',
                                                                                filters='UNDER', totalRow=False,
                                                                                totalColumn=False)
                            dfCounts10minUnder = self._dataSource.makeCountsDf(dfEvents, dates[0], dates[1],
                                                                               dtRes='10T',
                                                                               filters='UNDER', totalRow=False,
                                                                               totalColumn=False)

                            dfCountsHourlyOver = self._dataSource.makeCountsDf(dfEvents, dates[0], dates[1], dtRes='h',
                                                                               filters='OVER', totalRow=False,
                                                                               totalColumn=False)
                            dfCounts10minOver = self._dataSource.makeCountsDf(dfEvents, dates[0], dates[1], dtRes='10T',
                                                                              filters='OVER', totalRow=False,
                                                                              totalColumn=False)

                            excludeIndexes = []
                            for rowIndex in dfCountsHourlyUnder.index:
                                qApp.processEvents()
                                row = dfCountsHourlyUnder.loc[rowIndex]
                                if -1 in row.values:
                                    # exclude this day from averaging due to missing data
                                    excludeIndexes.append(rowIndex)

                            for row in excludeIndexes:
                                dfCountsHourlyUnder.drop(row, inplace=True)
                                dfCounts10minUnder.drop(row, inplace=True)
                                dfCountsHourlyOver.drop(row, inplace=True)
                                dfCounts10minOver.drop(row, inplace=True)

                            if dfCountsHourlyUnder.shape[0] > 0:
                                # ignores empty dfCountsHourly
                                meanDf = dfCountsHourlyUnder.mean().to_frame().T
                                if avgHdfUnder is None:
                                    avgHdfUnder = meanDf
                                else:
                                    avgHdfUnder = pd.concat([avgHdfUnder, meanDf])

                            if dfCounts10minUnder.shape[0] > 0:
                                # ignores empty dfCounts10min
                                meanDf = dfCounts10minUnder.mean().to_frame().T
                                if avg10dfUnder is None:
                                    avg10dfUnder = meanDf
                                else:
                                    avg10dfUnder = pd.concat([avg10dfUnder, meanDf])

                            if dfCountsHourlyOver.shape[0] > 0:
                                # ignores empty dfCountsHourly
                                meanDf = dfCountsHourlyOver.mean().to_frame().T
                                if avgHdfOver is None:
                                    avgHdfOver = meanDf
                                else:
                                    avgHdfOver = pd.concat([avgHdfOver, meanDf])

                            if dfCounts10minOver.shape[0] > 0:
                                # ignores empty dfCounts10min
                                meanDf = dfCounts10minOver.mean().to_frame().T
                                if avg10dfOver is None:
                                    avg10dfOver = meanDf
                                else:
                                    avg10dfOver = pd.concat([avg10dfOver, meanDf])

                        prog += 1
                        self._parent.updateProgressBar(prog, len(sporadicDatesList))

            avgDailyCountUnder = 0
            avgDailyCountOver = 0

            if avgHdfUnder is not None:
                avgHourDfUnder = avgHdfUnder.mean().round(0).astype(int).to_frame().T
                avgDailyCountUnder = int(avgHourDfUnder.iloc[0].sum())
                self.avgHourDf = avgHourDfUnder
                self.avgHourDf = self.avgHourDf.rename(index={0: 'UNDER'})

            if avgHdfOver is not None:
                avgHourDfOver = avgHdfOver.mean().round(0).astype(int).to_frame().T
                avgDailyCountOver = int(avgHourDfOver.iloc[0].sum())
                self.avgHourDf = pd.concat([self.avgHourDf, avgHourDfOver])
                self.avgHourDf = self.avgHourDf.rename(index={0: 'OVER'})

            avgDailyDict = {'UNDER': avgDailyCountUnder, 'OVER': avgDailyCountOver}
            avgDailyStr = json.dumps(avgDailyDict)
            self._settings.writeSetting('sporadicBackgroundDaily', avgDailyStr)
            calcDone = True

            if self.avgHourDf is not None:
                if self.avgHourDf.shape[0] > 0:
                    self.avgHourDf.loc['Total'] = self.avgHourDf.sum(numeric_only=True, axis=0)
                    avgHourStr = json.dumps(self.avgHourDf.to_dict())
                    self._settings.writeSetting('sporadicBackgroundByHour', avgHourStr)

            if avg10dfUnder is not None:
                self.avg10minDf = avg10dfUnder.mean().round(0).astype(int).to_frame().T
                self.avg10minDf = self.avg10minDf.rename(index={0: 'UNDER'})

            if avg10dfOver is not None:
                avg10minDfOver = avg10dfOver.mean().round(0).astype(int).to_frame().T
                self.avg10minDf = pd.concat([self.avg10minDf, avg10minDfOver])
                self.avg10minDf = self.avg10minDf.rename(index={0: 'OVER'})

            if self.avg10minDf is not None:
                if self.avg10minDf.shape[0] > 0:
                    self.avg10minDf.loc['Total'] = self.avg10minDf.sum(numeric_only=True, axis=0)
                    avg10minStr = json.dumps(self.avg10minDf.to_dict())
                    self._settings.writeSetting('sporadicBackgroundBy10min', avg10minStr)

        self._parent.busy(False)
        return calcDone

    def updateAndSendRMOBfiles(self):
        self.updateRMOBfiles(sendOk=True)

    def updateRMOBfiles(self, sendOk: bool = True):
        self._parent.busy(True)
        os.chdir(self._parent.workingDir)
        self._RMOBupdating = True
        self._dataSource = self._parent.dataSource
        self._ui.twStats.setCurrentIndex(self.STTW_TABLES)

        (qDateFrom, qDateTo) = self._parent.dataSource.QDateCoverage()
        (y, m, d) = qDateTo.getDate()
        qDateFrom.setDate(y, m, 1)
        fromDate = qDateFrom.toString("yyyy-MM-dd")
        toDate = self._parent.toDate

        df = self._dataSource.getADpartialFrame(fromDate, toDate)

        df2 = self._dataSource.makeCountsDf(df, fromDate, toDate, dtRes='h',
                                            filters=self._classFilterRMOB, totalRow=False, totalColumn=False,
                                            placeholder=-1)

        dfRMOB, monthNr, year = self._dataSource.makeRMOB(df2, lastOnly=True)
        filePrefix, txtFileName, dfMonth = self._generateRMOBtableFile(dfRMOB, year, monthNr)

        # the files prefix is stored for reporting purposes
        self._settings.writeSetting('RMOBfilePrefix', filePrefix)

        self._ui.lwTabs.setCurrentRow(self.TAB_RMOB_MONTH)
        self._ui.lwDiags.setCurrentRow(self.GRAPH_HEATMAP)
        self._ui.twStats.setCurrentIndex(self.STTW_DIAGRAMS)
        self.showDataDiagram()
        heatmapFileName = self._generateRMOBgraphFile()

        self._ui.lwDiags.setCurrentRow(self.GRAPH_BARS)
        self.showDataDiagram()
        bargraphFileName = self._generateRMOBgraphFile()

        # the heatmap, having aspect ratio about squared
        # does not need to cut away the padding
        hmPix = QPixmap(heatmapFileName).scaled(300, 220, transformMode=Qt.SmoothTransformation)
        hmPix.save("heatmap.jpg")

        # the bargraph instead is half-tall and the padding above
        # and below the graph must be cut away, so the height
        # is scaled taller than needed, to be cut away while drawing
        barPix = QPixmap(bargraphFileName).scaled(260, 110,
                                                  transformMode=Qt.SmoothTransformation)
        barPix.save("bargraph.jpg")

        logoPix = QPixmap(":/echoes_transparent").scaled(48, 48, transformMode=Qt.SmoothTransformation)
        # logoPix = QPixmap(":/rts").scaled(48, 48, transformMode=Qt.SmoothTransformation)
        family = self._settings.readSettingAsString('fontFamily')
        nf = QFont(family, 9, -1, False)
        bf = QFont(family, 9, 200, True)
        pixRMOB = QPixmap(700, 220)
        pixRMOB.fill(Qt.white)
        p = QPainter()
        if p.begin(pixRMOB):
            p.drawPixmap(400, 0, hmPix)
            p.drawPixmap(100, 110, barPix)
            p.setPen(Qt.black)

            p.setFont(bf)

            x = 5
            p.drawText(x, 15, "Observer: ")
            p.drawText(x, 30, "Country: ")
            p.drawText(x, 45, "City: ")
            p.drawText(x, 60, "Antenna: ")
            p.drawText(x, 75, "RF preamp: ")
            p.drawText(x, 90, "Obs.Method: ")
            p.drawText(x, 105, "Computer: ")

            x = 200
            p.drawText(x, 15, "Location: ")
            p.drawText(x, 30, "          ")
            p.drawText(x, 45, "Frequency: ")
            p.drawText(x, 60, "Az.:")
            p.drawText(x + 70, 60, "El.:")
            p.drawText(x, 75, "Receiver: ")

            cfgRev = self._dataSource.getCfgRevisions()[-1]
            echoesVer = self._dataSource.getEchoesVersion(cfgRev)
            obsMethod = "Echoes {} + Data Browser v.{}".format(echoesVer, self._parent.version)

            p.setFont(nf)
            x = 90
            p.drawText(x, 15, "{}".format(self._settings.readSettingAsString('owner')[:16]))
            p.drawText(x, 30, "{}".format(self._settings.readSettingAsString('country')[:16]))
            p.drawText(x, 45, "{}".format(self._settings.readSettingAsString('city')[:20]))
            p.drawText(x, 60, "{}".format(self._settings.readSettingAsString('antenna')[:16]))
            p.drawText(x, 75, "{}".format(self._settings.readSettingAsString('preamplifier')[:15]))
            p.drawText(x, 90, "{}".format(obsMethod))
            p.drawText(x, 105, "{}".format(self._settings.readSettingAsString('computer')[:50]))

            x = 275
            p.drawText(x, 15, "{}".format(self._settings.readSettingAsString('longitude')[:12]))
            p.drawText(x, 30, "{}".format(self._settings.readSettingAsString('latitude')[:12]))
            p.drawText(x, 45, "{} Hz".format(self._settings.readSettingAsString('frequencies')[:14]))
            p.drawText(x - 40, 60, "{}°".format(self._settings.readSettingAsString('antAzimuth')))
            p.drawText(x + 20, 60, "{}°".format(self._settings.readSettingAsString('antElevation')))
            p.drawText(x, 75, "{}".format(self._settings.readSettingAsString('receiver')[:18]))

            p.setFont(bf)
            ts = time.strptime(self._parent.toDate, "%Y-%m-%d")
            # headDate = time.strftime("%B %d, %Y", ts) month names shouldn't be localized
            months = ['', 'January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September',
                      'October', 'November', 'December']
            mon = months[ts.tm_mon]
            headDate = "{} {}, {}".format(mon, ts.tm_mday, ts.tm_year)
            p.drawText(10, 130, headDate)
            p.drawText(10, 145, "Hourly counts:")
            p.drawPixmap(10, 160, logoPix)
            p.end()

            # os.chdir(self._exportDir)
            os.chdir(self._parent.workingDir)
            jpgFileName = filePrefix + ".jpg"
            if pixRMOB.save(jpgFileName):
                print("{} generated".format(jpgFileName))
                if self._RMOBclient != 'UNKNOWN' and len(self._RMOBclient) > 0:
                    goSend = False
                    if sendOk:
                        if self._parent.isBatchRMOB:
                            goSend = True
                        elif self._parent.confirmMessage("RMOB data generation", "Send these data to RMOB ftp server?"):
                            goSend = True

                        if goSend and sendOk:
                            if platform.system() == 'Windows':
                                # the curse of the 'c:\program files' folder
                                cmd = "\"{}\" {} {}".format(self._RMOBclient, txtFileName, jpgFileName)
                            else:
                                cmd = "{} {} {}".format(self._RMOBclient, txtFileName, jpgFileName)
                            print("Sending files to RMOB.org: ", cmd)
                            ret = os.system(cmd)
                            if ret == 0:
                                self._parent.updateStatusBar("Sending successful")
                            else:
                                self._parent.infoMessage("Failed sending files to RMOB.org",
                                                         "command={}, returned={}".format(cmd, ret))
                    else:
                        self._parent.updateStatusBar("Files NOT SENT to RMOB.org")

        self._RMOBupdating = False
        self._parent.busy(False)
        return dfMonth, heatmapFileName, bargraphFileName

    def showDataTable(self):
        self._parent.busy(True)
        self._ui.gbClassFilter_2.show()
        self._ui.gbDiagrams_2.hide()
        self._dataSource = self._parent.dataSource
        row = self._ui.lwTabs.currentRow()
        self._ui.tvTabs.setEnabled(False)
        # self._ui.pbRMOB.setEnabled(False)
        if self._classFilter == '':
            # nothing to show
            self._parent.infoMessage('Statistic diagrams:',
                                     'No class filters set, nothing to show')
            self._parent.busy(False)
            return

        df = self._dataSource.getADpartialFrame(self._parent.fromDate, self._parent.toDate)

        if row == self.TAB_COUNTS_BY_DAY:
            self._dataFrame = self._dataSource.dailyCountsByClassification(df, self._classFilter, self._parent.fromDate,
                                                                           self._parent.toDate, totalRow=True,
                                                                           totalColumn=True,
                                                                           compensate=self._compensation,
                                                                           considerBackground=self._considerBackground)

        if row == self.TAB_COUNTS_BY_HOUR:
            self._dataFrame = self._dataSource.makeCountsDf(df, self._parent.fromDate, self._parent.toDate, dtRes='h',
                                                            filters=self._classFilter,
                                                            compensate=self._compensation,
                                                            considerBackground=self._considerBackground)

        if row == self.TAB_COUNTS_BY_10M:
            self._dataFrame = self._dataSource.makeCountsDf(df, self._parent.fromDate, self._parent.toDate, dtRes='10T',
                                                            filters=self._classFilter,
                                                            compensate=self._compensation,
                                                            considerBackground=self._considerBackground)
            self._dataFrame = self._dataSource.splitAndStackDataframe(self._dataFrame, maxColumns=24)

        if row == self.TAB_POWERS_BY_DAY:
            self._dataFrame = self._dataSource.dailyPowersByClassification(df, self._classFilter, self._parent.fromDate,
                                                                           self._parent.toDate,
                                                                           highestAvgRow=True, highestAvgColumn=True)

        if row == self.TAB_POWERS_BY_HOUR:
            self._dataFrame = self._dataSource.makePowersDf(df, self._parent.fromDate, self._parent.toDate, dtRes='h',
                                                            filters=self._classFilter,
                                                            highestAvgRow=True, highestAvgColumn=True)

        if row == self.TAB_POWERS_BY_10M:
            self._dataFrame = self._dataSource.makePowersDf(df, self._parent.fromDate, self._parent.toDate, dtRes='10T',
                                                            filters=self._classFilter,
                                                            highestAvgRow=True, highestAvgColumn=True)
            self._dataFrame = self._dataSource.splitAndStackDataframe(self._dataFrame, maxColumns=24)

        if row == self.TAB_LASTINGS_BY_DAY:
            self._dataFrame = self._dataSource.dailyLastingsByClassification(df, self._classFilter,
                                                                             self._parent.fromDate,
                                                                             self._parent.toDate, highestAvgRow=True,
                                                                             highestAvgColumn=True)

        if row == self.TAB_LASTINGS_BY_HOUR:
            self._dataFrame = self._dataSource.makeLastingsDf(df, self._parent.fromDate, self._parent.toDate, dtRes='h',
                                                              filters=self._classFilter, highestAvgRow=True,
                                                              highestAvgColumn=True)

        if row == self.TAB_LASTINGS_BY_10M:
            self._dataFrame = self._dataSource.makeLastingsDf(df, self._parent.fromDate, self._parent.toDate,
                                                              dtRes='10T',
                                                              filters=self._classFilter,
                                                              highestAvgRow=True, highestAvgColumn=True)
            self._dataFrame = self._dataSource.splitAndStackDataframe(self._dataFrame, maxColumns=24)

        if row == self.TAB_SESSIONS_REGISTER:
            # filters not applicable here
            self._ui.gbClassFilter_2.hide()
            self._dataFrame = self._dataSource.getASpartialFrame(self._parent.fromDate, self._parent.toDate)

        if row == self.TAB_RMOB_MONTH:
            df2 = self._dataSource.makeCountsDf(df, self._parent.fromDate, self._parent.toDate, dtRes='h',
                                                filters=self._classFilterRMOB, totalRow=False,
                                                totalColumn=False,
                                                compensate=self._compensation,
                                                considerBackground=self._considerBackground)
            self._dataFrame, monthName, year = self._dataSource.makeRMOB(df2)

        if row == self.TAB_SPORADIC_BG_BY_HOUR:
            self._dataFrame = self.avgHourDf

        if row == self.TAB_SPORADIC_BG_BY_10M:
            self._dataFrame = self.avg10minDf

        if row == self.TAB_SPORADIC_BG_BY_HOUR or row == self.TAB_SPORADIC_BG_BY_10M:
            if 'UNDER' in self._classFilter and 'OVER' in self._classFilter:
                pass
            elif 'UNDER' in self._classFilter:
                self._dataFrame = df.filter(items=['UNDER'], axis=0)
            elif 'OVER' in self._classFilter:
                self._dataFrame = df.filter(items=['OVER'], axis=0)

        self._ui.tvTabs.setEnabled(True)
        model = PandasModel(self._dataFrame)
        self._ui.tvTabs.setModel(model)
        self._parent.busy(False)

    def showDataDiagram(self):
        dataFrame = None
        self._parent.busy(True)
        self._ui.gbDiagrams_2.show()
        self._dataSource = self._parent.dataSource
        self._showColormapSetting(False)
        tableRow = self._ui.lwTabs.currentRow()
        graphRow = self._ui.lwDiags.currentRow()

        # maximum length of time axis in days for each kind of graph
        self._maxCoverages = [
            # [self.GRAPH_PLOT]
            [
                366,  # daily counts by day
                15,  # daily counts by hour
                7,  # daily counts by 10min
                366,  # daily powers by day
                15,  # daily powers by hour
                7,  # daily powers by 10min
                366,  # daily lastings by day
                15,  # daily lastings by hour
                7,  # daily lastings by 10min
                0,  # session table, no graphics
                1,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # undefined

            ],

            # [self.GRAPH_HEATMAP]
            [
                366,  # daily counts by day
                90,  # daily counts by hour
                31,  # daily counts by 10min
                366,  # daily powers by day
                90,  # daily powers by hour
                31,  # daily powers by 10min
                366,  # daily lastings by day
                15,  # daily lastings by hour
                7,  # daily lastings by 10min
                0,  # session table, no graphics
                31,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # undefined

            ],

            # [GRAPH_BARS]
            [
                366,  # daily counts by day
                15,  # daily counts by hour
                7,  # daily counts by 10min
                366,  # daily powers by day
                15,  # daily powers by hour
                7,  # daily powers by 10min
                366,  # daily lastings by day
                15,  # daily lastings by hour
                7,  # daily lastings by 10min
                0,  # session table, no graphics
                1,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # undefined
            ],

            # [GRAPH_PIE]
            [
                366,  # daily counts by classification, covers up to 1 year
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
                0,  # undefined
            ]
        ]

        cov = self._maxCoverages[graphRow][tableRow]

        if cov == 0:
            # nothing to show
            self._parent.infoMessage('Statistic diagrams;',
                                     'Combination table/graph not implemented')
            self._parent.busy(False)
            return

        if self._classFilter == '':
            # nothing to show
            self._parent.infoMessage('Statistic diagrams:',
                                     'No class filters set, nothing to show')
            self._parent.busy(False)
            return

        self._parent.getCoverage()

        if self._parent.coverage > cov and tableRow != self.TAB_RMOB_MONTH and tableRow != self.TAB_SPORADIC_BG_BY_10M \
                and tableRow != self.TAB_SPORADIC_BG_BY_HOUR:
            self._parent.busy(False)
            notice("Notice", "This graph cannot display {} days, please reduce the day coverage and "
                             "retry".format(self._parent.coverage))
            return

        # removing previously shown graph
        layout = self._ui.wContainer.layout()
        series = None
        resolution = None
        df = None
        res = None
        title = None
        inchWidth = 0
        inchHeight = 0
        yLabel = "placeholder"
        fullScale = 10000  # init value
        if layout is None:
            layout = QHBoxLayout()
        else:
            layout.removeWidget(self._diagram)
            layout.removeItem(self._spacer)

        qApp.processEvents()
        scroller = QScrollArea()
        self._diagram = scroller

        if self._parent.isReporting:
            self._ui.twMain.setCurrentIndex(self._parent.TWMAIN_STATISTICS)
            self._ui.twShots.setCurrentIndex(self.STTW_DIAGRAMS)

        if self._szBase is None:
            # must be recalculated only once
            # the diagram widget must be visible
            self._szBase = self._ui.twStats.currentWidget().size()  # self._diagram.size()
            print("graph base size = ", self._szBase)

        # try because things can go bad when the month / year changes:
        try:
            if graphRow == self.GRAPH_PLOT:
                self._XYplot(tableRow, layout)

            elif graphRow == self.GRAPH_HEATMAP:
                self._heatmap(tableRow, layout)

            elif graphRow == self.GRAPH_BARS:
                self._bargraph(tableRow, layout)

            elif graphRow == self.GRAPH_PIE:
                self._pieGraph(tableRow, layout)

        except AttributeError as e:
            # empty graph in case of problems
            print("Exception: ", e)
            layout.addWidget(scroller)

        self._changeHzoom(int(self._hZoom * 10), manual=False)
        self._changeVzoom(int(self._vZoom * 10), manual=False)

        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self._ui.wContainer.setLayout(layout)

        self._parent.busy(False)

    def _getClassFilter(self):
        self._parent.busy(True)
        self._classFilter = self._settings.readSettingAsString('classFilterStat')
        idx = 0
        for tag in self._parent.filterTags:
            isCheckTrue = tag in self._classFilter
            self._parent.filterCheckStats[idx].setChecked(isCheckTrue)
            idx += 1
        self._parent.busy(False)

    def _setClassFilter(self):
        self._parent.busy(True)
        classFilter = ''
        idx = 0
        for check in self._parent.filterCheckStats:
            if check.isChecked():
                classFilter += self._parent.filterTags[idx] + ','
            idx += 1

        if classFilter != '':
            classFilter = classFilter[0:-1]  # discards latest comma+space

        self._classFilter = classFilter
        self._settings.writeSetting('classFilterStat', self._classFilter)
        self._parent.updateStatusBar("Filtering statistic data by classification: {}".format(self._classFilter))
        self._parent.busy(False)

    def _updateTabGraph(self):
        if self._ui.twStats.currentIndex() == self.STTW_TABLES:
            self.showDataTable()
        if self._ui.twStats.currentIndex() == self.STTW_DIAGRAMS:
            self.showDataDiagram()
        self._ui.lbStatFilename.setText("undefined")

    def _toggleCheckAll(self):
        self._ui.chkOverdense_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkUnderdense_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeRfi_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeEsd_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeCar1_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeCar2_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeSat_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkFakeLong_2.setChecked(self._ui.chkAll_2.isChecked())
        self._ui.chkAcqActive_2.setChecked(self._ui.chkAll_2.isChecked())
        self._setClassFilter()

    def _cmapChanged(self, newCmapName):
        self._currentColormap = newCmapName
        print("selected colormap: ", newCmapName)

    def _showDiagramSettings(self, show: bool):
        self._ui.gbSettings_2.setVisible(show)

    def _showColormapSetting(self, show: bool, overrideCmap: str = None):
        self._ui.lbCmap_2.setVisible(show)
        self._ui.cbCmaps_2.setVisible(show)
        self._ui.cbCmaps_2.setEnabled(True)
        if overrideCmap is not None:
            self._ui.cbCmaps_2.setCurrentText(overrideCmap)
        else:
            self._ui.cbCmaps_2.setCurrentText(self._currentColormap)

    def _resetPressed(self, checked):
        self._linkedSliders = False
        self._smoothPlots = False
        self._showGrid = False
        self._showValues = False

        self._settings.writeSetting('linkedSlidersStat', self._linkedSliders)
        self._settings.writeSetting('smoothPlots', self._smoothPlots)
        self._settings.writeSetting('showGrid', self._showGrid)
        self._settings.writeSetting('showValues', self._showValues)
        self._settings.writeSetting('horizontalZoomStat', self._hZoom)
        self._settings.writeSetting('verticalZoomStat', self._vZoom)

        self._ui.chkLinked_3.setChecked(self._linkedSliders)
        self._ui.chkSmoothPlots.setChecked(self._smoothPlots)
        self._ui.chkGrid_2.setChecked(self._showGrid)
        self._ui.chkShowValues.setChecked(self._showValues)

        self._hZoom = self._settings.ZOOM_DEFAULT
        self._vZoom = self._settings.ZOOM_DEFAULT
        # self._ui.hsHzoom.setValue(int(self._settings.ZOOM_DEFAULT * 10))
        # self._ui.hsVzoom.setValue(int(self._settings.ZOOM_DEFAULT * 10))
        self._changeHzoom(int(self._hZoom * 10))
        self._changeVzoom(int(self._vZoom * 10))
        self._updateTabGraph()

    def _exportPressed(self, checked):
        self._parent.checkExportDir(self._exportDir)
        pngName = None
        # progressive number to make the exported files unique
        now = datetime.now()
        midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
        prog = "-{}".format((now - midnight).seconds)
        if os.path.exists(self._exportDir):
            os.chdir(self._exportDir)
            if self._ui.twStats.currentIndex() == self.STTW_TABLES:
                # the displayed table is exported as csv
                title = self._ui.lwTabs.currentItem().text() + prog
                self._dataFrame.style.set_caption(title)
                row = self._ui.lwTabs.currentRow()
                if row == self.TAB_SESSIONS_REGISTER:
                    defaultComment = "{}\nfrom {} to {},\n{} sessions in total\n\n".format(
                        title, self._parent.fromDate, self._parent.toDate, self._parent.covID)

                else:
                    filters = ''
                    fList = self._classFilter.split(',')
                    for f in fList:
                        filters += " -> {}\n".format(self.filterDesc[f], '\n')
                    defaultComment = "{}\nfrom {} to {},\n{} events in total\n\nactive filters:\n{}".format(
                        title, self._parent.fromDate, self._parent.toDate, self._parent.covID, filters)

                self._parent.busy(False)
                comment = QInputDialog.getMultiLineText(self._parent, "Export statistic table",
                                                        "Comment\n(please enter further considerations, if needed):",
                                                        defaultComment)
                self._parent.busy(True)
                title = title.lower().replace(' ', '_')
                if 'sessions' in title:
                    # resolution not applicable on sessions table
                    csvName = 'stat-' + title + '-NA-Table.csv'
                else:
                    csvName = 'stat-' + title + '-Table.csv'

                self._dataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())
                self._parent.updateStatusBar("Exported  {}".format(csvName))
                self._ui.lbStatFilename.setText(csvName)
                if len(comment[0]) > 0:
                    if 'sessions' in title:
                        # resolution not applicable on sessions table
                        commentsName = 'comments-' + title + '-NA-Table.txt'
                    else:
                        commentsName = 'comments-' + title + '-Table.txt'

                    with open(commentsName, 'w') as txt:
                        txt.write(comment[0])
                        txt.close()
                        self._parent.updateStatusBar("Exported  {}".format(commentsName))
                        self._ui.lbCommentsFilename.setText(commentsName)

            if self._ui.twStats.currentIndex() == self.STTW_DIAGRAMS:
                title = self._ui.lwTabs.currentItem().text() + prog

                filters = ''
                fList = self._classFilter.split(',')
                for f in fList:
                    filters += " -> {}\n".format(self.filterDesc[f], '\n')
                defaultComment = "{}\nfrom {} to {},\n{} events in total\n\nactive filters:\n{}".format(
                    title, self._parent.fromDate, self._parent.toDate, self._parent.covID, filters)

                self._parent.busy(False)
                comment = QInputDialog.getMultiLineText(self._parent, "Export statistic diagram",
                                                        "Comment\n(please enter further considerations, if needed):",
                                                        defaultComment)
                self._parent.busy(True)
                title = title.lower().replace(' ', '_')
                className = type(self._plot).__name__
                pngName = 'stat-' + title + '-' + className + '.png'
                self._plot.saveToDisk(pngName)
                self._parent.updateStatusBar("Exported  {}".format(pngName))
                self._ui.lbStatFilename.setText(pngName)
                if len(comment[0]) > 0:
                    commentsName = 'comments-' + title + '-' + className + '.txt'
                    commentsName = commentsName.replace(' ', '_')
                    with open(commentsName, 'w') as txt:
                        txt.write(comment[0])
                        txt.close()
                        self._parent.updateStatusBar("Exported  {}".format(commentsName))
                        self._ui.lbCommentsFilename.setText(commentsName)

            os.chdir(self._parent.workingDir)
        self._parent.busy(False)
        return pngName

    def _generateRMOBgraphFile(self):
        pngName = None
        self._parent.busy(True)
        os.chdir(self._parent.workingDir)
        if self._ui.twStats.currentIndex() == self.STTW_TABLES:
            # the displayed table is exported as csv
            title = self._ui.lwTabs.currentItem().text()
            self._dataFrame.style.set_caption(title)
            title = title.lower().replace(' ', '_')
            csvName = 'stat-' + title + '.csv'
            self._dataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())
            self._parent.updateStatusBar("Generated  {}".format(csvName))
            self._ui.lbStatFilename.setText(csvName)

        if self._ui.twStats.currentIndex() == self.STTW_DIAGRAMS:
            title = self._ui.lwTabs.currentItem().text()
            title = title.lower().replace(' ', '_')
            className = type(self._plot).__name__
            pngName = 'stat-' + title + '-' + className + '.png'
            self._plot.saveToDisk(pngName)
            self._parent.updateStatusBar("Generated  {}".format(pngName))
            self._ui.lbStatFilename.setText(pngName)

        qApp.processEvents()
        self._parent.busy(False)
        return pngName

    def _generateRMOBtableFile(self, dfRMOB: pd.DataFrame, year: int, monthNr: int):
        self._parent.busy(True)
        os.chdir(self._parent.workingDir)
        # RMOB data format is similar to hourly counts table, without the rightmost column (row totals)
        # and bottom row (column totals)
        owner = self._settings.readSettingAsString('owner').split(' ')
        filePrefix = "{}_{:02d}{}".format(owner[0], monthNr, year)
        filename = filePrefix + "rmob.TXT"
        monthName = calendar.month_abbr[monthNr].lower()
        dfRMOB.index.name = monthName
        dfRMOB.replace(-1, '??? ', inplace=True)
        dfRMOB = dfRMOB.astype(str)
        dfRMOB = dfRMOB.applymap(lambda x: x.ljust(4))
        dfRMOB.to_csv(filename, sep='|', lineterminator='|\r\n')
        self._parent.updateStatusBar("Exported  {}".format(filename))
        self._ui.lbStatFilename.setText(filename)

        # after counts, the station informations are appended to each file

        # converts the sexagesimal coordinates format (DEG° MIN' SEC'') to Cologramme format
        # (3 digits zero filled DEG°, MINSEC as 2+2 digits, i.e. 51° 28' 38'' ==> 051°2838 N)
        latDeg = self._settings.readSettingAsString('latitudeDeg')
        elems = [int(x) for x in re.split('°|\'|"', latDeg) if x != '']
        latDeg = "{:03}°{:02}{:02}".format(elems[0], elems[1], elems[2])
        latDeg += ' N' if elems[0] >= 0 else ' S'
        longDeg = self._settings.readSettingAsString('longitudeDeg')
        elems = [int(x) for x in re.split('°|\'|"', longDeg) if x != '']
        longDeg = "{:03}°{:02}{:02}".format(elems[0], elems[1], elems[2])
        longDeg += ' E' if elems[0] >= 0 else ' W'
        with open(filename, 'a') as f:
            fprint("[Observer]{}".format(self._settings.readSettingAsString('owner')), file=f)
            fprint("[Country]{}".format(self._settings.readSettingAsString('country')), file=f)
            fprint("[City]{}".format(self._settings.readSettingAsString('city')), file=f)
            fprint("[Longitude]{}".format(longDeg), file=f)
            fprint("[Latitude ]{}".format(latDeg), file=f)
            fprint("[Longitude GMAP]{}".format(self._settings.readSettingAsString('longitude')), file=f)
            fprint("[Latitude GMAP]{}".format(self._settings.readSettingAsString('latitude')), file=f)
            fprint("[Frequencies]{}".format(self._settings.readSettingAsString('frequencies')), file=f)
            fprint("[Antenna]{}".format(self._settings.readSettingAsString('antenna')), file=f)
            fprint("[Azimut Antenna]{}".format(self._settings.readSettingAsString('antAzimuth')), file=f)
            fprint("[Elevation Antenna]{}".format(self._settings.readSettingAsString('antElevation')), file=f)
            fprint("[Pre-Amplifier]{}".format(self._settings.readSettingAsString('preamplifier')), file=f)
            fprint("[Receiver]{}".format(self._settings.readSettingAsString('receiver')), file=f)
            fprint("[Observing Method]{}".format("Echoes 0.51++ Data Browser v.{}").format(self._parent.version),
                   file=f)
            fprint("[Computer Type]{}".format(self._settings.readSettingAsString('computer')), file=f)
            fprint("[Remarks]{}".format(self._settings.readSettingAsString('notes')), file=f)
            fprint("[Soft FTP]Echoes FTP client v.{}".format(self._parent.version), file=f)
            fprint("[E]{}".format(cryptDecrypt(self._settings.readSettingAsString('email'), 2503)), file=f)
        self._parent.busy(False)
        return filePrefix, filename, dfRMOB

    def _toggleGrid(self, state):
        self._showGrid = (state != 0)
        self._settings.writeSetting('showGridStat', self._showGrid)

    def _toggleValues(self, state):
        self._showValues = (state != 0)
        self._settings.writeSetting('showValues', self._showValues)

    def _toggleSmooth(self, state):
        self._smoothPlots = (state != 0)
        self._settings.writeSetting('smoothPlots', self._smoothPlots)

    def _toggleLinkedCursors(self, state):
        self._linkedSliders = (state != 0)
        self._settings.writeSetting('linkedCursorsStat', self._linkedSliders)

    def _toggleBackground(self, state):
        self._considerBackground = (state != 0)
        self._settings.writeSetting('subtractSporadicBackground', self._considerBackground)

    def _toggleCompensation(self, state):
        self._compensation = (state != 0)
        self._settings.writeSetting('compensation', self._compensation)

    def _changeHzoom(self, newValue, manual=True):
        pixWidth = 0
        pixHeight = 0
        self._ui.hsVzoom_3.blockSignals(True)
        newValue /= 10
        delta = (newValue - self._hZoom)
        if self._linkedSliders:
            if self._settings.ZOOM_MIN <= (self._vZoom + delta) <= self._settings.ZOOM_MAX:
                self._vZoom += delta
            elif (self._vZoom + delta) < self._settings.ZOOM_MIN:
                self._vZoom = self._settings.ZOOM_MIN
            elif (self._vZoom + delta) > self._settings.ZOOM_MAX:
                self._vZoom = self._settings.ZOOM_MAX
            self._ui.hsVzoom_3.setValue(int(self._vZoom * 10))

        self._hZoom = newValue
        self._settings.writeSetting('horizontalZoom', self._hZoom)
        self._ui.hsVzoom_3.setValue(int(self._vZoom * 10))
        self._ui.lbHzoom_3.setText("{} X".format(self._hZoom))
        self._ui.lbVzoom_3.setText("{} X".format(self._vZoom))
        if self._plot is not None:
            inchWidth, inchHeight, pixWidth, pixHeight = self._calcFigSizeInch()
            print("pixWidth={}, pixHeight={}, inchWidth={}, inchHeight={}".format(pixWidth, pixHeight,
                                                                                  inchWidth, inchHeight))
            canvas = self._plot.widget()
            canvas.resize(pixWidth, pixHeight)
            self._plot.zoom(inchWidth, inchHeight)

        elif manual:
            self.updateTabStats()

        if self._diagram is not None:
            self._diagram.ensureVisible(int(pixWidth / 2), int(pixHeight / 2))
        self._ui.hsVzoom_3.blockSignals(False)

    def _changeVzoom(self, newValue, manual=True):
        pixWidth = 0
        pixHeight = 0
        self._ui.hsHzoom_3.blockSignals(True)
        newValue /= 10
        delta = (newValue - self._vZoom)
        if self._linkedSliders:
            if self._settings.ZOOM_MIN <= (self._hZoom + delta) <= self._settings.ZOOM_MAX:
                self._hZoom += delta
            elif (self._hZoom + delta) < self._settings.ZOOM_MIN:
                self._hZoom = self._settings.ZOOM_MIN
            elif (self._hZoom + delta) > self._settings.ZOOM_MAX:
                self._hZoom = self._settings.ZOOM_MAX
            self._ui.hsHzoom_3.setValue(int(self._hZoom * 10))

        self._vZoom = newValue
        self._settings.writeSetting('verticalZoom', self._vZoom)
        self._ui.hsHzoom_3.setValue(int(self._hZoom * 10))
        self._ui.lbHzoom_3.setText("{} X".format(self._hZoom))
        self._ui.lbVzoom_3.setText("{} X".format(self._vZoom))
        if self._plot is not None:
            inchWidth, inchHeight, pixWidth, pixHeight = self._calcFigSizeInch()
            print("pixWidth={}, pixHeight={}, inchWidth={}, inchHeight={}".format(pixWidth, pixHeight,
                                                                                  inchWidth, inchHeight))
            canvas = self._plot.widget()
            canvas.resize(pixWidth, pixHeight)
            self._plot.zoom(inchWidth, inchHeight)

        elif manual:
            self.updateTabStats()

        if self._diagram is not None:
            self._diagram.ensureVisible(int(pixWidth / 2), int(pixHeight / 2))
        self._ui.hsHzoom_3.blockSignals(False)

    def _calcFigSizeInch(self):
        """
        recalc the container (scrollarea) containing the figure
        taking the zoom h/v in count.

        Note: the container contains a canvas (Qt5 interface to matplotlib)
        which contains a figure. Everything must be resized according to
        zoom sliders
        """

        pixWidth = self._szBase.width() * self._hZoom
        pixHeight = self._szBase.height() * self._vZoom
        if pixWidth > 65535:
            pixWidth = 65535
        if pixHeight > 65535:
            pixHeight = 65535

        # turns the container size to inches to be
        # used by the caller to resize the figure
        inchWidth = pixWidth / self._px
        inchHeight = pixHeight / self._px
        return inchWidth, inchHeight, int(pixWidth), int(pixHeight)

    def _tabChanged(self, row):

        self._ui.chkSubSB.setEnabled(row != self.TAB_RMOB_MONTH)

        if row == self.TAB_COUNTS_BY_DAY or row == self.TAB_COUNTS_BY_HOUR or self.TAB_COUNTS_BY_10M:
            self._ui.gbDataSettings.setVisible(True)
            self._ui.gbClassFilter_2.setVisible(True)
            self._ui.gbClassFilter_2.setEnabled(True)

        if row == self.TAB_SESSIONS_REGISTER or row == self.TAB_RMOB_MONTH:
            # RMOB data use an hardcoded filters, including only
            # non-fake events
            self._ui.gbDataSettings.setVisible(False)
            self._ui.gbClassFilter_2.setVisible(False)
            self._ui.gbClassFilter_2.setEnabled(False)

        if row == self.TAB_SPORADIC_BG_BY_HOUR or row == self.TAB_SPORADIC_BG_BY_10M:
            self._ui.gbDataSettings.setVisible(False)
            self._ui.gbClassFilter_2.setVisible(True)
            self._ui.gbClassFilter_2.setEnabled(True)

        if (row == self.TAB_POWERS_BY_DAY or row == self.TAB_POWERS_BY_HOUR or row == self.TAB_POWERS_BY_10M or
                row == self.TAB_LASTINGS_BY_DAY or row == self.TAB_LASTINGS_BY_HOUR or row == self.TAB_LASTINGS_BY_10M):
            self._ui.gbDataSettings.setVisible(False)
            self._ui.gbClassFilter_2.setVisible(True)
            self._ui.gbClassFilter_2.setEnabled(True)

        # self._updateTabGraph()

    def _selectSporadicDf(self, df):
        df3 = None
        if 'UNDER' in self._classFilter and 'OVER' in self._classFilter:
            df2 = df.loc['Total'].to_frame().T
            df3 = df2.rename(index={'Total': self._parent.toDate})
        elif 'UNDER' in self._classFilter:
            df2 = df.loc['UNDER'].to_frame().T
            df3 = df2.rename(index={'UNDER': self._parent.toDate})
        elif 'OVER' in self._classFilter:
            df2 = df.loc['OVER'].to_frame().T
            df3 = df2.rename(index={'OVER': self._parent.toDate})
        return df3

    def _XYplot(self, tableRow: int, layout: QHBoxLayout):

        """
        Generate a XY plot based on the selected data and configuration.
        """
        # Mapping tableRow values to configuration parameters
        tableRowConfig =self._get2DgraphsConfig()

        # Show colormap settings and get the current colormap
        self._showColormapSetting(True)
        colormap = self._parent.cmapDict[self._currentColormap]

        # Retrieve the base dataset
        baseDataFrame = self._dataSource.getADpartialFrame(self._parent.fromDate, self._parent.toDate)

        # Get configuration for the current tableRow
        config = tableRowConfig.get(tableRow)
        if not config:
            return

        # Retrieve specific data based on the tableRow configuration
        dataFunction = config["dataFunction"]
        dataArgs = config.get("dataArgs", {})
        seriesFunction = config["seriesFunction"]
        seriesArgs = config.get("seriesArgs", {})
        title = config["title"]
        resolution = config["resolution"]
        yLabel = config["yLabel"]
        fullScale = config["fullScale"]

        # Generate the DataFrame
        dataFrame = dataFunction(baseDataFrame, **dataArgs)
        series = seriesFunction(dataFrame, **seriesArgs)

        # Check if the DataFrame is valid
        if series is None:
            return

        # Calculate chart dimensions in pixels and inches

        pixelWidth = (self._szBase.width() * self._hZoom)
        pixelHeight = (self._szBase.height() * self._vZoom)
        if pixelWidth > 65535:
            pixelWidth = 65535
        if pixelHeight > 65535:
            pixelHeight = 65535
        inchWidth = pixelWidth / self._px  # from  pixels to inches
        inchHeight = pixelHeight / self._px  # from  pixels to inches
        print("pixelWidth={}, pixelHeight={}, inchWidth={}, inchHeight={}".format(pixelWidth, pixelHeight,
                                                                                  inchWidth, inchHeight))
        xygraph = StatPlot(series, self._settings, inchWidth, inchHeight, title, yLabel, resolution,
                           self._showValues,
                           self._showGrid, self._smoothPlots, self._considerBackground)

        canvas = xygraph.widget()

        # Embeds the xygraph in the layout
        canvas = xygraph.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the xygraph object for future reference
        self._plot = xygraph

    def _bargraph(self, tableRow: int, layout: QHBoxLayout):
        """
        Generate a bargraph visualization based on the selected data and configuration.
        """
        # Mapping tableRow values to configuration parameters
        tableRowConfig = self._get2DgraphsConfig()

        colormap = self._parent.cmapDict[self._currentColormap]

        # Retrieve the base dataset
        baseDataFrame = self._dataSource.getADpartialFrame(self._parent.fromDate, self._parent.toDate)

        # Get configuration for the current tableRow
        config = tableRowConfig.get(tableRow)
        if not config:
            return

        # Retrieve specific data based on the tableRow configuration
        dataFunction = config["dataFunction"]
        dataArgs = config.get("dataArgs", {})
        seriesFunction = config["seriesFunction"]
        seriesArgs = config.get("seriesArgs", {})
        title = config["title"]
        resolution = config["resolution"]
        yLabel = config["yLabel"]
        fullScale = config["fullScale"]

        # Generate the DataFrame
        dataFrame = dataFunction(baseDataFrame, **dataArgs)
        series = seriesFunction(dataFrame, **seriesArgs)

        # Check if the DataFrame is valid
        if series is None:
            return

        # Calculate chart dimensions in pixels and inches
        pixelWidth = min(self._szBase.width() * self._hZoom, 65535)
        pixelHeight = min(self._szBase.height() * self._vZoom, 65535)
        inchWidth = pixelWidth / self._px  # Convert pixels to inches
        inchHeight = pixelHeight / self._px

        print(f"pixelWidth={pixelWidth}, pixelHeight={pixelHeight}, inchWidth={inchWidth}, inchHeight={inchHeight}")

        # Creates the Bargraph object
        if tableRow == self.TAB_RMOB_MONTH:
            # override is only for GUI, the BargraphRMOB always behaves so
            overrideShowValues = False
            self._ui.chkShowValues.setChecked(overrideShowValues)
            overrideShowGrid = False
            self._ui.chkGrid_2.setChecked(overrideShowGrid)
            overrideCmap = "colorgramme"
            self._showColormapSetting(True, overrideCmap)
            cm = self._parent.cmapDict[overrideCmap]
            bargraph = BargraphRMOB(series, self._settings, inchWidth, inchHeight, cm, fullScale(dataFrame))
        else:
            bargraph = Bargraph(series, self._settings, inchWidth, inchHeight, title, yLabel, resolution,
                                self._showValues,
                                self._showGrid, self._considerBackground)

        # Embeds the bargraph in the layout
        canvas = bargraph.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the Bargraph object for future reference
        self._plot = bargraph

    def _heatmap(self, tableRow: int, layout: QHBoxLayout):
        """
        Generate a heatmap visualization based on the selected data and configuration.
        """
        # Mapping tableRow values to configuration parameters
        tableRowConfig = {
            self.TAB_COUNTS_BY_DAY: {
                "title": "Daily counts by classification",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyCountsByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
            },
            self.TAB_POWERS_BY_DAY: {
                "title": "Average S-N in the covered dates by classification",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyPowersByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate},
            },
            self.TAB_LASTINGS_BY_DAY: {
                "title": "Average lastings in the covered dates by classification",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyLastingsByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate, },
            },
            self.TAB_RMOB_MONTH: {
                "title": "RMOB monthly summary",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilterRMOB,
                             "placeholder": -1},
            },
            self.TAB_COUNTS_BY_HOUR: {
                "title": "Hourly counts",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
            },
            self.TAB_POWERS_BY_HOUR: {
                "title": "Average S-N by hour",
                "resolution": "hour",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter},
            },
            self.TAB_LASTINGS_BY_HOUR: {
                "title": "Average lastings by hour",
                "resolution": "hour",
                "dataFunction": self._dataSource.makeLastingsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter},
            },

            self.TAB_COUNTS_BY_10M: {
                "title": "Counts by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makeCountsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter,
                             "compensate": self._compensation,
                             "considerBackground": self._considerBackground},
            },
            self.TAB_POWERS_BY_10M: {
                "title": "Average S-N by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter},
            },
            self.TAB_LASTINGS_BY_10M: {
                "title": "Average lastings by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makeLastingsDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter},
            },
            self.TAB_SPORADIC_BG_BY_HOUR: {
                "title": "Sporadic background by hour",
                "resolution": "hour",
                "dataFunction": lambda df: self._selectSporadicDf(self.avgHourDf),
                # note: the df parameter is intentionally ignored
                "dataArgs": {},
            },
            self.TAB_SPORADIC_BG_BY_10M: {
                "title": "Sporadic background by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": lambda df: self._selectSporadicDf(self.avg10minDf),
                # note: the df parameter is intentionally ignored
                "dataArgs": {},
            },
        }
        # Show colormap settings and get the current colormap
        self._showColormapSetting(True)
        colormap = self._parent.cmapDict[self._currentColormap]

        # Retrieve the base dataset
        baseDataFrame = self._dataSource.getADpartialFrame(self._parent.fromDate, self._parent.toDate)

        # Get configuration for the current tableRow
        config = tableRowConfig.get(tableRow)
        if not config:
            return

        # Retrieve specific data based on the tableRow configuration
        dataFunction = config["dataFunction"]
        dataArgs = config.get("dataArgs", {})
        title = config["title"]
        resolution = config["resolution"]

        # Generate the DataFrame
        dataFrame = dataFunction(baseDataFrame, **dataArgs)

        # Check if the DataFrame is valid
        if dataFrame is None:
            return

        # Calculate chart dimensions in pixels and inches
        pixelWidth = min(self._szBase.width() * self._hZoom, 65535)
        pixelHeight = min(self._szBase.height() * self._vZoom, 65535)
        inchWidth = pixelWidth / self._px  # Convert pixels to inches
        inchHeight = pixelHeight / self._px

        print(f"pixelWidth={pixelWidth}, pixelHeight={pixelHeight}, inchWidth={inchWidth}, inchHeight={inchHeight}")

        # Create the Heatmap object
        if tableRow == self.TAB_RMOB_MONTH:
            heatmap = HeatmapRMOB(dataFrame, self._settings, inchWidth, inchHeight, self._parent.cmapDict['colorgramme'])
        else:
            heatmap = Heatmap(dataFrame, self._settings, inchWidth, inchHeight, colormap, title, resolution,
                              self._showValues, self._showGrid, self._considerBackground)

        # Embed the Heatmap in the layout
        canvas = heatmap.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the Heatmap object for future reference
        self._plot = heatmap

    def _pieGraph(self, tableRow: int, layout: QHBoxLayout):
        pie = None
        if tableRow == self.TAB_COUNTS_BY_DAY:
            # pie chart by classification, shows only the filtered classes
            df = self._dataSource.totalsByClassification(self._classFilter, self._parent.fromDate,
                                                         self._parent.toDate,
                                                         compensate=self._compensation,
                                                         considerBackground=self._considerBackground)

            pixWidth = (self._szBase.width() * self._hZoom)
            pixHeight = (self._szBase.height() * self._vZoom)
            if pixWidth > 65535:
                pixWidth = 65535
            if pixHeight > 65535:
                pixHeight = 65535

            inchWidth = pixWidth / self._px  # from  pixels to inches
            inchHeight = pixHeight / self._px  # from  pixels to inches
            print("pixWidth={}, pixHeight={}, inchWidth={}, inchHeight={}".format(pixWidth, pixHeight,
                                                                                  inchWidth, inchHeight))

            pie = NestedPie(df, self._settings, inchWidth, inchHeight, self._considerBackground)
            canvas = pie.widget()
            canvas.setMinimumSize(QSize(int(pixWidth), int(pixHeight)))
            self._diagram.setWidget(canvas)
            layout.addWidget(self._diagram)
        self._plot = pie
