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
from PyQt5.QtWidgets import QHBoxLayout, QScrollArea, QLabel
from .logprint import print

class Plots:

    PLTW_THUMBNAILS = 0
    PLTW_POWER = 1
    PLTW_CMAP = 2
    PLTW_3D = 3
    PLTW_DETAILS = 4

    def __init__(self, parent, ui):
        self._ui = ui
        self._parent = parent
        self._toDailyNr = 0
        self._currentDailyNr = 0
        self._plot = None
        self._dailyNr = 0
        self._utcDate = None
        self._ui.sbID_2.setValue(self._parent.currentID)
        self._ui.sbDaily_2.setValue(self._currentDailyNr)
        self._ui.pbBegin_2.clicked.connect(self._goBegin)
        self._ui.pbEnd_2.clicked.connect(self._goEnd)
        self._ui.pbBack_2.clicked.connect(self._goBack)
        self._ui.pbForward_2.clicked.connect(self._goForward)
        self._ui.twPlots.currentChanged.connect(self.updateTabPlots)

    def updateTabPlots(self):
        self._ui.sbID_2.setValue(self._parent.currentID)
        if self._ui.twMain.currentIndex() == self._parent.TWMAIN_PLOTS:
            if self._ui.twPlots.currentIndex() == self.PLTW_POWER:
                self.showPowerPlots()

    def getCoverage(self):
        (self._parent.fromId, self._parent.toId) = self._parent.dataSource.idCoverage(self._parent.fromDate,
                                                                                      self._parent.toDate)

        self._toDailyNr = self._parent.dataSource.dailyCoverage(self._parent.fromDate)
        self._currentDailyNr = 1
        self._parent.currentID = self._parent.fromId
        self._ui.sbID_2.setMinimum(self._parent.fromId)
        self._ui.sbID_2.setMaximum(self._parent.toId)
        self._ui.sbID_2.setValue(self._parent.currentID)
        self._ui.sbDaily_2.setMinimum(1)
        self._ui.sbDaily_2.setMaximum(self._toDailyNr)
        self._ui.sbDaily_2.setValue(self._currentDailyNr)

    def showPowerPlots(self):
        self._parent.busy(True)
        name, data, self._dailyNr, self._utcDate = self._parent.dataSource.extractDumpData(self._parent.currentID)
        print("_showPlots({}) {}".format(self._parent.currentID, name))

        layout = self._ui.wPowerContainer.layout()
        if layout is None:
            layout = QHBoxLayout()
        else:
            layout.removeWidget(self._plot)

        self._ui.sbDaily_2.setValue(self._dailyNr)
        scroller = QScrollArea()
        if name is not None:

            self._ui.lbDumpFilename.setText(name)



            #TBD...
        else:
            warning = QLabel()
            warning.setText("Nothing to show. The current event ID has no dump file associated.")
            warning.setStyleSheet("color:rgb(0, 255, 0); font: 14pt \"Gauge\"")
            scroller.setWidget(warning)

        layout.addWidget(scroller)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self._ui.wPowerContainer.setLayout(layout)
        self._plot = scroller

        self._parent.busy(False)

    def _goBegin(self):
        """
        browsing screenshots and plots
        @return:
        """
        self._parent.currentID = self._parent.fromId
        self.updateTabPlots()

    def _goEnd(self):
        """
        browsing screenshots and plots
        @return:
        """
        self._parent.currentID = self._parent.toId
        self.updateTabPlots()

    def _goBack(self):
        """
        browsing screenshots and plots
        @return:
        """
        if self._parent.currentID > self._parent.fromId:
            self._parent.currentID -= 1
        self.updateTabPlots()

    def _goForward(self):
        """
        browsing screenshots and plots
        @return:
        """
        if self._parent.currentID < self._parent.toId:
            self._parent.currentID += 1
        self.updateTabPlots()
