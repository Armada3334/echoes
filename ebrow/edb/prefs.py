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
import locale
import json
import sys
from glob import glob
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QColorDialog, QWidget, QButtonGroup, QFileDialog
from PyQt5.QtGui import QColor, QIcon, QPixmap, QStandardItem, QStandardItemModel
from .logprint import print
from .dateintervaldialog import DateIntervalDialog
from .utilities import getFromModule


class Prefs(QWidget):
    def __init__(self, parent, ui, settings):
        QWidget.__init__(self, parent)
        self._ui = ui
        self._parent = parent
        locale.setlocale(locale.LC_ALL, '')
        self._loadCbCountry()
        self._settings = settings
        self._plottingColors = dict()
        self._RMOBclient = "UNKNOWN"
        self._pressedButton = None
        self._tabStats = self._parent.tabStats
        self._dataSource = self._parent.dataSource
        self._sporadicDatesList = list()
        self._sporadicBackgroundByHour = ''
        self._sporadicBackgroundBy10min = ''
        self._attributeFilterNames = list()
        self._attributeFilterObject = list()
        self._attributeFilterEnabled = list()
        self._attributeFilters = dict()

        self._ssBase = "min-height: 20px; min-width: 20px; border: 1px solid yellow; background-color: {};"
        self._colorButtons = [self._ui.pbScolor, self._ui.pbNcolor, self._ui.pbDiffColor, self._ui.pbAvgDiffColor,
                              self._ui.pbUpperColor, self._ui.pbLowerColor, self._ui.pbCountsColor,
                              self._ui.pbGridsColor, self._ui.pbMinColor, self._ui.pbBackColor]
        self._colorKeys = ['S', 'N', 'diff', 'avgDiff', 'upperThr', 'lowerThr', 'counts', 'majorGrids', 'minorGrids',
                           'background']
        self._colorDict = self._settings.readSettingAsObject('colorDict')

        self._ui.cbFontFamily.currentTextChanged.connect(
            lambda text: self._settings.writeSetting('fontFamily', text))

        self._ui.cbFontStyle.currentTextChanged.connect(
            lambda text: self._settings.writeSetting('fontStyle', text))

        self._ui.cbDataLineStyle.currentTextChanged.connect(
            lambda text: self._settings.writeSetting('dataLineStyle', text))
        self._ui.cbMajorLineStyle.currentTextChanged.connect(
            lambda text: self._settings.writeSetting('majorLineStyle', text))
        self._ui.cbMinorLineStyle.currentTextChanged.connect(
            lambda text: self._settings.writeSetting('minorLineStyle', text))

        self._ui.sbFontSize.valueChanged.connect(lambda val: self._settings.writeSetting('fontSize', val))
        self._ui.sbDataLineWidth.valueChanged.connect(lambda val: self._settings.writeSetting('dataLineWidth', val))
        self._ui.sbMajorLineWidth.valueChanged.connect(lambda val: self._settings.writeSetting('majorLineWidth', val))
        self._ui.sbMinorLineWidth.valueChanged.connect(lambda val: self._settings.writeSetting('minorLineWidth', val))
        self._ui.chkTooltips.clicked.connect(lambda checked: self._settings.writeSetting('tooltipDisabled', checked))
        self._ui.chkAutosave.clicked.connect(lambda checked: self._settings.writeSetting('autosaving', checked))
        self._ui.chkShowCursor.clicked.connect(lambda checked: self._settings.writeSetting('cursorEnabled', checked))
        self._ui.cbDataSep.currentTextChanged.connect(lambda text: self._settings.writeSetting('separator', text))

        self._ui.leStation.textChanged.connect(lambda text: self._settings.writeSetting('stationName', text))
        self._ui.leOwner.textChanged.connect(lambda text: self._settings.writeSetting('owner', text))
        self._ui.cbCountry.currentTextChanged.connect(lambda text: self._settings.writeSetting('country', text))
        self._ui.leCity.textChanged.connect(lambda text: self._settings.writeSetting('city', text))
        self._ui.leLatitude.textChanged.connect(lambda text: self._settings.writeSetting('latitude', text))
        self._ui.leLongitude.textChanged.connect(lambda text: self._settings.writeSetting('longitude', text))
        self._ui.leLatitudeDeg.textChanged.connect(lambda text: self._settings.writeSetting('latitudeDeg', text))
        self._ui.leLongitudeDeg.textChanged.connect(lambda text: self._settings.writeSetting('longitudeDeg', text))
        self._ui.sbAltitude.valueChanged.connect(lambda val: self._settings.writeSetting('altitude', val))
        self._ui.pbLogo.pressed.connect(self._selectLogo)

        self._ui.leAntenna.textChanged.connect(lambda text: self._settings.writeSetting('antenna', text))
        self._ui.sbAntennaAzimuth.valueChanged.connect(lambda val: self._settings.writeSetting('antAzimuth', val))
        self._ui.sbAntennaElevation.valueChanged.connect(lambda val: self._settings.writeSetting('antElevation', val))
        self._ui.lePre.textChanged.connect(lambda text: self._settings.writeSetting('preamplifier', text))
        self._ui.leFrequencies.textChanged.connect(lambda text: self._settings.writeSetting('frequencies', text))
        self._ui.leReceiver.textChanged.connect(lambda text: self._settings.writeSetting('receiver', text))
        self._ui.leComputer.textChanged.connect(lambda text: self._settings.writeSetting('computer', text))
        self._ui.leEmail.textChanged.connect(lambda text: self._settings.writeSetting('email', text))
        self._ui.pteNotes.textChanged.connect(
            lambda: self._settings.writeSetting('notes', self._ui.pteNotes.toPlainText()))

        self._ui.chkRFIfilter.clicked.connect(lambda checked: self._settings.writeSetting('RFIfilter', checked))
        self._ui.chkESDfilter.clicked.connect(lambda checked: self._settings.writeSetting('ESDfilter', checked))
        self._ui.chkSatFilter.clicked.connect(lambda checked: self._settings.writeSetting('SATfilter', checked))
        self._ui.chkCarrierFilter1.clicked.connect(lambda checked: self._settings.writeSetting('CAR1filter', checked))
        self._ui.chkCarrierFilter2.clicked.connect(lambda checked: self._settings.writeSetting('CAR2filter', checked))
        self._ui.sbRFIthreshold.valueChanged.connect(lambda val: self._settings.writeSetting('RFIfilterThreshold', val))
        self._ui.sbESDthreshold.valueChanged.connect(lambda val: self._settings.writeSetting('ESDfilterThreshold', val))
        self._ui.sbSATthreshold.valueChanged.connect(lambda val: self._settings.writeSetting('SATfilterThreshold', val))
        self._ui.sbCar1Threshold.valueChanged.connect(
            lambda val: self._settings.writeSetting('CAR1filterThreshold', val))
        self._ui.sbCar2Threshold.valueChanged.connect(
            lambda val: self._settings.writeSetting('CAR2filterThreshold', val))
        self._ui.sbUnderThr.valueChanged.connect(lambda val: self._settings.writeSetting('underdenseMs', val))
        self._ui.sbOverThr.valueChanged.connect(lambda val: self._settings.writeSetting('overdenseSec', val))
        self._ui.sbCarLasting.valueChanged.connect(lambda val: self._settings.writeSetting('carrierSec', val))
        self._ui.pbFindRMOBclient.pressed.connect(self._selectRMOBclient)

        self._ui.chkRefNman.clicked.connect(lambda checked: self._settings.writeSetting('enableRefN', checked))
        self._ui.sbRefN.valueChanged.connect(lambda val: self._settings.writeSetting('refNlevel', val))

        self._ui.pbAdd.pressed.connect(self._addDateInterval)
        self._ui.pbDel.pressed.connect(self._removeDateInterval)
        self._ui.pbCalc.pressed.connect(self._calculateSporadicBackground)
        self._ui.pbEditParms.pressed.connect(self._editAttributeParms)

        self.updateTabPrefs()

    def updateTabPrefs(self):
        enableSB = 0
        avgDailyStr = self._settings.readSettingAsObject('sporadicBackgroundDaily')
        if len(avgDailyStr) > 0:
            enableSB |= 1

        avgHourStr = self._settings.readSettingAsObject('sporadicBackgroundByHour')
        if len(avgHourStr) > 0:
            enableSB |= 2

        avg10minStr = self._settings.readSettingAsObject('sporadicBackgroundBy10min')
        if len(avg10minStr) > 0:
            enableSB |= 4

        self._ui.pbAdd.setEnabled(self._parent.dbOk)
        self._ui.pbDel.setEnabled(self._parent.dbOk)
        self._ui.pbCalc.setEnabled(self._parent.dbOk)
        self._ui.lbSBok.setVisible(enableSB == 7)

        group = QButtonGroup(self)
        group.buttonClicked.connect(self._openColorDialog)
        for i in range(0, len(self._colorKeys)):
            colorKey = self._colorKeys[i]
            colorName = self._colorDict[colorKey].name()
            ss = self._ssBase.format(colorName)
            button = self._colorButtons[i]
            button.setStyleSheet(ss)
            button.setWhatsThis(colorKey)
            group.addButton(button)

        self._ui.chkRFIfilter.setChecked(self._settings.readSettingAsBool('RFIfilter'))
        self._ui.chkESDfilter.setChecked(self._settings.readSettingAsBool('ESDfilter'))
        self._ui.chkSatFilter.setChecked(self._settings.readSettingAsBool('SATfilter'))
        self._ui.chkCarrierFilter1.setChecked(self._settings.readSettingAsBool('CAR1filter'))
        self._ui.chkCarrierFilter2.setChecked(self._settings.readSettingAsBool('CAR2filter'))
        self._ui.sbRFIthreshold.setValue(self._settings.readSettingAsFloat('RFIfilterThreshold'))
        self._ui.sbESDthreshold.setValue(self._settings.readSettingAsFloat('ESDfilterThreshold'))
        self._ui.sbSATthreshold.setValue(self._settings.readSettingAsFloat('SATfilterThreshold'))
        self._ui.sbCar1Threshold.setValue(self._settings.readSettingAsFloat('CAR1filterThreshold'))
        self._ui.sbCar2Threshold.setValue(self._settings.readSettingAsFloat('CAR2filterThreshold'))
        self._ui.sbUnderThr.setValue(self._settings.readSettingAsInt('underdenseMs'))
        self._ui.sbOverThr.setValue(self._settings.readSettingAsInt('overdenseSec'))
        self._ui.sbCarLasting.setValue(self._settings.readSettingAsInt('carrierSec'))

        self._ui.chkRefNman.setChecked(self._settings.readSettingAsBool('enableRefN'))
        self._ui.sbRefN.setValue(self._settings.readSettingAsFloat('refNlevel'))

        self._ui.cbFontFamily.setCurrentText(self._settings.readSettingAsString('fontFamily'))
        self._ui.sbFontSize.setValue(self._settings.readSettingAsInt('fontSize'))
        self._ui.cbFontStyle.setCurrentText(self._settings.readSettingAsString('fontStyle'))

        self._ui.cbDataLineStyle.setCurrentText(self._settings.readSettingAsString('dataLineStyle'))
        self._ui.cbMajorLineStyle.setCurrentText(self._settings.readSettingAsString('majorLineStyle'))
        self._ui.cbMinorLineStyle.setCurrentText(self._settings.readSettingAsString('minorLineStyle'))
        self._ui.sbDataLineWidth.setValue(self._settings.readSettingAsFloat('dataLineWidth'))
        self._ui.sbMajorLineWidth.setValue(self._settings.readSettingAsFloat('majorLineWidth'))
        self._ui.sbMinorLineWidth.setValue(self._settings.readSettingAsFloat('minorLineWidth'))

        self._ui.chkTooltips.setChecked(self._settings.readSettingAsBool('tooltipDisabled'))
        self._ui.chkAutosave.setChecked(self._settings.readSettingAsBool('autosaving'))
        self._ui.chkShowCursor.setChecked(self._settings.readSettingAsBool('cursorEnabled'))

        self._ui.cbDataSep.setCurrentText(self._settings.readSettingAsString('separator'))

        self._ui.leStation.setText(self._settings.readSettingAsString('stationName'))
        self._ui.leOwner.setText(self._settings.readSettingAsString('owner'))
        self._ui.cbCountry.setCurrentText(self._settings.readSettingAsString('country'))
        self._ui.leCity.setText(self._settings.readSettingAsString('city'))
        self._ui.leLatitude.setText(self._settings.readSettingAsString('latitude'))
        self._ui.leLongitude.setText(self._settings.readSettingAsString('longitude'))
        self._ui.leLatitudeDeg.setText(self._settings.readSettingAsString('latitudeDeg'))
        self._ui.leLongitudeDeg.setText(self._settings.readSettingAsString('longitudeDeg'))
        self._ui.sbAltitude.setValue(self._settings.readSettingAsInt('altitude'))
        logoIcon = QIcon(self._settings.readSettingAsString('logoPath'))
        self._ui.pbLogo.setText('')
        self._ui.pbLogo.setIcon(logoIcon)
        self._ui.pbLogo.setIconSize(self._ui.pbLogo.size())
        self._ui.leAntenna.setText(self._settings.readSettingAsString('antenna'))
        self._ui.sbAntennaAzimuth.setValue(self._settings.readSettingAsInt('antAzimuth'))
        self._ui.sbAntennaElevation.setValue(self._settings.readSettingAsInt('antElevation'))
        self._ui.lePre.setText(self._settings.readSettingAsString('preamplifier'))
        self._ui.leFrequencies.setText(self._settings.readSettingAsString('frequencies'))
        self._ui.leReceiver.setText(self._settings.readSettingAsString('receiver'))
        self._ui.leComputer.setText(self._settings.readSettingAsString('computer'))
        self._ui.leEmail.setText(self._settings.readSettingAsString('email'))
        self._ui.pteNotes.setPlainText(self._settings.readSettingAsString('notes'))
        self._RMOBclient = self._settings.readSettingAsString('RMOBclient')
        self._ui.lbRMOBclientPath.setText(self._RMOBclient)
        self._ui.lbRMOBclientPath.setToolTip(self._RMOBclient)

        sdList = self._settings.readSettingAsObject('sporadicDates')
        if len(sdList) > 0:
            self._sporadicDatesList = json.loads(sdList)
        # only for a new calculation, no need to retrieve actual values from settings
        self._sporadicBackgroundByHour = None
        self._sporadicBackgroundBy10min = None

        self._ui.lwSporadic.clear()
        if self._sporadicDatesList is not None:
            for intervalStr in self._sporadicDatesList:
                self._addDateInterval(intervalStr)
        else:
            self._sporadicDatesList = list()
        self._dataSource = self._parent.dataSource
        self._ui.pbCalc.setEnabled(self._dataSource is not None)

        # attribute filters configuration interface
        cwd = None
        if not self._attributeFilterNames:
            afList = list()
            cwd = os.path.dirname(__file__)
            pyFiles = glob(os.path.join(cwd, "*.py"))
            for name in pyFiles:
                name = os.path.basename(name)
                if name.startswith("af"):
                    name = name[2:-3]
                    print("adding filter: ", name)
                    afList.append(name)
            self._attributeFilterNames = afList

            self._ui.cbAttribSettings.clear()

            # the edb/ directory must NOT be added to sys.path, only
            # its parent ebrow/ to allow programmatic imports
            cwdList = os.path.split(cwd)
            baseDir = cwdList[0]
            sys.path.append(baseDir)
            model = QStandardItemModel(3, 1)  # 3 rows, 1 col
            for r in range(0, len(self._attributeFilterNames)):
                afClassName = self._attributeFilterNames[r]
                # instantiate the attribute filter classes
                afModuleName = "af" + afClassName
                afClass = getFromModule(afModuleName, afClassName)
                enabled = False
                if afClass is not None:
                    af = afClass(self._parent, self._ui, self._settings)
                    self._attributeFilterObject.append(af)
                    enabled = af.isFilterEnabled()

                if enabled:
                    self._attributeFilterEnabled.append(afClassName)
                item = QStandardItem(afClassName)
                model.setItem(r, 0, item)

            self._ui.cbAttribSettings.setModel(model)
            self._ui.cbAttribSettings.setCurrentIndex(0)
            if not self._attributeFilterObject or len(self._attributeFilterObject) == 0:
                self._ui.cbAttribSettings.setToolTip("Enabled filters: None")
                self._ui.pbEditParms.setEnabled(False)
            else:
                self._ui.cbAttribSettings.setToolTip("Enabled filters:" + '\n'.join(self._attributeFilterEnabled))
                self._ui.pbEditParms.setEnabled(True)
                self._attributeFilters[afClassName] = af

    def afDict(self):
        return self._attributeFilters

    def _addDateInterval(self, intervalStr: str = None):
        if intervalStr is None:
            did = DateIntervalDialog(self._ui)
            did.exec()
            diTuple = did.getInterval()
            intervalStr = diTuple[0].isoformat() + " -> " + diTuple[1].isoformat()
            self._sporadicDatesList.append(intervalStr)
            print("Adding date interval:", intervalStr)
            sdList = json.dumps(self._sporadicDatesList)
            self._settings.writeSetting('sporadicDates', sdList)

        self._ui.lwSporadic.addItem(intervalStr)

    def _removeDateInterval(self):
        item = self._ui.lwSporadic.currentItem()
        if item:
            intervalStr = item.text()
            print("Removing date interval:", intervalStr)
            row = self._ui.lwSporadic.row(item)
            self._ui.lwSporadic.takeItem(row)
            self._sporadicDatesList.remove(intervalStr)
            sdList = json.dumps(self._sporadicDatesList)
            self._settings.writeSetting('sporadicDates', sdList)

    def _calculateSporadicBackground(self):
        self._parent.updateStatusBar("Sporadic background calculation...")
        done = self._tabStats.calculateSporadicBackground()
        if done:
            self.updateTabPrefs()

    def _openColorDialog(self, button):
        print("clicked button#", button)
        color = QColorDialog.getColor()
        if color.isValid():
            button.setStyleSheet(self._ssBase.format(color.name()))
            colorKey = button.whatsThis()
            self._colorDict[colorKey] = QColor(color.name())

    def _selectLogo(self):
        logoTuple = QFileDialog.getOpenFileName(
            self, "Open logo image", '*', "logo images (*.png *.jpeg *.jpg)", '')

        logoPath = logoTuple[0]
        if logoPath is None:
            return

        logo = QPixmap(logoPath)
        logoIcon = QIcon(logo)
        self._ui.pbLogo.setText('')
        self._ui.pbLogo.setIcon(logoIcon)
        self._ui.pbLogo.setIconSize(self._ui.pbLogo.size())
        self._settings.writeSetting('logoPath', logoPath)

    def _selectRMOBclient(self):
        if os.name == 'nt':
            clientTuple = QFileDialog.getOpenFileName(
                self, "Open RMOB FTP client executable file", '*', "executable files(*.exe *.EXE)", '')
        else:
            clientTuple = QFileDialog.getOpenFileName(
                self, "Open RMOB FTP client executable file", '*', '', '')

        self._RMOBclient = clientTuple[0]
        self._ui.lbRMOBclientPath.setText(self._RMOBclient)
        self._ui.lbRMOBclientPath.setToolTip(self._RMOBclient)
        self._settings.writeSetting('RMOBclient', self._RMOBclient)

    def _loadCbCountry(self):
        countryDict = {
            "AF": "Afghanistan",
            "AL": "Albania",
            "DZ": "Algeria",
            "AS": "American Samoa",
            "AD": "Andorra",
            "AO": "Angola",
            "AI": "Anguilla",
            "AQ": "Antarctica",
            "AG": "Antigua And Barbuda",
            "AR": "Argentina",
            "AM": "Armenia",
            "AW": "Aruba",
            "AU": "Australia",
            "AT": "Austria",
            "AZ": "Azerbaijan",
            "BS": "Bahamas",
            "BH": "Bahrain",
            "BD": "Bangladesh",
            "BB": "Barbados",
            "BY": "Belarus",
            "BE": "Belgium",
            "BZ": "Belize",
            "BJ": "Benin",
            "BM": "Bermuda",
            "BT": "Bhutan",
            "BO": "Bolivia",
            "BA": "Bosnia And Herzegovina",
            "BW": "Botswana",
            "BV": "Bouvet Island",
            "BR": "Brazil",
            "IO": "British Indian Ocean Territory",
            "BN": "Brunei Darussalam",
            "BG": "Bulgaria",
            "BF": "Burkina Faso",
            "BI": "Burundi",
            "KH": "Cambodia",
            "CM": "Cameroon",
            "CA": "Canada",
            "CV": "Cape Verde",
            "KY": "Cayman Islands",
            "CF": "Central African Republic",
            "TD": "Chad",
            "CL": "Chile",
            "CN": "China",
            "CX": "Christmas Island",
            "CC": "Cocos (keeling) Islands",
            "CO": "Colombia",
            "KM": "Comoros",
            "CG": "Congo",
            "CD": "Congo, The Democratic Republic Of The",
            "CK": "Cook Islands",
            "CR": "Costa Rica",
            "CI": "Cote D'ivoire",
            "HR": "Croatia",
            "CU": "Cuba",
            "CY": "Cyprus",
            "CZ": "Czech Republic",
            "DK": "Denmark",
            "DJ": "Djibouti",
            "DM": "Dominica",
            "DO": "Dominican Republic",
            "TP": "East Timor",
            "EC": "Ecuador",
            "EG": "Egypt",
            "SV": "El Salvador",
            "GQ": "Equatorial Guinea",
            "ER": "Eritrea",
            "EE": "Estonia",
            "ET": "Ethiopia",
            "FK": "Falkland Islands (malvinas)",
            "FO": "Faroe Islands",
            "FJ": "Fiji",
            "FI": "Finland",
            "FR": "France",
            "GF": "French Guiana",
            "PF": "French Polynesia",
            "TF": "French Southern Territories",
            "GA": "Gabon",
            "GM": "Gambia",
            "GE": "Georgia",
            "DE": "Germany",
            "GH": "Ghana",
            "GI": "Gibraltar",
            "GR": "Greece",
            "GL": "Greenland",
            "GD": "Grenada",
            "GP": "Guadeloupe",
            "GU": "Guam",
            "GT": "Guatemala",
            "GN": "Guinea",
            "GW": "Guinea-bissau",
            "GY": "Guyana",
            "HT": "Haiti",
            "HM": "Heard Island And Mcdonald Islands",
            "VA": "Holy See (vatican City State)",
            "HN": "Honduras",
            "HK": "Hong Kong",
            "HU": "Hungary",
            "IS": "Iceland",
            "IN": "India",
            "ID": "Indonesia",
            "IR": "Iran, Islamic Republic Of",
            "IQ": "Iraq",
            "IE": "Ireland",
            "IL": "Israel",
            "IT": "Italy",
            "JM": "Jamaica",
            "JP": "Japan",
            "JO": "Jordan",
            "KZ": "Kazakstan",
            "KE": "Kenya",
            "KI": "Kiribati",
            "KP": "Korea, Democratic People's Republic Of",
            "KR": "Korea, Republic Of",
            "KV": "Kosovo",
            "KW": "Kuwait",
            "KG": "Kyrgyzstan",
            "LA": "Lao People's Democratic Republic",
            "LV": "Latvia",
            "LB": "Lebanon",
            "LS": "Lesotho",
            "LR": "Liberia",
            "LY": "Libyan Arab Jamahiriya",
            "LI": "Liechtenstein",
            "LT": "Lithuania",
            "LU": "Luxembourg",
            "MO": "Macau",
            "MK": "Macedonia, The Former Yugoslav Republic Of",
            "MG": "Madagascar",
            "MW": "Malawi",
            "MY": "Malaysia",
            "MV": "Maldives",
            "ML": "Mali",
            "MT": "Malta",
            "MH": "Marshall Islands",
            "MQ": "Martinique",
            "MR": "Mauritania",
            "MU": "Mauritius",
            "YT": "Mayotte",
            "MX": "Mexico",
            "FM": "Micronesia, Federated States Of",
            "MD": "Moldova, Republic Of",
            "MC": "Monaco",
            "MN": "Mongolia",
            "MS": "Montserrat",
            "ME": "Montenegro",
            "MA": "Morocco",
            "MZ": "Mozambique",
            "MM": "Myanmar",
            "NA": "Namibia",
            "NR": "Nauru",
            "NP": "Nepal",
            "NL": "Netherlands",
            "AN": "Netherlands Antilles",
            "NC": "New Caledonia",
            "NZ": "New Zealand",
            "NI": "Nicaragua",
            "NE": "Niger",
            "NG": "Nigeria",
            "NU": "Niue",
            "NF": "Norfolk Island",
            "MP": "Northern Mariana Islands",
            "NO": "Norway",
            "OM": "Oman",
            "PK": "Pakistan",
            "PW": "Palau",
            "PS": "Palestinian Territory, Occupied",
            "PA": "Panama",
            "PG": "Papua New Guinea",
            "PY": "Paraguay",
            "PE": "Peru",
            "PH": "Philippines",
            "PN": "Pitcairn",
            "PL": "Poland",
            "PT": "Portugal",
            "PR": "Puerto Rico",
            "QA": "Qatar",
            "RE": "Reunion",
            "RO": "Romania",
            "RU": "Russian Federation",
            "RW": "Rwanda",
            "SH": "Saint Helena",
            "KN": "Saint Kitts And Nevis",
            "LC": "Saint Lucia",
            "PM": "Saint Pierre And Miquelon",
            "VC": "Saint Vincent And The Grenadines",
            "WS": "Samoa",
            "SM": "San Marino",
            "ST": "Sao Tome And Principe",
            "SA": "Saudi Arabia",
            "SN": "Senegal",
            "RS": "Serbia",
            "SC": "Seychelles",
            "SL": "Sierra Leone",
            "SG": "Singapore",
            "SK": "Slovakia",
            "SI": "Slovenia",
            "SB": "Solomon Islands",
            "SO": "Somalia",
            "ZA": "South Africa",
            "GS": "South Georgia And The South Sandwich Islands",
            "ES": "Spain",
            "LK": "Sri Lanka",
            "SD": "Sudan",
            "SR": "Suriname",
            "SJ": "Svalbard And Jan Mayen",
            "SZ": "Swaziland",
            "SE": "Sweden",
            "CH": "Switzerland",
            "SY": "Syrian Arab Republic",
            "TW": "Taiwan, Province Of China",
            "TJ": "Tajikistan",
            "TZ": "Tanzania, United Republic Of",
            "TH": "Thailand",
            "TG": "Togo",
            "TK": "Tokelau",
            "TO": "Tonga",
            "TT": "Trinidad And Tobago",
            "TN": "Tunisia",
            "TR": "Turkey",
            "TM": "Turkmenistan",
            "TC": "Turks And Caicos Islands",
            "TV": "Tuvalu",
            "UG": "Uganda",
            "UA": "Ukraine",
            "AE": "United Arab Emirates",
            "GB": "United Kingdom",
            "US": "United States",
            "UM": "United States Minor Outlying Islands",
            "UY": "Uruguay",
            "UZ": "Uzbekistan",
            "VU": "Vanuatu",
            "VE": "Venezuela",
            "VN": "Viet Nam",
            "VG": "Virgin Islands, British",
            "VI": "Virgin Islands, U.s.",
            "WF": "Wallis And Futuna",
            "EH": "Western Sahara",
            "YE": "Yemen",
            "ZM": "Zambia",
            "ZW": "Zimbabwe"
        }
        self._ui.cbCountry.insertItems(0, countryDict.values())

    def _editAttributeParms(self):
        idx = self._ui.cbAttribSettings.currentIndex()
        afClassName = self._attributeFilterNames[idx]
        print("Editing {} attribute filter parameters".format(afClassName))
        af = self._attributeFilterObject[idx]
        af.getParameters()
        try:
            self._attributeFilterEnabled.remove(afClassName)
        except ValueError:
            pass

        if af.isFilterEnabled():
            self._attributeFilterEnabled.append(afClassName)

        if not self._attributeFilterEnabled:
            self._ui.cbAttribSettings.setToolTip("Enabled filters: None")
        else:
            self._ui.cbAttribSettings.setToolTip("Enabled filters:" + '\n'.join(self._attributeFilterEnabled))
