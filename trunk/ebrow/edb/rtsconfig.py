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

import pandas as pd
from .pandasmodel import PandasModel
from .logprint import print


class RTSconfig:
    RTS_DEVICES = 0
    RTS_FFT = 1
    RTS_OUTPUT = 2
    RTS_STORAGE = 3
    RTS_PREFS = 4
    RTS_TOTAL = 5

    def __init__(self, parent, ui, settings):
        self._ui = ui
        self._parent = parent
        self._settings = settings
        self._configRevision = 0
        self._dataSource = self._parent.dataSource
        self._currentDf = None
        self._ui.lbID_2.setText('ID# ' + str(self._parent.currentID))
        self._ui.lbDaily_2.setText('Daily# ' + str(self._parent.currentDailyNr))
        self._ui.lbDay.setText(self._parent.currentDate)
        self._ui.twRTS.currentChanged.connect(self.updateTabRTSconfig)
        self._ui.pbCfgTabExp.clicked.connect(self._exportPressed)

    def updateTabRTSconfig(self):
        self._parent.busy(True)
        self._dataSource = self._parent.dataSource
        self._ui.lbID_2.setText('ID# ' + str(self._parent.currentID))
        self._ui.lbDaily_2.setText('Daily# ' + str(self._parent.currentDailyNr))
        self._ui.lbDay.setText(self._parent.currentDate)
        self._configRevision = self._dataSource.getCfgRevisionFromID(self._parent.currentID)
        self._ui.lbCfgRev.setNum(self._configRevision)
        if self._ui.twMain.currentIndex() == self._parent.TWMAIN_RTS:
            if self._ui.twRTS.currentIndex() == self.RTS_DEVICES:
                self._displayCfgDevices()
            if self._ui.twRTS.currentIndex() == self.RTS_FFT:
                self._displayCfgFFT()
            if self._ui.twRTS.currentIndex() == self.RTS_OUTPUT:
                self._displayCfgOutput()
            if self._ui.twRTS.currentIndex() == self.RTS_STORAGE:
                self._displayCfgStorage()
            if self._ui.twRTS.currentIndex() == self.RTS_PREFS:
                self._displayCfgPrefs()
        self._parent.busy(False)

    def _displayCfgDevices(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_devices') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgDevice.setModel(model)

    def _displayCfgFFT(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_fft') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgFFT.setModel(model)

    def _displayCfgOutput(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_output') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgOutput.setModel(model)

    def _displayCfgFilters(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_filters') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgFilters.setModel(model)

    def _displayCfgStorage(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_storage') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgStorage.setModel(model)

    def _displayCfgPrefs(self):
        self._currentDf = self._dataSource.loadTableConfig('cfg_prefs') # , self._configRevision)
        model = PandasModel(self._currentDf)
        self._ui.tvCfgPrefs.setModel(model)

    def _exportPressed(self, checked):
        self._parent.busy(True)
        # the displayed table is exported as csv
        os.chdir(self._parent.exportDir)
        tableNames = ['Device', 'FFT', 'Output', 'Filters', 'Storage', 'Preferences']
        title = tableNames[self._ui.twRTS.currentIndex()]
        self._currentDf.style.set_caption(title)
        title = title.lower()
        csvName = 'RTS_' + title + '_config.csv'
        self._currentDf.to_csv(csvName, index=True, sep=self._settings.dataSeparator(),
                               decimal=self._settings.decimalPoint())
        self._parent.updateStatusBar("Exported  {}".format(csvName))
        self._ui.lbCfgTabFilename.setText(csvName)
        os.chdir(self._parent.workingDir)
        self._parent.busy(False)

    def getAllTables(self, rev):
        tables = ['cfg_devices', 'cfg_fft', 'cfg_output', 'cfg_storage', 'cfg_prefs']
        rtsDf = list()
        for table in tables:
            df = self._dataSource.loadTableConfig(table, rev)
            rtsDf.append(df)
        return rtsDf
