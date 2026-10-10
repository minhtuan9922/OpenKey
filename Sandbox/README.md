# Kiem thu OpenKey trong Windows Sandbox

Phien moi ngay 2026-10-11 sau sua About workflow:
- x64: dist/sandbox-023a5ca909a447b182252a5bf5af43ab/OpenKey.wsb
- x86: dist/sandbox-f83d8c38ba82403c8068ab1e14e864b5/OpenKey.wsb
Duong dan tinh tu repo root; ten ZIP/hash trong BETA_PACKAGING.md.
Chi chuan bi, chua mo guest; hai phien nay KHONG kem vc_redist.
Phien runtime offline cu ben duoi van giu nguyen, khong tu copy vao phien moi.

Double-click OpenKey.wsb trong thu muc session do prepare-sandbox.ps1 tao.
Script khong tu mo Sandbox, bat Windows feature, tai/cai runtime hoac doi settings may host.

Sandbox chi nhan Input read-only va Logs co quyen ghi. Networking, clipboard
host/guest, microphone, webcam va printer redirection tat. vGPU giu Default;
khong coi Sandbox la ranh gioi an toan tuyet doi. Thu muc Logs tren host co the
bi app trong Sandbox sua/xoa; khong dat du lieu quan trong vao thu muc nay.

Bootstrap xac minh ZIP lan nua, giai nen vao LOCALAPPDATA trong guest, kiem tra
VC++ runtime. Neu thieu runtime, ghi prerequisite-missing va khong chay app;
khong tu tai/cai. Muon thu voi runtime can cung cap bo cai Microsoft rieng
trong mot phien test duoc cho phep, khong bat mang mot cach ngam dinh.

Phien x64 sandbox-77f36d02cbeb4ecb894041c71dcd4525 da duoc bo sung bo cai
offline vc_redist.x64.exe theo yeu cau nguoi dung (2026-10-10): 14.51.36247.0,
chu ky Authenticode Microsoft Corporation Valid tren host, SHA256
843068991DAAA1F73AD9F6239BCE4D0F6A07A51F18C37EA2A867E9BECA71295C.
Chua chay/cai bo cai. Trong guest mo C:\OpenKeySandboxInput\OFFLINE_RUNTIME.md
de xem cach cai thu cong va chay lai bootstrap trong cung phien.
prepare-sandbox.ps1 khong tu tai/copy bo cai vao cac phien moi.

Neu runtime dat, bootstrap mo UI va ghi trang thai process sau 3 giay.
ui-running-smoke-only KHONG co nghia GUI/hook/go tieng Viet da dat.
Engine khong duoc script khoi dong. Trong Sandbox, vao workDirectory trong
Logs/smoke-*.json, mo OpenKey64.exe hoac OpenKey32.exe neu muon test go.
Neu co hop thoai UAC, nguoi dung tu quyet dinh; khong tu dong chap nhan.

Kiem thu UI, Macro, Convert, theme, ban phim va Notepad theo FLUENT_UI_QA.md.
Clipboard noi bo guest van dung duoc; copy/paste voi host bi tat.
Khong dung du lieu that. Registry/config trong guest bi bo khi dong Sandbox;
chi bao cao trong Logs tren host duoc giu lai. Luu file test vao Logs neu can.

Script guest tu choi chay neu user khong phai WDAGUtilityAccount. Neu Sandbox
phien ban moi dung tai khoan khac, dung lai va xem xet guard; khong bo guard
de chay bootstrap tren host. Sandbox hien tai khong duoc agent dieu khien
truc tiep; cac ket qua native GUI phai do nguoi dung kiem thu.

Tai lieu Microsoft:
https://learn.microsoft.com/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file
