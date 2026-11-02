# Sửa dấu tại con trỏ

Nhánh phát triển: `feature/caret-tone-edit`.

## Sử dụng

Đang bật tiếng Việt, bảng mã Unicode dựng sẵn:

1. Nhấp chuột hoặc dùng phím mũi tên/Home/End/Page Up/Page Down để đặt con trỏ
   trong từ cần sửa, hoặc ngay đầu/cuối từ; không bôi đen văn bản.
2. Telex: `s/f/r/x/j` để thêm hoặc đổi dấu sắc/huyền/hỏi/ngã/nặng; `z` bỏ dấu
   thanh. `a/e/o` bật/tắt mũ trên a/e/o; `w` bật/tắt ă/ơ/ư (bao gồm cặp ươ);
   `d` bật/tắt đ. Chỉ cần nhấn một phím do ký tự gốc đã có trong từ cũ.
3. VNI: `1–5` thêm/đổi thanh, `0` bỏ thanh; `6` bật/tắt mũ trên nguyên âm
   a/e/o cuối phù hợp, `7` bật/tắt móc ơ/ư, `8` bật/tắt ă, `9` bật/tắt đ.
4. Nhấn lại cùng phím dấu để bỏ dấu đó. Giữ phím không sửa lặp lại.

Ví dụ câu `đây là nội dung băng tiếng Việt`: đặt con trỏ trong hoặc ngay sau
`băng`, nhấn `f` (Telex) hoặc `2` (VNI) để nhận `bằng`; `s` đổi thành `bắng`,
`z` trả về `băng`. Bỏ thanh vẫn giữ chữ `ă/â/ê/ô/ơ/ư/đ` và chữ hoa/thường.
Con trỏ giữ nguyên vị trí trong từ. Ctrl+Z sử dụng Undo của ô nhập.

Ví dụ `noi dung tiếng Việt`: đặt con trỏ ở trong hoặc ngay sau `noi`, gõ `oj`
để nhận `nội dung tiếng Việt` (`o`: `noi → nôi`, `j`: `nôi → nội`). VNI dùng
`65`. Có thể gõ tiếp `o`/`6` để bỏ mũ mà giữ dấu nặng: `nội → nọi`.
Nhấn `z`/`0` chỉ bỏ thanh (`nội → nôi`); muốn bỏ mũ nữa thì nhấn `o`/`6`.

Gõ ký tự thường sẽ kết thúc phiên sửa từ cũ; engine tiếp tục xử lý phím như
trước. Đường native không đọc lại ô nhập khi chỉ gõ văn bản bình thường.
Profile TSF có đọc ngữ cảnh giới hạn sau thông báo text/selection để cập nhật
khả năng sửa, nhưng chỉ sửa từ khi có thao tác di chuyển con trỏ thực tế.
Không lưu lịch sử văn bản. Chuyển sang chế độ
tiếng Anh hoặc bảng mã khác sẽ không kích hoạt sửa dấu tại con trỏ.

## Phạm vi hỗ trợ

- Ô Win32 `Edit` và `RichEdit`: đọc dòng tại con trỏ, tối đa 4096 ký tự; nhận
  từ tối đa 31 ký tự Unicode dựng sẵn. Chỉ thay đoạn ký tự bị ảnh hưởng và phục
  hồi vị trí con trỏ, không gán lại toàn bộ nội dung hay dùng clipboard.
- Ô **Nội dung nguồn** của trang **Chuyển mã** trong OpenKey WinUI: xử lý qua
  API TextBox của chính ứng dụng, thay phạm vi từ bằng SelectedText để giữ Undo.
- Các editor không có Win32 Edit/RichEdit có đường sửa qua TSF thử nghiệm,
  cần đăng ký DLL và chọn profile riêng. UI Automation chỉ đọc/chẩn đoán;
  Word/Chrome/VSCode chưa được nghiệm thu. Xem [thiết lập TSF](TEXT_SERVICE.md).
  Trên host Chromium, đặt con trỏ cuối từ; service chỉ sửa khi các ký tự cần
  đổi đều nằm trước con trỏ, mang theo phần đuôi không đổi để tránh mất chữ.
