#define AppPublisher "m4rl13r3"
#define AppUrl "https://github.com/m4rl13r3/datafuckerr"

[Setup]
AppId={{94CB7683-01DB-48BA-B4B8-020238E47C75}
AppName=datafuckerr
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl + "/issues"}
AppUpdatesURL={#AppUrl + "/releases"}
DefaultDirName={autopf}\datafuckerr
DefaultGroupName=datafuckerr
AllowNoIcons=yes
LicenseFile={#LicenseFile}
OutputDir={#OutputDirectory}
OutputBaseFilename={#OutputBaseName}
SetupIconFile={#IconFile}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\datafuckerr.exe
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=Interface professionnelle de sanitisation contrôlée
VersionInfoProductName=datafuckerr
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "{#SourceDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\datafuckerr"; Filename: "{app}\datafuckerr.exe"
Name: "{autodesktop}\datafuckerr"; Filename: "{app}\datafuckerr.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le bureau"; GroupDescription: "Raccourcis supplémentaires :"; Flags: unchecked

[Run]
Filename: "{app}\datafuckerr.exe"; Description: "Lancer datafuckerr"; Flags: nowait postinstall skipifsilent
