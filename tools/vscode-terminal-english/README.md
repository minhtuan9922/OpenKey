# OpenKey Terminal English — thử nghiệm 0.1.0

> Dừng phát triển theo yêu cầu người dùng (2026-11-05). Extension và helper
> AT-SPI được giữ lại để tham khảo, không còn là hướng xử lý terminal.
> Ưu tiên sửa lỗi mất từ đầu tiên trực tiếp trong addon; chưa có bản sửa được
> xác nhận trên desktop. Không cần cài/bật extension hoặc chạy helper này.

Hướng mới trong addon: [CommitOnly cho Ubuntu 26](../../docs/UBUNTU_COMMIT_ONLY.md)
(alpha.35, thử nghiệm) soạn từ trong bảng bộ gõ rồi gửi cả từ, không gửi client
preedit. Có đánh đổi với gợi ý code; chưa nghiệm thu desktop. Extension/helper
bên dưới vẫn dừng phát triển.

Bản thử tiếp theo [NativeDirect alpha.36](../../docs/UBUNTU_NATIVE_DIRECT.md)
không dùng ô nổi; thiếu xác nhận thì giữ phím nguyên dạng. Chưa công bố sửa
toàn bộ terminal native; không chạy lại extension/helper.

Extension opt-in cho VS Code Desktop trên Linux có Fcitx5 **ở máy giao diện**.
Không sửa addon, không hook phím toàn hệ thống, không đọc văn bản đang gõ,
clipboard, output terminal hoặc profile shell. Không tự cài/chạy lại Fcitx5.

## Cài và bật

1. Trong cửa sổ VS Code chạy **trên desktop Ubuntu có Fcitx5**, chọn Extensions
   → `…` → **Install from VSIX…**, chọn gói `openkey-terminal-english-0.1.0.vsix`.
2. Mở Settings, tìm `OpenKey Terminal English: Enabled` và bật ở User Settings.
   Mặc định tắt; không tự sửa cấu hình người dùng khi cài extension.
3. Gõ xong từ trong editor, nhấn **Ctrl+`** để vào terminal. Extension đọc
   trạng thái trước đó, chuyển focus và tắt input method Fcitx5 (`-c`).
4. Nhấn **Ctrl+`** khi terminal focus để trở lại editor. Nếu trước đó dùng
   OpenKey Telex/VNI và trạng thái vẫn phù hợp, extension khôi phục đúng kiểu
   gõ, không mặc định bật Vietnamese khi trước đó đang English.

Nếu shortcut bị extension khác chiếm, tìm hai lệnh **OpenKey: Focus Terminal
in English** và **OpenKey: Return to Editor and Restore** trong Keyboard
Shortcuts, gán phím riêng với điều kiện editorTextFocus/terminalFocus.

Không gõ tiếp trong khoảng chuyển vùng ngắn; xác nhận biểu tượng EN trước
khi nhập lệnh quan trọng. English ở đây nghĩa là input method bị deactivate,
không ép layout bàn phím sang US nếu hệ thống đang dùng layout khác.

## Giới hạn và an toàn

- **Chỉ chuyển qua phím/lệnh của extension**, không tự nhận diện bấm chuột.
  Không dùng activeTerminal làm bằng chứng ô terminal đang focus. Bấm chuột,
  phím focus khác hoặc chuyển tab terminal có thể làm mất chu kỳ khôi phục;
  chọn EN/VI thủ công nếu cần. Lệnh vào terminal mới bỏ snapshot cũ.
- Chỉ tự khôi phục `openkey-telex`/`openkey-vni`; IM khác đang active thì từ
  chối chuyển. Nếu bạn bật/chọn IM trong terminal hoặc editor đã có IM khác,
  extension không ép ghi đè. Không theo dõi mọi thay đổi EN/VI ở nền.
- Cửa sổ mất focus, terminal đích đóng, đổi setting hoặc unload sẽ quên
  snapshot và hủy bước còn chờ. Không tự khôi phục khi bạn ở ứng dụng khác.
  Nếu lúc đó đang EN, có thể phải bật lại VI thủ công. Lệnh D-Bus đã gửi
  trước khi mất focus không thể được bảo đảm thu hồi/rollback.
