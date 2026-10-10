# Kiểm thử giao diện Fluent

## Beta và Sandbox mới sau About workflow — 2026-10-11

Release All x64/x86 không SkipBuild sạch; mỗi ZIP 281 payload/hash/PE đạt
verifier PS7/5.1, ba executable khớp Release output, WHATWG notice khớp
nguồn, resources UI 4/4. Tên/hash và hai .wsb mới ở BETA_PACKAGING.md.
Gói mới đã chứa lượt dọn helper/liên kết About/khóa workflow; các ghi chú
"ZIP cũ chưa chứa" bên dưới là trạng thái lịch sử tại từng bước.
27/27 fixture Sandbox đạt cả hai shell; chuẩn bị session có input riêng
read-only và logs riêng, không chia sẻ repo/user profile. Staged ZIP khớp
hash đã xác minh; config tắt mạng/clipboard host/mic/camera/printer.
Logs dist/about-workflow-beta-{x64,x86}.log và
dist/about-workflow-sandbox-tests*.log.

Không mở Sandbox hoặc chạy/cài app/runtime. Phiên mới không kèm VC++ runtime,
bootstrap sẽ dừng khi thiếu runtime; không coi chuẩn bị/harness là nghiệm thu
máy sạch. Giữ gói/session cũ, không đổi settings/clipboard host, không ký
hoặc phát hành stable. UI/dialog/browser thật và engine/hook trong guest
vẫn chờ kiểm thử. Source không thay sau hồi quy 14/14 nhóm ở bước workflow.

## Khóa toàn bộ workflow xác nhận cập nhật — 2026-10-10

About giữ active ticket đến hết tải metadata, ContentDialog và Launcher,
không Finish/bật lại nút ngay sau tải như bước trước. Spinner dừng sau tải;
nút Kiểm tra cập nhật vẫn khóa trong lúc xác nhận/mở Releases.
FinishUpdate tập trung nhả đúng ticket và khôi phục control nếu trang còn
loaded, bắt hresult_error khi window đang đóng. Hủy, lỗi show dialog, root
null, Launcher false/lỗi và request stale đều đi qua cleanup trên UI thread.
Nếu UI apartment không còn thì dừng không chạm XAML; không tuyên bố hủy
worker/native async hoặc khôi phục trang trên apartment đã đóng.

Sau ShowAsync/LaunchUriAsync trở lại UI apartment tường minh trước kiểm tra
ticket/cleanup/XAML. ContentDialog dùng ActualTheme của trang. Link mới vẫn
được quyền sở hữu InfoBar và vô hiệu hóa xác nhận cũ, nhưng không nhả khóa
workflow đang chờ. Không cho download thứ hai hoặc dialog cập nhật thứ hai
bắt đầu khi ticket cũ chưa được nhả.

Thêm 12 ca state cho giữ khóa xuyên confirmation/launch, retry sau Finish,
status mới khi đang busy và unload/reload trong workflow. Đây là fixture
state, không mô phỏng XAML/ContentDialog/Launcher thật hoặc chứng minh
thứ tự callback UI; QA hủy/lỗi/nhấn lặp/đóng window vẫn cần thực hiện.
ZIP beta cũ chưa chứa lượt sửa này và các lượt dọn helper/liên kết trước.
Logs dist/about-update-gate-regression.log,
dist/about-update-gate-regression-ps51.log,
dist/about-update-gate-analyze-ui-x64.log và
dist/about-update-gate-{Debug,Release}-{x64,x86}.log.
Hồi quy đạt 14/14 nhóm trên PowerShell 7 và 5.1, x64/x86; tổng shared
update/lifecycle 62 ca (50 trước + 12 mới). Báo cáo:
dist/regression-ff201580621a4acda4d329aad8474132/summary.json và
dist/regression-b7fa047a6a1441b5ba56a06588e83163/summary.json.
UI Debug x64 static analysis đạt 0 cảnh báo/0 lỗi.
Bốn build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi.

## Liên kết About và quyền sở hữu thông báo — 2026-10-10

OnLinkClicked dùng cùng epoch với kiểm tra cập nhật: thao tác mới nhất sở hữu
InfoBar. Kết quả Launcher cũ sau nhấn liên kết khác, bắt đầu kiểm tra cập nhật,
Unloaded/Loaded không ghi đè thông báo hiện tại. BeginStatus không nhả worker
đang chạy; Begin vẫn chặn tải metadata trùng, Finish vẫn thu dọn worker cũ.
Liên kết mới cũng vô hiệu hóa continuation xác nhận mở Releases của request
cập nhật cũ. Không hủy ContentDialog/Launcher/download đã bắt đầu.

Giữ strong lifetime, bắt hresult_error của sender/tag/URI/Launcher, trở về
UI apartment trước chạm XAML; apartment đã đóng thì dừng. Lỗi mở liên kết
luôn đặt InfoBarSeverity::Error, không giữ severity Success từ lần kiểm tra
cập nhật trước. Không thêm resource hoặc đổi địa chỉ liên kết.

Thêm 13 fixture state về latest-link-wins, link/update xen kẽ, busy download,
finish stale, unload/reload và dialog continuation. Fixture không chạy XAML,
ContentDialog hoặc mở browser thật; cần QA runtime sau. ZIP beta hiện có
chưa chứa lượt sửa liên kết này hoặc lượt dọn helper trước đó.
Logs dist/about-links-regression.log, dist/about-links-regression-ps51.log,
dist/about-links-analyze-ui-x64.log và dist/about-links-{Debug,Release}-{x64,x86}.log.
Hồi quy 14/14 nhóm đạt trên PowerShell 7 và 5.1, gồm x64/x86; tổng
shared update/lifecycle 50 ca (37 trước + 13 mới). Báo cáo:
dist/regression-d69e01c484cf45f886e0e4eb92481543/summary.json và
dist/regression-e4e8f027eecf4291ba022baf3994c165/summary.json.
UI Debug x64 static analysis đạt 0 cảnh báo/0 lỗi.
Bốn build UI Debug/Release x64/x86 cuối đều đạt 0 cảnh báo/0 lỗi.

## Dọn helper tải metadata cũ — 2026-10-10

Sau khi mọi caller metadata đã chuyển sang UpdateService, bỏ declaration và
implementation OpenKeyHelper::getContentOfUrl không còn caller. Bỏ include
Urlmon/fstream/sstream và pragma Urlmon.lib khỏi OpenKeyHelper.cpp; không
giữ đường tải unbounded/locale reader dùng chung _OpenKey.tempf.
URLDownloadToFileW của tải ZIP trong updater vẫn giữ nguyên, không phải
mọi đường tải mạng đều đã chuyển sang WinHTTP. Không xóa tệp temp của người dùng.
Gói beta vừa tạo ở bước trước chưa chứa lượt dọn mã này.
Lượt compile đầu phát hiện kiểu byte đến gián tiếp từ Urlmon; đổi cast ghi
REG_SZ sang kiểu Win32 BYTE (cùng dữ liệu/độ dài), không đổi hành vi startup.
Log hồi quy: dist/update-cleanup-regression.log và
dist/update-cleanup-regression-ps51.log. Log build cuối:
dist/update-cleanup-analyze-final-x64.log và
dist/update-cleanup-{Debug,Release}-{x64,x86}.log; log analysis ban đầu
dist/update-cleanup-analyze-x64.log giữ lại để ghi nhận lỗi compile đã sửa.
Hồi quy PS7 và PS5.1 đều đạt 14/14 nhóm:
dist/regression-246e4d3547744898b4cda040a241d062/summary.json và
dist/regression-c2910566547b42048b70fd8ce06d719d/summary.json.
Static analysis engine Debug x64 trên bản sửa BYTE đạt 0 cảnh báo/0 lỗi.
Bốn build All Debug/Release x64/x86 cuối đều đạt 0 cảnh báo/0 lỗi.
Sau build chỉ bỏ dòng trắng cuối OpenKeyHelper.cpp để diff --check sạch,
không thay token C++. Không mở app, tải mạng hay sửa registry/clipboard thật.

## Làm mới beta sau metadata dùng chung — 2026-10-10

Đã tạo ZIP x64/x86 từ Release All không SkipBuild, 0 cảnh báo/0 lỗi; tên và
SHA-256 ghi ở BETA_PACKAGING.md. Mỗi gói 281 payload/hash/PE đạt verifier
PowerShell 7 và 5.1; executable engine/updater/UI khớp output Release,
notice WHATWG khớp nguồn và UI resources đạt 4/4 mỗi kiến trúc.
Logs dist/shared-update-beta-{x64,x86}.log. Các ghi chú "ZIP cũ chưa chứa"
ở những bước bên dưới mô tả trạng thái tại thời điểm bước đó; gói mới này
đã chứa thay đổi parser/background/Fluent/metadata updater vừa hoàn tất.
Không chạy/cài app hoặc đổi Sandbox input, settings/clipboard; chưa nghiệm
thu UI/mạng/máy sạch/nâng cấp thật, chưa ký hoặc phát hành stable.

## Metadata updater độc lập dùng chung — 2026-10-10

OpenKeyUpdate tải metadata qua UpdateService/WinHTTP như engine và Fluent,
không còn tải version.json vào _OpenKey.tempf hoặc đọc bằng locale wifstream.
Giữ cả hai policy gate (thư mục module và thư mục cài đặt) trước tải metadata.
Lỗi transport/schema/exception dừng trước tải ZIP với exit code 1.
Schema yêu cầu versionCode hợp lệ và latestWinVersion ở root, không nhận
metadata chỉ có tên phiên bản. Parser chung từ chối tag "." và "..".

23 fixture metadata updater (trước đây 16) gồm thiếu/overflow versionCode,
root lồng, root trùng, trailing garbage, dot tags và versionCode 0 hợp lệ.
Adapter ReadUpdateVersion dùng parser chung; production gọi UpdateService.
Fixture không gọi UpdateThreadFunction, không tải mạng hoặc chạy updater thật.
Tải ZIP vẫn dùng URLDownloadToFileW; staging/PE/install không đổi, chưa xác minh
chữ ký archive/executable hoặc cập nhật toàn bundle Fluent. Policy vẫn chặn
updater legacy cho Fluent. ZIP beta cũ chưa chứa thay đổi này.

Log xác minh: dist/updater-shared-regression.log,
dist/updater-shared-regression-ps51.log, dist/updater-shared-analyze-x64.log
và dist/updater-shared-{Debug,Release}-{x64,x86}.log.

Kết quả: 14/14 nhóm trên PowerShell 7 và 5.1, gồm x64/x86; 23 metadata
fixture đều đạt. Báo cáo JSON:
dist/regression-6d2e5d9a14604bd09f37bca8c6f29f1e/summary.json và
dist/regression-7c83f18cd49f4b0e8800555a460820b2/summary.json.
Static analysis updater Debug x64 và bốn build All Debug/Release x64/x86
đạt 0 cảnh báo, 0 lỗi. Lần đầu Debug x64 bị LNK1168 do engine đang chạy;
sau khi người dùng thoát, build lại đạt. Không tự dừng process người dùng.

## Kiểm tra cập nhật dùng chung với Fluent — 2026-10-10

UpdateService kết hợp downloader và parser thành một Outcome cho cả
OpenKeyManager legacy và AboutPage Fluent. Bỏ parser find/stoul riêng của
Fluent, không dùng GetStringAsync unbounded: HTTP/status/redirect/size/timeouts
và schema validation đều đi qua service đã kiểm thử. Fluent vẫn xác nhận rồi
mở trang Releases bằng Launcher, không chạy updater legacy/thoát engine.
Không thêm chuỗi UI; giữ resources tiếng Việt/Anh và InfoBar lỗi/thành công.

Sửa OpenKeyVersionCode của Fluent 2.0.5 từ ordered 0x00020005 sang packed
0x00050002 (major ở byte thấp), để so sánh theo cùng major/minor/patch như
engine. 37 ca service/lifecycle mới kiểm tra 2.0.2/2.0.4/2.0.5/2.0.6/2.1.0/
3.0.0/1.255.255, lỗi transport tới Outcome, malformed metadata không thành
UpToDate, và Begin/Busy/Finish/Loaded/Unloaded/stale ticket.

Coroutine giữ strong lifetime, tải/parse sau resume_background, trở lại UI
qua apartment_context trước chạm XAML. Nếu UI apartment không còn, dừng
không cập nhật XAML trên worker. UpdateRequestState chặn yêu cầu trùng,
đánh dấu epoch khi load/unload/request; kết quả cũ sau rời trang/đóng window/
load lại không hiện InfoBar hay dialog. Job đang chạy không bị reset bởi
Unloaded; khi hoàn tất vẫn khôi phục nút/spinner cho trang đang loaded.
Kiểm tra ticket lần nữa sau ContentDialog và Launcher async, không mở trang
download nếu xác nhận thuộc request cũ. Bắt lỗi tạo/show dialog; root null
không mở dialog. OnLinkClicked của các liên kết thường không đổi ở bước này.

Test service dùng HTTP giả và state thuần C++; chưa chứng minh coroutine,
ContentDialog, proxy/TLS hoặc navigation/window close trên UI thật. Background
download không bị ngắt khi unload, vẫn tuân thủ limits đã mô tả ở phần trước.
Gói ZIP beta hiện có chưa chứa thay đổi này. Nguồn threading:
[C++/WinRT concurrency](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency-2).

Xác minh mã cuối: full regression 14/14 nhóm mỗi PS7/PS5.1, gồm 37 ca
shared service/lifecycle mới mỗi x64/x86 và toàn bộ fixtures cũ:
dist/regression-7e9370bddd8a4abe96b163e3a36a5fa3/summary.json,
dist/regression-f8297f2267b74e9184b9323900efa960/summary.json.
UI Debug x64 static analysis sạch (AboutPage mới có trong CL /analyze):
dist/shared-update-analyze-ui-x64.log. Bốn All Debug/Release x64/x86 đều
0 cảnh báo/0 lỗi, logs dist/shared-update-final-{Debug,Release}-{x64,x86}.log.
Không khởi chạy app/browser/updater, không tải metadata thật, không thay
settings/clipboard, không đóng gói/cài/ký/phát hành.

## Kiểm tra cập nhật legacy trên luồng nền — 2026-10-10

About/MainControl và startup chuyển sang beginUpdateCheck: một worker nền tải/
parse metadata, không giữ AppDelegate/dialog hoặc gọi UI. AsyncUpdateCheck giữ
shared state với release/acquire publication, nhận kết quả bằng polling trên
luồng UI. Chặn yêu cầu trùng; nút yêu cầu bị khóa trong lúc kiểm tra, bật lại
trước thông báo. Timer 100 ms chỉ tồn tại khi có job; thu dọn khi kết thúc/thoát.
Property token của cửa sổ giúp bỏ qua HWND đã đóng/tái sử dụng; closeDialog
gỡ property trước DestroyWindow, giữ job busy tới completion nhưng không báo
kết quả cho dialog đã đóng. Startup không báo lỗi mạng. Guard exiting tránh
free engine lặp và tránh mở updater sau khi đã thoát trong một modal loop.

UpdateDownloadService dùng WinHTTP HTTPS tới đúng host/path metadata, không
dùng URLDownloadToFile/getContentOfUrl hoặc shared temp file. Đặt timeout
DNS/connect/send/receive 3/5/5/5 giây, chỉ nhận HTTP 200, không theo redirect,
không bỏ xác minh TLS. Đọc buffer 8192, giới hạn body 65536 byte trước tăng
payload; từ chối empty/NUL/non-ASCII. Kiểm tra budget 20 giây giữa các thao tác;
đây không phải deadline cứng 20 giây (một thao tác/OS network có thể kết thúc
muộn hơn). Hủy UI/thoát chỉ bỏ theo dõi kết quả, không đồng bộ chờ worker hoặc
ngắt WinHTTP từ thread khác. Worker còn giữ shared state đến lúc trả về.

37 ca transport/background mới: lỗi từng bước/HTTP status, đóng handle đủ và
đúng thứ tự, partial-read failure, malformed read count, body empty/đúng ngưỡng/
vượt ngưỡng, NUL/non-ASCII, chunk 1/7/8192, budget giữa reads; busy/pending/
completion/consume-once/launch failure/exception/abandon/destruction. Một
worker detached thật được gate bằng promise để chứng minh caller vẫn poll
được và worker không cùng thread caller. HTTP hoàn toàn giả; không có network,
UI, registry/clipboard hoặc updater thật trong tests.

