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
import matplotlib as mp
import matplotlib.pyplot as plt
import matplotlib.dates as md
from dateutil.rrule import SECONDLY, MINUTELY
from mplcursors import cursor
from matplotlib.ticker import MaxNLocator
from matplotlib.dates import AutoDateLocator

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')


class PowerPlot(BaseGraph):
    def __init__(self, df: pd.DataFrame, name: str, settings: Settings, inchWidth: float, inchHeight: float,
                 xLocBaseSecs: int = 5, yMinTicks: int = 10, showGrid: bool = False):
        BaseGraph.__init__(self, settings)

        colors = self._settings.readSettingAsObject('colorDict')
        # backColor = colors['background'].name()
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1) #, facecolor=backColor)
        # ax.set_facecolor(backColor)

        xLocator = AutoDateLocator()
        if xLocBaseSecs > 120:
            xLocBaseMins = xLocBaseSecs / 60
            xLocator.intervald[MINUTELY] = [xLocBaseMins]
        else:
            xLocator.intervald[SECONDLY] = [xLocBaseSecs]

        ax.xaxis.set_major_locator(xLocator)

        yLocator = MaxNLocator(min_n_ticks=yMinTicks)
        ax.yaxis.set_major_locator(yLocator)

        myFmt = md.DateFormatter('%H:%M:%S')
        ax.xaxis.set_major_formatter(myFmt)
        title = "Power profile from data file " + name
        self._min = df['S'].min()
        self._max = df['S'].max()
        ax.plot(df.index, df['N'], color=colors['N'].name(), label='N')
        ax.plot(df.index, df['S'], color=colors['S'].name(), label='S')
        ax.plot(df.index, df['diff'], color=colors['diff'].name(), label='S-N')

        # only for DATB files v.2++
        if 'avgDiff' in df.columns:
            ax.plot(df.index, df['avgDiff'], color=colors['avgDiff'].name(), label='average S-N')
            ax.plot(df.index, df['upThr'], color=colors['upperThr'].name(), label='up threshold')
            ax.plot(df.index, df['dnThr'], color=colors['lowerThr'].name(), label='down threshold')

        # ax.set_title(title + '\n', loc='left')
        self._fig.suptitle(title + '\n')
        ax.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        ax.tick_params(axis='x', which='both', labelrotation=60)
        if showGrid:
            ax.grid(which='major')
        ax.set_xlabel('Time of day')
        ax.set_ylabel('Total power [dBfs]')
        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._df = df

        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')

    def savePlotDataToDisk(self, fileName):
        self._df.to_csv(fileName, sep=self._settings.dataSeparator())

    def getMinMax(self):
        return [self._min, self._max]