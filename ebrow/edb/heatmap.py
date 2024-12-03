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
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from mplcursors import cursor

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')


class Heatmap(BaseGraph):
    def __init__(self, dataFrame: pd.DataFrame, settings: Settings, inchWidth: float, inchHeight: float, cmap: list,
                 title: str, res: str = 'hour', showValues: bool = False, showGrid: bool = False, subtractBackground: bool = False):
        BaseGraph.__init__(self, settings)

        xlabel = "Covered days"
        ylabel = "Hours of a day"
        # colors = self._settings.readSettingAsObject('colorDict')

        xTicks = 0
        yTicks = 0

        if res == "hour":  # len(dataFrame.columns) == 24:
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # 24 columns, one for each hour count from 00h to 23h
            df = dataFrame
            days = len(df.index)
            # calculate full-scale value (red color) taking the maximum in all the counts columns
            countsOnlyDf = df.iloc[0:, 0:]
            self._fullScale = countsOnlyDf.max(axis=1).max()

            # rows header (hours)
            rowsHdr = list()
            # rowsHdr.append("Day / Hour")
            yTicks = 24
            for hour in range(0, 24):
                s = "{:02d}h".format(hour)
                rowsHdr.append(s)

            # columns header (days)
            allColumns = list()
            colsHdr = list()
            xTicks = days
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
                    if df.iloc[dayCount, hour] is None or np.isnan(df.iloc[dayCount, hour]):
                        df.iloc[dayCount, hour] = 0
                    value = int(df.iloc[dayCount, hour])

                    column.append(value)
                allColumns.append(column)

        elif res == '10m':  # len(dataFrame.columns) == 144:
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # 144 columns, one for each 10min count from 00h.00m to 23h50m
            df = dataFrame
            days = len(df.index)

            # calculate full-scale value (red color) taking the maximum in all the counts columns
            # (the date column must be excluded for a correct calculation)

            countsOnlyDf = df.iloc[0:, 0:]
            self._fullScale = countsOnlyDf.max(axis=1).max()
            # rows header (hours)
            rowsHdr = list()
            # rows header (10minute intervals)
            yTicks = 144
            interval = 0
            for hour in range(0, 24):
                for minute in range(0, 60, 10):
                    s = "{:02d}h {:02d}m".format(hour, minute)
                    interval += 1
                    rowsHdr.append(s)
            allColumns = list()

            # columns header (days)
            colsHdr = list()
            xTicks = days
            for dayCount in range(0, days):
                isoDate = str(df.index[dayCount])
                if len(isoDate.split('-')) == 3:
                    date = QDate.fromString(isoDate, "yyyy-MM-dd")
                    s = date.day()
                else:
                    s = isoDate
                colsHdr.append(s)

                # days columns value
                interval = 0
                column = list()
                for hour in range(0, 24):
                    for minute in range(0, 60, 10):
                        if df.iloc[dayCount, interval] is None or np.isnan(df.iloc[dayCount, interval]):
                            df.iloc[dayCount, interval] = 0
                        value = int(df.iloc[dayCount, interval])
                        column.append(value)
                        interval += 1

                allColumns.append(column)

        elif res == 'day':  # 1 <= len(dataFrame.columns) <= 8:
            # up to 7 columns, one for each event class filtered + totals
            xlabel = "Events classification"
            ylabel = "Days covered"
            if subtractBackground:
                title += "\nafter sporadic background subtraction"
            # discard the daily totals, currently on rows
            # df = dataFrame.drop(labels=['Total'], axis=0)
            df = dataFrame

            # calculate full-scale value (red color) taking the maximum in all the counts columns
            countsOnlyDf = df.iloc[0:, 0:]
            self._fullScale = countsOnlyDf.max(axis=1).max()

            # rows header (days)
            yTicks = len(df.index)
            rowsHdr = list()
            for day in df.index:
                s = "{}".format(day)
                rowsHdr.append(s)

            allColumns = list()

            # columns header (classes)
            colsHdr = []
            colsList = list(dataFrame.columns)
            xTicks = len(colsList)
            for col in colsList:
                col = col.replace(' ', '\n')
                colsHdr.append(col)

            # columns counts per class by day
            df = df.fillna(0)
            for flt in range(0, len(colsHdr)):
                column = list()
                for dayCount in range(0, len(rowsHdr)):
                    print("flt={}, dayCount={}".format(flt, dayCount))
                    count = int(df.iloc[dayCount, flt])
                    column.append(count)
                allColumns.append(column)
        else:
            return

        data = np.array(allColumns)
        data = np.transpose(data)

        # backColor = colors['background'].name()
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1) #, facecolor=backColor)
        # ax.set_facecolor(backColor)

        im = ax.imshow(data, cmap=cmap, aspect='auto')

        # Show all ticks and label them with the respective list entries
        if showGrid:
            ax.spines[:].set_visible(True)
            ax.set_xticks(np.arange(len(colsHdr)), labels=colsHdr)  # , minor=True)
            ax.set_yticks(np.arange(len(rowsHdr)), labels=rowsHdr)  # , minor=True)

            # ax.grid(which="minor", color=colors['minorGrids'].name(),
            #        linestyle=self._settings.readSettingAsString('minorLineStyle'),
            #        linewidth=self._settings.readSettingAsString('minorLineWidth'))

            ax.grid(which="minor")
            ax.tick_params(which="minor", bottom=False, left=False)

            plt.hlines(y=np.arange(0, yTicks) + 0.5, xmin=np.full(yTicks, 0) - 0.5,
                       xmax=np.full(yTicks, xTicks) - 0.5)
            plt.vlines(x=np.arange(0, xTicks) + 0.5, ymin=np.full(xTicks, 0) - 0.5,
                       ymax=np.full(xTicks, yTicks) - 0.5)

        else:
            ax.spines[:].set_visible(False)
            ax.set_xticks(np.arange(len(colsHdr)), labels=colsHdr)
            ax.set_yticks(np.arange(len(rowsHdr)), labels=rowsHdr)

        ax.xaxis.set_ticks_position('top')
        ax.xaxis.set_label_position('top')
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        ax.tick_params(axis='x', which='both')

        plt.setp(ax.get_xticklabels(), ha="center")
        valFmt = mp.ticker.StrMethodFormatter('{x:.0f}')
        if showValues:
            # Loop over data dimensions and create text annotations.
            for i in range(len(rowsHdr)):
                for j in range(len(colsHdr)):
                    if data[i, j] > (self._fullScale / 4):
                        ax.text(j, i, valFmt(data[i, j]), ha="center", va="center", color="black")
                    else:
                        ax.text(j, i, valFmt(data[i, j]), ha="center", va="center", color="white")

        norm = mp.colors.Normalize(vmin=0, vmax=int(self._fullScale))
        self._fig.colorbar(im, drawedges=showGrid, norm=norm, cmap=cmap)

        if self._settings.readSettingAsString('cursorEnabled') == 'true':
            cursor(hover=True)

        self._fig.suptitle(title + '\n')
        # ax.set_title(title + '\n', loc='left')
        self._fig.set_tight_layout({"pad": 3.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        # avoids showing the original fig window
        plt.close('all')
