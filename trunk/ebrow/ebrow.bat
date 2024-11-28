REM starts Echoes Data Browser after compiling the ui files
REM remember to activate the right Venv first. I have 2 on 2 PCs
REM so here I try to start both - one fails sure, the other shouldn't
call py\Scripts\activate.bat
call venv311\Scripts\activate.bat

cd edb
pyuic5 mainwindow.ui -o ui_mainwindow.py --import-from=.
pyuic5 dateintervaldialog.ui -o ui_dateintervaldialog.py --import-from=.
pyuic5 afDummy.ui -o ui_afdummy.py --import-from=.
pyuic5 afHasHead.ui -o ui_afhashead.py --import-from=.
pyrcc5 ebrow.qrc -o ebrow_rc.py
cd ..
python -m edb.main --verbose 

