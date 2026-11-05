# Cài OpenKey thử nghiệm trên Ubuntu

Gói `fcitx5-openkey` cung cấp hai input method **OpenKey Telex (Experimental)** và
**OpenKey VNI (Experimental)**. Dùng engine OpenKey của Mai Vũ Tuyên và phần tích
hợp Linux của bản fork. Mã nguồn/giấy phép: GPL-3.0, xem
<https://github.com/minhtuan9922/OpenKey>.

Đây là bản alpha. Gói `fcitx5-openkey` chỉ chứa addon, không kèm UI/tray hoặc macro.
UI Qt Quick có gói preview riêng `openkey-settings`; tray vẫn build/cài riêng.
Xem [Ubuntu UI](UBUNTU_UI.md)
và [Ubuntu service](UBUNTU_SERVICE.md).
Từ alpha.3 có lựa chọn Preedit/Direct được lưu trong cấu hình Fcitx5.
Kiểm thử người dùng trên GNOME/Wayland, alpha.7: profile textarea thông thường
giữ gợi ý nhưng không chuyển dấu; profile textarea với accessibility support bật
gõ `cộng hoà`, không gạch chân và giữ gợi ý. Alpha.8 đã được người dùng xác nhận
sửa từ bằng Backspace đúng mong đợi; các ca Tab/Enter, undo/redo, đổi focus và gõ
nhanh cũng được báo đạt trong profile accessible này (không thay toàn bộ ma trận QA).
Alpha.9 bổ sung xóa Space vừa gõ để sửa tiếp từ trước đó; người dùng đã xác nhận
các ca hướng dẫn đạt mong đợi trên profile accessible này.
Alpha.10 bổ sung hai thiết lập chung cho Telex/VNI: kiểm tra chính tả và đặt dấu
kiểu mới, mặc định đều bật để giữ hành vi trước đây. Chưa nghiệm thu desktop alpha.10.
Alpha.11 thêm `QuickTelex`, mặc định tắt, chỉ mở rộng cặp phụ âm khi dùng Telex;
VNI không đổi. Chưa nghiệm thu desktop alpha.11.

## Cài gói

Alpha.34 là **bản sửa thử** cho lỗi mất từ đầu khi focus vào terminal VS Code
Wayland, mở rộng alpha.33 vốn chỉ khởi động native sau Enter. Ibus + Direct
nay bắt đầu mỗi từ ASCII nguyên dạng bằng phím native và giữ chuỗi nối thêm
nguyên dạng đi native (`ls`, `git`, `cd`), không đòi gõ Space trước. Chỉ nhận
quyền sửa từ nếu surrounding xác nhận đúng chữ vừa nhập. Không synthesize phím.
Biến đổi từ ngay phím đầu (Telex `w` → `ư`) vẫn dùng nhánh chuyển dấu hiện có.
Nếu không có ACK, từ đầu đi nguyên bản đến ranh giới (có thể không chuyển dấu).
Áp dụng đường ibus Direct, không tự phân biệt terminal/editor; Preedit và frontend
khác không đổi. Chưa nghiệm thu desktop; **chưa sửa** lỗi dấu `def` → `deè`
do xterm.js không áp dụng deleteSurroundingText xuống shell. Không cần extension.

Từ root repository, sau khi đã có gói và checksum trong `dist/linux/`:

```bash
cd dist/linux
sha256sum -c 'fcitx5-openkey_3.1.0~alpha.34-1_amd64.deb.sha256'
sudo apt install './fcitx5-openkey_3.1.0~alpha.34-1_amd64.deb'
```

APT sẽ xử lý dependency được khai báo trong gói. Bản build trên Ubuntu 26.04
amd64 không mặc nhiên tương thích Ubuntu 22.04/24.04: gói ghi yêu cầu runtime
theo thư viện máy build. Không bỏ qua dependency; hãy build lại trên bản Ubuntu
mục tiêu nếu APT báo không đáp ứng phiên bản thư viện.

Gói chỉ đặt module và descriptor vào hệ thống; không có maintainer script,
không đổi profile, không bật autostart, không dừng/chạy lại daemon hoặc thay IBus.
Không có mục mở ứng dụng OpenKey riêng trong launcher ở bản alpha này.

