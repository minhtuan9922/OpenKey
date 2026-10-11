# OpenKey Change Log

## Windows Fluent 3.0.5 — mã nguồn local, 2026-10-11

- Nâng đồng bộ version engine, updater, UI và bộ cài lên 3.0.5; metadata Windows
  `v3.0.5` / `327683`. Không thay phiên bản macOS.
- Bổ sung báo cáo chẩn đoán metadata cục bộ và mẫu báo lỗi/nghiệm thu beta.
- Kiểm tra notices trước đóng gói, guard bảo toàn dữ liệu bộ cài và cổng hồi quy
  PowerShell 7/5.1 trước tạo thư mục upload.

Đây là cập nhật mã nguồn, chưa build bộ cài 3.0.5 hoặc phát hành GitHub trong lượt này.

## Windows Fluent 3.0.3 — mã nguồn/bộ cài local, 2026-10-11

- Giao diện Fluent tiếng Việt mặc định, đổi Anh/Việt, theme Sáng/Tối/Theo hệ thống;
  menu tray tùy biến bo góc và mở các trang Fluent.
- Sửa chuyển Caps Lock giữa từ làm mất khả năng gõ dấu cho đến khi nhấn cách.
- Sửa menu tray bị bảng icon ẩn che và con trỏ giữ loading khi mở lần đầu.
- Khởi tạo owner của popup panel và tránh sao chép hàng khi đổi lựa chọn.
- Bộ cài offline chung x64/x86, có runtime, Start Menu và icon Desktop tùy chọn.
- Khi cập nhật, hỏi trước khi đóng engine/UI; chờ tối đa tổng 5 giây, không kill.
  Lỗi kiểm tra tiến trình hoặc PID cửa sổ thay đổi chặn cài đặt trước khi thay file.
- Thêm công cụ đo CPU/bộ nhớ thụ động và hướng dẫn hiệu năng; bộ hồi quy 19 suite.

Đây là ghi chú mã nguồn hiện tại, không khẳng định các chỉnh sửa local đã được
push/tag hoặc có trong asset GitHub đang phát hành. Bộ cài chưa ký số.
Luồng cài/gỡ/cập nhật/Sandbox/máy thật trước đó đã được người dùng xác nhận;
bộ cài vừa build chứa các sửa lỗi mới chưa được cài lại trong lượt đóng gói.

Các ghi chú macOS/Linux/legacy phía dưới được giữ nguyên từ dự án gốc.

##### OpenKey for Linux: (in development)

##### Version 1.2 RC5: (26/08/2019)
- Sửa lỗi không gõ được chữ "quởn".
- Không kiểm tra chính tả khi sử dụng dấu "[ ] { }".

##### Version 1.2 RC4: (24/08/2019)
- Bảng gõ tắt tiện lợi hơn, thêm tính năng Sửa từ.
- Cải thiện khả năng bỏ dấu, tốc độ nhanh hơn.
- Tự phục hồi dấu câu khi xóa ký tự (chữ “tuỳa” xóa “a” sẽ thành “tùy”,… )
- Sửa lỗi không gõ được từ “quét” khi bật chức năng tự phục hồi phím.
- Sửa lỗi ư và ơ khi gõ font Palatino trong MS Word.
- Sửa lỗi bảng mã VNI khi xóa ký tự, không thể gõ tiếng việt tiếp.

##### Version 1.2 RC3: (16/08/2019)
- Không gõ được "dui9, duoi96".
- Không gõ được "tuyps".

##### Version 1.2 RC2: (15/08/2019)
- Sửa lỗi không gõ được d i e u 9 6.
- Sửa lỗi không gõ tắt được khi dùng chế độ tiếng Anh với từ bắt đầu bằng.
- Sửa lỗi tự nhảy dấu khi gõ sai.
- Thêm thông tin phiên bản trong bảng giới thiệu.

##### Version 1.2 RC1: (13/08/2019)
- Chuyển chế độ thông minh: Bạn đang dùng chế độ gõ Tiếng Việt trên ứng dụng A, bạn chuyển qua ứng dụng B trước đó bạn dùng chế độ gõ Tiếng Anh, OpenKey sẽ tự động chuyển qua chế độ gõ Tiếng Anh cho bạn, khi bạn quay lại ứng dụng A, OpenKey tất nhiên sẽ chuyển lại chế độ gõ tiếng Việt, rất cơ động.
- Viết Hoa chữ cái đầu câu: Khi gõ văn bản dài, đôi khi bạn quên ghi hoa chữ cái đầu câu khi kết thúc một câu hoặc khi xuống hàng, tính năng này sẽ tự ghi hoa chữ cái đầu câu cho bạn, thật tuyệt vời.
Khôi phục phím với từ sai: hỗ trợ thêm các dấu ngắt câu như dấu chấm, phẩy,...
Sửa vài lỗi nho nhỏ khác.

