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
import os
import matplotlib as mp
import matplotlib.pyplot as plt

from .settings import Settings
from .logprint import print

mp.use('Qt5Agg')


class BaseGraph:
    def __init__(self, settings: Settings, target: str = None):
        self._settings = settings
        self._target = target
        self.setFont(self._target)
        self._canvas = None
        self._fig = None
        plt.rc('figure', figsize=(10.24, 76.8))  # figure size in inches

    def widget(self):
        return self._canvas

    def saveToDisk(self, fileName):
        pix = self._canvas.grab()
        pix.save(fileName)

    def setFont(self, target: str = None):

        SMALL_SIZE = self._settings.readSettingAsInt('fontSize')
        MEDIUM_SIZE = SMALL_SIZE + 5
        BIGGER_SIZE = MEDIUM_SIZE + 3

        if target == 'RMOB':
            if os.name == 'nt':
                CUSTOM_SIZE = 32
                plt.rc('font',
                       size=CUSTOM_SIZE,
                       family='Arial',
                       weight=100,
                       style='normal'
                       )  # controls default text font attributes
            else:
                CUSTOM_SIZE = 28
                plt.rc('font',
                       size=CUSTOM_SIZE,
                       family='Liberation Sans',
                       weight=100,
                       style='normal'
                       )  # controls default text font attributes

            # font size of the X axis tick labels
            plt.rc('xtick', labelsize=CUSTOM_SIZE)

            # font size of the Y axis tick labels
            plt.rc('ytick', labelsize=CUSTOM_SIZE)

        else:
            plt.rc('font',
                   size=SMALL_SIZE,
                   family=self._settings.readSettingAsString('fontFamily'),
                   weight='bold' if 'Bold' in self._settings.readSettingAsString('fontStyle') else 'normal',
                   style='italic' if 'Italic' in self._settings.readSettingAsString('fontStyle') else 'normal'
                   )  # controls default text font attributes
            # font size of the x and y labels

            plt.rc('axes', labelsize=MEDIUM_SIZE)

            # font size of the X axis tick labels
            plt.rc('xtick', labelsize=SMALL_SIZE)

            # font size of the Y axis tick labels
            plt.rc('ytick', labelsize=SMALL_SIZE)

            # legend font size
            plt.rc('legend', fontsize=MEDIUM_SIZE)

            # figure title font size
            plt.rc('figure', titlesize=BIGGER_SIZE)

        colors = self._settings.readSettingAsObject('colorDict')
        backColor = colors['background'].name()

        #  background color of the axes
        plt.rc('axes', facecolor=backColor)

        # axes autolimit mode
        plt.rc('axes', autolimit_mode='round_numbers')

        # axes3d background color
        # plt.rc('axes3d', facecolor=backColor)

        # axes3d background color
        # plt.rc('axes3d.xaxis', edgecolor=backColor)
        # plt.rc('axes3d.yaxis', edgecolor=backColor)
        # plt.rc('axes3d.zaxis', edgecolor=backColor)

        #  axes line styles
        plt.rc('lines', linestyle=self._settings.readSettingAsString('dataLineStyle'))

        #  axes line width
        plt.rc('lines', linewidth=self._settings.readSettingAsString('dataLineWidth'))

        #  X axis numbers color
        plt.rc('xtick', color=colors['counts'].name())

        #  Y axis numbers color
        plt.rc('ytick', color=colors['counts'].name())

        plt.rc('lines', color=colors['majorGrids'].name())

        #  grid color
        plt.rc('grid', color=colors['majorGrids'].name())

        #  grid line style
        plt.rc('grid', linestyle=self._settings.readSettingAsString('majorLineStyle'))

        #  grid line width
        plt.rc('grid', linewidth=self._settings.readSettingAsString('majorLineWidth'))

        # figure title font weight
        plt.rc('figure', titleweight='bold')

        # figure background color
        plt.rc('figure', facecolor=backColor)

        plt.rcParams['savefig.pad_inches'] = 0

    def zoom(self, horz: int, vert: int):
        self._fig.set_size_inches(horz, vert)
        self._canvas.draw()
