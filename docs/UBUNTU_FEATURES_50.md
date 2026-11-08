# Ubuntu alpha.50 / UI preview.11 — 2026-11-08

Bản thử nghiệm này bổ sung context engine riêng, năm tùy chọn nâng cao,
chuyển đổi file bằng tiến trình riêng và sao lưu lựa chọn bộ gõ/phím tắt.
Chưa nghiệm thu gõ trên desktop hoặc sửa dứt điểm Chrome omnibox.

## Cài thử trên Ubuntu 26 amd64

Tại thư mục gốc repository, sau khi đóng cửa sổ cài đặt OpenKey cũ:

```bash
sudo apt install ./dist/linux/fcitx5-openkey_3.1.0~alpha.50-1_amd64.deb ./dist/linux/openkey-settings_3.1.0~preview.11-1_amd64.deb
```

Gói không tự restart Fcitx5. Sau khi lưu công việc đang gõ, đăng xuất/đăng
nhập lại để nạp addon mới. UI cần đóng/mở lại; bản user-local có thể che bản
`/usr`. Trong phiên phát triển này, UI user-local được cập nhật riêng sau
kiểm tra gói; addon đang chạy vẫn là bản trước cho đến khi người dùng cài.
AppImage UI độc lập: xem [hướng dẫn](UBUNTU_APPIMAGE.md). Addon cài riêng.

## Engine và tùy chọn gõ

Mỗi Composition sở hữu EngineContext và xử lý từng sự kiện append/backspace.
Không replay cả từ sau mỗi phím. Khi thay tùy chọn, lịch sử tối đa 256 sự kiện
được replay có chủ đích. Copy context sở hữu trạng thái riêng, phục vụ nhánh
Direct thử thay đổi trước khi commit. API engine legacy vẫn có singleton và
biến cấu hình global để tương thích. Lookup table vẫn dùng trên event thread
Fcitx5; đây không phải cam kết engine đa luồng. Windows/macOS chưa build ở đợt này.

Năm tùy chọn mặc định tắt, có trong UI và cấu hình addon:

| Tùy chọn | Ví dụ Telex đã kiểm thử |
| --- | --- |
| Gõ dấu tự do (`FreeMark`) | Áp dụng trong khi gõ |
| Cho phép phụ âm Z/F/W/J (`AllowConsonantZFWJ`) | `zaf` → `zà` |
| Phụ âm đầu nhanh (`QuickStartConsonant`) | `fanh` + Space → `phanh ` |
| Phụ âm cuối nhanh (`QuickEndConsonant`) | `hag` + Space → `hang ` |
| Khôi phục từ sai (`RestoreWrongSpelling`) | `tieengz` + Space → `tieengz ` |

Ba tùy chọn cuối hoàn tất từ ở Space/Enter. Macro trùng khớp được ưu tiên.
Với Direct/NativeDirect, chỉ thay từ khi surrounding text xác nhận đầy đủ,
selection thu gọn và không có prediction/echo chờ xử lý. Nếu frontend không
cung cấp bằng chứng sở hữu từ, không thực hiện thay thế ở ranh giới này.
Không dùng clipboard hoặc phím giả. Xem [giới hạn NativeDirect](UBUNTU_NATIVE_DIRECT.md).

Đã sửa đọc quá cuối chuỗi trong kiểm tra capability NativeDirect, được ASan
phát hiện. Sửa lỗi bộ nhớ này chưa chứng minh Chrome omnibox gõ đúng.

## Chuyển đổi file lớn

Trang Chuyển đổi có chọn file nguồn, nơi lưu và hai bảng mã. Bấm Chuyển đổi
và lưu để chạy tiến trình con của chính binary UI; Hủy kết thúc tiến trình.
Timeout 30 giây, nguồn tối đa 16 MiB, kết quả tối đa 64 MiB. Không chạy Fcitx5,
clipboard hoặc tải UI trong worker. Công cụ chuyển đổi dán văn bản hiện có
vẫn giữ giới hạn riêng và không bị thay bằng luồng file.

Các file phải là **văn bản UTF-8**, có thể biểu diễn ký tự legacy của VNI Windows,
TCVN3 hoặc CP1258. Không đọc file byte legacy nguyên gốc, DOC/DOCX hay dữ liệu
nhị phân. UTF-8 sai, NUL, URL mạng, symlink và nguồn/đích trùng nhau bị từ chối.

Kết quả được ghi vào file tạm cùng thư mục đích, chỉ công bố khi thành công.
Hủy/lỗi/timeout dọn file tạm. Đích có sẵn được kiểm tra fingerprint trước khi
thay thế; đây không phải filesystem compare-and-swap chống mọi race từ tiến
trình khác. Người dùng chọn nơi lưu và chấp nhận ghi đè bằng file chooser.

## Sao lưu bộ gõ và phím tắt

- Trang chọn bộ gõ xuất/đọc JSON hoặc file JSON. Nhập chỉ chọn nháp; bấm
  **Chuyển bộ gõ** để áp dụng. Chỉ chấp nhận năm IM đã hỗ trợ và IM phải có
  trong nhóm hiện tại. Không sao lưu/khôi phục cả nhóm Fcitx5 hay tự thêm IM.
- Trang Phím tắt xuất/đọc file JSON theo schema riêng. Nhập chỉ tạo nháp;
  chọn từng hành động, kiểm tra rồi Apply qua preflight/readback hiện có.
  Chưa có restore nguyên tử tất cả tám hành động.
- Macro, cấu hình addon và giao diện vẫn có schema sao lưu riêng. Không cho
  phép trộn loại JSON hoặc tự áp dụng cấu hình khi mở file.

## Kiểm chứng và phần còn lại

- Addon: 10/10 nhóm CTest; bộ ASan/UBSan với leak detection: 10/10.
- Context differential: 64.000 sự kiện với Telex/VNI/Simple Telex 1/2,
  copy/reset/đổi tùy chọn/context xen kẽ và global bị đặt giá trị gây nhiễu.
- UI: 20/20 nhóm CTest, gồm tương tác offscreen ở scale 100% và 200%.
- Luồng UI → service → radio menu tray: một ca tích hợp với service thật trên
  private D-Bus và Fcitx giả; ba ca QtTest gồm init/cleanup đều đạt.
- Chuyển file: worker thật với nguồn trên 64 KiB, UTF-8 sai, giới hạn,
  hủy/timeout/xung đột và dọn file tạm.
- [Benchmark context so với replay](UBUNTU_REPLAY_BENCHMARK.md) chỉ đo CPU Composition.

Các test không thay thế nghiệm thu nhập liệu Chrome/Firefox/LibreOffice/VS
Code, Wayland/X11, file chooser/clipboard native, Orca và cài/nâng/gỡ APT thật.
Phiên phát triển không có DISPLAY/WAYLAND_DISPLAY; chưa có chuỗi phím/kết quả
Chrome mới từ người dùng. Xem [việc còn lại](UBUNTU_REMAINING_WORK.md).