##### Version 1.1 RC: (12/08/2019)
- Chế độ “Gửi từng phím”: OpenKey bản mới (1.1) mặc định dùng kỹ thuật mới gửi dữ liệu 1 lần thay vì gửi nhiều lần cho chuỗi ký tự, nên nếu có ứng dụng nào không tương thích, hãy bật tính năng này lên, mặc định thì nên tắt vì kỹ thuật mới sẽ chạy nhanh hơn.
- Phục hồi phím với từ sai.
- Nâng cao khả năng check chính tả.
- Sửa lỗi thanh địa chỉ trình duyệt (on/off).
- Bỏ tính năng "cho phép bỏ dấu tự do".
- Gõ tắt: bao gồm bật/tắt, bảng soạn các từ gõ tắt, hỗ trợ ký tự bất kỳ, bảng mã bất kỳ. Khi soạn thảo các từ gõ tắt, bạn phải nhập ở bảng mã Unicode dựng sẵn.
- Gõ tắt ngay khi trong chế độ gõ tiếng Anh (on/off).
- Sửa lỗi trên một số phần mềm.

##### Version 1.0.20: (06/08/2019)
- Sửa lỗi phím tắt chuyển chế độ, phím tắt của ứng dụng khác vẫn hoạt động nếu bị trùng.
- Cho phép gõ “Đ” ngay sau phụ âm.
- Không hiện Icon trên thanh Dock mục recent app.
- Sửa lỗi “oăc” ra “ooạc” trong kiểu gõ VNI.
- Sửa vài lỗi nhỏ xíu khác.

##### Version 1.0.19: (04/08/2019)
- Sửa lỗi không gõ được chữ “gì” khi dùng bỏ dấu kiểu cũ.
- Sửa lỗi không gõ được Unicode tổ hợp trên ứng dụng Stickies.
- Sửa lỗi gõ các âm "oong, ooc".

##### Version 1.0.18: (01/08/2019)
- Không sử dụng w -> ư trong Simple Telex.
- Bật tắt kêu beep khi chuyển chế độ.
- Thêm phím chuyển Shift, giờ có thể sử dụng Ctrl + Shift hoặc Command + Shift.
- Sửa vài lỗi khác.
- Hỗ trợ cho macOS bản cũ.

##### Version 1.0.17: (31/07/2019)
- Add Simple Telex mode.
- Black/White icon on menu bar.
- Space and back key improved.
- Modern orthography.
- Custom switch key.
- Quick telex (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Support TextWrangler.

##### Version 1.0.14: (09/04/2019)
- Add case "uýt".
- Improve typing English in Vietnamese mode.

##### Version 1.0.11: (27/02/2019)
- Add case "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Fix case "duocd".

##### Version 1.0.9: (22/02/2019)
- Fix incorrect word when switch language without pressing Space key.

##### Version 1.0.8: (19/02/2019)
- Switch key: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Fix case "duongd".
- Fix end consonant "t".

##### Version 1.0.6: (15/02/2019)
- Fix case "quatw".

##### Version 1.0.5: (13/02/2019)
- Spelling enhanced.
- Correct 1x menu icon.

##### Version 1.0.3: (11/02/2019)
- Fix auto correct on Chrome.

##### Version 1.0: (11/02/2019)
- First release.



# OpenKey lịch sử

##### Version 1.0.17: (31/07/2019)
- Thêm chế độ Simple Telex.
- Icon trắng đen trên menu bar.
- Lỡ bấm phím Space, xoá space vẫn có thể bỏ dấu.
- Bỏ dấu kiểu cũ/mới: òa, úy | oà, uý.
- Tuỳ chọn phím chuyển.
- Gõ nhanh (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Hỗ trợ TextWrangler.

##### Version 1.0.11: (27/02/2019)
- Thêm trường hợp "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Sửa lỗi "duocd".

##### Version 1.0.9: (22/02/2019)
- Sửa lỗi từ sai khi đổi chế độ mà không bấm phím Space.

##### Version 1.0.8: (19/02/2019)
- Phím chuyển: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Sửa lỗi "duongd".
- Sửa lỗi phụ âm cuối "t".

##### Version 1.0.6: (15/02/2019)
- Sửa lỗi "quatw".

##### Version 1.0.5: (13/02/2019)
- Nâng cao chính tả.
- Sửa icon cho màn hình non-retina.

##### Version 1.0.3: (11/02/2019)
- Sửa lỗi thanh địa chỉ trên Chrome.

##### Version 1.0: (11/02/2019)
- Phát hành lần đầu.
