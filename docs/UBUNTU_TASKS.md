# Task phát triển OpenKey cho Ubuntu

Nhánh triển khai hiện tại: `fix/ubuntu-direct-focus`

Mốc nền tảng v0.1: commit `b22c3eb`. Hướng dẫn build/cài thử và giới hạn của
prototype: [Linux README](../Sources/OpenKey/linux/README.md).

## v0.1 — Engine và build nền tảng

- [x] Tạo build entry point bằng CMake.
- [x] Biên dịch engine với macro nền tảng `LINUX`.
- [x] Tạo smoke test cho Unicode và VNI.
- [x] Bổ sung test hồi quy cho Telex, xử lý Backspace và macro.
- [x] Chuẩn hóa cảnh báo compiler trên GCC và Clang qua CMake/CI.
- [x] Tạo workflow build Linux trên CI.
- [ ] Thay `std::wstring_convert` đã deprecated bằng bộ chuyển UTF-8 có test Unicode riêng.
- [ ] Tách các API platform khỏi engine nếu phát hiện phụ thuộc ngoài Linux.

## v0.2 — Input method

- [x] Chọn frontend chính: Fcitx5.
- [x] Tạo module nhận key event từ Fcitx5 (Telex và VNI).
- [x] Cập nhật Unicode/preedit và Backspace trong từ đang gõ, commit qua Fcitx5.
- [x] Test headless nạp module thật, preedit/commit, Escape/reset/deactivate,
  shortcut, ô nhạy cảm và isolation giữa context.
- [x] Bổ sung build/install target và CI build addon.
- [x] Kiểm thử discovery descriptor, nạp addon từ install prefix và routing qua
  Fcitx event pipeline thật (headless, không dùng desktop).
- [x] Sửa lỗi commit lặp từ khi mất focus; test focus-out cho client preedit,
  ClientUnfocusCommit và server-side preedit, chuyển Telex/VNI và quay lại ô cũ.
- [ ] Kiểm thử cài vào desktop, phát hiện input method và focus-out thực tế.
- [ ] Đo độ trễ replay, thiết kế engine context riêng trước beta.
- [ ] Kết nối macro/cấu hình lưu bền và các kiểu gõ còn lại vào addon.
- [ ] Kiểm thử Firefox/Chrome, LibreOffice, VS Code, GTK và Qt.
- [ ] Kiểm thử X11 và Wayland.
- [ ] Đánh giá IBus fallback.

Checklist nghiệm thu thủ công: [Ubuntu input QA](UBUNTU_INPUT_QA.md).

### Phản hồi desktop 2026-11-03 — alpha.3 Direct

- Người dùng xác nhận text editor gõ ổn định; chưa xác định tên/toolkit của editor.
- VS Code không hoạt động; cần phân biệt không đặt dấu, fallback preedit hay lỗi
  completion, kèm desktop/session/backend và capability thực tế.
- Firefox/Chrome ở thanh địa chỉ: phải chuyển EN–VI sau nhận focus mới hoạt động.
- [x] Tái hiện và sửa trường hợp selection có sẵn lúc bắt đầu từ: commit phím mới
  thay selection theo semantics của ứng dụng, không xóa selection bằng surrounding API.
- [x] Tái hiện và sửa context được tái sử dụng/đổi khi không còn edit chờ:
  kết thúc sở hữu từ cũ, tự bắt đầu từ mới; không buộc toggle EN–VI.
- [x] Test selection đảo chiều + ACK chậm, context đổi không notification riêng,
  capability bị rút và không mất phím đầu khi fallback.
- [x] Thêm `DirectDiagnostics` tùy chọn, mặc định tắt: ghi reason/app/frontend/
  capability, không ghi nội dung gõ, selection, offset hay recovery text.
- [ ] Kiểm thử lại alpha.4 trên Firefox/Chrome desktop, không toggle EN–VI.
- [ ] Xác định nguyên nhân VS Code bằng biểu hiện cụ thể và diagnostics; không
  coi các test focus headless là bằng chứng đã sửa xong VS Code.

### Điều tra VS Code — alpha.5 diagnostics

- [x] Nhận biểu hiện cụ thể từ người dùng: Direct giữ nguyên `coongj`/`hoaf`,
  gợi ý code vẫn hiện; Preedit chuyển dấu được nhưng không hiện gợi ý.
  Máy ảo VMware dùng GNOME/Wayland, tiến trình VS Code dùng backend Wayland.
- [x] Đọc log bridge `app=gnome-shell frontend=ibus`, có
  `bypass:unconfirmed-edit`; chưa đủ để quy nguyên nhân cho timeout hoặc gán
  từng dòng log cho riêng VS Code.
- [x] Bổ sung `edit-aborted` trước khi dọn trạng thái, cause riêng cho timeout,
  snapshot sai/không hợp lệ, mất capability, tràn queue và từ chối chuẩn bị edit.
  Chỉ ghi enum/boolean, không ghi nội dung gõ, độ dài hoặc offset.
- [x] Thêm test phân loại snapshot không thay đổi ACK; kiểm tra log failure,
  bật/tắt và không lộ nội dung tài liệu/phím gõ. Diagnostics vẫn mặc định tắt.
  Trường `notified` phân biệt thiếu notification với notification chứa cache cũ.
- [x] Alpha.5: 7/7 CTest Release và ASan/UBSan; xác minh payload/dependency,
  nạp addon và chạy Direct tests từ gói đã giải nén. Chưa tự cài vào desktop.
- [x] Người dùng cài/thử alpha.5; log 19:59:33–19:59:35 lặp
  `direct:ready → context-reset`, không có `edit-aborted`/timeout trong khoảng này.
  Log qua bridge GNOME/IBus, chưa chứng minh snapshot thay đổi do thành phần nào.
- [ ] Dựa trên log mới chọn sửa tương thích hoặc xác định giới hạn backend;
  chưa bỏ guard an toàn, chưa tăng timeout và chưa công bố VS Code được hỗ trợ.

### Alpha.6 — Giữ lịch sử qua echo muộn đã biết

- [x] Tái hiện headless snapshot cũ đến sau ACK rồi cache trở lại snapshot hiện
  tại trước phím tiếp theo; alpha.5 làm mất lịch sử Telex/VNI ở trường hợp này.
- [x] Giữ lịch sử tạm thời với echo cũ đã biết; bắt buộc đọc lại snapshot hiện
  tại trong key path trước khi tiếp tục sửa. Nếu cache vẫn cũ thì bỏ lịch sử.
  Không chấp nhận snapshot chỉ vì có cùng hậu tố/từ trước con trỏ.
- [x] Thêm test `coongj hoaf` → `cộng hoà`, VNI `vie6t5` → `việt` qua chuỗi
  ACK → echo cũ → hiện tại, không preedit. Giữ test cache cũ tại phím tiếp theo,
  cursor/selection đổi, dữ liệu Unicode, ACK trễ và bảo vệ vùng chữ.
- [x] Log `context-wait` và `context-invalidated` trước khi dọn tracker,
  không ghi nội dung/offset; phân biệt echo muộn với context khác chưa biết.
- [x] 7/7 CTest Release và ASan/UBSan; Direct chạy lặp 10 lần đạt.
  Gói alpha.6 có checksum, xác minh dependency/payload và test addon giải nén.
- [x] Người dùng thử alpha.6; log 20:05:40–20:05:48 lặp
  `context-invalidated cause=surrounding-mismatch snapshot=changed pending=0 notified=1`.
  Không khớp echo cũ đã lưu; sửa echo muộn chưa giải quyết reset quan sát được.
  Chưa công bố đã sửa tương thích VS Code.

### Alpha.7 — Phân biệt thành phần snapshot thay đổi

- [x] So sánh read-only với `confirmed` hoặc `expected`: cờ text/cursor/anchor
  khác nhau, không ghi nội dung, hash, độ dài, vị trí hay phím gõ.
