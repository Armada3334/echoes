"""
******************************************************************************

    Echoes Data Browser (Ebrow) is a data navigation and report generation
    tool for Echoes.
    Echoes is a RF spectrograph for SDR devices designed for meteor scatter.

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
from PyQt5.QtGui import QColor, QPixmap
from PyQt5.QtCore import Qt, QRect, QSettings, QDate, QLocale
from .logprint import print

class Settings(QSettings):

    DBFS_RANGE_MIN = -150
    DBFS_RANGE_MAX = 20
    ZOOM_MIN = 0.5
    ZOOM_MAX = 10.0
    ZOOM_DEFAULT = 1.0
    AZIMUTH_DEFAULT = 0
    ELEVATION_DEFAULT = 45

    def __init__(self, iniFullPath):
        QSettings.__init__(self, iniFullPath, QSettings.IniFormat)
        self._locale = QLocale()
        print("Local country {}, language {}".format(QLocale.countryToString(self._locale.country()),
                                                               QLocale.languageToString(self._locale.language())))

        self._iniFilePath = iniFullPath
        self._general = ['geometry', 'lastDBfilePath', 'dateFrom', 'dateTo', 'tooltipDisabled', 'autosaving']

        self._screenshots = ['classFilter', 'currentDate', 'horizontalZoom', 'verticalZoom', 'linkedSliders', 'showGrid',
                             '3Dazimuth', '3Delevation', 'currentColormap']

        self._stats = ['classFilterStat', 'showValues', 'showGridStat', 'smoothPlots', 'subtractSporadicBackground',
                       'compensation', 'horizontalZoomStat', 'verticalZoomStat', 'linkedSlidersStat',
                       'currentColormapStat', 'RMOBfilePrefix',
                       'sporadicDates', 'sporadicBackgroundDaily', 'sporadicBackgroundByHour',
                       'sporadicBackgroundBy10min']

        self._plotStyles = ['fontFamily', 'fontSize', 'fontStyle','majorLineStyle', 'majorLineWidth', 'minorLineStyle',
                            'minorLineWidth', 'dataLineStyle', 'dataLineWidth',
                            'cursorEnabled',  'separator']

        self._siteInfos = ['stationName', 'owner', 'country', 'city', 'latitude', 'longitude', 'latitudeDeg',
                           'longitudeDeg', 'altitude', 'logoPath',
                           'antenna', 'antAzimuth', 'antElevation', 'preamplifier', 'receiver', 'frequencies',
                           'computer',
                           'email', 'notes']

        self._filters = ["RFIfilter", "RFIfilterThreshold", 'ESDfilter', 'ESDfilterThreshold', 'SATfilter', 'SATfilterThreshold', 'CAR1filter',
                         'CAR1filterThreshold', 'CAR2filter', 'CAR2filterThreshold',
                         'underdenseMs', 'overdenseSec', 'carrierSec', 'acqActive', 'refNlevel', 'enableRefN']

        self._attrFilters = ["afHasHeadEnabled"]

        self._report = ['siteInfosExc', 'dailyExc', 'RMOBexc', 'hourlyExc', 'tenMinExc',
                        'chronoExc', 'expEvExc', 'stTabExc', 'stGrpExc', 'setupExc',
                        'classFilterReport', 'RMOBclient', 'preface',
                        'aeMinLast', 'aeScreenshot', 'aePowerPlot', 'ae2Dplot', 'ae3Dplot', 'aeDetails', 'aeNoComments']

        self._colorMaps = ['echoes', 'colorgramme', 'gray', 'cividis', 'inferno', 'magma', 'plasma', 'viridis']

        self._groups = {'General': self._general, 'Plotting styles': self._plotStyles, 'Site infos': self._siteInfos,
                        'Screenshots': self._screenshots, 'Stats': self._stats, 'Filters': self._filters,
                        'Report': self._report, 'Attributes': self._attrFilters}

        d = dict()

        # default settings
        # warning: use 0/1 instead of False/True
        # general
        d['geometry'] = QRect(0, 0, 1024, 768)
        d['dateFrom'] = '2022-01-01'
        d['dateTo'] = '2022-10-31'
        d['lastDBfilePath'] = None
        d['tooltipDisabled'] = 0
        d['autosaving'] = 0

        # plot styles
        if os.name == 'nt':
            d['fontFamily'] = 'Times New Roman'
        else:
            d['fontFamily'] = 'Liberation Serif'

        d['fontSize'] = 9
        d['fontStyle'] = 'Normal'   # 'Italic Bold' is still possible
        d['dataLineStyle'] = 'solid'
        d['majorLineStyle'] = 'solid'
        d['minorLineStyle'] = 'solid'
        d['dataLineWidth'] = '1.0'
        d['majorLineWidth'] = '0.8'
        d['minorLineWidth'] = '0.5'
        d['cursorEnabled'] = 1
        d['separator'] = 'comma'

        # plotting colors
        d['colorDict'] = {'S': QColor('#ff0000'), 'N': QColor('#00ffff'), 'diff': QColor('#FF8C00'),
                          'avgDiff': QColor('#800000'), 'upperThr': QColor('#FFFF00'), 'lowerThr': QColor('#00FF00'),
                          'counts': QColor('#008080'), 'majorGrids': QColor('#696969'),
                          'minorGrids': QColor('#696969'), 'background': QColor('#FFFFFF')}

        # siteinfos
        d['stationName'] = ''
        d['owner'] = 'Somebody Somewhere'
        d['country'] = ''
        d['city'] = ''
        d['latitudeDeg'] = "0° 0\' 0\""
        d['longitudeDeg'] = "0° 0\' 0\""
        d['latitude'] = '0.0'
        d['longitude'] = '0.0'
        d['altitude'] = 0
        d['logoPath'] = ":/logo"
        d['antenna'] = ''
        d['antAzimuth'] = 0
        d['antElevation'] = 0
        d['frequencies'] = "0000.000.000"
        d['preamplifier'] = ''
        d['receiver'] = ''
        d['computer'] = ''
        d['email'] = ''
        d['notes'] = ''

        # screenshots
        d['currentDate'] = ''
        d['classFilter'] = 'OVER'
        d['horizontalZoom'] = self.ZOOM_DEFAULT
        d['verticalZoom'] =  self.ZOOM_DEFAULT
        d['linkedSliders'] = 0
        d['showGrid'] = 0
        d['3Dazimuth'] =  self.AZIMUTH_DEFAULT
        d['3Delevation'] =  self.ELEVATION_DEFAULT
        d['currentColormap'] = self._colorMaps[0] # default colormap for screenshots: echoes

        # stats
        d['classFilterStat'] = 'OVER'
        d['showGridStat'] = 0
        d['subtractSporadicBackground'] = 0
        d['compensation'] = 0
        d['showValues'] = 1
        d['smoothPlots'] = 1
        d['horizontalZoomStat'] = self.ZOOM_DEFAULT
        d['verticalZoomStat'] = self.ZOOM_DEFAULT
        d['linkedSlidersStat'] = 0
        d['currentColormapStat'] = self._colorMaps[1] # default colormap for statistics: colorgramme
        d['RMOBfilePrefix'] = 'UNKNOWN'
        d['sporadicDates'] = ''     # date intervals for sporadic backgroud calculation
        d['sporadicBackgroundDaily'] = ''
        d['sporadicBackgroundByHour'] = ''
        d['sporadicBackgroundBy10min'] = ''

        # filters
        d['RFIfilter'] = True
        d['RFIfilterThreshold'] = 0.8
        d['ESDfilter'] = True
        d['ESDfilterThreshold'] = 90.0
        d['SATfilter'] = True
        d['SATfilterThreshold'] = 50.0
        d['CAR1filter'] = True
        d['CAR1filterThreshold'] = 1.0E-5
        d['CAR2filter'] = True
        d['CAR2filterThreshold'] = 10.0
        d['underdenseMs'] = 200
        d['overdenseSec'] = 50
        d['carrierSec'] = 5
        d['acqActive'] = 0
        d['refNlevel'] = 0.0
        d['enableRefN'] = False

        # report
        d['siteInfosExc'] = 0
        d['dailyExc'] = 0
        d['RMOBexc'] = 0
        d['hourlyExc'] = 0
        d['tenMinExc'] = 1
        d['chronoExc'] = 0
        d['expEvExc'] = 0
        d['stTabExc'] = 0
        d['stGrpExc'] = 0
        d['setupExc'] = 0
        d['classFilterReport'] = "OVER, UNDER"
        d['RMOBclient'] = "unknown"
        d['preface'] = ""
        d['aeMinLast'] = 3
        d['aeScreenshot'] = True
        d['aePowerPlot'] = False
        d['ae2Dplot'] = False
        d['ae3Dplot'] = False
        d['aeDetails'] = True
        d['aeNoComments'] = True

        # attribute filters
        d['afDummyEnabled'] = False
        d['afHasHeadEnabled'] = False

        self._defaults = d
        self._settings = d

    def decimalPoint(self):
        return self._locale.decimalPoint()

    def dataSeparator(self):
        sepIdx = self.readSettingAsString('separator')
        separators = {'space': ' ', 'tabulation': '\t', 'comma': ',', 'semicolon': ';', 'colon': ':', 'pipe': '|'}
        return separators[sepIdx]

    def writeSetting(self, key: str, value: any):
        print("writeSetting(key={}, value={})".format(key, value))
        self._settings[key] = value

    def readSettingAsObject(self, key: str):
        d = self._settings
        return self._settings[key]

    def readSettingAsString(self, key: str, default: str = '----'):
        d = self._settings
        s = str(self._settings[key])
        if len(s) == 0:
            s = default
        return s

    def readSettingAsInt(self, key: str):
        d = self._settings
        value = self._settings[key]
        return int(value)

    def readSettingAsBool(self, key: str):
        d = self._settings
        value = self._settings[key]
        if value == '0' or value == 'false' or value == 'False' or value == 0 or value is False:
            value = False
        else:
            value = True
        return value

    def readSettingAsFloat(self, key: str):
        d = self._settings
        value = self._settings[key]
        return float(value)

    def coverage(self):
        """
        @return: tuple from,to
        """
        d = self._settings
        return d['dateFrom'], d['dateTo']

    def load(self):
        for group in self._groups.keys():
            self.beginGroup(group)
            for key in self._groups[group]:
                try:
                    value = self.value(key, self._defaults[key])
                    if value == 'false':
                        value = False
                    if value == 'true':
                        value = True
                    # print("loading key={}, value={}, type is {}".format(key, value, type(value)))
                except KeyError:
                    value = 0
                self._settings[key] = value
            self.endGroup()

        self.beginGroup('Plotting styles')
        colorNames = self._getDictValues(self._defaults['colorDict'], "Plotting colors")
        self._settings['colorDict'] = dict()
        for key in colorNames.keys():
            name = colorNames[key]
            color = QColor(name)
            self._settings['colorDict'][key] = color
        self.endGroup()

    def save(self):
        for group in self._groups.keys():
            self.beginGroup(group)
            for key in self._groups[group]:
                value = self._settings[key]
                # print("saving key={}, value={}, type is {}".format(key, value, type(value)))
                self.setValue(key, value)
            self.endGroup()

        colorNames = dict()
        for key in self._settings['colorDict'].keys():
            name = self._settings['colorDict'][key].name()
            colorNames[key] = name
        self.beginGroup('Plotting styles')
        self._setDictValues(colorNames, "Plotting colors")
        self.endGroup()
        self.sync()

    def _setDictValues(self, values: dict, groupName: str):
        self.beginGroup(groupName)
        for key in values.keys():
            self.setValue(key, values[key])
        self.endGroup()

    def _getDictValues(self, defaults: dict, groupName: str):
        self.beginGroup(groupName)
        values = dict()
        for key in defaults.keys():
            values[key] = self.value(key, defaults[key])
        self.endGroup()
        return values
