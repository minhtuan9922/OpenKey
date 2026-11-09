# NativeDirect — thử nghiệm alpha.38 cho Ubuntu 26

Addon 0.2.29, gói `fcitx5-openkey` alpha.38; dùng tiếp UI preview.2. Mục tiêu:
chữ xuất hiện ngay tại con trỏ, không ô nổi và không preedit từ OpenKey. Đây
là bản thử tiếp theo sau [CommitOnly alpha.35](UBUNTU_COMMIT_ONLY.md), chưa
nghiệm thu khả năng gõ trên desktop.

## Hành vi

Chọn `InputStyle=NativeDirect` trong cấu hình OpenKey của Fcitx hoặc chọn
**Gõ trực tiếp, không ô nổi (NativeDirect, thử nghiệm)** trong UI mới rồi Áp
dụng. Telex/VNI dùng chung chế độ. Mặc định vẫn Preedit; không tự đổi cấu hình
hay khởi động lại daemon khi build/cài gói.

- Ký tự đầu, các ký tự nối thêm không cần biến đổi, Backspace thông thường,
  Space/Enter/Tab, Escape và shortcut được ứng dụng nhận bằng phím gốc.
- Khi cần đổi dấu, chỉ sửa nếu ứng dụng đã gửi cập nhật surrounding-text khớp
  **toàn bộ snapshot dự kiến, con trỏ và selection**. Chỉ khớp hậu tố không đủ;
  cache khớp mà chưa có notification sau edit cũng không đủ.
- Ngoại lệ giới hạn từ alpha.38 cho ô Url: notification xác nhận toàn bộ phần
  đã gõ tại cuối tài liệu, kèm phần đuôi được chọn từ con trỏ đến hết nội dung.
  Bộ gõ giữ riêng snapshot thực này, không đưa dự đoán vào cache của client.
  Phím gõ tiếp thay vùng chọn theo semantics thông thường. Từ beta.2~alpha.2,
  khi đổi dấu cần xóa chữ trước con trỏ mà gợi ý đuôi còn được chọn, phím đi
  nguyên dạng và từ giữ raw (xem mục beta.2~alpha.2 bên dưới); chỉ commit
  không cần xóa mới được gửi. Từ beta.2~alpha.3, phím chữ Latin dùng một
  BackSpace forward để xóa gợi ý trước rồi sửa sau xác nhận (mục alpha.3). Không xóa vùng chọn chỉ vì một notification đến.
  Không áp dụng cho ô web/terminal khác, selection ở giữa, phần đầu thay đổi
  hoặc cache chưa báo cập nhật.
- Không dùng một snapshot dự đoán làm ACK. Không xếp hàng các phím đang gõ,
  không forward/synthesize phím, không đọc clipboard, không global hook.
- Nếu mất xác nhận hoặc thiếu hỗ trợ, phần còn lại của từ đi nguyên phím gốc
  và không sửa ngược. Sau ranh giới từ, thử lại với ngữ cảnh mới. Có thể thấy
  phím dấu thô như `tieengs` hoặc một từ chuyển dấu dở, nhưng không giữ chúng
  trong ô nổi để chờ Space.
- Context có cờ Terminal, Email hoặc định danh LibreOffice thuộc nhánh
  không sửa chữ: giữ nguyên phím gốc. Không tự chuyển EN/VI. Những ngữ cảnh
  không khai báo cờ/định danh này không thể được nhận diện chắc chắn.
- Từ alpha.37, ô mang cờ Url dùng cùng đường kiểm chứng như ô trên trang web.
  Alpha.36 chủ động bỏ qua Url nên không gõ dấu được ở thanh địa chỉ Chrome.
  Dùng chữ ghép sẵn NFC, không đổi sang mẹo thêm dấu kết hợp. Gợi ý đuôi được
  xử lý theo điều kiện alpha.38 ở trên; các thay đổi khác vẫn dừng sửa.
- Đổi focus, reset, IM hoặc cấu hình bỏ lịch sử sửa chữ, không ghi lại từ đã
  có trong ứng dụng. Trường nhạy cảm/mật khẩu bỏ qua bộ gõ khi được khai báo.
- Engine giữ các quy tắc Telex/VNI, chính tả, đặt dấu và QuickTelex đã chọn.
  Riêng phím đầu biến đổi ngay (`w` → `ư`) được hiển thị nguyên phím gốc trước;
  biến đổi khi phím tiếp theo/ranh giới xác nhận được ký tự đầu đã tới đích.
- Escape là phím của ứng dụng; không xóa từ đã nhập như khi hủy preedit.

## So với các chế độ trước

| Chế độ | Chữ khi đang gõ | Khi frontend không hỗ trợ |
| --- | --- | --- |
| Preedit | Composition trong ô nhập hoặc bảng bộ gõ | Phụ thuộc frontend |
| Direct cũ | Ghi/sửa ngay, có các nhánh tương thích riêng | Có thể fallback preedit |
| CommitOnly | Trong bảng nổi, hết từ mới gửi | Không sửa chữ đã gửi |
| NativeDirect | Phím gốc hiển thị ngay; sửa dấu có kiểm chứng | Giữ phím nguyên dạng, không popup |

