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
import matplotlib.colors as mcolors
from PyQt5.QtWidgets import qApp
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class MapPlot(BaseGraph):
    def __init__(self, dfMap: pd.DataFrame, dfPower: pd.DataFrame, settings: Settings, inchWidth: float,
                 inchHeight: float, cmap: list, name: str, vmin: float, vmax: float,
                 tickEveryHz: int = 1000, tickEverySecs: int = 1, showGrid: bool = True, showContour: bool = False, attrDict: dict = None):
        BaseGraph.__init__(self, settings)

        self._df = None
        self._attrDict = attrDict
        self._cmap = cmap
        self._df = dfMap

        dfMap = dfMap.reset_index()

        # --- horizontal x axis [Hz] ----
        # FFT bins
        freqs = dfMap['frequency'].unique()
        totFreqs = len(freqs)
        self._xLims = [freqs[0], freqs[-1]]
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
        self._yLims = date2num([startTime, endTime])

        yLoc = AutoDateLocator(interval_multiples=True)
        if tickEverySecs > 120.0:
            tickEveryMins = tickEverySecs / 60
            yLoc.intervald[MINUTELY] = [tickEveryMins]
        elif tickEverySecs < 1.0:
            tickEveryUs = tickEverySecs * 1E6
            yLoc.intervald[MICROSECONDLY] = [tickEveryUs]
        else:
            yLoc.intervald[SECONDLY] = [tickEverySecs]

        # note: MICROSECONDLY needs matplotlib 3.6.0++ and Python 3.8++

        # yFmt = PrecisionDateFormatter('%H:%M:%S.%f', tz=timezone(timedelta(0)))
        yFmt = DateFormatter('%H:%M:%S.%f')

        # ---- the waterfall flows downwards so the time increase from bottom to top (origin lower)
        data = dfMap[['S']].to_numpy().reshape(totScans, totFreqs)
        self._min, self._max = data.min(), data.max()
        np.clip(data, vmin, vmax, data)

        colors = self._settings.readSettingAsObject('colorDict')
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, self._ax = plt.subplots(1)

        im = self._ax.imshow(data, cmap=cmap, aspect='auto', vmin=vmin, vmax=vmax, interpolation=None,
                       origin='lower', extent=[self._xLims[0], self._xLims[1], self._yLims[0], self._yLims[1]])
        print("extent=", im.get_extent())

        self._ax.xaxis.set_major_locator(xLoc)
        self._ax.xaxis.set_major_formatter(xFmt)
        self._ax.set_xlabel('frequency [Hz]', labelpad=30)

        self._ax.yaxis.set_major_locator(yLoc)
        self._ax.yaxis.set_major_formatter(yFmt)
        self._ax.set_ylabel('time of day', labelpad=30)

        df = self._df.sort_values(by=['time', 'frequency'])
        pivotTable = df.pivot_table(index='time', columns='frequency', values='S', aggfunc='first')
        self._image = pivotTable.to_numpy()

        if showContour and attrDict and len(attrDict.keys()) > 0:
            self._plotContourOverlay()
            self._plotMaxPointOverlay()
            self._plotExtremePointOverlay()

        norm = mp.colors.Normalize(vmin=vmin, vmax=vmax)
        self._fig.colorbar(im, drawedges=False, norm=norm, cmap=cmap)
        title = "Mapped spectrogram from data file " + name
        self._fig.suptitle(title + '\n')
        self._ax.tick_params(axis='x', which='both', labelrotation=90, color=colors['majorGrids'].name())

        if showGrid:
            self._ax.grid(which='major', axis='both', color=colors['majorGrids'].name())

        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._fig.set_tight_layout({"pad": 5.0})
        self._fig.canvas.draw()

        self._canvas = FigureCanvasQTAgg(self._fig)

        # avoids showing the original fig window
        plt.close('all')

    def _plotContourOverlay(self):
        """ Crea un overlay trasparente per i contorni. """
        percentile = int(self._attrDict['percentile'])
        referenceFreq = int(self._attrDict['peak_hz'])

        # --- Percentile threshold ---
        powerThreshold = np.percentile(self._image, percentile)
        binaryImage = np.uint8(self._image >= powerThreshold)

        # --- find the contour ---
        contours, _ = cv2.findContours(binaryImage, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        mainContour = max(contours, key=cv2.contourArea, default=None)

        if mainContour is not None:
            mainContour = mainContour.squeeze()
            freqIndices = mainContour[:, 0]
            timeIndices = mainContour[:, 1]

            # Normalize and scaling
            numRows, numCols = self._image.shape
            contourFreqs = self._xLims[0] + (freqIndices / (numCols - 1)) * ( self._xLims[1] - self._xLims[0])
            contourTimes = self._yLims[0] + (timeIndices / (numRows - 1)) * ( self._yLims[1] - self._yLims[0])

            # --- Draw the contour ---
            self._ax.plot(contourFreqs, contourTimes, color='green', linewidth=2, alpha=0.5)
            self._fig.canvas.draw()

    def _plotMaxPointOverlay(self):
        """ Crea un overlay trasparente per il maxPoint. """
        try:
            dtsPeak = self._attrDict['peak_time'][:23]
            dtPeak = datetime.strptime(dtsPeak, "%Y-%m-%d %H:%M:%S.%f")
            tsPeak = (dtPeak - dtPeak.replace(hour=0, minute=0, second=0, microsecond=0)).total_seconds()
        except ValueError:
            return False

        maxPoint = (self._attrDict['peak_hz'], tsPeak)

        if maxPoint:
            maxFreq, maxTime = maxPoint

            # Normalize and scaling
            dtPeakNum = date2num(dtPeak)

            self._ax.plot(maxFreq, dtPeakNum, 'ro', color='white', markersize=8, label='Max Power Point', alpha=0.5)
            self._fig.canvas.draw()

    def _plotExtremePointOverlay(self):
        """ Crea un overlay trasparente per l'extremePoint. """
        try:
            dtsExtreme = self._attrDict['extreme_time'][:23]
            dtExtreme = datetime.strptime(dtsExtreme, "%Y-%m-%d %H:%M:%S.%f")
            tsExtreme = (dtExtreme - dtExtreme.replace(hour=0, minute=0, second=0, microsecond=0)).total_seconds()
        except ValueError:
            return False

        extremePoint = (self._attrDict['extreme_hz'], tsExtreme)

        if extremePoint:
            extremeFreq, extremeTime = extremePoint
            dtExtremeNum = date2num(dtExtreme)
            self._ax.plot(extremeFreq, dtExtremeNum, 'x', color='orange', markersize=10, label='Extreme Point', alpha=0.5)
            self._fig.canvas.draw()
        return True

    def savePlotDataToDisk(self, fileName):
        df = self._df.set_index('time')
        df.to_csv(fileName, sep=self._settings.dataSeparator())

    def getMinMax(self):
        return [self._min, self._max]
