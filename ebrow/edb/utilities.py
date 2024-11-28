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
import shutil
import datetime
from importlib import import_module

import numpy as np
from PyQt5.QtWidgets import QMessageBox
from PyQt5.QtCore import QByteArray

import pandas as pd
import math
import matplotlib.ticker as ticker
import matplotlib as mp
from matplotlib.colors import ListedColormap
from ctypes import *
from matplotlib.dates import num2date
from pathlib import Path
from .logprint import print


class DumpHeader(Structure):
    _pack_ = 1
    _fields_ = [('startMarker', 4 * c_char),
                ('resolution', c_double),
                ('startHz', c_uint32),
                ('points', c_uint32),
                ('isAudio', c_bool)]


class ScanHeader(Structure):
    _pack_ = 1
    _fields_ = [('scanMarker', 2 * c_char),
                ('timestamp_us', c_double),
                ('firstCell', c_float)]


class ScanFooterV1(Structure):
    _pack_ = 1
    _fields_ = [('lastCell', c_float),
                ('endMarker', 2 * c_char),
                ('avgN', c_float),
                ('maxDbfs', c_float),
                ('maxDiff', c_float)]

class ScanFooterV2(Structure):
    _pack_ = 1
    _fields_ = [('lastCell', c_float),
                ('endMarker', 2 * c_char),
                ('avgN', c_float),
                ('maxDbfs', c_float),
                ('maxDiff', c_float),
                ('avgDbfs', c_float),
                ('upThr', c_float),
                ('dnThr', c_float)
                ]


class PrecisionDateFormatter(ticker.Formatter):
    """
    Extend the `matplotlib.ticker.Formatter` class to allow for millisecond
    precision when formatting a tick (in days since the epoch) with a
    `~datetime.datetime.strftime` format string.

    """

    def __init__(self, fmt, precision=3, tz=None):
        """
        Parameters
        ----------
        fmt : str
            `~datetime.datetime.strftime` format string.
        """

        if tz is None:
            from matplotlib.dates import _get_rc_timezone
            tz = _get_rc_timezone()

        self.fmt = fmt
        self.tz = tz
        self.precision = precision

    def __call__(self, x, pos=0):
        # if x == 0:
        #     raise ValueError("PrecisionDateFormatter found a value of 0, which is "
        #                      "an illegal date; this usually occurs because "
        #                      "you have not informed that axis that it is "
        #                      "plotting dates, e.g., with ax.xaxis_date()")

        dt = num2date(x, self.tz)
        ms = dt.strftime("%f")[:self.precision]

        return dt.strftime(self.fmt).format(ms=ms)

    def set_tzinfo(self, tz):
        self.tz = tz


def splitASCIIdumpFile(data: QByteArray):
    lineNum = 1
    dataStr = data.data().decode()
    dataLines = dataStr.splitlines()
    mapList = []
    powerList = []
    powerCells = 0

    for line in dataLines:
        cells = line.split()
        if len(cells) >= 3:
            timeMs = float(cells[0])  # int(float(cells[0]) * 1000)
            if len(cells) == 3:
                cells[0] = timeMs
                cells[1] = int(cells[1])
                cells[2] = float(cells[2])
                mapList.append(cells[:3])

            if len(cells) == 7:
                # scan line end
                cells[0] = timeMs
                cells[1] = int(cells[1])
                cells[2] = float(cells[2])
                cells[3] = timeMs
                cells[4] = float(cells[4])      # S
                cells[5] = float(cells[5])      # N
                cells[6] = float(cells[6])      # S-N
                powerList.append(cells[3:])
                powerCells = 7

            if len(cells) == 10:
                # scan line end (Echoes 0.56++)
                cells[0] = timeMs
                cells[1] = int(cells[1])
                cells[2] = float(cells[2])
                cells[3] = timeMs
                cells[4] = float(cells[4])      # S
                cells[5] = float(cells[5])      # N
                cells[6] = float(cells[6])      # S-N
                cells[4] = float(cells[4])      # avg S-N
                cells[5] = float(cells[5])      # up threshold
                cells[6] = float(cells[6])      # dn threshold
                powerCells = 10

    dfMap = pd.DataFrame(mapList, columns=['time', 'frequency', 'S'])

    dfPower = None
    if powerCells == 7:
        dfPower = pd.DataFrame(powerList, columns=['time', 'N', 'S', 'diff'])
    else:
        dfPower = pd.DataFrame(powerList, columns=['time', 'N', 'S', 'diff', 'avgDiff', 'upThr', 'dnThr'])

    dfPower['timestamp'] = (pd.to_datetime(dfPower['time'], unit='s'))

    return dfMap, dfPower


