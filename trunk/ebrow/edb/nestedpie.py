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

import matplotlib as mp
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg

from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print
mp.use('Qt5Agg')


class NestedPie(BaseGraph):
    def __init__(self, df: pd.DataFrame, settings: Settings, inchWidth: float, inchHeight: float, subtractBackground: bool=False):
        BaseGraph.__init__(self, settings)

        df['sum'] = df.sum(axis='columns')
        totals = df.loc[0, 'sum']
        totalOver = df.loc[0, 'OVER']
        totalUnder = df.loc[0, 'UNDER']
        totalFakes = totals - totalOver - totalUnder
        cats = list()
        subcats = list()
        valuesCats = list()
        valuesSubcats = list()
        colors = self._settings.readSettingAsObject('colorDict')
        if totalUnder > 0:
            count = totalUnder
            percentage = np.round((count / totals) * 100.0, 2)
            valuesCats.append(count)
            cats.append("Underdense:\n{} = {}%".format(count, percentage))
            subcats.append(' ')
            valuesSubcats.append(count)

        if totalOver > 0:
            count = totalOver
            percentage = np.round((count / totals) * 100.0, 2)
            valuesCats.append(count)
            cats.append("Overdense:\n{} = {}%".format(count, percentage))
            subcats.append(' ')
            valuesSubcats.append(count)

        if totalFakes > 0:
            count = totalFakes
            percentage = np.round((count / totals) * 100.0, 2)
            valuesCats.append(count)
            cats.append("Fakes:\n{} = {}%".format(count, percentage))

            count = df.loc[0, 'FAKE_CAR1']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("carriers 1:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)
            count = df.loc[0, 'FAKE_CAR2']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("carriers 2:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)
            count = df.loc[0, 'FAKE_RFI']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("RFI:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)
            count = df.loc[0, 'FAKE_ESD']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("ESD:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)
            count = df.loc[0, 'FAKE_SAT']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("Saturations:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)
            count = df.loc[0, 'FAKE_LONG']
            if count > 0:
                percentage = np.round((count / totals) * 100.0, 2)
                subcats.append("Too long:\n{} = {}%".format(count, percentage))
                valuesSubcats.append(count)

        cmap = plt.colormaps['tab20c']
        colorsCats = cmap([0, 4, 16])
        colorsSubCats = cmap([1, 5, 17, 14, 18, 19, 15])  # 1=under, 5=over, 16++ fakes...

        backColor = colors['background'].name()
        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1, facecolor=backColor)
        ax.set_facecolor(backColor)

        plt.pie(valuesCats, labels=cats, colors=colorsCats, startangle=180, frame=True)
        plt.pie(valuesSubcats, labels=subcats, colors=colorsSubCats, radius=0.7, startangle=180,
                labeldistance=0.7)
        center = plt.Circle((0, 0), 0.4, colors['majorGrids'].name(), linewidth=0)
        ax.annotate("total:\n\n {} = 100%".format(totals), xy=(0, 0), ha="center")
        title = "total events by classification"
        if subtractBackground:
            title += "\nafter sporadic background subtraction"
        ax.set_title(title)
        fig = plt.gcf()
        fig.gca().add_artist(center)
        plt.axis('equal')
        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
