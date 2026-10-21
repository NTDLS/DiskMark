; DiskMark 64-bit installer. Build the x64 Release configuration first.
#define MyArch      "x64"
#define MyBuildDir  "x64"

[Setup]
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

#include "Setup.Common.iss"
