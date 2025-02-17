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
import datetime
import json
import numpy as np
from PyQt5.QtWidgets import QDialog, qApp
from .ui_afhashead import Ui_afHasHead
from .logprint import print
from .utilities import splitASCIIdumpFile, splitBinaryDumpFile

import pandas as pd
import cv2


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
        self._percentile = 0
        self._timeDelta = 0
        self._load()
        print("HasHead loaded")

    def _load(self):
        """
        loads this filter's parameters
        from settings file
        """
        self._enabled = self._settings.readSettingAsBool('afHasHeadEnabled')
        self._percentile = self._settings.readSettingAsInt('afHasHeadPercentile')
        self._timeDelta = self._settings.readSettingAsInt('afHasHeadTimeDelta')

        self._ui.chkEnabled.setChecked(self._enabled)
        self._ui.sbPercentile.setValue(self._percentile)
        self._ui.sbTimeDelta.setValue(self._timeDelta)

    def _save(self):
        """
        save ths filter's parameters
        to settings file
        """
        self._settings.writeSetting('afHasHeadEnabled', self._enabled)
        self._settings.writeSetting('afHasHeadPercentile', self._percentile)
        self._settings.writeSetting('afHasHeadTimeDelta', self._timeDelta)

    def _doppler(self, df, referenceFreq, percentile, timeDelta):

        # Sort the DataFrame by time and frequency
        df = df.sort_values(by=['time', 'frequency'])

        # Create a pivot table
        pivotTable = df.pivot_table(index='time', columns='frequency', values='S', aggfunc='first')

        # Round the values to two decimal places
        pivotTable = pivotTable.round(2)

        # Convert the pivot table to a 2D numpy array
        image = pivotTable.to_numpy()

        # Extract the lists of times and frequencies
        timeList = pd.to_datetime(pivotTable.index, unit='s')  # Convert to datetime
        freqList = list(map(float, pivotTable.columns.tolist()))

        # Normalize the image for power values
        minPower = np.min(image)
        maxPower = np.max(image)
        imageNorm = (image - minPower) / (maxPower - minPower)

        # Calculate a dynamic threshold based on the user-provided percentile
        powerThreshold = np.percentile(image, percentile)

        # Find points above the power threshold
        binaryImage = np.uint8(image >= powerThreshold)

        # Find contours in the binary image
        contours, _ = cv2.findContours(binaryImage, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        # Identify the main contour (largest area and near the reference frequency)
        mainContour = None
        doppler = 0
        maxArea = -1
        for contour in contours:
            qApp.processEvents()
            contour = contour.squeeze().astype(np.float32)
            if contour.ndim == 2 and contour.shape[1] == 2:
                freqIndices = contour[:, 0]
                timeIndices = contour[:, 1]
                avgFreq = np.mean([freqList[int(idx)] for idx in freqIndices if int(idx) < len(freqList)])
                area = cv2.contourArea(contour)
                if area > maxArea and abs(avgFreq - referenceFreq) < abs(referenceFreq * 0.1):
                    maxArea = area
                    mainContour = contour

        # Find the maximum power point
        maxPowerIdx = np.unravel_index(np.argmax(image, axis=None), image.shape)
        maxPowerFreq = freqList[maxPowerIdx[1]]
        maxPowerTime = timeList[maxPowerIdx[0]]

        referenceFreq = maxPowerFreq

        # Optimize the search for the extreme point
        extremePoint = None
        extremeFreq = 0.0
        extremeTime = datetime.datetime.now()
        # referenceFreq = 0.0
        maxDistance = -1
        if mainContour is not None:
            # Get coordinates of all points in the main contour
            contourPoints = np.array([[int(pt[0]), int(pt[1])] for pt in mainContour if len(pt) == 2])
            timeIndices = contourPoints[:, 1]
            freqIndices = contourPoints[:, 0]

            # Apply timeDelta constraint
            validIndices = [
                idx for idx, timeIdx in enumerate(timeIndices)
                if (maxPowerTime - timeList[timeIdx]).total_seconds() * 1000 >= timeDelta
            ]

            # Find the extreme point with the maximum frequency
            for idx in validIndices:
                qApp.processEvents()
                freqIdx = freqIndices[idx]
                timeIdx = timeIndices[idx]
                extremeFreq = freqList[freqIdx]
                distance = abs(extremeFreq - referenceFreq)
                # Ensure no points in the contour have a lower or equal Y and a higher frequency
                if (
                        distance > maxDistance
                        and extremeFreq > referenceFreq
                        and all(
                            freqList[freqIndices[otherIdx]] <= extremeFreq
                            for otherIdx in range(len(timeIndices))
                            if timeIndices[otherIdx] == timeIdx and otherIdx != idx
                        )
                        and timeIdx == np.min(timeIndices)  # Ensure it's the lowest Y
                ):
                    maxDistance = distance
                    extremePoint = (timeIdx, freqIdx)

        # Output the extreme point
        if extremePoint is not None:
            timeIdx, freqIdx = extremePoint
            extremeFreq = freqList[freqIdx]
            extremeTime = timeList[timeIdx]
            doppler = referenceFreq - extremeFreq
            print(f"Extreme Point: Frequency = {extremeFreq} Hz, Time = {extremeTime}, Doppler = {doppler} Hz")
        else:
            print("No valid extreme point found, doppler unknown")
            return None

        result = dict()
        result['percentile'] = round(percentile, 0)
        result['tdelta_ms'] = round(timeDelta, 0)
        result['extreme_hz'] = round(extremeFreq, 0)
        result['peak_hz'] = round(maxPowerFreq, 0)
        result['extreme_time'] = str(extremeTime)
        result['peak_time'] = str(maxPowerTime)
        result['freq_shift'] = int(doppler)

        # as result of the hough trasform, this filter must return
        # a JSON string containing the following 5 parameters:
        # freq0,time0 = starting point of the echo head
        # freq1, time1 = ending point of the echo head
        # doppler = frequency shift = (freq0 - freq1)
        return result

    def evalFilter(self, evId: int):
        """
        Calculates the frequency shift of the head echo from a DATB if present.
        The results must be stored by the caller.
        Returns a dictionary containing the positive and negative shifts
        centered on the carrier.
        A None value means that the calculation was impossible
        due to missing data
        """

        # df = self._parent.dataSource.getEventData(evId)
        dfMap = None
        df = self._parent.dataSource.getADpartialFrame(idFrom=evId, idTo=evId, wantFakes=False)
        if len(df) == 0:
            print("this event is fake, ignoring it")
            return None

        rec = df.loc[(df['event_status'] =='Fall')].reset_index(drop=True)
        rtsRevision = rec.loc[0, 'revision']
        datName, datData, dailyNr, utcDate = self._parent.dataSource.extractDumpData(evId)

        if datName is not None and datData is not None:
            if ".datb" in datName:
                dfMap, dfPower = splitBinaryDumpFile(datData)
            else:
                dfMap, dfPower = splitASCIIdumpFile(datData)

        if dfMap is not None:
            cfg = self._parent.dataSource.loadTableConfig('cfg_devices')  # , self._configRevision)
            idx = cfg.loc[(cfg['id'] == rtsRevision)].index[0]
            tune = cfg.loc[idx, 'tune']

            cfg = self._parent.dataSource.loadTableConfig('cfg_waterfall')  # , self._configRevision)
            offset = cfg.loc[idx, 'freq_offset']

            # dfMap is a table time,freq,S
            result = self._doppler(dfMap, tune+offset, self._percentile, self._timeDelta)
            return result

        else:
            self._parent.updateStatusBar(f"dump file for event#{evId} not available, ignoring  it.")
        return None

    def getParameters(self):
        """
        displays the parametrization dialog
        and gets the user's settings
        """
        print("HasHead.getParameters()")
        self._load()
        self.exec()
        self._enabled = self._ui.chkEnabled.isChecked()
        self._percentile = self._ui.sbPercentile.value()
        self._timeDelta = self._ui.sbTimeDelta.value()
        self._save()
        return None

    def isFilterEnabled(self) -> bool:
        return self._enabled
