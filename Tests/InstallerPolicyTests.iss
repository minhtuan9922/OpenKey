; Native Pascal policy probe. InitializeSetup always aborts before installation.
[Setup]
AppId=OpenKeyPolicyFixtureOnly
AppName=OpenKey policy fixture
AppVersion=1
PrivilegesRequired=lowest
CreateAppDir=no
Uninstallable=no
OutputDir={#ProbeOutput}
OutputBaseFilename=PolicyProbe
SetupLogging=yes

[Code]
#include "..\Installer\RuntimePolicy.iss"
#include "..\Installer\CaretPolicy.iss"
var
  Passed: Integer;

procedure Check(Value: Boolean; Name: String);
begin
  if not Value then RaiseException('FAIL: ' + Name);
  Passed := Passed + 1;
end;

function InitializeSetup: Boolean;
var
  Report: String;
  Code: Integer;
begin
  Result := False;
  Report := ExpandConstant('{param:Report|}');
  if Report = '' then Exit;
  Check(AppCloseKind('C:\OpenKey\OpenKey64.exe', 'C:\OpenKey', 'OpenKeyVietnameseInputMethod') = 1, 'engine exact path');
  Check(AppCloseKind('C:\OpenKey\OpenKey32.exe', 'C:\OpenKey\', 'OpenKeyVietnameseInputMethod') = 1, 'x86 exact path');
  Check(AppCloseKind('C:\OpenKey\OpenKey.exe', 'C:\OpenKey', 'OpenKeyVietnameseInputMethod') = 1, 'debug engine exact path');
  Check(AppCloseKind('c:\openkey\OpenKey.UI\OpenKey.UI.exe', 'C:\OpenKey', 'WinUI') = 2, 'UI exact path case insensitive');
  Check(AppCloseKind('C:\Other\OpenKey64.exe', 'C:\OpenKey', 'OpenKeyVietnameseInputMethod') = 0, 'other installation untouched');
  Check(AppCloseKind('C:\OpenKey-old\OpenKey64.exe', 'C:\OpenKey', 'OpenKeyVietnameseInputMethod') = 0, 'prefix directory untouched');
  Check(AppCloseKind('C:\OpenKey\OpenKey64.exe', 'C:\OpenKey', 'OtherClass') = 0, 'engine requires IPC class');
  Check(AppCloseKind('C:\OpenKey\other.exe', 'C:\OpenKey', 'OpenKeyVietnameseInputMethod') = 0, 'other executable untouched');
  Check(AppCloseAction(0, 1, 0, 42) = 0, 'already exited engine needs no message');
  Check(AppCloseAction(0, 2, 99, 42) = 0, 'already exited process ignores recycled window');
  Check(AppCloseAction(258, 1, 42, 42) = 1, 'running engine matching PID');
  Check(AppCloseAction(258, 2, 42, 42) = 2, 'running UI matching PID');
  Check(AppCloseAction(258, 1, 99, 42) = -1, 'recycled engine HWND stops');
  Check(AppCloseAction(258, 2, 99, 42) = -1, 'recycled UI HWND stops');
  Check(AppCloseAction(258, 1, 0, 42) = -1, 'window PID query failure stops');
  Check(AppCloseAction(258, 1, 0, 0) = -1, 'zero expected PID rejected');
  Check(AppCloseAction(258, 0, 42, 42) = -1, 'unknown close kind rejected');
  Check(AppCloseAction(258, 3, 42, 42) = -1, 'unsupported close kind rejected');
  Check(AppCloseAction(-1, 1, 42, 42) = -1, 'WAIT_FAILED never sends engine IPC');
  Check(AppCloseAction(-1, 2, 42, 42) = -1, 'WAIT_FAILED never sends UI close');
  Check(AppCloseAction(128, 1, 42, 42) = -1, 'unexpected wait result rejected');
  Check(AppCloseAction(1, 2, 42, 42) = -1, 'unknown wait error rejected');
  Check(AppCloseWaitMilliseconds(0) = 5000, 'initial close budget');
  Check(AppCloseWaitMilliseconds(1234) = 3766, 'shared budget decreases');
  Check(AppCloseWaitMilliseconds(4999) = 1, 'last millisecond');
  Check(AppCloseWaitMilliseconds(5000) = 0, 'deadline reached');
  Check(AppCloseWaitMilliseconds(5001) = 0, 'expired deadline clamps to zero');
  Check(AppCloseWaitMilliseconds(2147483647) = 0, 'large elapsed cannot overflow wait');
  Check(AppCloseWaitMilliseconds(-1) = 0, 'invalid negative elapsed fails closed');
  Check(RuntimeVersionSufficient(1, '14.51.36231.0', '14.51.36231.0'), 'equal minimum');
  Check(RuntimeVersionSufficient(1, 'v14.51.36247.0', '14.51.36231.0'), 'newer v prefix');
  Check(RuntimeVersionSufficient(1, 'V14.52.1.0', '14.51.36231.0'), 'newer V prefix');
  Check(RuntimeVersionSufficient(1, '15.0.0.0', '14.51.36231.0'), 'newer major');
  Check(not RuntimeVersionSufficient(1, '14.51.36230.0', '14.51.36231.0'), 'older patch');
  Check(not RuntimeVersionSufficient(1, '14.50.99999.0', '14.51.36231.0'), 'invalid component');
  Check(not RuntimeVersionSufficient(0, '14.51.36247.0', '14.51.36231.0'), 'not installed');
  Check(not RuntimeVersionSufficient(2, '14.51.36247.0', '14.51.36231.0'), 'invalid installed flag');
  Check(not RuntimeVersionSufficient(1, '', '14.51.36231.0'), 'missing version');
  Check(not RuntimeVersionSufficient(1, 'bad', '14.51.36231.0'), 'malformed version');
  Check(not RuntimeVersionSufficient(1, '14.51.36247.0', 'bad'), 'malformed minimum');
  Check(RuntimeOutcome(0, True) = 0, 'success verified');
  Check(RuntimeOutcome(0, False) = 2, 'success unverified stops');
  Check(RuntimeOutcome(1638, True) = 0, 'already installed verified');
  Check(RuntimeOutcome(1638, False) = 2, 'already installed wrong version stops');
  Check(RuntimeOutcome(3010, True) = 1, 'restart required');
  Check(RuntimeOutcome(3010, False) = 1, 'restart before verification');
  Check(RuntimeOutcome(1641, True) = 1, 'restart initiated');
  Check(RuntimeOutcome(1641, False) = 1, 'restart initiated unverified');
  for Code := 1 to 8 do begin
    Check(RuntimeOutcome(Code, True) = 2, 'failure never continues');
    Check(RuntimeOutcome(Code, False) = 2, 'failure unverified stops');
  end;
  Check(RuntimeOutcome(1602, True) = 2, 'user canceled');
  Check(RuntimeOutcome(1603, True) = 2, 'fatal runtime failure');
  Check(RuntimeOutcome(-1, True) = 2, 'unknown failure');
  Check(CaretNextAction(False, False, False) = 0, 'unselected optional component does not elevate');
  Check(CaretNextAction(True, True, False) = 2, 'installed matching component does not elevate');
  Check(CaretNextAction(True, False, True) = 0, 'silent core install does not request UAC');
  Check(CaretNextAction(True, False, False) = 1, 'interactive selected component requests UAC');
  Check(CaretRegistrationSucceeded(True, 0), 'component success');
  Check(not CaretRegistrationSucceeded(False, 1223), 'UAC canceled skips component');
  Check(not CaretRegistrationSucceeded(False, 0), 'failed launch cannot claim component installed');
  Check(not CaretRegistrationSucceeded(True, 5), 'component access denied skips component');
  if not SaveStringToFile(Report, IntToStr(Passed) + ' native Pascal policy checks passed; no registry, app, runtime or installation actions.', False) then
    RaiseException('Cannot write fixture report');
end;
