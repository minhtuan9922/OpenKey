# Điều tra Direct trên VS Code

## Ưu tiên hiện tại — mất từ đầu tiên trong terminal (2026-11-05)

Người dùng yêu cầu dừng hướng extension/helper, tập trung sửa addon. Giữ các
bản thử để tham khảo, không yêu cầu bật extension hoặc AT-SPI để khắc phục.
Ca đã báo: gõ `tieengs vieetj` trong terminal VS Code cho kết quả `' vieêt'`.
Người dùng xác nhận VS Code chạy **Wayland**: sau mỗi Enter chạy lệnh, từ
tiếp theo bị nuốt; Space mới cho gõ tiếp. Không chỉ xuất hiện khi focus ban đầu.
Đối chứng mới: `abc def` → `' deè'`. Từ ASCII đầu cũng bị mất, không chỉ khi
chuyển dấu. Phần `deè` là lỗi rewrite còn lại, không đồng nhất với mất từ đầu.

### Alpha.32 — candidate native prime sau Enter

Kết quả desktop người dùng: sau khi cài đúng alpha.32, `abc xyz` được giữ,
nhưng `ls` chỉ hiện `l`. Chưa đạt nghiệm thu terminal.

### Alpha.33 — giữ chuỗi nối thêm nguyên dạng đi native

Composition engine tự nó trả `ls` đúng; không phải phím dấu `s` bị engine bỏ.
Alpha.32 chỉ cho ký tự đầu đi native rồi chuyển ký tự sau sang commitString.
Candidate alpha.33 tiếp tục cho phím đi native nếu composition mới đúng bằng
từ đang giữ + ký tự vừa gõ, với surrounding đã xác nhận chính xác. Mỗi phím
native lại cập nhật expected snapshot. Không ACK thì giữ fallback raw; có
chuyển dấu thật thì rời chuỗi native và dùng rewrite cũ (hạn chế xterm còn đó).

Test mới `Enter → ls/git/cd`: mọi chữ unaccepted, không có commitString xen
vào, không lặp/mất ký tự; FAIL alpha.32 trước patch và PASS bản mới. Có test
Composition riêng cho các từ trên; Telex/VNI nhiều dòng cũ vẫn đạt. Build
alpha.33 qua 8/8 CTest và verifier gói/addon giải nén. Chờ người dùng desktop
thử lại; không tự cài hoặc restart daemon.

### Alpha.34 — mở rộng native start cho focus-entry

Người dùng báo alpha.33 vẫn mất cả từ đầu khi từ app khác hoặc editor focus
vào terminal VS Code; Space mới gõ được. Đã đối chiếu dpkg: alpha.33 đang cài.
Native start trước đó chỉ được arm bởi Enter, nên chưa bao phủ lượt focus mới.

Candidate alpha.34: mọi từ mới bắt đầu bằng ASCII nguyên dạng trong ibus Direct
đều dùng native start, không cần Enter. Chuỗi nối thêm nguyên dạng tiếp tục
native. Baseline surrounding chỉ để dự đoán; chỉ snapshot chính xác ở phím
tiếp mới nhận quyền sửa. GNOME có thể báo capability SurroundingText trễ;
không bỏ từ chỉ vì bit này trễ khi snapshot vẫn có thể xác nhận native text.
Snapshot không khớp/không có thì raw phần còn lại của từ. Biến đổi ngay phím
đầu (Telex w → ư) giữ nhánh cũ, không đổi sang raw vô điều kiện.

Test mới Telex/VNI: focus mới không Enter → `ls abc`, refocus → `git`, context
tái sử dụng đang giữ từ native của editor rồi chuyển sang prompt mới → `ls`;
không xóa từ cũ vào đích mới, không commit trùng khi focus-out. Test focus-entry
FAIL alpha.33 trước patch và PASS bản mới; 8/8 CTest đạt. Mô hình giả lập không
chứng minh browser thật đã sửa. Editor/chat ibus cũng chịu thay đổi; cần retest
chuyển dấu/gợi ý/Backspace, GNOME ACK trễ và chuyển focus. Lỗi rewrite xterm.js
(`deè`) và mọi tình huống đổi field giữa một từ đã chuyển dấu chưa được tuyên bố
đã giải quyết. Không triển khai lại extension hoặc nhận diện terminal heuristic.

### Alpha.34 — `pwd` chỉ hiện `p`