- [x] Thêm `reference` và `comparable` để không nhầm snapshot thiếu/không hợp lệ
  với các trường không thay đổi; giữ log trước khi dọn tracker.
- [x] Test từng thành phần độc lập qua logger thật và helper; test thiếu snapshot,
  thiếu mốc, expected chưa ACK, bật/tắt và không lộ nội dung.
- [x] 7/7 CTest Release và ASan/UBSan; gói alpha.7 có checksum và đã qua kiểm tra
  dependency/payload, nạp addon và chạy Direct tests từ gói giải nén.
- [x] Thử alpha.7 trong VS Code: snapshot đổi cả nội dung và offset, có ca chỉ
  đổi nội dung. Các cờ chưa chứng minh nguyên nhân ở app, bridge hay compositor.
  Không thay thuật toán Direct hoặc timeout trong bản này.

### Điều tra đường nhập VS Code — không tạo gói alpha mới

- [x] Alpha.7 desktop: có snapshot đổi cả nội dung/cursor/anchor và ca chỉ đổi
  nội dung; vẫn reset. Chưa xác định thay đổi từ app, bridge hay compositor.
- [x] Đối chứng XWayland: gõ có gạch chân là fallback Preedit, log
  `app=gtk3-im:code frontend=ibus fallback:no-surrounding-capability`.
- [x] Đọc frontend Fcitx5 5.1.19 và mã nguồn đúng commit VS Code 1.144.1:
  có đường EditContext/textarea; textarea phụ thuộc accessibility support.
- [x] Tạo launcher profile tạm theo từng mode/backend, tắt extension và giữ
  framework IM nguyên trạng; không đổi profile chính hay restart Fcitx5.
- [x] Kiểm tra Bash syntax, JSON, từ chối tham số/display không hợp lệ, smoke
  sáu mode/backend bằng CLI stub (không GUI); 7/7 CTest hiện tại vẫn đạt.
- [ ] Chạy desktop lần lượt `textarea wayland`, `accessible wayland`,
  `editcontext wayland`; chỉ đổi một yếu tố so với mốc textarea.
- [ ] Dựa vào kết quả chọn đường tương thích hoặc xác nhận giới hạn API.
  Không bỏ guard, không dùng clipboard/global hook để lách thiếu capability.
- [x] Đưa hồi quy trình chạy profile vào CTest Linux: 24 ca dùng CLI stub, sáu
  mode/backend, profile riêng/không dùng lại, copy fixture và quyền, flag/exit
  code, đường dẫn có khoảng trắng, help/guard không launch. TMPDIR tuyệt đối
  được kiểm tra, umask 077; không mở editor thật hoặc thay profile chính.
- [x] Sau tích hợp launcher: UI 13/13 và addon/service 17/17 CTest đạt.
  Không tạo gói addon mới, không restart Fcitx5/tray hoặc đổi đường nhập editor.
- [ ] Nghiệm thu gõ profile editcontext trên desktop; test launcher không thay
  bằng chứng gõ/khả năng surrounding-text hoặc kết quả completion thực tế.

Chi tiết, nguồn đối chiếu và cách thử:
[VS Code input investigation](UBUNTU_VSCODE_INPUT_INVESTIGATION.md).

### Alpha.8 — Backspace và đặt dấu sau sửa từ

- [x] Nhận kết quả profile đối chứng: `textarea wayland` không chuyển dấu;
  `accessible wayland` gõ `cộng hoà`, không gạch chân; cả hai hiện gợi ý.
  Người dùng xác nhận profile accessible vẫn lỗi khi Backspace rồi gõ tiếp.
- [x] Tái hiện bằng test đỏ alpha.7: `coongg` → Backspace → `j` không ra `cộng`;
  log desktop có `prepare-rejected` dù snapshot hiện tại khớp.
- [x] Thay từ chối blanket đối với snapshot lặp bằng chờ notification sau edit;
  cache khớp đơn thuần chưa ACK. Giữ validation và tuần tự hóa, không tăng timeout.
- [x] Test Telex/VNI sửa dư phụ âm, xóa/gõ lại, đổi dấu quay lại trạng thái cũ,
  ACK chậm, cache không notification, selection thay đổi trong khi sửa.
- [x] 7/7 CTest Release và ASan/UBSan; Direct lặp 10 lần đạt. Alpha.8 có checksum,
  kiểm tra payload/dependency và test addon từ gói giải nén đạt.
- [x] Người dùng xác nhận alpha.8 sửa từ đúng mong đợi trên profile accessible;
  sau đó xác nhận Tab/Enter, undo/redo, đổi focus và gõ nhanh đúng mong đợi.
  Đây là phản hồi môi trường này, chưa xác minh mọi race notification revisionless.

Giới hạn: notification trễ trùng hoàn toàn snapshot mong đợi vẫn không phân biệt
tuyệt đối được với ACK mới. Không đánh dấu hoàn thành Direct hoặc bỏ QA an toàn.

### Alpha.9 — Xóa Space rồi chỉnh sửa tiếp từ

- [x] Nhận yêu cầu: gõ chưa xong, vô tình Space, xóa khoảng trắng rồi đặt dấu tiếp.
- [x] Tái hiện test đỏ alpha.8: `coong` → Space → Backspace → `j` không ra `cộng`.
- [x] Giữ lịch sử một Composition trước boundary cùng tối đa 32 Space ASCII;
  xóa hết Space thì phục hồi lịch sử Telex/VNI của từ, không scan từ trong tài liệu.
- [x] Kiểm tra snapshot và toàn bộ hậu tố từ + Space trước khi sửa; vẫn serialize
  theo ACK, dùng cơ chế snapshot lặp alpha.8; không sửa theo suffix đơn thuần.
- [x] Bỏ lịch sử khi gõ từ mới, dấu câu, navigation/shortcut, context/selection/caret
  khác, mất focus/đổi chế độ. Preedit dự phòng không đổi.
- [x] Test nhiều Space, Telex/VNI, Unicode prefix, ACK chậm/burst, lặp boundary,
  giới hạn 32, đổi caret/selection/focus và selection đổi khi đang chờ ACK.
- [x] 7/7 CTest Release và ASan/UBSan; Direct lặp 10 lần đạt; alpha.9 có checksum,
  xác minh dependency/payload và chạy test addon từ gói đã giải nén.
- [x] Người dùng xác nhận alpha.9 xóa Space rồi sửa tiếp từ đúng mong đợi trên
  profile accessible. Không mở rộng kết quả sang ứng dụng/backend chưa kiểm thử.

## v0.2.1 — Gõ trực tiếp không preedit/gạch chân (ưu tiên)

Phản hồi kiểm thử: người dùng xác nhận bản alpha gõ tiếng Việt được, nhưng
preedit có gạch chân và gợi ý code trong VS Code không xuất hiện khi đang gõ.
Đây là phản hồi trên môi trường người dùng, chưa thay thế toàn bộ ma trận QA.

Mục tiêu: ký tự được đưa trực tiếp vào tài liệu, không giữ cả từ trong composition;
VS Code có thể cập nhật gợi ý trong lúc gõ. Chỉ ẩn nét gạch chân không đạt mục tiêu.
Giữ chế độ preedit hiện tại để tương thích với các ứng dụng không hỗ trợ gõ trực tiếp.

### Khảo sát và thiết kế

- [ ] Xác minh VS Code/Electron cung cấp surrounding text, vị trí con trỏ và
  API xóa/thay thế văn bản qua Fcitx5 trên native Wayland và X11/XWayland.
- [x] Thiết kế chế độ `Preedit` và `Direct` có thể chọn; không mặc định bật Direct
  cho mọi ứng dụng trước khi kiểm chứng. Xác định cách lưu cấu hình và chuyển chế độ
  khi đang gõ mà không commit lặp hoặc làm mất ký tự.
