# OpenKey cho Linux — Fcitx5 proof-of-concept

Nhánh `feature/ubuntu-v0.2-fcitx5` bổ sung module Fcitx5 thử nghiệm với hai input
method **OpenKey Telex** và **OpenKey VNI**, dùng engine C++ chung. Chưa có UI
Fluent, tray riêng, macro hoặc bộ cài `.deb`.

## Build và test

Công cụ: compiler C++20 cho addon, CMake >= 3.16, Ninja và SDK Fcitx5 >= 5.0.14.
Engine vẫn dùng C++17. Trên Ubuntu, tên gói SDK core là `libfcitx5core-dev`:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libfcitx5core-dev
cmake -S . -B build/ubuntu-v0.2 -G Ninja \
  -DOPENKEY_BUILD_FCITX5=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/ubuntu-v0.2 --parallel 2
ctest --test-dir build/ubuntu-v0.2 --output-on-failure
```

Chạy từ root repository. Mặc định addon tắt để engine có thể build độc lập.
Target `openkey_fcitx5` tạo `openkey.so`; không khởi động hoặc thay cấu hình
Fcitx5 của người dùng khi build/test.

## Kiểm tra gói trước khi cài

```bash
DESTDIR="$PWD/build/ubuntu-v0.2/stage" cmake --install build/ubuntu-v0.2
```

Trong staging có module dưới `usr/lib/.../fcitx5/`, addon descriptor dưới
`usr/share/fcitx5/addon/` và hai input method dưới
`usr/share/fcitx5/inputmethod/`. Thư mục lib cụ thể phụ thuộc cấu hình CMake;
đường dẫn đầy đủ được ghi trong `build/ubuntu-v0.2/install_manifest.txt`.

Nếu muốn cài thử vào hệ thống, sau khi kiểm tra staging:

```bash
sudo cmake --install build/ubuntu-v0.2
```

Không cần chạy lệnh trên để chạy các test tự động. Nó chỉ cài các file OpenKey,
không đổi input framework, không restart daemon và không thêm OpenKey vào profile.
Khi đang dùng Fcitx5 trong desktop session, khởi động lại Fcitx5 theo hướng dẫn
desktop của bạn, mở công cụ cấu hình Fcitx5, bỏ lọc ngôn ngữ nếu cần và thêm
**OpenKey Telex (Experimental)** hoặc **OpenKey VNI (Experimental)**.
Không dùng `fcitx5 -r` một cách mặc định trên KDE Wayland: Fcitx5 có thể được KWin
khởi chạy bằng socket riêng. Xem [hướng dẫn Wayland chính thức](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en).

Để gỡ bản cài thử, chuyển sang input method khác, xem danh sách trong
`install_manifest.txt` rồi chỉ xóa các file OpenKey được liệt kê ở đó. Không xóa
cả thư mục Fcitx5 hoặc profile người dùng.

## Hành vi và giới hạn

- Chỉ xuất Unicode dựng sẵn. Dùng keysym để ánh xạ chữ/số; không coi keycode
  Wayland/evdev là keycode X11 trong engine.
- Từ đang gõ nằm trong preedit; Space, Enter, dấu câu hoặc điều hướng commit từ
  rồi chuyển phím gốc cho ứng dụng. Không dùng clipboard, giả lập phím hoặc xóa
  văn bản đã commit. Chưa sửa dấu ở từ cũ ngoài preedit.
- Backspace xóa ký tự hiển thị trong preedit, không chỉ xóa phím Telex/VNI cuối.
  Escape hủy preedit; reset từ client hủy preedit. Khi focus-out, Fcitx5 hoặc
  client đã phụ trách commit client preedit thì addon chỉ xóa trạng thái để
  tránh lặp từ; với server-side preedit, addon tự commit. Khi đổi input method,
  addon commit phần đang gõ. Hành vi trên ứng dụng thật vẫn cần kiểm thử desktop.
- Shift/Caps Lock giữ chữ hoa. Ctrl/Alt/Super shortcut commit từ rồi chuyển
  shortcut cho ứng dụng. Ô Password/Sensitive bỏ qua engine.
- Mỗi input context có composition riêng. Vì engine legacy dùng trạng thái
  toàn cục, prototype replay tối đa 256 sự kiện của từ trên event thread Fcitx5;
  đến giới hạn thì commit phần cũ trước phím tiếp theo. Đây không phải giải pháp
  đa luồng hoặc engine nhiều instance; cần đo độ trễ trước beta.
- Chưa bật macro, cấu hình lưu bền, bảng mã legacy, smart-switch theo ứng dụng,
  global hotkey hay UI riêng. Fcitx5 quản lý chuyển input method ở giai đoạn này.

## Kiểm chứng

Đã build với GCC 15 và SDK Fcitx5 5.1.19. Có sáu mục CTest:

1. Engine smoke: chuyển mã Unicode/VNI.
2. Engine regression: Telex, Backspace và macro ở lớp engine.
3. Composition: Telex/VNI, chữ hoa, Backspace chuyển vị trí dấu, xen kẽ context
   và giới hạn sự kiện.
4. Addon: nạp module bằng `dlopen`, tạo addon qua factory thật, dùng frontend
   giả để kiểm tra preedit, commit, Escape, reset, deactivate, shortcut và ô nhạy cảm.
5. Install fixture: cài vào prefix riêng bên trong build, không ghi hệ thống.
6. Integration: Fcitx tự đọc descriptor và nạp module từ install fixture, chuyển
   key event qua pipeline thật, kiểm tra focus-out không lặp từ (cả client và
   server preedit), đổi Telex/VNI và quay lại ô nhập cũ. Không nạp frontend desktop.

Test headless không chứng minh tương thích Firefox/Chrome, LibreOffice, VS Code,
GTK/Qt hoặc GNOME/KDE Wayland/X11. Các mục này vẫn chưa nghiệm thu.
Xem [checklist nghiệm thu desktop](../../../docs/UBUNTU_INPUT_QA.md).

Tham khảo [API phát triển Fcitx5](https://fcitx-im.org/wiki/Develop_an_simple_input_method),
[kế hoạch Ubuntu](../../../docs/UBUNTU_DEVELOPMENT_PLAN.md) và
[task Ubuntu](../../../docs/UBUNTU_TASKS.md).