Dpkg xác nhận alpha.34. Người dùng gõ `pwd`, chỉ thấy `p`; chữ tiếp không hiện.
Khác `ls`: engine Telex đổi `pw` thành `pư` qua shortcut standalone W; `pwd`
được Composition render thành `pưd`, nên không còn là chuỗi nối thêm nguyên
dạng để alpha.34 cho đi native. Nhánh chuyển dấu/commit của terminal vẫn lỗi.
Kiểm tra chính tả đang bật không ngăn shortcut này; bảng engine còn cho phép
P làm phụ âm đầu, không được khẳng định đây là từ sai đã được checker nhận diện.

Không sửa riêng allowlist lệnh `pwd`, không tự ép English hoặc thêm heuristic
nhận diện terminal. Có thể thêm tùy chọn Telex không standalone W (engine đã
có biến thể SimpleTelex1), nhưng phải kiểm chứng đầy đủ và cần người dùng chọn:
nó đổi semantics `w` → `ư`, ảnh hưởng cả editor ibus, không tự giải quyết mọi
lệnh khác có phím dấu Telex. Chưa tạo alpha.35 hoặc sửa engine/setting cho ca này.

### Chi tiết candidate alpha.32 (lịch sử)

Đường ibus Direct kết thúc ownership khi Enter đi native, đánh dấu lượt tiếp
theo. Phím chữ đầu sau Enter đi nguyên bản/unaccepted, không commitString,
không accept+forwardKey. Surrounding phải khớp chính xác snapshot dự đoán trước
khi addon nhận quyền sửa chữ đó. Không ACK/capability thì cả phần còn lại của
từ đi raw đến boundary: ưu tiên không nuốt/xóa nhầm; có thể chưa chuyển dấu.
Focus/reset/đổi cấu hình/method/shortcut quên ownership này.

Không nhận diện terminal qua capabilities=82; đường này tác động cả editor ibus
sau Enter. Cần retest editor/chat, suggestions, Backspace và native Wayland.
Không xử lý toàn bộ hạn chế rewrite của xterm.js, không claim sửa `deè`.

Test mới yêu cầu phím đầu sau Enter unaccepted, không double-commit và kiểm
Telex/VNI nhiều dòng. Fault injection: IM commits bị bỏ sau Enter cho đến native
text key; không phải mô hình browser đầy đủ. Chạy binary test mới với **bản sao**
addon alpha.31 trong thư mục tạm thất bại tại assert native-prime; addon mới đạt.
Không dùng test này làm bằng chứng native đã PASS. Gói alpha.32 chờ người dùng
cài thử; không tự cài/restart Fcitx5 trong lượt phát triển.

Đã thêm ca headless Telex/VNI lặp ba chu kỳ `abc` → Enter → `tiếng việt` →
Enter, kiểm Enter unaccepted và không có client preedit. Ca này đạt trên
alpha.31 **trước khi sửa addon**: fake document không tái hiện được lỗi xterm.js,
không được dùng làm bằng chứng đã sửa native. Sau đó bổ sung native-prime như
candidate alpha.32 ở trên, không đổi kết luận về khả năng tái hiện browser thật.

