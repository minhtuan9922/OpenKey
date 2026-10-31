# Tích hợp TSF và UI Automation cho sửa từ tại con trỏ

Trạng thái: bản phát triển thử nghiệm trên `feature/caret-tone-edit`, đã có
trong build/ZIP/bộ cài beta local. Chưa phát hành stable. Cần bật profile TSF
để Windows nạp DLL vào editor; chỉ chạy lại engine chưa kích hoạt đường TSF.

## Cài và bật từ giao diện

Chạy bộ cài bình thường và chọn OpenKey Caret Editing. Setup chỉ xin UAC riêng
khi cần đăng ký thành phần; hủy yêu cầu này vẫn cài OpenKey. Sau đó mở
**Tổng quan → Sửa dấu tại con trỏ → Bật và sử dụng**. Nút này kiểm tra thành
phần, xin UAC nếu chưa cài, rồi bật profile cho tài khoản/phiên hiện tại.
Nút **Tắt** tắt profile và khôi phục layout trước đó nếu còn khả dụng.
Khi cập nhật DLL, đóng/mở lại editor để nạp phiên bản mới.

Build thông thường kèm cả DLL x64/x86 và helper theo kiến trúc ứng dụng.
Helper nhúng cả hai DLL, cài vào Program Files theo hash, kiểm tra kiến trúc,
nội dung và quyền ghi trước khi đăng ký. Gỡ cài chỉ bỏ đăng ký bản thuộc đúng
helper; thư mục DLL cũ giữ lại vì editor có thể đang nạp. Không thay đổi settings
OpenKey. Hủy quyền admin có thể bật lại sau từ giao diện. Luồng UAC và kích
hoạt trên máy sạch vẫn cần nghiệm thu, không suy ra từ kiểm thử tự động.

### Bản sửa kẹt phím — trace build 8

Service phân biệt lần nhấn mới với autorepeat bằng previous-state bit 30 của
thông báo phím. Lần nhấn mới xóa trạng thái chờ KeyUp cũ, tránh việc provider
bỏ sót nhả phím làm những lần sửa dấu tiếp theo bị nuốt mãi. Autorepeat của
phím đang giữ vẫn không áp dụng dấu thêm lần nữa. Fixture kiểm tra nhiều lần
liên tiếp ở cuối từ/EOF, cả đường TestKeyDown và đường KeyDown trực tiếp.

Sau khi cập nhật cần thoát hoàn toàn rồi mở lại editor để DLL mới được nạp.
Đổi layout hoặc tắt/bật profile có thể vẫn giữ DLL cũ trong process. Kiểm tra
ngày 2026-10-31 cho thấy Chrome còn nạp `dist/text-service-v2`, VSCode nạp
`dist/text-service-v7`, trong khi CLSID đã trỏ sang Program Files. Không kết
luận lỗi editor đã hết chỉ từ đăng ký mới hoặc fixture đạt; cần thử lại trên
process đã nạp bản mới, với `nam|` + `a` và `cộng|` + `z` → `công`.

## Cơ chế

- **TSF**: DLL COM `OpenKey.TextService.dll` triển khai
  `ITfTextInputProcessorEx`, key sink, document/thread focus sink và text edit
  sink. Sau thông báo text/selection, service đọc ngữ cảnh giới hạn để biết có
  từ phù hợp quanh con trỏ; thông báo này không tự bật chế độ sửa từ cũ.
  Trong callback phím, service xin read/write edit session đồng bộ, đọc tối đa
  33 ký tự mỗi bên, lập kế hoạch sửa rồi `ITfRange::SetText` đúng đoạn thay đổi.
  Khôi phục caret từ range mới, không gửi chuỗi bàn phím hay gán cả tài liệu.
  Trên host Chromium, sửa từ cũ dùng reconversion: mở composition trên range
  đã có trong một phiên ghi, trả khóa để frontend nhận range, rồi thay chữ và
  kết thúc composition trong phiên ghi tiếp theo. Có composition sink riêng
  để nhận hủy từ editor. Chỉ cho phép đọc composition đang sở hữu; không chạm
  composition khác. Cleanup trì hoãn (nếu cần) chỉ kết thúc composition, không
  thực hiện SetText trễ.
