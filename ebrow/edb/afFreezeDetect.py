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

import json
from PyQt5.QtWidgets import QDialog
from .ui_affreezedetect import Ui_afFreezeDetect
from .logprint import print


class FreezeDetect(QDialog):
    """
    This filter checks for dump files having "holes" on time axis. that means
    Echoes acquisition has frozen while recording, producing a fake overdense event.
    When such holes are detected, the filter returns the number of holes and the
    ms missed in each of them.
    The caller should mark the event as FAKE LONG
    """

    def __init__(self, parent, ui, settings):
        QDialog.__init__(self)
        self._parent = parent
        self._ui = Ui_afFreezeDetect()
        self._ui.setupUi(self)
        self._ui.pbOk.setEnabled(True)
        self._ui.pbOk.clicked.connect(self.accept)
        self._settings = settings
        self._enabled = False
        self._load()
        print("FreezeDetect loaded")

    def _load(self):
        """
        loads this filter's parameters
        from settings file
        """
        self._enabled = self._settings.readSettingAsBool('afFreezeDetectEnabled')
        self._ui.chkEnabled.setChecked(self._enabled)

    def _save(self):
        """
        save ths filter's parameters
        to settings file
        """
        self._settings.writeSetting('afFreezeDetectEnabled', self._enabled)

    def evalFilter(self, evId: int) -> dict:
        """
        Calculates the attributes for the given event
        The results must be stored by the caller
        Returns a dictionary with the calculated attributes.
        A None value means that the calculation was impossible
        due to missing data
        """
        datName, datData, dailyNr, utcDate = self._parent.dataSource.extractDumpData(evId)

        result = dict()
        result['none'] = 0
        return result

    def getParameters(self):
        """
        displays the parametrization dialog
        and gets the user's settings
        """
        self._load()
        self.exec()
        self._enabled = self._ui.chkEnabled.isChecked()
        self._save()
        return None

    def isFilterEnabled(self) -> bool:
        """
        the dummy filter can be enabled even
        if it does nothing
        """
        return self._enabled
