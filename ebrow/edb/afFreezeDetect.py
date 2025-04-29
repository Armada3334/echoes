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
from .utilities import splitASCIIdumpFile, splitBinaryDumpFile, timeToSeconds


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
        self._missedScans = 4
        self._load()
        print("FreezeDetect loaded")

    def _load(self):
        """
        loads this filter's parameters
        from settings file
        """
        self._enabled = self._settings.readSettingAsBool('afFreezeDetectEnabled')
        self._ui.chkEnabled.setChecked(self._enabled)
        self._missedScans = self._settings.readSettingAsInt('afFreezeDetectMissedScans')
        self._ui.sbMultiplier.setValue(self._missedScans)

    def _save(self):
        """
        save ths filter's parameters
        to settings file
        """
        self._settings.writeSetting('afFreezeDetectEnabled', self._enabled)
        self._settings.writeSetting('afFreezeDetectMissedScans', self._missedScans)

    def _findLargestGap(self, data: list) -> dict:
        if len(data) < 2:
            return None

            # Compute the intervals between consecutive values
        intervals = [data[i] - data[i - 1] for i in range(1, len(data))]

        # Compute the average interval
        avgInterval = sum(intervals) / len(intervals)

        # Find the largest gap that exceeds twice the average interval
        largestGap = None
        maxGapSecs = 0

        for i, interval in enumerate(intervals):
            if interval >= self._missedScans * avgInterval and interval > maxGapSecs:
                largestGap = {
                    "start": data[i],  # Initial time of the gap
                    "end": data[i + 1],  # Final time of the gap
                    "secs": interval  # Duration of the gap in seconds
                }
                maxGapSecs = interval

        return largestGap  # Returns the largest gap found, or None if no large gaps exist

    def evalFilter(self, evId: int):

        """
        Calculates the attributes for the given event
        The results must be stored by the caller
        Returns a dictionary with the calculated attributes.
        A None value means that the calculation was impossible
        due to missing data
        """

        edges = self._parent.dataSource.getEventEdges(evId)
        raiseTime = timeToSeconds(edges['raiseTime'])
        fallTime = timeToSeconds(edges['fallTime'])

        datName, datData, dailyNr, utcDate = self._parent.dataSource.extractDumpData(evId)
        if datName is not None:
            if ".datb" in datName:
                dfMap, dfPower = splitBinaryDumpFile(datData)
            else:
                dfMap, dfPower = splitASCIIdumpFile(datData)

            # ignore corrupted data
            if dfPower is not None:
                gap = self._findLargestGap(dfPower['time'])

                if gap:
                    if fallTime >= gap['start'] >= raiseTime or fallTime >= gap['end'] >= raiseTime:
                        # store the gap only if it matches someway with the raise/fall fronts of the
                        # event, that would mean the event is a fake. Otherwise ignores it
                        print(f"ID: {evId} embeds a {gap['secs']} seconds long acquisition hole")
                        return gap
        return None

    def getParameters(self):
        """
        displays the parametrization dialog
        and gets the user's settings
        """
        self._load()
        self.exec()
        self._enabled = self._ui.chkEnabled.isChecked()
        self._missedScans = self._ui.sbMultiplier.value()
        self._save()
        return None

    def isFilterEnabled(self) -> bool:
        """
        the dummy filter can be enabled even
        if it does nothing
        """
        return self._enabled
