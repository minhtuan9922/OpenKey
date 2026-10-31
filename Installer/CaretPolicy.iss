// Optional component policy. Never controls whether the core app may install.
function CaretNextAction(Selected, Installed, Silent: Boolean): Integer;
begin
  if not Selected then Result := 0
  else if Installed then Result := 2
  else if Silent then Result := 0
  else Result := 1;
end;

function CaretRegistrationSucceeded(Started: Boolean; ExitCode: Integer): Boolean;
begin
  Result := Started and (ExitCode = 0);
end;
