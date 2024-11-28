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
import numpy as np
from PyQt5.QtWidgets import QDialog
from skimage.transform import hough_line, hough_line_peaks
from .ui_afhashead import Ui_afHasHead
from .logprint import print
from .utilities import splitASCIIdumpFile, splitBinaryDumpFile



class HasHead(QDialog):
    """
    This filter detects the presence of a head echo.
    It cannot rely on Raise front data uniquely, they
    must be integrated with information taken
    from the related dump file, so this filter
    cannot work if dumps are disabled and evalFilter()
    will return always None in this case.
    """
    def __init__(self, parent, ui, settings):
        QDialog.__init__(self)
        self._parent = parent
        self._ui = Ui_afHasHead()
        self._ui.setupUi(self)
        self._ui.pbOk.setEnabled(True)
        self._ui.pbOk.clicked.connect(self.accept)
        self._settings = settings
        self._enabled = False
        self._load()
        print("HasHead loaded")

    def _load(self):
        """
        loads this filter's parameters
        from settings file
        """
        self._enabled = self._settings.readSettingAsBool('afHasHeadEnabled')
        self._ui.chkEnabled.setChecked(self._enabled)

    def _save(self):
        """
        save ths filter's parameters
        to settings file
        """
        self._settings.writeSetting('afHasHeadEnabled', self._enabled)

    def evalFilter(self, evId: int) -> bool:
        """
        Calculates the frequency shift of the head echo from a DATB if present.
        The results must be stored by the caller.
        Returns a dictionary containing the positive and negative shifts
        centered on the carrier.
        A None value means that the calculation was impossible
        due to missing data
        """

        df = self._parent.dataSource.getEventData(evId)
        datName, datData, dailyNr, utcDate = self._parent.dataSource.extractDumpData(evId)
        if datName is not None and datData is not None:
            if ".datb" in datName:
                dfMap, dfPower = splitBinaryDumpFile(datData)
            else:
                dfMap, dfPower = splitASCIIdumpFile(datData)

        # dfMap is a table time,freq,S

        result = dict()

        result['freq0'] = 0
        result['freq1'] = 0
        result['time0'] = 0
        result['time1'] = 0
        result['doppler'] = 0

        # as result of the hough trasform, this filter must return
        # a JSON string containing the following 5 parameters:
        # freq0,time0 = starting point of the echo head
        # freq1, time1 = ending point of the echo head
        # doppler = frequency shift = (freq0 - freq1)

        return json.dumps(result)


    def getParameters(self):
        """
        displays the parametrization dialog
        and gets the user's settings
        """
        print("HasHead.getParameters()")
        self.exec()
        self._enabled = self._ui.chkEnabled.isChecked()
        self._save()
        return None

    def isFilterEnabled(self) -> bool:
        return self._enabled