- Chỉ một transition một lúc, không xếp hàng. Mỗi subprocess có timeout
  1200 ms, output tối đa 1024 byte, binary cố định `/usr/bin/fcitx5-remote`,
  `--check` trên mọi lời gọi, không shell. Không reload/restart daemon.
- Chờ 100 ms sau lệnh focus để frontend cập nhật context; đây **không phải
  ACK từ Fcitx5** và không bảo đảm mọi máy/Wayland frontend đã ổn định. Cần
  nghiệm thu desktop; không công bố tự động đúng focus trong mọi trường hợp.
- Remote SSH/WSL/container: extension có `extensionKind: ["ui"]`. Nếu giao
  diện VS Code ở Windows/macOS, không điều khiển Fcitx5 ở máy Ubuntu remote.
  Máy có cửa sổ VS Code và Fcitx5 phải cùng là Linux. Không hỗ trợ VS Code Web.

## Thử nghiệm chuột — helper AT-SPI riêng

Đã thêm [helper](helper/atspi_focus.py) và [policy](helper/focus_core.py).
Đây là prototype chạy từ source, **không nằm trong VSIX 0.1.0** và chưa
nghiệm thu chuyển EN/VI trên desktop. Không tự cài dependency, bật accessibility,
thêm autostart hoặc sửa VS Code/Fcitx5. Chỉ hỗ trợ bản Code chính thức ở
`/usr/share/code/code`, không hỗ trợ Snap/Flatpak/Cursor trong prototype này.

Helper theo dõi focus thực qua AT-SPI, không suy đoán từ activeTerminal.
Nhận terminal qua role `terminal` hoặc class `xterm-helper-textarea`, nhận
editor qua hai class `inputarea monaco-mouse-cursor-text`. Nếu Chromium không
expose những metadata này, kết quả là `unknown` và **không chuyển bộ gõ**.
Không đoán theo tên cửa sổ, tên file, tọa độ hoặc mọi ô textarea.

### 1. Chuẩn bị và probe chỉ đọc

Tắt **OpenKey Terminal English: Enabled** trong VS Code trước khi thử helper,
để hai cơ chế không cùng điều khiển Fcitx5. Dependency Ubuntu (chỉ cài nếu thiếu):

```bash
sudo apt install python3-gi gir1.2-atspi-2.0
```

Nếu probe không nhận focus của Code, lưu công việc, thoát hoàn toàn Code rồi
mở lại từ terminal desktop với accessibility bật:

```bash
code --force-renderer-accessibility .
```

Lệnh trên không bảo đảm class DOM được expose; cần kiểm chứng bằng probe.
Từ **terminal ngoài VS Code**, tại thư mục gốc OpenKey:

```bash
/usr/bin/python3 tools/vscode-terminal-english/helper/atspi_focus.py --list
```

Lấy `pid` trong dòng `application` của Code cần thử, thay `PID` bên dưới bằng
số đó (không phải PID extension host hoặc Fcitx5):

```bash
/usr/bin/python3 tools/vscode-terminal-english/helper/atspi_focus.py --pid PID
```

Mặc định chỉ probe, **không gọi fcitx5-remote**. Bấm editor → terminal → editor
nhiều lần, kể cả cùng terminal vừa dùng. Log phải lần lượt có `region` là
`editor`, `terminal`, `editor`; log chỉ chứa trạng thái/region/PID, không chứa
text, tên file, tên cửa sổ hoặc thuộc tính DOM thô. Nếu chỉ thấy `ready` hoặc
`unknown`, dừng ở đây và báo kết quả; chưa bật `--switch`.

### 2. Bật chuyển tự động sau khi probe đạt

Ctrl+C dừng probe rồi chạy:

```bash
/usr/bin/python3 tools/vscode-terminal-english/helper/atspi_focus.py --pid PID --switch
```