- Hook engine chỉ nhường các phím sửa dấu sau thao tác chuột/phím điều hướng
  thực tế và khi TSF báo có từ phù hợp. Gõ ký tự thường kết thúc ý định sửa từ.
  Quyền xử lý profile được giữ qua các thông báo text/selection và đồng bộ qua
  IPC v2, kể cả editor dùng renderer hoặc HWND ẩn. Window property chỉ phục vụ
  chẩn đoán, chỉ đặt trên cửa sổ thuộc chính process service.
  Gõ Telex/VNI vẫn do engine OpenKey tính toán. Riêng Word, các phím chữ/số,
  Space và Backspace đi qua hàng đợi IPC theo thứ tự; DLL lấy kết quả của engine
  trong callback phím và ghi trực tiếp TSF range dưới khóa read/write. Ký tự
  không cần chuyển đổi đi nguyên bản vào Word. Đường này không gửi Backspace,
  không dùng clipboard/paste để thay chữ. Các editor khác giữ đường gõ sẵn có.
  DLL không chứa một bản engine độc lập hay đọc lại toàn tài liệu.
  Service hỏi engine qua IPC chỉ đọc để lấy chế độ Việt/Anh theo ứng dụng,
  kiểu gõ và chính tả thực tế; khi engine đang chạy nhưng không phản hồi giao
  thức mới, service không sửa từ. Vì vậy cần chạy lại engine đã build mới.
- Nếu editor từ chối khóa đồng bộ, service không giữ phím để ghi trễ; phím đi
  tiếp vào editor. Bỏ qua read-only, selection, interim composition, password /
  numeric-password input scopes, secure activation và context keyboard-disabled.
- **UI Automation**: `AutomationReader` chạy trên MTA, đọc TextPattern của ô
  đang focus, kiểm tra password/read-only/foreground/provider ancestry và lấy
  ngữ cảnh giới hạn. Lớp này dùng để phát hiện khả năng cung cấp text và chẩn
  đoán, **không ghi văn bản**. UIA TextPattern không có SetText theo phạm vi;
  không dùng SetValue cả tài liệu hoặc clipboard làm đường sửa dự phòng.

