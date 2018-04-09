[Setup]
;-- Main Setup Information
 AppName                = DiskMark
 AppVerName             = DiskMark 1.0.0.8
 AppCopyright           = Copyright © 1995-2018 NetworkDLS.
 DefaultDirName         = {pf}\NetworkDLS\DiskMark
 DefaultGroupName       = NetworkDLS\DiskMark
 UninstallDisplayIcon   = {app}\DiskMark.Exe
 PrivilegesRequired     = PowerUser
 Uninstallable          = Yes
 AppMutex               = DiskMark
 WizardImageFile        = /../../@Resources/Setup/LgSetup.bmp
 WizardSmallImageFile   = /../../@Resources/Setup/SmSetup.bmp
  Compression            = ZIP/9
 ChangesAssociations    = Yes
 OutputBaseFilename     = DiskMark64
 MinVersion             = 0.0,5.0
 ArchitecturesInstallIn64BitMode = x64
 ArchitecturesAllowed            = x64
 
;-- Windows 2000 & XP (Support Dialog)
 AppPublisher    = NetworkDLS
 AppPublisherURL = http://www.NetworkDLS.com/
 AppUpdatesURL   = http://www.NetworkDLS.com/
 AppVersion      = 1.0.0.8

[Files]
 Source: "..\..\@AutoUpdate\x64\Release\AutoUpdate.Exe";  DestDir: "{app}"; Flags: RestartReplace;
 Source: "AutoUpdate.xml";                                DestDir: "{app}"; Flags: IgnoreVersion;
 Source: "..\x64\Release\DiskMark.Exe";                   DestDir: "{app}"; Flags: IgnoreVersion;
 Source: "Help File\Help.Chm";                            DestDir: "{app}"; Flags: IgnoreVersion;

[Icons]
 Name: "{group}\DiskMark";      Filename: "{app}\DiskMark.Exe";

[Registry]
 Root: HKLM; Subkey: "Software\NetworkDLS\DiskMark"; ValueType: string; ValueName: "ChunkSize";  ValueData:"1024 * 64";
 Root: HKLM; Subkey: "Software\NetworkDLS\DiskMark"; ValueType: string; ValueName: "IOS";        ValueData:"128";
 Root: HKLM; Subkey: "Software\NetworkDLS\DiskMark"; ValueType: string; ValueName: "Iterations"; ValueData:"320";
 Root: HKLM; Subkey: "Software\NetworkDLS\DiskMark"; ValueType: DWORD;  ValueName: "AllowCache"; ValueData:"0";

[Run]
  Filename: "{app}\DiskMark.Exe"; Description: "Launch DiskMark now?"; Flags: PostInstall NoWait shellexec;