Chưa nghiệm thu responsiveness/nút/đóng dialog/thoát/timeout với mạng thật.
Fluent About có coroutine riêng, chưa đổi trong bước này. Downloader của
updater riêng vẫn là luồng cũ; ZIP beta hiện có chưa chứa thay đổi mới.
Nguồn API: [WinHTTP timeouts](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsettimeouts),
[WinHTTP reads](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpreaddata),
[timers](https://learn.microsoft.com/en-us/windows/win32/winmsg/using-timers).

Full regression mã cuối đạt 14/14 nhóm mỗi PS7/PS5.1, gồm 37 ca mới mỗi kiến
trúc và 32 parser/77 launch/16 updater metadata/10 archive/5 policy cũ:
dist/regression-cf078fdb3dab479d81ce266db053848e/summary.json,
dist/regression-379b61e8895d4a1baebe3197d4a8a983/summary.json.
Debug/Release All x64/x86 (engine/updater/UI) đều 0 cảnh báo/0 lỗi;
logs dist/background-update-{Debug,Release}-{x64,x86}.log. Static analysis
project engine Debug x64 sạch: dist/background-update-analyze-x64.log.
Không khởi chạy app, ký, cài, phát hành, hoặc thay settings/clipboard thật.

## Service kiểm tra metadata legacy — 2026-10-10

UpdateCheckService tách parser/so sánh phiên bản khỏi OpenKeyManager. Phân biệt
Available, UpToDate, DownloadFailed và InvalidMetadata; About/MainControl chỉ
báo phiên bản mới nhất khi metadata hợp lệ, lỗi kiểm tra báo thử lại. Startup
không hiện lỗi mạng và chỉ hỏi cập nhật khi Available. Không đổi engine/hook.

Parser giới hạn 65536 ký tự, độ sâu 16; chỉ chấp nhận schema release hiện tại
(object, chuỗi ASCII không escape, số nguyên không dấu). Yêu cầu đúng object
latestWinVersion cấp root, versionName ASCII an toàn tối đa 64 ký tự,
versionCode 24 bit; từ chối thiếu/trùng trường quan trọng, số âm/thập phân/
overflow/leading-zero, dữ liệu cắt cụt hoặc dư. Schema JSON khác sẽ báo lỗi
metadata, không suy ra máy đang dùng phiên bản mới nhất. Giữ thứ tự so sánh
major/minor/patch từ packed version legacy.

Giới hạn: transport vẫn dùng getContentOfUrl đồng bộ, temp file dùng chung và
chưa giới hạn download trước khi đọc; empty payload gộp với lỗi download.
Fluent About và updater riêng chưa chuyển sang parser này. Chưa nghiệm thu
dialog/network thật; không đánh dấu hoàn thành toàn bộ hạng mục updater.
ZIP beta ở BETA_PACKAGING.md được tạo trước thay đổi này, chưa chứa service mới.

Xác minh: 32 ca update-check metadata cuối + 77 launch + 16 updater metadata
+ 10 archive + 5 module-policy đạt mỗi x64/x86 trên PS7/PS5.1. Không dùng mạng,
không mở updater. Full regression trước sửa explicit ASCII cast và thêm 3 ca
boundary đạt 14/14 nhóm mỗi shell:
dist/regression-ebb6a887fe3d44ba9e2f088681ecaade/summary.json,
dist/regression-30b109bbad304401bbed6987f9f13cf5/summary.json.
Sau sửa cast, chạy lại toàn bộ updater tests ở process shell riêng; không có
cảnh báo compile. Lần chạy redirect trước có lỗi PowerShell runtime, không dùng
lần đó làm bằng chứng. Static analysis mã cuối sạch 0 cảnh báo/0 lỗi tại
dist/update-check-final-analyze-x64.log.

Bốn All Debug/Release x64/x86 trên mã cuối (engine/updater/UI) đều sạch
0 cảnh báo/0 lỗi. Logs dist/update-check-final-{Debug,Release}-{x64,x86}.log.
Không chạy ứng dụng, đổi settings/clipboard, đóng gói hoặc phát hành.

## Service mở updater legacy — 2026-10-10

UpdateLaunchService dùng chung cho AboutDialog, MainControlDialog và startup
AppDelegate. Policy Fluent được kiểm tra trước đọc module/launch; đường dẫn updater
và working directory lấy từ thư mục executable, không dùng CWD/MAX_PATH buffer.
Kiểm tra lỗi/truncation/NUL/đường dẫn tương đối trước gọi launch. Controllers chỉ
thoát engine khi ShellExecuteW trả INT_PTR >32; thất bại giữ engine và báo lỗi,
dialog vẫn bật lại nút. Chuỗi lỗi tiếng Việt dùng Unicode escape không phụ thuộc
code page nguồn. Nguồn: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew

77 injected launch cases mỗi x64/x86 đạt: 0..33 với policy cho/chặn, lỗi module,
NUL/tương đối/drive-relative/trailing separator/truncation, root drive,
UNC, đường dẫn dài và tiếng Việt/khoảng trắng, truyền đúng owner/path/directory.
Không khởi chạy process hoặc hiện dialog. 16 metadata/10 archive/5 module-policy
fixtures cũ vẫn đạt. Full regression 14/14 nhóm trên mã cuối đạt PS7/PS5.1:
dist/regression-b2dfd2c2168c4179be51117491cb1070/summary.json,
dist/regression-81a8ab90fa4d442489d59b9551d17c07/summary.json.

Bốn All Debug/Release x64/x86 (engine/updater/UI) sạch 0 cảnh báo/0 lỗi;
Debug x64 All có static analysis. Logs dist/update-launch-{debug,release}-{x64,x86}.log.

Beta Release All không SkipBuild x64/x86 sạch 0 cảnh báo/0 lỗi; 281 payload/
hash/PE đạt PS7/PS5.1, resources 4/4 mỗi kiến trúc và notice WHATWG khớp.
Tên/hash ở BETA_PACKAGING.md, logs dist/update-launch-beta-{x64,x86}.log.
Tài liệu trong gói snapshot trước khi biết tên/hash ZIP mới nên ghi rõ gói
read-failure cũ chưa chứa service launch; binary mới đã chứa mã cuối.
Không cài/chạy/ký/phát hành hay thay Sandbox input trước.

ShellExecute chấp nhận launch không chứng minh updater sống/chạy thành công.
Chưa thử nút/hủy/lỗi/thoát engine thực tế. Chỉ tách bước launch: checkUpdate
network/parser vẫn ở OpenKeyManager, có thể nhầm lỗi metadata/network là không có
phiên bản mới. Chưa hoàn thành tách toàn bộ update service; updater vẫn kênh stable
engine-only và bị chặn trong gói Fluent. Không tải/cài/phát hành update thật.

## Clipboard read failure không bỏ format — 2026-10-10

ConvertService phân biệt format vắng mặt, text rỗng và dữ liệu không đọc được.
Kiểm tra IsClipboardFormatAvailable trước GetClipboardData; format có sẵn nhưng
handle/GlobalSize/GlobalLock lỗi, allocation Unicode lẻ byte hoặc không có
terminator đều trả WriteFailed trước EmptyClipboard. Đăng ký HTML Format lỗi
cũng dừng vì chưa xác định được có rich text hay không. Unicode rỗng vẫn được
ghi lại khi chuyển HTML hợp lệ; HTML rỗng đi kèm Unicode có text bị từ chối,
không âm thầm bỏ format. Không có text hoặc chỉ Unicode rỗng vẫn NoText/no-op.
Nguồn API: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-isclipboardformatavailable
và https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboarddata

3.575 ca Clipboard fake mỗi x64/x86 đạt (thêm 32 fixture), gồm ma trận lỗi
trên cả Unicode/HTML, gọi qua Fluent và LegacyQuickConvert, đối chiếu map handle
và byte buffer nguyên vẹn, không allocation/clear và CloseClipboard đúng một lần.
175 pure-text/20 engine giữ nguyên; pure tests trap cả API kiểm tra availability.
Ba fixture lỗi đọc cũ chuyển từ kỳ vọng NoText sang WriteFailed. Harness không
dispatch delayed-rendering thật, không chạm Clipboard/settings hoặc mở ứng dụng.
SetClipboardData lỗi sau clear vẫn chưa có rollback; format ngoài Unicode/HTML
vẫn bị bỏ khi chuyển thành công theo chính sách hiện có, chưa bảo toàn mọi format.

Full regression 14/14 nhóm đạt PS7 và PS5.1:
dist/regression-a2743a5289174f429ea29a8602534479/summary.json,
dist/regression-71613971a0204164bb32e1b3a08aa220/summary.json.

Bốn build All Debug/Release x64/x86 (engine/updater/UI) sạch 0 cảnh báo/0 lỗi;
Debug x64 All có static analysis. Logs dist/clipboard-read-{debug,release}-{x64,x86}.log.

Beta Release All không SkipBuild x64/x86 sạch 0 cảnh báo/0 lỗi, 281 payload/
hash/PE đạt PS7/PS5.1. Resources 4/4 mỗi kiến trúc; notice WHATWG khớp nguồn.
Tên/hash ở BETA_PACKAGING.md, logs dist/clipboard-read-beta-{x64,x86}.log.
Tài liệu trong ZIP được snapshot trước khi biết tên/hash gói mới, ghi rõ gói
bridge legacy cũ chưa chứa read-failure fix; binary trong gói mới chứa mã cuối.
Không cài/chạy/ký/phát hành hoặc thay Sandbox input trước.

## Chuyển mã nhanh legacy dùng service chung — 2026-10-10

OpenKeyHelper::quickConvert (hotkey/tray và dialog Win32) chuyển sang
LegacyQuickConvert::convertClipboard với HWND của engine. Bridge snapshot
hai bảng mã và năm flag từ globals legacy, không ghi ngược cấu hình. UI Fluent
vẫn dùng GetActiveWindow của UI thread; bridge legacy không gọi GetActiveWindow.
Owner null/busy được từ chối, không mượn HWND của ứng dụng đang gõ.
Nguồn quy tắc owner: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard

3.543 ca Clipboard fake mỗi x64/x86 đạt, thêm 811 ca: 800 tổ hợp 5×5 bảng mã
và 32 flag (kể cả flag đồng thời), HTML/selection/thuộc tính và Unicode song song,
owner khi không có UI active, null/busy/no-text, lỗi cấp phát, HTML không hỗ trợ
và bảng mã sai. Kiểm tra globals giữ nguyên. 175 pure-text/20 engine giữ nguyên.
Ma trận bridge dùng public convert làm oracle mapping; 175 pure-text hiện có
đối chiếu riêng với legacy convertUtil và goldens. Chưa dispatch hotkey/tray thật.

HTML không còn đưa toàn bộ header/thẻ qua convertUtil: dùng parser/offset/entity
đã kiểm thử ở Fluent. HTML bảng mã legacy/raw-text không hỗ trợ bị từ chối trước
EmptyClipboard, không tự bỏ rich text; plain text vẫn hỗ trợ năm bảng mã.
Service giữ bản sửa ký tự cuối packed-output đã có ở Fluent. Không sửa engine
gõ/hook hay converter macOS. SetClipboardData lỗi sau clear chưa có rollback.
Không chạm Clipboard/settings thật hoặc chạy ứng dụng trong harness.

Mã bridge cuối sau khi tách globals khỏi service đã đạt 14/14 nhóm PS7:
dist/regression-a444bde8fe3548bbacd9ea557f75803e/summary.json.
14/14 nhóm PS5.1 trên cùng mã cuối cũng đạt:
dist/regression-afbc6373229c47178eddf1472f6e6c83/summary.json.
Debug x64 All/static analysis build sạch 0 cảnh báo/0 lỗi:
dist/quick-convert-final-debug-x64.log. Lượt build đầu chưa đạt do UI link globals
legacy và engine Debug bị process đang chạy giữ file; đã tách bridge và người
dùng xác nhận thoát process, không tự dừng engine/debugger.

Bốn build All (engine/updater/UI) Debug/Release x64/x86 cuối đều đạt
0 cảnh báo/0 lỗi. Debug x64 All chạy static analysis.
Logs: dist/quick-convert-final-{debug,release}-{x64,x86}.log.

Beta x64/x86 từ Release All không SkipBuild sạch 0 cảnh báo/0 lỗi;
281 payload/hash/PE đạt PS7/PS5.1, binary resources 4/4 mỗi kiến trúc,
notice WHATWG khớp nguồn. Tên/hash ở BETA_PACKAGING.md; logs
dist/quick-convert-beta-{x64,x86}.log. Gói snapshot tài liệu trước khi biết
tên/hash ZIP mới, nên BETA_PACKAGING.md bên trong ghi artifact literal là gói
trước bridge. Binary đã chứa bridge cuối. Không cài/chạy/ký/phát hành,
không thay Sandbox input cũ. QA hotkey/tray/paste HTML thật vẫn cần người dùng.

## CF_HTML literal reference recovery — 2026-10-10

Ampersand/tên không khớp và entity số không có digit được giữ như text literal,
chuyển casing rồi escape lại ampersand. Không giải mã đệ quy; longest-match
cho entity hợp lệ vẫn giữ nguyên. Selection được đặt trong chuỗi literal nhưng
vẫn bị từ chối khi cắt giữa entity đã nhận diện hoặc byte UTF-8.
Nguồn: https://html.spec.whatwg.org/multipage/parsing.html#ambiguous-ampersand-state

2.732 ca Clipboard fake mỗi kiến trúc đạt x64/x86, gồm goldens,
selection/caret hai loại context, casing xuyên node, escape roundtrip,
tên không khớp dài 10.000 ký tự và lỗi UTF-8/cấp phát trước EmptyClipboard.
175 pure-text/20 engine giữ nguyên. Full regression 14/14 nhóm đạt ở cả PS7
và PS5.1: dist/regression-b06cee00af1c4afd93d8cd39f34c278b/summary.json,
dist/regression-69f99366c2b9413cb6be2a3d57bbcf55/summary.json.
Không chạm Clipboard/settings thật hoặc mở UI.

Bốn UI Debug/Release x64/x86 build sạch 0 cảnh báo/0 lỗi; Debug x64 có
static analysis. Logs: dist/cfhtml-literal-{debug,release}-{x64,x86}.log.

Beta mới từ Release All không SkipBuild x64/x86 có 0 cảnh báo/0 lỗi,
281 payload/hash/PE đạt PS7/PS5.1. Binary resources 4/4 mỗi kiến trúc;
notice WHATWG khớp nguồn. Tên/hash ở BETA_PACKAGING.md, logs
dist/cfhtml-literal-beta-{x64,x86}.log. Không cài/chạy/ký/phát hành hoặc
đổi Sandbox input trước. BETA_PACKAGING.md trong gói ghi rõ artifact numeric cũ chưa
chứa literal recovery vì tài liệu được snapshot trước khi biết tên/hash ZIP mới.

Raw-text tags, HTML bảng mã legacy và layout/CSS chưa hỗ trợ; parser vẫn chỉ
xử lý tập con, không phải sanitizer. SetClipboardData lỗi sau clear vẫn chưa
có rollback. QA Clipboard thật/máy sạch và hotkey engine legacy còn riêng.

## CF_HTML numeric recovery — 2026-10-10

Entity số thập phân/hex tiêu thụ liên tiếp các chữ số hợp lệ, chấp nhận thiếu
dấu chấm phẩy và để nguyên suffix không thuộc entity. Giá trị tích lũy saturate
ở 0x110000, không tràn số nhưng vẫn tiêu thụ hết chuỗi digit. Null, surrogate
và ngoài Unicode được thay bằng U+FFFD. C1 dùng 27 mapping HTML/Windows-1252;
5 C1 không có mapping, control khác và noncharacter giữ scalar.
Nguồn: https://html.spec.whatwg.org/multipage/parsing.html#numeric-character-reference-end-state
Notice WHATWG đã bổ sung attribution cho bảng C1.

2.641 ca Clipboard fake mỗi kiến trúc đạt x64/x86; 175 pure-text và 20 engine giữ nguyên.
Thêm 204 ca và chuyển 15 fixture numeric cũ từ từ chối sang recovery:
goldens decimal/hex/thiếu dấu ;, 128 biến thể C1 đối chiếu Windows CP1252 độc lập,
control/noncharacter, 10.000 digit/leading zeroes, selection/full digit run,
attributes nguyên vẹn và stateful casing. Preview vẫn dùng toàn text node;
không cho selection cắt giữa entity số, dù prefix ngắn cũng là số hợp lệ.

Kỳ vọng decoder và casing được kiểm tra riêng: converter hiện có lowercase
khi không bật flag viết hoa. Integration so với public convert() văn bản thuần;
không sửa engine/converter hoặc thay semantics casing trong lượt này.

Ampersand/named reference không khớp và numeric không có digit vẫn bị từ chối;
raw-text tags, HTML bảng mã legacy và layout/CSS vẫn chưa hỗ trợ. Không phải
HTML parser tổng quát. Test fake không chạm Clipboard/settings thật hoặc chạy UI;
paste vào ứng dụng đích và máy sạch vẫn cần kiểm thử người dùng.

14/14 regression suites PowerShell 7 đạt:
dist/regression-8b228046d7b742bba8f5723c142af9a9/summary.json.
14/14 regression suites PowerShell 5.1 đạt:
dist/regression-f6c32ff9e3234abf9f8e81a355ebd295/summary.json.
Static analysis UI Debug x64 đạt 0 cảnh báo/0 lỗi:
dist/cfhtml-numeric-debug-x64.log.
Bốn UI Debug/Release x64/x86 đều build 0 cảnh báo/0 lỗi:
dist/cfhtml-numeric-{debug,release}-{x64,x86}.log.

Beta x64/x86 từ Release All không SkipBuild đạt 0 cảnh báo/0 lỗi.
281 payload/hash/PE đạt PS7/PS5.1, binary resources 4/4 mỗi kiến trúc,
notice WHATWG/C1 khớp nguồn. Đường dẫn/hash ở BETA_PACKAGING.md, logs
dist/cfhtml-numeric-beta-{x64,x86}.log. Không chạy/cài/ký/phát hành app,
không đổi settings/Clipboard thật; ZIP và Sandbox input trước được giữ nguyên.

## CF_HTML alias thiếu dấu chấm phẩy — 2026-10-10

Hỗ trợ 106 tên legacy trong text node, dựa trên longest-match của WHATWG:
https://html.spec.whatwg.org/multipage/parsing.html#named-character-reference-state
Danh sách đã đối chiếu với entities.json: 106/106 khớp; nhúng offline như bảng
2.125 tên đầy đủ. Không áp dụng decoder vào thuộc tính/comment/context.

Ví dụ text: &notin; giải mã thành ∉; &notit; giải mã tiền tố &not thành ¬it;.
Các tên không có alias hợp lệ (như &apos thiếu ;) vẫn bị từ chối. Unknown
ampersand/numeric recovery/raw-text/legacy code table vẫn chưa được hỗ trợ.
Đây không phải HTML parser đầy đủ và không đọc CSS/display.

Selection preview giải mã từ toàn bộ text node với giới hạn byte, không cắt
chuỗi trước lookup. Vị trí giữa entity, kể cả sau &not trong &notin;, bị từ chối
trước EmptyClipboard. Vị trí sau alias thực sự đã tiêu thụ được ánh xạ đúng.
Preview giữa CR/LF của một cặp CRLF cũng bị từ chối thay vì tạo offset khác output.
Preview không thay đổi ConversionState; text node thật vẫn commit một lần.

2.452 ca Clipboard fake mỗi kiến trúc, giữ 175 pure-text và 20 engine.
Thêm 160 ca: toàn bộ alias ở EOF/trước text, goldens longest-match/case,
thuộc tính giữ nguyên, selection context -1/cắt giữa reference và casing.
Mỗi alias còn kiểm tra theo sau bởi chữ ASCII; test không mở Clipboard thật.

14/14 regression suites PowerShell 7 đạt:
dist/regression-94091a2453844e07ba15e50d840659ab/summary.json.
14/14 regression suites PowerShell 5.1 đạt:
dist/regression-91430c9f454948f7890f5852ece3130b/summary.json.
Static analysis UI Debug x64 đạt 0 cảnh báo/0 lỗi:
dist/cfhtml-legacy-debug-x64.log.
Debug/Release UI x64/x86 đều build 0 cảnh báo/0 lỗi:
dist/cfhtml-legacy-{debug,release}-{x64,x86}.log.
Kiểm thử ứng dụng đích/Clipboard thật và máy sạch vẫn chờ QA người dùng.

Beta alias mới được tạo từ Release All không SkipBuild, 0 cảnh báo/0 lỗi.
281 payload/hash/PE đạt PS7/PS5.1, 4/4 binary resource checks mỗi kiến trúc,
notice WHATWG trong ZIP khớp nguồn. Đường dẫn/hash ở BETA_PACKAGING.md.
Logs dist/cfhtml-legacy-beta-{x64,x86}.log; không mở/cài/ký/phát hành app,
không đổi Clipboard/settings thật; đầu vào Sandbox và các gói cũ giữ nguyên.

## CF_HTML đầy đủ named entities có dấu chấm phẩy — 2026-10-10

Nhúng 2.125 mapping, gồm 93 tên hai Unicode scalar; tra cứu ordinal phân biệt
hoa/thường. Không tải mạng lúc chạy app. Nguồn và thay đổi dữ liệu được ghi trong
HTML_ENTITIES_NOTICE.md (BSD source-code incorporation); package-beta sao chép
notice vào ZIP. Không thay engine/keyboard hook/settings hoặc casing legacy.

Đối chiếu độc lập từng mapping header với JSON WHATWG tải lại: 2.125/2.125 khớp.
SHA-256 header tại thời điểm đối chiếu:
8A8F55FB4C389FA31BE95D9FF2AA5B0432267C1A40321C9F22ED29D15CA596A3.
Đây là fingerprint dữ liệu/source, không phải chứng minh nguồn gốc executable.

2.292/2.292 ca Clipboard mỗi kiến trúc đạt; 175 pure-text và 20 engine giữ nguyên.
Test toàn bảng so UTF-8 bằng Windows encoder từ scalar trong bảng; các goldens
riêng kiểm tra mapping đại diện, hai scalar, surrogate pair, tên dài, case/prefix,
không double-decode, attribute nguyên vẹn, byte offset selection và stateful casing.
Test toàn bảng không phải corpus mapping độc lập; đối chiếu JSON là bước read-only
ở trên. Các regression sau dùng bảng đã pin, không cần mạng.

14/14 regression suites PowerShell 7 đạt:
dist/regression-506ff8b5989542f4b28df098bb1dd080/summary.json.
14/14 suites PowerShell 5.1 đạt:
dist/regression-bcb130c1a49f4d80a841865ebe803fd3/summary.json.
Bốn UI Debug/Release x64/x86 build 0 cảnh báo/0 lỗi:
dist/cfhtml-named-{debug,release}-{x64,x86}.log.

Vẫn từ chối tên không biết/thiếu dấu chấm phẩy, numeric parse-error recovery,
raw-text tags và HTML bảng mã legacy. Chưa có DOM/CSS layout hay HTML parser đầy đủ.
Clipboard thật/paste vào ứng dụng đích, UI và máy sạch vẫn cần kiểm thử thủ công;
test fake không mở Clipboard hay chạy app.

Gói mới từ Release All sạch (không SkipBuild), 281 payload mỗi kiến trúc,
thêm HTML_ENTITIES_NOTICE.md; verifier PS7/PS5.1 đạt, notice khớp nguồn,
binary resources 4/4 mỗi gói.
Đường dẫn/hash được cập nhật ở BETA_PACKAGING.md; Sandbox input cũ giữ nguyên.

## CF_HTML casing xuyên text node — 2026-10-10

Tách ConversionState nội bộ khỏi vòng lặp convertText; convert() văn bản thuần
vẫn tạo trạng thái mới mỗi lần gọi. Callback HTML giữ state theo giá trị qua các
node; thẻ inline/comment không khởi động lại casing. Selection preview sao chép
callback từ trạng thái trước node, chuyển prefix mà không commit; text node thật
chỉ commit một lần sau preview. Không đổi API hoặc logic converter/engine legacy.

br/hr/p/div/list/heading/table và danh sách block tag tường minh dùng newline logic
để bắt đầu đoạn/dòng, không chèn ký tự vào output HTML. Đây là quy tắc app cho tập
con HTML: không đọc CSS/display, không dựng DOM hoặc mô phỏng layout trình duyệt.
Raw-text và bảng mã HTML khác Unicode vẫn từ chối trước EmptyClipboard.

137/137 Clipboard fake đạt x64/x86; giữ 175/175 pure-text và 20/20 engine.
Thêm 29 ca casing (thay ba ca từ chối cũ bằng hai ca bảng mã): inline/split-word,
comment, punctuation qua node, entity, attribute, br/paragraph/literal newline,
context -1, selection preview/giữa từ, nhiều lần gọi và fragment rỗng.
Giữ semantics casing/bỏ dấu hiện tại của converter; không thêm Unicode normalization.

Bốn build UI Debug/Release x64/x86 đạt 0 cảnh báo/0 lỗi; log
dist/cfhtml-casing-{debug,release}-{x64,x86}.log. Chưa thực thi UI/Clipboard thật;
HTML paste lên ứng dụng đích và layout/CSS vẫn cần QA, SetClipboardData sau clear
vẫn không có rollback.

Full regression 14/14 nhóm đạt PS7/PS5.1; 137/137 Clipboard mỗi kiến trúc trên
mã cuối. Reports: dist/regression-9ecaa7dad54949df912c75d347ca8165/summary.json
(PS7), dist/regression-78031df80e434e2f84208a19335bdb99/summary.json (PS5.1).

Đóng gói từ Release All không SkipBuild, 0 cảnh báo/0 lỗi. Mỗi ZIP 280 payload,
hash/PE đạt PS7/PS5.1, 4/4 kiểm tra binary resource, metadata/notices 9 package:

- x64: dist/OpenKey-Fluent-beta-x64-79985bd4380140acb1c1a38f553e3a4e.zip,
  SHA-256 2715853A6B1A944A53399873F6F9528866F237EF79B877A49501DD3850A3D670.
- x86: dist/OpenKey-Fluent-beta-x86-672ccba57ca046c4bd79c45b75232213.zip,
  SHA-256 6F34DDFEDC425816B9E3D6DFE3F8DF8936D549917D8C2B6E4DF6696A1A237A26.

Logs dist/cfhtml-casing-beta-x64.log và x86.log. Không chạy/cài/ký/phát hành app;
UI/Clipboard thật, runtime máy sạch và giấy phép vẫn cần nghiệm thu.

## CF_HTML character references — 2026-10-10

Giải mã entity ở text node trước chuyển chữ/bỏ dấu, sau đó escape &/</> để
ký tự được giải mã không trở thành markup. Hỗ trợ decimal/hex có dấu chấm phẩy,
amp/lt/gt/quot/apos/nbsp và AMP/LT/GT/QUOT; thuộc tính/comment giữ nguyên.
Không giải mã lặp lại: &#38;lt; vẫn là chữ &lt; sau một lượt decoding, không thành thẻ.

Chuẩn hóa CR/CRLF literal thành LF và re-escape CR từ numeric reference thành
&#13; để giữ khác biệt khi HTML được parse. Quy tắc tham khảo:
https://html.spec.whatwg.org/multipage/syntax.html#character-references và
https://html.spec.whatwg.org/multipage/parsing.html#preprocessing-the-input-stream.

Selection được tính theo output sau decoding/conversion/escaping. Vị trí giữa
entity hoặc UTF-8, thiếu semicolon, tên chưa hỗ trợ, numeric overflow/surrogate/
out-of-range/zero/control/noncharacter bị từ chối trước EmptyClipboard, giữ bản gốc.
Đây là strict subset: không giả lập HTML error recovery/Windows-1252 replacement,
không hỗ trợ mọi named entity, stateful casing, raw-text hoặc HTML legacy encoding.
Không phải parser/sanitizer HTML đầy đủ; SetClipboardData sau clear vẫn chưa rollback.

Thêm 53 ca Clipboard fake (tổng 109) gồm named/numeric, tiếng Việt/emoji, casing/
bỏ dấu, escaping và không double-decode, giữ attribute, xuống dòng và selection.
Bốn build UI Debug/Release x64/x86 đạt 0 cảnh báo/0 lỗi trên mã cuối; log
dist/cfhtml-entities-{debug,release}-{x64,x86}.log. Không chạy UI hoặc Clipboard thật.

109/109 Clipboard, 175/175 pure text và 20/20 engine đạt x64/x86 trên PS7/PS5.1.
Full regression 14/14 nhóm đạt với mã cuối; báo cáo PS7:
dist/regression-adf37eeda4bd404685cf6fdeec0a3c6f/summary.json; PS5.1:
dist/regression-03f5e11755df4791892dd7d73a5a118e/summary.json.
Report PS7 41dfc3bf01f24d26b466939f3295a5f3 trước đó chưa chứa ba ca CR/CRLF cuối.

Đã đóng gói lại Release All không SkipBuild: 0 cảnh báo/0 lỗi,
280 payload/hash/PE đạt PS7/PS5.1 mỗi ZIP, 9 package notices, binary resources 4/4.

- x64: dist/OpenKey-Fluent-beta-x64-61f3b13de3d44549ae88eb628f88359c.zip,
  SHA-256 9C0C63A949206A6DDE93C3B16D132F4F4FF12CBE4A0488DD5D20B8BAFED26576.
- x86: dist/OpenKey-Fluent-beta-x86-8e34bb95bdd4429bb064ebfe6d7a3c47.zip,
  SHA-256 C0572423C01DFABA18DB7C07183F093D0556CCB8E66C99EFC640E7A984332845.

Không chạy/cài app; không ký/phát hành. Logs dist/cfhtml-entities-beta-x64.log
và x86.log. UI/Clipboard HTML thật, runtime máy sạch và giấy phép chưa nghiệm thu.
Gói cũ và đầu vào Sandbox cũ giữ nguyên, không tự cập nhật.

## CF_HTML context và selection — 2026-10-10

Mở rộng HtmlClipboard.h: chấp nhận StartHTML/EndHTML cùng -1 với cặp marker chuẩn
bao quanh fragment; giữ nguyên hai sentinel khi ghi lại. Chấp nhận cặp
StartSelection/EndSelection trong fragment, ánh xạ offset byte qua text node và
markup/comment giữ nguyên. Với casing stateless đang hỗ trợ, chuyển tối đa hai
prefix text để tính vị trí mới, không dựng bảng ánh xạ O(n²).

Selection thiếu cặp/trùng trường, âm/overflow/đảo/ngoài fragment hoặc cắt giữa
ký tự UTF-8 bị từ chối trước EmptyClipboard. Hỗ trợ caret, fragment rỗng,
selection trong text và xuyên nhiều thẻ, cả khi bỏ dấu làm giảm số byte.
Context -1 chưa hỗ trợ các biến thể thiếu marker chuẩn hoặc dữ liệu dư sau marker.
Entity, raw-text/script/style, bảng mã legacy và sentence/title casing vẫn bị từ
chối; đây không phải parser/sanitizer HTML đầy đủ. Hotkey engine legacy chưa đổi.

56/56 ca Clipboard fake đạt x64/x86, thêm 21 ca context/selection vào 35 ca trước.
175/175 pure text và 20/20 engine vẫn đạt trên cả hai kiến trúc.
Bốn build UI Debug/Release x64/x86 đạt 0 cảnh báo/0 lỗi, log
dist/cfhtml-selection-{debug,release}-{x64,x86}.log. Không chạy app hoặc chạm
Clipboard thật; ZIP beta đã cập nhật như ghi bên dưới. SetClipboardData lỗi sau clear vẫn chưa
có rollback. Kiểm thử paste HTML trên ứng dụng đích vẫn cần người dùng nghiệm thu.

Full regression 14/14 nhóm đạt trên PS7 và PS5.1 với mã context/selection cuối;
báo cáo tương ứng dist/regression-d46418944d5f4996abba33ffe5823c22/summary.json
và dist/regression-c387cbcb33a447acb6cd5b315ed26944/summary.json.

Tạo lại beta từ Release All (không SkipBuild), 0 cảnh báo/0 lỗi:

- x64: dist/OpenKey-Fluent-beta-x64-c0863f5ec0574c39b921d7a809048d43.zip,
  SHA-256 1DF8E1B0DF092C6F6C39A7CC52256C8A8673FEE4309AEB34FD6075F76A63989A.
- x86: dist/OpenKey-Fluent-beta-x86-39116bfdcd1a486e9452fed92e725c9c.zip,
  SHA-256 B52450812F59B430B0AA25DD2813B7405F7568B920A1CAB62F7F1C426C2A5AE2.

Mỗi gói 280 payload/hash/PE xác minh trên PS7/PS5.1, 9 package notices và
4/4 kiểm tra resource binary trước đóng gói. Không chạy app; không cài runtime,
không ký/phát hành. Gói chứa cả bản vá HighContrastChanged trước đây, nhưng
khởi động thực tế vẫn cần xác nhận. ZIP cũ và Sandbox input cũ được giữ nguyên.

## CF_HTML an toàn — 2026-10-10

ConvertService Fluent không chuyển cả buffer HTML như văn bản nữa. HtmlClipboard.h
đọc bốn offset thập phân có kiểm tra overflow/bounds/field trùng, giữ độ rộng header,
SourceURL/context và thẻ/thuộc tính/comment; chuyển text node rồi tính lại EndHTML
và EndFragment theo số byte UTF-8. Kiểm tra UTF-8 nghiêm ngặt trước EmptyClipboard.
Theo đặc tả Microsoft, CF_HTML luôn dùng UTF-8 và offset tính từ đầu buffer theo byte:
https://learn.microsoft.com/en-us/windows/win32/dataxchg/html-clipboard-format

Đây là tập con có chủ đích: từ chối entity trong text, script/style/raw-text,
selection offsets, context -1, bảng mã khác Unicode và sentence/title casing.
Những ca chưa hỗ trợ trả WriteFailed và giữ Clipboard nguyên trạng; không tự hạ
xuống plain text. Chưa phải HTML parser đầy đủ hoặc nghiệm thu ứng dụng đích.
Unicode-only conversion vẫn giữ toàn bộ chế độ/bảng mã như trước.

35/35 ca Clipboard fake và 175/175 text + 20/20 engine đạt x64/x86 trên PS7/PS5.1.
Ca mới gồm offset thay đổi khi bỏ dấu, markup/attribute/comment giữ nguyên,
emoji, CR/LF/CRLF, fragment rỗng, UTF-8 lỗi, header/offset lỗi và từ chối an toàn.
Không chạm Clipboard Windows thật; lỗi SetClipboardData sau EmptyClipboard vẫn
có thể làm mất một phần dữ liệu, chưa có rollback. Hotkey engine legacy chưa đổi.

Build Debug UI x64/x86 thành công 0 cảnh báo/0 lỗi, log dist/cfhtml-debug-x64.log
và dist/cfhtml-debug-x86.log. Bản vá HighContrastChanged đã link vào binary Debug;
chưa chạy UI thật để nghiệm thu. ZIP beta cũ chưa cập nhật trong hạng mục này.

Full regression 14/14 nhóm đạt trên cả PowerShell 7 và 5.1, báo cáo lần lượt:
dist/regression-ca2f5ef38adf4aef839722e60800f9c2/summary.json và
dist/regression-52248f3d006942eabec45009cb88f505/summary.json.
Release UI x64/x86 cũng build đạt 0 cảnh báo/0 lỗi; không tạo lại ZIP beta.

## Chẩn đoán launcher UI Debug — 2026-10-10

run-ui-debug.ps1 dùng helper xác định đúng path Debug x64/x86 và kiểm tra process
giữ executable trước build. Không dừng process/debugger tự động. Path không đọc được
không bị đoán là cùng output; nếu khóa không phát hiện được thì build vẫn fail và
không chạy binary cũ. Build chạy ở shell con để exit của build.ps1 không bỏ qua
report finally, log và launch.json riêng trong dist/ui-debug-GUID.

Start-Process giữ handle sớm, probe sau ba giây; mã lỗi được ghi signed và hex.
Exit code không đọc được báo unknown/error, không tự coi là 0. Code 0 là trạng thái
exited-zero-unverified, không khẳng định redirect thành công; process sống là
running-unverified, không chứng minh cửa sổ hiển thị/đáp ứng. Không gắn debugger,
không đổi engine/hook/settings hoặc mở Sandbox.

15 ca launcher đạt PS7/PS5.1: path hai kiến trúc, chặn path tương ứng/case-insensitive,
không đoán path thiếu, phân loại trạng thái, hex crash code, parse và gọi script thật
với Get-Process giả để xác minh chặn trước build. Chưa chạy nhánh launch thành công,
crash thực tế hoặc redirect bằng script mới. Lỗi khóa LNK1104 đã được giải phóng;
bản vá HighContrastChanged đã link Debug x64/x86 trong hạng mục CF_HTML bên trên.

PowerShell 7 full regression 14/14 nhóm đạt, report
dist/regression-a5a39c6b5d4d4ae78c19d7f096c3f1a8/summary.json (launcher 14 ca tại
thời điểm chạy); ca preflight thứ 15 được chạy bổ sung trên PS7/PS5.1 đều đạt.
PowerShell 5.1 full regression 14/14 nhóm đạt trên mã cuối (launcher 15 ca), report
dist/regression-f08ccb7bbe2f4adcb204cec1d40b9f5c/summary.json. Không build/đóng gói
lại app trong hạng mục script này; gói beta cũ chưa chứa bản vá High Contrast.

- [ ] Sau Shift+F5: chạy script thật, kiểm tra build.log/launch.json và UI mở được.
- [ ] Thử crash/redirect/single-instance thực tế và lỗi đọc process path.

## Khởi động khi đăng ký High Contrast thất bại — 2026-10-10

Call Stack do người dùng cung cấp xác nhận exception từ
AccessibilitySettings.HighContrastChanged(auto_revoke) trong constructor
MainWindow; việc đăng ký không có catch làm cửa sổ không khởi tạo xong.
Thêm catch hresult_error cho riêng subscription tùy chọn này, ghi OutputDebugStringW
và tiếp tục ConfigureInitialWindow/navigation. Không bỏ logic đọc HighContrast,
không đổi settings/engine. ActualThemeChanged và ColorValuesChanged vẫn cập nhật
caption; chưa chứng minh mọi chuyển đổi High Contrast đều phát hai sự kiện này.

Regression cấu trúc kiểm tra catch/fallback và giữ hai kênh theme/color;
20/20 ca nguồn/XML đạt PS7/PS5.1, 414 khóa vi/en. Test nguồn không giả lập COM
failure thực tế. Visual Studio có thể vẫn dừng ở first-chance exception dù đã catch;
F5 tiếp tục phải đi qua catch và tạo cửa sổ, cần người dùng xác minh runtime.

- [ ] Chạy lại Debug x64 trên máy báo lỗi: cửa sổ mở sau catch/F5, không crash.
- [ ] Kiểm thử đổi High Contrast/theme và màu caption trên Windows thật.

## Cảnh báo tùy chọn phụ thuộc — 2026-10-10

Compatibility và Advanced có InfoBar Warning riêng, chỉ mở khi điều kiện tiên
quyết tắt nhưng tùy chọn con vẫn được chọn. Chromium phụ thuộc Sửa lỗi đúp từ;
hai tùy chọn chính tả nâng cao phụ thuộc Kiểm tra chính tả, theo logic khóa control
hiện có. Cảnh báo nói rõ lựa chọn được giữ và điều kiện để chỉnh sửa, không tự sửa
registry/toggle hoặc tuyên bố tùy chọn bị engine vô hiệu hóa. Không thay engine/hook.

SettingsDependencyState.h chỉ trả available/warnRetainedSelection; hai trang dùng
chung policy trong UpdateDependentControls lúc khởi tạo/refresh/thao tác. Advanced
vẫn cập nhật checkSpelling khi giữ pending edit sau lỗi lưu. Save status và warning
độc lập; vi/en thêm sáu khóa (414 khóa mỗi ngôn ngữ), LiveSetting=Polite.

13 ca logic phụ thuộc đạt x64/x86: bốn tổ hợp Chromium, tám tổ hợp chính tả nâng
cao và giữ lựa chọn khi bật lại điều kiện. Không có registry/UI/hook trong test.
test-ui-resources thêm kiểm tra warning/localization, tổng 19 ca XML.

Full regression: 13/13 suites, 1218 checks mỗi shell PS7/PS5.1 (74 common +
2 × 572 logic). Reports dist/regression-8628c8419cc6426caf98d589e923e3ad/summary.json
và dist/regression-310c6e131bd14402a9e6c7fd75c15c68/summary.json. PS7 XML test được
chạy lại sau thêm assertion status riêng; tất cả đạt. Debug x64 mã cuối build/static
analysis 0 cảnh báo/0 lỗi (dist/ui-dependency-debug-x64-final.log). IsOpen warning
chỉ được gán khi đổi trạng thái, không gán lặp theo timer; Narrator thực tế chưa QA.

Debug x86 và Release All x64/x86 cũng build 0 cảnh báo/0 lỗi. Đã tạo hai ZIP riêng
(tên/hash ở BETA_PACKAGING.md), checker binary 4/4 trước staging, 280 payload mỗi
ZIP và verifier PS7/PS5.1 đạt. Logs dist/ui-dependency-debug-x86.log và
dist/ui-dependency-beta-x64.log/x86.log. Không chạy app, đổi cấu hình hay thay ZIP
trong đầu vào Sandbox đã chuẩn bị; bộ cài offline vẫn ở phiên cũ.

- [ ] Cả hai ngôn ngữ: lựa chọn con bật + điều kiện tắt → control khóa và cảnh báo hiện; bật lại điều kiện → cảnh báo tắt, lựa chọn còn.
- [ ] Đổi từ tray rồi quay lại, lỗi lưu/retry và cuộn/narrow/DPI: warning không che thông báo lỗi hoặc ghi đè pending edit.
- [ ] Narrator đọc cảnh báo hợp lý, không lặp mỗi 500 ms; test XML không chứng minh hành vi live region runtime.

## Chuẩn bị Windows Sandbox — 2026-10-10

Thêm prepare-sandbox.ps1, cấu hình XML thuần, guest bootstrap và test-sandbox.ps1.
Chỉ map Input chỉ đọc và Logs riêng có quyền ghi; tắt mạng/clipboard/microphone/
camera/printer, vGPU Default. LogonCommand chạy sau mappings với tài khoản mặc định
WDAGUtilityAccount theo [tài liệu Microsoft](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file).
Guard tài khoản tránh chạy nhầm host, không chứng minh danh tính guest về mặt bảo mật.

ZIP được kiểm tra bằng hash ngoài trước staging và trong guest. Bootstrap chỉ giải
nén trong guest, đọc prerequisite, không cài runtime hoặc tự chạy engine. UI còn sống
sau ba giây chỉ ghi smoke-only. Logs host vẫn có thể bị guest thay đổi; không chứa
dữ liệu quan trọng. Chưa mở Sandbox hoặc thực hiện kiểm thử máy sạch.

27/27 ca Sandbox đạt trên PowerShell 7 và Windows PowerShell 5.1: mappings, quyền,
đường dẫn tuyệt đối/khác nhau, escaping XML, cấu hình thiết bị/mạng, guard, phân loại
trạng thái và parse script. Toàn bộ 13/13 regression suites / 1191 checks mỗi shell
đạt; reports dist/regression-fede43e5f8e64d65b37e9a85bb747e29/summary.json và
dist/regression-b394f1104db94b00b45f579735bf80b4/summary.json. Không build lại app;
hai ZIP 280 payload hiện tại không đổi. Hướng dẫn tại Sandbox/README.md.

- [x] Chuẩn bị cấu hình x64/x86, xác minh ZIP đầu vào và test harness hai shell.
- [ ] Mở Sandbox thật, kiểm tra log/runtime load và UI khởi động.
- [ ] Kiểm thử engine/gõ tiếng Việt, giao diện, clipboard nội bộ guest và migration.
- [ ] Kiểm thử máy Windows sạch ngoài Sandbox; Sandbox không thay toàn bộ test matrix.

## Icon và manifest executable — 2026-10-10

AppResources.rc nhúng icon.ico nguyên bản của OpenKey (sáu frame), IDI_OPENKEY_UI=101. MainWindow nạp handle shared bằng LoadImageW/LR_DEFAULTSIZE/LR_SHARED và gán AppWindow.SetIcon qua GetIconIdFromIcon; không DestroyIcon handle shared, không yêu cầu .ico bên ngoài hoặc thay icon tray/engine. Lỗi nạp/interop icon không chặn khởi tạo cửa sổ. Custom titlebar vẫn có nhãn OpenKey, không thêm control/bitmap trong vùng kéo. Tham chiếu: [AppWindow.SetIcon](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.windowing.appwindow.seticon), [LoadImageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagew).

test-ui-binary.ps1 kiểm tra output đã build: PE architecture, group icon đúng header/count, từng frame RT_ICON byte khớp asset gốc, manifest nhúng có PerMonitorV2. LoadLibraryExW dùng LOAD_LIBRARY_AS_DATAFILE để đọc resource, không chạy mã ứng dụng/GUI; finally FreeLibrary giải phóng mapping. package-beta gọi checker trước khi tạo stage/ZIP, còn verifier ZIP tiếp tục kiểm hash/PE độc lập. Tham chiếu: [LoadLibraryExW và data-file mapping](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw). Không chứng minh icon runtime, shell cache, DPI selection/scaling hoặc quyền phân phối runtime.

Kết quả cuối nhóm cache/history/icon: 12/12 regression suites, 1164 checks mỗi PS7/PS5.1 (46 common + 2 × 559 ca logic). Reports: dist/regression-1c47cb8520e545f895237a9b06bae0bc/summary.json và dist/regression-a6c969367025437b994992fc488bb182/summary.json. Kiểm tra binary thêm 4/4 × bốn cấu hình = 16 ca mỗi shell. Bốn UI build 0 cảnh báo/0 lỗi; Debug x64 rebuild/static analysis sạch, ba cấu hình còn lại build thường. Logs dist/ui-session-icon-<configuration>-<architecture>.log. Không dùng kết quả này để đánh dấu UI/Narrator/High Contrast/máy sạch đạt.

Đã tạo lại beta bằng Release All x64/x86 sau mã cuối, checker binary 4/4 đạt trước stage, không SkipBuild; build sạch. Hai ZIP xác minh 280 payload/hash/PE bằng PS7/PS5.1, tên/hash ở BETA_PACKAGING.md; logs dist/ui-session-icon-beta-x64.log và x86.log. Payload tăng icon.ico do Image item được copy; runtime SetIcon vẫn dùng resource nhúng. Không xóa gói cũ, chạy app, đổi settings hoặc phát hành stable.

- [ ] Explorer hiển thị icon OpenKey cho executable mới; taskbar/Alt+Tab dùng đúng icon sau mở UI.
- [ ] DPI 100/150/200%, nhiều màn hình: icon rõ, caption/drag/hit-test vẫn đúng; không kỳ vọng custom titlebar có thêm hình.
- [ ] Chạy beta với toàn bộ runtime; fallback startup vẫn hoạt động nếu gán icon thất bại.

## Giữ nội dung nhập liệu trong phiên điều hướng — 2026-10-10

MacroPage và ConvertPage khai báo NavigationCacheMode=Required: Frame giữ lại hai page instance trong cửa sổ hiện tại để không tạo lại editor khi quay về từ trang khác. Giữ snapshot/list/search/draft Macro và input/result Convert; không tự lưu nội dung hoặc làm persistence qua đóng cửa sổ. Các trang còn lại tiếp tục dùng mặc định Disabled. Timer hai trang vốn có Loaded/Unloaded weak handlers, Stop lúc rời trang và Start/refresh preference khi quay lại; Macro không tự tải lại danh sách để tránh thay snapshot/draft. Dữ liệu Macro đổi ngoài UI vẫn cần Tải lại và xác nhận draft.

ContentFrame có IsNavigationStackEnabled=False; shell chỉ điều hướng ngang cấp và nút Back đã collapsed. Không thêm history BackStack/ForwardStack mỗi lần chọn menu; Required cache là chính sách giữ page riêng, không phải lưu history. Tham chiếu: [Page.NavigationCacheMode](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.page.navigationcachemode), [Frame.IsNavigationStackEnabled](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.frame.isnavigationstackenabled?view=windows-app-sdk-2.0).

test-ui-resources bổ sung hai kiểm tra cấu trúc: chỉ hai trang draft Required, các trang khác Disabled; Frame không ghi history và Back UI collapsed. Sau thêm kiểm tra icon/manifest, tổng hiện tại 18 ca XML, không chứng minh giữ focus, timer/event hoặc bộ nhớ thực tế. Không thay engine/hook, clipboard hoặc registry người dùng. Nội dung đang nhập vẫn mất khi đóng UI; không thêm xác nhận đóng cửa sổ ở bước này.

- [ ] Macro: sửa/chưa lưu + bộ lọc → trang khác → quay lại; draft/selection/filter/status còn. Tải lại vẫn hỏi Hủy/Tiếp tục đúng.
- [ ] Convert: có input/result → trang khác → quay lại; nội dung/các nút Copy/Clear vẫn đúng, không tự chuyển mã/đổi clipboard.
- [ ] Chuyển nhiều trang lặp lại, đổi theme/tray lúc đang ẩn rồi quay lại: theme/preferences cập nhật, draft không bị ghi đè; timer ẩn không chạy.
- [ ] Hộp thoại đang mở + navigation/đóng UI: vẫn cần QA riêng; cache không phải khóa modal toàn shell.
- [ ] Đóng UI rồi mở lại: không có draft cũ; engine/tray vẫn hoạt động. Cache chỉ tồn tại trong cửa sổ hiện tại.

## Bảo vệ thao tác và tệp xuất Gõ tắt — 2026-10-10

Gõ tắt dùng khóa scope UI-thread chung cho Tải lại/Mới/chọn dòng/Lưu/Xóa/Import/Export. Các control chỉnh sửa, tìm kiếm và auto-capitalize tạm disabled trong thao tác; khóa được nhả khi hủy, return hoặc lỗi. Mới/chọn dòng/Import hỏi trước khi bỏ draft, mặc định Hủy; hủy chọn dòng phục hồi selection cũ. Dialog xóa chụp shortcut trước await và bắt hresult_error; dialog dùng ActualTheme hiện tại. Thêm ba resource động, tổng 408 khóa Việt/Anh khớp nhau. Khóa này không phải khóa liên tiến trình, không ngăn navigation hoặc đóng cửa sổ.

Export không còn truncate trực tiếp tệp đích: kiểm tra snapshot/encoding trước, CREATE_NEW tệp tạm cạnh đích, xử lý ghi thiếu/không tiến triển, FlushFileBuffers/CloseHandle rồi MoveFileExW với REPLACE_EXISTING/WRITE_THROUGH. Chỉ xóa tệp tạm đã tạo nếu thất bại, không xóa tệp trùng tên thuộc writer khác. Tham chiếu API: [Microsoft MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw). Không coi đây là bảo đảm crash/power-loss hoặc bảo toàn ACL/metadata; cleanup có thể để lại tệp tạm nếu bị từ chối xóa, nhiều writer vẫn có thể thay cùng đích.

71/71 ca macro đạt x64/x86 bằng PS7 và PS5.1: 51 ca service/draft cũ + 8 pure OperationGate + 12 export. Fault injection dùng API file thật trong thư mục fixture riêng, giả lập lỗi tạo/ghi/flush/thay tệp; kiểm tra partial write/zero progress, collision/retry/exhaustion, thay đích, khóa đích thật và đích thư mục. Kiểm tra tệp đã tạo không còn sau thao tác và tệp collision không bị xóa/đổi byte. Không dùng registry/clipboard thật hoặc chạy GUI. Test gate không mô phỏng coroutine, focus hoặc control/modal WinUI.

Full regression cuối đạt 12/12 suites, 1160 lượt kiểm tra mỗi shell: 42 common + 2 × (212 convert + 245 settings + 71 macro + 31 updater). Báo cáo PS7: dist/regression-014f2b44610a4b7f9f619806e95d1d96/summary.json; PS5.1: dist/regression-4502ca283f1243aea299578dd4be7b37/summary.json. Kiểm thử thủ công bên dưới và checklist tổng thể vẫn mở, không coi regression tự động là nghiệm thu migration.

UI/service rebuild kèm static analysis Debug/Release x64/x86 đạt 0 cảnh báo/0 lỗi; logs dist/macro-safety-<configuration>-<architecture>.log. Mã engine/hook không sửa trong nhóm thay đổi này.

Đã tạo lại hai ZIP beta sau thay đổi macro bằng Release All, không SkipBuild, 0 cảnh báo/0 lỗi; mỗi ZIP có 279 payload, verifier PS7/PS5.1 đạt, không chạy executable. Tên/hash mới ở BETA_PACKAGING.md; logs dist/macro-safety-beta-x64.log và x86.log. Gói cũ được giữ nguyên, không phát hành stable.

- [ ] Có draft → Mới/chọn dòng/Import/Tải lại → Hủy: giữ nội dung, selection và trạng thái editor phù hợp.
- [ ] Tiếp tục: thao tác thành công reset editor; picker/import hủy hoặc lỗi giữ draft.
- [ ] Nhấn nhanh nhiều thao tác/hotkey, lỗi dialog và hủy modal: không mở thao tác chồng, control/focus hoạt động lại.
- [ ] Export từ UI: đích mới/ghi đè/đích đang khóa/đường dẫn tiếng Việt; nhập lại file xuất.
- [ ] Điều hướng/đóng UI khi có draft hoặc modal: kiểm thử riêng, chưa có bảo vệ navigation toàn shell.

## Hoàn thiện resource, fallback và regression — 2026-10-10

Styles/FluentResources.xaml tập trung page padding/spacing, card padding/stroke/corner radius, typography title và wrapping của ToggleSwitch/CheckBox/ComboBox. 21 card dùng chung style, 9 trang dùng chung container/title. Giữ local padding 16,10 của Macro auto-caps, 0 của Macro list (header list vẫn có padding riêng), 28/corner 12 của brand About. Trang About bỏ MaxWidth/margin cũ, cùng lề với các trang còn lại. Không thay control templates/focus/hover/disabled của WinUI; màu card vẫn ThemeResource.

MainWindow kiểm tra MicaController.IsSupported, bắt hresult_error khi khởi tạo và giữ Border nền ApplicationPageBackgroundThemeBrush nếu không có material. Border phủ hai hàng title/content, chỉ collapsed sau khi SystemBackdrop được gán thành công, không đè Mica. Chính sách fallback khi tắt transparency/battery saver do backdrop tích hợp quản lý; chưa xác nhận runtime. Tham chiếu: [Microsoft system backdrops](https://learn.microsoft.com/en-us/windows/apps/develop/ui/system-backdrops).

Tất cả navigation dùng NavigateTo, bỏ việc Navigate lặp lại cùng trang, đọc AnimationsEnabled ở mỗi lần đổi trang; khi tắt animation dùng SuppressNavigationTransitionInfo. Không thêm storyboard/custom animation. Tham chiếu: [AnimationsEnabled](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.uisettings.animationsenabled), [SuppressNavigationTransitionInfo](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.media.animation.suppressnavigationtransitioninfo). Tổng quan giữ pending toggle sau lỗi save thay vì bị polling ghi đè, chưa xác nhận UI failure runtime.

test-ui-resources.ps1 đạt 14/14 kiểm tra cấu trúc trên PS7/5.1: XML, 405 khóa Việt/Anh khớp/unique, x:Uid có resources, x:Name unique, dictionary merge/project inclusion, 21 card/9 container/title, theme brushes/native templates, adaptive target/navigation và fallback. Đây không phải render hay visual QA.

Thêm test-all.ps1 All (hoặc x64/x86), child process riêng cho từng bộ, fail-fast có log và summary.json trong thư mục dist/regression-GUID riêng. Không chạy ứng dụng/hook, không đổi registry/clipboard thật; updater chỉ chạy helper với fixture giả. Toàn bộ 12 bộ đạt trên mỗi PS7 và PS5.1: 539 ca mỗi kiến trúc (212 convert/engine/clipboard + 245 settings + 51 macro + 31 updater), cộng 42 ca source/package/prerequisite/notices dùng chung = 1120 lượt kiểm tra mỗi shell. Lượt PS5.1 ban đầu thất bại ở fixture updater Unicode: script UTF-8 thiếu BOM bị đọc ANSI; đã thêm BOM, chạy lại toàn bộ và đạt. Test-all cũng giữ stderr trong log để ghi báo cáo thất bại thay vì bị NativeCommandError chặn trước summary.

Báo cáo cuối: dist/regression-e5e890e83613419eb7738f9b9e38ac15/summary.json (PS7), dist/regression-c093a183543c42e8aa862419bda46af6/summary.json (PS5.1). UI/service rebuild/static analysis Debug/Release x64/x86 0 cảnh báo/0 lỗi, logs dist/fluent-design-final-<configuration>-<architecture>.log. Mã engine/hook không sửa trong lượt này. Bộ kiểm thử giả không chứng minh timing, clipboard HTML, native events hoặc chạy trên máy sạch.

### Checklist kiểm tra một lượt sau khi hoàn thiện

Không cần đổi thiết lập hệ thống hoặc test ngay trong lúc agent làm việc. Khi sẵn sàng, dùng dữ liệu Macro/clipboard thử riêng và sao lưu cấu hình trước theo MIGRATION_FLUENT_BETA.md.

- [ ] Chín trang ở Việt/Anh: chữ đọc được, bố cục cùng lề, control/tab/focus không bị che; cửa sổ hẹp vẫn cuộn được.
- [ ] Sáng/Tối/Theo hệ thống: đổi ngay, lưu khi mở lại; đổi theme Windows trong phiên không ghi đè explicit theme.
- [ ] Mica/fallback, High Contrast, DPI/cỡ chữ 100/150/200% và Narrator.
- [ ] Windows tắt animation: đổi trang không có transition; click lại trang hiện tại không reset nội dung.
- [ ] Titlebar kéo/double-click/Snap, minimize/maximize/Restore, caption active/inactive đọc được.
- [ ] Đóng/mở UI giữ size/position/maximize; thử đa màn hình và tháo màn hình khi thuận tiện.
- [ ] Hotkey/tray đổi Việt/Anh/kiểu gõ/bảng mã; trang đang mở phản ánh đúng, đóng UI engine vẫn gõ được.
- [ ] Notepad/Office/Chromium/Firefox, cửa sổ thường/admin, Telex/VNI và các bảng mã dùng thực tế.
- [ ] Gõ tắt: thêm/sửa/xóa/tìm/tải lại, Hủy xác nhận giữ draft; import gộp/thay thế/export với dữ liệu thử.
- [ ] Chuyển mã: Unicode/TCVN3/VNI/CP1258, casing/bỏ dấu, Copy/Clear/clipboard và hotkey; CF_HTML riêng vẫn cần kiểm tra offsets.
- [ ] Single-instance, startup/tray/Explorer restart/sleep-resume và thoát engine.
- [ ] ZIP trên máy sạch, nâng cấp/rollback giữ dữ liệu; không dùng updater stable cho bundle Fluent.

Các mục trên vẫn mở theo yêu cầu người dùng kiểm thử tập trung sau. Không đánh dấu hoàn tất migration/release từ regression tự động. Đã đóng gói Release All x64/x86 mới: 279 payload mỗi ZIP, xác minh architecture/hash bằng PS7/PS5.1, giữ nguyên gói cũ và không chạy beta thật. Artifact/hash mới ghi ở BETA_PACKAGING.md; logs dist/fluent-design-beta-x64.log và x86.log.

## Tùy chọn theme — 2026-10-09

Trang Nâng cao có Theo hệ thống/Sáng/Tối, bản dịch Việt/Anh và automation name. Lưu một DWORD uiTheme (0/1/2) tại khóa cấu hình OpenKey; độc lập engine/input mode/ngôn ngữ UI. Thiếu, sai type/size/range hoặc lỗi đọc trả Theo hệ thống. Lưu sai enum không tạo key; lỗi ghi báo thất bại, giữ theme root và đưa ComboBox về lựa chọn trước. Không đọc lại registry để phục hồi ComboBox khi thao tác lưu thất bại.

Khởi tạo MainWindow áp dụng theme trước khi tạo backdrop/navigate. Thay lựa chọn áp dụng RequestedTheme trên root hiện có, không tạo lại trang hoặc xóa draft. Theo hệ thống được phân giải từ foreground của UISettings bằng phép phân loại màu; ColorValuesChanged dispatch về UI thread, weak callback/auto revoker. Explicit Sáng/Tối vẫn giữ khi Windows đổi màu. RequestedTheme của FrameworkElement đổi được lúc chạy; High Contrast được XAML ưu tiên. Tham chiếu: [RequestedTheme](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.frameworkelement.requestedtheme), [UISettings và phát hiện thay đổi theme](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes).

Caption buttons còn native nhưng foreground/hover/pressed được lấy từ XAML theme brushes, thay cho mặc định native của bước trước. ActualThemeChanged cập nhật qua dispatcher sau đổi theme; HighContrastChanged đọc màu hệ thống cho caption. Không tự vẽ nút, không thay drag region. Windows 10 có giới hạn đổi màu caption; cần QA riêng. Event revokers được hủy cùng MainWindow; callback không giữ window sống.

23/23 ca theme fake-registry/pure-color trên x64/x86 bằng PS7 và PS5.1. Tổng settings 245/245 mỗi kiến trúc/shell (172 cũ + 50 geometry/titlebar + 23 theme); không xác minh event Windows, Mica hay UI thật. XML MainWindow/Advanced hợp lệ, 405 khóa resource Việt/Anh khớp nhau, không trùng. Không ghi registry thật hoặc đổi theme Windows khi chạy test. Đã làm mới ZIP beta cho theme/titlebar/maximize mới; tên và hash ở BETA_PACKAGING.md.

- [ ] Nâng cao → Sáng/Tối/Theo hệ thống đổi ngay toàn bộ trang, menu/flyout, dialog và Mica; không cắt chữ/nút ở Việt/Anh.
- [ ] Chọn theme, đóng/mở UI: khôi phục lựa chọn; bộ gõ ở tray không đổi chế độ.
- [ ] Theo hệ thống → đổi Windows Light/Dark khi UI đang mở; explicit theme không bị Windows ghi đè.
- [ ] Có draft ở chuyển mã/gõ tắt; đổi theme và quay lại không mất draft do thay theme (không suy ra page navigation đã giữ draft).
- [ ] Bật/tắt High Contrast: chữ, focus, caption active/inactive/hover/pressed đọc được; kiểm tra thêm Windows 10 và DPI khác nhau.
- [ ] Lỗi lưu thật (nếu môi trường test an toàn): báo lỗi, giữ theme đang áp dụng/lựa chọn trước, không sửa engine settings.

Debug/Release x64/x86 đều build 0 cảnh báo/0 lỗi; logs dist/fluent-theme-final-<configuration>-<architecture>.log. package-beta gọi lại Release All cho mỗi kiến trúc rồi tạo ZIP mới, không ghi đè/xóa gói cũ. Hai ZIP đều xác minh 278 payload/kiến trúc/hash bằng PS7 và PS5.1, không giải nén/chạy app. Kiểm thử trực quan vẫn chưa hoàn tất; checklist theme tổng thể giữ mở.

## Thanh tiêu đề Fluent — 2026-10-09

MainWindow XAML có hai hàng: TitleBarDragRegion và NavigationView; titlebar mặc định collapsed, chỉ hiện khi AppWindowTitleBar.IsCustomizationSupported=true. ExtendsContentIntoTitleBar và SetTitleBar đưa vùng tiêu đề vào client/Mica hiện có, giữ native caption buttons và native handling thay vì tự vẽ nút. Brand OpenKey dùng CaptionTextBlockStyle và ellipsis. Không thêm control tương tác/tab stop trong drag region. Không hỗ trợ FullScreen/CompactOverlay ở bước này.

UpdateTitleBarLayout đọc LeftInset/RightInset/Height native, quy đổi pixel→DIP theo DPI HWND, chừa hai cột Pixel cho native UI và đặt chiều cao tương ứng (fallback 32 DIP). Cập nhật lúc Loaded và window đổi vị trí/kích thước, weak handler. Navigation ở hàng riêng tránh lấy vùng click điều hướng làm drag region. Nền caption active/inactive trong suốt để backdrop hiện qua, còn foreground/hover/pressed dùng mặc định native; Activated giảm opacity text khi inactive. Windows 10 không hỗ trợ đầy đủ đổi màu, cần QA visual riêng. Tham chiếu: [Microsoft title bar customization](https://learn.microsoft.com/en-us/windows/apps/develop/title-bar?tabs=winui3). Không nâng Windows App SDK để dùng TitleBar control mới.

Thêm 3 ca math cho inset tại 96/144/192 DPI, invalid/negative metrics, DPI fallback và fractional DIP. Window/layout tests 50/50 + settings cũ 172/172 = 222 ca trên x64/x86 bằng PowerShell 7 và Windows PowerShell 5.1. MainWindow XML parse và kiểm tra navigation row riêng đạt. Test không mô phỏng XAML/native drag/caption hit testing; chưa xác minh native metrics và Mica visual runtime.

- [ ] Kéo vùng trống/title: di chuyển cửa sổ; double-click maximize/Restore; Alt+Space và Snap vẫn dùng được.
- [ ] Minimize/maximize/close bấm được và không bị label/drag che; vùng điều hướng click/keyboard đúng như trước.
- [ ] Đổi DPI/màn hình/resize: label không che caption, titlebar không cắt chữ; window persistence còn đúng.
- [ ] Theme sáng/tối, High Contrast, active/inactive và Windows 10: chữ/nút đọc được, fallback native không thêm hàng trống.
- [ ] Đóng UI vẫn không làm thay đổi engine/input mode; không suy ra lifecycle/hook từ build.

Build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi, kể cả build cuối sau inset Pixel/transparent caption/inactive title; log dist/fluent-titlebar-final-<configuration>-<architecture>.log. XAML cuối parse và cấu trúc title/navigation row đạt. ZIP beta hiện chưa chứa titlebar và maximize/cache mới. Hạng mục custom titlebar tổng thể giữ mở đến khi QA tương tác/visual đạt.

## Ghi nhớ maximize và geometry thường — 2026-10-09

Thêm khóa UI riêng uiWindowMaximized dạng REG_DWORD 0/1. Thiếu/type/size/value sai hoặc lỗi đọc thì mặc định không maximize. Khởi tạo vẫn khôi phục size/position thường, chọn display gần nhất, fit/clamp như bước trước, capture geometry thường, rồi gọi OverlappedPresenter.Maximize nếu có flag hợp lệ và presenter phù hợp. Không lưu/khôi phục minimize.

MainWindow cache geometry thường theo AppWindow.Changed khi size/position thay đổi; callback weak, không ghi registry lúc kéo/resize. CaptureNormalGeometry chỉ thay cache khi native window không minimize/maximize và geometry hợp lệ. Khi đóng qua Closing: nếu minimize, không ghi tùy chọn window; ngược lại cập nhật cache rồi lưu size/position thường và maximize flag. Đóng lúc maximize không lưu rectangle toàn màn hình vào geometry thường; giữ geometry thường cuối phiên đã capture để Restore lần sau dùng. Ghi ba value riêng, không có transaction; lỗi từng ghi không chặn đóng hoặc đổi engine settings. Kill/crash/đường đóng không phát Closing chưa được bao phủ.

Thêm 12 ca fake/pure: flag thiếu/true-false roundtrip, bảo toàn size/position, type/size/value sai, lỗi open/query/create/set, cache geometry thường, bỏ qua geometry maximize/minimize/không hợp lệ và cập nhật sau Restore. Window tests 47/47 + settings cũ 172/172 = 219 ca mỗi x64/x86 trên PS7/5.1. Test không mô phỏng event HWND/Changed/DPI/activation hoặc gọi maximize UI thật; chưa xác nhận thứ tự native flags tại Changed. API tham chiếu: [AppWindowChangedEventArgs](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.windowing.appwindowchangedeventargs?view=windows-app-sdk-1.6), [OverlappedPresenter.Maximize](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.windowing.overlappedpresenter.maximize?view=windows-app-sdk-1.6).

- [ ] Resize/move window thường → maximize → X/Alt+F4 → mở lại: mở maximize; Restore trở về geometry thường cuối phiên trong work area.
- [ ] Restore về window thường rồi đóng/mở: không tự maximize; size/position thường giữ đúng.
- [ ] Đóng từ trạng thái minimize không lưu rectangle icon hoặc state minimized; lần mở tiếp vẫn nhìn thấy window.
- [ ] Maximize/Restore ở màn hình DPI khác và sau tháo màn hình: window vẫn nhìn thấy, geometry thường an toàn.
- [ ] AppWindow.Changed không ghi registry/engine settings hoặc khiến app tự mở/đổi focus; lỗi lưu preference không chặn đóng.

Build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi; log dist/window-state-<configuration>-<architecture>.log. ZIP beta từ bước vị trí cửa sổ chưa chứa thay đổi maximize/cache mới. Các ghi chú cũ “không lưu lúc maximize” và “maximize persistence còn mở” ở mục lịch sử được thay bằng implementation này, nhưng QA thực tế vẫn mở.

## Ghi nhớ vị trí cửa sổ — 2026-10-09

Bổ sung HKCU\\SOFTWARE\\TuyenMai\\OpenKey / uiWindowPosition, REG_BINARY version=1 và LONG x/y theo pixel màn hình, hỗ trợ tọa độ âm. Record sai type/size/version, lỗi đọc hoặc tọa độ ngoài ±1.000.000 bị bỏ qua, không sửa output caller và UI căn giữa theo behavior cũ. Không đổi uiWindowSize hoặc engine settings khi lưu riêng vị trí. Khi đóng window thường, lưu kích thước và vị trí bằng hai record riêng; không có transaction giữa hai ghi, lỗi từng ghi không chặn đóng. Vẫn không lưu khi minimize/maximize, không lưu trạng thái maximize hoặc lần resize trong phiên bị kill/crash.

Khi khôi phục, chọn DisplayArea gần điểm lưu với fallback Nearest; trước activation, đưa window vào work area display đó để HWND cập nhật DPI, sau đó tính kích thước theo DIP và clamp tọa độ để toàn bộ window nằm trong work area. Nếu display đã tháo hoặc vị trí cũ ngoài màn hình, nearest/clamp là đường fallback; không lưu định danh màn hình hay offset tương đối, nên bố trí có thể khác khi topology/DPI thay đổi. Không có displayArea thì resize theo DPI HWND hiện tại như fallback cũ.

Tài liệu API: [DisplayArea.GetFromPoint](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.windowing.displayarea.getfrompoint?view=windows-app-sdk-1.6), [WM_DPICHANGED](https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged). Chưa kiểm chứng thứ tự DPI/MoveAndResize thực tế chỉ bằng build.

Thêm 14 ca registry giả và math: position thiếu/âm/roundtrip, bảo toàn size, type/byte size/version/bounds sai, read/create/set lỗi, save không hợp lệ không ghi, clamp màn hình primary/negative/taskbar-offset/tiny/extreme arithmetic. Window tests 35/35 + settings cũ 172/172 = 207 ca mỗi x64/x86 trên PS7/5.1. Không thay registry người dùng, không tự đổi bố trí màn hình.

- [ ] Di chuyển và resize window thường, X/Alt+F4 rồi mở lại: kích thước/vị trí giữ đúng trong cùng bố trí màn hình.
- [ ] Màn hình phụ bên trái/trên (tọa độ âm): khôi phục đúng display, không bị tràn khỏi work area.
- [ ] Tháo màn hình phụ hoặc đổi DPI/topology: cửa sổ còn nhìn thấy được, kích thước fit display hiện tại.
- [ ] Chưa có position record hoặc lỗi ghi: fallback an toàn, không chặn đóng; minimize/maximize không ghi vị trí/kích thước sai.

Build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi; log dist/window-position-<configuration>-<architecture>.log. QA runtime vẫn mở; không đánh dấu phần đa màn hình đạt chỉ từ math fake. Gói beta được làm mới sau khi hoàn tất build và ghi nhận artifact bên dưới.

Artifact mới chứa toàn bộ thay đổi đến bước vị trí cửa sổ, bao gồm readback Gõ tắt và lưu kích thước. package-beta.ps1 không SkipBuild: Release All 0 cảnh báo/0 lỗi x64/x86, mỗi ZIP 278 payload, verifier PS7 và độc lập PS5.1 với architecture/SHA ngoài đạt:

- x64: dist/OpenKey-Fluent-beta-x64-dac9e4b76fc340d79b408f392d6f02fd.zip — SHA-256 E9516483CCAC3324D4D380678975F6B9AD8960DA45232854E63BD670EAE70E01.
- x86: dist/OpenKey-Fluent-beta-x86-6f114409c3c54f22b6f5b27dc47c6db5.zip — SHA-256 628A7B6DEF19A8B066AEE88FD47744ECBA933170322AD04D3DA8FDDED08FFD98.

ZIP cũ giữ nguyên. Những ghi chú “gói chưa chứa readback/window-size” ở mục lịch sử bên dưới được thay thế bởi hai artifact này. Chưa giải nén/chạy beta trên máy sạch, ký số, kiểm tra giấy phép redistribution hoặc phát hành stable.

## Ghi nhớ kích thước cửa sổ — 2026-10-09

MainWindow đọc tùy chọn UI riêng HKCU\\SOFTWARE\\TuyenMai\\OpenKey / uiWindowSize, REG_BINARY gồm ba DWORD version=1, widthDIP, heightDIP. Mặc định 1120×800 DIP khi key/value thiếu, đọc lỗi, type/size/version sai hoặc kích thước ngoài giới hạn 640–8192 × 400–8192. Tùy chọn không ghi engine settings hoặc gửi thông báo reload engine. Khi mở, scale theo DPI cửa sổ hiện tại, giới hạn work area với lề 24 DIP và căn giữa như trước; màn hình nhỏ được ưu tiên hơn giới hạn kích thước lưu.

Lưu kích thước window thường khi đóng qua AppWindow.Closing, handler weak; đọc HWND/rect/DPI trước khi ghi một record. Không lưu lúc minimize/maximize, không chặn đóng khi ghi lỗi, không ghi vị trí hoặc trạng thái maximize. Đóng lúc maximize/minimize giữ record đã lưu từ lần trước (không lưu lần resize gần nhất của phiên đang đóng). Kill/crash/đóng bằng đường không phát Closing chưa được bao phủ. Tài liệu event hệ thống: [Microsoft AppWindow.Closing](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.windowing.appwindow.closing?view=windows-app-sdk-1.6).

test-settings.ps1 bổ sung UiWindowSizeTests: 21/21 mỗi x64/x86 trên PowerShell 7 và Windows PowerShell 5.1, registry giả và phép tính pure. Bao phủ record hợp lệ/fallback/type/size/version/bounds, lưu roundtrip, lỗi create/set, invalid save không ghi, DPI 96/120/144/192 và fallback, logical↔physical, work area nhỏ/degenerate. Bộ settings cũ 172/172 vẫn đạt trên cả hai kiến trúc và hai PowerShell, tổng 193 ca. Test không resize/đóng UI thật hoặc ghi registry người dùng.

- [ ] Chỉnh kích thước window thường, đóng bằng X/Alt+F4 rồi mở lại: kích thước giữ đúng, nội dung có thể cuộn, engine ở tray không bị đổi chế độ.
- [ ] Đóng lúc maximize/minimize: không lưu kích thước toàn màn hình hoặc kích thước biểu tượng nhỏ, lần sau vẫn mở window thường.
- [ ] Đổi DPI/màn hình/vùng làm việc: window khôi phục nằm trong work area; không xác nhận multi-monitor chỉ từ phép tính pure.
- [ ] Không còn record UI hoặc lỗi quyền ghi: dùng mặc định an toàn, không chặn đóng ứng dụng.

Build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi, kể cả build cuối sau đăng ký header trong project; log dist/window-size-final-<configuration>-<architecture>.log. Vị trí cửa sổ và maximize persistence còn mở. ZIP beta trước chưa chứa readback Gõ tắt và window-size mới; chưa làm mới gói ở bước này.

## Đọc lại Gõ tắt sau thao tác ghi — 2026-10-09

MacroPage dùng tryReadEntries khi nạp lần đầu, hiện lỗi đọc thay vì âm thầm coi dữ liệu hỏng là danh sách rỗng. Cả ba đường save/delete/import dùng ReloadAfterWrite; helper chỉ thay snapshot nếu đọc toàn bộ thành công. Khi lần đọc lại thất bại sau khi ghi thành công, caller không RefreshList/ClearEditor/reset search hoặc focus, hiển thị InfoBar Warning Anh/Việt nói rõ thao tác đã hoàn tất nhưng danh sách/draft có thể cũ, hướng dẫn Tải lại và không lặp thao tác. Không rollback dữ liệu đã ghi, không tự retry thao tác ghi. Lần đọc lỗi giữ nguyên snapshot và UI; dữ liệu thật có thể đã khác snapshot đó.

Thêm 3 ca API giả: save/delete/import ghi thành công rồi query bị từ chối, snapshot caller giữ nguyên và không ghi lần hai; khi query hoạt động lại, đọc được đúng dữ liệu mới (kể cả danh sách rỗng sau xóa). Tổng macro 51/51 trên x64/x86 bằng PowerShell 7 và Windows PowerShell 5.1. Chúng xác minh service readback/snapshot, không mô phỏng control/focus/InfoBar. Resource vi/en XML hợp lệ, 396 khóa khớp và không trùng. Build UI Debug/Release x64/x86 đều đạt 0 cảnh báo/0 lỗi; log dist/macro-readback-<configuration>-<architecture>.log.

- [ ] Các thao tác thêm/sửa/xóa/import thường vẫn refresh danh sách, reset editor và focus đúng sau thành công.
- [ ] Cảnh báo readback hiện đúng Anh/Việt, giữ draft/bộ lọc; không tự báo thành công hoặc ghi lại. Không cần cố ý làm hỏng registry thật để test; ca lỗi hiện chỉ có bằng chứng API giả.
- [ ] Tải lại thành công sau cảnh báo cập nhật snapshot; xác nhận bỏ draft vẫn mặc định Hủy.

Các ZIP beta ở mục trước chứa bản bảo vệ storage nhưng chưa chứa thay đổi readback/UI cảnh báo này; chưa làm mới gói trong bước này. Chưa có xác nhận UI runtime mới, không đánh dấu QA thủ công đạt.

## Bảo vệ lưu/xóa/import/export Gõ tắt — 2026-10-09

saveEntry, deleteEntry, import giữ danh sách cũ và export đều dùng tryReadEntries. Lỗi đọc hoặc dữ liệu không hợp lệ chặn thao tác trước khi tạo key/ghi registry; export trả StorageError trước khi mở tệp đích nên không tạo/truncate tệp. MacroPage hiển thị lỗi export đọc dữ liệu riêng với lỗi ghi tệp; thông báo StorageError Anh/Việt bao phủ cả lỗi đọc/ghi/dữ liệu hỏng. Không đổi định dạng binary hoặc file backup legacy.

Import thay thế toàn bộ không cần đọc store cũ: đây vẫn là lựa chọn explicit trong dialog, cho phép khôi phục store lỗi từ backup hợp lệ. Store chưa có key/value là danh sách rỗng hợp lệ; entry legacy có replacement rỗng vẫn đọc và bảo toàn khi thêm entry khác. readEntries legacy và việc đọc snapshot lại sau thao tác thành công trong UI chưa được chuyển ở bước này.

Thêm 8 nhóm test: lỗi open/query/read/type-change chặn add/edit/delete/merge/export và không gọi create/set; payload thiếu/UTF-8 lỗi/trailing bytes/sai kiểu bị chặn và tệp export cũ giữ nguyên; merge store thiếu; save value thiếu; replace khôi phục store hỏng; bảo toàn replacement rỗng; backup rỗng hợp lệ; export đọc lỗi không tạo tệp. Tổng 48/48 trên mỗi x64/x86 bằng PowerShell 7 và Windows PowerShell 5.1. Registry giả, file fixture riêng, không chạm dữ liệu thật. Debug/Release UI x64/x86 0 cảnh báo/0 lỗi; log dist/macro-storage-<configuration>-<architecture>.log. Resource vi/en vẫn khớp 394 khóa.

Giới hạn: không có transaction/lock giữa nhiều writer, không rollback nếu export lỗi sau khi bắt đầu ghi, không kiểm tra dialog/control/hook runtime. Không cố tình sửa registry thật để tạo dữ liệu hỏng. Ca UI thủ công bình thường còn mở: thêm/sửa/xóa, import gộp/thay thế, export và đọc lại backup trên dữ liệu thử riêng; lỗi đọc chỉ được chứng minh bằng API giả.

ZIP beta nội bộ mới, package-beta.ps1 không SkipBuild, Release All 0 cảnh báo/0 lỗi mỗi kiến trúc; verifier PS7 và độc lập PS5.1 đạt đủ 278 payload/gói với architecture/SHA ngoài:

- x64: dist/OpenKey-Fluent-beta-x64-217eeeec1a474703abb0e1674d8a658e.zip — SHA-256 78EA8565E74DDEB85CAFEEACC976EB4FC9970E85BF0D6D72776B729AF9C900AE.
- x86: dist/OpenKey-Fluent-beta-x86-26659096430247a2b61e699719b874f7.zip — SHA-256 22677BB538B0EB0E5DDE58659077E96931E9C703E6933898FA85DB2D568CF71B.

Gói mới thay thế artifact của đợt trước để thử nghiệm; các ZIP cũ giữ nguyên, chưa chạy/cài/phát hành. Các điều kiện máy sạch, ký số, giấy phép và QA UI vẫn mở.

## Đợt regression và hoàn thiện đồng bộ — 2026-10-09

Chuyển mã refresh tùy chọn từ settings mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor, handler weak. Refresh có guard không lưu ngược; tạm dừng khi một dropdown mở hoặc sau lỗi lưu, tiếp tục sau lần lưu thành công. Không sửa văn bản nguồn/kết quả, không tự chuyển mã hay ghi clipboard. Mã phím ngoài danh sách chỉ thêm một lần. Sửa handler để Bỏ dấu tiếng Việt độc lập với nhóm bốn lựa chọn hoa/thường, không làm mất lựa chọn đang bật.

Tải lại Gõ tắt dùng API mới tryReadEntries: key/value chưa có là danh sách rỗng hợp lệ; lỗi quyền đọc/query, sai kiểu registry, payload thiếu/trailing bytes/từ khóa rỗng/UTF-8 lỗi bị từ chối. Chỉ thay snapshot sau khi đọc/parse toàn bộ thành công; nếu thất bại giữ danh sách, bộ lọc và draft, hiện thông báo lỗi. Đây là cải thiện cho reload, thay thế giới hạn đọc lỗi tại mục bên dưới; constructor và CRUD/import/export vẫn dùng readEntries legacy, chưa tuyên bố toàn bộ dịch vụ có transactional persistence.

Regression offline PowerShell 7: engine 20/20, chuyển mã 212/212 (20 engine + 175 văn bản + 17 clipboard giả), settings/ngôn ngữ 172/172, macro 40/40 và updater 31/31 trên mỗi x64/x86. Lần chạy updater x86 trong vòng redirect output gặp lỗi host StandardOutputEncoding; chạy lại riêng đạt đủ 31 ca. Packaging 17/17, prerequisites 6/6 và notices 5/5. Trên Windows PowerShell 5.1, chạy lại settings 172/172, macro 40/40 và chuyển mã 212/212 mỗi kiến trúc, packaging 17/17 đều đạt. Các test registry/clipboard dùng API giả, file fixture riêng; không đổi settings/startup/clipboard thật và không kiểm chứng hook/UI runtime.

Resource vi/en XML hợp lệ: 394 khóa mỗi ngôn ngữ, khớp và không trùng. Log regression: dist/batch-*.log, dist/batch-ps51-*.log. Debug UI cuối cùng x64/x86 build 0 cảnh báo/0 lỗi tại dist/batch-final-Debug-*.log; Release All và gói beta mới được ghi nhận ở phần artifact bên dưới sau xác minh.

- [ ] Chuyển mã: nhập văn bản, đổi tùy chọn từ nguồn ngoài nếu có, kiểm tra refresh không xóa input/result hoặc tự chạy chuyển mã/ghi clipboard.
- [ ] Mở dropdown bảng mã/phím tắt: không tự nhảy lựa chọn khi đang mở; cập nhật sau khi đóng.
- [ ] Bật viết hoa rồi bật Bỏ dấu tiếng Việt: cả hai giữ bật; “tiếng Việt” thành “TIENG VIET”. Thử lại với chữ thường.
- [ ] Gõ tắt Tải lại: Hủy giữ draft; xác nhận đọc thành công reset editor; đọc lỗi giữ snapshot và draft, hiển thị lỗi đúng Anh/Việt.
- [ ] Mở lại UI: mặc định tiếng Việt khi chưa chọn ngôn ngữ, chọn English/Tiếng Việt được giữ khi mở lại; không đổi chế độ gõ của engine.
- [ ] Các ca DPI/cỡ chữ/Narrator/máy sạch và gõ thực tế ở ứng dụng khác còn cần kiểm thử; không suy ra từ build/regression.

Artifact mới trong dist, build bằng package-beta.ps1 không SkipBuild (Release All x64/x86: 0 cảnh báo/0 lỗi), chứa các thay đổi layout, kích thước khởi động, ngôn ngữ UI và đồng bộ đã làm:

- x64: OpenKey-Fluent-beta-x64-a0aa6d17addc4d9f98d3a1daea8d11aa.zip — SHA-256 6EB3519E647954E1EBEE0E6084F32473550324907474CE6D2D978997D1ED1A5D.
- x86: OpenKey-Fluent-beta-x86-659715a9d4e44738977001a66505b019.zip — SHA-256 1025F14A6534B8D1D1889A9ECF931B65479AB27D87C872C114DD2F9E41063350.

Mỗi ZIP xác minh 278 payload bằng verifier PS7 và độc lập PS5.1 với architecture/SHA bên ngoài; hash ba executable trong ZIP khớp output Release. Không giải nén/chạy app từ gói, không cài hoặc phát hành. Gói cũ giữ nguyên. Notices có metadata 9 package, nghĩa vụ giấy phép/runtime redistribution và máy sạch vẫn cần rà soát; đây chỉ là beta nội bộ. Các ghi chú “ZIP beta cũ chưa cập nhật” trong những mục lịch sử bên dưới được thay thế bởi artifact này.

## Đồng bộ tùy chọn và Tải lại danh sách Gõ tắt

MacroPage dùng timer 500 ms khi Loaded để refresh riêng autoCapsMacro, dừng khi Unloaded/destructor, handler weak; guard không lưu ngược. Sau lỗi lưu tùy chọn giữ lựa chọn đang sửa đến khi lưu lại thành công. Timer không đọc lại danh sách hoặc thay ShortcutBox/ReplacementBox, không ghi macro thật.

Nút Tải lại ở header danh sách đọc lại danh sách đã lưu theo yêu cầu, giữ từ khóa tìm kiếm và reset editor/selection. Nếu editor mới có nội dung hoặc khác entry gốc trong snapshot, ContentDialog xác nhận bỏ draft; mặc định Hủy, không đọc lại/reset editor khi hủy. Disable nút trong thao tác để tránh hai dialog tải lại cùng lúc. Reload không xóa hoặc ghi dữ liệu macro đã lưu. MacroService::readEntries hiện không phân biệt đọc lỗi với danh sách rỗng; chưa cải thiện API lỗi đọc ở bước này và không hiển thị thông báo thành công sau reload.

Thêm MacroEditorState::HasUnsavedChanges với 8 test pure: editor trống, draft từ khóa/nội dung, entry không đổi, đổi tên, sửa nội dung, thiếu entry gốc và hoàn nguyên/Unicode. Tổng macro 32/32 mỗi kiến trúc trên PowerShell 7/5.1; settings/ngôn ngữ 172/172 mỗi kiến trúc trên PowerShell 7. Registry fake, file fixture riêng; chưa kiểm tra UI dialog/cancel/timer thật. 393 khóa resource vi/en khớp, không trùng; MacroPage XML hợp lệ và TabIndex 0–10 không trùng.

- [ ] Tự viết hoa cập nhật theo cấu hình ngoài nếu có, không hiện thông báo lưu hoặc thay nội dung đang nhập.
- [ ] Nhập draft mới, bấm Tải lại rồi Hủy: hai ô nhập, selection và bộ lọc giữ nguyên.
- [ ] Sửa tên/nội dung entry đã chọn, Tải lại rồi Hủy: draft giữ nguyên.
- [ ] Xác nhận Tải lại: danh sách/số mục theo dữ liệu lưu, từ khóa lọc giữ nguyên, editor reset; dữ liệu macro đã lưu không bị xóa.
- [ ] Chọn entry không sửa, Tải lại không hỏi bỏ draft; nút hoạt động lại sau tải hoặc hủy, không mở hai dialog tải lại cùng lúc.
- [ ] Tab/focus, bố cục header ở cửa sổ hẹp và tên trợ năng đúng cả Anh/Việt.
- [ ] CRUD/import/export/gõ tắt thực tế vẫn hoạt động; test pure không chứng minh hook runtime.

Debug/Release UI x64/x86 build tuần tự 0 cảnh báo/0 lỗi; log dist/macro-sync-<configuration>-<architecture>.log. Không tự chạy GUI hoặc đổi registry/clipboard/startup thật; các ca thủ công còn mở. ZIP beta cũ chưa cập nhật.

## Hoàn thiện đồng bộ trang Nâng cao

Mở rộng timer hiện có từ chỉ refresh checkSpelling sang bảy toggle Nâng cao: viết hoa đầu câu, cho phép z/w/f/j, Ctrl tạm tắt chính tả, icon đơn sắc, chạy cùng Windows, hiện cửa sổ lúc khởi động và kiểm tra cập nhật. Vẫn refresh mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor và handler weak. Chỉ gán toggle khác giá trị dưới m_loadingPreferences; refresh không gọi lưu settings, engine reload hoặc đăng ký startup.

Nếu lưu thất bại, giữ bảy lựa chọn đang sửa đến lần lưu thành công; riêng checkSpelling vẫn đọc từ cấu hình ngoài và cập nhật hai control phụ thuộc. UiLanguageCombo và thông báo chờ mở lại UI không bị thay bởi refresh. Phần này mở rộng hành vi chỉ đồng bộ phụ thuộc đã mô tả ở mục Chính tả/Nâng cao bên dưới; không đổi keyboard hook, resource hoặc cơ chế áp dụng ngôn ngữ.

- [ ] Mở Nâng cao rồi rời/quay lại: bảy toggle đúng cấu hình đã lưu; không có thông báo lưu tự phát.
- [ ] Đổi “Dùng biểu tượng đơn sắc” trong UI rồi quay lại trang: toggle và biểu tượng tray giữ đúng lựa chọn.
- [ ] Khi cấu hình Nâng cao được thay từ nguồn ngoài nếu có, trang cập nhật mà không tự lưu lại giá trị cũ.
- [ ] Sau chọn ngôn ngữ Anh/Việt, chờ vài nhịp refresh: ComboBox và thông báo mở lại giữ nguyên; engine input mode không bị đổi.
- [ ] Lưu lỗi không làm mất pending edit; bật/tắt chính tả từ tray vẫn cập nhật availability của hai control phụ thuộc.
- [ ] Chỉ mở/refresh trang không thay thiết lập startup hoặc chạy updater. Kiểm thử thay đổi startup thực tế vẫn là ca riêng, chưa xác nhận.

Regression settings/ngôn ngữ 172/172 mỗi kiến trúc x64/x86 đạt bằng API giả trên PowerShell 7. Không tự chạy GUI, thay startup, registry hoặc clipboard thật; các ca thủ công trên còn mở. ZIP beta cũ chưa được cập nhật.

Debug/Release UI x64/x86 build tuần tự đều 0 cảnh báo/0 lỗi; log dist/advanced-sync-<configuration>-<architecture>.log. Build/test không chứng minh control/timer runtime đã đạt.

## Đồng bộ trang Phím tắt

HotkeySettingsPage refresh snapshot mỗi 500 ms khi Loaded; chỉ áp dụng control khi switchKeyStatus thay đổi. ApplyHotkey dùng chung lúc nạp trang và refresh: modifier Ctrl/Alt/Win/Shift, phím chính, âm báo và preview/cảnh báo đều lấy từ cùng status. Mã phím ngoài danh sách có sẵn được thêm theo giá trị, không lặp lại cùng mã qua nhiều lần refresh. Timer weak dừng khi Unloaded/destructor; guard ngăn event control lưu ngược khi nạp.

Tạm ngưng refresh khi KeyCombo đang mở hoặc sau lỗi lưu để giữ lựa chọn đang thao tác; lưu thành công cho phép refresh tiếp. Không sửa encoding hotkey, hook hoặc chế độ gõ; không tự đổi hotkey thật khi kiểm thử. Đây là polling UI, không hoàn thành callback toàn bộ ứng dụng.

- [ ] Mở trang Phím tắt: tổ hợp và âm báo đúng cấu hình hiện tại, không có thông báo lưu tự phát.
- [ ] Thay cấu hình hotkey từ nguồn ngoài nếu có: checkbox/phím/âm báo/preview cập nhật đồng nhất.
- [ ] Giữ dropdown mở trong lúc cấu hình ngoài thay đổi: selection không tự nhảy; cập nhật sau khi đóng.
- [ ] Chỉnh phím/modifier/âm báo trong UI rồi rời/quay lại: trạng thái giữ đúng; hotkey thực tế đổi Việt/Anh đúng, âm báo đúng lựa chọn.
- [ ] Cấu hình phím ngoài danh sách: giữ mã và không thêm mục trùng khi refresh/điều hướng.
- [ ] Lưu lỗi không làm mất lựa chọn đang sửa; đổi một lựa chọn và lưu lại thành công cho phép refresh tiếp.
- [ ] Thay phím tắt không ghi đè kiểu gõ/bảng mã/chế độ Việt-Anh đã đổi từ tray.

Regression settings/ngôn ngữ đạt 172/172 mỗi kiến trúc x64/x86 bằng API giả trên PowerShell 7. Chúng không kiểm tra timer/control/hotkey runtime; các ca thủ công trên còn mở. ZIP beta cũ chưa cập nhật.

Debug/Release UI x64/x86 build tuần tự đều 0 cảnh báo/0 lỗi; log dist/hotkey-sync-<configuration>-<architecture>.log. Chưa tự chạy GUI hoặc thay đổi cấu hình thật.

## Đồng bộ Ứng dụng/Tương thích

CompatibilitySettingsPage refresh bảy tùy chọn từ settings mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor; handler weak. Guard m_loadingPreferences ngăn refresh gọi lưu ngược; không đổi toggle nếu giá trị hiển thị đã đúng. Clipboard bật khi sendKeyStepByStep == 0, giữ mapping đảo hiện có. Sau refresh cập nhật trạng thái IsEnabled của Chromium và thông báo yêu cầu theo tùy chọn sửa lỗi gợi ý, không xóa giá trị Chromium đã lưu khi disable.

Sau lỗi lưu, tạm ngưng refresh để giữ pending edit; một lần lưu lại thành công cho phép refresh tiếp. Không đổi hook/engine, resource, ngôn ngữ UI hoặc startup; không tự ghi settings/clipboard thật trong kiểm thử.

- [ ] Trang Tương thích mở: thay cấu hình tương thích từ nguồn ngoài nếu có, UI phản ánh đúng mà không hiện thông báo lưu tự động.
- [ ] Tắt sửa lỗi gợi ý: Chromium bị disable và hiện thông báo yêu cầu; bật lại: control được enable, giá trị Chromium cũ giữ nguyên.
- [ ] Chọn chế độ Clipboard rồi rời/quay lại trang: toggle phản ánh đúng lựa chọn đã lưu; chưa suy ra gõ thực tế bằng Clipboard trên mọi app đã đạt.
- [ ] Đổi một tùy chọn Tương thích sau khi dùng hotkey/tray đổi Việt/Anh hoặc bảng mã: trạng thái ngoài trang không bị ghi đè.
- [ ] Điều hướng ra/vào nhiều lần không có thông báo lưu tự phát; sau lỗi lưu, pending edit không mất do polling.

Regression settings/ngôn ngữ 172/172 mỗi kiến trúc x64/x86 đạt bằng API giả trên PowerShell 7. Test không bao phủ control, timer/navigation hoặc engine runtime; các ca thủ công trên còn mở. ZIP beta cũ chưa cập nhật.

Debug/Release UI x64/x86 build tuần tự 0 cảnh báo/0 lỗi; log dist/compatibility-sync-<configuration>-<architecture>.log. Chưa tự chạy GUI hoặc thay đổi cấu hình thật.

## Đồng bộ Chính tả/Gõ nhanh và phụ thuộc Nâng cao

SpellingSettingsPage refresh sáu toggle (kiểm tra chính tả, khôi phục phím, đặt dấu hiện đại, Telex nhanh, phụ âm đầu/cuối nhanh) mỗi 500 ms khi Loaded; dừng khi Unloaded/destructor, handler weak. Chỉ gán toggle khác giá trị mới dưới guard m_loadingPreferences, không lưu registry hoặc phát thông báo lưu. Nếu lưu lỗi, giữ lựa chọn đang sửa đến khi lưu thành công hoặc mở lại trang; không ghi đè pending edit bằng polling.

AdvancedSettingsPage có timer cùng vòng đời nhưng chỉ refresh checkSpelling: bật/tắt hai control Cho phép z/w/f/j và Ctrl tạm tắt chính tả, cùng thông báo yêu cầu. Không sửa các toggle Nâng cao khác, lựa chọn ngôn ngữ UI hoặc dữ liệu đang chờ lưu; không tự thay chế độ gõ hay startup. Đây vẫn là polling UI, chưa hoàn thành event/callback toàn bộ app.

- [ ] Trang Chính tả mở: bật/tắt kiểm tra chính tả từ tray; toggle UI cập nhật, không xuất hiện thông báo lưu tự động.
- [ ] Bật/tắt từ UI: tray và gõ thực tế phản ánh đúng; các tùy chọn khác không trở về snapshot cũ.
- [ ] Trang Nâng cao mở: đổi kiểm tra chính tả từ tray; hai control phụ thuộc và thông báo yêu cầu cập nhật đúng; không xóa giá trị đã chọn của hai toggle khi disable.
- [ ] Giữ lựa chọn ngôn ngữ giao diện đã lưu/chờ mở lại; refresh chính tả không thay lựa chọn đó.
- [ ] Điều hướng ra/vào trang nhiều lần: trạng thái mới đúng, không nhiều timer hoạt động hoặc thông báo lưu tự phát.
- [ ] Sau lỗi lưu ở Chính tả, lựa chọn đang sửa không mất do refresh; lưu lại thành công cho phép refresh tiếp.

Regression settings/ngôn ngữ vẫn đạt 172/172 mỗi kiến trúc x64/x86 với API giả trên PowerShell 7. Không sửa hook/engine, không chạy GUI, không ghi settings thật; các ca runtime trên chưa xác nhận. ZIP beta cũ chưa được cập nhật.

Debug/Release UI x64/x86 đã build tuần tự sau guard cuối, đều 0 cảnh báo/0 lỗi. Log ở dist/spelling-sync-<configuration>-<architecture>.log. Chưa xác minh timer/control trong GUI thật.

## Đồng bộ Kiểu gõ/Bảng mã từ tray

InputSettingsPage đọc snapshot mới mỗi 500 ms khi Loaded và dừng timer khi Unloaded/destructor; callback weak tránh giữ page sống sau điều hướng. Cập nhật kiểu gõ, bảng mã, ghi nhớ bảng mã dưới guard m_loadingPreferences, chỉ gán control khi giá trị hiển thị khác. Refresh không lưu hoặc phát thông báo thành công. Timer có thể trễ khi dispatcher bận, không phải cam kết thời gian thực ([Microsoft DispatcherQueueTimer](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.dispatching.dispatcherqueuetimer?view=windows-app-sdk-1.8)).

Không refresh khi ComboBox đang mở hoặc sau lỗi lưu, để không thay lựa chọn đang thao tác/đang chờ retry. Sau một lần lưu thành công, tự refresh tiếp; rời và mở lại trang sẽ đọc settings đang lưu. Chỉ thay UI, không sửa hook/engine, không thay resource hoặc ngôn ngữ mặc định.

- [ ] Để trang Kiểu gõ mở, đổi Telex/VNI/Simple Telex từ tray; ComboBox cập nhật mà không xuất hiện thông báo lưu tự động.
- [ ] Đổi bảng mã từ tray; UI phản ánh bảng mã hiện tại. Khi bật ghi nhớ theo ứng dụng, chuyển focus có thể khôi phục bảng mã riêng của ứng dụng; kiểm tra riêng cả bật/tắt ghi nhớ, không coi cơ chế này là lỗi refresh.
- [ ] Mở dropdown trong UI trong lúc trạng thái ngoài thay đổi: selection không tự nhảy khi dropdown mở, cập nhật sau khi đóng.
- [ ] Đổi trong UI: lựa chọn lưu đúng, tray/gõ thực tế đúng, chế độ Việt/Anh không bị ghi đè.
- [ ] Điều hướng sang trang khác rồi quay lại: trạng thái mới đúng; không nhiều timer hoạt động hoặc thông báo lưu do refresh.
- [ ] Khi lưu thất bại, lựa chọn đang sửa không tự mất do polling; lần lưu lại thành công cho phép refresh tiếp.

172 ca settings/ngôn ngữ mỗi kiến trúc x64/x86 vẫn đạt với API giả trên PowerShell 7. Chúng không kiểm tra timer, navigation, control hoặc tray runtime. Các ca thủ công trên chưa xác nhận; ZIP beta cũ chưa cập nhật.

Debug/Release UI x64/x86 build tuần tự 0 cảnh báo/0 lỗi; log dist/input-sync-<configuration>-<architecture>.log. Chưa tự chạy GUI hoặc ghi settings thật.

## Ngôn ngữ giao diện — mặc định tiếng Việt

App đặt Microsoft.Windows.Globalization.ApplicationLanguages.PrimaryLanguageOverride trước khi tải XAML/resource. Khi uiDisplayLanguage chưa có, không đọc được, sai kiểu/kích thước hoặc ngoài 0/1, dùng vi-VN. Nâng cao → Ngôn ngữ giao diện có Tiếng Việt/English; lựa chọn lưu một DWORD uiDisplayLanguage tại HKCU\SOFTWARE\TuyenMai\OpenKey, không đổi vLanguage, settings engine hay macro và không gửi reload engine. Đóng rồi mở lại UI để áp dụng; không tự đóng cửa sổ hoặc mất văn bản đang nhập. Override của app unpackaged cần được đặt lại mỗi lần chạy, và resource đã tải không bảo đảm tự đổi: [tài liệu Microsoft](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.windows.globalization.applicationlanguages.primarylanguageoverride?view=windows-app-sdk-1.6).

Hai bộ vi-VN/en-US có 389 khóa khớp và không trùng; XAML mới parse XML hợp lệ. 12 ca storage mới đạt, tổng 172 ca settings/ngôn ngữ mỗi kiến trúc x64/x86 trên PowerShell 7 và 5.1. Test dùng API giả, không ghi registry thật. ZIP beta cũ chưa được cập nhật.

Debug/Release UI x64/x86 build tuần tự 0 cảnh báo/0 lỗi; log dist/ui-language-<configuration>-<architecture>.log. Chưa tự xác minh resource đang hiển thị trong GUI; các ca thủ công dưới đây còn mở.

- [ ] Chưa có lựa chọn ngôn ngữ UI: mở trên Windows tiếng Anh vẫn thấy giao diện tiếng Việt (không xóa registry thật chỉ để thử).
- [ ] Chọn English, đóng/mở lại UI: điều hướng, nhãn tĩnh, thông báo động đều tiếng Anh; lựa chọn giữ qua lần mở sau.
- [ ] Chọn Tiếng Việt, đóng/mở lại UI: toàn bộ nhãn/thông báo trở lại tiếng Việt, dấu hiển thị đúng.
- [ ] Việc đổi ngôn ngữ UI không đổi chế độ gõ, bảng mã, kiểu gõ hoặc macro; engine ở tray tiếp tục chạy.
- [ ] Tab/focus và tên trợ năng của ComboBox đúng cả hai ngôn ngữ.

## Lưu chọn lọc, không ghi đè snapshot cũ — 09/10/2026

Cả 8 trang dùng SettingsService::saveChangedPreferences, so sánh với snapshot của trang và chỉ ghi registry field đã sửa; snapshot tiến lên khi lưu thành công. Không thay API lưu đầy đủ của legacy. Không có thay đổi thì không tạo registry key hoặc gửi thông báo reload engine. Lỗi ghi giữ snapshot để retry; nhiều trường không phải giao dịch nguyên tử và hai tiến trình cùng sửa một trường vẫn là last-writer-wins. Không thêm thao tác registry/clipboard/startup thật vào test.

Thêm 40 ca store: từng field trong 35 field bảo toàn các giá trị ngoài trang đã thay đổi; no-op, create failure, partial failure/retry, lỗi trường không sửa và không có engine window. Tổng settings 81 store + 41 mapping + 38 defaults = 160/160 mỗi kiến trúc x64/x86 trên PowerShell 7 và 5.1. Guard event lúc nạp trang Kiểu gõ/Chính tả trước khi sửa model, tránh giá trị control chưa khởi tạo làm lệch snapshot.

- [ ] Để trang Chính tả mở, đổi Việt/Anh hoặc bảng mã ở tray; bật/tắt một tùy chọn Chính tả, kiểm tra trạng thái/bảng mã mới vẫn giữ nguyên.
- [ ] Tương tự ở Kiểu gõ, Phím tắt, Tương thích, Nâng cao, Gõ tắt và Chuyển mã: chỉ lựa chọn vừa sửa thay đổi.
- [ ] Mở lại Chính tả và kiểm tra các toggle đúng settings đã lưu, không thay đổi chỉ vì mở trang.
- [ ] Kiểm tra thông báo lỗi/lưu và trạng thái engine thực tế; test API giả không chứng minh runtime UI đạt.

ZIP beta cũ chưa chứa thay đổi này.

Debug/Release UI x64/x86 build tuần tự 0 cảnh báo/0 lỗi, log dist/settings-delta-<configuration>-<architecture>.log. Chưa tự chạy GUI hoặc xác nhận các ca thủ công trên.

## Gõ tiếng Việt và đồng bộ trạng thái — 09/10/2026

- [x] Người dùng xác nhận ca gõ tiếng Việt đã hướng dẫn (Notepad, Telex/Unicode) hoạt động tốt. Đây là xác nhận thủ công, không suy ra toàn bộ ứng dụng/OS/kiến trúc đã đạt.
- [ ] Mở trang Tổng quan, đổi Việt/Anh bằng hotkey: toggle và mô tả cập nhật trong khoảng 0,5 giây khi UI không bận.
- [ ] Đổi Việt/Anh từ tray, rồi kiểm tra Tổng quan; đổi từ UI và kiểm tra tray/gõ thực tế.
- [ ] Rời Tổng quan rồi quay lại: trạng thái mới đúng, không tự ghi settings hoặc phát thông báo lưu khi refresh.
- [ ] Đổi bảng mã/kiểu gõ ở tray trong lúc Tổng quan mở, sau đó bật/tắt ở Tổng quan: lựa chọn khác không bị trả về snapshot cũ.

OverviewPage dùng DispatcherQueueTimer khi Loaded và dừng khi Unloaded/destructor; callback weak không giữ page sống sau điều hướng. Refresh chỉ đọc settings, chặn Toggled lưu ngược. Trước thao tác lưu đọc lại snapshot để giữ các lựa chọn hiện tại; chưa phải giao dịch nguyên tử giữa hai tiến trình. Các trang khác và event/callback đồng bộ tổng quát vẫn còn trong checklist.

Debug/Release x64/x86 build tuần tự đều 0 cảnh báo/0 lỗi (dist/overview-sync-*.log); test-settings đạt 120/120 mỗi kiến trúc bằng API giả. Các test này không kiểm tra timer/navigation/hotkey/tray runtime; các ca thủ công ở trên vẫn chưa đánh dấu đạt. Không thay keyboard hook hoặc tự chạy engine; ZIP beta cũ chưa được cập nhật.

## Smoke test Chuyển mã — người dùng xác nhận 09/10/2026

- [x] Ảnh sau sửa: nội dung không còn khoảng trống lớn bên trái, nút chính hiển thị Convert; theme tối/giao diện tiếng Anh. Phần cài đặt phía dưới dùng cuộn dọc.
- [x] Ảnh kết quả Unicode → Unicode, viết hoa: `tiếng Việt` → `TIẾNG VIỆT`, dấu tiếng Việt hiển thị đúng; các nút Convert/Copy/Clear được bật.
- [x] Ảnh kết quả viết hoa + bỏ dấu: `tiếng Việt` → `TIENG VIET`.
- [x] Người dùng xác nhận Copy rồi dán Notepad được `TIENG VIET`.
- [x] Người dùng xác nhận sao chép `tiếng Việt` trong Notepad, Convert Clipboard với hai tùy chọn trên rồi dán lại được `TIENG VIET`.
- [x] Người dùng xác nhận Clear xóa hai vùng văn bản và tắt Convert/Copy/Clear.
- [ ] Xác minh cửa sổ hẹp, cuộn toàn trang và thao tác bàn phím.

Các ca clipboard/Clear ghi theo xác nhận của người dùng, không phải agent tự điều khiển Windows. Chưa xác định DPI, cỡ chữ, kiến trúc runtime hoặc phiên bản OS từ ảnh; không suy ra ma trận đầy đủ, CF_HTML, clipboard lock/delayed rendering, screen reader hoặc các bảng mã legacy đã đạt.

## Bố cục, DPI và cỡ chữ

Theo yêu cầu giảm nhẹ kích thước lúc mở, MainWindow dùng kích thước mong muốn 1120 × 800 DIP thay 1160 × 880 DIP. Giữ nguyên scale theo DPI, căn giữa work area và giới hạn kích thước với lề màn hình 24 DIP; không thay responsive breakpoint, dữ liệu settings hoặc startup. Nội dung dài vẫn cuộn dọc. Release x64 build 0 cảnh báo/0 lỗi; cần mở lại app để xác nhận kích thước thực tế trên màn hình/DPI của người dùng. Gói ZIP beta cũ không tự cập nhật binary.

Theo yêu cầu chỉnh bố cục 7 trang trước khi tiếp tục QA, đã cập nhật Tổng quan, Kiểu gõ/Bảng mã, Chính tả/Gõ nhanh, Phím tắt, Ứng dụng/Tương thích, Nâng cao và Gõ tắt. Các container MaxWidth=760/padding 40,32,40,48 được thay bằng PageStackLayoutStyle/PageGridLayoutStyle trong App.xaml, stretch theo viewport, lề 24,24,24,32 DIP và khoảng cách nhóm 20 DIP. Trang Chuyển mã cũng dùng style chung để giữ cùng mép trái/spacing; không đổi business logic, event, tab index, resource ngôn ngữ hoặc settings. Giữ các adaptive state hai cột/một cột và cuộn dọc, không ép toàn bộ nội dung vào một viewport.

Debug/Release x64/x86 đã build tuần tự, 0 cảnh báo/0 lỗi. Kiểm tra XML 8/8 container không có Width/MaxWidth cố định, style chung hợp lệ và tên đích adaptive setter có tồn tại. Log ở dist/layout-pages-44bd1b130d8d4ea9a0b712f59e57d0c4.log. Chưa có ảnh 7 trang sau sửa hoặc test cửa sổ hẹp/DPI; chưa chuyển sang bước QA chức năng tiếp theo. Notification Snipping Tool xuất hiện trên ảnh gốc là overlay của Windows, không được coi là lỗi layout của app. ZIP beta cũ chưa chứa layout mới.

Ảnh người dùng cung cấp của trang Chuyển mã (theme tối, giao diện tiếng Anh, chưa nhập văn bản) cho thấy nhãn/control đọc được và nút Convert/Copy/Clear đang disabled phù hợp trạng thái rỗng. Khoảng trống lớn bên trái và phần quick clipboard phía dưới chưa nằm trọn khung ảnh cần kiểm tra layout/cuộn. Không xác định DPI/cỡ chữ từ ảnh, không suy ra tiếng Việt, Narrator, thao tác nút hoặc clipboard đã đạt.

Đã chỉnh riêng ConvertPage: bỏ MaxWidth=900 của StackPanel nội dung, stretch theo viewport; padding từ 40,32,40,48 thành 24,24,24,32; hai TextBox từ 230 xuống 180 DIP để giảm cuộn. Đổi resource en-US của nút chính từ Encoding converter thành Convert, giữ tên trang và 383 khóa vi/en khớp nhau. Debug/Release x64 build 0 cảnh báo/0 lỗi; chưa có ảnh xác nhận sau sửa. Nội dung dài vẫn dùng cuộn dọc, không hứa toàn bộ trang cùng hiện trong mọi kích thước cửa sổ. ZIP beta tạo trước lượt chỉnh layout không tự cập nhật.

- [ ] Chạy `./run-ui-debug.ps1`, mở lần lượt tất cả trang điều hướng.
- [ ] Thu nhỏ/phóng rộng cửa sổ qua mốc 1100 DIP: Tổng quan, Gõ tắt, Chuyển mã và Giới thiệu chuyển giữa bố cục xếp dọc và nhiều cột, không chồng lấn.
- [ ] Kiểm tra DPI 100%, 125%, 150%, 200% trong Windows Settings > System > Display; mở lại ứng dụng sau mỗi thay đổi.
- [ ] Kiểm tra Text size 100%, 150%, 200% trong Windows Settings > Accessibility > Text size.
- [ ] Kiểm tra phối hợp DPI 200% và Text size 200%: mọi nội dung vẫn đọc được qua cuộn dọc; không mất nút hay xuất hiện thanh cuộn ngang.
- [ ] Kiểm tra danh sách và trình sửa Gõ tắt, thanh tìm kiếm/nhập/xuất, vùng nhập/kết quả Chuyển mã, mô tả phím tắt dài.
- [ ] Di chuyển cửa sổ giữa hai màn hình có DPI khác nhau nếu có; chữ và điều khiển không bị cắt sau khi chuyển.

## Tiếng Anh và trợ năng

Sau giảm kích thước mặc định xuống 1120 × 800 DIP, Debug và Release x64 đều build 0 cảnh báo/0 lỗi; chưa chạy GUI để xác nhận kích thước mới. Không suy ra đã kiểm thử DPI hoặc cập nhật ZIP beta.

Xác minh compile sau chỉnh ConvertPage: Debug/Release x64 và x86 đều build 0 cảnh báo/0 lỗi. Không thay engine/service hay ghi settings, không chạy clipboard; cần mở lại ứng dụng để xem resource/layout mới. Ma trận ảnh/DPI/trợ năng vẫn chưa hoàn tất.

- [ ] Chọn English trong Nâng cao → Ngôn ngữ giao diện rồi đóng/mở lại UI; kiểm tra nhãn tĩnh và thông báo lưu/chuyển mã động, không còn khóa resource hoặc chuỗi lỗi mã hóa. Ngôn ngữ Windows không quyết định lựa chọn UI.
- [ ] Dùng Tab/Shift+Tab, Space/Enter và phím mũi tên để thao tác toàn bộ trang; focus luôn nhìn thấy và không mắc kẹt.
- [ ] Bật Narrator bằng Win+Ctrl+Enter; kiểm tra tên, trạng thái bật/tắt, lựa chọn và mô tả các điều khiển, gồm điều hướng và hộp thoại.
- [ ] Bật một Contrast theme; kiểm tra chữ, biểu tượng, focus và trạng thái lỗi/thành công đều phân biệt được.

## Tiêu chí ghi nhận

Chỉ đánh dấu đã kiểm thử khi thao tác trực tiếp thành công. Build sạch và kiểm tra XML không thay thế kiểm thử hiển thị hoặc screen reader. Khi gặp lỗi, ghi trang, kích thước cửa sổ, DPI, Text size, ngôn ngữ và ảnh chụp.

Hiện tại: đã có ảnh và xác nhận người dùng cho smoke test Chuyển mã phía trên. Agent vẫn không tự điều khiển Windows vì công cụ báo `native pipe is unavailable`; các ca còn lại tiếp tục chờ kiểm thử.

Kiểm tra kỹ thuật đã đạt: XML và đích VisualState của 11 file XAML hợp lệ; build Debug x64, Debug x86 và Release x64 thành công; UI Debug x64 khởi động, phản hồi và đóng bình thường với mã thoát 0.

## Static analysis

Chạy `./build.ps1 Debug x64 UI -Analyze` để rebuild kèm MSVC code analysis. Lượt Debug x64 đã phân tích UI và các service dùng chung, phát hiện rồi sửa hai cảnh báo C6387 và một C6262; lượt kiểm tra lại không còn cảnh báo.

Engine/tray legacy Debug x64 đã được phân tích: 84 → 77 sau sửa clipboard/mở UI/thông báo tray → 13 sau khởi tạo handle Win32 → 9 sau sửa startup/COM/đường dẫn Gõ tắt → 0 sau khởi tạo hook state/sửa chỉ số nguyên âm thứ hai/loại so sánh Enter trùng trên Windows. Tiếp đó đã sửa 36 cảnh báo C4018 của x86 bằng chuyển kiểu rõ ràng ở so sánh chỉ số/kích thước; không đổi kiểu các chỉ số dùng khi duyệt ngược. Lượt rebuild kèm MSVC NativeRecommendedRules cuối đạt 0 cảnh báo, 0 lỗi cho cả Debug x64, Debug x86, Release x64 và Release x86. Các log nằm trong thư mục build bị Git bỏ qua.

UI: rebuild kèm static analysis đã đạt 0 cảnh báo, 0 lỗi trên Debug x64, Debug x86, Release x64 và Release x86, gồm lượt chạy lại Debug x64 trên mã cuối cùng. Không cần sửa mã UI trong lượt kiểm tra này. Các cấu hình chạy tuần tự vì dùng chung Generated Files.

Updater: rebuild kèm static analysis đạt 0 cảnh báo, 0 lỗi trên Debug/Release x64/x86. Đã thay WinExec bằng CreateProcessW với đường dẫn PowerShell hệ thống, chờ tiến trình giải nén và kiểm tra exit code; mỗi lần dùng thư mục staging riêng. Không xóa executable đang cài trước khi giải nén; kiểm tra MoveFileExW trước khi báo thành công. Handle tiến trình và worker được đóng, HWND dialog được truyền trực tiếp cho worker.

Kiểm thử helper thật bằng `./test-updater.ps1 x64` và `./test-updater.ps1 x86`: 6/6 ca đạt trên mỗi kiến trúc (gói hợp lệ, thiếu executable, ZIP hỏng, đích đang khóa, staging tồn tại, đường dẫn tiếng Việt/khoảng trắng). Ca thành công kiểm tra byte tệp mới; ca lỗi kiểm tra byte tệp cũ không đổi. Harness include mã updater nhưng chỉ gọi InstallDownloadedUpdate, không gọi entry point tải mạng/UI. Fixture và compiler output giữ trong thư mục Debug bị Git bỏ qua; không xóa tệp người dùng.

Kiểm thử phát hiện Windows PowerShell không tự nạp được Archive khi kế thừa PSModulePath từ PowerShell 7. Đã chỉ định module Archive bên dưới PSHOME của Windows PowerShell, rồi chạy lại không có instrumentation chẩn đoán.

Metadata updater: thêm 16 ca kiểm thử (section Windows/Mac, whitespace/thứ tự key, thiếu section/key/colon/quote, sai kiểu, tag rỗng/quá dài, path traversal/query, key trùng, dữ liệu quá lớn). Đạt cùng 6 fixture trên x64/x86. Extractor kiểm tra section/key trước khi lấy substring, giới hạn tag 64 ký tự và tập ký tự ASCII an toàn cho release URL; đọc tối đa 64 Ki ký tự metadata. Đường dẫn/URL dùng wstring thay bộ đệm wsprintf cố định. Static analysis bốn cấu hình vẫn 0 cảnh báo, 0 lỗi. Không khẳng định hỗ trợ đường dẫn dài end-to-end: URLDownloadToFile và Windows PowerShell còn giới hạn riêng.

Updater mới đã chọn engine theo kiến trúc build: OpenKey64.exe cho x64, OpenKey32.exe cho x86. Kiểm tra DOS/PE signature và machine trước MoveFileExW, giới hạn offset PE; đây chỉ là sanity check kiến trúc, không xác minh chữ ký hay chứng minh executable chạy được. Đối chiếu cấu trúc với [PE format của Microsoft](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format). Engine/updater phải cùng kiến trúc; không hỗ trợ ghép updater x86 với engine x64 của layout legacy khác.

10 fixture + 16 metadata đạt trên mỗi x64/x86. Thêm wrong-name, wrong-machine, invalid-PE và ZIP có cả hai engine: chỉ engine đúng được thay, tệp engine còn lại giữ nguyên byte. Fixture PE chỉ gồm header giả và payload marker; không chạy binary hoặc cập nhật thật.

Mã updater cuối cùng rebuild/static analysis trên Debug/Release x64/Win32 đều 0 cảnh báo/0 lỗi. Đã chạy lại 26 ca trên mỗi kiến trúc và gom updater Release mới vào ZIP beta x64/x86, 278 payload mỗi gói được verifier kiểm tra. Engine/UI không bị sửa hoặc khởi chạy trong lượt này.

Chưa chạy cập nhật thật, kiểm thử giao diện updater hoặc giải nén chậm. Updater vẫn tải stable và chỉ thay engine, không cập nhật đồng bộ Fluent UI/runtime. Thiết kế hiện tại chưa kiểm tra chữ ký gói. Extractor metadata chỉ hỗ trợ section phẳng của schema hiện có, không xác thực toàn bộ JSON; cần parser JSON đầy đủ nếu mở rộng schema.

Kết quả sạch cảnh báo áp dụng cho mức warning/ruleset hiện tại của project, không thay thế kiểm thử runtime hay kiểm tra bảo mật. Narrator, High Contrast và DPI/font scaling vẫn chờ kiểm thử trực quan.

Chạy `./test-engine.ps1` hoặc `./test-engine.ps1 x86`: biên dịch và chạy engine thật trong console, không cài hook, không sửa registry hay clipboard. 20 ca hồi quy gồm Telex/VNI, ia/ua/gi/qu, dấu kiểu cũ/hiện đại và các chữ có dấu mũ/móc; đã đạt trước và sau sửa. Trạng thái hook mặc định cũng được kiểm tra các trường scalar/buffer bằng 0 và vector rỗng trên cả x64/x86. Những ca này không thay thế kiểm thử gõ trực tiếp, macro, mọi bảng mã hay toàn bộ từ tiếng Việt.

Parser Windows đã kiểm tra dạng lệnh `/tr` với đường dẫn thường, khoảng trắng và tiếng Việt: đều giữ được dấu ngoặc kép bao quanh executable. Đây không phải kiểm thử Task Scheduler end-to-end; không tạo/xóa task hoặc shortcut trong lượt kiểm tra.

- [ ] Kiểm thử startup admin với thư mục cài đặt chứa khoảng trắng/tiếng Việt, rồi đăng nhập lại.
- [ ] Tạo shortcut desktop và mở qua shortcut; kiểm tra bật/tắt tùy chọn nhiều lần.
- [ ] Nhập/xuất Gõ tắt ở đường dẫn gần MAX_PATH, thử tạo tệp mới và xác nhận ghi đè tệp hiện có.

Các sửa đổi Win32 đối chiếu với tài liệu Microsoft: [kích thước bộ đệm hộp thoại tệp](https://learn.microsoft.com/en-us/windows/win32/dlgbox/open-and-save-as-dialog-boxes), [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), [cân bằng COM initialization](https://learn.microsoft.com/en-us/windows/win32/api/objbase/nf-objbase-coinitialize).

## Hồi quy clipboard sau sửa engine/tray

- [ ] Với chế độ gửi qua clipboard, gõ Telex/VNI và thử mở rộng Gõ tắt trong Notepad.
- [ ] Chuyển mã nhanh với clipboard chỉ chứa Unicode; không tạo thêm định dạng HTML rỗng.
- [ ] Chuyển mã nhanh với clipboard có HTML và Unicode; kiểm tra cả hai kết quả sau khi dán.
- [ ] Clipboard chứa ảnh hoặc không có văn bản: báo thất bại, không xóa nội dung gốc.
- [ ] Clipboard bị ứng dụng khác giữ khóa: không crash và không báo chuyển mã thành công.
- [ ] Kiểm thử thông báo tray dài: cắt ngắn an toàn, ứng dụng vẫn phản hồi.

Các kiểm thử thao tác trực tiếp này chưa được thực hiện trong lượt static analysis. Việc cấp phát thất bại được xử lý trong mã trước khi xóa clipboard; lỗi SetClipboardData xảy ra sau khi xóa vẫn có thể để lại dữ liệu thiếu và được trả về là thất bại.

## Kiểm thử chuyển mã thuần văn bản

Sau sửa clipboard: UI rebuild với RunCodeAnalysis=true trên Debug/Release x64/Win32 đã hoàn tất thành công. ConvertService.nativecodeanalysis.xml của cả bốn cấu hình không có defect; binary UI được cập nhật. Test-convert chạy lại 20 engine + 175 text + 17 clipboard fake trên mỗi kiến trúc, đạt trong PowerShell 7 và Windows PowerShell 5.1. Chưa chạy UI hoặc thao tác clipboard thực tế; không suy ra fake test thay thế test Win32/WinUI trên ứng dụng đích.

Clipboard service Fluent: sửa OpenClipboard(nullptr) thành owner GetActiveWindow của UI thread, thiếu owner trả ClipboardUnavailable mà không mở/xóa clipboard. Theo [Microsoft SetClipboardData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata), mở với owner NULL rồi EmptyClipboard khiến việc SetClipboardData thất bại. Service hiện dùng RAII đóng clipboard/unlock/free, đọc có giới hạn GlobalSize và terminator, kiểm tra bảng mã trước mở và cấp phát/lock tất cả output trước xóa dữ liệu cũ.

17/17 ca clipboard fake đạt trên mỗi x64/x86 bằng test-convert.ps1: Unicode/emoji, UTF-8 trong format HTML đăng ký, hai format, đang bị khóa/không có owner/không có text/non-text, thiếu terminator/read-lock lỗi, allocation đầu/thứ hai lỗi/output-lock lỗi/EmptyClipboard lỗi giữ dữ liệu cũ, SetClipboardData lỗi giải phóng memory/partial write và options sai. Fake kiểm tra close/lock/ownership, toàn bộ API clipboard và global memory được thay thế trong harness; không chạm clipboard Windows. Lỗi SetClipboardData sau EmptyClipboard vẫn có thể để lại thiếu dữ liệu và trả WriteFailed, không rollback. HTML fixture không phải CF_HTML đầy đủ: chưa kiểm tra header/byte offsets, delayed rendering, owner WinUI hoặc hành vi ứng dụng đích. Các ZIP beta tạo trước lượt sửa này chưa chứa binary mới.

`./test-convert.ps1 x64`/`x86` build C++20 với mã `ConvertService.cpp`, engine, Vietnamese tables và `ConvertTool.cpp` thật. 20/20 engine regression được chạy trước, rồi 175/175 ca chuyển mã đạt trên mỗi kiến trúc. Các API clipboard/memory dùng trong service được thay bằng trap ném lỗi nếu gọi; harness chỉ gọi convert/isValidCodeTable gián tiếp, không chạy convertClipboard, hook, registry hoặc SendInput.

150 ca so sánh 5 bảng mã nguồn × 5 bảng mã đích × 6 chế độ riêng (mặc định/hoa/thường/đầu câu/đầu từ/bỏ dấu) với legacy trên corpus tiếng Việt, dấu câu, xuống dòng, ASCII, emoji. Corpus legacy có newline cuối để tránh nhánh output packed ở ký tự cuối của legacy; chỉ bỏ một NUL terminator do convertUtil thêm, không coi engine legacy là oracle đầy đủ. 25 ca còn lại có golden Unicode casing/bỏ dấu, empty/plain emoji, 6 bảng mã không hợp lệ, 5 round-trip và 5 round-trip ký tự có dấu ở cuối, embedded NUL và chuỗi 55.000 UTF-16 code unit. Không kiểm thử mọi tổ hợp tùy chọn xung đột, mọi ký tự/bảng mã, dữ liệu clipboard HTML hay malformed Unicode. Không sửa mã production trong lượt này.

## Kiểm thử dịch vụ macro độc lập

`./test-macro.ps1 x64`/`x86` compile `MacroService.cpp` thật trong harness C++20, thay 5 API registry bằng fake trong bộ nhớ. 24/24 ca đạt trên mỗi kiến trúc, /W4 không cảnh báo. File fixture được ghi vào thư mục GUID mới dưới Release/MacroTests-<architecture> và giữ lại, Git bỏ qua; không truy cập macroData thật hoặc gọi hook/UI.

Các ca bao phủ thêm/sửa/rename/xóa, trùng/rename xung đột giữ blob cũ, trường rỗng, tiếng Việt/emoji/Unicode tổ hợp, UTF-16 lỗi, shortcut 255/258 byte và replacement 65535/65536 byte, layout binary legacy đơn giản, sai registry type và các prefix bị cắt của một record. Import kiểm tra BOM/CRLF, shortcut bắt đầu bằng dấu hai chấm, merge/replace, duplicate/malformed/UTF-8 sai, header sai/không có entry hợp lệ/file thiếu/lỗi storage. Export round-trip giữ byte blob; nội dung multiline bị từ chối trước truncate file đích, đường dẫn không tồn tại báo lỗi. Đây không phải fuzz test, test cạnh tranh registry, fixture macro thật của người dùng hoặc kiểm thử mở dialog/keyboard hook; fake registry không mô phỏng mọi lỗi đọc/ghi Windows.

## Kiểm thử settings store độc lập

Khôi phục mặc định: harness gọi `SettingsService::restoreDefaults` thật qua helper ghi giả; 38/38 ca đạt trên mỗi x64/x86, cùng 41 store + 41 load case (120 tổng). Kiểm tra 34 trường được ghi và globals tương ứng, free-mark về 0 nhưng không ghi khóa vFreeMark, giữ 8 record không liên quan/không reset (gồm macroData kiểu REG_BINARY), không thêm/xóa record, reset lặp lại giữ cùng dữ liệu và load lại khớp những gì đã lưu. Các record giả không thay thế kiểm thử blob macro thật hoặc registry thật.

Hành vi legacy được pin, không sửa: reset vUseMacro về 0 trong khi load thiếu khóa dùng 1; reset vSupportMetroApp về 1 trong khi model/load thiếu khóa dùng 0; hotkey reset dùng DefaultSwitchKeyStatus khác fallback load. vQuickStartConsonant/vQuickEndConsonant/vCheckNewVersion/vRunAsAdmin/vCreateDesktopShortcut giữ nguyên. Helper setRegInt trả void, nên harness không chứng minh báo lỗi ghi hoặc transaction/rollback; không gọi AppDelegate, startup/task/shortcut hoặc phát message runtime. Những khác biệt mặc định cần quyết định UX riêng trước khi thay đổi.

Mở rộng harness: `test-settings.ps1` hiện compile `SettingsMappingTests.cpp`, gọi lại 41 store case rồi chạy 41 load mapping case trên mỗi x64/x86 (/W4 không cảnh báo). Mã `SettingsService::load` thật được include, globals dùng đúng kiểu header nhưng định nghĩa độc lập; `OpenKeyHelper::getRegInt/setRegInt` được thay bằng helper giả, không link helper production/hook/engine handler. 35 trường được thử 0/1/37/257/DWORD bit cao, kiểm tra toàn bộ globals sau từng thay đổi và nạp lại store rỗng; 4 tùy chọn ngoài model, store không đọc được và giá trị mặc định được kiểm tra. Bool dùng !=0, Uint8 có phép thu hẹp hiện tại, hotkey 0 đổi thành EmptyHotKey, free-mark luôn 0. Không có ghi helper hoặc thông báo khi load.

Fallback load vSwitchKeyStatus vẫn là 0x7A000206, khác Preferences/restoreDefaults 0x5A00025A. Test pin hành vi hiện có, không xác nhận đây là lựa chọn UX mong muốn hoặc thay mặc định. Nhận message thực tế, tray/UI và registry/startup vẫn cần kiểm thử riêng. Những giá trị ngoài miền hợp lệ trong harness chỉ kiểm tra phép chuyển kiểu hiện tại, không khẳng định cấu hình đó được UI hỗ trợ.

`./test-settings.ps1 x64` và `x86` compile mã `SettingsServiceStore.cpp` thật trong harness, thay toàn bộ API registry/FindWindow/PostMessage bằng fake trong bộ nhớ. 41/41 ca đạt trên mỗi kiến trúc với /W4 không cảnh báo: 35 trường kiểm tra tên khóa legacy, fallback thiếu/type/size sai và round-trip 0/1/37/DWORD bit cao; 6 ca kiểm tra lỗi mở/tạo/ghi đầu/ghi cuối, không có cửa sổ engine và giữ dữ liệu macro/khóa không liên quan. Handle được đóng và thông báo chỉ phát khi tất cả ghi thành công. Lỗi ghi có thể để lại một phần thay đổi: store không có transaction/rollback, test không tuyên bố tính nguyên tử. Chưa kiểm thử registry thật, mapping Preferences sang globals trong load, hoặc engine nhận thông báo runtime.

## Benchmark engine độc lập

`./benchmark-engine.ps1 x64` và `x86` build harness với MSVC /O2 /DNDEBUG, gọi engine trực tiếp sau khi 20/20 regression case đạt. Mỗi kiến trúc chạy đủ 4 scenario Telex/VNI × đặt dấu cũ/hiện đại, warm-up 20 cycle rồi 100 batch × 200 cycle với 6 từ và phím Space. Macro/smart-switch/quick-consonants tắt, spelling/free-mark bật theo standalone globals. Không link hook Win32, UI, clipboard hoặc registry.

Report JSON giữ ở Release/EngineBenchmark-<architecture> bị Git bỏ qua, có event count, mean, p50/p95/p99 batch average tính theo microsecond/event. Setup/reset được làm ngoài vùng đo mỗi batch; boundary Space và engine bookkeeping nằm trong vùng đo. Percentile của batch average không phải percentile độ trễ từng phím. Máy có scheduling/load/thermal noise; chưa có baseline lịch sử và chưa chứng minh không tăng độ trễ gõ hoặc UI không chặn hook. Khởi động nguội, tray memory và end-to-end SendInput/ứng dụng đích vẫn cần phép đo riêng.

## Đóng gói beta nội bộ

Sau sửa clipboard Fluent, chạy package-beta.ps1 x64/x86 không SkipBuild: Release All hoàn tất 0 cảnh báo/0 lỗi và mỗi ZIP có 278 payload. Gói mới x64 là `dist/OpenKey-Fluent-beta-x64-8c2d1f13db404fd8bf9cd04b4f94e9b8.zip`, SHA-256 `D8243A3E6501FCCD8D185BE9CBF9A85946EC3709751F45AB75359734D4EFC0A5`; x86 là `dist/OpenKey-Fluent-beta-x86-ace629c988bd4d8292633bf47723aecf.zip`, SHA-256 `A8EF4570FB030B9BC6F382E9C4C64BD83EF551641D094E2DB10DA40B39EF2844`. Verifier tích hợp PowerShell 7 và verifier độc lập Windows PowerShell 5.1 với ExpectedArchitecture/ExpectedSha256 đều đạt. Hash payload của engine/updater/UI trong manifest khớp ba executable Release tương ứng, skippedBuild=false; không giải nén/load/chạy binary. 17 ca verifier tổng hợp chạy lại đạt trên PowerShell 7/5.1.

Hai ZIP mới chứa UI sau sửa clipboard; các ZIP trước đó vẫn được giữ nguyên và không tự cập nhật. Log build/package ở `dist/package-refresh-f2f6ec6bb088474c82519459033d74fe.log`; toàn bộ artifact dist bị Git bỏ qua. Hash không phải chữ ký xác thực; kiểm thử máy sạch, runtime load, UI/clipboard thật, CF_HTML offsets, migration/rollback và rà soát giấy phép vẫn chưa hoàn tất. Chưa phát hành hoặc cài beta trong lượt đóng gói này.

Update policy beta: About Fluent giữ kiểm tra phiên bản nhưng nút chính chỉ mở releases bằng Launcher; không gọi OpenKeyUpdate.exe, không gửi exit message cho engine và không Application.Exit. Chuỗi dialog/nút được đổi đồng bộ vi-VN/en-US. Win32 startup/About/Control kiểm tra policy trước gọi updater, bật lại nút khi bị chặn. Updater trực tiếp kiểm tra cả thư mục module và thư mục đích trước tải metadata/ZIP; không đổi vCheckNewVersion trong registry.

5 layout module policy đạt trên mỗi x64/x86: legacy/unrelated được phép, marker manifest/flat UI/nested UI bị chặn (nested folder rỗng vẫn chặn bundle chưa hoàn chỉnh). Harness chạy entry point --policy của test executable được copy vào fixture, không chạy updater UI/download. Cùng 26 ca hồi quy updater đạt; kiểm thử trực quan dialog/mở browser lỗi/hủy/thông báo Win32 vẫn chờ. Marker là chốt tránh cập nhật thiếu bundle, không phải bằng chứng nguồn gốc/an toàn của binary.

Header policy dùng UTF-8 BOM để legacy compiler không phụ thuộc code page nguồn. Đã kiểm tra chuỗi UTF-16 tiếng Việt trong 8 binary engine/updater Debug/Release x64/x86 khớp nội dung mong đợi; không suy ra đã kiểm thử hiển thị/font. Resource vi/en vẫn có 383 khóa khớp nhau.

Xác minh policy cuối: engine, updater và UI rebuild/static analysis trên Debug/Release x64/Win32 đều 0 cảnh báo/0 lỗi. Engine Debug/Release x64 được build/phân tích lại sau khi cố định encoding header. 31 ca updater/policy mỗi kiến trúc chạy lại đạt. Gói Release x64/x86 đã được tạo lại từ binary mới và qua verifier, 278 payload mỗi gói. Dialog thực tế/browser/error/cancel vẫn cần kiểm thử trên Windows.

Notices: collector đọc project.assets.json và nuspec trong package cache đã restore; sao chép nguyên tệp license/notice, lưu metadata/copyright UTF-8 trong inventory. 9 package được ghi nhận, gồm dependency runtime và build tools. Microsoft.Windows.SDK.BuildTools không khai báo license file trong nuspec nên inventory ghi manual review. Đây chưa phải binary-to-license mapping hoặc xác nhận đủ điều kiện phân phối.

5 ca collector tổng hợp đạt trên PowerShell 7/Windows PowerShell 5.1: sao chép/inventory đúng, từ chối overwrite output, license file thiếu, đường dẫn notice traversal và output ngoài dist. Cache NuGet thật không bị sửa; fixture được giữ trong dist. ZIP x64/x86 với notices có 278 payload và verifier đạt.

Tài liệu: README root/Windows đã phân biệt Fluent beta với yêu cầu/ảnh legacy, ghi môi trường build và lệnh chạy/test/package. `MIGRATION_FLUENT_BETA.md` mô tả HKCU dùng chung, sao lưu registry/macro, giới hạn dữ liệu nhiều dòng, rollback executable và thao tác registry chỉ khi chủ động xác nhận; startup/task/shortcut cần ghi riêng. Chưa thực hiện export/import registry, nâng cấp, rollback hoặc chụp ảnh Fluent trong lượt này.

Sau thêm migration note vào gói: ZIP x64/x86 từ output Release có 254 payload đã qua verifier; 17 ca verifier vẫn đạt. Không chạy lại compiler vì chỉ thay tài liệu/script đóng gói.

Verifier độc lập `verify-beta.ps1`: kiểm tra schema metadata tối thiểu, files bắt buộc, path traversal/entry trùng, kích thước và SHA-256 payload, PE header/architecture ba executable; không giải nén hay load binary. Hỗ trợ ExpectedArchitecture và ExpectedSha256 của toàn ZIP từ bên ngoài. 17 ca tổng hợp đạt: hai kiến trúc hợp lệ, payload bị sửa/thiếu/thừa, duplicate ZIP/record, traversal, metadata architecture/runtime sai, PE sai kiến trúc, thiếu tệp bắt buộc/manifest, hash sai format, hash bên ngoài đúng/sai và expected architecture sai. Các PE trong fixture chỉ là header giả, không phải chương trình chạy được.

Đóng gói lại từ Release output bằng SkipBuild x64/x86: mỗi gói 253 payload, verifier mới đạt. Chưa build lại binary trong lượt chỉ sửa script này. Manifest unsigned không xác thực nguồn gốc nếu payload và manifest đều bị sửa; cần hash ngoài đáng tin hoặc chữ ký để tăng bảo đảm.

Chạy `test-packaging.ps1` trên PowerShell 7 và Windows PowerShell 5.1: đều đạt 17/17; `test-prerequisites.ps1` trên Windows PowerShell 5.1 đạt 6/6. Không đổi execution policy hệ thống, registry hoặc ứng dụng đang cài.

`package-beta.ps1` tạo layout engine/updater ở root và UI/runtime trong OpenKey.UI; giữ resource đa ngôn ngữ và loại artifact biên dịch. Đã chạy `-SkipBuild` cho x64/x86: mỗi ZIP xác minh đủ 251 payload, đúng kiến trúc ba executable và SHA-256 từng entry. Manifest không tự hash chính nó. Artifact dưới dist bị Git bỏ qua, không phát hành hay chạy app.

Windows App SDK self-contained chưa đồng nghĩa C++ runtime self-contained; gói hiện yêu cầu VC++ Redistributable và chưa bundle/cài nó. Chưa xác minh máy sạch, giấy phép runtime, chữ ký số hoặc nâng cấp. Hướng dẫn và giới hạn updater stable ghi trong `BETA_PACKAGING.md`.

Đã chạy thêm `./package-beta.ps1 x64` và `./package-beta.ps1 x86` không SkipBuild: build Release engine/updater/UI (0 cảnh báo/0 lỗi), tạo và xác minh ZIP thành công.

Gói mới có thêm `check-prerequisites.ps1` và trường vcRuntimeMinimumVersion trong manifest: 252 payload mỗi kiến trúc, hash ZIP đạt. Checker chỉ đọc registry qua cả view 32/64-bit; không cài runtime. 6 ca so sánh version đạt (bằng/mới hơn/cũ hơn/chưa installed/rỗng/sai format). Checker đóng gói đọc đúng x64/x86 từ manifest. Máy phát triển có 14.51.36247.0 cho cả hai kiến trúc, đạt minimum 14.51.36231.0. Đây là kiểm tra prerequisite qua registry, không phải kiểm thử load DLL hoặc máy sạch. Minimum mặc định cần được rà soát khi thay toolset; SkipBuild không suy ra version compiler của binary cũ.
