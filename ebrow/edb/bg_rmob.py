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

import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
import matplotlib.dates as md
from matplotlib.ticker import MultipleLocator
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class BargraphRMOB(BaseGraph):
    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, cmap: list, fullScale: int ):
        BaseGraph.__init__(self, settings, 'RMOB')
        self._series = series
        x = self._series.index
        y = self._series.values

        colors = self._settings.readSettingAsObject('colorDict')
        plt.rc('axes', linewidth=2)

        plt.figure(figsize=(inchWidth, inchHeight))

        self._fig, ax = plt.subplots(1, facecolor='#ffffff')
        ax.set_facecolor('#ffffff')
        xdt = np.asarray(x, dtype='datetime64[s]')
        ax.tick_params(axis='x', which='both', labelrotation=0)
        # generate colorlist referred to maximum monthly value as fullscale, the same of the heatmap
        barColors = list()
        ratio = len(cmap.colors) / fullScale
        for value in y:
            idx = int(value * ratio)
            if idx >= len(cmap.colors):
                idx = len(cmap.colors)-1
            barColors.append(cmap.colors[idx])

        ax.bar(xdt, y, width=1/24, color=barColors, edgecolor="white", align="edge")

        ax.yaxis.set_ticklabels([])
        ax.set_yticks([])
        ax.xaxis.set_major_locator(MultipleLocator(4 / 24))
        myFmt = md.DateFormatter('%Hh')
        ax.xaxis.set_major_formatter(myFmt)
        self._maxVal = self._series.values.max(initial=0)
        ax.annotate(str(  self._maxVal), xy=(-0.1,   self._maxVal - 4.0), xycoords=("axes fraction", "data"))
        ax.axhline(y=  self._maxVal, color=colors['minorGrids'].name(), linestyle='--')
        self._fig.set_tight_layout({"w_pad": 1.0, "h_pad": 0.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')

    def maxVal(self):
        return self._maxVal
