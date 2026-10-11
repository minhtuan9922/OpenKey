# OpenKey for Windows

## Fluent UI beta

Nhánh này bổ sung bảng điều khiển WinUI/Fluent; engine/tray và UI Win32 dự phòng vẫn
được giữ. Các yêu cầu và ảnh legacy bên dưới không áp dụng cho bản Fluent beta.
Beta hướng tới Windows 10/11 x64/x86; project khai báo min Windows build 17763,
người dùng đã xác nhận cài/gỡ/cập nhật/Sandbox/máy thật hoạt động ổn.
Chưa nghiệm thu riêng toàn bộ ma trận Windows/kiến trúc/DPI.
Không dùng beta Fluent trên Vista/Windows 7 theo yêu cầu legacy ở phần dưới.

Môi trường đã build: Visual Studio 2026 Community (18.x), MSVC v145, workload
Desktop development with C++, Windows SDK và công cụ C++/WinRT/WinUI.
Các NuGet version đang dùng được ghi trong OpenKey.UI.vcxproj; build chạy restore.
Máy chạy không cần Visual Studio nhưng cần VC++ Redistributable phù hợp và toàn bộ
thư mục UI/runtime trong gói. Windows App SDK đang được đóng gói self-contained;
điều này chưa loại bỏ phụ thuộc VC++ runtime hoặc thay kiểm thử máy sạch.

Chạy từ PowerShell tại root repository:

```powershell
.\build.ps1 Debug x64 All
.\run-debug.ps1 x64       # build/chạy engine Debug
.\run-ui-debug.ps1 x64    # build/chạy UI Debug; engine phải chạy để kiểm thử gõ
.\build.ps1 Release x64 All
.\build.ps1 Debug x64 UI -Analyze
.\test-engine.ps1 x64
.\test-settings.ps1 x64    # 258 ca cấu hình/window/theme/phụ thuộc; API giả, không đổi cấu hình
.\test-macro.ps1 x64       # 71 ca macro/editor/gate/export; registry giả và file fixture riêng
.\test-ui-binary.ps1 x64 Release # 4 ca PE/icon/manifest; can output da build, khong chay GUI
.\test-convert.ps1 x64     # 175 text + 17 clipboard giả + 20 engine; không chạm clipboard thật
.\benchmark-engine.ps1 x64 # chỉ đo handler; không phải độ trễ gõ end-to-end
.\measure-runtime.ps1 -ProcessId 1234 -Seconds 15 # thay PID; chỉ đọc CPU/bộ nhớ
.\test-runtime-performance.ps1 # fixture phép tính, không chạy app
.\test-updater.ps1 x64
.\test-prerequisites.ps1
.\test-packaging.ps1
.\test-third-party.ps1
.\test-ui-resources.ps1  # 19 kiểm tra XML/style/bản dịch; không thay visual QA
.\test-sandbox.ps1       # 27 ca cấu hình/guard; không mở Sandbox hoặc chạy GUI
.\test-debug-launcher.ps1 # 15 ca launcher/preflight giả; không build/chạy app
.\test-all.ps1 All       # toàn bộ test x64/x86, log và JSON riêng trong dist
.\package-beta.ps1 x64
.\build-setup.ps1 -Release # build mới cả x64/x86 và tạo một setup offline
```

Thay x64 bằng x86 ở các script nhận Platform. ZIP nằm trong dist và bị Git bỏ qua.
Xác minh bằng `verify-beta.ps1 -ZipPath <file.zip>`, rồi giải nén vào thư mục mới và
chạy `check-prerequisites.ps1` trong gói. Các script chạy ứng dụng không tự gắn debugger;
để debug tương tác, mở solution trong Visual Studio và chọn project khởi động.

run-ui-debug.ps1 từ chối build nếu đúng executable Debug đang chạy/bị debugger giữ;
không tự kill process. Dừng bằng Shift+F5 hoặc đóng UI rồi thử lại. Mỗi lần được phép
build tạo `dist/ui-debug-<GUID>/build.log` và `launch.json`; build lỗi không chạy binary
cũ. Sau chạy đợi 3 giây: process lỗi báo exit code dạng hex; code 0 chỉ cảnh báo khả
năng single-instance redirect, không xác nhận cửa sổ đã hiện. Process còn chạy cũng
không phải nghiệm thu GUI và không được script gắn debugger.

Đọc [hướng dẫn beta](../../../BETA_PACKAGING.md) và
[migration/rollback](../../../MIGRATION_FLUENT_BETA.md) trước khi chạy.
Beta và legacy dùng chung registry settings/macro: thư mục riêng không cô lập dữ liệu.
Không dùng updater stable để nâng cấp bundle Fluent. Chưa có ảnh Fluent mới được
xác nhận; các ảnh bên dưới là giao diện legacy, không phải ảnh nghiệm thu beta.

Có thể chuẩn bị Windows Sandbox bằng `prepare-sandbox.ps1` tại root; cần ZIP và
SHA-256 ngoài archive. Xem [hướng dẫn Sandbox](../../../Sandbox/README.md).
Chỉ chia sẻ một thư mục đầu vào chỉ đọc và một thư mục log riêng có quyền ghi;
không chia sẻ project, không tự mở Sandbox, bật feature hay cài runtime.

## Bản legacy/stable

