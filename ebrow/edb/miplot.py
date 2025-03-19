import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
import matplotlib.dates as md
from matplotlib.ticker import MultipleLocator, MaxNLocator
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
# from matplotlib.widgets import Cursor  # If you need the cursor
import os  # needed for the font size check

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')


class MIPlot(BaseGraph):
    """
    Mass Index bi-logarithmic plot.
    """

    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, title: str,
                 yLabel: str,
                 showValues: bool = False, showGrid: bool = False):
        BaseGraph.__init__(self, settings)  # Call BaseGraph's constructor

        self._series = series
        x = self._series.index
        y = self._series.values

        plt.figure(figsize=(inchWidth, inchHeight))
        self._fig, ax = plt.subplots(1)

        ax.set_xscale('log')
        ax.set_yscale('log')

        if not pd.api.types.is_datetime64_any_dtype(x):
            try:
                xdt = pd.to_datetime(x).values.astype('datetime64[s]')
            except:
                xdt = x
        else:
            xdt = x

        ax.plot(xdt, y, label='Mass Index')

        if pd.api.types.is_datetime64_any_dtype(xdt):
            n = len(xdt)
            if n < 24:
                locator = MultipleLocator(1)
            else:
                n = 24
                locator = MaxNLocator(n)

            ax.xaxis.set_major_locator(locator)
            myFmt = md.DateFormatter('%Y-%m-%d')
            ax.xaxis.set_major_formatter(myFmt)
            ax.tick_params(axis='x', which='both', labelrotation=60)

        self._fig.suptitle(title + '\n')

        if showGrid:
            ax.grid(which='both')

        ax.set_xlabel('Time')
        ax.set_ylabel(yLabel)

        if showValues:
            # TBD
            pass

        # Cursor (if needed - handle settings from self._settings)
        #if self._settings.readSettingAsString('cursorEnabled') == 'true':
        #    cursor(hover=True)

        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        plt.close('all')