def splitBinaryDumpFile(rawData: QByteArray):
    """

    """
    dh = DumpHeader()
    # print("sizeof dh=", sizeof(dh))
    sh = ScanHeader()
    # print("sizeof sh=", sizeof(sh))

    beg = 0
    ver = 0
    end = beg + sizeof(dh)

    dh = DumpHeader.from_buffer(rawData[beg:end])
    if dh.startMarker == b'DUMP':
        ver = 1
        print("binary dump file version 1 (Echoes vv.0.53 ->.0.55, startMarker=", dh.startMarker)
    elif dh.startMarker == b'DMP2':
        ver = 2
        print("binary dump file version 2 (Echoes vv.0.56++, startMarker=", dh.startMarker)
    else:
        print("binary dump file out of sync: ", dh.startMarker)
        return None, None

    scanNr = 0
    mapList = []
    powerList = []
    crc_16 = c_uint16

    while end < (rawData.length() - sizeof(crc_16)):
        sh = ScanHeader()
        beg = end
        end += sizeof(sh)
        sh = ScanHeader.from_buffer(rawData[beg:end])
        if sh.scanMarker != b'$$':
            print("binary dump file, scan#{} out of sync: {}".format(scanNr, sh.scanMarker))
            return None, None

        # print("reading scan#", scanNr)

        # the first cell value is in the header
        mapList.append([sh.timestamp_us, dh.startHz, sh.firstCell])

        # while other cells must be read as array of [dh.points - 2] elements
        # since the first cell has been read with the header and
        # the last one will be read with the footer

        class ScanArray(Array):
            _length_ = (dh.points - 2)
            _type_ = c_float

        beg = end
        end += sizeof(ScanArray)
        scanArr = ScanArray.from_buffer(rawData[beg:end])

        for idx in range(0, dh.points - 2):
            point = idx + 1
            freq = dh.startHz + (dh.resolution * point)
            dBfs = scanArr[idx]
            mapList.append([sh.timestamp_us, freq, dBfs])

        beg = end

        sf = None
        dfCols = []
        if ver == 1:
            end += sizeof(ScanFooterV1)
            sf = ScanFooterV1.from_buffer(rawData[beg:end])
            if sf.endMarker != b'&&':
                print("binary dump file V1, scan#{} termination out of sync: {}".format(scanNr, sf.endMarker))
                return None, None
            powerList.append([sh.timestamp_us, sf.avgN, sf.maxDbfs, sf.maxDiff])
            dfCols = ['time', 'N', 'S', 'diff']

        elif ver == 2:
            end += sizeof(ScanFooterV2)
            sf = ScanFooterV2.from_buffer(rawData[beg:end])
            if sf.endMarker != b'&&':
                print("binary dump file V2, scan#{} termination out of sync: {}".format(scanNr, sf.endMarker))
                return None, None
            powerList.append([sh.timestamp_us, sf.avgN, sf.maxDbfs, sf.maxDiff, sf.avgDbfs, sf.upThr, sf.dnThr])
            dfCols = ['time', 'N', 'S', 'diff', 'avgDiff', 'upThr', 'dnThr']

        scanNr += 1

    # TODO: CRC-16 check
    if end < rawData.length():
        beg = end
        end += sizeof(crc_16)
        crc_16 = c_uint16.from_buffer(rawData[beg:end]).value
        print("CRC-16 read: {:04x}".format(crc_16))
    else:
        print("*** MISSING CRC16 ***")

    dfMap = pd.DataFrame(mapList, columns=['time', 'frequency', 'S'])
    dfPower = pd.DataFrame(powerList, columns=dfCols)
    dfPower['timestamp'] = (pd.to_datetime(dfPower['time'], unit='s'))
    print("binary data read successful")
    return dfMap, dfPower


def castFloatPrecision(value: float, precision: int = 4):
    if value != 0:
        multipliers = [0, 10, 100, 1000, 10e3, 10e4, 10e5, 10e6, 10e7]
        if precision > 8:
            precision = 8
        mult = multipliers[precision]
        v2 = value * mult
        result = int(v2) / mult
        # print ("cast: {} --> {}".format(value, result))
    else:
        result = value
    return result


def clearLayout(layout):
    toDelete = layout.count()
    # print("Clearing {} thumbnails".format(toDelete))
    while layout.count() > 0:
        child = layout.takeAt(0)
        if child.widget():
            child.widget().hide()
            child.widget().deleteLater()
            # print("deleted child widget")
        elif child.layout():
            clearLayout(child.layout())
            # print("deleted child layout")


def notice(title: str, msg: str):
    errorbox = QMessageBox()
    errorbox.setWindowTitle("Echoes data browser")
    errorbox.setText(title + "\n" + msg)
    errorbox.setIcon(QMessageBox.Information)
    errorbox.exec_()


