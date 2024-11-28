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
from datetime import datetime, timezone
from dateutil.rrule import SECONDLY, MINUTELY

import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
from matplotlib.dates import AutoDateLocator, date2num, MICROSECONDLY, DateFormatter
from mplcursors import cursor
from matplotlib.ticker import MaxNLocator, ScalarFormatter
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .utilities import PrecisionDateFormatter
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class MapPlot(BaseGraph):
    def __init__(self, dfMap: pd.DataFrame, dfPower: pd.DataFrame, settings: Settings, inchWidth: float,
                 inchHeight: float, cmap: list,
                 name: str, vmin: float, vmax: float, tickEveryHz: int = 1000, tickEverySecs: int = 1,
                 showGrid: bool = True):
        BaseGraph.__init__(self, settings)

        # plt.rcParams['axes.autolimit_mode'] = 'round_numbers'

        # dfMap.to_csv('C:/temp/map.csv', sep=';')
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
        # backColor = colors['background'].name()
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1) #, facecolor=backColor)
        # ax.set_facecolor(backColor)

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
        # ax.set_title(title, loc='left', pad=20)
        self._fig.suptitle(title + '\n')
        ax.tick_params(axis='x', which='both', labelrotation=90, color=colors['majorGrids'].name())

        if showGrid:
            ax.grid(which='major', axis='both', color=colors['majorGrids'].name())

        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._df = dfMap
        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')

    def savePlotDataToDisk(self, fileName):
        self._df = self._df.set_index('time')
        self._df.to_csv(fileName, sep=self._settings.dataSeparator())


    def getMinMax(self):
        return [self._min, self._max]
