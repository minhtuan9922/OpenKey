# Nghiệm thu bộ gõ Ubuntu — Fcitx5

## Trạng thái

Đã kiểm chứng bằng test headless: discovery của descriptor trong install prefix,
nạp module, preedit/commit, xử lý Backspace, focus-out cho client preedit,
ClientUnfocusCommit và server preedit, chuyển Telex/VNI và quay lại ô nhập cũ.
Đây là Fcitx event pipeline thật với frontend giả, không phải gõ trong desktop.

Chưa nghiệm thu trên GNOME/KDE, X11/Wayland và các ứng dụng bên dưới. Môi trường
phát triển hiện tại là tty, không có DISPLAY hoặc WAYLAND_DISPLAY.

Alpha.3: đã có lựa chọn Preedit/Direct và test headless Direct qua addon/pipeline
thật, gồm xác nhận surrounding-text chậm/cũ, nhiều từ, timeout, đổi context và
phục hồi chữ chờ. Chưa xác nhận Direct/IntelliSense trên VS Code thực tế.

### Phản hồi người dùng — 2026-11-03, alpha.3 Direct

| Ứng dụng/vùng nhập | Kết quả báo lại |
|---|---|
| Text editor trên desktop | Direct hoạt động ổn định; tên editor/toolkit chưa xác định |
| VS Code | Không hoạt động; biểu hiện cụ thể và backend chưa xác định |
| Firefox, thanh địa chỉ | Ban đầu không hoạt động; toggle EN–VI mới hoạt động |
| Chrome, thanh địa chỉ | Ban đầu không hoạt động; toggle EN–VI mới hoạt động |

Chưa có thông tin GNOME/KDE, Wayland/X11 hoặc backend native/XWayland cho các
kết quả này. Không gán chúng vào một ô môi trường cụ thể trong ma trận bên dưới.
Alpha.4 đã sửa hai trường hợp headless: selection ban đầu và context đổi lúc
không còn edit chờ. Cần người dùng xác nhận lại trên browser/VS Code thật.

Ca nghiệm thu bổ sung: Ctrl+L hoặc click thanh địa chỉ, giữ selection cũ rồi gõ
ngay Telex/VNI; thử cả bôi đen từ trái/phải, address rỗng và address có sẵn. Đổi
qua lại ô nhập trong trang và address bar nhiều lần, không toggle EN–VI.

### Bổ sung kết quả VS Code và chẩn đoán alpha.5

Người dùng xác nhận Direct giữ nguyên `coongj`/`hoaf`, popup gợi ý code vẫn hiện;
Preedit chuyển dấu được nhưng không hiện gợi ý trong lúc composition. Đã kiểm
tra môi trường hiện tại: VMware, GNOME/Wayland, VS Code dùng backend Wayland.
Log qua bridge `app=gnome-shell frontend=ibus` có `bypass:unconfirmed-edit` nhưng
không định danh được ứng dụng đích; chưa xác nhận nguyên nhân cụ thể của VS Code.

