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
# from mplcursors import cursor don't work in 3d
from matplotlib.ticker import MaxNLocator, ScalarFormatter
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .utilities import PrecisionDateFormatter
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class MapPlot3D(BaseGraph):
    def __init__(self, dfMap: pd.DataFrame, dfPower: pd.DataFrame, settings: Settings, inchWidth: float,
                 inchHeight: float, cmap: object,
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

        # TODO: MICROSECONDLY needs matplotlib 3.6.0++ and Python 3.8++
        # yFmt = PrecisionDateFormatter('%H:%M:%S.%f', tz=timezone(timedelta(0)))
        yFmt = DateFormatter('%H:%M:%S.%f')

        data = dfMap[['S']].to_numpy().reshape(totScans, totFreqs)
        self._min = data.min()
        self._max = data.max()
        np.clip(data, vmin, vmax, data)
        data[np.isnan(data)] = vmin

        colors = self._settings.readSettingAsObject('colorDict')
        # backColor = colors['background'].name()
        self._fig = plt.figure(figsize=(inchWidth, inchHeight)) #, facecolor=backColor)
        self._ax3d = plt.axes(projection='3d')

        # for cstride/rstride takes totFreqs/totScans cube root
        cstride = int(totFreqs ** (1. / 3))
        rstride = int(totScans ** (1. / 3))
        if cstride < rstride:
            rstride = cstride
        else:
            cstride = rstride

        print("frequency bins (X axis)={}, total scans (Y axis)={}, min/max power (Z axis)={}..{}".format(totFreqs,
                                                                                                          totScans,
                                                                                                          self._min,
                                                                                                          self._max))
        x = np.linspace(xLims[0], xLims[1], totFreqs)
        y = np.linspace(yLims[0], yLims[1], totScans)
        X, Y = np.meshgrid(x, y)

        im = self._ax3d.plot_surface(X, Y, data, rstride=rstride, cstride=cstride, alpha=0.5, linewidth=0, vmin=vmin,
                                     vmax=vmax, antialiased=False, cmap=cmap)

        self._ax3d.xaxis.set_major_locator(xLoc)
        self._ax3d.xaxis.set_major_formatter(xFmt)
        self._ax3d.set_xlabel('frequency [Hz]', labelpad=20)
        self._ax3d.tick_params(axis='x', which='both', labelrotation=90)

        self._ax3d.yaxis.set_major_locator(yLoc)
        self._ax3d.yaxis.set_major_formatter(yFmt)
        self._ax3d.set_ylabel('time of day', labelpad=20)
        self._ax3d.yaxis_date()
        self._ax3d.tick_params(axis='y', which='both', labelrotation=90)

        # locator and formatter not needed for Z axis
        self._ax3d.set_zlabel('power [dBfs]', labelpad=10)
        self._ax3d.tick_params(axis='z', which='both', labelrotation=0, pad=5)

        norm = mp.colors.Normalize(vmin=vmin, vmax=vmax)
        self._fig.colorbar(im, drawedges=False, norm=norm, cmap=cmap)

        title = "3D mapped spectrogram from data file " + name
        # self._ax3d.set_title(title, loc='left', pad=20)
        self._fig.suptitle(title + '\n')

        # ax.set_title(title + '\n', loc='left')
        self._ax3d.set(xlim=xLims, ylim=yLims, zlim=(vmin, vmax))
        self._ax3d.grid(visible=showGrid, color=colors['majorGrids'].name())
        self._df = dfMap

        # cursor(hover=True) don't work with 3d
        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)

        # avoids showing the original fig window
        plt.close('all')

    def rotate(self, azimuth: int, elevation: int):
        # no rolling
        self._ax3d.view_init(elevation, azimuth)
        self._canvas.draw()
        # plt.pause(0.01)

    def savePlotDataToDisk(self, fileName):
        self._df = self._df.set_index('time')
        self._df.to_csv(fileName, sep=self._settings.dataSeparator())

    def getMinMax(self):
        return [self._min, self._max]

    def getAzimuth(self):
        return self._ax3d.azim

    def getElevation(self):
        return self._ax3d.elev
