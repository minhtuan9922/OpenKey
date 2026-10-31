# Kiem thu OpenKey trong Windows Sandbox

## Installer smoke thực tế — 2026-10-31

Đã chạy `InstallerSmoke.ps1` trong Windows Sandbox 10.0.26100.0 (x64),
WDAGUtilityAccount có token admin. Bộ cài 3.0.9 có SHA-256
`4CDFB3A9697CA94D0E3AC87F3CB9ABE72D081E4076D4A7A7B219C4972F0C9714`.
Kết quả **25/25** kiểm tra đạt: cài silent theo tài khoản, payload và shortcut,
silent bỏ qua đăng ký tùy chọn, helper đăng ký/gỡ TSF cả hai registry view,
cài lại giữ giá trị cấu hình thử, UI sống sau 5 giây, gỡ app giữ cấu hình.
Không xác minh nội dung giao diện hoặc gõ tiếng Việt bằng kiểm tra process.

Log: `dist/sandbox-installer-fbfb50dd664147eeb92c0ca3d9ac2ff3/Logs/installer-result.json`,
cùng `install.log`, `reinstall.log`, `uninstall.log`, runtime logs.
Lượt đầu timeout runtime sau 3 phút; runtime tiếp tục và hoàn tất trong guest.
Lượt chạy lại tăng giới hạn từng tiến trình lên 10 phút và đạt. Không coi đây
là nghiệm thu runtime cold-install trong giới hạn 3 phút. Sandbox đã được dừng.

Runner chỉ chạy dưới tài khoản/mapping Sandbox, kiểm tra hash trước cài,
chỉ dùng Input read-only và Logs writable, không bật mạng hoặc cài trên host.
Session fixture riêng dùng `installer-session.json` với `setupSha256` và
`runtimeSha256`, kèm `OpenKey-Setup.exe`, `vc_redist.x64.exe` và runner trong Input.
Không thay bootstrap ZIP chỉ đọc của `prepare-sandbox.ps1`.
Chưa kiểm tra tương tác UAC đồng ý/từ chối, nút bật/tắt trên Tổng quan hoặc
gõ/sửa dấu trên editor. Computer Use không kết nối được native pipe trong lượt này.

Hoi quy staging: test-packaging.ps1 tu goi test-sandbox-staging.ps1 voi ZIP
gia, bao gom 10 dau vao loi khong tao session va 3 phien hop le khong runtime
(x64/x86/lap x64). Kiem tra hash/helper/metadata/mapping va cach ly host.
13/13 dat PS7/5.1. Thu muc sandbox sinh tu test chi chua du lieu gia;
KHONG mo cac .wsb fixture de nghiem thu. Khong co guest/app/installer duoc chay.

Tu 2026-10-11, prepare-sandbox.ps1 co hai tham so tuy chon:
-RuntimePath <bo-cai-local.exe> -RuntimeExpectedSha256 <SHA256-64-ky-tu>.
Phai cung cap ca hai hoac bo ca hai. Script kiem tra hash, Authenticode
Valid/Microsoft Corporation, ProductName dung x64/x86 va FileVersion >=
minimum da xac minh cua beta, truoc staging va sau copy. Khong tai/chay/cai.
Sai dau vao bi tu choi truoc tao session; loi sau copy co the de lai thu muc
GUID dang do, nhung khong tao .wsb. Khong tu xoa cac session cu.
Input/session.json ghi metadata runtime; Input/OFFLINE_RUNTIME.md huong dan
cai thu cong CHI trong guest. Khong cung cap runtime: hanh vi cu giu nguyen.
Session moi tao bang script (chi chuan bi, chua mo guest):
- x64: dist/sandbox-8f96c320e1f34788ab1ca4be75af0b53/OpenKey.wsb
- x86: dist/sandbox-7a2d1dd51c2947c59f5aa963c196cf5c/OpenKey.wsb

Phien moi ngay 2026-10-11 sau sua About workflow:
- x64: dist/sandbox-023a5ca909a447b182252a5bf5af43ab/OpenKey.wsb
- x86: dist/sandbox-f83d8c38ba82403c8068ab1e14e864b5/OpenKey.wsb
Duong dan tinh tu repo root; ten ZIP/hash trong BETA_PACKAGING.md.
Chi chuan bi, chua mo guest. Da bo sung runtime offline ngay 2026-10-11:
Input/vc_redist.x64.exe hoac Input/vc_redist.x86.exe dung kien truc session.
Ca hai 14.51.36247.0, Authenticode Valid/Microsoft Corporation tren host;
hash va nguon trong BETA_PACKAGING.md va Input/OFFLINE_RUNTIME.md moi phien.
Chua chay/cai bo cai. Chi mo bo cai BEN TRONG Sandbox, khong mo tren host.
Session/bo cai cu giu nguyen; script chi copy runtime khi nhan du hai tham so.

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
Script khong tu tai bo cai; cac phien moi chi kem runtime khi duoc chi dinh.

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
