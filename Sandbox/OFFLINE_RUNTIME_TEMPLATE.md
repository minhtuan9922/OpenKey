# Offline VC++ runtime — install ONLY inside Windows Sandbox

Installer in guest: C:\OpenKeySandboxInput\vc_redist.%ARCH%.exe
FileVersion: %VERSION%; beta minimum: %MINIMUM%.
SHA-256: %SHA256%
Host validation date: %DATE%; Authenticode Valid, Microsoft Corporation.

The installer was only copied/validated, NOT executed on host or guest.
Open the installer ONLY in Explorer INSIDE Sandbox. Review license terms
and decide whether to install/accept UAC yourself. Do not run the copy from
the host's Input directory. Redistribution rights remain a separate review.

After installing in guest, open Windows PowerShell INSIDE Sandbox and run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\OpenKeySandboxInput\SandboxSmoke.ps1
```

Bootstrap verifies beta/prerequisites and opens UI if they pass; it does not
start the engine. Logs: C:\OpenKeySandboxLogs. Process-alive smoke status is
not UI/hook/typing acceptance. Runtime is discarded when Sandbox closes.
Network and host clipboard remain disabled; no .wsb change is needed.

Official download reference (not an automatic download or installation):
https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/
