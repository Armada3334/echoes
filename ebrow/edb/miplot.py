import pandas as pd
import numpy as np
import matplotlib as mp
import matplotlib.pyplot as plt
from scipy.stats import linregress
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg
from .settings import Settings
from .basegraph import BaseGraph
from .logprint import print

mp.use('Qt5Agg')
class MIplot(BaseGraph):

    def __init__(self, series: pd.Series, settings: Settings, inchWidth: float, inchHeight: float, metric: str, title: str,
                 yLabel: str, showValues: bool = False, showGrid: bool = False):
        """
        Plots a series on a logarithmic scatter plot with linear regression.

        @param series: Pandas series with thresholds as indices and counts as values.
        @param settings: Settings object with chart settings.
        @param inchWidth: Width of the chart in inches.
        @param inchHeight: Height of the chart in inches.
        @param metric: "power" or "lasting"
        @param title: Title of the chart.
        @param yLabel: Label of the Y-axis.
        @param showValues: If True, shows the values ​​of the data points.
        @param showGrid: If True, shows the chart grid.
        """

        self._series = series
        x = self._series.index[:-2]  # Esclude le ultime due colonne (Totals e Mass index)
        y = self._series.values[:-2]

        colors = settings.readSettingAsObject('colorDict')

        self._fig, self._ax = plt.subplots(figsize=(inchWidth, inchHeight))

        self._ax.set_xscale('log')
        self._ax.set_yscale('log')

        self._ax.set_title(title)
        if metric == "power":
            self._ax.set_xlabel('Power thresholds [mW]')
        else:
            self._ax.set_xlabel('Lasting thresholds [ms]')

        self._ax.set_ylabel(yLabel)

        if showGrid:
            self._ax.grid(True, which="both", ls="--")

        # converts x values from string to num
        xNum = np.array([float(val) for val in x])

        # Regression
        if metric=='power':
            # dBfs are already logarithmic but with negative values
            # so we must convert them back to mW:
            xMilliWatt = 10 ** (xNum / 10)

            # Scatter plot
            self._ax.scatter(xMilliWatt, y, label='Counts')
            slope, intercept, r_value, p_value, std_err = linregress(xNum, np.log10(y))
            self._ax.plot(xMilliWatt, np.exp(intercept) * xMilliWatt ** slope, color=colors['S'].name(),
                          label=f'Linear regression (R² = {r_value ** 2:.2f})')
        else:
            # Scatter plot
            self._ax.scatter(xNum, y, label='Counts')
            slope, intercept, r_value, p_value, std_err = linregress(np.log10(xNum), np.log10(y))
            self._ax.plot(xNum, np.exp(intercept) * xNum ** slope, color=colors['N'].name(),
                      label=f'Linear regression (R² = {r_value ** 2:.2f})')

        if showValues:
            for i, txt in enumerate(y):
                self._ax.annotate(f'{txt:.2f}', (xNum[i], y[i]))

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