
# [OpenKey](http://open-key.org)

## Windows Fluent UI beta trong nhánh này

Nhánh Windows này bổ sung giao diện WinUI/Fluent, tiếng Việt mặc định, tùy chọn
English, theme Sáng/Tối/Theo hệ thống và menu tray bo góc. Mã nguồn hiện là 3.0.8;
engine và giao diện Win32 dự phòng vẫn được giữ.

Tải bộ cài Windows tại [Releases của bản fork](https://github.com/minhtuan9922/OpenKey/releases).
Chọn asset `OpenKey-Setup.exe`: một bộ cài offline tự chọn x64/x86, có runtime,
Start Menu và tùy chọn icon Desktop. Yêu cầu Windows 10 1809 trở lên hoặc Windows 11;
không hỗ trợ ARM. Bộ cài chưa ký số; đối chiếu checksum đi kèm trước khi chạy.

Trong ứng dụng, chọn **Kiểm tra cập nhật → Tải bản cập nhật** để tải bộ cài
trực tiếp, kiểm tra SHA-256 từ `SHA256SUMS.txt` của cùng bản phát hành rồi chạy
bộ cài. OpenKey tự thoát khi bộ cài khởi chạy; tải hoặc xác minh thất bại thì
ứng dụng tiếp tục chạy. Không cần mở trình duyệt. Hãy lưu nội dung đang chỉnh sửa
trước khi cập nhật. File bộ cài đã chạy được giữ trong thư mục tạm của Windows.

Người dùng đã xác nhận cài đặt, gỡ bỏ, cập nhật lên 3.0.3, Sandbox và máy thật
hoạt động ổn. Điều này không thay thế toàn bộ ma trận Windows/DPI hoặc ca lỗi UAC/runtime.
Các mục trợ năng/hiệu năng được chốt theo yêu cầu người dùng, không coi là đã có
đầy đủ phép đo hoặc nghiệm thu Narrator/High Contrast riêng.
Xem [hướng dẫn Windows](Sources/OpenKey/win32/README.md#fluent-ui-beta),
[bộ cài offline](INSTALLER.md), [build/đóng gói beta](BETA_PACKAGING.md),
[sao lưu/nâng cấp/rollback](MIGRATION_FLUENT_BETA.md),
[checklist](TODO_FLUENT_UI.md) và [ghi chú QA](FLUENT_UI_QA.md).

Bản fork phát triển từ [OpenKey của Mai Vũ Tuyên](https://github.com/tuyenvm/OpenKey).
Thông tin và ảnh macOS/legacy bên dưới được giữ để ghi nguồn, không phải ảnh UI Fluent mới.

Khi cần đối chiếu phiên bản/hash để báo lỗi beta, xem [hướng dẫn chẩn đoán](DIAGNOSTICS.md). Công cụ chỉ xuất metadata cục bộ, không thu nội dung gõ/cấu hình và không upload.

Chạy regression tự động bằng `./test-all.ps1 All` tại root repository;
log từng bộ và báo cáo JSON nằm trong `dist`. Đây không thay thế kiểm thử giao diện,
keyboard hook hoặc chạy bản beta trên máy sạch.

Chuẩn bị môi trường kiểm thử cô lập bằng `./prepare-sandbox.ps1 -ZipPath <file.zip>
-ExpectedSha256 <hash> -Platform x64`. Script chỉ tạo cấu hình, không mở máy ảo
hoặc cài runtime. Xem [hướng dẫn Windows Sandbox](Sandbox/README.md).

### [Download bản gốc macOS/legacy của tác giả](https://github.com/tuyenvm/OpenKey/releases)
[![GitHub release](https://img.shields.io/github/v/release/tuyenvm/OpenKey.svg)](https://github.com/tuyenvm/OpenKey/releases/latest)

### Open source Vietnamese Input App for macOS - Bộ gõ tiếng Việt nguồn mở cho macOS.
Bộ gõ tiếng Việt mới cho macOS, sử dụng kỹ thuật `Backspace`. Loại bỏ lỗi gạch chân khó chịu ở bộ gõ mặc định. Hoàn toàn miễn phí và là nguồn mở, luôn cập nhật và phát triển.

### Mã nguồn của ứng dụng được mở công khai, minh bạch dưới giấy phép GPL. Điều này nghĩa là bạn hoàn toàn có thể tải mã nguồn về tự build, cải tiến theo mục đích của bạn. Nếu bạn tái phân phối bản cải tiến của bạn, thì nó cũng phải là mã nguồn mở và thông báo bản gốc là OpenKey.

### Lưu ý, khi sử dụng OpenKey, bạn nên tắt hẳn bộ gõ khác vì 2 chương trình bộ gõ sẽ xung đột nhau, dẫn đến thao tác không chính xác.

![Giao diện](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-main-control.png "Main UI")
![Giao diện](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-main-control-2.png "Main UI")
![Giao diện](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-main-control-3.png "Main UI")
![Menu](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-small-control.png "Menu bar")
![Gõ tắt](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-macro.png "Macro")
![Chuyển mã](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-convert-tool.png "ConvertTool")

## Hỗ trợ kiểu gõ
- Telex
- VNI
- Simple Telex

## Bảng mã thông dụng:
- Unicode (Unicode dựng sẵn).
- TCVN3 (ABC).
- VNI Windows.
- Unicode Compound (Unicode tổ hợp).
- Vietnamese Locale CP 1258.
- ...

## Tính năng:
- **Modern orthography** (On/Off) - Đặt dấu oà, uý thay vì òa, úy.
- **Quick Telex** (On/Off) - Gõ nhanh (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- **Grammar check** (On/Off) - Kiểm tra ngữ pháp.
- **Spelling check** (On/Off) - Kiểm tra chính tả.
- **Restore key if invalid word** (on/off) - Phục hồi phím với từ sai.
- **Run on startup** (On/Off) - Chạy cùng macOS.
- **Gray menu bar icon** (On/Off) - Biểu tượng xám trên thanh menu phù hợp với chế độ Dark mode.
- **Switch input mode by shortcut key** - Đổi chế độ gõ bằng phím tắt tùy chọn.
- **Autocorrect fixed** (On/Off) - Sửa lỗi autocorrect trên trình duyệt như Chrome, Safari, Firefox, Microsoft Excel.
- **Underline issue fixed on macOS** (On/Off) - Sửa lỗi gạch chân trên macOS.
- **Tạm tắt kiểm tra chính tả bằng phím Ctrl** (On/Off) (Bản 1.5 về sau).
- **Tạm tắt OpenKey bằng phím Cmd/Alt** (On/Off) (Bản 2.0.1 về sau).
- **Cho phép dùng f z w j làm phụ âm đầu** (On/Off) (Bản 1.5 về sau).
- **Gõ tắt phụ âm đầu: f->ph, j->gi, w->qu** (On/Off) (Bản 1.6 về sau).
- **Gõ tắt phụ âm cuối: g->ng, h->nh, k->ch** (On/Off) (Bản 1.6 về sau).
- **Hiện biểu tượng trên thanh Dock** (On/Off) (Bản 2.0.1 về sau). Bấm vào icon trên thanh Dock sẽ mở nhanh Bảng điều khiển.
- **Macro** - Tính năng gõ tắt vô cùng tiện lợi. Gõ tắt của macOS chỉ hỗ trợ 20 ký tự, còn OpenKey không giới hạn ký tự.
- **Chuyển chế độ thông minh:** (On/Off) (Bản 1.2 về sau) - Bạn đang dùng chế độ gõ Tiếng Việt trên ứng dụng A, bạn chuyển qua ứng dụng B trước đó bạn dùng chế độ gõ Tiếng Anh, OpenKey sẽ tự động chuyển qua chế độ gõ Tiếng Anh cho bạn, khi bạn quay lại ứng dụng A, OpenKey tất nhiên sẽ chuyển lại chế độ gõ tiếng Việt, rất cơ động.
- **Viết Hoa chữ cái đầu câu** (On/Off) (Bản 1.2 về sau) - Khi gõ văn bản dài, đôi khi bạn quên ghi hoa chữ cái đầu câu khi kết thúc một câu hoặc khi xuống hàng, tính năng này sẽ tự ghi hoa chữ cái đầu câu cho bạn, thật tuyệt vời.
- **Chế độ “Gửi từng phím”:** (On/Off) (Bản 1.1 về sau) mặc định dùng kỹ thuật mới gửi dữ liệu 1 lần thay vì gửi nhiều lần cho chuỗi ký tự, nên nếu có ứng dụng nào không tương thích, hãy bật tính năng này lên, mặc định thì nên tắt vì kỹ thuật mới sẽ chạy nhanh hơn.
- **Cập nhật tự động:** (Bản 1.3 về sau) tính năng hỗ trợ cập nhật phiên bản OpenKey mới nhất mỗi khi mở OpenKey hoặc tự check trong phần mục Giới thiệu.
- **Công cụ chuyển mã:** (Bản 1.4 về sau) hỗ trợ chuyển mã qua lại văn bản, thích hợp cho việc chuyển đổi văn bản cũ viết bằng VNI, TCVN3 qua Unicode,... Hỗ trợ cấu hình phím tắt chuyển mã nhanh, bảng cấu hình tùy chọn chuyển mã.
- **Tự ghi nhớ bảng mã theo ứng dụng:** (Bản 2.0.1 về sau) Phù hợp cho các bạn dùng Photoshop, CAD,... với các bảng mã VNI, TCVN3. OpenKey tự ghi nhớ ứng dụng nào dùng bảng mã nào để lần sau sử dụng Photoshop, CAD,... OpenKey có thể tự chuyển sang bảng mã đó.
- ...


[Changelog](https://github.com/tuyenvm/OpenKey/blob/master/CHANGELOG.md)

## Cài đặt:
**Cài đặt thủ công:**  
Tải bản OpenKey mới nhất từ [đây](https://github.com/tuyenvm/OpenKey/releases/latest), mở file `dmg` ra rồi kéo thả `OpenKey.app` vào thư mục `Application`.

**Cài bằng Homebrew:** (by nhymxu)  
Nếu chưa cài Homebrew, mở terminal, nhập:
```
$ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

Kiểm tra phiên bản OpenKey:
```
$ brew info --cask openkey
```
Gõ lệnh sau để homebrew tự cài OpenKey cho bạn:
```
$ brew install --cask openkey
```

Để update phiên bản mới nhất của OpenKey
```
$ brew upgrade --cask openkey
```

## Note - Lưu ý:
OpenKey cần cấp quyền, vào *System Preferences -> Security & Privacy -> Accessibility*, kích hoạt `OpenKey.app`. **Không tắt nó khi đang dùng OpenKey**.
![Guide](https://raw.githubusercontent.com/tuyenvm/tuyenvm.github.io/master/images/openkey-guide.png "Accessibility").

## Tác giả
- Mai Vũ Tuyên.
- Mọi góp ý, gửi cho mình qua maivutuyen.91@gmail.com  
- Fanpage: [https://www.facebook.com/OpenKeyVN](https://www.facebook.com/OpenKeyVN)

## Liên kết
- [OpenKey cho Windows, xem chi tiết tại đây](https://github.com/tuyenvm/OpenKey/tree/master/Sources/OpenKey/win32)
- [OpenKey cho Linux (đang phát triển)](https://github.com/tuyenvm/OpenKey/tree/master/Sources/OpenKey/linux)
## Một điều nhỏ nhoi
Đừng quên ủng hộ tác giả bằng cách mua ly cafe cho tác giả tỉnh ngủ nhé:  
[Buy me a coffee ^^](https://tuyenvm.github.io/donate.html)  
[Redbull cũng được ^^](https://paypal.me/tuyenmai)  
Hoặc trực tiếp qua ví momo:   
![Donate by momo](https://tuyenvm.github.io/images/momo.png "Momo").   

Cảm ơn các bạn rất nhiều.
