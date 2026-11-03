# Điều tra Direct trên VS Code

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
