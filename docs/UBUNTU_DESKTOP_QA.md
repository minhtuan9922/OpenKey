# Nghiệm thu desktop alpha.50 / UI preview.11

Ưu tiên Chrome omnibox và terminal VS Code. Bộ test headless hiện đạt nhưng
chưa chứng minh hai nơi này gõ đúng. Không cần phát hành alpha mới chỉ để chạy
quy trình này. [Cài bản thử nghiệm](UBUNTU_FEATURES_50.md), lưu công việc và
nạp addon mới trong phiên desktop trước khi thử.

## Chuẩn bị

1. Đóng/mở lại UI để dùng preview.11; xác nhận kiểu gõ và InputStyle đang dùng.
   Với yêu cầu không gạch chân/không ô nổi, thử NativeDirect trước. Ghi rõ nếu
   tự chọn chế độ khác để đối chứng; không trộn kết quả hai chế độ.
2. Bật Chẩn đoán Direct trong Cài đặt nâng cao, chỉ trong thời gian thử.
   Không gõ mật khẩu, URL riêng hoặc dữ liệu công việc trong ca thử.
3. Chọn một ứng dụng/ô nhập mỗi lần; ghi desktop GNOME/KDE, Wayland/X11 và
   backend ứng dụng nếu biết. Không suy ra backend chỉ từ loại phiên desktop.
4. Ghi phiên bản addon đã cài và việc đã nạp lại. Phiên bản gói cài trên đĩa
   không chứng minh module đang chạy là bản đó. Không restart Fcitx5 giữa lúc
   có bản nháp/composition đang nhập.

## Ca gõ cần ghi kết quả

| Nơi thử | Thao tác | Kết quả cần kiểm |
| --- | --- | --- |
| Chrome — ô nhập web trống | Telex `coongj hoaf xax hooij chur nghiax`, thêm Space | `cộng hoà xã hội chủ nghĩa ` với modern orthography bật; đủ từ, không lặp/mất chữ |
| Chrome — thanh địa chỉ | Cùng chuỗi, ban đầu không Enter; lặp với gợi ý autocomplete đang chọn | Giữ kết quả khi gợi ý xuất hiện; không tự mở URL/tìm kiếm trong lúc quan sát |
| Chrome — thanh địa chỉ | VNI `co6ng5 hoa2 xa4 ho6i5 chu3 nghia4`, thêm Space | Cùng câu; không kết luận Telex đạt thì VNI cũng đạt |
| Chrome — selection/focus | Backspace, Ctrl+A rồi gõ lại; chuyển sang ô web rồi quay lại | Không sửa vùng ngoài từ đang nhập; không giữ lịch sử từ ô trước |
| VS Code — editor mới | Telex `tieengs vieetj`; lặp sau Backspace và đổi focus | `tiếng việt`; ghi nhận gạch chân hoặc ô nổi nếu xuất hiện |
| VS Code — terminal Bash | Gõ `# ls git cd`, Enter; lặp vài lần rồi chuyển focus từ editor vào terminal | Dòng comment nguyên vẹn, đặc biệt từ đầu/phím đầu sau Enter và sau focus |
| VS Code — terminal Bash | Gõ `# tieengs vieetj`, Enter; lặp rồi thử `# abc xyz` | `# tiếng việt` và ASCII nguyên vẹn; không chạy chuỗi Telex như lệnh shell |
| GTK/Qt/LibreOffice | Câu Telex/VNI; Backspace, selection, focus và Undo/Redo | Ghi kết quả từng ứng dụng, không dùng kết quả Qt làm đại diện Chrome |

Ký tự `#` ở đầu dòng terminal dùng làm comment trong Bash; nếu terminal đang
chạy REPL/chương trình khác thì không Enter chuỗi thử. `abc def` không phải
đối chứng ASCII tốt khi bật Telex: `f` có thể là phím dấu huyền. Có thể chọn
English để đối chứng đường nhập ASCII riêng, ghi rõ lần chuyển đó.

Simple Telex 1/2 cần thử riêng sau các ca Telex/VNI: chuyển từ UI rồi từ menu
icon, đối chiếu tên và dấu chọn ở cả hai nơi; thử `w` để phân biệt Simple 1
với Simple 2. Kiểm tra lại sau đóng/mở UI và đăng nhập lại, không chỉ lúc vừa Apply.

## Tạo báo cáo chẩn đoán đã lọc

Công cụ Python 3 đi kèm mã nguồn repository; chưa nằm trong các gói alpha.50.
Chạy từ thư mục dự án, ngay sau từng ca (ví dụ Chrome thanh địa chỉ):

```bash
python3 scripts/linux-compat-report.py --since-minutes 5 --case chrome-url --output /tmp/openkey-chrome-url.json
```

Tên file phải chưa tồn tại. Nếu thử lại, dùng tên mới; công cụ không ghi đè.
Các nhãn khác: `chrome-web`, `vscode-editor`, `vscode-terminal`, `gtk`, `qt`.
Nhãn do người thử chọn, không phải ứng dụng được công cụ tự phát hiện. Cửa sổ
5 phút có thể chứa log của nhiều ca; nên thử riêng từng ca và đọc các nhóm
trong báo cáo. Số record là diagnostics được addon khử lặp, không phải số phím
hoặc số lần lỗi. Không có record không chứng minh diagnostics đang tắt.

Nếu stderr Fcitx5 nằm ở file thường thay vì user journal:

```bash
python3 scripts/linux-compat-report.py --journal-file /duong/dan/fcitx.log --case vscode-terminal --output /tmp/openkey-vscode-terminal.json
```

