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

import json
from PyQt5.QtWidgets import QDialog
from .ui_afdummy import Ui_afDummy
from .logprint import print


class Dummy(QDialog):
    """
    This is a prototype attribute filter and also serves as a base class for other attribute filters.
    An attribute is information relating to an event that is calculated on the basis of information present in the database
    or in external files. An attribute filter is a python class that can be added in the attributes folder.
    It has a graphical interface for configuring its parameters, a method for calculating the attributes and a third method
    that indicates whether the filter is valid, i.e. it was possible to calculate the attributes and the fourth returns
    the value of the attributes.

    """

    def __init__(self, parent, ui, settings):
        QDialog.__init__(self)
        self._parent = parent
        self._ui = Ui_afDummy()
        self._ui.setupUi(self)
        self._ui.pbOk.setEnabled(True)
        self._ui.pbOk.clicked.connect(self.accept)
        self._settings = settings
        self._enabled = False
        self._load()
        print("Dummy loaded")

    def _load(self):
        """
        loads this filter's parameters
        from settings file
        """
        self._enabled = self._settings.readSettingAsBool('afDummyEnabled')
        self._ui.chkEnabled.setChecked(self._enabled)

    def _save(self):
        """
        save ths filter's parameters
        to settings file
        """
        self._settings.writeSetting('afDummyEnabled', self._enabled)

    def evalFilter(self, evId: int) -> dict:
        """
        Calculates the attributes for the given event
        The results must be stored by the caller
        Returns a dictionary with the calculated attributes.
        A None value means that the calculation was impossible
        due to missing data
        """
        df = self._parent.dataSource.getEventData(evId)

        result = dict()

        result['none'] = 0
        return json.dumps(result)

    def getParameters(self):
        """
        displays the parametrization dialog
        and gets the user's settings
        """
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
