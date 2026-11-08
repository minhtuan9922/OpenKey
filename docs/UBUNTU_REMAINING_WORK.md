# Phạm vi còn lại — rà soát 2026-11-08

Checklist gốc: [UBUNTU_TASKS](UBUNTU_TASKS.md). **Chưa hoàn tất toàn bộ**.
Các dòng alpha cũ là lịch sử điều tra, không phải yêu cầu phát hành lại tất
cả bản cũ. Helper/extension terminal đã ngừng phát triển theo phạm vi trước đó.

## Đã thực hiện trong đợt xử lý toàn bộ backlog

- Codec UTF-8 thay codecvt, kiểm tra exhaustive scalar và malformed input;
  giữ UTF-16 units cho engine. Bộ test sanitizer riêng cho engine.
- Microbenchmark replay: [kết quả và cách chạy](UBUNTU_REPLAY_BENCHMARK.md).
- Công cụ chuyển mã UI cho VNI Windows/TCVN3/CP1258/NFC/NFD, worker có Hủy,
  giới hạn, kết quả chỉ đọc, clipboard chỉ theo click; không đổi code table IM.
- AppImage thử nghiệm chỉ chứa UI: [build và giới hạn](UBUNTU_APPIMAGE.md).

## Đợt alpha.49 / preview.9

Đã thêm Simple Telex 1/2 vào addon/discovery, trạng thái service/tray và UI;
macro từ addon đến CRUD/JSON/file trong UI; sửa một hành động phím tắt
Fcitx5 có preflight/readback; quy tắc InputStyle theo program ID cụ thể.
[Xem cách dùng, giới hạn và cài thử](UBUNTU_FEATURES_49.md).
Không có thay đổi sửa Chrome omnibox; chưa hoàn tất toàn bộ backlog.

## Đợt alpha.50 / preview.11

Context Composition riêng, năm tùy chọn nâng cao, worker chuyển file có Hủy/
timeout và sao lưu IM/hotkey bằng file đã triển khai. Kiểm thử addon và
sanitizer 10/10, UI 20/20; UI → service → menu tray đạt trong private D-Bus.
[Xem hướng dẫn và giới hạn](UBUNTU_FEATURES_50.md). Chưa nghiệm thu desktop.

## Triển khai còn thiếu

| Nhóm | Kết quả cần có trước khi đánh dấu xong |
| --- | --- |
| Macro | Backend/persistence/CRUD/JSON/file đã có. Còn nghiệm thu frontend thật, mở rộng các nhánh Direct không có snapshot xác nhận nếu API cho phép; định dạng macro Windows chưa nhập. Replay engine vẫn tắt macro legacy, addon xử lý bảng riêng. |
| Engine context | Composition đã xử lý tăng dần bằng context riêng, differential 64.000 sự kiện và benchmark mới. API legacy còn singleton; lookup table chưa cam kết đa luồng. Còn QA đổi focus trên desktop. |
| Kiểu gõ/bảng mã trong addon | Simple Telex 1/2 đã nối discovery/activate/tray/test. Còn output legacy/NFD của IM, nghiệm thu thực tế các tùy chọn phụ âm đầu/cuối/khôi phục từ sai đã triển khai; không nhầm với công cụ chuyển mã. |
| Phím tắt | Đã sửa từng hành động Fcitx5 có xung đột/readback và JSON nháp. File picker JSON đã có; còn restore toàn bộ nguyên tử nếu API hỗ trợ, tập tên phím mở rộng và global shortcut Wayland qua cơ chế desktop phù hợp. |
| Theo ứng dụng | Đã có chính sách InputStyle/UI/lưu bền theo program ID chính xác. Còn tự liệt kê/kiểm chứng ID desktop, thiết lập khác hoặc IM riêng. GNOME bridge chỉ báo gnome-shell vẫn không thể tách Chrome/VS Code. |
| Tray | Đã nối ba lệnh mở trang Gõ tắt/Chuyển mã/Giới thiệu (preview.12). Còn action bật/tắt chính tả/macro, smart switch, bảng mã IM, theme/chuyển mã nhanh theo TrayContract; không hiện action enabled khi chưa triển khai. |
| Chuyển đổi lớn | Đã có worker tiến trình riêng, Hủy kill, timeout 30 giây, nguồn UTF-8 16 MiB/kết quả 64 MiB. Còn QA desktop và định dạng byte legacy nguyên gốc/DOC nếu mở rộng phạm vi. Luồng dán văn bản vẫn có giới hạn riêng. |
| Flatpak | Cần flatpak-builder và Qt/KDE SDK/runtime; hiện không có tool và sudo noninteractive bị từ chối. Chưa tạo artifact Flatpak. |

Các nhóm này vẫn mở trong checklist. Không được coi AppImage UI là bản
đóng gói addon Fcitx5 vào sandbox hoặc là bằng chứng gõ ổn trên mọi ứng dụng.

## Nghiệm thu bên ngoài test tự động

- Chrome/Firefox/LibreOffice/VS Code, GTK/Qt; native Wayland và XWayland/X11;
  gõ, gợi ý, Tab/Enter, selection, Backspace, focus, undo/redo và gõ nhanh.
- Tray trên GNOME/KDE, menu, IM switch, autostart qua đăng nhập lại và instance.
- UI trên desktop: theme hệ thống, native/portal file chooser, clipboard,
  lưu bền, HiDPI, bàn phím và Orca/AT-SPI.
- APT cài/nâng/gỡ thật, cấu hình người dùng còn nguyên; test payload trong
  root tạm chưa thay thế nghiệm thu này.
- CI trên GitHub: cần quyền push/workflow của repository; lần push trước đã
  thất bại vì chưa có credential. Không coi chạy local là đã chạy runner.

Không tự restart phiên, đóng tài liệu hoặc đổi IM của người dùng chỉ để đánh
dấu QA. Cần phiên desktop và môi trường ứng dụng cụ thể; các giới hạn Chrome
đã ghi ở [NativeDirect](UBUNTU_NATIVE_DIRECT.md) vẫn còn áp dụng.

Tham khảo đóng gói Flatpak/Qt:
[hướng dẫn KDE](https://develop.kde.org/docs/flatpak/packaging/).

## Công cụ hỗ trợ nghiệm thu

Đã có [quy trình desktop Chrome/VS Code](UBUNTU_DESKTOP_QA.md) và script
`linux-compat-report.py` trong mã nguồn. Báo cáo chỉ tổng hợp diagnostics đã
lọc, không tự đánh dấu app đạt. 14 ca Python đạt; CTest có thêm nhóm riêng khi
có Python3. Không thay thế các mục nghiệm thu desktop còn mở ở trên.