Tài liệu nền: [TSF edit session](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontext-requesteditsession),
[đăng ký text service](https://learn.microsoft.com/en-us/windows/win32/tsf/text-service-registration),
[UI Automation text model](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-understandingtheuiautomationtextobjectmodel).

## Build và bật để thử

Chạy ở thư mục gốc repository:

```powershell
./scripts/build-text-service.ps1 All
```

DLL nằm tại `dist/text-service/x64/` và `dist/text-service/x86/`. Hai kiến trúc
đều cần để phục vụ editor x64 và Word/editor x86. Đăng ký COM/TSF là thao tác
machine-wide, nên mở **PowerShell bằng Run as administrator**, quay về root và
chạy:

```powershell
./scripts/register-text-service.ps1
```

Script đăng ký đúng regsvr32 cho từng kiến trúc, thêm profile Vietnamese
**OpenKey Caret Editing (experimental)** với layout nền US. Không tự kích hoạt
hay thay default input profile. Không di chuyển/xóa DLL sau khi đăng ký; nếu
đổi vị trí repository cần đăng ký lại đường dẫn mới.

Thoát OpenKey cũ, build/chạy engine mới và khởi động lại editor. Trong cửa sổ
editor, dùng **Win+Space** chọn profile OpenKey Caret Editing. Engine OpenKey
phải tiếp tục chạy, đang bật tiếng Việt và dùng bảng mã Unicode dựng sẵn.

```powershell
./scripts/run-debug.ps1 x64
```

Chọn profile TSF khác bằng Win+Space để ngừng đường sửa TSF. Gỡ đăng ký bằng
PowerShell Administrator:

```powershell
./scripts/register-text-service.ps1 -Unregister
```

Khởi động lại các editor đã nạp DLL trước khi build/xóa file đang bị khóa.
Script không tự nâng quyền, đổi default keyboard, đóng ứng dụng hoặc giết process.

## Cập nhật bản sửa lỗi gõ khi đang bật profile

Bản sửa tách thông báo cập nhật con trỏ của editor khỏi thao tác di chuyển con
trỏ của người dùng, tránh nhường phím giữa từ đang gõ. Khi provider chưa công bố
từ sẵn sàng, phím sửa dấu được thăm dò trên context của callback thực tế; không
phụ thuộc việc có nhận OnEndEdit trước đó hay không. Nếu không có từ phù hợp,
kết thúc ý định sửa và tiếp tục gõ bình thường. Word có đường ghi kết quả engine
trực tiếp vào TSF range để tránh lỗi `laapj → lâập` khi bật profile.

1. Chọn layout US bằng Win+Space, lưu tài liệu và đóng Word/VSCode/Chrome để
   gỡ DLL cũ khỏi bộ nhớ.
2. Build DLL vào thư mục mới để không ghi đè bản đã nạp:

   ```powershell
   ./scripts/build-text-service.ps1 All -OutputRoot ./dist/text-service-v7
   ```

3. Trong PowerShell **Administrator**, tại thư mục gốc repository:

   ```powershell
   ./scripts/register-text-service.ps1 -ComponentRoot ./dist/text-service-v7
   ```

4. Thoát OpenKey cũ, chạy `./scripts/run-debug.ps1 x64`, mở lại editor và
   chọn OpenKey Caret Editing bằng Win+Space.

Phải cập nhật cả engine và DLL để dùng đường ghi Word mới. DLL x64/x86
mới được build tại `dist/text-service-v7/`; không di chuyển thư mục này sau đăng ký.
Nếu gỡ bản đăng ký ở đường dẫn này, truyền cùng `-ComponentRoot` với `-Unregister`.

Ngày 2026-10-30, kiểm tra module đang nạp cho thấy VSCode vẫn dùng DLL ở
`dist/text-service/`, trong khi Word đã nạp `dist/text-service-v2/`. Đăng ký lại
không thay DLL đã nạp: cần thoát hết tiến trình editor rồi mở lại. Không tự đóng
ứng dụng hoặc tài liệu của người dùng.

Lần kiểm tra lỗi `cộng` sau đó xác nhận VSCode đã nạp đúng DLL v3. Bản v4 sửa
đường thay ký tự nằm trước con trỏ trên các host Chromium (Code/Code Insiders,
Chrome/Edge/Brave). Ví dụ caret sau `g`, thay `ộng → ông`, mang theo `ng` không
đổi để đoạn ghi kết thúc đúng tại caret; giữ cách ghi tối thiểu trên các host khác.
Đối chiếu [mã TSFTextStore của Chromium](https://chromium.googlesource.com/chromium/src/+/HEAD/ui/base/ime/win/tsf_text_store.cc),
đường commit existing text dựa vào vị trí caret đầu phiên ghi. Từ đó suy ra việc
chỉ thay `ộ → ô` khi caret còn ở cuối từ có thể làm mất phần đuôi trong frontend.
Fixture renderer mô phỏng hợp đồng này; chưa thay thế nghiệm thu VSCode thực tế.

Trong VSCode/Chrome, đặt caret **cuối từ** khi sửa dấu. Nếu ký tự cần đổi nằm
phía sau caret, service từ chối sửa để tránh ghi sai phạm vi; phím đi tiếp vào
editor. Hỗ trợ sửa về phía trước và phục hồi caret bất kỳ trên backend này vẫn
cần adapter composition/extension riêng. Không giả định đổi selection trong
cùng một phiên ghi sẽ cập nhật được caret frontend của IME.

Bản v5 bổ sung nhánh cuối text store: so sánh caret với `ITfContext::GetEnd`,
không kéo range bên phải vượt endpoint đã biết và không gọi GetText trên range
rỗng. Số ký tự đọc hai phía được khởi tạo riêng, không dùng lại bộ đếm/buffer
của phía trái. Bên phải rỗng là ngữ cảnh hợp lệ, không yêu cầu dấu chấm hoặc
khoảng trắng. Hỗ trợ thêm ranh giới dòng Unicode U+2028/U+2029.
Ca `nam|` + Telex `a` / VNI `6` phải thành `nâm|`, kể cả cuối dòng có dòng khác
bên dưới. Fixture thử cả provider chỉ cung cấp dòng hiện tại và provider từ
chối đọc range rỗng ở cuối. Đây là bản sửa nhánh đọc biên, chưa phải xác nhận
nguyên nhân runtime trên VSCode vì kết nối điều khiển ứng dụng không hoạt động.

Bản v7 được điều chỉnh sau trace `trace-20261030-225803.log`: sự kiện đầu ghi
`applied`, before=3, after=0, word=3, mode=2; các sự kiện sau là `edit-rules`
với before=3, after=2..5, word=5..8. Không có lỗi khóa hay đọc EOF trong trace.
Không thể suy ra ký tự cụ thể vì log chỉ có metadata, nhưng dấu hiệu phù hợp
với frontend không thực hiện đúng phép thay thế đã báo thành công ở text store.
Đối chiếu nhánh `StartCompositionOnExistingText` trong Chromium và
[ITfContextComposition::StartComposition](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontextcomposition-startcomposition),
bản này mở composition trên chữ cũ trước khi thay, thay vì commit SetText trực
tiếp. Đây là hướng sửa khâu áp dụng kết quả; chưa nghiệm thu VSCode thực tế.
Các callback chỉ có KeyDown cũng chuẩn bị context và đồng bộ ý định sửa, không
phụ thuộc việc editor gọi TestKeyDown trước đó.

## Chẩn đoán trên editor thật

### Trace callback khi cuối dòng vẫn lỗi

Người dùng xác nhận bản v5 vẫn lỗi `nam|` + `a` trong VSCode đã nạp đúng DLL.
Chưa xác định nguyên nhân runtime; các ca EOF fixture đạt không chứng minh lỗi
này đã được sửa. Bản `dist/text-service-diagnostic` thêm kênh trace để so sánh
hai ca lỗi/đạt, không thay đổi quy tắc sửa dấu và không đánh dấu lỗi là đã xong.

Lưu tài liệu, thoát hết editor. PowerShell Administrator tại repository:

```powershell
./scripts/register-text-service.ps1 -ComponentRoot 'D:\Project\OpenKey\dist\text-service-diagnostic'
```

Mở lại VSCode, chọn profile OpenKey Caret Editing. Chạy trong PowerShell thường:

```powershell
./scripts/probe-caret-context.ps1 -Trace
```

Trong 45 giây, quay sang VSCode, thử `nam|` + `a`, rồi thử `nam|.` + `a`.
Trace chỉ ghi callback test/key, bước bị từ chối, HRESULT, độ dài ngữ cảnh và
các cờ chế độ; không ghi phím, từ, nội dung tài liệu, tiêu đề hoặc đường dẫn.
Log nằm trong `dist/caret-context-probe/trace-*.log`.

Kênh chia sẻ bộ nhớ chỉ tồn tại khi collector chạy, tối đa 128 bản ghi đang chờ;
DLL không tạo kênh và không lưu trace ra đĩa. Collector không gọi UIA ở chế độ
`-Trace`, không đổi focus/selection hay gửi phím. `traceEvents=0` nghĩa là chưa
thu được callback từ DLL chẩn đoán, không được diễn giải là hết lỗi.

`intent-missing` là hook không báo ý định sửa; `read-lock`/`write-lock` kèm mã
lỗi là editor không cấp khóa; `selection`/`composition`/`input-scope` là context
bị từ chối; `word-boundary`/`edit-rules` là ngữ cảnh đọc được nhưng không lập được
phép sửa; `ready` mới là test-key đạt, `applied` mới là SetText trả thành công.
Trace build 7 thêm `composition-start`, `composition-denied`, `composition-end`;
cờ 64 chỉ đường reconversion, cờ 128 chỉ cleanup chưa hoàn tất đồng bộ.
Các lớp ký tự 0..8 lần lượt là rỗng/chữ/xuống dòng/khoảng trắng/số/dấu tổ hợp/
ký tự định dạng/dấu phân cách/khác. Không thể suy ra ký tự cụ thể từ trace.

### Probe chỉ đọc UIA

```powershell
./scripts/probe-caret-context.ps1
```

Khi có thông báo, trong 3 giây chuyển sang đúng editor và đặt caret trong từ
tiếng Việt. Công cụ chỉ đọc, không đổi focus hay nội dung. JSON trả về
`uiaReadable`, `stage`, `wordLength`, `runtimeProtocol`, `tsfActive`,
`tsfCaretIntent`, `tsfWordReadyProperty`; không in từ,
câu, đường dẫn tài liệu hoặc tiêu đề cửa sổ. `uiaReadable=false` không tự chứng
minh rằng TSF không hoạt động; đó là hai giao diện độc lập.
`tsfActive` và `tsfCaretIntent` lấy từ engine qua IPC v2; property có thể false
trên renderer dù profile vẫn hoạt động. `runtimeProtocol=false` nghĩa là probe
không nhận được phản hồi hợp lệ từ engine mới.

VSCode có nhiều backend editor. Với backend textarea, editor có thể không công
bố từ quanh caret khi accessibility support bị tắt. Nếu probe không đọc được,
có thể thử **Editor: Accessibility Support = on** trong Settings rồi kiểm tra
lại; không tự sửa cấu hình người dùng. Thiết lập này có thể ảnh hưởng folding /
minimap, xem [hướng dẫn VSCode](https://code.visualstudio.com/docs/configure/accessibility/accessibility).
Các backend không cung cấp surrounding text vẫn cần adapter riêng hoặc extension;
chưa coi là được hỗ trợ chỉ vì process có tên VSCode/Chrome/Word.

## Nghiệm thu

Mở tài liệu/ô nhập thử nghiệm mới trong từng app, chọn profile và thử:

- Gõ mới `laapj → lập`, `coongj → cộng`, `xax → xã`, `hoaf → hòa` (hoặc `hoà`
  khi dùng chính tả mới). Thử tiếp sau dấu cách và sau khi click vào ô trống.
- `noi dung tiếng Việt`: click đầu/giữa/cuối `noi`, gõ `oj` để nhận `nội`;
  `o` bỏ mũ giữ dấu nặng, `z` bỏ thanh. VNI dùng `65`, `6`, `0` tương ứng.
  Với host Chromium, thử ở cuối từ; các vị trí trước ký tự cần đổi phải không
  làm biến đổi văn bản qua TSF.
- `cộng`: đặt caret sau `g`, nhấn `z` (VNI `0`) phải thành `công`, còn đủ `ng`
  và caret ở cuối từ. Thử tương tự `công → cộng`, `băng → bằng`, `nội → nọi`.
- `nam`: đặt caret sau `m`, nhấn `a` (VNI `6`) phải thành `nâm` ở cuối tài liệu,
  cuối dòng có dòng trống/dòng văn bản bên dưới; đối chiếu có dấu chấm/khoảng trắng.
- `đây là nội dung băng tiếng Việt`: click `băng`, nhấn `f` để nhận `bằng`.
- Giữ phím không đổi dấu nhiều lần, chuỗi phím nhanh, Undo/Redo, selection,
  read-only, đổi focus, dấu câu, tiếng Anh, Unicode tổ hợp và composition khác.
- Chrome: textarea và contenteditable; Word: đoạn văn mới; VSCode: plain text
  editor, không dùng terminal hay ô tìm kiếm thay cho editor chính.

Trạng thái phải ghi riêng cho mỗi app/backend. Người dùng đã bật profile và
báo lỗi gõ trong Word/VSCode. Bản sửa mới chưa được nghiệm thu trực tiếp trong
Word/Chrome/VSCode sau bản sửa range typing; session hiện tại không có Administrator token để cập nhật
đăng ký DLL thay người dùng.

## Kiểm thử tự động

```powershell
./scripts/test-text-service.ps1 x64
./scripts/test-text-service.ps1 x86
./scripts/test-text-service.ps1 x64 -NativeContext
./scripts/test-text-service.ps1 x86 -NativeContext
./scripts/test-text-service.ps1 x64 -Interactive
./scripts/probe-caret-context.ps1 -BuildOnly
```

- Mặc định: COM context fixture kiểm tra lock, sửa theo range, caret, chuỗi
  phím, repeat/keyup, từ chối write session, password scopes và deactivation;
  LoadLibrary DLL thật kiểm tra exports, class factory và unload. Không cài profile,
  không ghi registry hay mở tài liệu người dùng.
  DLL kiểm thử nằm riêng trong `dist/text-service-tests-<arch>/component/`,
  không ghi đè DLL đã đăng ký.
- `-NativeContext`: thêm ô RichEdit do suite sở hữu, không giành foreground;
  kiểm tra range adapter trên text store TSF thật. Đây không phải profile đã
  được đăng ký hoặc nhập phím qua global hook.
  Các fixture native x64/x86 cần chạy lần lượt vì cùng sử dụng focus/TSF desktop.
- `-Interactive`: cần fixture đang foreground, kiểm tra thêm UIA đọc field và
  từ chối field bị mask. Lần thử trên máy hiện tại dừng ở foreground guard;
  không dùng mẹo vượt giới hạn focus và không tính là ca đạt.

Đọc thêm [hành vi sửa dấu](CARET_TONE_EDIT.md).

Kết quả ngày 2026-10-30: DLL x64/x86 và engine Debug x64/Win32 build đạt;
probe UIA compile đạt. 933 kiểm tra COM/DLL/IPC đạt mỗi kiến trúc, gồm provider không
gửi thông báo hoặc không nhận sink, đường ghi kết quả Word, từ chối khóa ghi và
password. 176 ca đối chiếu range typing với engine và 32 kiểm tra hàng đợi IPC
đạt mỗi kiến trúc. 949 kiểm tra có thêm text store RichEdit TSF thật đạt trên x64,
bao gồm phép thay `lâp → lập`, `cộng → công` giữ `ng` và con trỏ cuối từ.
Các ca renderer thử Telex/VNI ở mọi vị trí caret trên `cộng`, `công`, `băng`,
`nội`, `tuong`, giữ nguyên hai từ xung quanh và từ chối sửa về phía trước.
Ca biên gồm `nam` + Telex `a` / VNI `6` với cuối text store rỗng, provider giới
hạn theo dòng, LF/CRLF/U+2028/U+2029, có dấu chấm hoặc khoảng trắng phía sau.
Bộ hồi quy đạt 20/20:
`dist/regression-fbfc6595d040424eb02b662c8b3e2404/summary.json`.
Các kết quả này không bao gồm cập nhật profile, nghiệm thu Word/Chrome/VSCode
hay nhập phím thật qua global keyboard hook.

Bản chẩn đoán: 942 kiểm tra mỗi kiến trúc đạt, gồm phân biệt intent/khóa đọc,
metadata của test-key so với lần ghi, kênh tắt không nhận sự kiện, vòng đệm hữu
hạn và cùng layout x64/x86. Collector compile đạt; hồi quy 20/20:
`dist/regression-db4b684107a64a2d96543572971b196e/summary.json`.
Đây là kiểm thử công cụ chẩn đoán, không phải nghiệm thu sửa lỗi cuối dòng.

Bản v7: 1005 kiểm tra COM/DLL/IPC mặc định và 1024 kiểm tra có thêm text store
RichEdit TSF thật đạt trên từng kiến trúc x64/x86. Có negative control mô phỏng
dấu hiệu 3+0 → 3+2 sau SetText không composition; đường reconversion giữ `nam`
ở 3 ký tự qua hai lần sửa, thử cả mode 2 trong trace và ba kiểu gõ còn lại.
Các ca từ chối composition, từ chối khóa thứ hai, context thay đổi, owner hủy,
retry cleanup và composition có sẵn của bên khác đều đạt.
Hồi quy 20/20: `dist/regression-014e2369d40a4192b275125efc9d94fe/summary.json`.
Các kết quả này chưa thay thế việc xác nhận lại ca cuối dòng trên VSCode.

Tham khảo hợp đồng trả phím về editor khi `OnKeyDown` không tiêu thụ phím:
[ITfKeystrokeMgr::KeyDown](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfkeystrokemgr-keydown).
