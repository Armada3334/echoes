#NSIS 2.3 script file for Echoes
#(C)GMB 18JAN23

#Unicode True

!include "MUI2.nsh"

#base directory is 2 levels up from where this file is placed
#it should match the echoes-git/trunk directory
!define BASEDIR      "..\.."
!define HOMEDIR     "$DOCUMENTS\..\${APPNAME}"
!define EBROWDIR    "${BASEDIR}\ebrow"
# !define ASSETSDIR   "${EBROWDIR}\edb\assets"
!define RMOBDIR     "${BASEDIR}\..\..\RMOBclient"

!define APPNAME     "echoes"
!define EXENAME     "${APPNAME}.exe"

!define PLATFORM    "Win64"
!define COMPANYNAME "GABB"
!define DESCRIPTION "SoapySDR-based RF spectrograph for meteor scatter"
!define INSTALLEREXE "Install-${APPNAME}"
!define SRCDIR      "${BASEDIR}\echoes"
!define EXEDIR      "${SRCDIR}\NSIS\win64"  #<--at the end of deploying, the exe and dlls are here
!define DOCSDIR     "${SRCDIR}\docs"

!define COMPILER_MSVC 1
# !define COMPILER_MINGW 1


!ifdef COMPILER_MSVC
#Soapy library and drivers from Pothosware distribution, built with MSVC
!define SOAPYBINDIR "${PROGRAMFILES64}\PothosSDR\bin"
!define SOAPYMODDIR "${PROGRAMFILES64}\PothosSDR\lib\SoapySDR\modules0.8"
!endif

!ifdef COMPILER_MINGW
#Soapy library and drivers compiled with MinGW. The public repo is here:
#https://github.com/gmbertani/Soapy
#WARNING: these DLLs cannot be replaced by official Pothosware Soapy binaries, that are compiled with MSVC.
# !define SOAPYBINDIR "${SRCDIR}\deps\x86_64\Soapy\bin"
# !define SOAPYMODDIR "${SRCDIR}\deps\x86_64\Soapy\lib\SoapySDR\modules0.8"
# !define SOAPYINSTALLDIR "$INSTDIR\Soapy\modules0.8"
!endif

#auxiliary files and libraries
!define LICENSE     "${DOCSDIR}\LICENSE.txt"

!define SWVERSION $%APP_VERSION%


# These will be displayed by the "Click here for support information" link in "Add/Remove Programs"
# It is possible to use "mailto:" links in here to open the email client
!define HELPURL "mailto://gm_bertani@yahoo.it" # "Support Information" link
!define UPDATEURL "https://sourceforge.net/projects/echoes" # "Product Updates" link
!define ABOUTURL "https://sourceforge.net/projects/echoes" # "Publisher" link

#For UI Modern2
!define MUI_ICON "${SRCDIR}\icons\echoes_icon128.ico"

#installer logfile
!define LOGFILE "$DOCUMENTS\echoes-install.log"
#uninstaller logfile
!define UNLOGFILE "$DOCUMENTS\echoes-uninstall.log"



;--------------------------------
;General
    
Name "${COMPANYNAME} ${APPNAME} v.${SWVERSION}"

OutFile "${INSTALLEREXE}-${SWVERSION}-bundle-${PLATFORM}.exe"
Icon "${SRCDIR}\icons\${APPNAME}.ico"


;Default installation folder
InstallDir "$PROGRAMFILES64\${COMPANYNAME}\${APPNAME}"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "InstallLocation" 


# rtf or txt file - remember if it is txt, it must be in the DOS text format (\r\n)
LicenseData "${LICENSE}"

RequestExecutionLevel admin ;Require admin rights on NT6+ (When UAC is turned on)

ShowInstDetails show
ShowUninstDetails show

;--------------------------------
;Interface Settings

!define MUI_ABORTWARNING

;--------------------------------

!include WinCore.nsh
!include winmessages.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include FontRegAdv.nsh
!include FontName.nsh

#GMB: nothing to include for inetc?

;--------------------------------
;pages

#mandatory
!define MUI_WELCOMEPAGE_TITLE "${DESCRIPTION}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!define MUI_CONFIRMPAGE_TITLE "${DESCRIPTION}" 
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE "${DESCRIPTION}" 
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Version infos

VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "Comments" "alpha quality software"
VIAddVersionKey "CompanyName" "GABB"
VIAddVersionKey "LegalTrademarks" "-"
VIAddVersionKey "LegalCopyright" "(C)2018-2023 Giuseppe Massimo Bertani"
VIAddVersionKey "FileDescription" "${DESCRIPTION}"
VIAddVersionKey "FileVersion" "${SWVERSION}"