NativeDirect giúp ứng dụng nhận sự kiện gõ bình thường để kích hoạt gợi ý.
Gợi ý khi biến đổi dấu, undo/redo và thứ tự các sự kiện vẫn cần nghiệm thu.
Không thể coi hỗ trợ phím gốc là bằng chứng mọi chức năng editor đã đạt.

## Giới hạn phải kiểm tra

1. Không sửa được giao thức của ứng dụng. Nếu ứng dụng báo surrounding-text
   khớp nhưng bỏ lệnh xóa/chèn, lần biến đổi đầu vẫn có thể lặp hoặc thiếu
   chữ. Phím sau sẽ dừng sửa nếu snapshot không khớp; không thể hoàn tác đáng
   tin cậy phần đã phát ra. Không tuyên bố sửa toàn bộ terminal xterm.js.
2. Terminal VS Code/Cursor trên bridge GNOME có thể báo cùng capability với
   editor. NativeDirect không đoán qua `82`, tên cửa sổ hoặc WordCompletion.
   Terminal không khai báo Terminal vẫn có rủi ro ở mục 1; dùng EN cho lệnh.
3. Cập nhật surrounding-text trễ khi gõ nhanh làm từ đi raw. Không tăng timeout
   hoặc đoán ngữ cảnh để che điều này. Không có bảo đảm mọi từ đều chuyển dấu.
4. Snapshot không có revision của tài liệu. Notification cũ trùng hoàn toàn
   một trạng thái mới vẫn không phân biệt được tuyệt đối; không coi đây là
   giải pháp loại bỏ mọi race.
5. App có thể tự gạch chân chính tả/link. OpenKey không gửi preedit trong chế
   độ này, nhưng không can thiệp cách ứng dụng trang trí văn bản đã ghi.

## Cài và quay lại chế độ cũ

Gói thử được tạo trong `dist/linux/`. Kiểm checksum tại đó rồi cài từ terminal
desktop nếu muốn nghiệm thu:

```bash
cd dist/linux
sha256sum -c fcitx5-openkey_3.1.0~alpha.38-1_amd64.deb.sha256
sha256sum -c openkey-settings_3.1.0~preview.2-1_amd64.deb.sha256
sudo apt install ./fcitx5-openkey_3.1.0~alpha.38-1_amd64.deb ./openkey-settings_3.1.0~preview.2-1_amd64.deb
```

Lưu nội dung đang gõ rồi đăng xuất/đăng nhập để nạp addon mới. Trong công cụ
cấu hình Fcitx chọn NativeDirect, hoặc mở UI preview.2 và chọn chế độ đó. UI
cũ chỉ hiểu hai/ba chế độ không hỗ trợ schema mới; dùng công cụ cấu hình Fcitx
nếu chưa nâng UI. UI mới đọc được schema hai, ba và bốn chế độ, chỉ hiện lựa
chọn được addon đang chạy hỗ trợ. Readback runtime không chứng minh đã lưu bền.

Muốn quay lại: chọn Preedit, Direct hoặc CommitOnly bằng cùng bộ chọn. Đổi
khi đã kết thúc từ. Không cần extension/helper terminal cũ. Xem thêm
[cài/gỡ Ubuntu](UBUNTU_INSTALL.md).

## Nghiệm thu desktop

| Ca | Kỳ vọng | Trạng thái |
| --- | --- | --- |
| VS Code/Cursor editor: `tieengs vieetj` | Chữ ở con trỏ, không bảng nổi; `tiếng việt` khi xác nhận đủ | Chưa thử |
| Editor: `print`, `console.`, Tab/Enter | Gợi ý và nhận completion đúng, không lặp từ | Chưa thử |
| Editor/chat: Backspace, Ctrl+A, Escape, Undo/Redo | Không sửa nhầm/chèn lại từ | Chưa thử |
| Click field khác/Alt+Tab giữa từ | Không ghi từ cũ vào field mới | Chưa thử |
| Terminal: focus mới rồi EN `pwd`, `ls`; Enter nhiều dòng | Không mất từ đầu; phím/lệnh nguyên dạng | Chưa thử |
| Terminal VI | Ghi kết quả thực tế; không coi là đã hỗ trợ | Chưa thử |
| Chrome: ô nhập trang web trên alpha.36 | Chuyển dấu | Người dùng xác nhận được |
| Chrome: thanh địa chỉ trên alpha.36 | Chuyển dấu | Người dùng xác nhận FAIL; bị chặn Url |
| Chrome: thanh địa chỉ trên alpha.37 | Chuyển dấu toàn câu | FAIL: người dùng thấy `coongj hoaf xã hội chủ nghia` |
| Chrome: thanh địa chỉ trên alpha.38 | Câu tiếng Việt đủ dấu khi có gợi ý đuôi; không popup/lặp chữ | FAIL: người dùng thấy `coôngj hoà xã hội chủ` |
| GTK/Qt/Firefox/LibreOffice | Chữ hiện ngay; ghi khả năng chuyển dấu và phần đi raw | Chưa thử |
| Wayland/XWayland và Snap/Flatpak | Kiểm từng ứng dụng/backend riêng | Chưa thử |
| Gõ nhanh và trường mật khẩu | Không giữ phím; không popup; trường nhạy cảm nguyên phím | Chưa thử |

