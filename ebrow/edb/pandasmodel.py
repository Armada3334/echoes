"""
******************************************************************************

    Echoes Data Browser (Ebrow) is a data navigation and report generation
    tool for Echoes.
    Echoes is a RF spectrograph for SDR devices designed for meteor scatter
    Both copyright (C) 2018-2025
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
*********************************************************************************************************************************
Original PandasModel code:
Copyright © 2022 The Qt Company Ltd. Documentation contributions included herein are the copyrights of their respective owners.
The documentation provided herein is licensed under the terms of the GNU Free Documentation License version 1.3
(https://www.gnu.org/licenses/fdl.html) as published by the Free Software Foundation. Qt and respective logos are trademarks
of The Qt Company Ltd. in Finland and/or other countries worldwide. All other trademarks are property of their respective owners.
**********************************************************************************************************************************
"""
from PyQt5.QtCore import QAbstractTableModel, Qt, QModelIndex
from PyQt5.QtGui import QColor, QFont
import pandas as pd


class PandasModel(QAbstractTableModel):
    """A model to interface a Qt view with pandas DataFrame, with custom row and column styles."""

    def __init__(self, dataFrame: pd.DataFrame, rowStyles: dict = None, columnStyles: dict = None, parent=None):
        """
        Initializes the PandasModel.

        Args:
            dataFrame: The pandas DataFrame to display.
            rowStyles: A dictionary mapping row names to style dictionaries.
            columnStyles: A dictionary mapping column names to style dictionaries.
            parent: The parent object.
        """
        QAbstractTableModel.__init__(self, parent)
        self._dataFrame = dataFrame.copy()
        self._rowStyles = rowStyles or {}
        self._columnStyles = columnStyles or {}
        self._sortColumn = None
        self._sortAscending = True

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

        rowStyle = self._rowStyles.get(rowName, {})
        columnStyle = self._columnStyles.get(columnName, {})

        defaultRowStyle = self._rowStyles.get('*', {})
        defaultColumnStyle = self._columnStyles.get('*', {})

        if role == Qt.ForegroundRole:
            color = rowStyle.get('fgColor', defaultRowStyle.get('fgColor')) or columnStyle.get('fgColor',
                                                                                               defaultColumnStyle.get(
                                                                                                   'fgColor'))
            if color:
                return color

        if role == Qt.BackgroundRole:
            color = rowStyle.get('bgColor', defaultRowStyle.get('bgColor')) or columnStyle.get('bgColor',
                                                                                               defaultColumnStyle.get(
                                                                                                   'bgColor'))
            if color:
                return color

        if role == Qt.FontRole:
            font = rowStyle.get('font', defaultRowStyle.get('font')) or columnStyle.get('font',
                                                                                        defaultColumnStyle.get('font'))
            if font:
                return font

        if role == Qt.TextAlignmentRole:
            alignment = rowStyle.get('alignment', defaultRowStyle.get('alignment')) or columnStyle.get('alignment',
                                                                                                       defaultColumnStyle.get(
                                                                                                           'alignment'))
            if alignment:
                if alignment == 'left':
                    return Qt.AlignLeft | Qt.AlignVCenter
                elif alignment == 'center':
                    return Qt.AlignCenter
                elif alignment == 'right':
                    return Qt.AlignRight | Qt.AlignVCenter
                else:
                    return Qt.AlignLeft | Qt.AlignVCenter  # default value

        return None

    def headerData(self, section: int, orientation: Qt.Orientation, role=Qt.ItemDataRole):
        """Override method from QAbstractTableModel. Return DataFrame index as vertical header data and columns as horizontal header data."""
        if role == Qt.DisplayRole:
            if orientation == Qt.Horizontal:
                return str(self._dataFrame.columns[section])

            if orientation == Qt.Vertical:
                return str(self._dataFrame.index[section])
        return None

    def sort(self, column: int, order: Qt.AscendingOrder):
        """Override method from QAbstractTableModel. Sort DataFrame by column number."""
        columnName = self._dataFrame.columns[column]
        self._dataFrame = self._dataFrame.sort_values(by=columnName, ascending=(order == Qt.AscendingOrder))
        self.layoutChanged.emit()

    def setSortColumn(self, column: int):
        """Function to handle the sorting logic."""
        if self._sortColumn == column:
            self._sortAscending = not self._sortAscending
        else:
            self._sortColumn = column
            self._sortAscending = True

        order = Qt.AscendingOrder if self._sortAscending else Qt.DescendingOrder
        self.sort(column, order)