def getColorMap(name: str = 'echoes'):
    # echoes color scale
    echoes = [
        "black",
        "blue",
        "purple",
        "orange",
        "yellow",
        "white"
    ]

    # Colorgramme-like 24 colors scale
    # the black means -1 == data missing that is not
    # included in colormap but set at display time
    colorgramme = [
        "#0064fe",
        "#006dfd",
        "#0079f0",
        "#0087fe",
        "#009eff",
        "#00b0ff",
        "#00c6f3",
        "#00eaff",
        "#38fcf5",
        "#4efdd4",
        "#6cfcb8",
        "#83fca1",
        "#a4f892",
        "#b8fc7a",
        "#d1fc68",
        "#f4f96d",
        "#ffe600",
        "#ffc500",
        "#ffae00",
        "#ff9400",
        "#ff8600",
        "#ff7300",
        "#ff6300",
        "#ff1600"
    ]

    cmap = None
    if name == 'echoes':
        colors = echoes
        totColors = len(colors)
    elif name == 'colorgramme':
        colors = colorgramme
        totColors = len(colors)
    else:
        cmap = mp.colormaps[name]
        totColors = cmap.N
        # cmap = mp.colormaps[name]._resample(totColors)

    if cmap is None:
        cmap = ListedColormap(colors, name=name)
    print("name={}, total colors={}".format(name, totColors))
    return cmap


def createCustomColormapsDict():
    myCmaps = dict()

    # matplotlib embedded colormaps
    for key in mp.colormaps.keys():
        # print("key=", key)
        myCmaps[key] = mp.colormaps[key]

    # customized colormaps
    for key in ['echoes', 'colorgramme']:
        cmap = getColorMap(key)
        myCmaps[key] = cmap
    return myCmaps


def fpCmp(val1: float, val2: float, decimals: int):
    multiplier = math.pow(10, decimals)
    i1 = int(val1 * multiplier)
    i2 = int(val2 * multiplier)
    return i2 - i1


def fuzzyCompare(a: float, b: float, tolerance: float):
    diff = math.fabs(a - b)
    if a != 0:
        percDiff = math.fabs((diff * 100) / a)
    else:
        percDiff = math.fabs(diff * 100)

    if percDiff > tolerance:
        return 1 if (a > b) else -1
    return 0


def normalize(a: object, beg: float, end: float, default: float):
    b = np.interp(a, (a.min(), a.max()), (beg, end))
    b[np.isnan(b)] = default
    return b


def cleanFolder(folderPath: str, removeEmptyFolders: bool = True):
    for filename in os.listdir(folderPath):
        file_path = os.path.join(folderPath, filename)
        try:
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path) and removeEmptyFolders:
                shutil.rmtree(file_path)
        except Exception as e:
            print('Failed to delete %s. Reason: %s' % (file_path, e))
            return False
    return True


def mkExportFolder(folderPath: Path):
    return Path.mkdir(folderPath, parents=True, exist_ok=True)


def cryptDecrypt(st: str, key: int) -> str:
    stCrypt = ''
    for i in range(len(st)):
        stCrypt += chr(ord(st[i]) ^ (key >> 8))
    return stCrypt


def timestamp2sideral(ts: str) -> str:
    jd = pd.Timestamp(ts).to_julian_date()

    # sideral time in degrees
    gmst0 = (280.46061837 + 360.98564736629 * (jd - 2451545)) % 360

    # conversion to decimal hours:
    stDecHour = gmst0 / 15.0

    # extracting hexagesimal minutes:
    stHexaMin = math.modf(stDecHour)[0] * 60

    # extracting hexagesimal seconds:
    stHexaSec = math.modf(stHexaMin)[0] * 60

    # formatting string
    sideralTime = "{:02}:{:02}:{:02}".format(int(stDecHour), int(stHexaMin), int(stHexaSec))
    return sideralTime


def getFromModule(moduleName, attrName):
    moduleName = "edb." + moduleName
    print("moduleName={}, attrName={}".format(moduleName, attrName))

    try:
        module = import_module(moduleName)
    except ImportError as err:
        print(err)
        return None

    try:
        return getattr(module, attrName)
    except AttributeError:
        return None


def addDateDelta(date: str, deltaDays: int):
    # convert string to date object
    dt = datetime.datetime.strptime(date, "%Y-%m-%d")
    td = datetime.timedelta(days=deltaDays)
    dt = dt + td
    return dt.strftime("%Y-%m-%d")

def toTypePoints(pixel: int):
    return pixel / 1.33

def toChars(pixel: int):
    return pixel / 8