[![GitHub release](https://img.shields.io/github/v/release/tuyenvm/OpenKey.svg)](https://github.com/tuyenvm/OpenKey/releases/latest)
- OpenKey phiên bản cho Windows bắt đầu từ bản 1.5, có tất cả các tính năng của bản macOS.  
- OpenKey mặc định chạy ở chế độ `Administrator` để có thể dễ dàng gõ Tiếng Việt trong **game**. Mã nguồn đã được phát hành công khai dưới giấy phép **GPL** nên các bạn hoàn toàn yên tâm về tính minh bạch của chương trình, không keylog, không backdoor,...
- OpenKey for Windows hỗ trợ từ Windows Vista trở lên, Windows XP thì chưa có cơ hội để test, Windows 2000 trở về trước thì chắc chắn không thể hỗ trợ rồi.
![Giao diện chính](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/win32_2.0_1.png "Main UI")



## Tính năng
Mặc định, OpenKey cho phép gõ với chế độ bỏ dấu tự do, không cần phải bỏ dấu sau nguyên âm.
* Hỗ trợ kiểu gõ:
	* Telex
	* VNI
	* Simple Telex
	* Có thể yêu cầu thêm kiểu gõ.
* Hỗ trợ bảng mã:
	* Unicode dựng sẵn
	* TCVN3 (ABC)
	* VNI Windows
	* Unicode Tổ hợp
	* Vietnamese locale CP 1258
* Chính tả:
	* Bật tắt kiểm tra chính tả
	* Tự khôi phục phím với từ sai
	* Cho phép chữ "z w j f" làm phụ âm đầu
	* Tạm tắt chính tả bằng phím Ctrl
	* Tạm tắt OpenKey bằng phím Alt
* Tùy chọn:
	* Tự cấu hình phím tắt chuyển Anh - Việt
	* Bỏ dấu kiểu mới (oà, uý) và kiểu cũ (òa úy)
	* Sửa lỗi gợi ý trên thanh địa chỉ trình duyệt, Excel
	* Hiện bảng điều khiển khi khởi động
	* Hai loại biểu tượng trên system tray (đồng bộ với Win10)
	* Khởi động cùng Windows.
	* Chạy với chế độ Admin.
	* Sửa lỗi trên các trình duyệt Chromium
	* Hỗ trợ các ứng dụng metro trên Windows 10
* Công cụ và tính năng nâng cao:
	* Loại trừ ứng dụng thông minh
	* Tự ghi nhớ bảng mã theo ứng dụng
	* Viết Hoa chữ cái đầu câu
	* Gõ nhanh (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th)
	* Gõ tắt và công cụ soạn gõ tắt.
	* Chuyển mã với nhiều tùy chọn.
	* Gõ tắt phụ âm đầu: f->ph, j->gi, w->qu
	* Gõ tắt phụ âm cuối: g->ng, h->nh, k->ch

## Cài đặt
- Vào [đây](https://github.com/tuyenvm/OpenKey/releases/latest) tải file `.zip` phiên bản mới nhất về máy, giải nén vào nơi bất kỳ.
- Nhấp đúp vô `OpenKey.exe` để chạy chương trình.
- Lưu ý, khi chạy chương trình lần đầu, Windows sẽ mở một hộp thoại để hỏi bạn, bấm `yes` để đồng ý OpenKey chạy với quyền `admin`
- Xong rồi một biểu tượng chữ `V` sẽ hiện trên system tray, đúp click vô đó để mở bảng điều khiển, cấu hình những thứ bạn cần.
- Xong.
## Một số hình ảnh khác
![Menu](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/win32_5.png "Menu")
![Bảng gõ tắt](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/win32_2.png "Macro table")
![Công cụ chuyển mã](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/win32_3.png "Convert tool")

## Thông tin
- Dự án OpenKey ra đời vào ngày 18/01/2019. OpenKey được viết lại hoàn toàn mới bởi tác giả Mai Vũ Tuyên. Mục đích ra đời của OpenKey là để khắc phục lỗi gạch chân khó chịu do cơ chế `preedit` gây ra trên hệ điều hành macOS
- Phiên bản đầu tiên cho hệ điều hành macOS được phát hành trên Github vào ngày 01/02/2019  
Trải qua nhiều lần cập nhật và phát triển, ngày 31/07/2019, OpenKey cho macOS được mọi người biết đến qua trang tinhte.vn  
- Với nhiều ý kiến đóng góp từ cộng đồng, OpenKey cho macOS phát triển tới phiên bản 1.5 đã đầy đủ các tính năng của một bộ gõ chuẩn, tiện lợi.  
- Ngày 19/09/2019 tác giả đã "port" OpenKey cho hệ điều hành Windows, phát triển song song với phiên bản cho macOS.  
- Phiên bản OpenKey cho Linux vẫn đang âm thầm phát triển, vì Linux có hệ thống GUI phức tạp, API "lạ" nên đòi hỏi thời gian nhiều hơn cho việc phát triển.

## Mã nguồn
- Mã nguồn cho Windows được build thành công với Microsoft Visual Studio 2017.
## Giấy phép
OpenKey for Windows được phát hành dưới giấy phép GPL, điều này nghĩa là bạn hoàn toàn có thể tự do tải mã nguồn về chỉnh sửa, cải tiến theo mục đích của bạn. Nếu bạn tái phân phối bản cải tiến của bạn, bạn **phải** công bố mã nguồn và credit OpenKey bản gốc.

## Một điều nữa
Đừng quên ủng hộ tác giả bằng cách mua ly cafe cho tác giả tỉnh ngủ nhé: [Gửi cafe](https://paypal.me/tuyenmai)  
Hoặc trực tiếp qua ví momo:   
![Donate by momo](https://tuyenvm.github.io/images/momo.png "Momo").   
Cảm ơn các bạn.