- [x] Xác định điều kiện hỗ trợ Direct và chính sách fallback khi client thiếu
  capability, surrounding text không hợp lệ, có selection hoặc cursor đã di chuyển.

### Triển khai

- [x] Commit ký tự ngay ở chế độ Direct; không gửi client/server preedit cho từ
  đang gõ. Tính phần văn bản thực sự cần thay đổi khi thêm/bỏ dấu Telex/VNI.
- [x] Thay thế đúng đoạn vừa gõ bằng API surrounding-text của Fcitx5 khi được hỗ trợ;
  kiểm tra Unicode code point, vị trí cursor/anchor và ranh giới từ trước khi sửa.
- [x] Quản lý context riêng cho từng ô nhập, trạng thái văn bản dự kiến và các edit
  đang chờ xác nhận; xử lý surrounding-text cập nhật chậm hoặc đến không đúng thứ tự
  khi gõ nhanh. Không coi một snapshot cũ là trạng thái hiện tại của ứng dụng.
- [ ] Reset ngữ cảnh đúng khi click/di chuyển cursor, chọn vùng, nhận gợi ý code,
  dán, undo/redo, đổi focus hoặc đổi kiểu gõ; không sửa nhầm văn bản ngoài vùng
  engine còn quản lý. Không commit lại từ Direct khi mất focus.
- [x] Định nghĩa hành vi Backspace, Escape, Space/Enter và shortcut trong Direct;
  Escape không được xóa lại từ đã commit như khi hủy preedit.
- [x] Giữ nguyên bypass ô Password/Sensitive. Fallback có kiểm soát về preedit
  hoặc chuyển phím nguyên trạng khi không thể sửa an toàn; không giả định mọi
  ứng dụng đều hỗ trợ Direct. Không dùng clipboard hoặc hook phím toàn hệ thống
  làm giải pháp mặc định.

### Kiểm thử và nghiệm thu

- [x] Thêm test engine/adapter và Fcitx pipeline cho Telex/VNI Direct, thay dấu,
  chữ hoa, Backspace và từ dài; bảo đảm chế độ Preedit cũ không hồi quy.
- [x] Test headless ACK chậm/cũ, burst nhiều từ, timeout/overflow, cursor/selection,
  đổi chế độ, không làm mất panel của IM khác và không ghi đè selection từ phím chờ.
  Chữ chờ ở context đã thay đổi được giữ trong bảng phụ, Enter để chèn, Escape hủy.
- [ ] Thêm test context cập nhật chậm, gõ nhanh liên tiếp, selection/cursor thay đổi,
  chấp nhận completion, focus-out và fallback; không mất/lặp ký tự hoặc sửa sai vùng.
- [ ] Kiểm thử VS Code thực tế: gõ identifier ASCII như `console`/`print` và xác
  nhận popup IntelliSense xuất hiện/cập nhật trước Space; nhận gợi ý bằng Tab/Enter
  không làm hỏng nội dung. Kiểm tra inline completion riêng nếu đang dùng.
- [ ] Kiểm thử tiếng Việt trong comment/string, đổi qua lại English/Telex/VNI;
  phân biệt thay đổi do quy tắc Telex/VNI với lỗi tương tác completion.
- [ ] Kiểm thử native Wayland và X11/XWayland, GTK/Qt, trình duyệt và LibreOffice;
  ghi rõ ứng dụng nào dùng Direct được và ứng dụng nào cần fallback.
- [ ] Đo độ trễ và hành vi undo/redo thực tế; ghi giới hạn trong
  [Ubuntu input QA](UBUNTU_INPUT_QA.md), [Linux README](../Sources/OpenKey/linux/README.md)
  và hướng dẫn lựa chọn chế độ trước khi phát hành `.deb` mới.

Điều kiện hoàn thành: gõ trực tiếp không composition/gạch chân trên các ứng dụng
được công bố hỗ trợ, gợi ý code VS Code hoạt động khi đang gõ, không mất/lặp ký tự
và có fallback đã kiểm chứng. Đã triển khai alpha.3 và kiểm thử headless; chưa
nghiệm thu IntelliSense/undo/redo và Direct trên VS Code desktop thực tế.

## v0.3 — Tray và hotkey

- [x] Tạo nền tảng tiến trình session Linux `openkey-service`: một instance theo
  XDG_RUNTIME_DIR, IPC riêng cùng UID, status/quit và dừng bằng signal.
  Không điều khiển engine, auto-start hoặc thay Fcitx5; tray opt-in được thêm ở
  mục exporter bên dưới, chưa có cửa sổ UI điều khiển.
- [x] 8/8 CTest Release với service bật; lifecycle service lặp 10 lần và test
  ASan/UBSan đạt. Kiểm tra crash recovery, runtime permission và từ chối socket
  path là regular file/symlink. Các test dùng runtime tạm, không daemon desktop.
- [x] Đọc trạng thái/input method Fcitx5 qua D-Bus bằng `--input-status` trước khi
  hiện V/E: chỉ đọc, không auto-activate daemon, pin unique owner, kiểm tra state
  trước/sau; phân biệt Vietnamese/English/inactive/other/unknown/unavailable.
- [x] Test reader trên private D-Bus: mapping trạng thái, lỗi hai method, timeout,
  state thay đổi và owner lifecycle; 9/9 Release, test service/reader ASan/UBSan.
  Chưa nghiệm thu theo focus thực tế hoặc hiển thị tray.
- [x] Monitor polling nền, debounce hai mẫu, cache hết hạn và IPC không chờ bus;
  lỗi/mất bus vô hiệu hóa V/E ngay, có test transitions và daemon replacement.
- [x] 10/10 CTest Release; service/reader/monitor đạt ASan/UBSan. Reader và monitor
  chạy lặp 10 lần đạt, gồm reader chậm/treo, expiry, recovery và shutdown.
- [ ] Nghiệm thu monitor khi focus/IM đổi thực tế trên desktop, đo độ trễ/tài nguyên;
  không dùng context gần nhất như trạng thái riêng từng cửa sổ.
- [x] Export tray StatusNotifierItem (org.kde) và menu trạng thái chỉ đọc qua D-Bus,
  opt-in `--run-tray`; không cần Qt SDK, không tự bật host/extension.
- [x] Icon glyph `V`/`E`, `-` cho inactive, `O` cho IM khác, `?` cho unknown/unavailable;
  cập nhật từ monitor, tooltip/title ghi rõ sampled, không giả định trạng thái từng cửa sổ.
- [x] Thay icon bitmap ô vuông bằng nét bo tròn, khử răng cưa, nền trong suốt;
  xuất ARGB 24×24 và 48×48 cho panel HiDPI. Test cả năm glyph, alpha và hai kích thước.
  Sau thay đổi: 11/11 CTest Release, 4/4 test service ASan/UBSan và tray lặp 10 lần đạt.
- [x] Người dùng xác nhận icon bản đầu đã hiện trên GNOME (ảnh phản hồi);
  chưa nghiệm thu hình thức icon mới/menu hoặc KDE.
- [x] Sao lưu cấu hình Fcitx5, lưu tắt riêng `notificationitem` qua controller
  theo yêu cầu người dùng; không dừng daemon, không thay engine/preedit/hotkey.
