# CommitOnly — chế độ gửi cả từ cho Ubuntu 26

**Bản thử tiếp theo: [NativeDirect alpha.38](UBUNTU_NATIVE_DIRECT.md)** — chữ
hiện tại con trỏ, không bảng nổi. Khi frontend chưa xác nhận được ngữ cảnh thì
giữ phím nguyên dạng, không tự fallback preedit. CommitOnly bên dưới vẫn được
giữ để đối chiếu; không đáp ứng yêu cầu nhìn chữ ngay trong ô nhập.

Trạng thái: bản thử nghiệm (addon 0.2.26 / gói alpha.35), chưa nghiệm thu
desktop. Mục tiêu là tránh gạch chân composition trong ô nhập và tránh sửa ngược
văn bản ở các ứng dụng không hỗ trợ surrounding-text đáng tin cậy.

## Cách hoạt động

- Telex/VNI soạn từ trong bộ gõ. Từ đang soạn hiển thị trong bảng Fcitx/GNOME,
  không gửi client preedit vào ứng dụng.
- Space, Enter, dấu câu, Tab, phím điều hướng hoặc shortcut kết thúc từ:
  gửi từ hoàn chỉnh một lần bằng commitString, rồi để phím đi native.
- Backspace sửa từ đang soạn; khi không còn từ, ứng dụng xử lý Backspace.
  Escape hủy từ đang soạn. Nhả phím và phím modifier không ghi từ.
- Đổi Telex/VNI, chuyển English hoặc mất focus kết thúc từ theo lifecycle
  Fcitx. Reset từ frontend hủy từ; không chèn lại vào đích mới sau reset.
- Trường mật khẩu/nhạy cảm bỏ qua bộ gõ và xóa buffer khi được báo capability.
  Giữ giới hạn buffer 256 sự kiện của Composition; đạt giới hạn thì ngắt từ.

Đường này không gọi deleteSurroundingText, forwardKey, clipboard hoặc hook phím.
Nó dùng cùng một chính sách cho editor và terminal, không đoán terminal qua
tên cửa sổ, accessibility hoặc capability giống nhau của bridge GNOME/IBus.
Extension/helper trong [thử nghiệm terminal cũ](../tools/vscode-terminal-english/README.md)
vẫn dừng phát triển.

## Chọn chế độ

Sau khi build/cài addon mới và để Fcitx nạp addon đó, mở cấu hình OpenKey trong
fcitx5-configtool, chọn `InputStyle=CommitOnly`. Giao diện OpenKey mới cũng có
lựa chọn **Gửi cả từ (CommitOnly)** trong phần chế độ gõ khi schema runtime hỗ
trợ. Chọn rồi nhấn Áp dụng. Telex/VNI dùng chung thiết lập này.

UI mới vẫn đọc được addon cũ có hai chế độ, nhưng ẩn CommitOnly khi schema
chưa hỗ trợ. UI cũ không đọc được schema ba chế độ; dùng UI mới hoặc cấu hình
Fcitx khi thử addon mới. Mặc định vẫn Preedit; Direct giữ nguyên hành vi cũ.
Quay lại Preedit/Direct bằng cùng bộ chọn. Đổi cấu hình khi đang soạn sẽ gửi
từ còn chờ một lần; nên kết thúc từ trước khi đổi.

Build và kiểm thử từ thư mục gốc:

```bash
cmake -S . -B build/commit-only -DOPENKEY_BUILD_FCITX5=ON
cmake --build build/commit-only --parallel 2
ctest --test-dir build/commit-only --output-on-failure
```

Build gói addon: `bash scripts/build-linux-deb.sh`. Lệnh tạo gói trong
`dist/linux`, không cài gói hoặc restart daemon. Xem
[cài/gỡ Ubuntu](UBUNTU_INSTALL.md) và [build UI](UBUNTU_UI.md).

## Đánh đổi và phạm vi hỗ trợ