!ifdef COMPILER_MSVC
VIAddVersionKey "Compiler" "MSVC"
!endif

!ifdef COMPILER_MINGW
VIAddVersionKey "Compiler" "MINGW"
!endif



VIProductVersion "${SWVERSION}.0.0"

;--------------------------------

#macros

!macro VerifyUserIsAdmin
UserInfo::GetAccountType
pop $0
${If} $0 != "admin" ;Require admin rights on NT4+
        messageBox mb_iconstop "Administrator rights required!"
        setErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
        quit
${EndIf}
!macroend


 
!macro UninstallExisting exitcode uninstcommand
    Push `${uninstcommand}`
    Call UninstallExisting
    Pop ${exitcode}
!macroend

 
#----------------------------------------

#functions

function .onInit
    
    #Avoids multiple instances
    System::Call 'kernel32::CreateMutexA(i 0, i 0, t "installMutex") i .r1 ?e'
        Pop $R0
     
    StrCmp $R0 0 +3
        MessageBox MB_OK|MB_ICONEXCLAMATION "The installer is already running."
        Abort

    #Check for admin privileges
    setShellVarContext all
    !insertmacro VerifyUserIsAdmin

    #Check if already installed and its version, eventually asking for removal
    ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "UninstallString"
    ReadRegStr $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "SWVersion"
    ${If} $0 != ""
    ${AndIf} ${Cmd} `MessageBox MB_YESNO|MB_ICONQUESTION "Are you sure that you want remove ${APPNAME} $1?" /SD IDYES IDYES`
        !insertmacro UninstallExisting $0 $0
        ${If} $0 <> 0
            MessageBox MB_YESNO|MB_ICONSTOP "Failed to uninstall, continue anyway?" /SD IDYES IDYES +2
                Abort
        ${EndIf}
    ${EndIf}    
functionEnd
 

#----

Function UninstallExisting
    Exch $1 ; uninstcommand
    Push $2 ; Uninstaller
    Push $3 ; Len
    StrCpy $3 ""
    StrCpy $2 $1 1
    StrCmp $2 '"' qloop sloop
    sloop:
        StrCpy $2 $1 1 $3
        IntOp $3 $3 + 1
        StrCmp $2 "" +2
        StrCmp $2 ' ' 0 sloop
        IntOp $3 $3 - 1
        Goto run
    qloop:
        StrCmp $3 "" 0 +2
        StrCpy $1 $1 "" 1 ; Remove initial quote
        IntOp $3 $3 + 1
        StrCpy $2 $1 1 $3
        StrCmp $2 "" +2
        StrCmp $2 '"' 0 qloop
    run:
        StrCpy $2 $1 $3 ; Path to uninstaller
        StrCpy $1 161 ; ERROR_BAD_PATHNAME
        GetFullPathName $3 "$2\.." ; $InstDir
        IfFileExists "$2" 0 +4
        ExecWait '"$2" /S _?=$3' $1 ; This assumes the existing uninstaller is a NSIS uninstaller, other uninstallers don't support /S nor _?=
        IntCmp $1 0 "" +2 +2 ; Don't delete the installer if it was aborted
        Delete "$2" ; Delete the uninstaller
        RMDir "$3" ; Try to delete $InstDir
        RMDir "$3\.." ; (Optional) Try to delete the parent of $InstDir
    Pop $3
    Pop $2
    Exch $1 ; exitcode
FunctionEnd

#-------------------------------------------------------

Function InstallFonts
    StrCpy $FONT_DIR $FONTS	
	#ANSI only. If the fonts are already present, a "Invalid handle 32" messagebox appears while installing.
	IfFileExists $FONTS\Gauge-Regular.ttf skip1
		DetailPrint "Installing font Gauge-Regular.ttf"
		!insertMacro InstallTTF "${SRCDIR}\fonts\Gauge-Regular.ttf"
skip1:
	IfFileExists $FONTS\Gauge-Oblique.ttf skip2
		DetailPrint "Installing font Gauge-Oblique.ttf"
		!insertMacro InstallTTF "${SRCDIR}\fonts\Gauge-Oblique.ttf"
skip2:
	IfFileExists $FONTS\Gauge-Heavy.ttf skip3
		DetailPrint "Installing font Gauge-Heavy.ttf"
		!insertMacro InstallTTF "${SRCDIR}\fonts\Gauge-Heavy.ttf"
skip3:
    SendMessage ${HWND_BROADCAST} ${WM_FONTCHANGE} 0 0 /TIMEOUT=5000
FunctionEnd


#-------------------------------
section "install"

    SetShellVarContext current

