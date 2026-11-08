# Alpha.49 / UI preview.9 — Simple Telex, macro, phím tắt, ứng dụng

Đây là bản thử nghiệm tiếp theo, chưa hoàn tất toàn bộ [backlog](UBUNTU_REMAINING_WORK.md).
Không có thay đổi sửa giao thức Chrome omnibox trong đợt này.

## Cài thử

Tại thư mục repository, sau khi build:

```bash
sudo apt install ./dist/linux/fcitx5-openkey_3.1.0~alpha.49-1_amd64.deb ./dist/linux/openkey-settings_3.1.0~preview.9-1_amd64.deb
```

Lưu công việc rồi đăng xuất/đăng nhập để addon mới được nạp. Đóng cửa sổ cài
đặt cũ rồi mở lại UI. Không tự restart Fcitx5 từ script build/cài đặt.
UI mới cần addon mới để bật các chức năng mới; addon cũ sẽ hiện không hỗ trợ.

## Simple Telex

Trong cấu hình Fcitx5, thêm **OpenKey Simple Telex 1** hoặc **OpenKey Simple
Telex 2** vào nhóm đang dùng. Việc cài DEB không tự thay nhóm hay kiểu gõ.

- `tieengs` → `tiếng`, `dduowngf` → `đường`, hỗ trợ Backspace và chữ hoa.
- Simple Telex 1 giữ `w` độc lập; Simple Telex 2 dùng `w` độc lập → `ư`.
- `aw`, `ow`, `uw` vẫn đặt hình dạng nguyên âm theo engine.
- Gõ nhanh Telex (`cc` → `ch`, v.v.) chỉ áp dụng Telex đầy đủ.
- Các chế độ Preedit/Direct/CommitOnly/NativeDirect dùng chung lựa chọn này;
  không biến khả năng surrounding-text của ứng dụng thành có hỗ trợ.
- Source service/tray đã hiểu trạng thái và action S1/S2. Service đang chạy
  bản cũ cần được cập nhật/nạp lại riêng; gói addon/UI không chứa service.

## Gõ tắt

Trang **Gõ tắt**:

1. Đọc bảng runtime để có mốc đối chiếu.
2. Thêm từ khóa/nội dung, hoặc chọn Sửa/Xóa trên từng dòng.
3. Nhấn **Áp dụng bảng**; bật công tắc gõ tắt bằng nút Áp dụng riêng.
4. Gõ `vn` rồi Space/Enter với mục `vn → Việt Nam`.

Khớp từ **đã chuyển dấu đang hiển thị**, phân biệt hoa/thường, không phải
chuỗi phím Telex thô. Từ khóa mới tạo thêm mục; cùng từ khóa thay nội dung.
Bản nháp không tự ghi addon. JSON nhập chỉ thay bản nháp; mở file chỉ đưa
JSON vào ô soạn. Xuất/lưu JSON phải theo thao tác, không dùng clipboard tự động.
JSON có format `openkey-macros`, version 1, mảng `entries` chứa `key/value`.
Chưa nhập định dạng nhị phân macro Windows.

Giới hạn 256 mục, từ khóa 32 đơn vị UTF-16/128 byte UTF-8, nội dung 4096 byte
mỗi mục; transport toàn bảng 65536 byte. Cho phép xuống dòng/Tab trong nội
dung. JSON sai, trùng khóa hoặc vượt giới hạn giữ bản nháp trước. File đọc
UTF-8 strict, giới hạn 512 KiB và phải là regular file; lưu atomic.

Preedit/CommitOnly mở rộng trước commit. Direct/NativeDirect chỉ mở rộng khi
snapshot đầy đủ khớp baseline đã xác nhận, selection thu gọn, vùng từ còn
đúng và không có ACK đang chờ. Không dùng dự đoán/echo/selection gợi ý để
mở rộng macro. Nếu không đủ dữ liệu, xử lý boundary bình thường, không mở
rộng; chưa bảo đảm macro trong mọi nhánh Direct/IBus hoặc app không hỗ trợ.
Space/Enter đi native, không forward phím hoặc sửa clipboard.
Mất focus/reset không tự mở rộng macro. Trường mật khẩu/nhạy cảm bỏ qua.

## Phím tắt Fcitx5

Trang **Phím tắt** cho phép sửa **một hành động mỗi lần**, tối đa tám dòng;
để trống để bỏ gán. Editor chấp nhận một tập tên phím có giới hạn, ví dụ
`Control+space`, `Control+Alt+v`, `F6`, `Shift_L`; các tên Fcitx khác vẫn đọc
được, có thể sửa bằng cấu hình Fcitx5.

Kiểm tra trùng tổ hợp giữa các hành động và trong danh sách, kể cả thứ tự
modifier khác nhau. Trước ghi, đọc lại toàn bộ tám danh sách và owner Fcitx5;
khác mốc thì báo xung đột. Gửi một patch của hành động đã chọn, đọc lại xác
nhận; timeout/ACK mất hoặc khác readback báo chưa xác nhận, không tự retry.
Fcitx không cung cấp compare-and-swap: vẫn có cửa sổ race giữa đọc và ghi.

Có xuất JSON `openkey-hotkeys` và lấy hành động đã chọn từ JSON vào nháp,
sau đó nhấn Áp dụng từng hành động để khôi phục. Chưa có file picker/khôi
phục toàn bộ hotkey nguyên tử. Không sửa shortcut của compositor, không
đăng ký thêm phím global trên Wayland; desktop/app vẫn có thể giữ phím.

## Quy tắc theo ứng dụng

Trang **Ứng dụng & tương thích**, thẻ **Chế độ theo ứng dụng**:
đọc runtime → thêm `program ID`/chế độ → Áp dụng quy tắc. Thêm cùng ID thay
quy tắc cũ; Xóa rồi Áp dụng để trở lại chế độ chung. JSON `openkey-app-rules`
nhập/xuất bản nháp riêng, không dùng chung JSON macro.

Khớp **chính xác** giá trị `InputContext::program()` từ frontend, gồm prefix
nếu có (`gtk3-im:google-chrome` khác `google-chrome`). Chỉ dùng ID thật từ
frontend/diagnostics; không đoán từ tên cửa sổ hoặc backend UI. Các ID bridge
chung `gnome-shell`, `unknown`, `ibus`, `wayland`, `fcitx5` bị từ chối, kể cả
prefix/path có basename này. Không đọc nội dung cửa sổ. Frontend không báo
ID cụ thể tiếp tục dùng chế độ chung; không thể tách Chrome/VS Code sau
cùng ID gnome-shell. Chưa có tự phát hiện app, chọn IM riêng hay tắt EN/VI
riêng theo app. Quy tắc chỉ đổi InputStyle của context đó.

## Phạm vi kiểm thử

Có test engine/composition, addon thật với frontend giả, discovery từ prefix,
private D-Bus, UI offscreen 100%/200%, parser và I/O. Không thay bằng chứng
Chrome/Firefox/LibreOffice/VS Code desktop, native portal, clipboard, APT
hay đăng xuất/đăng nhập. Xem kết quả cụ thể trong [task](UBUNTU_TASKS.md).
