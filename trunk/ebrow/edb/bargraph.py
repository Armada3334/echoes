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
from matplotlib.ticker import MultipleLocator, MaxNLocator
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')


class Bargraph(BaseGraph):
    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, title: str, yLabel: str, res: str = 'hour',
                 showValues: bool = False, showGrid: bool = False, subtractBackground = False):
        BaseGraph.__init__(self, settings)
        self._series = series
        x = self._series.index
        y = self._series.values

        # colors = self._settings.readSettingAsObject('colorDict')
        # backColor = colors['background'].name()

        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1)     #, facecolor=backColor)
        # ax.set_facecolor(backColor)

        xdt = np.asarray(x, dtype='datetime64[s]')

        n = len(x)
        if n < 24 or res != 'day':
            locator = MultipleLocator(1)
        else:
            n = 24
            locator = MaxNLocator(n)
        ax.xaxis.set_major_locator(locator)

        myFmt = md.DateFormatter('%Y-%m-%d')
        ax.xaxis.set_major_formatter(myFmt)
        ax.tick_params(axis='x', which='both', labelrotation=60)
        bars = None
        if res == '10m':
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # bars = ax.bar(xdt, y, width=1 / 144, color=colors['counts'].name(), edgecolor=backColor, align="edge")
            bars = ax.bar(xdt, y, width=1 / 144, align="edge", edgecolor="white")
            locator = MultipleLocator(6 / 144)
            # locator = MaxNLocator(24)
            locator.MAXTICKS = 10000
            ax.xaxis.set_minor_locator(locator)
            myFmt = md.DateFormatter('%Hh')
            ax.xaxis.set_minor_formatter(myFmt)

        elif res == 'hour':
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # bars = ax.bar(xdt, y, width=1 / 24, color=colors['counts'].name(), edgecolor="white", align="edge")
            bars = ax.bar(xdt, y, width=1 / 24, edgecolor="white", align="edge")
            # locator = MaxNLocator(24)
            locator = MultipleLocator(6 / 24)
            ax.xaxis.set_minor_locator(locator)
            myFmt = md.DateFormatter('%Hh')
            ax.xaxis.set_minor_formatter(myFmt)

        elif res == 'day':
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # bars = ax.bar(xdt, y, color=colors['counts'].name(), edgecolor="white", align="edge")
            bars = ax.bar(xdt, y, edgecolor="white", align="edge")

        ax.set_xlabel('Covered days')
        ax.set_ylabel(yLabel)

        if showValues and bars is not None:
            # Loop through the bars and add annotations
            for bar in bars:
                height = bar.get_height()
                ax.annotate(f'{height}', xy=(bar.get_x() + bar.get_width() / 2, height), xytext=(0, 3),
                            textcoords="offset points", ha='center', va='bottom')

        if showGrid:
            '''
            ax.grid(color=colors['majorGrids'].name(), linestyle=self._settings.readSettingAsString('majorLineStyle'),
                    linewidth=self._settings.readSettingAsString('majorLineWidth'), which='major')
            ax.grid(color=colors['minorGrids'].name(), linestyle=self._settings.readSettingAsString('minorLineStyle'),
                    linewidth=self._settings.readSettingAsString('minorLineWidth'), which='minor')
            '''
            ax.grid(which='major')
            ax.grid(which='minor')

        # ax.set_title(title + '\n', loc='left')
        self._fig.suptitle(title + '\n')
        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')
