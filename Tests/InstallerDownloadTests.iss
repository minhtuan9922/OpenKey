; Explicit network probe. Never executes a downloaded runtime or installs anything.
[Setup]
AppId=OpenKeyDownloadFixtureOnly
AppName=OpenKey download fixture
AppVersion=1
PrivilegesRequired=lowest
CreateAppDir=no
Uninstallable=no
OutputDir={#ProbeOutput}
OutputBaseFilename=DownloadProbe

[Code]
function CancelDownload(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  Result := False;
end;

function InitializeSetup: Boolean;
var
  Failed: Boolean;
begin
  Result := False;
  Failed := False;
  try
    DownloadTemporaryFile('{#RuntimeUrl}', 'canceled.exe', '{#RuntimeHash}', @CancelDownload);
  except
    Failed := True;
  end;
  if not Failed then RaiseException('Cancellation did not stop download');
  DownloadTemporaryFile('{#RuntimeUrl}', 'verified.exe', '{#RuntimeHash}', nil);
  if CompareText(GetSHA256OfFile(ExpandConstant('{tmp}\verified.exe')), '{#RuntimeHash}') <> 0 then
    RaiseException('Downloaded bytes differ from pin');
  Failed := False;
  try
    DownloadTemporaryFile('{#RuntimeUrl}', 'bad-hash.exe',
      '0000000000000000000000000000000000000000000000000000000000000000', nil);
  except
    Failed := True;
  end;
  if not Failed then RaiseException('Wrong SHA-256 accepted');
  if not SaveStringToFile(ExpandConstant('{param:Report}'),
    '3 native download checks passed: cancel, retry/success, wrong hash rejected. No runtime executed.', False) then
    RaiseException('Cannot write probe report');
end;
