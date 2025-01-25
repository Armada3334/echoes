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
from datetime import datetime, timezone
from dateutil.rrule import SECONDLY, MINUTELY

import cv2
import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
from matplotlib.dates import AutoDateLocator, date2num, MICROSECONDLY, DateFormatter
from mplcursors import cursor
from matplotlib.ticker import MaxNLocator, ScalarFormatter
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from PyQt5.QtWidgets import qApp
from .utilities import PrecisionDateFormatter
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class MapPlot(BaseGraph):
    def __init__(self, dfMap: pd.DataFrame, dfPower: pd.DataFrame, settings: Settings, inchWidth: float,
                 inchHeight: float, cmap: list, name: str, vmin: float, vmax: float, 
                 tickEveryHz: int = 1000, tickEverySecs: int = 1, showGrid: bool = True, attrDict: dict = None):
        BaseGraph.__init__(self, settings)

        self._df = None
        dfMap = dfMap.reset_index()

        # --- horizontal x axis [Hz] ----
        # FFT bins
        freqs = dfMap['frequency'].unique()
        totFreqs = len(freqs)
        xLims = [freqs[0], freqs[-1]]
        freqSpan = dfMap['frequency'].max() - dfMap['frequency'].min()

        nTicks = (freqSpan / tickEveryHz) - 1
        xLoc = MaxNLocator(nTicks, steps=[1, 2, 5], min_n_ticks=nTicks)
        xFmt = ScalarFormatter()

        # --- vertical Y axis [sec] ----

        # data scans
        scans = dfPower.index.unique().to_list()
        totScans = len(scans)
        dt = datetime.fromtimestamp(scans[0], tz=timezone.utc)
        startTime = np.datetime64(dt)
        dt = datetime.fromtimestamp(scans[-1], tz=timezone.utc)
        endTime = np.datetime64(dt)
        yLims = date2num([startTime, endTime])

        yLoc = AutoDateLocator(interval_multiples=True)
        if tickEverySecs > 120.0:
            tickEveryMins = tickEverySecs / 60
            yLoc.intervald[MINUTELY] = [tickEveryMins]
        elif tickEverySecs < 1.0:
            tickEveryUs = tickEverySecs * 1E6
            # yLoc.intervald[7] = [tickEveryUs]
            yLoc.intervald[MICROSECONDLY] = [tickEveryUs]
        else:
            yLoc.intervald[SECONDLY] = [tickEverySecs]

        # note: MICROSECONDLY needs matplotlib 3.6.0++ and Python 3.8++

        # yFmt = PrecisionDateFormatter('%H:%M:%S.%f', tz=timezone(timedelta(0)))
        yFmt = DateFormatter('%H:%M:%S.%f')

        # ---- the waterfall flows downwards so the time increase from bottom to top (origin lower)
        data = dfMap[['S']].to_numpy().reshape(totScans, totFreqs)
        self._min = data.min()
        self._max = data.max()
        np.clip(data, vmin, vmax, data)

        colors = self._settings.readSettingAsObject('colorDict')
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1)
        
        im = ax.imshow(data, cmap=cmap, aspect='auto', vmin=vmin, vmax=vmax, interpolation=None,
                       origin='lower', extent=[xLims[0], xLims[1], yLims[0], yLims[1]])
        print("extent=", im.get_extent())

        ax.xaxis.set_major_locator(xLoc)
        ax.xaxis.set_major_formatter(xFmt)
        ax.set_xlabel('frequency [Hz]', labelpad=30)

        ax.yaxis.set_major_locator(yLoc)
        ax.yaxis.set_major_formatter(yFmt)
        ax.set_ylabel('time of day', labelpad=30)

        norm = mp.colors.Normalize(vmin=vmin, vmax=vmax)
        self._fig.colorbar(im, drawedges=False, norm=norm, cmap=cmap)
        title = "Mapped spectrogram from data file " + name
        self._fig.suptitle(title + '\n')
        ax.tick_params(axis='x', which='both', labelrotation=90, color=colors['majorGrids'].name())

        if showGrid:
            ax.grid(which='major', axis='both', color=colors['majorGrids'].name())

        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        self._cmap = cmap
        self._df = dfMap
        self._attrDict = attrDict

        if attrDict:
            self._plotExtras()

        # avoids showing the original fig window
        plt.close('all')

    def _plotExtras(self):
        """
        Adds a plot overlay with the contour, max power point, and extreme point.
        Adjusts colors to avoid conflict with the colormap.

        """
        # Default colors
        contourColor = 'green'
        maxPointColor = 'red'
        extremePointColor = 'orange'

        # Check for conflicts with colormap colors
        if self._cmap:
            availableColors = ['blue', 'magenta', 'cyan', 'yellow', 'black', 'white']
            for color in [contourColor, maxPointColor, extremePointColor]:
                if color in self._cmap:
                    newColor = next((c for c in availableColors if c not in self._cmap), None)
                    if newColor:
                        if color == contourColor:
                            contourColor = newColor
                        elif color == maxPointColor:
                            maxPointColor = newColor
                        elif color == extremePointColor:
                            extremePointColor = newColor

        ax = self._fig.axes[0]

        # Plot contour
        extremePoint = (int(self._attrDict['freq0']), int(self._attrDict['time0']))
        maxPoint = (int(self._attrDict['freq1']), int(self._attrDict['time1']))
        percentile = int(self._attrDict['perc'])
        referenceFreq = int(self._attrDict['freq1'])

        # Sort the DataFrame by time and frequency
        df = self._df.sort_values(by=['time', 'frequency'])

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
        # imageNorm = (image - minPower) / (maxPower - minPower)

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
        if mainContour is not None:
            # Get coordinates of all points in the main contour
            contourPoints = np.array([[int(pt[0]), int(pt[1])] for pt in mainContour if len(pt) == 2])
            timeIndices = contourPoints[:, 1]
            freqIndices = contourPoints[:, 0]
            ax.plot(freqIndices, timeIndices, color=contourColor, label='Contour', linewidth=2)

        # Plot maximum power point
        if maxPoint:
            maxFreq, maxTime = maxPoint
            ax.plot(maxFreq, maxTime, 'o', color=maxPointColor, markersize=8, label='Max Power Point')

        # Plot extreme point
        if extremePoint:
            extremeFreq, extremeTime = extremePoint
            ax.plot(extremeFreq, extremeTime, 'x', color=extremePointColor, markersize=10, label='Extreme Point')

        # Update the legend to include new elements
        ax.legend(loc='upper left')

    def savePlotDataToDisk(self, fileName):
        df = self._df.set_index('time')
        df.to_csv(fileName, sep=self._settings.dataSeparator())

    def getMinMax(self):
        return [self._min, self._max]