Chữ chưa vào tài liệu trong lúc soạn từ, nên gợi ý code, tìm kiếm tức thời,
kiểm tra chính tả và undo theo từng ký tự không hoạt động như Direct. Sau khi
gửi từ, chỉnh sửa thuộc ứng dụng; không giữ ownership để sửa dấu từ đã ghi.
Telex vẫn có semantics của engine, ví dụ `w` có thể chuyển thành `ư`: chọn EN
khi nhập lệnh cần nguyên ASCII. CommitOnly không tự chuyển terminal sang EN.

Cần desktop hiển thị bảng bộ gõ để nhìn thấy từ đang soạn. Nếu bridge không
hiện bảng, chữ chỉ thấy khi gửi; không tự fallback sang client preedit vì như
vậy có thể tái xuất hiện gạch chân hoặc lộ từng mảnh vào xterm.js.

Không gửi client preedit loại bỏ nguồn gạch chân composition từ OpenKey.
Ứng dụng vẫn có thể tự gạch chân lỗi chính tả hoặc link. Không có cam kết mọi
ứng dụng Ubuntu 26 tương thích: frontend phải nhận commitString và giữ thứ tự
commit trước phím Space/Enter. Test giả lập không chứng minh đường GNOME,
Chromium/xterm.js hoặc ứng dụng Snap/Flatpak thực tế đã đạt.

## Nghiệm thu desktop còn chờ

Thử trên văn bản/lệnh vô hại, không dùng Enter chạy chuỗi thử như lệnh shell.

| Ứng dụng/backend | Kiểm tra | Trạng thái |
| --- | --- | --- |
| GTK trên Wayland | Telex/VNI, bảng từ, Space/Enter, focus | Chưa thử |
| Qt trên Wayland | Như GTK, selection, shortcut | Chưa thử |
| Chrome/Firefox | input, textarea, contenteditable, thanh địa chỉ | Chưa thử |
| VS Code/Cursor editor và chat | Từ đúng sau commit, mất focus, gợi ý sau commit | Chưa thử |
| VS Code/Cursor terminal | Focus mới, Enter nhiều dòng, không mất/lặp từ | Chưa thử |
| GNOME Terminal/PTY | Không lộ phần từ trung gian, phím boundary đúng thứ tự | Chưa thử |
| LibreOffice | Đủ ký tự đầu, Backspace, focus | Chưa thử |
| XWayland, Snap, Flatpak | Lặp các ca ứng dụng tương ứng | Chưa thử |

Chuỗi Telex: `ddaay laf nooij dung nhaapj bawngf tieengs vieetj` phải thành
`đây là nội dung nhập bằng tiếng việt`. Kiểm từng từ khi Space, từ cuối khi
Enter trong tài liệu thử. Terminal thử Enter riêng với lệnh vô hại đã kiểm
tra, và EN với `pwd`, `ls`, `git`. VNI: `tie6ng1 vie6t5` → `tiếng việt`.
Thử Backspace trước commit, Escape, Tab, Ctrl+A, đổi IM, Alt+Tab, click sang
field mới, trường mật khẩu, gõ nhanh và UI tỉ lệ khác nhau. Kiểm từ không bị
mất/nhân đôi hoặc gửi nhầm field khi focus thay đổi.

Headless hiện kiểm addon thật qua frontend giả lập ở các bộ capability có/
không Preedit, SurroundingText, Terminal, tên frontend ibus và thường; cùng
pipeline Fcitx thật cho focus-out/đổi IM/đổi chế độ. Desktop còn chờ xác nhận.

Kết quả local ngày 2026-11-05 trên Ubuntu 26.04.1: bản Release đạt 8/8 CTest
addon/engine/launcher; UI đạt 15/15 CTest, gồm chọn CommitOnly và tương thích
schema addon cũ. Gói alpha.35 đã qua kiểm tra dependency tự suy từ ELF, payload
và test integration/Direct trên addon giải nén. Chưa cài gói/restart daemon.
