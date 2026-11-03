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
Preedit/Direct với xác nhận runtime sau explicit Apply;
chưa thêm vào `.deb`, chưa thay đổi bộ gõ/tray đang chạy.

- [x] Chọn Qt 6/Qt Quick/QML (minimum khai báo 6.4, đã test SDK 6.10.2).
- [x] Dựng cửa sổ chính, sidebar thu gọn và đủ 8 mục theo Windows.
- [x] Dựng design tokens cho màu, spacing, typography và corner radius.
- [x] Tạo theme sáng/tối/theo SystemPalette trong phiên chạy, chưa lưu bền.
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
- [ ] Nghiệm thu Apply hai chiều trên desktop và đối chiếu cấu hình đã lưu;
  chưa dùng API ghi trên bộ gõ thật trong build/test.
- [x] Sửa popup chọn chế độ/theme bị chữ trắng trên nền sáng ở dòng được chọn;
  test popup mở, các dòng/highlight có tương phản >= 4.5:1 trên sáng/tối và scale
  100%/200%, không tự Apply khi mở hoặc duyệt menu. UI đạt 7/7 CTest.
- [ ] Port trang Tổng quan, Chính tả, Phím tắt và Tương thích.
- [ ] Port trang Nâng cao, Macro, Chuyển mã và Giới thiệu.
- [ ] Kiểm thử DPI, keyboard navigation và accessibility.

## v0.5 — Tính năng nâng cao

- [ ] Macro editor và import/export.
- [ ] Chuyển mã clipboard.
- [ ] Cấu hình theo ứng dụng.
- [ ] Lưu/khôi phục cấu hình.
- [ ] Thông báo lỗi và trạng thái engine.

## v0.6 — Đóng gói

- [ ] Tạo AppImage thử nghiệm.
- [x] Tạo gói `.deb` alpha cho addon Fcitx5 (chưa có UI).
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
