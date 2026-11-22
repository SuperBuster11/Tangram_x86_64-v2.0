; Inno Setup script for Tangram (port of the Amiga game by Thalion, 1991) - version 2.0
; Build the game first (Release x64 -> bin64\Tangram_x64.exe, Release x86 -> bin\Tangram_x86.exe; one of them is enough),
; then open this file in Inno Setup 6.3 or newer and press Compile (Ctrl+F9). The installer goes to installer\output\.
; On 64-bit Windows the 64-bit exe is installed, on 32-bit Windows the 32-bit one; both are installed as Tangram.exe.

#define AppName "Tangram"
#define AppVersion "2.0"
#define HaveX64 FileExists(AddBackslash(SourcePath) + "..\bin64\Tangram_x64.exe")
#define HaveX86 FileExists(AddBackslash(SourcePath) + "..\bin\Tangram_x86.exe")
#if !HaveX64 && !HaveX86
  #error Build the game first (Release x64 and/or x86): bin64\Tangram_x64.exe or bin\Tangram_x86.exe is missing.
#endif

[Setup]
; the same AppId in every version, so a newer version replaces an older one
AppId={{C649B390-D6CB-42BD-9DB9-E9BEBFCAEC1F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; installing for all users needs administrator rights; the user may choose "only for me" instead
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=output
OutputBaseFilename=Tangram_Setup_v{#AppVersion}
SetupIconFile=..\assets\tangram.ico
UninstallDisplayIcon={app}\Tangram.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#if HaveX64 && !HaveX86
; only the 64-bit build is there: the installer runs on 64-bit Windows only
ArchitecturesAllowed=x64compatible
#endif
#if HaveX64
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
#if HaveX64
Source: "..\bin64\Tangram_x64.exe"; DestDir: "{app}"; DestName: "Tangram.exe"; Check: Is64BitInstallMode; Flags: ignoreversion
#endif
#if HaveX86
Source: "..\bin\Tangram_x86.exe"; DestDir: "{app}"; DestName: "Tangram.exe"; Check: not Is64BitInstallMode; Flags: ignoreversion
#endif
; v2.0: the game data next to the program (with music1.wav / music2.wav if they are in the project's data folder)
Source: "..\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Tangram.exe"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Tangram.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Tangram.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; The high-score table stays in %APPDATA%\Tangram after uninstalling (like a saved game).
