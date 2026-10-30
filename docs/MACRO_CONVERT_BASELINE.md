# Baseline Gõ tắt và Chuyển mã

Kiểm kê source ngày 2026-10-11. Đây là danh sách hành vi cần bảo toàn,
không phải biên bản nghiệm thu GUI/engine. Không mở ứng dụng hoặc thay dữ liệu
cấu hình để lập tài liệu này. Nghiệm thu trên VM/snapshot với dữ liệu thử riêng;
engine khởi động có thể cập nhật startup, xem TRAY_LIFECYCLE_BASELINE.md.

## Gõ tắt — dialog Win32

Nguồn: MacroDialog.cpp, Macro.cpp và Macro.h.

- Danh sách hai cột tên/nội dung; chọn hàng đưa dữ liệu vào hai ô sửa.
  Danh sách hiển thị theo thứ tự đảo của vector do getAllMacro trả về.
- Nhập tên đã tồn tại đổi nhãn nút từ Thêm sang Sửa. Thêm/sửa gọi addMacro;
  sửa tên trong legacy không phải thao tác đổi tên nguyên tử của một hàng.
- Tên hoặc nội dung rỗng bị chặn bằng MessageBox. Buffer thêm/xóa là
  4096 TCHAR; việc dò tên để đổi nhãn dùng buffer 128 TCHAR.
- Xóa theo tên đang có trong ô tên; không có hộp xác nhận xóa.
- Sau ghi/xóa thành công: lưu macroData REG_BINARY, tải lại danh sách,
  xóa ô nhập và đưa focus về tên. Các thao tác lưu ngay, đóng dialog không rollback.
- Nhập file txt qua file picker, hỏi giữ dữ liệu hiện có (Yes: gộp,
  No: thay thế), gọi readFromFile rồi lưu/tải lại. Handler legacy không kiểm tra
  kết quả import trước saveAndReload; không coi đây là chuẩn xử lý lỗi an toàn.
- Xuất file txt, tên mặc định OpenKeyMacro, hỏi ghi đè bằng save picker.
- Tự viết hoa dùng vAutoCapsMacro và lưu ngay. Bật chức năng gõ tắt nói chung
  và gõ tắt trong chế độ tiếng Anh là tùy chọn khác, không phải checkbox này.
- OK/đóng chỉ đóng dialog; không thoát engine.

## Đối chiếu trang Gõ tắt Fluent

Nguồn: MacroPage.xaml.cpp, MacroService.cpp và SettingsServiceStore.cpp.

- Giữ dữ liệu macroData và vAutoCapsMacro; không yêu cầu chuyển đổi dữ liệu cũ.
- Có danh sách, tìm trong tên/nội dung, số kết quả, trạng thái rỗng, chọn sửa,
  thêm mới, lưu/đổi tên, xóa, tải lại, nhập và xuất file.
- Khác biệt có chủ đích: từ chối tên trùng thay vì ghi đè ngầm; đổi tên dựa vào
  originalShortcut. Xóa có xác nhận, mặc định Hủy; chuyển chọn/mới/tải lại
  có bảo vệ bản nháp ở các handler tương ứng. Không suy rộng thành bảo vệ mọi
  đường đóng cửa sổ hoặc chuyển trang nếu chưa nghiệm thu.
- Validate giới hạn theo UTF-8: tên tối đa 255 byte, nội dung tối đa 65535 byte;
  không phải số ký tự hiển thị. Thêm/sửa không nhận tên/nội dung rỗng.
- Nhập kiểm tra header, UTF-8, BOM/CRLF; bỏ qua dòng lỗi/trùng và báo số lượng.
  Gộp không ghi đè store hỏng; thay thế tường minh có thể phục hồi store hỏng.
  Không có dòng hợp lệ thì không ghi store.
- Xuất từ store đã đọc hợp lệ; nội dung nhiều dòng bị từ chối vì định dạng txt
  không round-trip được. Lỗi đọc/ghi báo InfoBar, không giả vờ thành công.
- Khóa thao tác khi dialog bất đồng bộ đang mở; có phím tắt tìm kiếm/xóa.
  Poll 500 ms ở trang đang tải chỉ đồng bộ tùy chọn viết hoa, không tự tải lại
  danh sách macro hoặc bỏ bản nháp; dùng nút tải lại cho dữ liệu ngoài trang.
- MacroPage có thêm bật gõ tắt và gõ tắt trong chế độ Anh, dùng vUseMacro và
  vUseMacroInEnglishMode qua Preferences. Lệnh Gõ tắt ở tray ưu tiên trang
  Fluent (--page macro), fallback MacroDialog nếu không launch được UI.

## Chuyển mã — dialog Win32

Nguồn: ConvertToolDialog.cpp, OpenKeyHelper::quickConvert và shared services.

- Hai combo nguồn/đích: Unicode, TCVN3, VNI Windows, Unicode tổ hợp,
  Windows-1258; nút đổi chỗ lưu lại hai giá trị cấu hình.
