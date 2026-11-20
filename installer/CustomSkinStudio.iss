; Inno Setup script for the Windows installer (built by .github/workflows/release.yml).
; iscc /DAppVersion=0.1.0 /DSourceDir=..\dist\CustomSkinStudio installer\CustomSkinStudio.iss
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\CustomSkinStudio"
#endif

[Setup]
AppId={{6F2C7E4A-3B1D-4C8E-9A5F-1D2E3C4B5A69}
AppName=Custom Skin Studio
AppVersion={#AppVersion}
AppPublisher=Custom Skin Studio
AppPublisherURL=https://github.com/ihasim/custom-skin-studio
AppSupportURL=https://github.com/ihasim/custom-skin-studio/issues
DefaultDirName={autopf}\Custom Skin Studio
DefaultGroupName=Custom Skin Studio
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=CustomSkinStudio-Setup-{#AppVersion}
SetupIconFile=..\platform\icon\app.ico
UninstallDisplayIcon={app}\CustomSkinStudio.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "assoc"; Description: ".fantome dosyalarını Custom Skin Studio ile aç"; GroupDescription: "Dosya ilişkilendirme:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "vc_redist*.exe"
Source: "{#SourceDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\Custom Skin Studio"; Filename: "{app}\CustomSkinStudio.exe"
Name: "{autodesktop}\Custom Skin Studio"; Filename: "{app}\CustomSkinStudio.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.fantome\OpenWithProgids"; ValueType: string; ValueName: "CustomSkinStudio.fantome"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\CustomSkinStudio.fantome"; ValueType: string; ValueName: ""; ValueData: "League skin paketi"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\CustomSkinStudio.fantome\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\CustomSkinStudio.exe,0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\CustomSkinStudio.fantome\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\CustomSkinStudio.exe"" ""%1"""; Tasks: assoc

[Run]
; Microsoft Visual C++ runtime (needed by the app; skipped if already installed)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Microsoft Visual C++ bileşenleri kuruluyor..."; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\CustomSkinStudio.exe"; Description: "{cm:LaunchProgram,Custom Skin Studio}"; Flags: nowait postinstall skipifsilent
