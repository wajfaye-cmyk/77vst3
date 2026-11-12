; 77.MP3 - Windows installer (Inno Setup 6)
; Build: ISCC.exe /DAppVersion=1.0.0 installer\77mp3.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define AppName       "77.MP3"
#define AppPublisher  "WajFay"
#define AppURL        "https://www.instagram.com/wajfay"
#define BundleName    "77mp3.vst3"
#define BuildDir      "..\build\SeventySevenMP3_artefacts\Release\VST3"

[Setup]
AppId={{D61E29F8-08F1-4208-8DC5-55094E4BF423}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
AppCopyright=Copyright (c) 2026 {#AppPublisher}
AppComments=Vocal Draw and Auto-Tune VST3 plugin

VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (c) 2026 {#AppPublisher}

DefaultDirName={commonpf64}\{#AppPublisher}\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0

OutputDir=..\dist
OutputBaseFilename=77MP3-Setup-{#AppVersion}-x64
SetupIconFile=77mp3.ico
UninstallDisplayIcon={app}\77mp3.ico
UninstallDisplayName={#AppName} VST3

WizardStyle=modern
WizardSizePercent=110
WizardImageFile=wizard_100.bmp,wizard_125.bmp,wizard_150.bmp,wizard_175.bmp,wizard_200.bmp
WizardSmallImageFile=small_100.bmp,small_125.bmp,small_150.bmp,small_175.bmp,small_200.bmp
WizardImageStretch=no
LicenseFile=LICENSE.txt

Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
CloseApplicationsFilter=FL64.exe,FL.exe
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel1=Welcome to {#AppName}
WelcomeLabel2=This will install {#AppName} {#AppVersion} by {#AppPublisher} on your computer.%n%nVocal Draw and Auto-Tune in one plugin. The VST3 will be installed to the standard VST3 folder so FL Studio finds it automatically.%n%nClose FL Studio before continuing.
FinishedHeadingLabel=Installation complete
FinishedLabelNoIcons={#AppName} is installed in your VST3 folder.%n%nIn FL Studio open Options > Manage plugins and click Find installed plugins, then load 77.MP3 on your vocal track.

[Files]
Source: "{#BuildDir}\{#BundleName}\*"; DestDir: "{commoncf64}\VST3\{#BundleName}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "77mp3.ico";   DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; remove a previous bundle completely so stale binaries never linger
Type: filesandordirs; Name: "{commoncf64}\VST3\{#BundleName}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#BundleName}"
Type: dirifempty;     Name: "{app}"
Type: dirifempty;     Name: "{commonpf64}\{#AppPublisher}"
