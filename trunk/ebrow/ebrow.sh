#!/bin/sh

#starts Echoes Data Browser after compiling the ui files
echo "Activating venv"
source venv/bin/activate
echo "generating UI and resources files"
cd edb
pyuic5 mainwindow.ui -o ui_mainwindow.py --import-from=.
pyuic5 dateintervaldialog.ui -o ui_dateintervaldialog.py --import-from=.
pyuic5 afDummy.ui -o ui_afdummy.py --import-from=.
pyuic5 afHasHead.ui -o ui_afhashead.py --import-from=.
pyrcc5 ebrow.qrc -o ebrow_rc.py
cd .. 
echo "Starting program"
python3 -m edb.main --multiple --verbose --rmob