## Gói giao diện riêng — preview.1

Gói `openkey-settings` không cài kèm addon/tray và không thay profile, bật
autostart hoặc restart Fcitx5. Có mục **OpenKey Settings / Cài đặt OpenKey**
trong menu ứng dụng, executable và D-Bus activation. Theme/sidebar và cấu hình
người dùng không nằm trong payload; chưa phát hành trên kho APT.

Tạo gói từ root repository (cần SDK Qt theo [Ubuntu UI](UBUNTU_UI.md), cùng
`dpkg-dev`, `desktop-file-utils`, Ninja và công cụ build):

```bash
bash scripts/build-linux-ui-deb.sh
```

Script build/test và giải nén để kiểm tra, **không cài gói**. Sau khi đã có
gói/checksum, người dùng có thể cài chủ động:

```bash
cd dist/linux
sha256sum -c 'openkey-settings_3.1.0~preview.1-1_amd64.deb.sha256'
sudo apt install './openkey-settings_3.1.0~preview.1-1_amd64.deb'
```

Không bỏ qua dependency Qt/QML hoặc cài gói Ubuntu 26.04 vào bản Ubuntu cũ
bằng force. Build trên Ubuntu mục tiêu; job Ubuntu 24.04 mới được cấu hình,
chưa nghiệm thu trên remote. UI vẫn mở được khi thiếu addon, nhưng các phần
runtime sẽ báo chưa kết nối/chưa hỗ trợ. Gói không tự cài hoặc bật bộ gõ.

Đóng UI cũ trước khi mở bản mới. Máy từng cài component vào `~/.local` có thể
còn binary, desktop entry hoặc D-Bus service người dùng ưu tiên hơn bản `/usr`.
Kiểm tra `type -a openkey-settings` và đường Exec trong
`~/.local/share/dbus-1/services/org.openkey.Settings.service` nếu có. Để thử
đúng gói, đóng cửa sổ cũ rồi chạy `/usr/bin/openkey-settings`; nếu muốn chuyển
hẳn sang gói hệ thống, sao lưu/di chuyển riêng các launcher/activation cũ có
chủ đích. Gói và script không tự xóa bản cài local của bạn.

### Kiểm tra vòng đời payload trong root tạm

Sau khi có gói, target sau dùng `dpkg` với root, database và log nằm riêng
trong thư mục build. Chạy bằng tài khoản thường, không dùng sudo:

```bash
cmake --build build/ubuntu-ui-deb --target verify_linux_ui_deb_lifecycle
```

Kiểm tra lần lượt giải nén gói mới, giải nén lại, nâng lên gói phiên bản QA
tổng hợp chỉ trong root tạm, remove, giải nén lại rồi purge. So sánh payload
và các file cấu hình giả lập sau từng bước. Gói QA không phải bản phát hành
và không được cài vào máy thật. Fixture/database/log được giữ trong thư mục
`build/ubuntu-ui-deb/ui-deb-lifecycle/run-*` để kiểm tra, không tự xóa build cũ.
Script build UI và job CI cũng chạy target này sau khi tạo/kiểm tra gói.

Guard từ chối chạy root và các tùy chọn dpkg mặc định không nằm trong danh
sách cho phép, đặc biệt invoke hook, path filter hoặc force flag. Log dpkg
được trỏ riêng vào root tạm; không chạy maintainer script/trigger, không
đổi HOME, không dùng `force-depends` hoặc `script-chrootless`. Nếu guard báo
cấu hình dpkg không hỗ trợ, không tắt guard hoặc sửa cấu hình máy thật chỉ
để chạy test; dùng môi trường thử riêng.

Đây là kiểm chứng **unpack/remove/purge và giữ file**, không configure gói,
không giải dependency bằng APT, không cài Qt vào root tạm hay mở desktop.
Không thay nghiệm thu launcher, singleton, nâng cấp thật và cài/gỡ bằng APT.
Chỉ dùng gói build từ nguồn tin cậy; root tạm không phải sandbox bảo mật cho
binary lạ (verifier có chạy smoke test từ executable trong gói).

