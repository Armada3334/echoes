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
from PyQt5.QtCore import QDate
from matplotlib.colors import ListedColormap
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')


class HeatmapRMOB(BaseGraph):
    def __init__(self, dataFrame: pd.DataFrame, settings: Settings, inchWidth: float, inchHeight: float, cmap: ListedColormap):
        BaseGraph.__init__(self, settings, 'RMOB')

        colors = self._settings.readSettingAsObject('colorDict')

        if len(dataFrame.columns) == 24:
            # 24 columns, one for each hour count from 00h to 23h
            df = dataFrame
            days = len(df.index)
            # calculate full-scale value (red color) taking the maximum in all the counts columns
            countsOnlyDf = df.iloc[0:, 0:]
            self._fullScale = countsOnlyDf.max(axis=1).max()

            # rows header (hours)
            rowsHdr = list()
            # rowsHdr.append("Day / Hour")
            for hour in range(0, 24):
                s = "{:02d}h".format(hour)
                rowsHdr.append(s)

            # columns header (days)
            allColumns = list()
            colsHdr = list()
            for dayCount in range(0, days):
                isoDate = str(df.index[dayCount])
                if len(isoDate.split('-')) == 3:
                    date = QDate.fromString(isoDate, "yyyy-MM-dd")
                    s = date.day()
                else:
                    s = isoDate
                colsHdr.append(s)

                # days columns values
                column = list()
                for hour in range(0, 24):
                    value = int(df.iloc[dayCount, hour])
                    column.append(value)
                allColumns.append(column)
        else:
            return

        colsHdr = ['01', ' ', ' ', ' ', ' D', ' ', ' a', ' ', ' y', ' ', ' s', ' ', ' ', ' ', '15', ' ', ' ', ' ',
                   ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '  ', '  ', '31']
        rowsHdr = ['00h', '   ', 'UT ', '   ', '   ', '   ',
                   '06h', '   ', '   ', '   ', '   ', '   ',
                   '12h', '   ', '   ', '   ', '   ', '   ',
                   '18h', '   ', '   ', '   ', '   ', '   ', ]

        data = np.array(allColumns)
        data = np.transpose(data)

        cmap.set_bad(color='black')
        data = data.astype(float)
        data[data == -1] = np.nan
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1, facecolor='#ffffff')
        ax.set_facecolor('#ffffff')

        im = ax.imshow(data, cmap=cmap, aspect='auto')

        # Show all ticks and label them with the respective list entries
        ax.spines[:].set_visible(False)
        xTicks = len(colsHdr)
        yTicks = len(rowsHdr)
        ax.set_xticks(np.arange(xTicks), labels=colsHdr)  # , minor=True)
        ax.set_yticks(np.arange(yTicks), labels=rowsHdr)  # , minor=True)
        # ax.grid(which="minor", color="w", linestyle='-', linewidth=2)
        # ax.tick_params(which="both", bottom=False, left=False, labelrotation=0)
        ax.tick_params(which="both", labelrotation=0, width=2)
        # ax.xaxis.set_ticks_position('bottom')
        ax.xaxis.set_ticks_position('top')
        ax.yaxis.set_ticks_position('left')
        # ax.xaxis.set_label_position('top')
        plt.setp(ax.get_xticklabels(), ha="center")
        plt.hlines(y=np.arange(0, yTicks) + 0.5, xmin=np.full(yTicks, 0) - 0.5,
                   xmax=np.full(yTicks, xTicks) - 0.5, color="white")
        plt.vlines(x=np.arange(0, xTicks) + 0.5, ymin=np.full(xTicks, 0) - 0.5,
                   ymax=np.full(xTicks, yTicks) - 0.5, color="white")

        valFmt = mp.ticker.StrMethodFormatter('{x:.0f}')
        norm = mp.colors.Normalize(vmin=0, vmax=int(self._fullScale))
        self._fig.colorbar(im, drawedges=False, norm=norm, cmap=cmap)
        self._fig.set_tight_layout({"pad": 0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')