- Bốn kiểu chữ loại trừ nhau: toàn bộ hoa, toàn bộ thường, hoa đầu câu,
  hoa đầu mỗi từ. Bỏ dấu là tùy chọn độc lập, kết hợp được với một kiểu chữ.
- Dialog chỉ chuyển clipboard; checkbox dùng clipboard luôn bật và bị vô hiệu
  hóa. Không có vùng nhập/kết quả như Fluent.
- Checkbox thông báo lưu nghịch đảo convertToolDontAlertWhenCompleted.
  Nút chuyển thành công có thể hiện MessageBox; hotkey/tray dùng thông báo
  riêng của engine, không phải hộp thông báo của dialog.
- Phím tắt gồm Ctrl/Alt/Win/Shift và phím chính; Delete/Backspace xóa phím
  chính thành sentinel 0xFE. Thay đổi cập nhật cấu hình và tray ngay.
- OK/đóng chỉ đóng dialog. Lệnh chuyển nhanh ở tray xử lý clipboard trực tiếp;
  lệnh mở công cụ ưu tiên trang Fluent (--page convert), fallback legacy
  nếu không launch được UI.

## Đối chiếu trang Chuyển mã Fluent

Nguồn: ConvertPage.xaml.cpp, ConvertService và ClipboardConvertService.

- Giữ nguồn/đích, đổi chỗ, bốn kiểu chữ loại trừ, bỏ dấu, thông báo và phím tắt
  clipboard trong cấu hình dùng chung. Combo hiển thị giới hạn chỉ số 0..4.
- Thêm vùng nhập/kết quả, Chuyển, Sao chép, Xóa. Chuyển văn bản không tự ghi
  clipboard; Sao chép mới ghi kết quả dạng text. Xóa không reset cấu hình.
- Chuyển clipboard dùng dịch vụ chung; báo riêng thành công, không có text,
  clipboard bận và lỗi ghi qua InfoBar. Không đồng nhất Sao chép text với
  chuyển clipboard giữ định dạng HTML được hỗ trợ.
- Có tùy chọn phím tắt chuyển nhanh và preview; phím hiện tại ngoài danh sách
  vẫn được đưa vào combo để giữ giá trị cũ. Hotkey cần engine chạy, UI-only
  không cài keyboard hook.
- Poll 500 ms khi trang được tải, tránh refresh lúc combo mở hoặc save lỗi;
  không tự đổi nội dung đang nhập thành kết quả mới khi thay tùy chọn.

## Checklist nghiệm thu còn mở

Ghi riêng kiến trúc, bản build/hash, OS/DPI, ngôn ngữ UI và kết quả từng ca.
Các ô dưới đây chưa được đánh dấu bởi fixture tự động.

- [ ] Legacy và Fluent cùng đọc đúng dữ liệu macro cũ, tên/nội dung tiếng Việt.
- [ ] Thêm, sửa, đổi tên, trùng tên, xóa/hủy; kiểm tra dữ liệu sau mở lại.
- [ ] Bản nháp: hủy chuyển chọn, tạo mới, tải lại; kiểm tra đóng/chuyển trang
  riêng, không giả định mọi đường đều có xác nhận.
- [ ] Nhập gộp/thay thế/hủy, BOM/CRLF, dòng lỗi/trùng; xuất/nhập lại giữ dữ liệu.
- [ ] File picker đường dẫn Unicode; hủy, ghi đè, quyền ghi lỗi; không mất dữ liệu.
- [ ] Tự viết hoa, bật/tắt macro, macro trong tiếng Anh; engine thực sự mở rộng
  từ thử trong editor ngoài app, không chỉ xác nhận list CRUD.
- [ ] Năm bảng mã: fixture văn bản chuẩn, đổi chỗ, bốn kiểu chữ và bỏ dấu;
  so sánh kết quả legacy/Fluent và glyph đúng font của bảng mã tương ứng.
- [ ] Chuyển/Sao chép/Xóa; nhập rỗng, text dài, nhiều dòng; cấu hình không bị reset.
- [ ] Clipboard text/HTML hỗ trợ, rỗng/bận/lỗi; xác nhận định dạng thật ở app đích.
- [ ] Hotkey và tray chuyển nhanh khi engine chạy; thông báo bật/tắt và tránh
  xung đột shortcut app khác. UI-only không được coi là đã pass hotkey.
- [ ] Đồng bộ tùy chọn giữa legacy/Fluent; thay đổi ngoài trang không mất bản nháp.
- [ ] Tab/focus, phím tắt, tiếng Việt/Anh, theme/High Contrast, DPI và cửa sổ nhỏ.

## Bằng chứng tự động và giới hạn

test-macro.ps1 chạy MacroServiceTests với registry giả và file fixture riêng.
test-convert.ps1 chạy engine/ConvertService và clipboard giả. Kiểm tra này
không thao tác registry/clipboard thật, không chứng minh file picker, focus,
hook/hotkey, tray hoặc clipboard liên ứng dụng đã hoạt động.
Kết quả chạy đối chiếu mới được ghi ở FLUENT_UI_QA.md.