Gỡ riêng UI trên hệ thống thật: `sudo apt remove openkey-settings`. Không xóa addon/tray hoặc
cấu hình trong thư mục người dùng. Vòng đời cài mới/nâng cấp/gỡ bằng APT và
launcher trên desktop vẫn cần nghiệm thu; kiểm tra gói giải nén không thay
cho các bước này. AppImage/Flatpak vẫn chưa có.

## Bật bộ gõ

Trong phiên desktop đã thiết lập Fcitx5, lưu công việc rồi khởi động lại input
framework theo hướng dẫn desktop (hoặc đăng xuất/đăng nhập). Mở cấu hình Fcitx5,
bỏ lọc ngôn ngữ nếu cần và thêm OpenKey Telex hoặc OpenKey VNI. Chuyển bộ gõ
bằng phím tắt hiện có của Fcitx5.

Nếu đang dùng IBus, việc cài gói không tự chuyển sang Fcitx5. Hãy theo
[hướng dẫn setup Fcitx5](https://fcitx-im.org/wiki/Setup_Fcitx_5) và
[hướng dẫn Wayland](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en)
cho desktop đang dùng. Không mặc định áp biến môi trường hoặc restart daemon
giống nhau cho mọi GNOME/KDE/Wayland session.

Thử Telex `tieengs vieetj` hoặc VNI `tie6ng1 vie6t5`, rồi Space. Kỳ vọng:
`tiếng việt `. Kiểm tra thêm đổi focus không làm lặp từ.

## Chính tả và đặt dấu — alpha.10

Sau khi nâng cấp gói, module cũ vẫn nằm trong tiến trình Fcitx5 đang chạy.
Lưu công việc và khởi động lại framework theo desktop hoặc đăng xuất/đăng nhập
để nạp addon mới. Không cần đổi profile hay cài lại tray.

Trong UI đã cập nhật, mở **Chính tả & gõ nhanh**, nhấn **Đọc lại** rồi chọn
Bật/Tắt và **Áp dụng** riêng từng thiết lập. Với addon cũ thiếu schema, hai
điều khiển mới bị khóa; Preedit/Direct và diagnostics vẫn dùng được.

| Thiết lập | Mặc định | Ví dụ Telex / VNI |
| --- | --- | --- |
| `CheckSpelling` | `True` | Bật: `fas` / `fa1` giữ nguyên; tắt: chuyển thành `fá` |
| `UseModernOrthography` | `True` | Bật: `hoas` / `hoa1` → `hoá`; tắt → `hóa` |

Đây là quy tắc xử lý dấu của engine, không phải gạch đỏ kiểm lỗi tài liệu.
Áp dụng chung Preedit/Direct và Telex/VNI; không chuyển EN/VI. Chỉ thay đổi khi
đã gõ xong từ vì addon kết thúc context hiện tại. UI xác nhận runtime, không
bảo đảm ghi đĩa nếu addon gặp lỗi lưu. Hai khóa lưu cùng cấu hình OpenKey;
khóa lạ được giữ khi lưu. Gõ nhanh/khôi phục từ sai chưa được nối vào addon.

## Gõ nhanh Telex — alpha.11

Trong **Chính tả & gõ nhanh → Gõ nhanh Telex**, chọn Bật rồi **Áp dụng** khi đã
gõ xong từ. `QuickTelex=False` là mặc định, không thay hành vi trước đây.

| Cặp Telex | Khi bật |
| --- | --- |
| `cc` | `ch` |
| `gg` | `gi` |
| `kk` | `kh` |
| `nn` | `ng` |
| `qq` | `qu` |
| `pp` | `ph` |
| `tt` | `th` |

Chỉ áp dụng OpenKey Telex, cả Preedit/Direct; OpenKey VNI không dùng mở rộng
này kể cả khi cấu hình bật. Đây không phải macro hay gõ nhanh phụ âm đầu/cuối.
Tùy chọn lưu chung trong cấu hình OpenKey và không tự chuyển EN/VI/Telex/VNI.
Apply chỉ đổi một khóa, giữ mode/chính tả/diagnostics. Runtime xác nhận không
bảo đảm ghi đĩa thành công nếu addon báo lỗi lưu cấu hình.

UI yêu cầu schema `QuickTelex` Boolean và giá trị chuỗi hợp lệ. Addon alpha.10
vẫn cho dùng hai tùy chọn chính tả, nhưng khóa Gõ nhanh Telex. Sau nâng cấp gói,
lưu công việc rồi nạp lại framework theo desktop hoặc đăng xuất/đăng nhập để
module mới có hiệu lực; đóng UI cũ, mở lại và Đọc lại. Không tự restart daemon.

## Chọn có/không gạch chân

Trong cấu hình Fcitx5, mở cấu hình OpenKey (Telex/VNI, hoặc addon OpenKey), chọn
**Chế độ gõ / Input style**:

- **Preedit**: mặc định, giữ cơ chế gõ có composition để tương thích.
- **Direct**: thử nghiệm, commit từng ký tự và thay dấu qua surrounding-text,
  không gửi preedit khi ứng dụng hỗ trợ.

Lựa chọn áp dụng chung cho cả Telex và VNI. Khi lưu cấu hình, từ Preedit đang
gõ được commit một lần; từ Direct đã nằm trong tài liệu không bị commit lại.
Cấu hình lưu tại `$XDG_CONFIG_HOME/fcitx5/conf/openkey.conf` hoặc mặc định
`~/.config/fcitx5/conf/openkey.conf`, với `InputStyle=Preedit` hoặc `InputStyle=Direct`.

Direct yêu cầu ứng dụng hỗ trợ surrounding-text hợp lệ. Từ alpha.4, selection
có sẵn ở đầu từ (như address bar vừa nhận focus) được thay bởi phím mới qua
`commitString` của ứng dụng; không dùng lệnh xóa surrounding để xóa selection.
Sau đó chỉ theo dõi phần ký tự mới đã gõ. Chỉ phục hồi chữ đang chờ vẫn yêu cầu
con trỏ không có selection như mô tả dưới đây.
Nếu thiếu hỗ trợ ngay đầu từ, addon dùng Preedit. Nếu mất đồng bộ giữa từ, addon
ngừng thay dấu và gõ nguyên trạng đến ranh giới tiếp theo hoặc context hợp lệ mới;
từ đó có thể còn phím
Telex/VNI thô. Khi snapshot vẫn khớp vị trí cũ nhưng chờ quá lâu, phím chờ được
chuyển tiếp nguyên trạng. Nếu context/selection đã thay đổi, phần chữ chưa nhập
được giữ trong bảng phụ **Phím chưa nhập / Pending**: bỏ chọn vùng và đặt con trỏ,
rồi Enter xác nhận chèn tại vị trí hiện tại, Escape hủy. Không chèn khi vẫn có
selection hoặc surrounding-text chưa hợp lệ. Các shortcut/
phím điều hướng đang chờ bị hủy trong trường hợp mất ngữ cảnh; chỉ giữ phần chữ.
Timeout xác nhận là 250 ms, hàng đợi
tối đa 64 sự kiện; mục tiêu là không xóa nhầm nội dung khi client cập nhật chậm.
Snapshot có cùng nội dung/vị trí nhưng không có revision không phân biệt được
ACK mới với echo cũ; trường hợp mơ hồ được ngừng theo dõi thay vì tiếp tục sửa.

Space và ký tự phân cách in được trong Direct được commit theo cùng hàng đợi
để giữ đúng thứ tự nhiều từ, kể cả sau dấu câu.
Enter/Tab/shortcut vẫn được chuyển cho ứng dụng; sau một sự kiện làm đổi vị trí
không thể dự đoán, phần còn lại của burst có thể được gõ nguyên trạng.
Escape ở Direct chỉ kết thúc theo dõi từ, không xóa ký tự đã commit.
Ngoại lệ: khi bảng phục hồi đang mở, Enter/Escape là lệnh xác nhận/hủy chữ chờ,
không được chuyển sang ứng dụng; chữ gõ thêm được thêm vào phần chờ (tối đa 256).

Direct không tắt quy tắc Telex/VNI trong identifier. Ví dụ `console` có thể chứa
phím đặt dấu; chuyển English khi cần gõ code nguyên trạng. Chế độ này nhằm bỏ
composition kéo dài, không thay cấu hình IntelliSense hoặc bảo đảm mọi frontend
Electron đều cung cấp API cần thiết. Hãy thử gõ `print` chưa Space để kiểm tra gợi ý,
và kiểm tra Tab/Enter nhận completion, undo/redo trên VS Code thực tế.

### Chẩn đoán khi Direct không hoạt động trong một ứng dụng

Trong cấu hình OpenKey, bật tạm **Direct diagnostics (no text content)**
(`DirectDiagnostics=True` trong cấu hình). Mặc định tùy chọn này tắt.
Chỉ log reason/cause, trạng thái snapshot/pending/notified, định danh app/frontend và
capability flags; không log ký tự,
URL, nội dung tài liệu, vị trí con trỏ hoặc phần chữ đang chờ.

Các reason thường gặp:

- `direct:ready` / `direct:selection-start`: đã đi vào Direct.
- `fallback:no-surrounding-capability`: client chưa công bố capability cần thiết.
- `fallback:invalid-surrounding-text`: capability có nhưng context chưa hợp lệ.
- `fallback:preedit-word-in-progress`: từ hiện tại đã bắt đầu ở fallback Preedit;
  kết thúc từ trước khi thử Direct ở từ kế tiếp.
- `context-reset` / `context-ready`: bỏ sở hữu từ cũ khi context đổi, không buộc toggle EN–VI.
- `context-wait cause=late-known-echo` (alpha.6): snapshot cũ đã biết đến sau ACK.
  Tạm giữ lịch sử, không sửa theo snapshot cũ. Trước phím tiếp theo, cache phải
  khớp snapshot hiện tại đã xác nhận; nếu còn cũ thì bỏ lịch sử và bắt đầu từ mới.
- `context-invalidated` (alpha.6): ghi trạng thái trước khi bỏ lịch sử lúc idle.
  `cause=key-snapshot-not-current` là cache vẫn chưa khớp khi nhận phím tiếp theo;
  `surrounding-mismatch`/`surrounding-invalid` là cập nhật khác/không hợp lệ.
- `bypass:unconfirmed-edit` / `recovery:context-changed`: không tiếp tục sửa từ cũ
  vì edit chưa xác nhận hoặc context đã đổi; có thể cần phục hồi theo bảng Pending.
- `bypass:sensitive-field`: ứng dụng khai báo ô Password/Sensitive.
- `edit-aborted` (alpha.5): ghi trước khi dọn trạng thái; xem `cause` để phân biệt
  `ack-timeout` (hết 250 ms chờ xác nhận), `surrounding-mismatch` (snapshot không
  khớp), `surrounding-invalid` (snapshot không hợp lệ), `capability-withdrawn`,
  `queue-overflow` hoặc `prepare-rejected`. Không suy ra timeout chỉ từ dòng
  `bypass:unconfirmed-edit`.

`snapshot=stale` nghĩa là cache khớp một trạng thái cũ đã biết (kể cả sau ACK);
`unexpected` là trạng thái khác cả dự kiến lẫn các echo được nhận diện;
`invalid` là snapshot không dùng được (không nhất thiết thiếu capability).
`expected` khớp edit đang chờ; `initial`/`current`/`changed` mô tả trạng thái khi
không có edit chờ. `pending=1` là có edit chưa được xác nhận tại lúc log.
`notified=1` nghĩa là đã nhận ít nhất một sự kiện surrounding-text khi edit đang
chờ; với `ack-timeout snapshot=stale`, dùng trường này để phân biệt không nhận
sự kiện cập nhật (`0`) với có cập nhật nhưng vẫn là trạng thái cũ (`1`). Đây là
quan sát ở phía addon, không chứng minh lỗi nằm ở VS Code hay bridge/compositor.
Log chỉ phân loại, không chứa snapshot thực tế hay độ dài/vị trí trong tài liệu.

Từ alpha.7, `reference=confirmed` so sánh với snapshot đã xác nhận gần nhất;
`reference=expected` so với kết quả dự kiến của edit đang chờ (không phải ACK).
`reference=none` là chưa có mốc so sánh. Chỉ đọc các cờ thay đổi nếu
`comparable=1`; `comparable=0` nghĩa là thiếu mốc hoặc snapshot không hợp lệ,
không được hiểu các cờ `0` là không thay đổi.

- `text_changed=1`: chuỗi surrounding-text khác; chưa chứng minh tài liệu thực
  sự bị sửa (frontend có thể báo một cửa sổ văn bản khác).
- `cursor_changed=1`: offset con trỏ trong snapshot khác; không ghi offset.
- `anchor_changed=1`: offset đầu selection khác; không ghi offset.

Các cờ độc lập và có thể cùng bằng `1`. Khi snapshot bị cắt/rebase, offset cũng
có thể thay đổi mà người dùng không di chuyển con trỏ. Vì thế các cờ này giúp
khoanh vùng, chưa đủ để cho phép bỏ kiểm tra an toàn hoặc quy lỗi cho VS Code.
Đọc dòng `context-invalidated`/`edit-aborted` trước khi tracker bị dọn; dòng
`context-reset reference=none comparable=0` sau đó không còn mốc để so sánh.

Nếu `app=gnome-shell frontend=ibus`, đây là định danh bridge mà Fcitx5 nhìn thấy,
không chứng minh ứng dụng đích là GNOME Shell hay riêng VS Code. Khi thử alpha.9,
ghi giờ bắt đầu/kết thúc, chọn Direct và gõ `coongj hoaf` trong tài liệu trống
không chứa dữ liệu nhạy cảm, rồi lấy log cùng khoảng thời gian và báo kết quả.
Sau đó thử lại Preedit riêng để đối chiếu; không thay cấu hình nhiều ứng dụng
cùng lúc. Alpha.9 không thay timeout, kiểm tra suffix/cursor/selection hoặc thứ tự edit.
Không khôi phục lịch sử chỉ vì vùng trước con trỏ có cùng hậu tố: không đủ bằng
chứng để phân biệt cửa sổ surrounding-text đổi với di chuyển sang từ giống nhau.

Log đi tới stderr của tiến trình Fcitx5. Nếu desktop launcher chuyển stderr vào
user journal, có thể đọc bằng:

```bash
journalctl --user --since '10 minutes ago' --no-pager | grep 'OpenKey Direct:'
```

Nếu không có log, chưa thể kết luận addon không chạy: nơi thu stderr tùy launcher.
Không chạy thêm daemon hoặc `fcitx5 -r` chỉ để lấy log khi chưa biết cách desktop
khởi chạy Fcitx5. Khi báo lỗi, gửi reason tương ứng và thông tin GNOME/KDE,
Wayland/X11, native Wayland/XWayland của ứng dụng nếu biết; tắt diagnostics sau đó.

### Đối chứng riêng cho VS Code

Với alpha.7, người dùng xác nhận profile `accessible wayland` gõ đúng một lượt,
không gạch chân và giữ gợi ý; `textarea wayland` chưa chuyển dấu. Cửa sổ XWayland
qua GTK/IBus thiếu surrounding capability nên fallback Preedit có gạch chân. Dùng
[trình chạy profile đối chứng](UBUNTU_VSCODE_INPUT_INVESTIGATION.md) để phân biệt
đường EditContext/textarea/accessibility, không sửa profile VS Code chính.

Trong profile thử `accessible`, các settings là:

```json
{
  "editor.editContext": false,
  "editor.accessibilitySupport": "on"
}
```

Đây là cấu hình tương thích tạm thời trên máy đã thử, không bảo đảm mọi máy;
accessibility support có thể thay đổi hành vi editor. Ca sửa từ alpha.8 và các
thao tác cơ bản đã được người dùng báo đạt trên profile này.

Alpha.8: `coongg` → `côngg` → Backspace → `công` → `j` → `cộng` đã được người
dùng thử đạt. Backspace xóa ký tự hiển thị, không hoàn tác phím Telex cuối. Vì vậy để sửa
`congg` (không có `oo`), còn cần nhập đủ phím tạo `ô`; không mặc định bỏ một `g`
rồi thêm `j` sẽ tạo dấu mũ. Thử thêm xóa `g` trong `cộng`, gõ lại `g`, đổi dấu,
gõ nhanh, selection/focus và undo/redo.

Nếu edit sẽ quay lại snapshot đã từng gặp, alpha.8 không từ chối ngay như alpha.7;
thay vào đó chờ surrounding-text notification sau khi bắt đầu edit. Cache trùng
khớp nhưng không có notification chưa được tính là ACK (`ack_requires_update=1`).
Phím sau vẫn xếp hàng; mất context hoặc quá 250 ms vẫn đi vào đường bảo vệ cũ.
Giao thức không có revision cho snapshot: notification trễ có nội dung hoàn toàn
giống kết quả mong đợi vẫn không phân biệt tuyệt đối được với ACK mới. Không coi
điều kiện notification là bằng chứng chống mọi race; cần desktop QA trước beta.

### Xóa Space vừa gõ để sửa tiếp từ — alpha.9, Direct

Ví dụ: `coong` → Space → Backspace → `j` phải thành `cộng`; hoặc `ho` → Space →
Backspace → `af` thành `hoà`. Với VNI: `co6ng` → Space → Backspace → `5` thành `cộng`.
Chỉ phục hồi sau khi đã xóa hết khoảng trắng ngay sau từ đang được quản lý.

Giới hạn: giữ một từ vừa kết thúc và tối đa 32 dấu cách ASCII liên tiếp, không
lưu lịch sử cả tài liệu, không dò ngược từ đã nhập trước đó. Khi gõ từ mới, dấu
câu, Tab/Enter, shortcut, đổi con trỏ/selection, đổi chế độ hoặc mất focus, lịch
sử này bị bỏ. Dán/undo làm context khác cũng bỏ lịch sử; không tự dựng lại từ.
Tính năng này chỉ áp dụng Direct, không thay hành vi Preedit dự phòng.

Trước khi xóa Space, kiểm tra snapshot hiện tại và toàn bộ hậu tố gồm từ cũ cùng
chuỗi dấu cách; xóa/sửa vẫn đợi ACK như các edit khác. Log `boundary-restored`
ghi nhận bắt đầu khôi phục lịch sử sau Space cuối (không chứng minh edit đã ACK).
Đã có test Telex/VNI, nhiều Space, Unicode, ACK trễ, đổi selection/caret/focus,
giới hạn lịch sử. Người dùng đã thử hai ca hướng dẫn trên desktop và báo đúng
mong đợi; chưa mở rộng nghiệm thu sang mọi ứng dụng/backend.

## Gỡ và nâng cấp

Chuyển sang input method khác, bỏ OpenKey khỏi danh sách trong cấu hình Fcitx5,
rồi chạy:

```bash
sudo apt remove fcitx5-openkey
```

Gói không xóa profile Fcitx5 hoặc dữ liệu người dùng. Đăng xuất/đăng nhập nếu
daemon còn giữ module cũ. Để nâng cấp, dùng `sudo apt install ./<gói-mới>.deb`;
việc cài không tự khởi động lại phiên gõ.

Nếu trước đây cài module bằng `sudo cmake --install`, đọc install manifest của
bản đó trước; APT chỉ quản lý các đường dẫn nằm trong gói `.deb`, không xóa bản
cũ đặt ở prefix khác như `/usr/local`.

## Tự build gói

```bash
sudo apt install build-essential cmake ninja-build dpkg-dev libfcitx5core-dev
bash scripts/build-linux-deb.sh
```

Script build Release, chạy CTest, tạo `.deb` bằng CPack và checksum SHA-256,
giải nén vào build riêng rồi kiểm tra discovery/nạp addon qua Fcitx5 thật với
frontend giả. Không dùng sudo hoặc cài gói khi build/test.

Các file đầu ra nằm trong `dist/linux/`. Muốn debug dependency và payload:

```bash
dpkg-deb --info 'dist/linux/fcitx5-openkey_3.1.0~alpha.9-1_amd64.deb'
dpkg-deb --contents 'dist/linux/fcitx5-openkey_3.1.0~alpha.9-1_amd64.deb'
```

Đã kiểm tra local trên Ubuntu 26.04 amd64: 7/7 CTest, checksum, payload,
dependency tự sinh, nạp addon từ `.deb` đã giải nén và `apt-get --simulate install`.
Chưa chạy cài/gỡ/nâng cấp bằng APT thật hoặc kiểm thử ứng dụng desktop. CI có
job build gói Ubuntu 24.04 riêng; thêm workflow chưa đồng nghĩa job remote đã chạy.

Tham khảo kỹ thuật đóng gói: [CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html).
