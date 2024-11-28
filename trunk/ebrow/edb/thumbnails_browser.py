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

from PyQt5.QtCore import Qt, QSize
from PyQt5.QtGui import QPixmap
from PyQt5.QtWidgets import QGridLayout, QBoxLayout, QLabel, qApp

from .utilities import clearLayout
from .logprint import print


class ThumbnailsBrowser:
    THUMBNAIL_WIDTH = 300
    THUMBNAIL_HEIGHT = 225

    def __init__(self, parentWindow, parentTab, ui):
        self._ui = ui
        self._parent = parentWindow
        self._tab = parentTab
        self._grid = QGridLayout(self._ui.wTnContainer)
        self._grid.setVerticalSpacing(5)
        self._id = self._parent.currentID
        self._currentElem = None
        self._cachedNames = None
        self._cachedThumbs = None
        self._cols = 0
        self._rows = 0
        self._dayShown = self._parent.currentDate
        self._filteredDailies = self._parent.filteredDailies

    def reloadDailyThumbs(self):
        # extracts and keeps in cache all the thumbnails having the same date:
        # when selecting an event in another day, the cache is freed and
        # the thumbnails of that day are loaded

        self._parent.busy(True)
        print("reloadDailyThumbs() currentID=", self._parent.currentID)

        clearLayout(self._grid)
        if len(self._parent.filteredIDs) == 0:
            self._parent.busy(False)
            return

        self._cols = int(self._ui.wTnContainer.width() / self.THUMBNAIL_WIDTH)
        self._rows = len(self._parent.filteredIDs) / self._cols
        row = 0
        col = 0

        tupleList = self._parent.dataSource.extractShotsData(self._parent.filteredIDs)
        if tupleList is not None:
            for tup in tupleList:
                qApp.processEvents()
                tid, name, data, dailyNr, currDate = tup
                pixLabel = QLabel()
                textLabel = QLabel()
                pixLabel.setAlignment(Qt.AlignCenter)
                textLabel.setAlignment(Qt.AlignCenter)
                pix = QPixmap()
                pix.loadFromData(data)
                pix = pix.scaled(QSize(self.THUMBNAIL_WIDTH, self.THUMBNAIL_HEIGHT), Qt.KeepAspectRatio,
                                 Qt.SmoothTransformation)
                pixLabel.setPixmap(pix)
                pixLabel.setToolTip(name)

                caption = "Daily#{}".format(dailyNr)
                textLabel.setText(caption)
                pixLabel.mousePressEvent = lambda e, hid=tid, hDaily=dailyNr, hrow=row, hcol=col: \
                    self._onThumbnailClick(e, hid, hDaily, hrow, hcol)
                textLabel.mousePressEvent = pixLabel.mousePressEvent
                thumbnail = QBoxLayout(QBoxLayout.TopToBottom)
                thumbnail.addWidget(pixLabel)
                thumbnail.addWidget(textLabel)
                self._grid.addLayout(thumbnail, row, col, Qt.AlignCenter)

                # print("inserted thumbnail {} at position row={} col={}".format(name, row, col))
                col += 1
                if col >= self._cols:
                    col = 0
                    row += 1
            self._ui.wTnContainer.setLayout(self._grid)

        self.selectID(self._parent.currentID)
        print("Showed {} thumbnails".format(self._grid.count()))
        self._parent.busy(False)

    def _onThumbnailClick(self, event, hid: int, hDaily: int, hrow: int, hcol: int):
        print("_onThumbnailClick() hid={}, hDaily={}, hrow={}, hcol={}".format(hid, hDaily, hrow, hcol))
        # Deselect all thumbnails in the image selector
        items = range(self._grid.count())
        for itemIndex in items:
            pixLabelItem = self._grid.itemAt(itemIndex)
            layout = pixLabelItem.layout()
            if layout is not None:
                textLabel = layout.itemAt(1).widget()
                textLabel.setStyleSheet("background-color: none;")

        # Select the single clicked thumbnail
        pixLabelItem = self._grid.itemAtPosition(hrow, hcol)
        layout = pixLabelItem.layout()
        if layout is not None:
            pixLabel = layout.itemAt(0).widget()
            print("_onThumbnailClick() selecting {}".format(pixLabel.toolTip()))
            self._ui.lbShotFilename.setText(pixLabel.toolTip())
            textLabel = layout.itemAt(1).widget()
            textLabel.setStyleSheet("background-color: blue;")
            try:
                idx = self._parent.filteredDailies.index(hDaily)
            except (ValueError, IndexError):
                idx = 0
                if self._tab is not None:
                    self._tab.refresh()

            self.selectID(self._parent.filteredIDs[idx])

    def selectID(self, selId: int):
        print("selectID: ", selId)
        self._id = selId

        if selId == 0:
            self._ui.lbShotFilename.setText('NONE')
            self._ui.lbTime.setText('NONE')
            self._ui.lbLasting.setText("0 s")
            self._ui.lbClass.setText('NONE')
            self._ui.lbID.setText('Nothing selected')
            self._ui.lbDaily.setText('------')
            self._ui.lbThisDay.setText('------')

        elif selId in self._parent.filteredIDs:
            # the required ID has already been displayed
            self._parent.currentIndex = self._parent.filteredIDs.index(selId)
            self._parent.currentDailyNr = self._parent.filteredDailies[self._parent.currentIndex]
            self._parent.currentID = selId
            self._ui.lbID.setText('ID# ' + str(self._parent.currentID))
            self._ui.lbThisDay.setText(self._parent.currentDate)
            # Deselect all thumbnails in the image selector
            items = range(self._grid.count())
            for textLabelIndex in items:
                pixLabelItem = self._grid.itemAt(textLabelIndex)
                layout = pixLabelItem.layout()
                if layout is not None:
                    # pixLabel = layout.itemAt(0).widget()
                    # print("_onThumbnailClick() deselecting {}".format(pixLabel.toolTip()))
                    textLabel = layout.itemAt(1).widget()
                    textLabel.setStyleSheet("background-color: none;")
            # Select the thumbnail at given index
            pixLabelItem = self._grid.itemAt(self._parent.currentIndex)
            if pixLabelItem is not None:
                layout = pixLabelItem.layout()
                if layout is not None:
                    pixLabel = layout.itemAt(0).widget()
                    print("selectID() selecting {}".format(pixLabel.toolTip()))
                    df = self._parent.dataSource.getEventData(self._parent.currentID)
                    self._ui.lbShotFilename.setText(pixLabel.toolTip())
                    self._ui.lbTime.setText(df.iloc[0, 0])  # utc_time,RAISE
                    self._ui.lbLasting.setText(str(df.iloc[10, 2] / 1000) + " s")  # lasting_ms,FALL
                    self._ui.lbClass.setText(df.iloc[20, 2])  # classification,FALL
                    self._ui.lbDaily.setText('Daily# ' + str(self._parent.currentDailyNr))
                    textLabel = layout.itemAt(1).widget()
                    textLabel.setStyleSheet("background-color: blue;")
                    qApp.processEvents()
                    self._ui.saTn.ensureWidgetVisible(textLabel)
        if self._dayShown != self._parent.currentDate:
            self._dayShown = self._parent.currentDate
            print("selectID() day changed, refresh needed")
            return True
        if  self._filteredDailies != self._parent.filteredDailies:
            self._filteredDailies = self._parent.filteredDailies
            print("selectID() filters changed, refresh needed")
            return True
        return False