Thử trong tài liệu trống, chưa dùng trên lệnh/nội dung quan trọng. Tại terminal,
không Enter chạy chuỗi thử tiếng Việt như lệnh shell. Ghi rõ app, backend, kiểu
gõ và chuỗi trước/sau nếu lỗi. Một môi trường đạt không đồng nghĩa mọi app đạt.

## Kiểm thử tự động

Frontend giả lập kiểm Telex/VNI, native ASCII, nhiều dòng/focus-entry, Backspace,
selection, đổi context dù hậu tố giống nhau, ACK trễ/cache-only, hồi phục sau
Space, password, đổi mode và không có client/server preedit/forwardKey. Test
mới chạy trên bản sao addon alpha.35 thất bại tại native ASCII; bản mới đạt.
Pipeline Fcitx thật được kiểm qua frontend giả cho NativeDirect và focus-out.
Các kết quả này không thay cho kiểm thử GNOME/VS Code/PTY trên desktop.

Kết quả local trên Ubuntu 26.04.1, 2026-11-05: Release addon đạt 8/8 CTest;
toàn bộ 15 mục UI đã đạt sau khi sửa và chạy lại ca bộ chọn thường/HiDPI.
Có fault injection frontend bỏ lệnh xóa: kiểm bộ gõ dừng sửa ở phím sau,
không tuyên bố kết quả lần sửa đầu đã đúng. Phiên kiểm thử headless không có
DISPLAY/WAYLAND_DISPLAY; mọi ca desktop ở bảng trên vẫn chờ nghiệm thu.

