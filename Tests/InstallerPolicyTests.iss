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
  if not SaveStringToFile(Report, IntToStr(Passed) + ' native Pascal policy checks passed; no registry, app, runtime or installation actions.', False) then
    RaiseException('Cannot write fixture report');
end;
