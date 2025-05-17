import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')
class DistPlot(BaseGraph):

    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, metric: str, title: str,
                 xLabel: str, yLabel: str, xScale: str="linear", yScale: str="linear", showValues: bool = False, showGrid: bool = False):
        """
        Plots a series as XY where X are not datetimes

        @param series: Pandas series with thresholds as indices and counts as values.
        @param settings: Settings object with chart settings.
        @param inchWidth: Width of the chart in inches.
        @param inchHeight: Height of the chart in inches.
        @param title: Title of the chart.
        @param xLabel: Label of the X-axis.
        @param yLabel: Label of the Y-axis.
        @param showValues: If True, shows the values of the data points.
        @param showGrid: If True, shows the chart grid.
        """
        BaseGraph.__init__(self, settings)
        self._series = series
        x = self._series.index
        y = self._series.values

        colors = settings.readSettingAsObject('colorDict')

        self._fig, self._ax = plt.subplots(figsize=(inchWidth, inchHeight))

        # self._ax.set_title(title)
        self._fig.suptitle(title + '\n')
        self._ax.set_xlabel(xLabel)
        self._ax.set_ylabel(yLabel)

        self._ax.set_xscale(xScale)
        self._ax.set_yscale(yScale)

        if showGrid:
            self._ax.grid(True, which="both", ls="--")

        self._ax.plot(x, y, label=yLabel)

        if showValues:
            for i, txt in enumerate(y):
                if yScale == 'linear':
                    txt = int(txt)
                else:
                    txt = float(txt)
                self._ax.annotate(f'{txt}', (x[i], y[i]))

        self._ax.legend()
        self._fig.set_tight_layout({"pad": 5.0})
        self._canvas = FigureCanvasQTAgg(self._fig)
        plt.close('all')

    def show(self):
        """Mostra il grafico."""
        plt.show()


    def save(self, filename: str):
        """Salva il grafico in un file."""
        self._fig.savefig(filename)