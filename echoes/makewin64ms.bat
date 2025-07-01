REM creates the Echoes installation packages for Windows 64bit
@ECHO OFF

REM WARNING: The PothosSDR libraries and drivers binaries compiled with Msvc must be already installed  
REM this batch

REM ----- CUSTOMIZE HERE ---------------------------------------------
SET qmake=\Qt\5.15.2\msvc2019_64\bin\qmake.exe
SET nsispath=\"Program Files (x86)"\NSIS
REM SET make=\"Program Files (x86)"\"Microsoft Visual Studio"\2019\Community\VC\Tools\MSVC\14.28.29910\bin\Hostx64\x64\nmake.exe
SET make=\"Program Files (x86)"\"Microsoft Visual Studio"\2019\Community\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\nmake.exe
SET deploy=\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe
SET qtenv=\Qt\5.15.2\msvc2019_64\bin\qtenv2.bat
SET vcvarsall=\"program files (x86)\Microsoft Visual Studio"\2019\Community\VC\Auxiliary\Build\vcvarsall amd64
SET nsis=%nsispath%\makensisw.exe
SET APP_VERSION=0.61

REM ----- CUSTOMIZE END ---------------------------------------------

DEL /Q .qmake.stash
CD > trash.me
SET /P SRCDIR= < trash.me
PUSHD .
CALL %qtenv%
CALL %vcvarsall%
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

MKDIR NSIS\win64
COPY release\echoes.exe NSIS\win64
%deploy% NSIS\win64 --release --no-patchqt --no-opengl --compiler-runtime -core -multimedia -network -widgets NSIS\win64\echoes.exe

START %nsis% "NSIS\setupEchoes64full.nsi"
START %nsis% "NSIS\setupEchoes64ms.nsi"

IF %ERRORLEVEL% NEQ 0 ( 
	ECHO "Building failed, no package created"
	PAUSE
	EXIT -1
)
ECHO "Package created successfully"
PAUSE
:end