Đối chiếu API Fcitx 5.1.19:
[IBus frontend](https://github.com/fcitx/fcitx5/blob/5.1.19/src/frontend/ibusfrontend/ibusfrontend.cpp)
gửi CommitText/DeleteSurroundingText riêng, nhận SetSurroundingText từ client;
[InputContext](https://github.com/fcitx/fcitx5/blob/5.1.19/src/lib/fcitx/inputcontext.cpp)
không cung cấp revision tài liệu nguyên tử để xác nhận giao dịch thay chữ.
Không dùng dữ liệu cache tự dự đoán để tuyên bố ứng dụng đã thực hiện edit.

## Phản hồi Chrome — 2026-11-05

Người dùng báo chưa gõ được tiếng Việt trong Chrome. Máy đã cài addon alpha.36,
UI preview.2, cấu hình NativeDirect và diagnostics bật. Tệp đính kèm chỉ chứa
`editContext`, không chứa log gõ hoặc kết quả đối chứng.

Journal của bridge GNOME có `capabilities=4178` với `unsupported-context` và
`82`/`262226` với `unconfirmed-context`. `4178` bao gồm cờ Url: alpha.36 chủ
động giữ phím nguyên dạng ở đó. Các giá trị còn lại không đủ xác nhận app/ô
nhập; `program=gnome-shell` không phân biệt Chrome, editor và terminal. Chưa
gán từng dòng log cho Chrome hoặc kết luận lỗi Chrome đã sửa.

Đã bổ sung diagnostics trong source để tách `email-context`,
`no-surrounding-capability`, `awaiting-surrounding-update`, `snapshot-mismatch`
và `raw-word-in-progress`. So sánh NativeDirect dùng snapshot của chính nó,
không còn báo `initial` từ tracker Direct không hoạt động. Chỉ log enum/boolean,
không log nội dung, phím, URL hay vị trí con trỏ.

Người dùng đã xác nhận ô trong trang web gõ được tiếng Việt, chỉ thanh địa chỉ
không được. Candidate alpha.37 bỏ cổng từ chối Url của NativeDirect; giữ nguyên
guard snapshot/notification, không thay đường Direct cũ hoặc mọi ô nhập thành
heuristic Chrome. UI preview.2 vẫn dùng được, không thêm setting/mode mới.

Regression mới dùng cap 4178, frontend ibus, Telex/VNI và nội dung chọn sẵn:
`ddaay laf tieengs vieetj` / `d9a6y la2 tie6ng1 vie6t5` phải ra NFC
`đây là tiếng việt`, không client/server preedit, không synthesize phím.
Có Backspace, Ctrl+A, Enter, gợi ý chọn phần đuôi, và frontend bỏ lệnh xóa.
Ca bỏ xóa chỉ kiểm dừng sửa tiếp, không chứng minh lần sửa đầu đúng. Nếu Chrome
thật không thực hiện delete, nguy cơ lặp chữ vẫn còn; chưa tuyên bố desktop PASS.

Đối chiếu thêm [InputMethodAuraLinux của Chromium](https://chromium.googlesource.com/chromium/src/+/HEAD/ui/base/ime/linux/input_method_auralinux.cc)
và [Textfield](https://chromium.googlesource.com/chromium/src/+/HEAD/ui/views/controls/textfield/textfield.cc):
có đường xử lý delete khi không có composition. Đây là cơ sở để không coi cờ
Url là bằng chứng API không hoạt động, không phải xác nhận đường thực thi của
Chrome 157 đang chạy. Nghiệm thu thực tế vẫn cần sau khi nạp alpha.37.

## Alpha.38 — từ bị giữ nguyên khi thanh địa chỉ thêm gợi ý đuôi

Desktop alpha.37: người dùng báo `coongj hoaf xã hội chủ nghia`. Chuỗi phím
gốc chưa được xác nhận; regression dùng chuỗi Telex chuẩn
`coongj hoaf xax hooij chur nghiax` → `cộng hoà xã hội chủ nghĩa`.
Tệp đính kèm chứa settings VS Code (`editor.editContext=false`, accessibility
bật), không phải log Chrome và không được chỉnh sửa.

Journal của daemon alpha.37 có cap 4178, `awaiting-surrounding-update`,
`text_changed=1 cursor_changed=0 anchor_changed=1`, sau đó
`raw-word-in-progress`. Mẫu này phù hợp với phần đuôi gợi ý được chọn, chưa
chứng minh nội dung cụ thể. Không đọc/log nội dung thanh địa chỉ để suy đoán.

Candidate alpha.38 chấp nhận notification có hình dạng giới hạn:

- Snapshot dự kiến trước đó có con trỏ thu gọn tại cuối nội dung.
- Phần từ đầu đến con trỏ thực khớp toàn bộ snapshot dự kiến, không chỉ từ
  hoặc hậu tố. Con trỏ không đổi.
- Anchor ở cuối nội dung thực, chọn riêng phần đuôi thêm vào, hướng tiến.
- Chỉ ô Url; snapshot thực lúc xử lý phím phải còn khớp notification đã nhận.

Không thể phân biệt gợi ý tự động với selection thủ công có cùng hình dạng.
Phím gõ mới được xử lý như thao tác thay phần đuôi đang chọn, có thể sửa phần
từ đang được bộ gõ sở hữu trước con trỏ. Không sửa do notification tự đến,
không cho selection ngược/ở giữa/từ context khác quyền sửa. Backspace khi
đang chọn phần đuôi đi native và giữ nguyên lịch sử phần chữ đã gõ.

Mô hình client giả đã thêm gợi ý sau mỗi phím native và tùy chọn sau mỗi IM
commit; không gửi snapshot thu gọn trung gian để giả ACK. Test Telex/VNI chạy
với hai cách frontend xóa: mở rộng selection như Chromium Textfield, hoặc
giữ selection sau khi xóa phần trước con trỏ để commit tiếp thay thế nó.
Ca câu đầy đủ đạt NFC `cộng hoà xã hội chủ nghĩa`, không preedit/popup hay
synthesize phím. Có Backspace, Ctrl+A, selection ngược/khác prefix/chưa có
notification, và kiểm bộ matcher không nhận context giữa tài liệu.

Test mới chạy với bản sao addon alpha.37 thất bại ở ca giữ lịch sử qua gợi ý;
bản mới đạt 8/8 CTest. Đây là mô hình hành vi, không phải Chrome thật. Cần
thử lại alpha.38 trên desktop; nguy cơ frontend không thực hiện edit và
giao thức snapshot không có revision ở phần giới hạn vẫn còn.

Khi diagnostics bật, `native:url-completion-edit`, `snapshot=url-completion`
và `native_url_completion=1` cho biết đã đi qua nhánh mới. Chỉ có enum/boolean,
không chứa nội dung hay độ dài phần gợi ý.

## Alpha.38 — phản hồi desktop vẫn lặp chữ

Người dùng báo `coôngj hoà xã hội chủ`. Chưa xác nhận đã gõ hết câu hay dừng
ở `chur`, nên chưa kết luận phần `nghĩa` bị mất. Máy đang chạy addon alpha.38.
Journal có `native:url-completion-edit` rồi `awaiting-surrounding-update`:
nhánh gợi ý đã chạy, nhưng edit chưa nhận được xác nhận đúng. Kết quả phù hợp
với thao tác xóa không có hiệu lực trước khi chèn `ô`; đây chưa phải bằng
chứng xác định riêng thành phần gây lỗi.

Mô hình client trong test chỉ mô phỏng cách Textfield mở rộng selection;
chưa mô phỏng đầy đủ việc đổi vùng xóa qua Mutter và Wayland text-input-v3.
Vì vậy 8/8 test không chứng minh thanh địa chỉ Chrome đã hoạt động. Chưa phát
hành alpha tiếp theo hoặc tăng vùng xóa dựa riêng trên giả thuyết này.

Đã đối chiếu đúng mã nguồn Chrome 157.0.8103.38:
[text-input-v3](https://chromium.googlesource.com/chromium/src/+/refs/tags/157.0.8103.38/ui/ozone/platform/wayland/host/zwp_text_input_v3.cc)
tính vùng xóa quanh selection/preedit đã lưu, sau đó
[WaylandInputMethodContext](https://chromium.googlesource.com/chromium/src/+/refs/tags/157.0.8103.38/ui/ozone/platform/wayland/host/wayland_input_method_context.cc)
kiểm vùng xóa phải bao trọn selection. Còn cần kiểm đường thực thi và thứ tự
notification trên desktop; đọc source chưa chứng minh nguyên nhân cụ thể.

### Thử đường GTK/Fcitx qua XWayland

[Tài liệu Fcitx cho GNOME](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland#GNOME)
khuyến nghị Chrome qua XWayland với `GTK_IM_MODULE=fcitx`. Máy đã có
`fcitx5-frontend-gtk3`, `fcitx5-frontend-gtk4` và XWayland. Có thể thử bằng
profile riêng để Chrome đang chạy không nhận lệnh rồi bỏ qua cờ backend:

```bash
env GTK_IM_MODULE=fcitx XMODIFIERS=@im=fcitx google-chrome \
  --ozone-platform=x11 --gtk-version=3 \
  --user-data-dir="$HOME/.local/share/openkey-chrome-x11-test" \
  --no-first-run --no-default-browser-check about:blank
```

Chạy từ terminal trong desktop, giữ chế độ NativeDirect, bật OpenKey rồi thử
`coongj hoaf xax hooij chur nghiax` trên thanh địa chỉ; kỳ vọng
`cộng hoà xã hội chủ nghĩa`. Profile thử không có lịch sử để tạo gợi ý giống
profile chính, nên cả kết quả đạt cũng cần phân biệt với ca autocomplete.
Lệnh này không thay launcher hay cấu hình Chrome chính. **Thử thực tế đã
FAIL với NativeDirect**, xem kết quả dưới đây; không dùng như cách khắc phục
đã xác nhận cho chế độ này.

### Kết quả thử GTK/XWayland — 2026-11-05

Người dùng nhận `coongj hoaf xax  hooij chur nghiax`. Journal lúc 23:21 có
`app=google-chrome frontend=dbus`, `cause=no-surrounding-capability` và
`snapshot=invalid`. OpenKey đã nhận phím, nhưng Chrome không cung cấp khả năng
surrounding-text cần thiết; NativeDirect cố ý cho phím nguyên dạng đi qua.
Đây là lỗi tương thích khác với lần sửa chữ bị lặp trên bridge GNOME.

Đối chiếu [InputMethodContextImplGtk của đúng Chrome 157.0.8103.38](https://chromium.googlesource.com/chromium/src/+/refs/tags/157.0.8103.38/ui/gtk/input_method_context_impl_gtk.cc):
`SetSurroundingText` có thân rỗng; signal retrieve/delete-surrounding chưa
được xử lý. Đường code này dùng chung GTK3/GTK4, nên đổi riêng phiên bản GTK
không bổ sung được khả năng còn thiếu. Khuyến nghị chung của Fcitx về XWayland
không bảo đảm hỗ trợ NativeDirect của OpenKey.

Không bỏ guard capability, giả ACK hoặc chuyển sang gửi Backspace tự động để
che lỗi. Hiện chưa có đường Chrome đã xác nhận đáp ứng đồng thời gõ đủ dấu,
không preedit và không ô nổi. Nếu cần nhập tiếng Việt ngay, có thể tạm chọn
Preedit trong cấu hình OpenKey; chế độ đó có thể hiện gạch chân/ô composition
và chưa đáp ứng yêu cầu giao diện ban đầu. Không cần cài lại alpha.38 hay đổi
các settings VS Code từ tệp đính kèm để xử lý thiếu sót của Chrome GTK.

## Thu kết quả desktop sau alpha.50

Có [bộ ca Chrome/VS Code và công cụ tổng hợp diagnostics](UBUNTU_DESKTOP_QA.md)
để tách lỗi thiếu capability, chờ surrounding-text và hạn chế định danh bridge.
Công cụ không xuất nội dung gõ và không thay guard của NativeDirect. Chưa có
kết quả desktop mới chứng minh Chrome omnibox hoặc terminal VS Code đã đạt.

## Beta.2~alpha.2 — `goo` → `goô` trên thanh địa chỉ Chrome (2026-11-09)

Người dùng gõ Telex `goo` trên thanh địa chỉ Chrome 157 (Wayland gốc, GNOME,
frontend ibus) và nhận `goô`. Journal lúc đó không còn (daemon chạy với stdout
vào pty đã đóng), nên nguyên nhân được xác định từ mã nguồn đúng tag
157.0.8103.38 và tái hiện bằng client giả:

1. Omnibox đặt gợi ý đuôi bằng
   `SetTextAndSelectedRange(display_text, {display_text.size(), user_text.length()})`:
   anchor ở cuối, con trỏ sau phần đã gõ (selection **ngược**, đúng hình dạng
   `selectedUrlCompletion` nhận).
2. Ở phím `o` thứ ba, OpenKey gọi `deleteSurroundingText(-1, 1)` rồi
   `commitString("ô")`. Mutter đổi thành `delete_surrounding_text(1, 0)` tính
   theo con trỏ.
3. `zwp_text_input_v3.cc` tính `index = delete_around_range.start() - before`,
   với `delete_around_range` là selection (start = anchor = 10), nên được
   vùng `[9, 10)`. `WaylandInputMethodContext::OnDeleteSurroundingText` thấy
   vùng không bao trọn selection `[2, 10)` → `LOG(DFATAL)` và bỏ lệnh xóa.
4. Commit `ô` thay vùng chọn → `go` + `ô` = `goô`. Nếu `before` ≥ độ dài phần
   gợi ý, Chrome xóa sai vị trí, nên không có độ dài xóa nào an toàn.

Mô hình client cũ (`selectionAwareDeletes`) giả định Chrome xóa cả vùng chọn
lẫn phần trước con trỏ, nên test alpha.38 đạt dù Chrome thật lỗi.

Sửa (addon 0.2.46): khi gợi ý đuôi đang được chọn và phím cần xóa chữ đã có,
OpenKey không gửi lệnh xóa. Phím đi nguyên dạng (Chrome thay vùng chọn như gõ
thường), bộ gõ ghi nhận chữ raw thực tế trong ô và giữ lịch sử engine. Khi một
phím/ranh giới sau đó thấy con trỏ thu gọn khớp đúng snapshot dự kiến, phần raw
được sửa bằng edit kiểm chứng thông thường (`goos ` → `gố `). Nếu gợi ý vẫn còn
đến ranh giới, từ giữ raw (`goo ` vẫn là `goo ` khi lịch sử có `google.com`).
Backspace trên chữ raw đang giữ trong ô Url đi native, xóa đúng một ký tự nhìn
thấy. Diagnostics: `reason=native:url-completion-raw cause=selection-blocks-delete`.

Đánh đổi: thà để từ raw còn hơn làm hỏng chữ. Trong khi Chrome còn tự điền đuôi
(tiền tố trùng lịch sử), dấu không được đặt; từ được sửa khi gợi ý biến mất.
Không thêm fallback CommitOnly/preedit, không gửi Backspace giả.

Test mới mô phỏng đúng chuỗi Mutter → text-input-v3 → WaylandInputMethodContext
→ Textfield (`chromiumWaylandDeletes`, `completionTarget`). Với addon cũ test
thất bại: `Omnibox completion corrupted the typed word: [goô]`.

Kiểm tra trên desktop sau khi cài:
- Thanh địa chỉ, khi đã có `google.com` trong lịch sử: `goo` hiện `goo` +
  gợi ý (không còn `goô`); `goos ` → `gố `; `tieengs vieetj` → `tiếng việt`.
- Ô thường (Chrome trang web, Cursor/VS Code editor, gedit, LibreOffice, Qt):
  `tieengs vieetj ddaay laf mer ` → `tiếng việt đây là mẻ `.

## Beta.2~alpha.3 — hiện `gô` ngay khi gợi ý đang được chọn (2026-11-09)

Alpha.2 đúng nhưng chậm: `goo` hiện `goo`, sửa khi gõ Space. Yêu cầu: `goo` →
`gô`, `gooo` → `goo` ngay lúc gõ. Ba cách đã xét (Chromium 157, Mutter):

- (a) Commit/preedit rỗng: `OnDone` bỏ commit rỗng; preedit gây gạch chân và
  Chrome bỏ lệnh xóa khi đang có composition (`OnDeleteSurroundingText`). Loại.
- (c) Lệnh xóa tính từ anchor: Mutter đổi offset quanh **con trỏ** sang byte và
  giới hạn `before` ở vị trí con trỏ (`meta-wayland-text-input.c`); Chrome tính
  `index = selection.start() - before` từ anchor (`zwp_text_input_v3.cc`) và bỏ
  mọi range không phủ hết vùng chọn (`wayland_input_method_context.cc`). Không
  giá trị nào xóa được cả chữ đã gõ lẫn phần gợi ý. Loại.
- (b) Forward BackSpace: chọn. BackSpace trên vùng chọn đảo của omnibox xóa đúng
  gợi ý, `just_deleted_text` chặn autocomplete hiện lại
  (`omnibox_view.cc`, `omnibox_edit_model.cc`).

Cơ chế (chỉ ô Url, gợi ý đuôi đã xác nhận, phím chữ ASCII): nuốt phím, forward
BackSpace có hardware keycode 22 (keycode 0 bị Chrome bỏ qua), chờ notification
bằng đúng phần đã gõ với con trỏ không có vùng chọn rồi mới xóa/commit như ô
thường. Phím kế hoặc 150 ms không xác nhận → commit phím raw (`clear-unconfirmed`,
từ được sửa ở phím sau như alpha.2). Reset/đổi focus → bỏ phím, không commit
sang ô khác; đổi cấu hình → commit raw. Phím ranh giới vẫn theo alpha.2.
Diagnostics: `native:url-completion-clear`, `verified-clear`, `clear-unconfirmed`.

Rủi ro: forward key từng bị nuốt trên stack này (alpha.13) → chậm tối đa 150 ms
rồi ra raw; thoáng thấy `go` giữa BackSpace và sửa; chưa nghiệm thu desktop.

Kiểm tra: lịch sử có `google.com`, gõ `goo` → `gô` ngay, `gooo` → `goo`,
`goos` → `gố`, VNI `go6` → `gô`, `tieengs vieetj` → `tiếng việt`; Backspace sau
`gô` còn `g`. Log nên có `verified-clear`, không có `clear-unconfirmed`.

## Beta.2~alpha.4 — chẩn đoán bước xóa gợi ý (2026-11-09)

Log alpha.3 thật (14:04): mỗi từ có `url-completion-clear`, rồi 50–250 ms sau
`clear-unconfirmed`, rồi `awaiting-surrounding-update`/`raw-word-in-progress`.
Log không đủ phân biệt: dòng `clear-unconfirmed` so với kỳ vọng *sau* commit raw
nên mọi `*_changed=1` khớp cả “BackSpace đã tới (go@2)” lẫn “không tới
(go|ogle.com)”. Hai lỗi phụ đã thấy: timer tạo với accuracy 0 nên sd-event cho
trễ thêm tới 250 ms (sửa: 1 ms); một lần fallback và `passthrough` cách nhau
38 µs, tức do một phím (có thể chính BackSpace forward quay lại) kích hoạt.

Alpha.4 giữ nguyên cơ chế, thêm tối đa 12 dòng `OpenKey Direct trace:` cho mỗi
lần xóa (armed/forwarded/update/key-press/key-release/timer/settle-raw/apply):
`elapsed_us`, `len`, `cursor`, `anchor`, `base_len`, `base_cursor`, các cờ
`equals_base`, `starts_with_base`, `prefix_of_base`, `equals_expected`,
`emitting`, `pending`; với phím chỉ loại (`backspace`/`printable`/...) và
keycode của phím không phải chữ. Không có nội dung văn bản hay ký tự gõ.

## Beta.2~alpha.5 — hai commit thường thay BackSpace forward (2026-11-09)

Trace alpha.4 thật (14:19–14:20, mỗi lần đổi dấu trên `google.com`):
`armed` → `forwarded` (~30 µs) → không có `update` nào trong 150 ms, vùng chọn
giữ nguyên `len=10 cursor=2 anchor=10` → `timer` (151 ms) → commit raw → `update`
sau ~6 ms `cursor=3 anchor=10` (Chrome autocomplete lại). BackSpace forward không
quay lại fcitx5, không đến muộn, không xóa chữ: Chrome không bao giờ nhận nó
(GNOME Shell 50.1, `clutter_input_method_forward_key` tạo sự kiện không thiết bị).
Ngoài ra alpha.4 ghi keycode chữ khi NumLock bật (`key=other code=32 states=16`);
alpha.5 chỉ ghi loại phím.

Cơ chế alpha.5 (ô Url, gợi ý đuôi đã xác nhận, phím chữ ASCII cần đổi chữ đã gõ):
1. Commit phần đuôi mới; commit luôn thay vùng chọn: `go|ogle.com` → `goô`.
2. Khi notification đúng bằng text đó, con trỏ không chọn, ở cuối: xóa
   `removed + |đuôi|` trước con trỏ và commit đuôi → `gô` (lệnh xóa ở caret
   thường Chrome chấp nhận). Trace thật cho thấy ACK sau ~6 ms.
Chrome autocomplete lại đuôi trung gian (cần lịch sử bắt đầu bằng chuỗi như
`goô`, `viêtệt`): không xóa, ngừng cách hai bước tới khi đổi focus, từ giữ như
alpha.2. Phím đến trước ACK: không xóa gì, từ có thể giữ đuôi trung gian.
Reset/đổi focus: bỏ, không sửa ô khác. Không forward phím, không preedit.

Kiểm tra: `goo` → `gô`, `gooo` → `goo`, `goos` → `gố`, VNI `go6` → `gô`,
`tieengs vieetj` → `tiếng việt`. Log nên có `verified-clear`; nếu thấy
`clear-unconfirmed` hoặc `clear-reautocompleted` thì gửi trace.

## Beta.2~alpha.6 — từ sau khi xóa ô địa chỉ (2026-11-09)

Kiểm thử thật bằng ydotool (uinput → GNOME → Fcitx5 → Chrome 157), alpha.5:
`goo` → `gô` đúng ngay (commit đuôi, ACK 7,4 ms, sửa ở con trỏ). Nhưng mọi từ
sau Ctrl+L, Ctrl+A, Delete (hoặc Backspace) giữ raw (`gooo`, `goos`, `tieengs`):
`awaiting-surrounding-update` rồi `raw-word-in-progress`. Bắt bus ibus bằng
dbus-monitor: Chrome không gửi `SetSurroundingText` sau Ctrl+L/Ctrl+A/Delete, chỉ
`SetCursorLocation`; lần kế tiếp là sau chữ đầu của từ mới. Chữ đầu dự đoán từ
cache cũ (từ trước) nên không bao giờ khớp.

Alpha.6: phím không phải chữ/Space đi native đánh dấu cache cũ (`nativeStale`,
notification bất kỳ xóa cờ). Chữ đầu trên cache cũ bắt đầu từ mới; notification
kế tiếp là mốc nếu ký tự ngay trước con trỏ đúng chữ đó, con trỏ không chọn hoặc
có gợi ý URL chọn tới cuối (`native:stale-cache`, `adopted-update`). Mọi lệnh sửa
sau đó vẫn kiểm tra đúng các chữ đã đặt trước con trỏ.


## Beta.2~alpha.7 — DEL trong terminal (thử nghiệm, mặc định tắt)

Addon 0.2.51 thêm `TerminalDeleteFix=False`. Bật bằng `fcitx5-configtool` →
Add-ons → OpenKey → **Sửa dấu trong terminal VS Code/Cursor bằng DEL (thử nghiệm)**,
hoặc đặt các dòng sau trong `~/.config/fcitx5/conf/openkey.conf` rồi reload cấu hình:

```ini
InputStyle=NativeDirect
TerminalDeleteFix=True
```

Chỉ bật khi thực sự cần và tắt lại (`False`) trước khi dùng vim/raw apps, SSH
hay terminal không rõ cấu hình. Không dùng CommitOnly và không có preedit gạch chân.
Tùy chọn chỉ nhận program `code`, `code-insiders`, `cursor` (không phân biệt hoa
thường; chấp nhận đường dẫn và hậu tố `.desktop`), cùng surrounding text hợp lệ,
không có selection, chỉ chứa khoảng trắng hoặc rỗng. URL/email và ứng dụng khác
không dùng DEL. Khi editor báo chữ thật, addon quay lại NativeDirect bình thường;
editor rỗng cũng có thể khớp chữ ký nên chữ ký này không chứng minh terminal focus.

Mỗi lần sửa, addon giữ tiền tố chung, commit N ký tự U+007F (DEL) rồi đuôi mới.
N không vượt số codepoint addon đã tự commit trong từ hiện tại; không xóa tiền tố
native, không mở lại từ trước dấu cách. Focus/reset, Enter/Tab, điều hướng,
Ctrl/Alt và Backspace vượt vùng đã ghi đều bỏ quyền sửa. Không log nội dung gõ.

**Giới hạn quan trọng:** trên GNOME Wayland, ibus thường chỉ báo program
`gnome-shell`, không phải Code/Cursor. Khi đó tùy chọn cố ý **không hoạt động**,
kể cả khi surrounding toàn dấu cách; không dùng tên app đoán hoặc cache GNOME
cũ để gửi DEL. Alpha.7 chưa giải quyết nhận diện app cho trường hợp này.
Cần frontend báo đúng program; không tự thay thiết lập ứng dụng/GNOME hay config
người dùng để ép nhận diện.

DEL chỉ xóa một codepoint khi line discipline đang canonical, UTF-8 (`IUTF8`)
và `VERASE=^?`. `IUTF8` tắt, mã hóa khác UTF-8, SSH tới host cấu hình lạ, vim/raw
apps, bracketed-paste hoặc app xử lý control char riêng có thể để lộ DEL, xóa sai
byte hoặc làm hỏng chữ. Addon không thể xác minh tty/host ở xa và cũng không thể
biết một commit đã thật sự đến PTY. Đây là tùy chọn thử nghiệm, không bảo đảm an
toàn cho mọi terminal; không bật khi gõ lệnh quan trọng.

Test giả lập canonical UTF-8 kiểm tra `ddaay laf tieengs vieetj` → `đây là tiếng việt`,
`goo` → `gô`, `gooo` → `goo`, giới hạn DEL, reset/focus/điều hướng và editor `mer` → `mẻ`.
Không đồng nghĩa đã xác minh VS Code/Cursor thật. Script thủ công chuẩn bị tại
`/tmp/openkey-e2e/term-alpha7.sh`; chỉ dùng profile tạm, accessibilitySupport=off,
terminal `cat`, không đổi config thật và không tự bật tùy chọn.
