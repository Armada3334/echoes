REM building package
rem the testPyPi token must be passed as argument
set PASSWORD=%1
del /S /Q dist\*.*
python -m build 

REM loading packages on TestPyPi repo
python -m twine upload -u __token__ -p%PASSWORD% --verbose --repository pypi dist\*