Thay đường dẫn nguồn bằng file log thực. Công cụ đọc text hoặc journal JSONL,
không nhận symlink/FIFO, không tự tạo daemon hay đổi cấu hình. Giới hạn đầu
vào 8 MiB, mỗi dòng 16 KiB, 256 nhóm; journal lấy tối đa 2.000 record khớp
trong cửa sổ 1–1.440 phút, timeout 15 giây. `input_limited`/`dropped_groups`
báo cắt dữ liệu; báo cáo không đủ để kết luận toàn bộ khoảng thời gian.

Báo cáo chỉ xuất enum, nhóm ứng dụng đã biết, capability và bộ đếm boolean.
Không chép log thô, tên host, đường dẫn, timestamp, program ID lạ, URL, chuỗi
phím hoặc surrounding text. File mới có quyền 0600. Lỗi journal/file chỉ hiện
thông báo cố định. Bản thân log gốc vẫn có thể chứa dữ liệu của thành phần
khác; gửi báo cáo đã lọc, không cần gửi nguyên user journal.

| Finding | Diễn giải |
| --- | --- |
| `no-diagnostics-in-window` | Chưa thấy record phù hợp; kiểm tra thời gian, bật diagnostics và nơi launcher thu stderr |
| `bridge-does-not-identify-application` | Fcitx thấy `gnome-shell`; không xác định riêng Chrome/VS Code |
| `surrounding-capability-missing-at-observation` | Capability thiếu tại thời điểm ghi; có thể đến chậm, không tự kết luận thiếu vĩnh viễn |
| `surrounding-synchronization-needs-review` | Có dấu hiệu chờ ACK/snapshot không khớp; chưa quy lỗi cho frontend cụ thể |
| `surrounding-capability-withdrawn-at-observation` | Capability bị rút tại bản ghi; chưa xác định lý do từ phía desktop |
| `direct-edit-aborted-observed` | Có nhánh hủy sửa; đọc cause cụ thể và flags trước khi trạng thái bị dọn |
| `context-reset-observed` | Có reset context; có thể là đổi focus hợp lệ, chưa chứng minh lỗi |
| `frontend-policy-restricts-direct-editing` | Chính sách frontend hạn chế sửa trực tiếp; không bỏ guard chỉ để nhận tiếng Việt |

Đường Chrome GTK/XWayland đã thử trước đây không cung cấp surrounding text
cho NativeDirect. Các giới hạn và bằng chứng nằm ở [NativeDirect](UBUNTU_NATIVE_DIRECT.md).
Không có finding hay record nào tự đánh dấu ca desktop đạt; `desktop_result`
luôn là `not-assessed`.

## Kết quả gửi lại

Mỗi ca gồm: bản addon/UI, nơi nhập, kiểu gõ/InputStyle, chuỗi phím đã gõ,
kết quả thực nhận, có autocomplete/selection hay không và báo cáo JSON đã lọc.
Chỉ dùng các mẫu không nhạy cảm ở trên. Nếu Undo/Redo hoặc focus gây lỗi,
ghi đúng thứ tự thao tác. Tắt Chẩn đoán Direct sau khi thu xong.

Sau khi các lỗi gõ được nghiệm thu, tiếp tục QA menu/tray, file chooser,
macro/tùy chọn mới, APT cài/nâng/gỡ và CI remote theo [checklist](UBUNTU_TASKS.md).

## Báo cáo phiên bản 2 — điều tra reset/context

Công cụ hiện xuất `version: 2`. Bổ sung đầy đủ các cause
`capability-withdrawn`, `key-snapshot-not-current`, `queue-overflow`,
`prepare-rejected`, ngoài `ack-timeout` và các cause đã có. Trước đây bốn
cause này bị gom thành `other`, làm mất thông tin cần cho điều tra VS Code.

`recent_records` giữ tối đa 32 record đã lọc theo thứ tự đầu vào;
`omitted_recent_records` cho biết số record trước đó không nằm trong danh sách.
Mỗi record có reason/cause/snapshot/reference và những boolean thực sự xuất
hiện trong log. Không chứa thời gian, nội dung, offset hoặc program ID lạ.
Giá trị `false` khác với trường vắng mặt. Reference vắng mặt là `unavailable`,
giá trị lạ là `other`; không tự suy ra đã có mốc so sánh.

Ví dụ chuỗi `direct:ready → context-invalidated → context-reset` cần đọc cause
và flags ở record `context-invalidated`, trước khi reset xóa mốc theo dõi.
`comparable=false` hoặc thiếu trường này không cho phép diễn giải
`text_changed=false` thành tài liệu không đổi. `notified=false` khi timeout
khác với đã nhận notification nhưng cache vẫn cũ.

Thứ tự này có thể xen kẽ nhiều context, ứng dụng hoặc lần chạy daemon;
`sequence_scope` luôn nhắc giới hạn đó. Không tính latency, không dựng trace
cho riêng VS Code, không quy reset cho notification đứng cạnh nó, và không
suy ra lỗi chỉ từ việc thấy reset. Không có ID context trong báo cáo.

Lần đọc journal 30 phút ở đợt tiếp tục này thấy ba record
`native:passthrough / awaiting-surrounding-capability`: một qua GNOME bridge,
hai qua XIM. Không có bằng chứng định danh riêng Chrome/VS Code trong mẫu
này; chưa dùng để thay timeout, bỏ guard hay xác nhận lỗi gõ đã được sửa.
