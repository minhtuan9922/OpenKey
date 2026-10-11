// Pure policy, shared with the native Pascal fixture; no registry/process APIs.
function RuntimeVersionSufficient(Installed: Cardinal; VersionText, MinimumText: String): Boolean;
var
  Version, Minimum: Int64;
begin
  Result := False;
  if Installed <> 1 then Exit;
  if (Length(VersionText) > 0) and ((VersionText[1] = 'v') or (VersionText[1] = 'V')) then
    Delete(VersionText, 1, 1);
  if not StrToVersion(VersionText, Version) then Exit;
  if not StrToVersion(MinimumText, Minimum) then Exit;
  Result := ComparePackedVersion(Version, Minimum) >= 0;
end;

// 0 = continue, 1 = restart and rerun Setup, 2 = stop without app installation.
function RuntimeOutcome(ExitCode: Integer; Satisfied: Boolean): Integer;
begin
  Result := 2;
  if (ExitCode = 3010) or (ExitCode = 1641) then Result := 1
  else if ((ExitCode = 0) or (ExitCode = 1638)) and Satisfied then Result := 0;
end;
