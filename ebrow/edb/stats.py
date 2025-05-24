"""
******************************************************************************

    Echoes Data Browser (Ebrow) is a data navigation and report generation
    tool for Echoes.
    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
    Both copyright (C) 2018-2025
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
from typing import Union

import numpy as np
# from scipy.optimize import curve_fit
import matplotlib.pyplot as plt
import pandas as pd

from PyQt5.QtCore import QSize
from PyQt5.QtGui import QPainter, QPixmap, QFont, QColor
from PyQt5.QtWidgets import QHBoxLayout, QScrollArea, QInputDialog, qApp, QAbstractItemView
from PyQt5.QtCore import Qt, QItemSelectionModel

from .heatmap import Heatmap
from .hm_rmob import HeatmapRMOB
from .bg_rmob import BargraphRMOB
from .bargraph import Bargraph
from .statplot import StatPlot
from .miplot import MIplot
from .aslplot import ASLplot
from .distplot import DistPlot
from .pandasmodel import PandasModel
from .utilities import notice, cryptDecrypt, mkExportFolder, addDateDelta, radiantAltitudeCorrection, utcToASL
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
    TAB_MASS_INDEX_BY_POWERS = 9
    TAB_MASS_INDEX_BY_LASTINGS = 10
    TAB_POWER_DISTRIBUTION = 11
    TAB_LASTING_DISTRIBUTION = 12
    TAB_CUMULATIVE_COUNTS_BY_POWERS = 13
    TAB_CUMULATIVE_COUNTS_BY_LASTINGS = 14
    TAB_SESSIONS_REGISTER = 15
    TAB_RMOB_MONTH = 16
    TAB_SPORADIC_BG_BY_HOUR = 17
    TAB_SPORADIC_BG_BY_10M = 18
    TAB_METEOR_SHOWERS = 19

    GRAPH_PLOT = 0
    GRAPH_HEATMAP = 1
    GRAPH_BARS = 2

    filterDesc = {'OVER': "Overdense event", 'UNDER': "Underdense event", 'FAKE RFI': "Fake event (RFI)",
                  'FAKE ESD': "Fake event (ESD)", 'FAKE CAR1': "Fake event (Carrier#1)",
                  'FAKE CAR2': "Fake event (Carrier#2)", 'FAKE SAT': "Fake event (Saturation)",
                  'FAKE LONG': "Event lasting too long",
                  "ACQ ACT": "Acquisition active"}

    def __init__(self, parent, ui, settings):
        self._subDataFrame = None
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
        self._rawDataFrame = None
        self._sbDataFrame = None
        self._considerBackground = False
        self._compensation = False
        self._timeUnitSize = 1
        self._radarComp = 1.0
        self._targetShower = 'None'
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
        self._sbIsMin = self._settings.readSettingAsBool('sporadicTypeMin')
        self._ui.rbSBmin.clicked.connect(self._toggleSBmode)
        self._ui.rbSBavg.clicked.connect(self._toggleSBmode)

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
        self._ui.rbSBmin.setChecked(self._sbIsMin)

        self._ui.hsHzoom_3.valueChanged.connect(self._changeHzoom)
        self._ui.hsVzoom_3.valueChanged.connect(self._changeVzoom)
        self._ui.chkLinked_3.clicked.connect(self._toggleLinkedCursors)
        self._ui.sbTUsize.valueChanged.connect(self._changeTUsize)
        self._ui.sbRadarComp.valueChanged.connect(self._changeRadarComp)
        self._ui.cbShower.currentTextChanged.connect(self._changeTargetShower)

        self._showDiagramSettings(False)
        self._showColormapSetting(False)
        self._ui.twTables.setTabVisible(1, False)
        self._ui.twTables.setTabVisible(2, False)
        self._ui.twTables.setTabVisible(3, False)

    def _get2DgraphsConfig(self):
        # 2D graphs (XY, scatter and bar graphs) configuration structure
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
                             "radarComp": self._radarComp,
                             "considerBackground": self._considerBackground},
                "seriesFunction": lambda df: df['Total'].squeeze(),
                "seriesArgs": {},
                "yLabel": "Filtered daily counts",
                "fullScale": -1
            },
            self.TAB_POWERS_BY_DAY: {
                "title": "Average S, in the covered dates, daily totals",
                "resolution": "day",
                "dataFunction": self._dataSource.dailyPowersByClassification,
                "dataArgs": {"filters": self._classFilter,
                             "dateFrom": self._parent.fromDate,
                             "dateTo": self._parent.toDate,
                             "highestAvgColumn": True
                             },
                "seriesFunction": lambda df: df['Average'].squeeze(),
                "seriesArgs": {},
                "yLabel": "Filtered average S by hour [dBfs]",
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
                             "radarComp": self._radarComp,
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
                             "radarComp": self._radarComp,
                             "considerBackground": self._considerBackground},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered hourly counts",
                "fullScale": -1
            },
            self.TAB_POWERS_BY_HOUR: {
                "title": "Average S by hour",
                "resolution": "hour",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": 'h',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 24)},
                "yLabel": "Filtered average s by hour [dBfs]",
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
                             "radarComp": self._radarComp,
                             "considerBackground": self._considerBackground},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered counts by 10min",
                "fullScale": -1
            },

            self.TAB_POWERS_BY_10M: {
                "title": "Average S by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": self._dataSource.makePowersDf,
                "dataArgs": {"dtStart": self._parent.fromDate,
                             "dtEnd": self._parent.toDate,
                             "dtRes": '10T',
                             "filters": self._classFilter},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered average S by 10 min [dBfs]",
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

            self.TAB_MASS_INDEX_BY_POWERS: {
                "title": "TBD",
                "resolution": "D",
                "dataFunction": self._calcMassIndicesDf,
                "dataArgs": {"TUsize": self._timeUnitSize,
                             "metric": 'power',
                             'filters': self._classFilter,
                             "finalDfOnly": True},
                "seriesFunction": self._getSelectedMIdata,
                "seriesArgs": {},
                "yLabel": "TBD",
                "fullScale": -1
            },

            self.TAB_MASS_INDEX_BY_LASTINGS: {
                "title": "TBD",
                "resolution": "D",
                "dataFunction": self._calcMassIndicesDf,
                "dataArgs": {"TUsize": self._timeUnitSize,
                             "metric": 'lasting',
                             'filters': self._classFilter,
                             "finalDfOnly": True},

                "seriesFunction": self._getSelectedMIdata,
                "seriesArgs": {},
                "yLabel": "TBD",
                "fullScale": -1
            },

            self.TAB_POWER_DISTRIBUTION: {
                "title": "Events distribution by power",
                "resolution": "D",
                "dataFunction": self._calculateDistributionDf,
                "dataArgs": {"metric": 'power',
                             'filters': self._classFilter},
                "seriesFunction": lambda df: df.set_index('S')['counts'],
                "seriesArgs": {"xScale": "linear", "yScale": "linear"},
                "yLabel": "Counts",
                "fullScale": -1
            },

            self.TAB_LASTING_DISTRIBUTION: {
                "title": "Events distribution by lasting",
                "resolution": "D",
                "dataFunction": self._calculateDistributionDf,
                "dataArgs": {"metric": 'lasting',
                             'filters': self._classFilter},
                "seriesFunction": lambda df: df.set_index('lasting_ms')['counts'],
                "seriesArgs": {"xScale": "log", "yScale": "linear"},
                "yLabel": "Counts",
                "fullScale": -1
            },

            self.TAB_CUMULATIVE_COUNTS_BY_POWERS: {
                "title": "Cumulative events count by power",
                "resolution": "D",
                "dataFunction": self._calculateCCountsDf,
                "dataArgs": {"TUsize": self._timeUnitSize,
                             "metric": 'power',
                             'filters': self._classFilter,
                             "finalDfOnly": True},
                "seriesFunction": lambda df: df.loc['Total'][1:],
                "seriesArgs": {"xScale": "linear", "yScale": "log"},
                "yLabel": "Log10(counts)",
                "fullScale": -1
            },

            self.TAB_CUMULATIVE_COUNTS_BY_LASTINGS: {
                "title": "Cumulative events count by lasting",
                "resolution": "D",
                "dataFunction": self._calculateCCountsDf,
                "dataArgs": {"TUsize": self._timeUnitSize,
                             "metric": 'lasting',
                             'filters': self._classFilter,
                             "finalDfOnly": True},
                "seriesFunction": lambda df: df.loc['Total'][1:],
                "seriesArgs": {"xScale": "linear", "yScale": "log"},
                "yLabel": "Log10(counts)",
                "fullScale": -1
            },

            self.TAB_SPORADIC_BG_BY_HOUR: {
                "title": "Sporadic background by hour",
                "resolution": "hour",
                "dataFunction": lambda df: self._selectSporadicDf(self._dataSource.avgHourDf),
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
                "dataFunction": lambda df: self._selectSporadicDf(self._dataSource.avg10minDf),
                "dataArgs": {},
                "seriesFunction": self._dataSource.tableTimeSeries,
                "seriesArgs": {"columns": range(0, 144)},
                "yLabel": "Filtered counts",
                "fullScale": -1
            },
        }
        return tableRowConfig

    def updateTabStats(self):
        self._dataSource = self._parent.dataSource
        if self._ui.twMain.currentIndex() == self._parent.TWMAIN_STATISTICS:
            self._timeUnitSize = self._settings.readSettingAsInt('MItimeUnitSize')
            self._ui.sbTUsize.setValue(self._timeUnitSize)

            self._radarComp = self._settings.readSettingAsFloat('RadarCompensation')

            self._ui.sbRadarComp.setValue(self._radarComp)
            enableSB = 0
            avgDailyStr = self._settings.readSettingAsObject('sporadicBackgroundDaily')
            if len(avgDailyStr) > 0:
                self._dataSource.avgDailyDict = json.loads(avgDailyStr)
                enableSB |= 1

            avgHourStr = self._settings.readSettingAsObject('sporadicBackgroundByHour')
            if len(avgHourStr) > 0:
                self._dataSource.avgHourDf = pd.DataFrame.from_dict(json.loads(avgHourStr))
                enableSB |= 2

            avg10minStr = self._settings.readSettingAsObject('sporadicBackgroundBy10min')
            if len(avg10minStr) > 0:
                self._dataSource.avg10minDf = pd.DataFrame.from_dict(json.loads(avg10minStr))
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
        the third is the sum of the two.
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
                    fromDate, toDate = self._dataSource.dbCoverage()

                    if fromDate <= dates[0] and toDate >= dates[1]:
                        clashes = self._dataSource.getActiveShowers(dates[0], dates[1])
                        if len(clashes) > 0:
                            result = self._parent.confirmMessage("Warning",
                                                                 f"The date interval {dates} clashes with the following meteor showers: {clashes}\n"
                                                                 "Press Cancel to stop calculation and change this interval\n"
                                                                 "or press OK to go ahead anyway.")
                            if result is False:
                                self._parent.busy(False)
                                return False

                        dfEvents = self._dataSource.getADpartialFrame(dates[0], dates[1])
                        if dfEvents is not None:
                            dfCountsHourlyUnder, rawDf, sbDf = self._dataSource.makeCountsDf(dfEvents, dates[0],
                                                                                             dates[1], dtRes='h',
                                                                                             filters='UNDER',
                                                                                             totalRow=False,
                                                                                             totalColumn=False)

                            if dfCountsHourlyUnder is None:
                                self._parent.busy(False)
                                return False

                            dfCounts10minUnder, rawDf, sbDf = self._dataSource.makeCountsDf(dfEvents, dates[0],
                                                                                            dates[1],
                                                                                            dtRes='10T',
                                                                                            filters='UNDER',
                                                                                            totalRow=False,
                                                                                            totalColumn=False)

                            dfCountsHourlyOver, rawDf, sbDf = self._dataSource.makeCountsDf(dfEvents, dates[0],
                                                                                            dates[1], dtRes='h',
                                                                                            filters='OVER',
                                                                                            totalRow=False,
                                                                                            totalColumn=False)

                            dfCounts10minOver, rawDf, sbDf = self._dataSource.makeCountsDf(dfEvents, dates[0], dates[1],
                                                                                           dtRes='10T',
                                                                                           filters='OVER',
                                                                                           totalRow=False,
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
                                if self._sbIsMin:
                                    df = dfCountsHourlyUnder.min().to_frame().T
                                else:
                                    df = dfCountsHourlyUnder.mean().to_frame().T

                                if avgHdfUnder is None:
                                    avgHdfUnder = df
                                else:
                                    avgHdfUnder = pd.concat([avgHdfUnder, df])

                            if dfCounts10minUnder.shape[0] > 0:
                                # ignores empty dfCounts10min
                                if self._sbIsMin:
                                    df = dfCounts10minUnder.min().to_frame().T
                                else:
                                    df = dfCounts10minUnder.mean().to_frame().T

                                if avg10dfUnder is None:
                                    avg10dfUnder = df
                                else:
                                    avg10dfUnder = pd.concat([avg10dfUnder, df])

                            if dfCountsHourlyOver.shape[0] > 0:
                                # ignores empty dfCountsHourly
                                if self._sbIsMin:
                                    df = dfCountsHourlyOver.min().to_frame().T
                                else:
                                    df = dfCountsHourlyOver.mean().to_frame().T

                                if avgHdfOver is None:
                                    avgHdfOver = df
                                else:
                                    avgHdfOver = pd.concat([avgHdfOver, df])

                            if dfCounts10minOver.shape[0] > 0:
                                # ignores empty dfCounts10min
                                if self._sbIsMin:
                                    df = dfCounts10minOver.min().to_frame().T
                                else:
                                    df = dfCounts10minOver.mean().to_frame().T

                                if avg10dfOver is None:
                                    avg10dfOver = df
                                else:
                                    avg10dfOver = pd.concat([avg10dfOver, df])

                        prog += 1
                        self._parent.updateProgressBar(prog, len(sporadicDatesList))
                    else:
                        self._parent.infoMessage("Error",
                                                 f"Date interval {dates} overlaps the selected DB coverage {fromDate}->{toDate}.\n"
                                                 "Please change this interval and perform a new calculation.")
                        self._parent.busy(False)
                        return calcDone

            avgDailyCountUnder = 0
            avgDailyCountOver = 0

            if avgHdfUnder is not None:
                avgHourDfUnder = avgHdfUnder.mean().round(0).astype(int).to_frame().T
                avgDailyCountUnder = int(avgHourDfUnder.iloc[0].sum())
                self._dataSource.avgHourDf = avgHourDfUnder
                self._dataSource.avgHourDf = self._dataSource.avgHourDf.rename(index={0: 'UNDER'})

            if avgHdfOver is not None:
                avgHourDfOver = avgHdfOver.mean().round(0).astype(int).to_frame().T
                avgDailyCountOver = int(avgHourDfOver.iloc[0].sum())
                self._dataSource.avgHourDf = pd.concat([self._dataSource.avgHourDf, avgHourDfOver])
                self._dataSource.avgHourDf = self._dataSource.avgHourDf.rename(index={0: 'OVER'})

            self._dataSource.avgDailyDict = {'UNDER': avgDailyCountUnder, 'OVER': avgDailyCountOver}
            avgDailyStr = json.dumps(self._dataSource.avgDailyDict)
            self._settings.writeSetting('sporadicBackgroundDaily', avgDailyStr)
            calcDone = True

            if self._dataSource.avgHourDf is not None:
                if self._dataSource.avgHourDf.shape[0] > 0:
                    self._dataSource.avgHourDf.loc['Total'] = self._dataSource.avgHourDf.sum(numeric_only=True, axis=0)
                    avgHourStr = json.dumps(self._dataSource.avgHourDf.to_dict())
                    self._settings.writeSetting('sporadicBackgroundByHour', avgHourStr)

            if avg10dfUnder is not None:
                self._dataSource.avg10minDf = avg10dfUnder.mean().round(0).astype(int).to_frame().T
                self._dataSource.avg10minDf = self._dataSource.avg10minDf.rename(index={0: 'UNDER'})

            if avg10dfOver is not None:
                avg10minDfOver = avg10dfOver.mean().round(0).astype(int).to_frame().T
                self._dataSource.avg10minDf = pd.concat([self._dataSource.avg10minDf, avg10minDfOver])
                self._dataSource.avg10minDf = self._dataSource.avg10minDf.rename(index={0: 'OVER'})

            if self._dataSource.avg10minDf is not None:
                if self._dataSource.avg10minDf.shape[0] > 0:
                    self._dataSource.avg10minDf.loc['Total'] = self._dataSource.avg10minDf.sum(numeric_only=True,
                                                                                               axis=0)
                    avg10minStr = json.dumps(self._dataSource.avg10minDf.to_dict())
                    self._settings.writeSetting('sporadicBackgroundBy10min', avg10minStr)

        self._parent.busy(False)
        return calcDone

    def _sporadicAveragesByThresholds(self, df: pd.DataFrame, filters: str, dateFrom: str = None, dateTo: str = None,
                                      TUsize: int = 1, metric: str = 'power', aggregateSporadic: bool = False,
                                      radarComp: float = 1.0) -> Union[pd.DataFrame, None]:
        """
        Calculates sporadic averages by thresholds, either for a single date range or for multiple sporadic periods.

        Args:
            df (pd.DataFrame): Input DataFrame with meteoric data.
            filters (str): Filter string for event classification.
            dateFrom (str, optional): Start date (inclusive). Defaults to None.
            dateTo (str, optional): End date (inclusive). Defaults to None.
            TUsize (int, optional): Time unit size in minutes. Defaults to 1.
            metric (str, optional): Counting metric ('power', 'lasting'). Defaults to 'power'.
            aggregateSporadic (bool, optional): If True, averages across multiple sporadic periods. Defaults to False.

        Returns:
            pd.DataFrame: DataFrame with sporadic averages by thresholds.
        """
        if aggregateSporadic:
            sbdfList = []
            sdList = self._settings.readSettingAsObject('sporadicDates')
            if len(sdList) > 0:
                sporadicDatesList = json.loads(sdList)
                for intervalStr in sporadicDatesList:
                    qApp.processEvents()
                    dates = intervalStr.split(" -> ")
                    sbdf = self._sporadicAveragesByThresholds(df, filters, dates[0], dates[1], TUsize, metric,
                                                              False, radarComp=radarComp)
                    sbdfList.append(sbdf)

                # Concatenate results and compute mean
                combinedSbdf = pd.concat(sbdfList, keys=range(len(sbdfList)))
                return combinedSbdf.groupby(level=1).mean().round().astype(int)

            return None

        # Calculate sporadic averages for a given date range
        odf, dummy1, dummy2, dummy3 = self._dailyCountsByThresholds(df, filters, dateFrom, dateTo, TUsize, metric,
                                                                    isSporadic=True, radarComp=radarComp)

        if odf is None or odf.empty:
            return pd.DataFrame()

        # Extract time units from the MultiIndex
        timeUnits = odf.index.get_level_values('time_unit').unique()

        # Initialize an empty DataFrame to store the averaged results
        averagedData = {}

        # Iterate through each unique time unit
        for timeUnit in timeUnits:
            # Filter odf for the current time unit
            timeUnitData = odf.xs(timeUnit, level='time_unit')

            if self._sbIsMin:
                # Calculate the minimum for the current time unit and round to the nearest integer
                averagedData[timeUnit] = timeUnitData.min().round().astype(int)
            else:
                # Calculate the mean for the current time unit and round to the nearest integer
                averagedData[timeUnit] = timeUnitData.mean().round().astype(int)

        # Create a new DataFrame from the averaged/minimum data
        sbdf = pd.DataFrame.from_dict(averagedData, orient='index')

        return sbdf

    def updateAndSendRMOBfiles(self):
        self.updateRMOBfiles(sendOk=True)

    def updateRMOBfiles(self, sendOk: bool = True):
        self._parent.busy(True)
        os.chdir(self._parent.workingDir)
        self._RMOBupdating = True
        self._dataSource = self._parent.dataSource
        self._ui.twStats.setCurrentIndex(self.STTW_TABLES)

        # Note: the RMOB data to be sent must be the recentmost month in DB
        # regardless the time coverage set in GUI
        (qDateFrom, qDateTo) = self._parent.dataSource.dbQDateCoverage()
        (y, m, d) = qDateTo.getDate()
        qDateFrom.setDate(y, m, 1)
        fromDate = qDateFrom.toString("yyyy-MM-dd")
        # toDate = self._parent.toDate
        toDate = qDateTo.toString("yyyy-MM-dd")

        df = self._dataSource.getADpartialFrame(fromDate, toDate)

        finalDf, rawDf, sbDf = self._dataSource.makeCountsDf(df, fromDate, toDate, dtRes='h',
                                                             filters=self._classFilterRMOB, totalRow=False,
                                                             totalColumn=False,
                                                             placeholder=-1)

        dfRMOB, monthNr, year = self._dataSource.makeRMOB(finalDf, lastOnly=True)
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
        colors = self._settings.readSettingAsObject('tableColorDict')
        emphasizedTextColor = colors['tableFg']
        emphasizedAltTextColor = colors['tableAltFg']
        emphasizedBackColor = colors['tableBg']

        rowColorDict = {
            '*': {'alignment': 'center'},
            'Total': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Totals': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Average': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Highest average': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
        }

        columnColorDict = {
            '*': {'alignment': 'center'},
            'Total': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Totals': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Average': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Highest average': {'fgColor': QColor(emphasizedTextColor), 'bgColor': QColor(emphasizedBackColor)},
            'Mass index': {'fgColor': QColor(emphasizedAltTextColor), 'bgColor': QColor(emphasizedBackColor)},
        }

        self._rawDataFrame = None
        self._ui.twTables.setTabVisible(1, False)
        self._subDataFrame = None
        self._ui.twTables.setTabVisible(2, False)
        self._sbDataFrame = None
        self._ui.twTables.setTabVisible(3, False)

        row = self._ui.lwTabs.currentRow()
        self._ui.tvTabs.setEnabled(False)

        if self._classFilter == '':
            # nothing to show
            self._parent.infoMessage('Statistic diagrams:',
                                     'No class filters set, nothing to show')
            self._parent.busy(False)
            return

        df = self._dataSource.getADpartialFrame(self._parent.fromDate, self._parent.toDate)
        sm = QAbstractItemView.NoSelection

        self._ui.sbTUsize.setEnabled(False)
        self._ui.cbShower.setEnabled(False)
        self._ui.chkCompensation.setEnabled(False)

        # calculating statistic dataframes:

        if row == self.TAB_COUNTS_BY_DAY:
            self._ui.chkCompensation.setEnabled(True)
            tuple3df = self._dataSource.dailyCountsByClassification(df,
                                                                    self._classFilter,
                                                                    self._parent.fromDate,
                                                                    self._parent.toDate,
                                                                    totalRow=True,
                                                                    totalColumn=True,
                                                                    compensate=self._compensation,
                                                                    radarComp=self._radarComp,
                                                                    considerBackground=self._considerBackground)
            if tuple3df is not None:
                self._dataFrame, self._rawDataFrame, self._sbDataFrame = tuple3df


        if row == self.TAB_COUNTS_BY_HOUR:
            self._ui.cbShower.setEnabled(True)
            self._ui.chkCompensation.setEnabled(True)
            tuple3df = self._dataSource.makeCountsDf(df,
                                                     self._parent.fromDate,
                                                     self._parent.toDate,
                                                     dtRes='h',
                                                     filters=self._classFilter,
                                                     totalRow=True,
                                                     totalColumn=True,
                                                     compensate=self._compensation,
                                                     radarComp=self._radarComp,
                                                     considerBackground=self._considerBackground)
            if tuple3df is not None:
                self._dataFrame, self._rawDataFrame, self._sbDataFrame = tuple3df

        if row == self.TAB_COUNTS_BY_10M:
            self._ui.cbShower.setEnabled(True)
            self._ui.chkCompensation.setEnabled(True)
            tuple3df = self._dataSource.makeCountsDf(df,
                                                     self._parent.fromDate,
                                                     self._parent.toDate,
                                                     dtRes='10T',
                                                     filters=self._classFilter,
                                                     totalRow=True,
                                                     totalColumn=True,
                                                     compensate=self._compensation,
                                                     radarComp=self._radarComp,
                                                     considerBackground=self._considerBackground)

            if tuple3df is not None:
                self._dataFrame, self._rawDataFrame, self._sbDataFrame = tuple3df
                self._dataFrame = self._dataSource.splitAndStackDataframe(self._dataFrame, maxColumns=24)
                if self._rawDataFrame is not None:
                    self._rawDataFrame = self._dataSource.splitAndStackDataframe(self._rawDataFrame, maxColumns=24)

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
            df2, dummy1, dummy2 = self._dataSource.makeCountsDf(df, self._parent.fromDate, self._parent.toDate,
                                                                dtRes='h',
                                                                filters=self._classFilterRMOB, totalRow=False,
                                                                totalColumn=False,
                                                                compensate=self._compensation,
                                                                radarComp=self._radarComp,
                                                                considerBackground=self._considerBackground)
            self._dataFrame, monthName, year = self._dataSource.makeRMOB(df2)

        if row == self.TAB_SPORADIC_BG_BY_HOUR:
            self._dataFrame = self._dataSource.avgHourDf

        if row == self.TAB_SPORADIC_BG_BY_10M:
            self._dataFrame = self._dataSource.avg10minDf

        if row == self.TAB_SPORADIC_BG_BY_HOUR or row == self.TAB_SPORADIC_BG_BY_10M:
            if 'UNDER' in self._classFilter and 'OVER' in self._classFilter:
                pass
            elif 'UNDER' in self._classFilter:
                self._dataFrame = df.filter(items=['UNDER'], axis=0)
            elif 'OVER' in self._classFilter:
                self._dataFrame = df.filter(items=['OVER'], axis=0)

        if row == self.TAB_MASS_INDEX_BY_POWERS:
            self._ui.cbShower.setEnabled(True)
            self._ui.sbTUsize.setEnabled(True)
            tuple4df = self._calcMassIndicesDf(df, filters=self._classFilter, TUsize=self._timeUnitSize, metric='power')
            if tuple4df is not None:
                self._dataFrame, self._subDataFrame, self._rawDataFrame, self._sbDataFrame = tuple4df
                # allows rows and columns selection
                sm = QAbstractItemView.ExtendedSelection

        if row == self.TAB_MASS_INDEX_BY_LASTINGS:
            self._ui.cbShower.setEnabled(True)
            self._ui.sbTUsize.setEnabled(True)
            tuple4df = self._calcMassIndicesDf(df, filters=self._classFilter, TUsize=self._timeUnitSize,
                                               metric='lasting')
            if tuple4df is not None:
                self._dataFrame, self._subDataFrame, self._rawDataFrame, self._sbDataFrame = tuple4df
                # allows rows and columns selection
                sm = QAbstractItemView.ExtendedSelection

        if row == self.TAB_POWER_DISTRIBUTION:
            self._ui.cbShower.setEnabled(True)
            self._dataFrame = self._calculateDistributionDf(df, filters=self._classFilter, metric='power')

        if row == self.TAB_LASTING_DISTRIBUTION:
            self._ui.cbShower.setEnabled(True)
            self._dataFrame = self._calculateDistributionDf(df, filters=self._classFilter, metric='lasting')

        if row == self.TAB_CUMULATIVE_COUNTS_BY_POWERS:
            self._ui.cbShower.setEnabled(True)
            self._ui.sbTUsize.setEnabled(True)
            tuple4df = self._calculateCCountsDf(df, filters=self._classFilter, TUsize=self._timeUnitSize,
                                                metric='power')
            if tuple4df is not None:
                self._dataFrame, self._subDataFrame, self._rawDataFrame, self._sbDataFrame = tuple4df

        if row == self.TAB_CUMULATIVE_COUNTS_BY_LASTINGS:
            self._ui.cbShower.setEnabled(True)
            self._ui.sbTUsize.setEnabled(True)
            tuple4df = self._calculateCCountsDf(df, filters=self._classFilter, TUsize=self._timeUnitSize,
                                                metric='lasting')
            if tuple4df is not None:
                self._dataFrame, self._subDataFrame, self._rawDataFrame, self._sbDataFrame = tuple4df

        if row == self.TAB_METEOR_SHOWERS:
            self._dataFrame = self._parent.tabPrefs.getMSC()
            # allows columns selection for sorting
            sm = QAbstractItemView.ExtendedSelection
            self._ui.tvTabs.setSortingEnabled(True)

        # Displaying the dataframes as QTableViews:

        if self._dataFrame is not None:
            self._ui.tvTabs.setEnabled(True)
            model = PandasModel(self._dataFrame, rowStyles=rowColorDict, columnStyles=columnColorDict)
            self._ui.tvTabs.setModel(model)
            self._ui.tvTabs.setSelectionMode(sm)

        if self._subDataFrame is not None:
            self._ui.twTables.setTabVisible(1, True)
            model = PandasModel(self._subDataFrame, rowStyles=rowColorDict, columnStyles=columnColorDict)
            self._ui.tvTabsSub.setModel(model)
            self._ui.tvTabs.setSelectionMode(sm)
        else:
            self._ui.twTables.setTabVisible(1, False)

        if self._rawDataFrame is not None:
            self._ui.twTables.setTabVisible(2, True)
            model = PandasModel(self._rawDataFrame, rowStyles=rowColorDict, columnStyles=columnColorDict)
            self._ui.tvTabsRaw.setModel(model)
            self._ui.tvTabs.setSelectionMode(sm)
        else:
            self._ui.twTables.setTabVisible(2, False)

        if self._sbDataFrame is not None:
            self._ui.twTables.setTabVisible(3, True)
            model = PandasModel(self._sbDataFrame, rowStyles=rowColorDict, columnStyles=columnColorDict)
            self._ui.tvTabsBg.setModel(model)
            self._ui.tvTabs.setSelectionMode(sm)
        else:
            self._ui.twTables.setTabVisible(3, False)

        self._parent.busy(False)

    def showDataDiagram(self):
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
                30,  # mass index by powers
                30,  # mass index by lastings
                366,  # events distribution by power
                366,  # events distribution by lasting
                30,  # cumulative counts by powers
                30,  # cumulative counts by lastings
                0,  # session table, no graphics
                1,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # meteor showers
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
                0,  # mass index by powers, only plots
                0,  # mass index by lastings, only plots
                0,  # events distribution by power
                0,  # events distribution by lasting
                0,  # cumulative counts by powers
                0,  # cumulative counts by lastings
                0,  # session table, no graphics
                31,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # meteor showers

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
                0,  # mass index by powers, only plots
                0,  # mass index by lastings, only plots
                366,  # events distribution by power
                366,  # events distribution by lasting
                0,  # cumulative counts by powers
                0,  # cumulative counts by lastings
                0,  # session table, no graphics
                1,  # RMOB month, current day only
                1,  # daily sporadic background by hour
                1,  # daily sporadic background by 10min
                0,  # meteor showers
            ],
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
            self.clearLayout(layout)

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
                if self.TAB_MASS_INDEX_BY_POWERS <= tableRow <= self.TAB_MASS_INDEX_BY_LASTINGS:
                    self._MIplot(tableRow, layout)

                elif self.TAB_POWER_DISTRIBUTION <= tableRow <= self.TAB_CUMULATIVE_COUNTS_BY_LASTINGS:
                    self._distPlot(tableRow, layout)

                else:
                    self._XYplot(tableRow, layout)

            elif graphRow == self.GRAPH_HEATMAP:
                self._heatmap(tableRow, layout)

            elif graphRow == self.GRAPH_BARS:
                self._bargraph(tableRow, layout)


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
        prog = "{}".format((now - midnight).seconds)
        if os.path.exists(self._exportDir):
            os.chdir(self._exportDir)
            if self._ui.twStats.currentIndex() == self.STTW_TABLES:
                # the displayed table is exported as csv
                subTab = self._ui.twTables.currentIndex()

                title = f"{self._ui.lwTabs.currentItem().text()}_{self._ui.twTables.tabText(subTab)}-{prog}"
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

                if subTab == 0:
                    self._dataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())

                if subTab == 1:
                    self._subDataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())

                if subTab == 2:
                    self._rawDataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())

                if subTab == 3:
                    self._sbDataFrame.to_csv(csvName, index=True, sep=self._settings.dataSeparator())

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
                title = self._ui.lwTabs.currentItem().text() + '-' + prog
                title = title.lower().replace(' ', '_')
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

    def _toggleSBmode(self, state):
        self._sbIsMin = self._ui.rbSBmin.isChecked()
        self._settings.writeSetting('sporadicTypeMin', self._sbIsMin)

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

    def _changeTargetShower(self, val):
        if self._dataSource is not None:
            self._settings.writeSetting('targetShower', val)
            self._targetShower = val

    def _changeTUsize(self, val):
        self._settings.writeSetting('MItimeUnitSize', val)
        self._timeUnitSize = val

    def _changeRadarComp(self, val):
        self._settings.writeSetting('RadarCompensation', val)
        self._radarComp = val

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

        if (row == self.TAB_SESSIONS_REGISTER or row == self.TAB_RMOB_MONTH or row == self.TAB_METEOR_SHOWERS or
                row == self.TAB_POWER_DISTRIBUTION or row == self.TAB_LASTING_DISTRIBUTION):
            # RMOB data use an hardcoded filters, including only
            # non-fake events.
            # background subtraction and compensations are senseless for distributions
            self._ui.gbDataSettings.setVisible(False)
            if row != self.TAB_POWER_DISTRIBUTION and row != self.TAB_LASTING_DISTRIBUTION:
                # filtering could have some sense with distributions
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
        tableRowConfig = self._get2DgraphsConfig()

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

        considerBackground = False
        if "considerBackground" in dataArgs.keys():
            considerBackground = dataArgs["considerBackground"]

        # Generate the DataFrame
        retval = dataFunction(baseDataFrame, **dataArgs)
        dataFrame = retval
        if isinstance(retval, tuple):
            # if retval is a tuple, takes the first element (final data)
            dataFrame = retval[0]

        df = dataFrame
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
                           self._showGrid, self._smoothPlots, considerBackground)

        # Embeds the xygraph in the layout
        canvas = xygraph.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the xygraph object for future reference
        self._plot = xygraph

    def _MIplot(self, tableRow: int, layout: QHBoxLayout):

        """
        Scattered plot for mass indices
        """
        # Mapping tableRow values to configuration parameters
        tableRowConfig = self._get2DgraphsConfig()

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
        yLabel = config["yLabel"]
        metric = config["dataArgs"]["metric"]

        # Generate the DataFrame and extracts the total series
        dataFrame = dataFunction(baseDataFrame, **dataArgs)
        seriesFunction = config['seriesFunction']
        series, selection, selTitle = seriesFunction(dataFrame)

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

        if selection == "row":
            # log/log counts scatter plot vs. thresholds and linear regression
            title = f"Linear regression of log(counts) by thresholds for apparent solar longitude = {selTitle}°"
            yLabel = "Log10 counts"
            graph = MIplot(series, self._settings, inchWidth, inchHeight, metric, title, yLabel,
                           self._showValues, self._showGrid)
        else:
            if selection == "all":
                yLabel = "Mass index"
                title = "Mass indexes by apparent solar longitude"

            if selection == "column":
                yLabel = "Counts"
                title = f"Counts exceeding threshold {selTitle}"

            graph = ASLplot(series, self._settings, inchWidth, inchHeight, title, yLabel,
                            self._showValues,
                            self._showGrid, self._smoothPlots)

        # Embeds the graph in the layout
        canvas = graph.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the graph object for future reference
        self._plot = graph

    def _distPlot(self, tableRow: int, layout: QHBoxLayout):

        """
        XY plot for distributions
        """
        # Mapping tableRow values to configuration parameters
        tableRowConfig = self._get2DgraphsConfig()

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
        yLabel = config["yLabel"]
        metric = config["dataArgs"]["metric"]
        xLabel = None

        if metric == "lasting":
            xLabel = "lasting [mS]"
        if metric == "power":
            xLabel = "power [dBfs]"

        # Generate the DataFrame and extracts the total series
        dataFrame = dataFunction(baseDataFrame, **dataArgs)
        seriesFunction = config['seriesFunction']
        series = seriesFunction(dataFrame)

        # the series args are used to carry additional params for plot function
        extraArgs = config['seriesArgs']
        xScale = extraArgs["xScale"]
        yScale = extraArgs["yScale"]
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
        distgraph = DistPlot(series, self._settings, inchWidth, inchHeight, metric, title, xLabel, yLabel,
                             xScale, yScale, self._showValues, self._showGrid)

        # Embeds the distgraph in the layout
        canvas = distgraph.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the distgraph object for future reference
        self._plot = distgraph

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

        considerBackground = False
        if "considerBackground" in dataArgs.keys():
            considerBackground = dataArgs["considerBackground"]

        # Generate the DataFrame
        retval = dataFunction(baseDataFrame, **dataArgs)
        dataFrame = retval
        if isinstance(retval, tuple):
            # if retval is a tuple, takes the first element (final data)
            dataFrame = retval[0]

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
                                self._showGrid, considerBackground)

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
                             "radarComp": self._radarComp,
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
                             "radarComp": self._radarComp,
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
                             "radarComp": self._radarComp,
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
                "dataFunction": lambda df: self._selectSporadicDf(self._dataSource.avgHourDf),
                # note: the df parameter is intentionally ignored
                "dataArgs": {},
            },
            self.TAB_SPORADIC_BG_BY_10M: {
                "title": "Sporadic background by 10-minute intervals",
                "resolution": "10m",
                "dataFunction": lambda df: self._selectSporadicDf(self._dataSource.avg10minDf),
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

        considerBackground = False
        if "considerBackground" in dataArgs.keys():
            considerBackground = dataArgs["considerBackground"]

        # Generate the DataFrame
        retval = dataFunction(baseDataFrame, **dataArgs)
        dataFrame = retval
        if isinstance(retval, tuple):
            # if retval is a tuple, takes the first element (final data)
            dataFrame = retval[0]

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
            heatmap = HeatmapRMOB(dataFrame, self._settings, inchWidth, inchHeight,
                                  self._parent.cmapDict['colorgramme'])
        else:
            heatmap = Heatmap(dataFrame, self._settings, inchWidth, inchHeight, colormap, title, resolution,
                              self._showValues, self._showGrid, considerBackground)

        # Embed the Heatmap in the layout
        canvas = heatmap.widget()
        canvas.setMinimumSize(QSize(int(pixelWidth), int(pixelHeight)))
        self._diagram.setWidget(canvas)
        layout.addWidget(self._diagram)

        # Store the Heatmap object for future reference
        self._plot = heatmap

    def _calculateDistributionDf(self, df: pd.DataFrame, filters: str, metric: str):
        """
        Calculates the power or lasting distribution of all events in df

        Args:
            df (pd.DataFrame): DataFrame of events (falling edges only)
            filters:
            metric: power or lasting

        Returns:
            pd.DataFrame: DataFrame of required distribution, crescent powers or lastings
            associated with the counts of events having the same power or lasting
            For power, one row for each dBfs (approximate to integer values), while
            lastings instead are approximated by multiples of the latest scan interval time
        """
        sdf = None
        df = df.loc[df['event_status'] == 'Fall']

        # Filter by classification
        if filters:
            strippedFilters = [f.strip() for f in filters.split(',')]  # Split filters string
            df = df[df['classification'].isin(strippedFilters)]

        if metric == 'power':
            # sdf = df['S'].astype(int).value_counts().sort_index().reset_index()
            sdf = df['S'].round(1).value_counts().sort_index().reset_index()
            sdf.columns = ['S', 'counts']

        if metric == 'lasting':
            si = self._dataSource.getEchoesSamplingInterval()
            lastingsRounded = (df['lasting_ms'] / si).round() * si
            sdf = lastingsRounded.value_counts().sort_index().reset_index()
            sdf.columns = ['lasting_ms', 'counts']
        return sdf

    def _calculateCCountsDf(self, df: pd.DataFrame, filters: str, TUsize: int, metric: str, finalDfOnly: bool = False):
        sbf = None
        if self._considerBackground:
            # calculates a dataframe with sporadic background by thresholds
            # the sporadic is calculated starting from a base of an entire year of data
            oneYearAgo = addDateDelta(self._parent.fromDate, -366)
            fullSbf = self._dataSource.getADpartialFrame(oneYearAgo, self._parent.toDate)
            sbf = self._sporadicAveragesByThresholds(fullSbf, filters, TUsize=TUsize, metric=metric,
                                                     aggregateSporadic=True, radarComp=self._radarComp)
        tuple4df = self._dailyCountsByThresholds(df, filters,
                                                 self._parent.fromDate,
                                                 self._parent.toDate,
                                                 TUsize=TUsize,
                                                 metric=metric,
                                                 sporadicBackgroundDf=sbf,
                                                 radarComp=self._radarComp)

        finalDf, subDf, rawDf, sporadicBackgroundDf = tuple4df
        finalDf.drop('Mass index', axis=1, inplace=True)
        finalDf.loc['Total'] = finalDf.sum(numeric_only=True, axis=0)
        for col in finalDf.select_dtypes(include=['number']).columns:
            finalDf[col] = pd.to_numeric(finalDf[col], errors='coerce').astype('Int64')

        if self._considerBackground:
            subDf.drop('Mass index', axis=1, inplace=True)
            subDf.loc['Total'] = subDf.sum(numeric_only=True, axis=0)
            for col in subDf.select_dtypes(include=['number']).columns:
                subDf[col] = pd.to_numeric(subDf[col], errors='coerce').astype('Int64')

            rawDf.drop('Mass index', axis=1, inplace=True)
            rawDf.loc['Total'] = rawDf.sum(numeric_only=True, axis=0)
            for col in rawDf.select_dtypes(include=['number']).columns:
                rawDf[col] = pd.to_numeric(rawDf[col], errors='coerce').astype('Int64')

        tuple4df = finalDf, subDf, rawDf, sporadicBackgroundDf
        if finalDfOnly:
            return tuple4df[0]

        return tuple4df

    def _calcMassIndicesDf(self, df: pd.DataFrame, filters: str, TUsize: int, metric: str, finalDfOnly: bool = False):
        sbf = None
        if self._considerBackground:
            # calculates a dataframe with sporadic background by thresholds
            # the sporadic is calculated starting from a base of an entire year of data
            oneYearAgo = addDateDelta(self._parent.fromDate, -366)
            fullSbf = self._dataSource.getADpartialFrame(oneYearAgo, self._parent.toDate)
            sbf = self._sporadicAveragesByThresholds(fullSbf, filters, TUsize=TUsize, metric=metric,
                                                     aggregateSporadic=True, radarComp=self._radarComp)

        tuple4df = self._dailyCountsByThresholds(df, filters,
                                                 self._parent.fromDate,
                                                 self._parent.toDate,
                                                 TUsize=TUsize,
                                                 metric=metric,
                                                 sporadicBackgroundDf=sbf,
                                                 radarComp=self._radarComp)
        if finalDfOnly:
            return tuple4df[0]

        # mass indices calculated on subtracted data are unreliable
        if tuple4df is not None:
            tuple4df[1].drop('Mass index', axis=1, inplace=True)
        return tuple4df

    def _calculateMassIndex(self, df: pd.DataFrame, thresholds: list):
        """
        Calculates the mass index for each time unit (row) in the DataFrame using numpy.polyfit().

        Args:
            df (pd.DataFrame): DataFrame with counts for time units (rows) and thresholds (columns).
            thresholds (list): List of thresholds (linear power or duration).

        Returns:
            pd.DataFrame: DataFrame with mass indices for time units.
        """

        results = {}
        doneItems = 0
        self._parent.updateProgressBar(doneItems, df.shape[0])
        for index, row in df.iterrows():  # Iterate over time units (rows)
            timeUnit = row['time unit']
            eventCounts = row.values[1:]  # Counts for the current time unit
            unsortedThresholdsUsed = np.array(thresholds)  # Use all thresholds

            # Sort thresholds and counts in descending order
            sortedIndices = np.argsort(unsortedThresholdsUsed)[::-1]
            thresholdsUsed = unsortedThresholdsUsed[sortedIndices]
            eventCounts = eventCounts[sortedIndices]

            # Convert counts to log10, handling zeros
            fixedCounts = np.where(eventCounts > 0.0, eventCounts, 1.0).astype(float)
            logCounts = np.log10(fixedCounts)

            # Convert thresholds to log10
            logThresholds = np.log10(thresholdsUsed)

            try:
                # Perform linear regression using numpy.polyfit()
                slope, intercept = np.polyfit(logThresholds, logCounts, 1)
                k = 1
                results[index] = 1 - (
                        ((abs(slope) - k) * 4.0) / 3.0)
            except Exception as e:
                print(f"Error during fit for {timeUnit}: {e}")
                results[index] = np.nan

            doneItems += 1
            self._parent.updateProgressBar(doneItems, df.shape[0])

        if not results:
            return None

        return pd.DataFrame(results, index=['alpha']).T

    def _getSelectedCCounts(self, df):
        pass

    def _getSelectedMIdata(self, df):
        ctv = None
        cw = self._ui.twTables.currentWidget()
        if cw.objectName() == "tabFinal":
            ctv = self._ui.tvTabs

        if cw.objectName() == "tabSub":
            ctv = self._ui.tvTabsSub

        if cw.objectName() == "tabRaw":
            ctv = self._ui.tvTabsRaw

        if cw.objectName() == "tabBg":
            ctv = self._ui.tvTabsBg

        selectionModel = ctv.selectionModel()
        if selectionModel:
            selectedIndexes = selectionModel.selectedIndexes()
            if selectedIndexes:
                rowCount = df.shape[0]
                columnCount = df.shape[1]
                selectedRows = set(index.row() for index in selectedIndexes)
                if len(selectedRows) == 1 and len(selectedIndexes) == columnCount:
                    # Single row fully selected
                    rowNum = list(selectedRows)[0]
                    return df.iloc[rowNum, 1:-1], "row", df.index[rowNum]

                selectedColumns = set(index.column() for index in selectedIndexes)
                if len(selectedColumns) == 1 and len(selectedIndexes) == rowCount:
                    # Single column fully selected
                    colNum = list(selectedColumns)[0]
                    return df.iloc[:, colNum], "column", df.columns[colNum]

        return df['Mass index'], "all", ""

    def _completeMIdataframe(self, df: pd.DataFrame, metric: str, thresholds: list) -> pd.DataFrame:
        if df is not None:
            # the mass indices are not calculated for sporadic background
            self._parent.updateStatusBar("Calculating counts totals")

            if metric == 'power':
                # Convert thresholds to linear values to avoid calculate log(0)
                thresholdsMw = self._settings.powerThresholds(wantMw=True)
                if len(thresholdsMw) < len(thresholds):
                    self._parent.updateStatusBar("Converting power thresholds to mW partially failed")
                    print("thresholds in dB:", thresholds)
                    print("thresholds in mW:", thresholdsMw)
                thresholds = thresholdsMw

            # Calculate mass index
            massIndices = self._calculateMassIndex(df, thresholds)

            if massIndices is None:
                raise ValueError("Mass index calculation failed.")

            # Add mass index as a column (round to 2 decimal places)
            df['Mass index'] = massIndices['alpha'].round(2).values

        return df

    def _patchMIdataframe(self, df):
        """
        Ensure the df counts are decreasing by increasing thresholds,
        (monotonic) patching the values if it isn't and applicate the
        radiant elevation correction
        """
        # scanning counts rows
        patched = False
        for index, row in df.iterrows():
            print(f"index={index}")
            # scanning counts columns backwards to highest to lowest threshold
            # skipping the firt column (time unit)
            for j in range(len(row) - 2, 1, -1):
                k = j - 1
                # Compare current cell with next one
                print(f"j={j} content: {df.loc[index, df.columns[j]]}")
                if df.loc[index, df.columns[k]] < df.loc[index, df.columns[j]]:
                    # if greater, aligns its value
                    print(f"patching value {df.loc[index, df.columns[k]]} at k={k} to {df.loc[index, df.columns[j]]}")
                    df.loc[index, df.columns[k]] = df.loc[index, df.columns[j]]
                    patched = True

        # hiding timeunits when radiant was not visible. If None shower specified, skips this code
        self._targetShower = self._settings.readSettingAsString("targetShower") #self._ui.cbShower.currentText()
        if self._targetShower != "None":
            lat = self._settings.readSettingAsFloat('latitude')
            lon = self._settings.readSettingAsFloat('longitude')
            alt = self._settings.readSettingAsFloat('altitude')
            msc = self._parent.tabPrefs.getMSC()
            ts = msc[msc['name'] == self._targetShower]
            rowsToDrop = []
            for index, row in df.iterrows():
                print(f"index={index}")
                ds, tr = row['time unit']
                startHourStr, endHourStr = tr.replace('h', '').split('-')
                startHour = int(startHourStr)
                endHour = int(endHourStr)

                # Compute the average time in hours (can be float)
                meanHour = (startHour + endHour) / 2

                # Extract hour and minutes from the fractional hour
                hour = int(meanHour)
                minute = int(round((meanHour - hour) * 60))

                # Build ISO 8601 datetime string
                utcDatetimeStr = f"{ds}T{hour:02d}:{minute:02d}:00"

                sinAlt = radiantAltitudeCorrection(ts['ra'], ts['dec'], utcDatetimeStr, lat, lon, alt)
                if sinAlt == 0:
                    print(f"discarding timeunit {index} since radiant was not above the horizon")
                    rowsToDrop.append(index)
                else:
                    # skips the "time unit" column and sets the fixed counts in the following ones
                    fromCol = df.columns[1]
                    toCol = df.columns[-2]
                    countsToFix = np.array(row[1:-1]).astype(float)
                    df.loc[index, fromCol:toCol] = np.round(countsToFix * sinAlt)
                    print(f"timeunit {index} radiant was {sinAlt} above the horizon")

            df.drop(index=rowsToDrop, inplace=True)
        return df

    def _timeUnitsToASLindex(self, df):
        """
        calculate the apparent solar longitude for every time unit
        making a new df index with them
        """
        # scanning counts rows

        aslColumn = []
        for index, row in df.iterrows():
            print(f"index={index}")
            ds, tr = index
            startHourStr, endHourStr = tr.replace('h', '').split('-')
            startHour = int(startHourStr)
            endHour = int(endHourStr)

            # Compute the average time in hours (can be float)
            meanHour = (startHour + endHour) / 2

            # Extract hour and minutes from the fractional hour
            hour = int(meanHour)
            minute = int(round((meanHour - hour) * 60))

            # Build ISO 8601 datetime string
            utcDatetimeStr = f"{ds}T{hour:02d}:{minute:02d}:00"
            asl = utcToASL(utcDatetimeStr)
            print(f"timeunit {index} with average time {meanHour} has apparent solar longitude {asl}")
            aslColumn.append(asl)
        df['asl'] = aslColumn
        df['time unit'] = df.index
        df = df.set_index('asl')
        cols = df.columns.tolist()
        cols.remove('time unit')
        cols.insert(0, 'time unit')
        df = df[cols]
        return df

    def _dailyCountsByThresholds(self, df: pd.DataFrame, filters: str, dateFrom: str = None, dateTo: str = None,
                                 TUsize: int = 1, metric: str = 'power',
                                 isSporadic: bool = False, sporadicBackgroundDf: pd.DataFrame = None,
                                 radarComp: float = 1.0) -> Union[tuple, None]:
        """
        Calculates event counts per threshold, adds totals per threshold, mass index, and average mass index.

        Args:
            df (pd.DataFrame): DataFrame with event data.
            filters (str): Comma-separated list of classification filters.
            dateFrom (str, optional): Start date for filtering. Defaults to None.
            dateTo (str, optional): End date for filtering. Defaults to None.
            TUsize (int, optional): Time unit size in hours (1-24). Defaults to 1.
            metric (str, optional): Metric to use ('power' or 'lasting'). Defaults to 'power'.
            isSporadic (bool, optional): the method has been called to calculate the sporadic background
            sporadicBackgroundDf (pd.DataFrame, optional): DataFrame with sporadic background data. Defaults to None.
            radarComp (float, optional): radar scan effect compensation factor

        Returns:
            tuple of 4 pd.DataFrame: DataFrame with counts, totals per threshold, mass index, and average mass index.
                            final data, subtracted data, raw data and sporadic background (same of sporadicBackgroundDf)
                          Returns None on error.
        """
        retval = None

        if not 1 <= TUsize <= 24:
            raise ValueError("Invalid time unit size, must be 1 <= TUsize <= 24")

        if metric == 'power':
            thresholds = self._settings.powerThresholds()
            eventStatusFilter = 'Peak'
            valueColumn = 'S'
        elif metric == 'lasting':
            thresholds = self._settings.lastingThresholds()
            eventStatusFilter = 'Fall'
            valueColumn = 'lasting_ms'
        else:
            raise ValueError("Invalid metric. Choose 'power' or 'lasting'.")

        # Convert date strings to datetime objects for comparison
        if dateFrom:
            dateFrom = pd.to_datetime(dateFrom)
        if dateTo:
            dateTo = pd.to_datetime(dateTo)

        # Adjust dateFrom if the range exceeds 30 days
        if dateFrom and dateTo:
            dateRange = (dateTo - dateFrom).days
            if isSporadic is False and dateRange > 30:
                dateFrom = dateTo - pd.Timedelta(days=30)  # Set dateFrom to 30 days before dateTo
                self._parent.infoMessage("Warning",
                                         "The selected coverage exceeds the 30 days limit for mass indexes calculation.\n"
                                         f"The days preceeding {dateFrom} won't be considered")

        # Filter by event_status and date range
        shortDf = df[df['event_status'] == eventStatusFilter].copy()
        if dateFrom:
            shortDf = shortDf[pd.to_datetime(shortDf['utc_date']) >= dateFrom]
        if dateTo:
            shortDf = shortDf[pd.to_datetime(shortDf['utc_date']) <= dateTo]

        # Filter by classification
        if filters:
            strippedFilters = [f.strip() for f in filters.split(',')]  # Split filters string
            shortDf = shortDf[shortDf['classification'].isin(strippedFilters)]

        # Create a list of all date and time unit combinations
        dateTimeUnits = []
        for date in shortDf['utc_date'].unique():
            for hour in range(0, 24, TUsize):
                qApp.processEvents()
                timeUnit = f"{hour:02d}h-{hour + TUsize:02d}h"  # es. "00h-01h", "01h-02h", ecc.
                dateTimeUnits.append((date, timeUnit))

        # Initialize odf with zeros for ALL date and time unit combinations and thresholds
        subDf = pd.DataFrame(index=pd.MultiIndex.from_tuples(dateTimeUnits, names=['utc_date', 'time_unit']))
        rawDf = None
        if metric == 'power':
            for threshold in thresholds:
                colName = f"{threshold:.1f}"
                subDf[colName] = 0  # Initialize all columns to 0
        else:
            for threshold in thresholds:
                colName = str(threshold)
                subDf[colName] = 0  # Initialize all columns to 0

        # Sort thresholds in descending order (for range checking)
        sortedThresholds = sorted(thresholds, reverse=True)

        # Iterate through ALL date/time unit combinations
        if isSporadic:
            self._parent.updateStatusBar("Sporadic background: calculating cumulative counts by time unit")
        else:
            self._parent.updateStatusBar("Calculating cumulative counts by time unit")

        doneItems = 0
        for utcDate, timeUnit in subDf.index:  # Iterate through the MultiIndex
            # Extract start and end hour from timeUnit
            startHour = int(timeUnit[:2])
            endHour = startHour + TUsize

            # Filter shortDf for the current date and time unit RANGE
            dailyShortDf = shortDf[
                (shortDf['utc_date'] == utcDate) &
                (shortDf['utc_time'].str.slice(0, 2).astype(int) >= startHour) &
                (shortDf['utc_time'].str.slice(0, 2).astype(int) < endHour)
                ]

            for _, row in dailyShortDf.iterrows():  # Iterate only on rows filtered by data and current timeUnit
                value = row[valueColumn]

                for i, threshold in enumerate(sortedThresholds):
                    if metric == 'power':
                        colName = f"{threshold:.1f}"
                    else:
                        colName = str(threshold)

                    # Check if the value exceeds the correct range for this threshold
                    if value > threshold:
                        subDf.loc[(utcDate, timeUnit), colName] += 1

            doneItems += 1
            self._parent.updateProgressBar(doneItems, len(subDf.index))

        # Convert counts to integers, handling NaN values (after the loop)
        for col in subDf.columns:
            # subDf[col] = subDf[col].fillna(0).astype(int)
            subDf[col] = subDf[col].mul(radarComp, fill_value=0).astype(int)

        if isSporadic is False:
            subDf = self._timeUnitsToASLindex(subDf)
            if sporadicBackgroundDf is not None:
                rawDf = subDf.copy()
                self._parent.updateStatusBar("Subtracting sporadic background by thresholds")
                # Check sporadicBackgroundDf dimensions
                if len(sporadicBackgroundDf.columns) != (len(thresholds)):
                    raise ValueError("sporadicBackgroundDf columns and thresholds don't match")

                # background subtraction
                doneItems = 0
                for index in subDf.index:
                    timeUnit = subDf.loc[index, 'time unit']
                    for threshold in thresholds:
                        qApp.processEvents()
                        if metric == 'power':
                            colName = f"{threshold:.1f}"  # Format power thresholds with one decimal
                        else:
                            colName = str(threshold)  # Lasting thresholds as integers
                        hourOnly = timeUnit[1]
                        backgroundValue = sporadicBackgroundDf.loc[
                            hourOnly, colName] if hourOnly in sporadicBackgroundDf.index else 0
                        subDf.loc[index, colName] = max(0, subDf.loc[index, colName] - backgroundValue)
                    doneItems += 1
                    self._parent.updateProgressBar(doneItems, len(subDf.index))

        try:
            if not isSporadic:
                subDf = self._completeMIdataframe(subDf, metric, thresholds)
                finalDf = subDf.copy()
                finalDf = self._patchMIdataframe(finalDf)
                finalDf = self._completeMIdataframe(finalDf, metric, thresholds)
                rawDf = self._completeMIdataframe(rawDf, metric, thresholds)

                if sporadicBackgroundDf is not None:
                    retval = finalDf, subDf, rawDf, sporadicBackgroundDf
                else:
                    retval = finalDf, None, None, None

            else:
                # subDf contains sporadic background, no need to calculate MI
                sbDf = subDf
                retval = sbDf, None, None, None

        except Exception as e:
            print(f"Error in _dailyCountsByThresholds: {e}")
            return None

        return retval

    def clearLayout(self, layout):

        if layout is None:
            return

        while layout.count():
            item = layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()  # Elimina il widget
            elif item.layout():
                self.clearLayout(item.layout())  # Ricorsivamente svuota i sub-layout
