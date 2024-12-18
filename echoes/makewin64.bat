REM creates the Echoes installation packages for Windows 64bit
@ECHO OFF

REM WARNING: The Soapy (https://github.com/pothosware/SoapySDR/wiki) libs and drivers binaries
REM compiled with MinGW must be already installed under deps/ before running 
REM this batch

REM ----- CUSTOMIZE HERE ---------------------------------------------

SET mingwroot=d:\Qt\Tools\mingw810_64\bin
SET make=%mingwroot%\mingw32-make.exe
SET strip=%mingwroot%\strip.exe

SET qtroot=d:\Qt\5.15.2\mingw81_64\bin
SET qmake=%qtroot%\qmake.exe
SET deploy=%qtroot%\windeployqt64releaseonly.exe
SET qtenv=%qtroot%\qtenv2.bat

SET nsispath=D:\"Program Files (x86)"\NSIS
SET nsis=%nsispath%\makensisw.exe

SET APP_VERSION=0.58

REM ----- CUSTOMIZE END ---------------------------------------------

CD > trash.me
SET /P SRCDIR= < trash.me
PUSHD .
CALL %qtenv%
POPD
@ECHO ON
DEL /Q trash.me
RMDIR /S /Q NSIS\win64
SET dist=echoes-%APP_VERSION% 
DATE /T > trash.me
SET /P bDate= < trash.me
REM TIME /T > trash.me
REM SET /P bTime= < trash.me
SET BUILD_DATE=%bDate%
%qmake% echoes.pro -r APP_VERSION=%APP_VERSION% BUILD_DATE=%BUILD_DATE% 
%make% -fMakefile.Release clean 
%make% -fMakefile.Release 
IF %ERRORLEVEL% NEQ 0 ( 
	ECHO "Building failed, no package created"
	PAUSE
	EXIT -1
)
%strip% release\echoes.exe
MKDIR NSIS\win64
COPY release\echoes.exe NSIS\win64
%deploy% NSIS\win64 --release --no-patchqt --no-opengl --compiler-runtime -core -multimedia -network -widgets NSIS\win64\echoes.exe
START %nsis% "NSIS\setupEchoes64.nsi"
IF %ERRORLEVEL% NEQ 0 ( 
	ECHO "Building failed, no package created"
	PAUSE
	EXIT -1
)
ECHO "Package created successfully"
PAUSE