- Bỏ qua ô mật khẩu, chỉ đọc, disabled, vùng bôi đen, chuỗi quá dài, Unicode
  tổ hợp, identifier chứa số/underscore và từ không có cấu trúc âm tiết phù hợp.
  Win32 cũng bỏ qua từ chạm ranh giới xuống dòng mềm để tránh sửa một mảnh từ.
  Không xử lý IME Trung/Nhật đang mở hoặc composition trong ô WinUI.
- Hỗ trợ dấu thanh và dấu cấu tạo chữ trên từ cũ; không kiểm tra nghĩa hoặc
  tự động sửa chính tả. Khi đổi mũ/móc, giữ thanh và đặt lại vị trí phù hợp với
  tổ hợp nguyên âm cùng tùy chọn chính tả cũ/mới.

## Thiết kế và kiểm thử

- `Sources/OpenKey/engine/CaretToneEdit.h`: xử lý Unicode thuần, không sửa
  trạng thái engine và không giữ lịch sử gõ. Giữ vị trí thanh hiện có khi đổi
  dấu; khi thêm thanh dùng quy tắc đặt dấu cũ/mới theo thiết lập của OpenKey.
- `Sources/OpenKey/win32/OpenKey/OpenKey/CaretToneEdit.h`: đọc/ghi native control
  bằng SendMessageTimeout có giới hạn; xác minh lại focus, caret và dòng trước
  khi thay. Hook bỏ qua input tự sinh, giữ cặp keydown/keyup và chặn auto-repeat.
- WinUI chỉ gắn property lên cửa sổ của chính nó trong phiên sửa để hook cho
  phím dấu đi qua mà không tích lũy sai buffer. Không đặt property lên app khác.
- `Tests/CaretToneEditTests.cpp`: các phép đổi thanh, ranh giới từ, định tuyến
  phím và Edit/RichEdit ẩn do suite tự tạo; câu mẫu được thử ở mọi vị trí caret
  trong từ. Không thao tác trên tài liệu/cửa sổ người dùng và không cài hook.
- `Tests/EngineRegressionTests.cpp`: đối chiếu 640 trường hợp đặt dấu với engine
  gõ thực có sẵn, gồm hai cách đặt dấu và chữ hoa/thường.

```powershell
./scripts/test-caret-tone-edit.ps1 x64
./scripts/test-caret-tone-edit.ps1 x86
./test-all.ps1 All
```

Suite settings chạy kèm các ca caret tone trên mỗi kiến trúc. Kiểm thử fixture
không thay thế nghiệm thu nhập phím thật qua global hook hay thao tác WinUI.
Các message native riêng lẻ không tạo giao dịch nguyên tử với app đích; editor
thay đổi bất đồng bộ trong lúc thao tác vẫn cần nghiệm thu riêng trước phát hành.

Kết quả ngày 2026-10-30: 1150 ca sửa thanh/mũ/móc/đ trên mỗi kiến trúc x64/x86
đạt; gồm chuỗi `noi → nôi → nội` ở mọi vị trí con trỏ, giữ phím không sửa lặp,
thay cặp ươ và Undo. 640 ca thanh và 60 ca thêm mũ/móc/đ đối chiếu với engine
trên mỗi kiến trúc đạt. Bộ hồi quy đầy đủ đạt 20/20 tại
`dist/regression-ad9799aa26f643ada600873cd5dde905/summary.json`.
Engine Debug x64/Win32 và WinUI Debug x64 build thành công vào output
riêng trong `dist/caret-tone-*`. Chưa nghiệm thu thao tác phím thật trên WinUI.

Chạy bản debug mới (thoát OpenKey cũ trước nếu executable đang bị khóa):

```powershell
./scripts/run-debug.ps1 x64
./scripts/run-ui-debug.ps1 x64
```