- [ ] Sau đăng nhập lại, nghiệm thu chỉ còn icon OpenKey. Fcitx có thể fallback
  sang tray XEmbed khi tắt `notificationitem`; không tự tắt toàn bộ legacy tray
  GNOME vì ảnh hưởng ứng dụng khác. Xem [hướng dẫn](UBUNTU_SERVICE.md#ẩn-tray-fcitx5).
- [x] 11/11 CTest Release; service/reader/monitor/tray đạt ASan/UBSan. Tray private-bus
  kiểm tra host đến muộn, watcher recovery, GetAll/menu, tín hiệu, ARGB và no-op click.
- [ ] Nghiệm thu icon/menu trên panel GNOME AppIndicator và KDE Plasma thực tế;
  protocol test không thay kiểm thử desktop. Đã có Quit và Telex/VNI; còn các actions/hotkeys khác.
- [x] Port bước đầu `POPUP_OPENKEY_EXIT` (ID 2000): menu “Thoát tray OpenKey
  (giữ bộ gõ)”, chỉ dừng session service, không tắt Fcitx5 hoặc autostart.
  Test private-bus: enabled/label, bỏ qua hover/open/close/root/status,
  từ chối ID lạ; spawn service thật, click Quit, exit 0 và dọn socket.
  Sau thay đổi: 12/12 CTest Release, 5/5 test nền tảng ở build ASan/UBSan đạt;
  tray chạy lặp 10 lần đạt. Chỉ restart service tray để nạp menu mới.
- [ ] Nghiệm thu menu Quit trên panel thật: icon biến mất, gõ tiếng Việt vẫn
  hoạt động, chạy lại tray không sinh instance trùng.
- [ ] Port các lệnh trong [TrayContract.json](../Tests/TrayContract.json).
- [x] Port chọn Telex/VNI (ID 910/911) qua Fcitx5 controller: worker có timeout,
  không chặn tray/IPC, không auto-start daemon, whitelist IM trong nhóm hiện tại,
  kiểm tra lại nhóm/owner và xác nhận sau write; không tự sửa profile/thử lại write.
  Dòng kết quả trong menu, không đánh dấu chọn khi chưa biết IM thực.
  13/13 CTest Release và 6/6 test nền tảng trong build ASan/UBSan đạt;
  service/reader-actions/tray chạy lặp 5 lần đạt. Cập nhật binary cài riêng và
  restart unit tray; không đổi IM thật hoặc restart Fcitx5 trong kiểm thử.
- [ ] Nghiệm thu chuyển Telex/VNI từ menu trên desktop/VS Code; kiểm tra focus,
  thiếu VNI trong nhóm, lỗi daemon và gõ tiếp sau khi chuyển. Máy thử mới có Telex.
- [x] Phân biệt kiểu gõ trên tray: icon T=Telex, N=VNI, E=English; tooltip/title
  ghi Telex/VNI; menu radio chọn theo State+CurrentInputMethod đã được monitor
  xác nhận, không theo click. Chuyển ngoài tray cũng cập nhật; trạng thái lỗi,
  inactive/English/IM khác hoặc cache hết hạn xóa lựa chọn. IPC thêm telex/vni
  thay cho vietnamese gộp; giữ debounce hai mẫu riêng cho T↔N.
  13/13 CTest Release, 6/6 test nền tảng trong build ASan/UBSan đạt;
  reader/monitor/tray chạy lặp 10 lần đạt. Test T/N khác bitmap ở cả 24/48px,
  tooltip và dấu chọn khi đổi IM ngoài menu; cập nhật binary service đã cài.
  Đối chiếu D-Bus máy thật: input-status=telex, title Telex, menu 910=1/911=0.
  AppIndicators lại INACTIVE/Enabled: No ở lần kiểm tra này, tray waiting-host;
  chưa xác nhận hiển thị panel, không tự đổi thiết lập extension.
- [ ] Nghiệm thu T/N/E, tooltip và dấu chọn menu trên GNOME thật khi đổi bằng
  tray/phím tắt; chưa coi test D-Bus là xác nhận hình thức panel.
- [x] Thêm mục “Chuyển sang tiếng Anh (US)” (ID 920 riêng Linux): chọn
  `keyboard-us` đã có trong nhóm, dùng cùng worker/guard/xác nhận như Telex/VNI;
  radio và icon E theo monitor, không deactivate/restart Fcitx5. Đây là selector
  trực tiếp, chưa port toggle tiếng Việt ID 900 Windows.
  13/13 CTest Release, 6/6 test nền tảng trong build ASan/UBSan đạt; reader-actions
  và tray lặp 5 lần đạt. Test US có/thiếu trong nhóm, lỗi write không thử lại,
  menu radio theo trạng thái thật và hover không tác động. Đã cập nhật service cài riêng.
- [ ] Nghiệm thu vòng Telex → English → Telex/VNI trên desktop và VS Code,
  focus/ShareInputState, menu đang mở và gõ tiếp; thử thiếu US keyboard trong
  nhóm test riêng. Không sửa nhóm làm việc chỉ để kiểm thử.
- [x] Người dùng xác nhận đã kiểm thử phần chuyển Telex/English trên tray rồi
  yêu cầu tiếp tục; không suy ra toàn bộ ca VNI/focus/thiếu cấu hình đã đạt.
- [x] Thêm “Cấu hình bộ gõ Fcitx5…” (ID 1001 riêng Linux) qua controller Configure;
  worker timeout, không shell/auto-start daemon, dùng chung một slot với lệnh IM.
  Chỉ báo đã gửi yêu cầu, không giả định cửa sổ hiện; không đổi IM hoặc tự sửa
  cấu hình. Không phải cửa sổ OpenKey/Fluent hoặc lệnh control panel Windows.
  13/13 CTest Release, 6/6 test nền tảng trong build ASan/UBSan đạt;
  reader-actions/tray lặp 5 lần đạt. Đã cập nhật service cài riêng; chỉ đọc
  properties để đối chiếu menu trên bus thật, không tự mở cửa sổ người dùng.
- [ ] Nghiệm thu click cấu hình: cửa sổ Fcitx5 hiện, thêm VNI/chỉnh tùy chọn được,
  đóng cửa sổ vẫn gõ bình thường; thiếu tool cấu hình không được báo đã hiện UI.
- [x] Thêm prototype global hotkey X11 mở cấu hình Fcitx5: Ctrl+Alt+Shift+O,
  mặc định tắt ở build/runtime; chỉ phiên X11, không Wayland/XWayland. XCB
  checked passive grab, từ chối xung đột, Caps/Num Lock, chống auto-repeat,
  nhả grab khi keymap đổi/lỗi; worker không gửi trực tiếp lệnh IM từ X callback.
  IPC hotkey-status và menu hiển thị active/disabled/conflict/lỗi.
  15/15 CTest Release, 8/8 test nền tảng build ASan/UBSan đạt; Xvfb/service/tray
  lặp 5 lần đạt. Build không có X11 backend cũng đạt 9/9 test riêng.
  Test XTEST chỉ trên Xvfb giải nén tạm, không cài gói hệ thống/không gửi phím
  vào desktop thật. Service đã cập nhật; hotkey runtime vẫn tắt trên máy Wayland.
- [ ] Nghiệm thu hotkey mở cấu hình trên desktop X11 thật, map/layout tùy biến,
  xung đột WM và vòng đời phiên; chưa có hotkey đổi ngôn ngữ riêng của OpenKey.
- [x] Hiển thị giới hạn hotkey riêng trên tray theo XDG_SESSION_TYPE:
  Wayland chưa hỗ trợ, X11 chưa triển khai, thiếu/loại khác ghi chưa rõ phiên.
  Không suy ra X11 từ DISPLAY (có thể XWayland). Dòng hướng dẫn chỉnh phím tắt
  trong cấu hình Fcitx5, chỉ đọc/no-op; không đăng ký hook/đổi shortcut của người dùng.
  13/13 CTest Release, 6/6 test nền tảng trong build ASan/UBSan và tray lặp
  10 lần đạt. Đã cập nhật riêng binary service, không restart Fcitx5.
- [ ] Nghiệm thu dòng hướng dẫn phím tắt trên panel thật; chưa coi đây là
  triển khai hotkey toàn cục X11 hoặc tích hợp portal Wayland.
- [x] Thêm công cụ opt-in tạo/tắt XDG autostart desktop entry theo người dùng;
  Exec tuyệt đối `--run-tray`, không tự bật lúc build/install, không thay Fcitx5.
  Bảo toàn entry lạ/symlink, kiểm tra đường dẫn và test cấu hình riêng trong temp.
  12/12 CTest Release đạt; 5/5 test nền tảng trong build ASan/UBSan đạt
  (autostart là shell test); autostart chạy lặp 10 lần đạt.
  Xem [cách bật/tắt](UBUNTU_SERVICE.md#tự-khởi-động-tray-sau-đăng-nhập--opt-in).
- [ ] Nghiệm thu autostart qua đăng xuất/đăng nhập thật: một instance, V/E và
  gõ bình thường; disable không tự chạy. Đã quan sát cả hai entry autostart
  OpenKey/Fcitx5 và cả hai tiến trình đang chạy; chưa thay nghiệm thu đăng nhập.
- [x] Chuyển tray máy thử sang systemd user service theo chấp thuận người dùng:
  binary cài riêng, không phụ thuộc terminal agent/thư mục build; sao lưu và
  tắt desktop entry tray cũ để tránh hai cơ chế startup, giữ Fcitx5 nguyên trạng.
- [x] Unit opt-in theo graphical session, restart khi lỗi, rate-limit, log journal;
  Quit menu/IPC không restart. Test generator trong temp và systemd-analyze verify;
  kiểm thử trên unit thật SIGKILL phục hồi, Quit sạch không tự bật lại.
  Xem [vận hành/khôi phục](UBUNTU_SERVICE.md#systemd-user-service--chạy-độc-lập-terminal).
- [ ] Nghiệm thu service chạy lâu/qua đăng nhập lại và panel render icon.
  Lần kiểm tra tiếp theo AppIndicators đã ACTIVE/Enabled: Yes; service giữ nguyên
  PID 68802, NRestarts=0, IPC running và watcher có item OpenKey (registered).
  Không cần bật/tắt extension hoặc restart service; legacy tray vẫn tắt và
  desktop entry tray cũ disabled. Chờ xác nhận icon hiển thị/ổn định từ người dùng;
  trạng thái D-Bus chưa thay nghiệm thu panel hoặc đăng nhập mới.
- [x] Người dùng báo dùng thêm một lúc không còn mất icon sau khi chuyển systemd;
  nghiệm thu ổn định trong phiên thử, không thay xác nhận đăng nhập mới/KDE.

## v0.4 — UI giống Windows

Người dùng đã cài SDK; xác minh Qt 6.10.2. UI preview opt-in
`OPENKEY_BUILD_UI=ON`, mặc định OFF; không cần Qt cho engine/service khi UI tắt.
Hướng dẫn mở và checklist desktop: [Ubuntu UI](UBUNTU_UI.md).
Tổng quan có model chỉ đọc qua IPC + file cấu hình đã lưu và API áp dụng chế độ
Preedit/Direct và DirectDiagnostics với xác nhận runtime sau explicit Apply.
UI/tray đã cài ở tài khoản thử, chưa thêm vào `.deb`; không restart Fcitx5.

- [x] Chọn Qt 6/Qt Quick/QML (minimum khai báo 6.4, đã test SDK 6.10.2).
- [x] Dựng cửa sổ chính, sidebar thu gọn và đủ 8 mục theo Windows.
- [x] Dựng design tokens cho màu, spacing, typography và corner radius.
- [x] Tạo theme sáng/tối/theo SystemPalette trong phiên chạy, chưa lưu bền.
- [x] Lưu riêng theme/sidebar cho UI normal trong `openkey/ui.ini`, khôi phục
  khi mở lại; ghi atomic, báo lỗi và không ghi đè file không hợp lệ. CLI theme
  chỉ ghi đè phiên mới, offline/smoke/screenshot không đọc/ghi preferences.
- [x] UI đạt 9/9 CTest với test preferences round-trip, file lỗi/phiên bản lạ,
  special file và lỗi ghi; kiểm tra tương tác lưu/thông báo lỗi ở scale 100%/200%.
- [ ] Nghiệm thu lưu/khôi phục theme và sidebar trên desktop thật.
- [x] Test offscreen: điều hướng, sidebar, theme, bàn phím, 720×560 và scale 200%;
  resource QML đóng gói nạp được. Build UI đạt 5/5 CTest gồm 2 engine tests.
- [ ] Nghiệm thu shell/theme trên GNOME thực tế, nhất là theme hệ thống khi đổi lúc chạy.
- [x] Tạo model chỉ đọc: trạng thái lấy mẫu qua IPC service, đọc `InputStyle` và
  `DirectDiagnostics` từ file người dùng; phân biệt cấu hình đã lưu với runtime.
- [x] IPC bất đồng bộ, timeout/giới hạn phản hồi/UID peer; mất kết nối không giữ
  trạng thái cũ. Không tự bật service/Fcitx5 hoặc ghi file. Chế độ `--offline`.
- [x] Test model bằng fixture riêng: polling, enum, lỗi protocol, timeout và
  reconnect, cấu hình thiếu/lỗi, FIFO/symlink; binding card, không ghi cấu hình.
  Build UI đạt 6/6 CTest (gồm 2 engine tests), scale 100% và 200%.
- [ ] Nghiệm thu dữ liệu Tổng quan trên desktop với service đang chạy.
- [x] Thiết kế API cấu hình qua Fcitx5: schema/enum whitelist, owner-pinned,
  patch chỉ InputStyle, kiểm tra conflict và readback runtime; không suy ra lưu bền.
  Xem [thiết kế API](UBUNTU_CONFIG_API.md).
- [x] Nối điều khiển Preedit/Direct trên Tổng quan, chỉ gửi khi nhấn Áp dụng;
  khóa khi busy/offline/lỗi schema, không tự retry hoặc restart bộ gõ.
- [x] Build UI đạt 7/7 CTest; controller có 34 ca QtTest trên bus mock riêng,
  gồm schema, partial patch, conflict, timeout, owner và readback; UI thử explicit
  Apply/không ghi khi chọn, khóa busy/offline ở scale 100% và 200%.
- [x] Người dùng xác nhận kiểm thử desktop sau sửa popup và chuyển task; nghiệm thu
  cơ bản UI/đổi chế độ trong phiên thử. Không coi là kiểm chứng mọi backend/DPI
  hoặc lưu bền khi addon gặp lỗi; build/test chỉ dùng mock cho API ghi.
- [x] Sửa popup chọn chế độ/theme bị chữ trắng trên nền sáng ở dòng được chọn;
  test popup mở, các dòng/highlight có tương phản >= 4.5:1 trên sáng/tối và scale
  100%/200%, không tự Apply khi mở hoặc duyệt menu. UI đạt 7/7 CTest.
- [x] Thêm menu “Cài đặt OpenKey…” riêng với cấu hình Fcitx5; khởi chạy qua D-Bus
  activation khi click, không chạy shell hoặc thay chế độ gõ.
- [x] UI normal single-instance theo session bus: lần mở sau gọi Show rồi thoát;
  offline/smoke/screenshot không chiếm name, Wayland có thể hạn chế focus.
- [x] UI 8/8 CTest, activation 17 ca private-bus (cold launch, owner/PID, forward,
  offline), addon/service 16/16 (menu, launcher error/timeout/worker không chặn).
- [x] Cài UI/activation file vào `~/.local`, cập nhật và restart riêng tray;
  Fcitx5 giữ nguyên tiến trình. D-Bus nhận activation name sau ReloadConfig;
  tray đang waiting-host, chưa xác nhận menu trên panel. Không tự mở GUI để test.
- [ ] Nghiệm thu mở từ tray lần đầu, click lặp/minimize/đóng-mở trên desktop;
  không dừng bộ gõ khi đóng UI.
- [x] Trang Nâng cao: đọc runtime và bật/tắt DirectDiagnostics qua Fcitx5, chỉ
  Apply mới ghi; patch một khóa giữ nguyên mode, xác thực schema, conflict theo
  khóa đang sửa và readback. Không tự bật chẩn đoán, đọc hoặc tải log lên.
- [x] Controller mở rộng có 62 ca QtTest; UI kiểm tra bật/tắt không tự Apply,
  giữ mode, khóa busy/offline và tương phản popup diagnostics ở sáng/tối,
  scale 100%/200%. Test launcher/menu cô lập activation dirs, không mở UI đã cài.
- [ ] Nghiệm thu bật/tắt diagnostics trên desktop, xác nhận mode không đổi và
  đối chiếu thiết lập đã lưu; các tùy chọn nâng cao khác chưa tích hợp.
- [x] Trang Chính tả: nối CheckSpelling/UseModernOrthography từ UI qua API đến
  engine, mặc định đều bật; Apply từng khóa, giữ mode và các thiết lập khác.
- [x] Options riêng từng context Telex/VNI; cập nhật context active/idle sau đổi
  cấu hình, kết thúc composition an toàn, không commit lặp phần Direct đã nhập.
- [x] UI kiểm tra capability theo schema: addon cũ vẫn dùng mode/diagnostics,
  nhóm chính tả thiếu/sai/trùng bị khóa. 160 ca controller QtTest, UI 9/9 CTest,
  kiểm tra Apply hai hướng, busy/offline và popup sáng/tối 100%/200%.
- [x] Tạo gói addon alpha.10 và SHA-256; 7/7 CTest, kiểm tra payload/dependency
  và addon giải nén. Chưa tự cài addon hay restart Fcitx5 của desktop.
- [x] Kiểm thử hồi quy addon/service sau mở rộng options: 16/16 CTest đạt;
  kiểm tra ảnh trang Chính tả sáng/tối offscreen, không thay nghiệm thu desktop.
- [x] Cài riêng UI mới vào tài khoản thử sau 9/9 CTest; addon đang cài vẫn
  alpha.9, Fcitx5 không restart. Đóng UI cũ rồi mở lại để nhận trang mới.
- [ ] Nghiệm thu alpha.10 desktop: hai tùy chọn Bật/Tắt, Telex/VNI,
  Preedit/Direct, mode/khóa còn lại giữ nguyên; đối chiếu lưu bền sau mở lại.
- [x] Trang Phím tắt có backend chỉ đọc tám danh sách runtime Fcitx5, không
  đoán mặc định; Đọc lại bất đồng bộ, pin owner/no auto-start, xóa mẫu khi lỗi
  hoặc đổi owner. Không ghi cấu hình/đăng ký hotkey hoặc bắt phím toàn cục.
- [x] Model Phím tắt đạt 33 ca QtTest trên bus fixture, gồm schema/list lỗi,
  timeout, owner đổi và cờ NO_AUTO_START trên wire. UI đạt 10/10 CTest, bao gồm
  tương tác chỉ đọc, refresh bàn phím, khóa offline/loading và scale 200%.
  Đã cài riêng UI mới; không cập nhật addon hoặc restart Fcitx5/tray.
- [ ] Nghiệm thu trang Phím tắt desktop: so với Global Options, Đọc lại sau
  thay đổi ngoài UI, cuộn cửa sổ nhỏ/scale 200%; không đánh đồng cấu hình với
  việc desktop thật sự chuyển tổ hợp phím đến Fcitx5.
- [x] Trang Tương thích tích hợp một phần: snapshot loại phiên và backend của
  riêng UI qua whitelist, không suy ra backend ứng dụng đang focus; hướng dẫn
  Direct/fallback và VS Code, không chạy script hoặc sửa cấu hình editor.
- [x] Dùng chung một điều khiển InputStyle giữa Tổng quan/Tương thích, Apply
  qua API hiện có; không có draft thứ hai hoặc tự ghi khi chuyển trang.
- [x] UI đạt 11/11 CTest sau tích hợp Tương thích: whitelist môi trường, Apply
  đúng mode/giữ thiết lập khác, trạng thái chưa xác nhận, offline/busy, cuộn cửa
  sổ nhỏ và popup sáng/tối ở scale 100%/200%. Đã cài riêng UI mới, không thay
  addon hoặc restart Fcitx5/tray; chờ kiểm thử desktop.
- [ ] Nghiệm thu trang Tương thích desktop: môi trường UI, chuyển hai trang,
  Apply mode chung, cuộn/DPI; không coi runtime Direct là kết quả gõ của app khác.
- [ ] Hoàn thiện Tổng quan/Chính tả, thêm chỉnh sửa Phím tắt và chính sách theo app;
  gõ nhanh phụ âm đầu/cuối và khôi phục từ sai vẫn chưa tích hợp.
- [x] Gõ nhanh Telex: nối QuickTelex mặc định tắt từ UI/API đến replay riêng
  context, chỉ Telex; kiểm thử Preedit/Direct, Backspace, caps, VNI không đổi,
  đổi cấu hình/persistence và schema tương thích ngược. Tạo gói alpha.11.
- [x] QuickTelex: UI 13/13 CTest (227 ca controller, tương tác/popup sáng/tối
  ở 100%/200%), addon/service 17/17; gói alpha.11 qua 8/8 CTest và kiểm tra
  payload/dependency/addon giải nén, SHA-256 đúng. Sửa đồng bộ picker khi schema
  chuyển từ chưa hỗ trợ sang hỗ trợ; không dùng binding cũ trong changed handler.
- [x] Cài riêng UI mới; addon desktop vẫn alpha.9, không restart Fcitx5/tray.
  Nhóm chính tả/QuickTelex chỉ mở khi schema addon đang chạy hỗ trợ tương ứng.
- [ ] Nghiệm thu alpha.11 desktop: bật/tắt QuickTelex, bảy cặp Telex, Backspace,
  Preedit/Direct, VNI không đổi, mode/chính tả/diagnostics giữ nguyên và lưu bền.
- [x] Trang Chuyển mã có công cụ Unicode thủ công: chuẩn hóa NFC/NFD, đổi
  hoa/thường, bỏ dấu tiếng Việt; model Qt Core cục bộ, không dùng global options
  của engine legacy hoặc API bộ gõ. Chỉ chuyển khi nhấn nút, nguồn giữ nguyên.
- [x] Giới hạn/kiểm tra Unicode và options, xóa kết quả cũ khi đổi nguồn/tùy
  chọn hoặc gặp lỗi; Xóa văn bản xóa cả hai ô, không lưu nội dung vào preferences.
- [x] UI đạt 12/12 CTest, gồm model Unicode, thao tác chuyển đổi chủ động/giữ
  nguồn/xóa kết quả cũ/giới hạn, popup sáng/tối và scale 100%/200%. Giảm giới hạn
  đồng bộ xuống 8192 đơn vị UTF-16 sau ca Unicode dài xử lý chậm; bộ test model
  sau giảm giới hạn đạt khoảng 0.06 giây trên máy thử. Đã cài riêng UI mới,
  không cập nhật addon hoặc restart Fcitx5/tray.
- [ ] Nghiệm thu công cụ Unicode desktop: NFC/NFD, hoa/thường, bỏ dấu, emoji,
  chọn/sao chép kết quả thủ công, cuộn cửa sổ nhỏ, đóng/mở không giữ nội dung.
- [x] Trang Giới thiệu: metadata binary UI/Qt, ghi nguồn engine/fork, URL dạng
  PlainText, toàn văn LICENSE đóng trong Qt resource và chỉ đọc khi click.
  Không suy ra phiên bản addon, chạy shell, mở browser hoặc kiểm tra cập nhật.
- [x] UI đạt 14/14 CTest: metadata/Qt đúng bản build, resource khớp LICENSE,
  nạp/ẩn/mở lại không lẫn model; tương tác chỉ đọc, cửa sổ nhỏ, sáng/tối và
  scale 100%/200%. Đã cài riêng UI mới; không cập nhật addon hoặc restart bộ gõ.
- [ ] Nghiệm thu Giới thiệu desktop: metadata, mở/ẩn/cuộn giấy phép, chọn địa
  chỉ nguồn, cửa sổ nhỏ/theme và không ảnh hưởng bộ gõ khi đóng UI.
- [ ] Hoàn thiện Nâng cao, Macro và chuyển mã legacy.
- [x] Thêm job CI UI riêng trên Ubuntu 24.04, Qt từ APT, addon/service OFF;
  offscreen/software/compose, toàn bộ CTest và HiDPI, JUnit/log giữ 7 ngày.
  Kiểm tra cài component UI vào staging và chạy smoke, không cài vào hệ thống.
- [x] Kiểm tra cấu trúc YAML và chạy tương đương job trong build tạm sạch:
  Ubuntu 26.04/Qt 6.10.2 đạt 14/14, JUnit không có failure, staged install và
  smoke binary đã cài đạt; không ghi vào bản UI đang cài hoặc restart bộ gõ.
- [ ] Chạy/nghiệm thu job UI trên GitHub runner sau khi mã lên remote; chưa coi
  kiểm thử cục bộ Ubuntu 26.04/Qt 6.10.2 là bằng chứng Ubuntu 24.04 đã đạt.
- [x] Điều hướng bàn phím trong cửa sổ: Ctrl+Tab/Ctrl+Shift+Tab quay vòng trang,
  F1 mở Giới thiệu; focus sidebar/menu thu gọn, tooltip và metadata accessible.
  Test sáng/tối, bản nháp Chuyển mã, cuộn đầu trang và cửa sổ khác active;
  không Apply/refresh/chuyển đổi/clipboard hoặc đăng ký hotkey hệ thống.
- [x] Sau điều hướng bàn phím: 15/15 CTest đạt (UI 100%/200%); cài riêng
  component UI, binary cài khớp bản build và smoke headless đạt. Không thay
  addon hoặc restart Fcitx5/tray; đóng cửa sổ UI cũ rồi mở lại để dùng bản mới.
- [ ] Nghiệm thu keyboard navigation trên desktop, DPI và Orca/AT-SPI;
  test headless không thay kiểm chứng screen reader hoặc phím do compositor giữ.

## v0.5 — Tính năng nâng cao

- [ ] Ưu tiên sửa trực tiếp lỗi mất từ đầu trong terminal VS Code: ca người
  dùng `tieengs vieetj` → `' vieêt'`. Người dùng xác nhận Wayland, lỗi lặp sau
  mỗi Enter chạy lệnh, Space mới gõ tiếp. Ca Telex/VNI lặp Enter headless đạt
  trên bản cũ, chưa tái hiện native. Cần đối chứng ASCII/chuyển dấu và log mới;
  đối chứng mới `abc def` → `' deè'`. Candidate alpha.32 cho phím đầu sau Enter
  đi native; chỉ nhận ownership khi ACK đúng, không ACK thì raw hết từ đầu.
  Regression mới FAIL với alpha.31, PASS bản mới; chờ desktop nghiệm thu,
  chưa sửa lỗi dấu `deè`. Build alpha.32 đạt 8/8 CTest, verifier payload/
  dependency/addon giải nén và checksum; chưa tự cài/restart daemon.
  Desktop alpha.32: `abc xyz` đạt, `ls` chỉ còn `l`. Candidate alpha.33 giữ
  chuỗi ASCII nối thêm nguyên dạng đi native; test `ls/git/cd` FAIL bản cũ,
  PASS bản mới, 8/8 CTest/verifier gói đạt; chờ nghiệm thu desktop alpha.33.
  Alpha.33 desktop vẫn mất từ đầu khi focus từ app khác/editor vào terminal.
  Candidate alpha.34 bắt đầu mọi từ ASCII nguyên dạng bằng native (không cần
  Enter), giữ ACK chính xác và fallback raw; test focus/refocus/context tái
  sử dụng FAIL bản cũ, PASS bản mới. 8/8 CTest, verifier gói/addon giải nén và
  checksum đạt; còn chờ desktop alpha.34. Không tự cài/restart daemon.
  Desktop alpha.34: `pwd` chỉ hiện `p`. Engine Telex đổi `pw` → `pư`, rời
  native-append và gặp lại giới hạn chuyển dấu của xterm.js. Không coi các
  lệnh có phím dấu là ASCII nguyên dạng, không thêm allowlist riêng từng lệnh.
  Chưa có bản sửa cho ca này; cần chọn semantics nếu bỏ shortcut standalone W,
  hoặc dùng EN khi nhập lệnh. Không tự đổi cấu hình/ngôn ngữ người dùng.
  Dừng phát triển extension/helper; giữ bản
  thử dưới đây để tham khảo, không coi là giải pháp được nghiệm thu.

- [x] Extension VS Code thử nghiệm chuyển EN khi vào terminal qua Ctrl+`,
  quay editor khôi phục OpenKey Telex/VNI trước đó nếu còn sở hữu trạng thái.
  Opt-in User Setting, local UI host Linux, không tự nhận diện chuột, không
  đọc nội dung terminal/editor hoặc thay addon/profile. Không ép ghi đè nếu
  người dùng đã chọn IM khác; busy không xếp hàng, mất focus/disable thì hủy.
- [x] Extension TypeScript 0.1.0 có 75 test giả lập cho core, process bridge
  và adapter VS Code; đóng VSIX tám file, không kèm node_modules/test.
  npm audit không báo lỗ hổng tại lần kiểm tra; CI Node 22 build/test/đóng gói,
  chưa chạy remote. Chưa cài VSIX, chưa đổi EN/VI hoặc restart Fcitx5 thật.
- [ ] Nghiệm thu VSIX trên desktop Ubuntu: Telex/VNI/English, Ctrl+` hai
  chiều, nhiều terminal, đổi IM thủ công, mất focus giữa thao tác, Wayland/
  XWayland và Remote SSH. Delay focus không phải ACK frontend Fcitx5.
  Xem [hướng dẫn extension](../tools/vscode-terminal-english/README.md).
- [x] Prototype chuột bằng helper AT-SPI chạy riêng từ source: probe chỉ đọc
  mặc định, `--switch` opt-in, khóa một helper/desktop, giới hạn PID Code chính
  thức và cùng frame. Chỉ đọc role/class/focus/process; unknown/mất focus/lỗi
  quên lịch sử, không đọc Text/Value/name/description, không sửa VS Code.
  Có 21 test policy/bridge giả lập; 75 test extension cũ vẫn đạt. Probe thật
  trên Ubuntu đăng ký listener và dừng sạch, chưa bật chuyển EN/VI thật.
- [ ] Nghiệm thu metadata và chuột trên desktop: bật accessibility nếu cần,
  probe phải phân biệt editor/terminal kể cả click lại cùng terminal; sau đó
  mới thử `--switch` với Telex/VNI/English, nhiều cửa sổ, Quick Open, Alt+Tab,
  đổi IM thủ công, Wayland/XWayland. Nếu class DOM không expose thì unknown,
  chưa coi hỗ trợ chuột hoàn tất. Tắt extension keyboard khi dùng helper.
- [ ] Macro editor và import/export.
- [x] Clipboard thủ công cho Chuyển mã: Dán nguồn/Sao chép kết quả chỉ khi click,
  giới hạn/Unicode/plain text, giữ bản nháp và clipboard khi lỗi; không poll,
  không đọc lại khi copy, không tự dán hoặc sửa tài liệu ứng dụng khác.
- [x] UI đạt 15/15 CTest, gồm clipboard bridge dùng fake backend, Unicode/MIME/
  giới hạn và không có GUI application, Dán/Copy chỉ theo thao tác, giữ bản nháp
  khi lỗi và khóa kết quả cũ; tương tác 100%/200%. Không test clipboard thật.
  Đã cài riêng UI mới sau kiểm thử; không thay addon hoặc restart Fcitx5/tray.
- [ ] Nghiệm thu clipboard native Wayland/X11, transfer/owner chậm và history;
  test fake không thay kiểm chứng clipboard thật. Hotkey chuyển clipboard nhanh
  toàn cục và timeout/fetch worker riêng vẫn chưa triển khai.
- [x] Chuyển Unicode sang worker Qt Concurrent, tối đa một tác vụ/model,
  có Hủy; sửa nguồn/tùy chọn hoặc Xóa vô hiệu hóa kết quả muộn. Không chạy
  lại tự động, không dùng QObject UI trong worker. Nâng giới hạn nguồn lên
  65536/kết quả 262144 đơn vị UTF-16, clipboard dùng cùng giới hạn.
- [x] Sau worker: UI 15/15 CTest đạt, model chuyển đổi 104 ca QtTest,
  UI 45 ca ở mỗi scale 100%/200%; thêm 13 ca lifecycle/cancel/clear/destruction
  đạt với ASan/UBSan (bật leak detection). Không dùng clipboard/Fcitx5 thật.
- [x] Cài riêng component UI mới sau kiểm thử; binary cài khớp bản build,
  smoke headless đạt. Không thay addon hoặc restart Fcitx5/tray; đóng UI cũ
  rồi mở lại để dùng worker và nút Hủy.
- [ ] Nghiệm thu chuyển đổi nền trên desktop: Unicode dài, Hủy/sửa nguồn/
  tùy chọn/Xóa, chuyển trang khi đang xử lý, đóng/mở UI và clipboard native.
  Hủy không ngắt giữa một lời gọi Qt; chờ worker cũ trước khi nhận tác vụ mới.
- [ ] Tài liệu trên 65536 đơn vị UTF-16, tiến trình riêng/hủy CPU tức thời và
  timeout xử lý Unicode; hiện vẫn giới hạn bộ nhớ, chưa có đọc/ghi file lớn.
- [ ] Cấu hình theo ứng dụng.
- [x] Phần UI của lưu/khôi phục: tạo JSON theme/sidebar trong Nâng cao,
  kiểm tra/xem trước rồi xác nhận; sửa JSON khóa khôi phục đến khi xem lại.
  Kiểm tra schema/giới hạn, lưu hai giá trị atomic một lần; lỗi không thay
  lựa chọn giao diện. Không tự đọc/ghi file sao lưu hoặc clipboard.
- [x] Sau JSON giao diện: 15/15 CTest đạt, preferences 67 ca QtTest,
  UI 48 ca mỗi scale 100%/200%. Đã cài riêng UI mới, binary khớp bản build
  và smoke headless đạt; không thay addon hoặc restart Fcitx5/tray.
- [ ] Nghiệm thu JSON giao diện desktop: chọn/sao chép/dán thủ công, xem
  trước, khôi phục hai theme/menu, lỗi file/quyền, đóng/mở và DPI.
- [ ] Lưu/khôi phục cấu hình bộ gõ Fcitx5, macro và hotkey; JSON giao diện
  không phải bản sao toàn bộ OpenKey. File picker/xuất file riêng chưa có.
- [x] Tổng quan có bảng kết nối/lỗi: phân biệt mẫu service/input method,
  API cấu hình runtime và file đã lưu; hướng dẫn cho conflict/unconfirmed/
  unsupported, service timeout/protocol-error và file lỗi. Không coi thiếu
  service là bộ gõ đã dừng hoặc có dữ liệu là engine đã nghiệm thu.
- [x] Nút Đọc lại trạng thái chỉ refresh model và reload API theo click,
  khóa khi đang đọc/áp dụng; không tự Apply, retry write, restart hoặc dùng
  clipboard. Enum không nhận diện chỉ hiện thông báo cố định, không in dữ liệu lạ.
- [x] Sau bảng trạng thái: 15/15 CTest đạt, UI 50 ca mỗi scale 100%/200%,
  dùng nguồn giả cho lỗi/kết nối, refresh theo bàn phím và không ghi cấu hình.
  Đã cài riêng UI mới, binary khớp bản build và smoke headless đạt; không
  thay addon hoặc restart Fcitx5/tray.
- [ ] Nghiệm thu bảng trạng thái desktop khi service/API mất rồi kết nối lại,
  Apply chưa xác nhận/xung đột và file lỗi. Đây chưa phải giám sát sức khỏe
  engine, theo dõi log hoặc thông báo native/toast toàn hệ thống.

## v0.6 — Đóng gói

- [ ] Tạo AppImage thử nghiệm.
- [x] Tạo gói `.deb` alpha cho addon Fcitx5 (chưa có UI).
- [x] Đóng gói UI độc lập `openkey-settings` preview.1: component riêng,
  launcher menu ứng dụng và D-Bus activation; khai báo Qt/QML runtime,
  không kèm addon/tray, không bật autostart hoặc restart bộ gõ.
- [x] Thêm script build/test/CPack và verifier UI: SHA-256, dependency,
  payload đúng sáu file, không maintainer script, desktop entry hợp lệ và
  smoke binary giải nén. CI UI tạo/upload gói riêng; chưa chạy trên remote.
- [x] Build Ubuntu 26.04/Qt 6.10.2: 15/15 CTest đạt; gói UI preview.1 amd64
  qua kiểm tra dependency, sáu file đúng, desktop entry, SHA-256 và smoke
  binary giải nén. Guard từ chối UI OFF, addon ON và prefix khác `/usr`.
  Chưa cài gói/không restart Fcitx5/tray; checksum addon alpha.11 giữ nguyên.
- [ ] Nghiệm thu APT/UI desktop: cài mới/nâng cấp/gỡ, launcher, singleton,
  dữ liệu người dùng giữ nguyên và bản `~/.local` không che bản `/usr`.
- [x] Thêm kiểm thử vòng đời payload UI bằng dpkg trong root/database/log
  tạm: unpack, cài đè cùng bản, nâng phiên bản QA, remove, unpack lại và purge;
  đối chiếu sáu file và hash cấu hình giả lập sau từng bước. Không configure,
  giải dependency bằng APT hoặc thay gói/cấu hình hệ thống thật.
- [x] Guard lifecycle không chạy root, không dùng script-chrootless/
  force-depends; từ chối hook/path filter/tùy chọn dpkg ngoài allowlist.
  Script build và CI chạy kiểm tra riêng sau khi tạo gói, không xóa build cũ.
- [x] Chạy lifecycle trên Ubuntu 26.04, UID thường: cả sáu bước đạt,
  payload đủ 6/6 khi unpack và còn 0/6 sau remove/purge; 5/5 hash fixture
  giữ nguyên. Guard từ chối parent `/`, đường tương đối và gói addon.
  Đối chiếu trước/sau: database/log dpkg hệ thống, UI local và cấu hình
  OpenKey/Fcitx5 không đổi. Không đánh dấu nghiệm thu APT/desktop ở trên.
- [x] Kiểm tra dependency, payload và nạp addon từ gói đã giải nén.
- [ ] Tạo Flatpak.
- [ ] Kiểm tra cài mới, nâng cấp, gỡ cài đặt và giữ cấu hình.
- [x] Thêm checksum SHA-256 và job artifact `.deb` vào CI (chưa chạy trên remote).

Build/cài/gỡ bản alpha: [Ubuntu install](UBUNTU_INSTALL.md). Kiểm thử cài/gỡ bằng
APT trên máy desktop vẫn chưa nghiệm thu; không đánh dấu xong mục vòng đời gói ở trên.

## Điều kiện nghiệm thu bản Ubuntu đầu tiên

- Gõ Telex và VNI ổn định trong các ứng dụng phổ biến.
- Không mất phím khi chuyển cửa sổ hoặc dùng Backspace.
- Tray, hotkey và autostart hoạt động đúng trên desktop được hỗ trợ.
- UI có theme sáng/tối, tiếng Việt và điều hướng bằng bàn phím.
- Có gói cài đặt và hướng dẫn gỡ cài đặt.
