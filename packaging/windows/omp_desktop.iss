; OMP Desktop Windows Installer (Inno Setup 6)
; Build via: ISCC.exe /DAppVersion=0.1.0 /DBuildDir=..\..\build-release packaging\windows\omp_desktop.iss

#ifndef AppVersion
  #define AppVersion "0.1.1"
#endif

#ifndef BuildDir
  #define BuildDir "..\..\build-release"
#endif

#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

#define AppName "OMP Desktop"
#define AppPublisher "senran-N"
#define AppURL "https://github.com/senran-N/OMP-E"
#define AppExeName "omp_desktop.exe"

[Setup]
; Stable GUID for clean in-place upgrades across future releases
AppId={{8E74D3C1-9B52-4F6A-A8C4-2E91C57B0D31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\OMP Desktop
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile=..\..\LICENSE
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=OMP-Desktop-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\icons\omp_desktop.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\SDL3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\assets\fonts\*"; DestDir: "{app}\assets\fonts"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\assets\locales\*"; DestDir: "{app}\assets\locales"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
