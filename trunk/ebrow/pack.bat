@echo on

rem the package version is stored in pyproject.toml
rem mainwindow/APPVERSION does not include the third digit

rem the testPyPi token must be passed as argument
set PASSWORD=%1

del /Q /S packaging\ebrow\*.*
mkdir packaging\ebrow\src\ebrow
copy  /Y deploy.bat packaging\ebrow
xcopy  /I  /Y edb packaging\ebrow\src\ebrow\edb
xcopy  /I  /Y pyproject.toml packaging\ebrow
copy  /Y README.md packaging\ebrow
copy  /Y README.md packaging\ebrow\src\ebrow
copy  /Y setup.py packaging\ebrow
copy  /Y LICENSE.txt packaging\ebrow

copy  /Y edb\resources\icons\LICENSES packaging\ebrow\src\ebrow\edb\resources\icons
xcopy  /I  /Y edb\assets\css\*.css packaging\ebrow\src\ebrow\edb\assets\css
xcopy  /I  /Y edb\assets\img\*.* packaging\ebrow\src\ebrow\edb\assets\img
xcopy  /I  /Y edb\resources\icons\*.ico packaging\ebrow\src\ebrow\edb\resources\icons
xcopy  /I  /Y edb\resources\icons\*.png packaging\ebrow\src\ebrow\edb\resources\icons
xcopy  /I  /Y edb\resources\icons\*.xpm packaging\ebrow\src\ebrow\edb\resources\icons
xcopy  /I  /Y edb\resources\colormaps\*.png packaging\ebrow\src\ebrow\edb\resources\colormaps
xcopy  /I  /Y edb\resources\fonts\*.ttf packaging\ebrow\src\ebrow\edb\resources\fonts 
xcopy  /I  /Y edb\resources\fonts\*.otf packaging\ebrow\src\ebrow\edb\resources\fonts 
xcopy  /I  /Y edb\resources\fonts\*.woff packaging\ebrow\src\ebrow\edb\resources\fonts 
xcopy  /I  /Y edb\resources\fonts\LICENCE.txt packaging\ebrow\src\ebrow\edb\resources\fonts 

cd packaging\ebrow
call deploy.bat %PASSWORD%
cd ..\..




