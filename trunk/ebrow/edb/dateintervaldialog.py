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

from PyQt5.QtWidgets import QDialog
from .ui_dateintervaldialog import Ui_DateIntervalDialog
from datetime import date


class DateIntervalDialog(QDialog):
    def __init__(self, parent):
        QDialog.__init__(self)
        self._parent = parent
        self._address = None
        self._ui = Ui_DateIntervalDialog()
        self._ui.setupUi(self)
        self._ui.pbOk.setEnabled(True)
        self._ui.pbOk.clicked.connect(self.accept)

    def getInterval(self):
        cDate = self._ui.cwFrom.selectedDate()
        dateBeg = date(cDate.year(), cDate.month(), cDate.day())
        cDate = self._ui.cwTo.selectedDate()
        dateEnd = date(cDate.year(), cDate.month(), cDate.day())
        return dateBeg, dateEnd
