; Shared by Setup32.Iss and Setup64.Iss. Not meant to be compiled on its own.
; Expects the including script to define MyArch ("x86" or "x64") and MyBuildDir ("Win32" or "x64").

#define MyAppName       "DiskMark"
#define MyAppVersion    "2.0.0.0"
#define MyAppPublisher  "NetworkDLS"
#define MyAppURL        "https://www.NetworkDLS.com/"
#define MyAppExe        "DiskMark.exe"

[Setup]
; Same AppId for both architectures so either installer upgrades an existing install of the other.
AppId={{7B0C3A52-4E1D-4C8E-9A57-5D0F0C6B2E14}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppCopyright=Copyright (C) 1995-2026 NetworkDLS. All rights reserved.
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoVersion={#MyAppVersion}

DefaultDirName={autopf}\NetworkDLS\DiskMark
DefaultGroupName=NetworkDLS\DiskMark
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExe}
UninstallDisplayName={#MyAppName}

; Installing to Program Files requires elevation; the user may choose a per-user install instead.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

; The application is Windows 10+ only.
MinVersion=10.0

Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\Resources\MainIcon.ico
OutputDir=Output
OutputBaseFilename=DiskMark-{#MyAppVersion}-{#MyArch}-Setup

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\{#MyBuildDir}\Release\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "Help File\Help.chm"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\NetworkDLS\DiskMark"; Filename: "{app}\{#MyAppExe}"
Name: "{autodesktop}\DiskMark"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Registry]
; DiskMark keeps its settings per user (and writes them itself). Remove them when the program is uninstalled.
Root: HKCU; Subkey: "Software\NetworkDLS\DiskMark"; Flags: uninsdeletekey dontcreatekey

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