Chọn VI và bấm editor trước, chờ khoảng một giây để lấy trạng thái nền.
Sau đó bấm terminal: chuyển EN; bấm lại editor: khôi phục Telex/VNI trước đó.
Ban đầu English thì vẫn English. Bắt đầu trực tiếp trong terminal không đủ
thông tin khôi phục, nên helper không tự chuyển ở lượt đó.

Giới hạn an toàn:

- Một helper cho mỗi phiên desktop, có khóa chống chạy trùng. Chỉ PID đã
  chọn và cùng frame cửa sổ mới được khôi phục; theo dõi PID tái sử dụng.
- Mất focus ra ứng dụng khác, vùng `unknown`, đổi cửa sổ, hủy tác vụ hoặc lỗi
  sẽ quên lịch sử, không cố khôi phục muộn. Có thể phải bật VI lại thủ công.
- Đọc trạng thái Fcitx5 mỗi khoảng 0,5 giây khi ở vùng đã nhận diện; trạng
  thái editor trước khi click là bản lấy mẫu, không phải snapshot nguyên tử.
  Thay EN/VI thủ công ngay sát lúc click có thể chưa được ghi nhận. Việc đổi
  IM trong terminal được phát hiện khi có mẫu; không bảo đảm phát hiện giữa
  hai lần đọc. IM khác OpenKey không bị ép tắt/ghi đè.
- Chờ focus ổn định 120 ms, kiểm tra focused + frame active trước/sau mỗi
  subprocess, timeout 1,2 giây; không gọi shell. Không đọc Text/Value,
  clipboard, terminal output, accessible name/description hoặc hook phím.
  AT-SPI có quyền accessibility rộng: helper này chủ động chỉ dùng metadata.
- Dừng bằng Ctrl+C; không khôi phục lúc thoát vì ứng dụng khác có thể đang
  focus. Không tự retry một thao tác lỗi; một chu kỳ editor mới mới lấy lại
  lịch sử. Lệnh đã gửi tới Fcitx5 không thể bảo đảm rollback khi đổi focus.

Nghiệm thu với text/lệnh vô hại: Telex, VNI, English ban đầu, click lại cùng
terminal, nhiều terminal, cửa sổ Code khác, Alt+Tab, Search/Quick Open,
đổi IM thủ công và dừng helper. Xác nhận editor còn gợi ý và terminal nhập
nguyên ASCII. Các ca thật trên Wayland/XWayland **còn chờ người dùng thử**.

Kiểm thử policy/bridge dùng process giả, không cần GI hay daemon thật:

```bash
/usr/bin/python3 -m unittest discover -s tools/vscode-terminal-english/helper -v
```

API tham khảo: [AT-SPI EventListener](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.EventListener.register.html).

## Build / test / đóng gói

Yêu cầu Node 22 trở lên và npm. Từ thư mục extension:

```bash
npm ci --ignore-scripts
npm test
npm run package
```

Gói VSIX chỉ chứa JavaScript đã biên dịch, manifest, README và giấy phép;
không đóng node_modules hay test. Không publish Marketplace và không cài
extension bằng các lệnh trên. Publisher `openkey-local` chỉ là định danh local.

Test dùng port/process giả, không gọi Fcitx5 thật hoặc mở VS Code. Bao phủ
Telex/VNI, English, IM khác, busy, mất focus, manual switch, lỗi/readback,
cancel/dispose và manifest. Chưa thay kiểm thử extension host/desktop thật.

## Nghiệm thu desktop

- Bật VI Telex → Ctrl+` → terminal EN; gõ `tieengs vieetj` còn nguyên ASCII.
- Ctrl+` về editor → Telex; gõ `tieengs vieetj` → `tiếng việt`.
- Lặp với VNI và khi ban đầu English; không tự bật VI ở trường hợp English.
- Bấm phím nhanh, nhiều terminal, đổi IM thủ công, mất focus giữa chuyển vùng,
  đóng terminal và tắt setting: không có lệnh khôi phục chạy muộn.
- Thử cả Wayland và XWayland; xác nhận không thay đổi profile, nội dung,
  clipboard hoặc ảnh hưởng ứng dụng khác. Tránh thử trên lệnh quan trọng.

Giấy phép GPL-3.0; xem LICENSE đóng cùng gói.
