REM creates the Echoes installation packages for Windows 32bit
@ECHO ON

REM WARNING: The Soapy (https://github.com/pothosware/SoapySDR/wiki) libs and drivers binaries
REM compiled with MinGW must be already installed under deps/ before running 
REM this batch

REM ----- CUSTOMIZE HERE ---------------------------------------------
SET qmake=C:\Qt\5.15.2\mingw81_32\bin\qmake.exe
SET nsispath=C:\"Program Files"\NSIS
SET make=C:\Qt\Tools\mingw810_32\bin\mingw32-make.exe
SET strip==C:\Qt\Tools\mingw810_32\bin\strip.exe
SET deploy=c:\Qt\Tools\mingw810_32\bin\\windeployqtreleaseonly.exe
SET qtenv=C:\Qt\5.15.2\mingw81_32\bin\qtenv2.bat
SET nsis=%nsispath%\makensisw.exe
SET APP_VERSION=0.51

REM ----- CUSTOMIZE END ---------------------------------------------

CD > trash.me
SET /P SRCDIR= < trash.me
PUSHD .
CALL %qtenv%
POPD
@ECHO ON
DEL /Q trash.me
RMDIR /S /Q NSIS\win32
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
MKDIR NSIS\win32
COPY release\echoes.exe NSIS\win32
%deploy% NSIS\win32 --release --no-patchqt --no-opengl --compiler-runtime -core -multimedia -network -widgets NSIS\win32\echoes.exe
START %nsis% "NSIS\setupEchoes32.nsi"
IF %ERRORLEVEL% NEQ 0 ( 
	ECHO "Building failed, no package created"
	PAUSE
	EXIT -1
)
ECHO "Package created successfully"
PAUSE