# prerequisites: Zadig Winusb.dll must be installed 
    SearchPath $0 "WinUSB.dll"
    StrCmp $0 "" noWinUSB okWinUSB

noWinUSB:
    DetailPrint "WinUSB.dll not found"
    Abort "WinUSB.dll not found$\nplease install Zadig drivers$\n(http://zadig.akeo.ie/) first and retry afterwards."

okWinUSB:
    DetailPrint "WinUSB.dll found in $0"
    
# prerequisites: PothosSDR must be installed 
    SearchPath $0 "SoapySDR.dll"
    StrCmp $0 "" noSoapySDR okSoapySDR

noSoapySDR:
    DetailPrint "SoapySDR.dll not found, PothosSDR is not installed"
    Abort "SoapySDR.dll not found$\nplease install the latest package from here:$\n(https://downloads.myriadrf.org/builds/PothosSDR/) first and retry afterwards.\n \
WARNING for RTLSDR users: for some reason, the latest package causes frequent crashes after stopping acquisition. If this happens to you, try with the 28.12.2020 package."

okSoapySDR:
    DetailPrint "SoapySDR.dll found in $0"
	
    # Files for the install directory - to build the installer, these should be in the same directory as the install script (this file)
    setOutPath $INSTDIR

    # Files added here should be removed by the uninstaller (see section "uninstall")

    DetailPrint "Installing Echoes files..."
    
    file "${SRCDIR}\icons\echoes_icon128.ico"
    file "${SRCDIR}\icons\echoes_icon64.ico"
    file "${SRCDIR}\icons\echoes_icon32.ico"
    file "${SRCDIR}\icons\echoes_icon16.ico"
    file "${SRCDIR}\icons\echoes_logo.png"
    file "${SRCDIR}\sounds\ping.wav"
    file "${SRCDIR}\fonts\*.ttf"   
    file "${SRCDIR}\langs\*.qm"
    file "${SRCDIR}\*.qss"
    
    file "${DOCSDIR}\${LICENSE}"
    file "${DOCSDIR}\*.txt"
    file "${DOCSDIR}\tests\*.rts"
    file "${DOCSDIR}\*.bat"
    
    file "${EXEDIR}\${EXENAME}"
    file /r "${EXEDIR}\*.dll"
    
    # The FULL installer includes RMOBclient and Ebrow and its link for desktop
    file "${RMOBDIR}\src\release\RMOBclient.exe"
    file "${EBROWDIR}\dist\ebrow.exe"
    file "${SRCDIR}\icons\ebrow_icon32.ico"
    file "${SRCDIR}\icons\ebrow_icon16.ico"
   
    # libliquid is already present in PothosSDR but Echoes needs version 1.3.2++
	# regardless PothosSDR version
	DetailPrint "Installing liquidSDR library"   
    file "${SRCDIR}\deps\x86_64\libliquid.dll"
    
    DetailPrint "Installing database model"
    file "${SRCDIR}\deps\db\echoes.sqlite3"
    
!ifdef COMPILER_MINGW
    DetailPrint "Installing Soapy library and utilities"
        
    #Soapy library, SDR specific and proprietary libraries 
    file "${SOAPYBINDIR}\*.dll"
 
    #Soapy and SDR specific utilities
    file "${SOAPYBINDIR}\*.exe"
         
    #support libraries
    DetailPrint "Installing Soapy support libraries"
    setOutPath ${SOAPYINSTALLDIR}
    file "${SOAPYMODDIR}\libaudioSupport.dll"
    file "${SOAPYMODDIR}\libremoteSupport.dll"
    file "${SOAPYMODDIR}\librtlsdrSupport.dll"
    file "${SOAPYMODDIR}\libairspySupport.dll"