Alpha.5 chỉ bổ sung `edit-aborted` với cause/snapshot/pending và test headless
cho diagnostics; không đổi thuật toán Direct hoặc thời gian chờ 250 ms.
Cần thử riêng VS Code trong khoảng giờ xác định theo
[hướng dẫn chẩn đoán](UBUNTU_INSTALL.md#chẩn-đoán-khi-direct-không-hoạt-động-trong-một-ứng-dụng).
Chưa nghiệm thu Direct hoặc khả năng vừa chuyển dấu vừa giữ IntelliSense.

### Alpha.5 retest và alpha.6

Lần thử người dùng lúc 19:59:33–19:59:35 ghi nhận `direct:ready` xen kẽ
`context-reset`; không có timeout/edit-aborted trong khoảng này. Vì alpha.5 log
reset sau khi dọn tracker, chưa xác định snapshot đó là echo cũ hay context khác.

Alpha.6 sửa một trường hợp tái hiện headless: ACK thành công, nhận lại snapshot
cũ đã biết, rồi cache trở lại hiện tại trước phím kế tiếp. Giữ lịch sử trong
khoảng này, nhưng vẫn reset nếu cache chưa khớp khi nhận phím mới. Test Telex/VNI
xác nhận chuyển dấu và không preedit; test cache cũ tại phím mới vẫn không cho
phép sửa từ cũ. Chưa kiểm thử lại trên desktop. Không coi đây là bằng chứng đã
sửa VS Code; cần đọc `context-wait`/`context-invalidated` nếu lỗi tiếp diễn.

### Alpha.6 retest và alpha.7 diagnostics

Log lần thử alpha.6 lúc 20:05:40–20:05:48 vẫn reset do `surrounding-mismatch`,
`snapshot=changed pending=0 notified=1`, không phải echo cũ đã được lưu.
Không coi alpha.6 là đã sửa VS Code.

Alpha.7 bổ sung cờ `text_changed`/`cursor_changed`/`anchor_changed`, chỉ có ý nghĩa
khi `comparable=1`, kèm `reference=confirmed|expected|none`. Không log nội dung,
hash, độ dài hoặc offset. Test helper và logger kiểm tra từng thành phần đổi riêng,
snapshot thiếu và mốc expected chưa ACK; không thay đổi thuật toán Direct.
Chưa có kết quả desktop alpha.7. Xem [hướng dẫn đọc log](UBUNTU_INSTALL.md).

Cập nhật sau retest: alpha.7 vẫn reset với cả ba cờ thay đổi hoặc chỉ
`text_changed=1`. XWayland gõ có gạch chân do thiếu surrounding capability và
fallback Preedit, không phải Direct thành công. Ma trận đối chứng profile riêng
và bằng chứng mã nguồn ở [điều tra VS Code](UBUNTU_VSCODE_INPUT_INVESTIGATION.md).

### Profile accessible và alpha.8 correction

Người dùng xác nhận cả hai profile có gợi ý, nhưng textarea/off không chuyển dấu
(hành vi như English), textarea/accessibility on gõ đúng một lượt được `cộng hoà`,
không gạch chân. Ca accessible chưa đạt sửa từ: Backspace rồi đặt dấu bị mất lịch sử.
Log có `prepare-rejected snapshot=current`.

Alpha.8 sửa ca headless `coongg` → Backspace → `j` thành `cộng`, cùng VNI,
xóa/gõ lại và đổi dấu quay lại snapshot đã gặp. Cache matching không notification
chưa giải phóng phím chờ; selection/context đổi vẫn giữ đường bảo vệ. Chưa có
desktop retest; không coi cả ca Direct là đạt chỉ vì sửa headless qua test.

### Alpha.8 desktop và alpha.9 Space correction

Người dùng xác nhận alpha.8 sửa Backspace đúng mong đợi, rồi xác nhận Tab/Enter
nhận gợi ý, undo/redo, đổi focus và gõ nhanh đúng mong đợi trên profile accessible.
Không gán kết quả này cho mọi ứng dụng/backend hoặc các phiên bản khác.

Yêu cầu mới: xóa Space gõ nhầm rồi đặt dấu tiếp. Alpha.9 giữ một từ trước boundary
và tối đa 32 Space, phục hồi sau khi xóa hết Space trong context còn khớp.
Test `coong` → Space → Backspace → `j` ra `cộng`, `ho` → Space → Backspace →
`af` ra `hoà`, VNI tương đương. Có test ACK trễ và context/selection/caret/focus
đổi để tránh sửa từ cũ. Người dùng đã xác nhận alpha.9 xóa Space rồi sửa tiếp từ
đúng mong đợi trên profile accessible; chưa thay thế ma trận backend/ứng dụng.

## Alpha.12 — Word boundary / first key / Wayland passthrough (headless)

Headless Direct bổ sung: Enter/Space/dấu câu cắt từ, Home/End và Ctrl+A không bị nuốt khi pending ACK;
ký tự Direct đầu tiên dùng native key; Direct không Preedit capability thì passthrough.
**Chưa** thay thế kết quả desktop 2026-11-05; cần retest LibreOffice/Qt/browser/terminal.

## Ca bổ sung cho Direct

- Chọn **Chế độ gõ / Input style → Direct** theo [hướng dẫn cài](UBUNTU_INSTALL.md).
- Gõ `print` chưa Space: chữ phải ở tài liệu, không ở preedit; xác minh popup
  IntelliSense cập nhật ngay khi gõ. Kiểm tra inline completion riêng.
- Gõ Telex/VNI trong comment/string, dùng Backspace, nhận completion bằng Tab/Enter,
  di chuyển chuột/cursor, chọn vùng, dán, undo/redo, đổi focus và đổi kiểu gõ.
- Gõ nhanh nhiều từ và nhiều dòng; ghi nhận fallback nếu client thiếu capability
  hoặc ACK quá chậm. Không được tự thay dấu trong vùng đã chọn hoặc từ cũ ngoài
  đoạn đang quản lý.
- Nếu bảng **Phím chưa nhập / Pending** xuất hiện sau đổi selection, kiểm tra
  nội dung tài liệu không tự bị ghi đè; Enter chỉ chèn sau xác nhận, Escape hủy.
- Chuyển Preedit ↔ Direct khi đang gõ: không lặp từ, không mất chữ chưa xác nhận,
  không xóa panel thuộc một input method khác.
- Với identifier có phím Telex đặt dấu (ví dụ `console`), phân biệt quy tắc gõ
  tiếng Việt với lỗi completion. Dùng English khi cần giữ nguyên code.

Tất cả ca UI trên vẫn cần người kiểm thử chạy trên desktop; test headless không
đủ để đánh dấu đạt IntelliSense, undo grouping hoặc tương thích Electron.

## Chuẩn bị

1. Build, test và cài thử theo [Linux README](../Sources/OpenKey/linux/README.md).
2. Lưu công việc đang soạn thảo trước khi đổi cấu hình input method.
3. Trong phiên desktop đang dùng Fcitx5, thêm OpenKey Telex và OpenKey VNI.
4. Ghi commit, Ubuntu release, desktop, session type, phiên bản Fcitx5,
   phiên bản ứng dụng và backend của ứng dụng (native Wayland hay XWayland).
5. Với mỗi ca kiểm thử, ghi PASS/FAIL, chuỗi đã gõ và kết quả thực tế; chỉ dùng
   văn bản thử nghiệm, không thu nội dung cá nhân hoặc mật khẩu.

Không cần và không được coi việc đổi input framework đang dùng là một phần của
test tự động. Hướng dẫn desktop chính thức:
[Fcitx5 setup](https://fcitx-im.org/wiki/Setup_Fcitx_5) và
[Fcitx5 Wayland](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en).

## Ca chức năng

| Ca | Thao tác | Kỳ vọng |
|---|---|---|
| Telex | Gõ `tieengs vieetj` rồi Space | `tiếng việt `, không mất/lặp phím |
| VNI | Gõ `tie6ng1 vie6t5` rồi Space | `tiếng việt ` |
| Chữ hoa | Gõ `TIEENGS` bằng Shift và Caps Lock | `TIẾNG` |
| Backspace | Gõ `tieengs`, Backspace, `g` | `tiến` rồi `tiếng` |
| Đặt lại dấu | Gõ `hoasn`, Backspace | `hoán` rồi `hoá` |
| Escape | Gõ một từ chưa commit rồi Escape | Hủy từ đang preedit, không xóa nội dung cũ |
| Space/Enter/dấu câu | Kết thúc từ bằng mỗi phím | Commit một lần, phím kết thúc vẫn đến ứng dụng |
| Điều hướng | Gõ từ rồi Left/Right/Home/End | Commit một lần trước khi di chuyển con trỏ |
| Shortcut | Gõ từ rồi Ctrl+A/C/V hoặc shortcut ứng dụng | Không nuốt shortcut, không lặp từ |
| Focus-out | Gõ `tieengs` chưa Space, bấm sang ô khác | Từ cũ xuất hiện đúng một lần, không sang ô mới |
| Focus quay lại | Quay lại ô cũ và gõ `dduowngf` | Từ mới đúng, không giữ trạng thái dấu của từ cũ |
| Đổi kiểu gõ | Đang gõ VNI, chuyển Telex rồi gõ từ mới | Commit từ cũ một lần; kiểu gõ mới có hiệu lực |
| Password | Gõ trong ô thử nghiệm loại Password/Sensitive | Không có preedit hoặc biến đổi tiếng Việt |
| Dán/chuột | Dán text hoặc đổi vị trí bằng chuột khi đang gõ | Không mất/lặp từ; ghi rõ hành vi nếu client gửi reset |
| Giới hạn | Gõ từ dài hơn 256 sự kiện | Commit theo phần, không treo, không mất nội dung |

## Ma trận ứng dụng — tất cả chưa chạy

| Môi trường | Firefox/Chrome | VS Code | LibreOffice | GTK editor | Qt editor |
|---|---|---|---|---|---|
| GNOME Wayland | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| GNOME X11 nếu có | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| KDE Wayland | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| KDE X11 nếu có | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |

Nếu desktop không cung cấp phiên X11 thì ghi N/A kèm lý do, không coi là PASS.
Nếu ứng dụng chạy qua XWayland thì ghi rõ; kết quả đó không chứng minh native Wayland.

## Điều kiện chuyển sang beta

- Các ca chính trên desktop mục tiêu phải có kết quả thực tế.
- Không có lỗi mất/lặp ký tự khi commit, đổi focus và đổi input method.
- Có phép đo độ trễ cho replay và lộ trình engine context riêng.
- Các giới hạn chưa hỗ trợ được giữ rõ trong tài liệu và UI.

Theo dõi triển khai tại [task Ubuntu](UBUNTU_TASKS.md).