Đã loại giả thuyết cần lọc preedit rỗng lặp lại trong addon: Fcitx5 5.1.19
đã thực hiện việc này trong
[InputContext::updatePreedit](https://github.com/fcitx/fcitx5/blob/5.1.19/src/lib/fcitx/inputcontext.cpp#L440).

Kiểm tra môi trường: daemon đang nạp addon hệ thống; dpkg báo alpha.31.
Diagnostics đã bật nhưng stdout/stderr daemon hiện ra terminal desktop;
journal chỉ có log của phiên cũ, không dùng làm bằng chứng cho lần tái hiện mới.
Không restart daemon hoặc sửa cấu hình người dùng trong lần kiểm tra này.

Trước khi sửa cần đối chứng, cùng một prompt mới: `abc def`, `tieengs vieetj`,
và lặp `tieengs vieetj` sau khi xóa dòng bằng Ctrl+U. Không nhấn Enter để tránh
chạy nội dung thử như lệnh shell. Ghi mất từ xảy ra chỉ khi vừa focus terminal
hay cả mỗi dòng mới, cùng backend Wayland/XWayland hiện dùng.

Direct ibus hiện dùng deleteSurroundingText + commitString. Đây là rủi ro đã
biết với xterm.js, nhưng chưa chứng minh nguyên nhân cụ thể của việc mất cả
từ đầu. Không thêm heuristic capabilities=82 để coi mọi editor là terminal;
không đánh dấu lỗi đã sửa chỉ dựa trên fake frontend bỏ lần xóa.

## Hướng cũ đã dừng — extension chuyển EN qua phím tắt

Addon alpha.31 vẫn có giới hạn terminal không được frontend phân biệt với
editor. Có [extension thử nghiệm](../tools/vscode-terminal-english/README.md)
riêng, opt-in: Ctrl+` từ editor chuyển terminal và deactivate IM; Ctrl+` từ
terminal quay editor và khôi phục OpenKey Telex/VNI trước đó khi còn phù hợp.
Không sửa thuật toán Direct hay bật chế độ commit-only bằng heuristic.

Extension chỉ chạy trên local UI host Linux có Fcitx5, không trên máy SSH
remote tùy ý. Không theo dõi chuột, không đọc terminal/editor/clipboard hoặc
chạy shell. Mỗi lời gọi dùng `/usr/bin/fcitx5-remote --check`, có timeout,
readback, hủy bước còn chờ khi mất focus và không retry tự động. Giữa lệnh
focus VS Code và context Fcitx5 có delay ngắn, chưa có ACK đồng bộ; cần test
desktop, không coi test giả lập là bằng chứng đã sửa terminal native.

Nguồn API: [when contexts](https://code.visualstudio.com/api/references/when-clause-contexts),
[activeTerminal](https://code.visualstudio.com/api/references/vscode-api#window.activeTerminal),
[local UI extensions](https://code.visualstudio.com/api/advanced-topics/remote-extensions).

## Kết quả đã quan sát — 2026-11-03

Môi trường: Ubuntu trong VMware, GNOME/Wayland, VS Code 1.144.1
(commit `261c84cb1eddf11235f9e2c921425e9663fbe556`), Fcitx5 5.1.19.

- Native Wayland qua bridge GNOME/IBus: Direct bắt đầu rồi reset lịch sử.
  Alpha.7 có `text_changed=1`, thường kèm `cursor_changed=1 anchor_changed=1`;
  có ca chỉ nội dung khác. Chưa xác định thành phần nào làm thay đổi snapshot.
- Cửa sổ XWayland riêng: người dùng gõ tiếng Việt được nhưng có gạch chân.
  Log xác nhận `app=gtk3-im:code frontend=ibus`,
  `fallback:no-surrounding-capability`, rồi `fallback:preedit-word-in-progress`.
  Cấu hình vẫn `InputStyle=Direct`. Đây là fallback Preedit, không phải Direct đạt.
- Không tăng timeout, không bỏ validation, không suy ra vùng tài liệu an toàn
  chỉ từ hậu tố giống nhau. Ở mốc điều tra backend dùng alpha.7; sau khi có ca
  sửa từ tái hiện được, alpha.8 được tạo riêng cho Backspace (xem cập nhật cuối).

## Đối chiếu mã nguồn

Các kết luận về mã nguồn không đồng nghĩa đã xác định đường thực sự được dùng
trong cửa sổ đang chạy:

1. [Fcitx5 IBus frontend 5.1.19](https://github.com/fcitx/fcitx5/blob/5.1.19/src/frontend/ibusfrontend/ibusfrontend.cpp)
   chuyển CommitText/DeleteSurroundingText qua D-Bus và nhận SetSurroundingText
   để cập nhật cache. Frontend này không tự bảo đảm cache phản ánh mô hình tài liệu
  của VS Code. `ibus` trong log không có nghĩa máy phải chạy daemon IBus riêng.
2. [VS Code editor options, đúng commit đang cài](https://github.com/microsoft/vscode/blob/261c84cb1eddf11235f9e2c921425e9663fbe556/src/vs/editor/common/config/editorOptions.ts)
   có `editor.editContext` mặc định true. [Cấu hình hiệu lực](https://github.com/microsoft/vscode/blob/261c84cb1eddf11235f9e2c921425e9663fbe556/src/vs/editor/browser/config/editorConfiguration.ts)
   còn phụ thuộc browser có API EditContext. Bật setting không tự chứng minh API
   thực sự được dùng.
3. [Textarea implementation](https://github.com/microsoft/vscode/blob/261c84cb1eddf11235f9e2c921425e9663fbe556/src/vs/editor/browser/controller/editContext/textArea/textAreaEditContext.ts)
   có thể trả `TextAreaState.EMPTY` khi accessibility support bị tắt trên Linux;
   khi hỗ trợ screen reader bật, dùng nội dung theo selection qua paged strategy.
   [Textarea input](https://github.com/microsoft/vscode/blob/261c84cb1eddf11235f9e2c921425e9663fbe556/src/vs/editor/browser/controller/editContext/textArea/textAreaEditContextInput.ts)
   ghi lại nội dung textarea khi focus/render theo điều kiện.

Giả thuyết cần kiểm chứng: snapshot mà IM nhận có thể thuộc buffer nhập của
editor, không phải vùng tài liệu ổn định. Đây là lý do thử EditContext/textarea
và accessibility trong profile tách biệt, không phải lý do bỏ kiểm tra Direct.
Không coi accessibility support là cách sửa đã xác nhận; nó có thể đổi hành vi UI.

## Chạy đối chứng, không đổi profile chính

Trong Terminal của desktop Ubuntu, tại repository (không chạy từ SSH):

```bash
bash scripts/test-vscode-input.sh textarea wayland
```

Trình chạy tạo profile mới bằng `mktemp`, cài settings thử nghiệm từ
[fixtures](../Tests/fixtures/vscode-input/), không nạp extension, không sửa profile
chính hay cấu hình Fcitx5. Chỉ đóng cửa sổ thử sau mỗi ca; terminal sẽ in thời gian
bắt đầu/kết thúc và vị trí profile. Giữ diagnostics bật theo
[hướng dẫn cài](UBUNTU_INSTALL.md). Không mở dữ liệu riêng tư hoặc đăng nhập Settings
Sync trong profile thử.

Trong file JavaScript mẫu, click giữa cặp dấu nháy rỗng, **gõ** `coongj hoaf`,
không dán. Ghi kết quả chuyển dấu và gạch chân. Thử popup code riêng ở dòng mới
với `print` chưa Space, hoặc `console.` khi chuyển English; gợi ý khi English
không chứng minh gợi ý hoạt động khi đang chuyển dấu.

Nếu đủ thời gian, chạy lần lượt, không chạy nhiều ca cùng lúc:

| Mode | EditContext yêu cầu | Accessibility support | Mục đích |
|---|---|---|---|
| `textarea` | false | off | Mốc textarea thông thường |
| `accessible` | false | on | Chỉ thay cách cung cấp nội dung cho accessibility |
| `editcontext` | true | off | So sánh đường nhập với mốc textarea; API có thể không khả dụng |

```bash
bash scripts/test-vscode-input.sh accessible wayland
bash scripts/test-vscode-input.sh editcontext wayland
```

Nếu cần đối chiếu backend, thay đối số cuối bằng `x11` và giữ cùng mode.
Không đổi `GTK_IM_MODULE` hay framework cùng lúc: phải giữ một biến thử mỗi lần.
`--user-data-dir` tách tiến trình theo [VS Code CLI](https://code.visualstudio.com/docs/configure/command-line);
backend được chọn bằng [Ozone runtime flag](https://chromium.googlesource.com/chromium/src/+/main/docs/ozone_overview.md).
Trình chạy kiểm tra biến display nhưng đó không phải phép xác nhận backend thực
tế; đối chiếu tiến trình và log như các lần trước.

## Tiêu chí và giới hạn

Ca đạt Direct phải đồng thời chuyển dấu được, không preedit/gạch chân,
không log fallback, không mất/lặp ký tự. Gợi ý, Tab/Enter nhận completion,
Backspace, selection, focus và undo/redo phải kiểm tra riêng trước khi công bố hỗ trợ.

Nếu mọi ca đều thiếu capability hoặc reset snapshot, không phát hành bản
"đã sửa VS Code". Cần xác định API/đường tích hợp khác hoặc công bố giới hạn.
Không tự chuyển sang clipboard/global hook hay extension có quyền sửa tài liệu
mà chưa trao đổi phạm vi và đánh giá rủi ro.

## Kết quả profile và sửa từ — cập nhật 2026-11-03

Người dùng đã thử hai profile Wayland với alpha.7:

- `textarea`: `coongj hoaf` giữ nguyên, không gạch chân, có gợi ý.
- `accessible`: `cộng hoà`, không gạch chân, có gợi ý. Gõ đúng một lượt ổn,
  Backspace sửa từ rồi đặt dấu tiếp bị lỗi, phải gõ lại từ đầu.

Log 20:47–20:48 có `prepare-rejected snapshot=current`. Test headless tái hiện
`coongg` → Backspace → `j`: alpha.7 từ chối edit quay về snapshot cũ rồi mất lịch
sử. Alpha.8 cho phép edit đó nhưng chỉ ACK khi có notification sau lúc bắt đầu
edit; đọc cache khớp không đủ. Test cả Telex/VNI, ACK chậm và selection thay đổi.
Không thay hành vi Backspace thành hoàn tác phím Telex. Cần thử alpha.8 desktop.

Giao thức snapshot không có revision: notification cũ trùng hoàn toàn đích mới
vẫn có thể bị nhầm. Không khẳng định đã xử lý hết race; timeout/context protection
và QA trên desktop vẫn cần thiết. Xem [hướng dẫn alpha.8](UBUNTU_INSTALL.md).

Cập nhật desktop: người dùng xác nhận hai ca Backspace alpha.8 đạt mong đợi,
sau đó xác nhận Tab/Enter, undo/redo, đổi focus và gõ nhanh đạt mong đợi trong
profile accessible. Giới hạn môi trường và giao thức ở trên vẫn còn.

Alpha.9 thêm xóa Space nhầm để sửa tiếp từ: giữ một Composition và tối đa 32
Space ASCII, phục hồi sau khi xóa hết Space nếu snapshot/hậu tố đầy đủ còn khớp.
Gõ từ mới, đổi context hoặc focus sẽ bỏ lịch sử. Không scan/reconstruct từ tùy ý.
Người dùng đã xác nhận các ca alpha.9 xóa Space rồi sửa tiếp từ đạt mong đợi trên
profile accessible, ngoài các test headless; không suy ra mọi app/backend đều đạt.

Trạng thái: đã đọc mã nguồn; Bash syntax, JSON và display/argument guards đạt;
sáu mode/backend qua CLI stub đạt (không mở GUI), 7/7 CTest hiện tại đạt.
Đã có phản hồi desktop cho textarea/accessible; profile editcontext chưa nghiệm thu.
Profile tạm được giữ lại để xem settings; không tự xóa hoặc đụng profile chính.

## Hồi quy trình chạy — cập nhật 2026-11-04

[Launcher tests](../Tests/LinuxVscodeLauncherTests.sh) được đưa vào CTest Linux
với tên `openkey_vscode_launcher_tests`, chạy cả khi UI/Fcitx5 build OFF. Test
dùng [launcher giả](../Tests/fixtures/vscode-input/code-stub.sh), display không
tồn tại và runtime/config riêng; không mở editor thật, không cần desktop và
không đọc/ghi clipboard hoặc gọi bộ gõ.

24 ca bao phủ:

- Sáu tổ hợp mode/backend, backend mặc định Wayland, profile mới khi chạy lại.
- Copy đúng settings/file mẫu, quyền profile 0700 và file 0600, thư mục extension
  riêng; các flag disable extensions/new-window/wait/goto được chuyển nguyên vẹn.
- Đường dẫn checkout, launcher và thư mục profile có khoảng trắng.
- Giữ profile sau khi launcher kết thúc và trả đúng exit code khác 0.
- Help, tham số sai/thừa/thiếu, thiếu display, launcher thiếu/không executable,
  TMPDIR không hợp lệ: không gọi stub và không tạo profile thêm.
- Sentinel profile chính trong fixture không bị thay đổi.

Chạy riêng từ root repository sau khi đã configure build:

```bash
ctest --test-dir build/ubuntu-ui -R '^openkey_vscode_launcher_tests$' --output-on-failure
# Hoặc chạy trực tiếp, không cần Qt/Fcitx5:
bash Tests/LinuxVscodeLauncherTests.sh "$PWD"
```

CI Linux hiện có bước chạy toàn bộ CTest nên tự bao gồm test này sau khi mã
được đưa lên remote; chưa chạy CI remote trong lượt này. Cần Bash và công cụ
GNU thông thường trên Ubuntu (coreutils, grep), không cần VS Code để chạy test.
Test tự xóa riêng cây fixture mới tạo; không dọn các profile thử desktop trước đó.

Trình chạy thật vẫn giữ profile để kiểm tra. Có thể đặt `TMPDIR` tới thư mục
tạm **tuyệt đối, tồn tại và ghi được**; mặc định `/tmp`. Thư mục/file mới dùng
umask 077. Không dùng TMPDIR tương đối hoặc đoán/export display để chạy thật.
Không thay launcher desktop hay profile VS Code chính.

Đây là nghiệm thu **trình chạy**, không chứng minh Telex/VNI, Direct, completion
hoặc một backend cụ thể hoạt động trên desktop. Kết quả accessible trước đó vẫn
giữ nguyên; profile `editcontext` chưa có nghiệm thu gõ. Không tạo addon alpha
mới, không tăng timeout hoặc nới guard Direct trong thay đổi này.
