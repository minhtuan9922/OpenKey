; Single offline installer. Build only through build-setup.ps1 (verified payloads).
#ifndef PayloadX64
  #error Supply verified payload/runtime paths through build-setup.ps1.
#endif

[Setup]
AppId={{88759ADC-2201-4DF1-A68A-687EB37B1A02}
AppName=OpenKey Fluent Beta
AppVersion=3.0.1
AppPublisher=OpenKey contributors
DefaultDirName={localappdata}\Programs\OpenKey-Fluent
DefaultGroupName=OpenKey Fluent
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x86os x64os
MinVersion=10.0.17763
OutputDir={#SetupOutput}
OutputBaseFilename=OpenKey-Setup
OutputManifestFile={#SetupManifest}
SetupIconFile=..\Sources\OpenKey\win32\OpenKey\OpenKey\icon.ico
UninstallDisplayIcon={app}\OpenKey.UI\OpenKey.UI.exe
LicenseFile=..\LICENSE
Compression=lzma2/fast
SolidCompression=yes
DiskSpanning=no
WizardStyle=modern dynamic
CloseApplications=no
RestartApplications=no
SetupLogging=yes
UninstallLogging=yes

[Languages]
Name: "vietnamese"; MessagesFile: "compiler:Default.isl,Vietnamese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
vietnamese.DesktopIcon=Tạo biểu tượng OpenKey trên Desktop
english.DesktopIcon=Create an OpenKey Desktop shortcut
vietnamese.StartApp=Chạy OpenKey
english.StartApp=Start OpenKey
vietnamese.RuntimeTitle=Thư viện cần thiết
english.RuntimeTitle=Required libraries
vietnamese.RuntimeInfo=Khi thiếu thư viện, bộ cài sẽ cài Microsoft Visual C++ Runtime đi kèm trước OpenKey. Windows sẽ yêu cầu quyền quản trị. Không cần Internet. Hủy hoặc cài thất bại sẽ dừng cài OpenKey. Gỡ OpenKey không gỡ runtime dùng chung.
english.RuntimeInfo=If missing, Setup installs the bundled Microsoft Visual C++ Runtime before OpenKey. Windows will request administrator permission. No download is needed. Canceling or failure stops installation. Uninstalling OpenKey does not remove the shared runtime.
vietnamese.RuntimeConsent=Tôi đồng ý cài Microsoft Visual C++ Runtime nếu cần
english.RuntimeConsent=I agree to install the Microsoft Visual C++ Runtime if needed
vietnamese.AcceptRuntime=Vui lòng đồng ý cài thư viện cần thiết hoặc hủy bộ cài.
english.AcceptRuntime=Please accept installation of the required runtime, or cancel Setup.
vietnamese.CloseApp=Vui lòng thoát OpenKey ở tray và đóng giao diện hoặc dừng debugger trước khi tiếp tục. Bộ cài không tự dừng ứng dụng.
english.CloseApp=Please exit OpenKey from the tray and close its UI or stop the debugger before continuing. Setup does not stop it automatically.
vietnamese.SilentRuntime=Thiếu Microsoft Visual C++ Runtime hoặc phiên bản quá cũ. Chạy bộ cài tương tác để đồng ý cài thư viện.
english.SilentRuntime=Required Microsoft Visual C++ Runtime is missing or too old. Run interactive Setup to approve its installation.
vietnamese.BadRuntime=Kiểm tra tính toàn vẹn runtime thất bại. OpenKey chưa được cài.
english.BadRuntime=Bundled runtime integrity check failed. OpenKey was not installed.
vietnamese.CanceledRuntime=Đã hủy hoặc không thể bắt đầu cài runtime. OpenKey chưa được cài.
english.CanceledRuntime=Runtime installation was canceled or could not start. OpenKey was not installed.
vietnamese.RestartRuntime=Runtime yêu cầu khởi động lại. Khởi động lại Windows rồi chạy lại bộ cài này.
english.RestartRuntime=Runtime requires a restart. Restart Windows, then run this Setup again.
vietnamese.FailedRuntime=Cài runtime không đạt kiểm tra phiên bản. OpenKey chưa được cài.
english.FailedRuntime=Required runtime installation failed verification. OpenKey was not installed.
vietnamese.UninstallRunning=Thoát OpenKey ở tray và đóng giao diện trước khi gỡ. Cấu hình và dữ liệu gõ tắt được giữ lại.
english.UninstallRunning=Exit OpenKey from the tray and close its UI before uninstalling. Settings and text expansions will be preserved.

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Flags: unchecked

[Files]
Source: "{#PayloadX64}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseX64
Source: "{#PayloadX86}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseX86
Source: "{#RuntimeX64}"; DestName: "vc_redist.x64.exe"; Flags: dontcopy
Source: "{#RuntimeX86}"; DestName: "vc_redist.x86.exe"; Flags: dontcopy

[Icons]
Name: "{userprograms}\OpenKey Fluent\OpenKey"; Filename: "{app}\{code:EngineName}"; WorkingDir: "{app}"
Name: "{userprograms}\OpenKey Fluent\OpenKey Settings"; Filename: "{app}\OpenKey.UI\OpenKey.UI.exe"; Parameters: "--page overview"; WorkingDir: "{app}"
Name: "{userdesktop}\OpenKey"; Filename: "{app}\{code:EngineName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{code:EngineName}"; Description: "{cm:StartApp}"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent unchecked runasoriginaluser

[Code]
#include "RuntimePolicy.iss"

var
  RuntimePage: TInputOptionWizardPage;

function UseX64: Boolean;
begin
  Result := IsWin64;
end;

function UseX86: Boolean;
begin
  Result := not UseX64;
end;

function EngineName(Param: String): String;
begin
  if UseX64 then Result := 'OpenKey64.exe' else Result := 'OpenKey32.exe';
end;

function RuntimeKey: String;
begin
  Result := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\';
  if UseX64 then Result := Result + 'x64' else Result := Result + 'x86';
end;

function RuntimeInView(Root: Integer): Boolean;
var
  Installed: Cardinal;
  VersionText: String;
begin
  Result := False;
  if not RegQueryDWordValue(Root, RuntimeKey, 'Installed', Installed) then Exit;
  if not RegQueryStringValue(Root, RuntimeKey, 'Version', VersionText) then Exit;
  Result := RuntimeVersionSufficient(Installed, VersionText, '{#MinimumRuntime}');
end;

function RuntimeSatisfied: Boolean;
begin
  Result := RuntimeInView(HKLM32);
  if not Result and IsWin64 then Result := RuntimeInView(HKLM64);
end;

function EngineRunning: Boolean;
begin
  Result := (FindWindowByClassName('OpenKeyVietnameseInputMethod') <> 0) or
    (FindWindowByWindowName('OpenKey') <> 0);
end;

procedure InitializeWizard;
begin
  RuntimePage := CreateInputOptionPage(wpLicense, CustomMessage('RuntimeTitle'),
    'Microsoft Visual C++ Runtime {#MinimumRuntime} or newer',
    CustomMessage('RuntimeInfo'), False, False);
  RuntimePage.Add(CustomMessage('RuntimeConsent'));
  RuntimePage.Values[0] := False;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = RuntimePage.ID) and RuntimeSatisfied;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = RuntimePage.ID) and not RuntimeSatisfied and not RuntimePage.Values[0] then begin
    if not WizardSilent then MsgBox(CustomMessage('AcceptRuntime'), mbError, MB_OK);
    Result := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  FileName, ExpectedHash: String;
  ExitCode: Integer;
begin
  Result := '';
  if EngineRunning then begin
    Result := CustomMessage('CloseApp');
    Exit;
  end;
  if RuntimeSatisfied then Exit;
  if WizardSilent or not RuntimePage.Values[0] then begin
    Result := CustomMessage('SilentRuntime');
    Exit;
  end;
  if UseX64 then begin
    FileName := 'vc_redist.x64.exe'; ExpectedHash := '{#RuntimeHashX64}';
  end else begin
    FileName := 'vc_redist.x86.exe'; ExpectedHash := '{#RuntimeHashX86}';
  end;
  ExtractTemporaryFile(FileName);
  FileName := ExpandConstant('{tmp}\') + FileName;
  if CompareText(GetSHA256OfFile(FileName), ExpectedHash) <> 0 then begin
    Result := CustomMessage('BadRuntime'); Exit;
  end;
  if not ShellExec('runas', FileName, '/install /passive /norestart', '', SW_SHOWNORMAL,
    ewWaitUntilTerminated, ExitCode) then begin
    Result := CustomMessage('CanceledRuntime'); Exit;
  end;
  Log(Format('VC runtime exit code: %d', [ExitCode]));
  if RuntimeOutcome(ExitCode, RuntimeSatisfied) = 1 then begin
    NeedsRestart := True;
    Result := CustomMessage('RestartRuntime'); Exit;
  end;
  if RuntimeOutcome(ExitCode, RuntimeSatisfied) <> 0 then
    Result := CustomMessage('FailedRuntime');
end;

function InitializeUninstall: Boolean;
begin
  Result := not EngineRunning;
  if not Result and not UninstallSilent then
    MsgBox(CustomMessage('UninstallRunning'), mbError, MB_OK);
end;

// No app settings/Run registry writes or recursive deletion: preserve user data.