!endif
	
    SetOutPath "$INSTDIR\assets\css" 
	DetailPrint "Installing ebrow report assets from ${EBROWDIR}\edb\assets\css"
	file "${EBROWDIR}\edb\assets\css\*"
	
	SetOutPath "$INSTDIR\assets\img" 
	DetailPrint "Installing ebrow report assets from ${EBROWDIR}\edb\assets\img"
	file "${EBROWDIR}\edb\assets\img\*"

	SetOutPath "$INSTDIR\assets\js" 
	DetailPrint "Installing ebrow report assets from ${EBROWDIR}\edb\assets\js"
	file /nonfatal "${EBROWDIR}\edb\assets\js\*"
    
    SetOutPath "$INSTDIR"              
    #Copy fonts under \Windows\Fonts
    DetailPrint "Installing fonts..."
    Call InstallFonts
  
    # Uninstaller - See function un.onInit and section "uninstall" for configuration
    DetailPrint "Creating uninstaller..."
    writeUninstaller "$INSTDIR\uninstall.exe"

    # Start/Programs Menu

    DetailPrint "Creating Start menu link..."
    createDirectory "$SMPROGRAMS\${COMPANYNAME}"
    createShortCut "$SMPROGRAMS\${COMPANYNAME}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\echoes_icon16.ico"
    createShortCut "$SMPROGRAMS\${COMPANYNAME}\ebrow.lnk" "$INSTDIR\ebrow.exe" "" "$INSTDIR\ebrow_icon16.ico"
    createShortCut "$SMPROGRAMS\${COMPANYNAME}\uninstall.lnk" "$INSTDIR\uninstall.exe"
    # Start/Programs/Startup Menu

    DetailPrint "Creating desktop link..."
    createShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\echoes_icon32.ico"
    createShortCut "$DESKTOP\ebrow.lnk" "$INSTDIR\ebrow.exe" "" "$INSTDIR\ebrow_icon32.ico"
 
    # Registry information for add/remove programs
    DetailPrint "Insert registry keys..."
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "DisplayName" "${COMPANYNAME} - ${APPNAME} - ${DESCRIPTION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "InstallLocation" "$\"$INSTDIR$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "DisplayIcon" "$\"$INSTDIR\echoes_icon32.ico$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "Publisher" "$\"${COMPANYNAME}$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "HelpLink" "$\"${HELPURL}$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "URLUpdateInfo" "$\"${UPDATEURL}$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "URLInfoAbout" "$\"${ABOUTURL}$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "SWVersion" "$\"${SWVERSION}$\""
    
    # There is no option for modifying or repairing the install
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "NoRepair" 1

    # HKLM (all users) vs HKCU (current user) defines
    !define env_hklm 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'

!ifdef COMPILER_MINGW
    #setting SOAPY_SDR_PLUGIN_PATH 
    WriteRegExpandStr ${env_hklm} "SOAPY_SDR_PLUGIN_PATH" ${SOAPYINSTALLDIR}
!endif

    # Calculates the estimated size of the package so Add/Remove Programs can accurately report the size
    DetailPrint "Calculates the estimated size of the package"
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}" "EstimatedSize" $0
    DetailPrint "Estimated package size $0"
    
    ; make sure windows knows about environment changes
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000
			
	DumpLog::DumpLog "${LOGFILE}" .R0

	#MessageBox MB_OK "DumpLog::DumpLog$\n$\n\
	#		Errorlevel: [$R0]"

    DetailPrint "End install"
sectionEnd
 
#---------------------------------------------

# Uninstaller
 
section "uninstall"
 
    SetShellVarContext current

    DetailPrint "Removing Start menu links..."
    
    
    # Remove Start/Programs Menu launcher
    delete "$SMPROGRAMS\${COMPANYNAME}\${APPNAME}.lnk"
    
    # Remove Start/Programs/Startup Menu launcher
    delete "$SMSTARTUP\${APPNAME}.lnk"

    # Remove the desktop icon
    delete "$DESKTOP\${APPNAME}.lnk"

    # Try to remove the Start Menu folder - this will only happen if it is empty
    rmDir "$SMPROGRAMS\${COMPANYNAME}"
    
    DetailPrint "Removing application files..."
  
    delete $INSTDIR\*.*
    rmDir "$INSTDIR"

    #DetailPrint "Removing fonts..."
    #Call un.InstallFonts
	 
    # Always delete uninstaller as the last action
    delete $INSTDIR\uninstall.exe
 
    # Try to remove the install directory - this will only happen if it is empty
    rmDir /r $INSTDIR

    # Remove uninstaller information from the registry
    DetailPrint "Removing registry keys..."
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${COMPANYNAME} ${APPNAME}"
    DeleteRegValue ${env_hklm} "SOAPY_SDR_PLUGIN_PATH" 
    MessageBox MB_YESNO "Do you want to delete the configuration files, the logs, the shots generated by ${APPNAME} found in$\n ${HOMEDIR}?" \
        IDYES okErase IDNO endSection

okErase:
    DetailPrint "Cleaning directory ${HOMEDIR}..."
    rmDir /r ${HOMEDIR}
    
endSection:


    DetailPrint "Saving log file ${UNLOGFILE}"
	
	
	DumpLog::DumpLog ${UNLOGFILE} .R0

	#doesn't work - to be checked
	#MessageBox MB_OK "DumpLog::DumpLog$\n${UNLOGFILE}$\n\
	#		Errorlevel: [$R0]"
	
    #StrCpy $0 ${UNLOGFILE}
    #Push $0
    #Call un.DumpLog
	
    DetailPrint "End uninstall."
sectionEnd


