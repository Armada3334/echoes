"""
*********************************************************************************************************************************
Copyright © 2022 The Qt Company Ltd. Documentation contributions included herein are the copyrights of their respective owners.
The documentation provided herein is licensed under the terms of the GNU Free Documentation License version 1.3
(https://www.gnu.org/licenses/fdl.html) as published by the Free Software Foundation. Qt and respective logos are trademarks
of The Qt Company Ltd. in Finland and/or other countries worldwide. All other trademarks are property of their respective owners.
**********************************************************************************************************************************
"""
import pandas as pd
from PyQt5.QtCore import QAbstractTableModel, Qt, QModelIndex
from PyQt5.QtCore import QAbstractTableModel, Qt, QModelIndex
from PyQt5.QtGui import QColor
import pandas as pd
from PyQt5.QtCore import QAbstractTableModel, Qt, QModelIndex
from PyQt5.QtGui import QColor
import pandas as pd

class PandasModel(QAbstractTableModel):
    """A model to interface a Qt view with pandas DataFrame, with custom row and column colors."""

    def __init__(self, dataFrame: pd.DataFrame, rowColors: dict = None, columnColors: dict = None, parent=None):
        """
        Initializes the PandasModel.

        Args:
            dataFrame: The pandas DataFrame to display.
            rowColors: A dictionary mapping row names to QColor objects.
            columnColors: A dictionary mapping column names to QColor objects.
            parent: The parent object.
        """
        QAbstractTableModel.__init__(self, parent)
        self._dataFrame = dataFrame
        self._rowColors = rowColors or {}
        self._columnColors = columnColors or {}

    def rowCount(self, parent=QModelIndex()) -> int:
        """Override method from QAbstractTableModel. Return row count of the pandas DataFrame."""
        if parent == QModelIndex():
            return len(self._dataFrame)
        return 0

    def columnCount(self, parent=QModelIndex()) -> int:
        """Override method from QAbstractTableModel. Return column count of the pandas DataFrame."""
        if parent == QModelIndex():
            return len(self._dataFrame.columns)
        return 0

    def data(self, index: QModelIndex, role=Qt.ItemDataRole):
        """Override method from QAbstractTableModel. Return data cell from the pandas DataFrame."""
        if not index.isValid():
            return None

        rowName = self._dataFrame.index[index.row()]
        columnName = self._dataFrame.columns[index.column()]

        if role == Qt.DisplayRole:
            return str(self._dataFrame.iloc[index.row(), index.column()])

        if role == Qt.ForegroundRole:
            if rowName in self._rowColors:
                return self._rowColors[rowName]
            if columnName in self._columnColors:
                return self._columnColors[columnName]
        return None

    def headerData(self, section: int, orientation: Qt.Orientation, role=Qt.ItemDataRole):
        """Override method from QAbstractTableModel. Return DataFrame index as vertical header data and columns as horizontal header data."""
        if role == Qt.DisplayRole:
            if orientation == Qt.Horizontal:
                return str(self._dataFrame.columns[section])

            if orientation == Qt.Vertical:
                return str(self._dataFrame.index[section])
        return None