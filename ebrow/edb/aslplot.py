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

import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
import matplotlib.dates as md
from mplcursors import cursor
from matplotlib.ticker import MultipleLocator, MaxNLocator
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from scipy.signal import savgol_filter
from .settings import Settings
from .basegraph import BaseGraph


mp.use('Qt5Agg')


class ASLplot(BaseGraph):
    """
    X-Y graphs having Apparent Solar Longitude on X axis

    """

    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, title: str, yLabel: str,
                 showValues: bool = False, showGrid: bool = False,  smooth: bool = True):
        BaseGraph.__init__(self, settings)

        self._series = series
        x = self._series.index
        y = self._series.values

        windowSize = 17

        if smooth and y.size > windowSize:
            # smoothing by Savitsky-Golay filter
            # is switched off when data aren't enough

            if windowSize > y.size:
                windowSize = y.size

            print("windowsize=", windowSize)

            polyOrder = 5
            if polyOrder >= y.size:
                polyOrder = y.size-1
            print("polyOrder=", polyOrder)
            title += "\nsmoothed through Savitsky-Golay filter,\n (windowSize={}, polyOrder={})".format(windowSize, polyOrder)
            y = savgol_filter(y, windowSize, polyOrder)

        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1)
        xdt = np.asarray(x, dtype='float')
        locator = MaxNLocator(len(x))
        ax.xaxis.set_major_locator(locator)
        #myFmt = md.DateFormatter('%Y-%m-%d')
        #ax.xaxis.set_major_formatter(myFmt)

        ax.plot(xdt, y, label='Counts')
        self._fig.suptitle(title + '\n')
        ax.tick_params(axis='x', which='both', labelrotation=60)

        if showGrid:
            ax.grid(which='major')
            ax.grid(which='minor')

        ax.set_xlabel('Apparent solar longitude [°]')
        ax.set_ylabel(yLabel)

        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')
