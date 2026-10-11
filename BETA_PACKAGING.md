# Gói beta Fluent UI — thử nghiệm nội bộ

## Bộ cài offline một file — 2026-10-11

Đã tạo `dist/setup-9c0099c69ee6441f8edd91f2ace73cbe/Output/OpenKey-Setup.exe`
tự chọn x64/x86, 62.053.914 byte; SHA-256
`ED88989112773AB4FB2F7DAC65FE91BE5E9394415280E7FFA89D2C4F27B2B567`.
Chứa các thay đổi macro/theme/routes/popup tray và chống giật mới nhất.
Start Menu, Desktop tùy chọn, runtime offline với consent/UAC trước app và gỡ
cài giữ settings. Hướng dẫn build/verify và giới hạn ở [INSTALLER.md](INSTALLER.md).
Chưa ký số/chạy cài thật/nghiệm thu máy sạch hoặc chốt quyền phân phối.

Payload mới (Release All build sạch cả hai kiến trúc, 281 payload/ZIP):

- x64: `dist/OpenKey-Fluent-beta-x64-d5327f761c924577aa0046e42aa4318b.zip`;
  SHA-256 `160D2453D7E935C3F050B34039417F53643A697EB41D006C914185619DF7346D`.
- x86: `dist/OpenKey-Fluent-beta-x86-aad2934fccec48298819cf8a349ae160.zip`;
  SHA-256 `65DAB1F89BADCA77DC690B379D8D7A1CB14FCB797539D600C079CE2DF122FA11`.

ZIP và các Sandbox session cũ giữ nguyên. Bộ cài chứa hai VC++ Redistributable
14.51.36247.0 ký Microsoft, hash nguồn như mục runtime bên dưới; chỉ cài bản
đúng kiến trúc nếu thiếu/cũ, không tự cài trong quá trình build trên host.
Verifier PS7/5.1 đối chiếu compiler manifest 566 input trên một media slice,
hai ZIP/staged payload/hash/PE setup/runtime. Đây là xác minh artifact/input,
không chứng minh cài/gỡ hay shortcut hoạt động trong guest.

Đọc [hướng dẫn sao lưu, thử beta và rollback](MIGRATION_FLUENT_BETA.md) trước khi chạy.
Gói ở thư mục riêng vẫn dùng chung settings/macro với bản cũ của cùng tài khoản.

## Runtime offline bổ sung cho Sandbox — 2026-10-11

`prepare-sandbox.ps1` nay nhận cặp tùy chọn `-RuntimePath` và
`-RuntimeExpectedSha256`: xác minh hash/chữ ký Microsoft/kiến trúc/phiên bản
theo minimum của beta trước staging và sau sao chép. Không tự tải hoặc cài.
Không truyền cặp này vẫn chuẩn bị Sandbox không kèm runtime như trước.

Hai session mới tạo bằng tùy chọn này (cùng ZIP/hash và runtime bên dưới):

- x64: `dist/sandbox-8f96c320e1f34788ab1ca4be75af0b53/OpenKey.wsb`.
- x86: `dist/sandbox-7a2d1dd51c2947c59f5aa963c196cf5c/OpenKey.wsb`.

Từng Input có `session.json` ghi metadata runtime và `OFFLINE_RUNTIME.md`
hướng dẫn cài thủ công chỉ trong guest. Chưa mở Sandbox hoặc chạy installer;
không thay ZIP hiện có, không đưa redistributable vào gói phát hành.

Hai session của artifact bên dưới đã có bộ cài riêng trong Input read-only:

- x64: `dist/sandbox-023a5ca909a447b182252a5bf5af43ab/Input/vc_redist.x64.exe`; SHA-256 `843068991DAAA1F73AD9F6239BCE4D0F6A07A51F18C37EA2A867E9BECA71295C`.
- x86: `dist/sandbox-f83d8c38ba82403c8068ab1e14e864b5/Input/vc_redist.x86.exe`; SHA-256 `F0BAB33A302B3CDB2E11113760D016F54FD3D2632C65BA7834FAC4F0ABD7F1A3`.

Cả hai FileVersion 14.51.36247.0, ProductName đúng kiến trúc và Authenticode
Valid/Microsoft Corporation; kiểm tra lại trên PS7/PS5.1 và sau copy.
Đáp ứng minimum beta 14.51.36231.0. x64 tái dùng bộ cài đã tải ở session cũ;
x86 tải từ permalink chính thức ngày 2026-10-11, bản gốc giữ ở
`dist/offline-runtime-03963ffb01ae434c95da5ef2723bf6b8/vc_redist.x86.exe`.
Nguồn/phiên bản/hash và hướng dẫn cài thủ công chỉ trong guest được ghi trong
`Input/OFFLINE_RUNTIME.md` của từng session. Không chạy bộ cài hoặc mở guest.

ZIP beta và .wsb không đổi; kiểm tra lại staged beta hash, mạng/clipboard
host tắt và Input read-only trên hai shell. Không thêm bộ cài vào ZIP beta,
không tự cài runtime và không xác nhận quyền phân phối. prepare-sandbox.ps1
vẫn không tự tải/copy runtime cho session mới. Máy sạch/UI/hook chưa nghiệm thu.
Theo [Microsoft](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/),
runtime phải khớp kiến trúc ứng dụng và không cũ hơn build tools; quyền phân
phối phải xét theo điều khoản Visual Studio, chưa được chốt ở bước này.

## Artifact mới nhất — About workflow và bộ Sandbox (2026-10-11)

Chứa toàn bộ thay đổi metadata dùng chung bên dưới, dọn helper tải cũ,
latest-action-wins cho liên kết About và giữ khóa kiểm tra cập nhật đến hết
ContentDialog/Launcher. Dialog dùng theme của trang; hủy/lỗi/stale khôi phục
nút trên UI thread. Không thay engine gõ hoặc updater tải/cài ZIP stable.

- x64: `dist/OpenKey-Fluent-beta-x64-7a943eaebd96486eb4679708c3ca7616.zip`; SHA-256 `DBEEEC0B3CB4C31809A206606702DD8C1D2C5E78A26F9E3A44AF9374ECC5AE67`.
- x86: `dist/OpenKey-Fluent-beta-x86-184f2a67e6374a149e5159f987477386.zip`; SHA-256 `EAA05EBB3B4D74D5B8C7B4EE3CCA29D0146DE1A88AF3CD72B75D51143CF65736`.

Release All không SkipBuild sạch mỗi kiến trúc, 281 payload/hash/PE đạt
verifier PowerShell 7 và 5.1. Ba executable khớp output Release, notice
WHATWG khớp nguồn; UI resources 4/4 mỗi kiến trúc. Metadata/notices thu thập
cho 9 restored packages, vẫn cần rà giấy phép trước phát hành.
Logs dist/about-workflow-beta-{x64,x86}.log. Mã trước đóng gói đã đạt
14/14 nhóm hồi quy trên hai shell và 62 shared update/lifecycle fixtures.

Sandbox mới (chỉ chuẩn bị, chưa chạy guest):

- x64: `dist/sandbox-023a5ca909a447b182252a5bf5af43ab/OpenKey.wsb`.
- x86: `dist/sandbox-f83d8c38ba82403c8068ab1e14e864b5/OpenKey.wsb`.

Input riêng read-only, Logs riêng có quyền ghi; mạng/clipboard host/mic/camera/
printer redirection tắt. ZIP staged được đối chiếu SHA-256. 27/27 fixture
Sandbox đạt PS7/PS5.1, logs dist/about-workflow-sandbox-tests*.log.
Khi chuẩn bị ban đầu, các phiên chưa kèm runtime; nay đã bổ sung bộ cài offline
như mục trên. Bootstrap vẫn dừng khi thiếu runtime đã cài và không tự tải/cài.
Xem Sandbox/README.md, chưa nghiệm thu máy sạch/UI/hook.
Không mở/cài/ký/phát hành app hoặc thay settings/clipboard host. Gói và
Sandbox input cũ được giữ nguyên. Hash manifest không chứng minh nhà phát hành.
Tài liệu BETA_PACKAGING.md bên trong ZIP có trước khi ghi nhận hash gói này;
dùng phần này của tài liệu workspace để đối chiếu artifact mới nhất.

## Artifact trước About workflow — metadata cập nhật dùng chung (2026-10-10)

Gói này chứa UpdateService/UpdateCheckService/WinHTTP chung cho engine,
About Fluent và metadata updater độc lập. Engine kiểm tra trên worker nền;
Fluent sửa packed version 2.0.5 và bỏ qua kết quả cũ sau unload. Updater bỏ
metadata temp file/locale reader, dừng khi transport/schema lỗi. Fluent vẫn
chỉ mở Releases sau xác nhận; updater stable engine-only bị chặn trong bundle.

- x64: `dist/OpenKey-Fluent-beta-x64-1387e7a9774244ab8070e5f46a134464.zip`; SHA-256 `39D1B118C8AA585955353A53592571B93B17CFD41334C21721AA8B181B4100AB`.
- x86: `dist/OpenKey-Fluent-beta-x86-87cf21e6149e47adb8e15a1e622ff29b.zip`; SHA-256 `EF270C8E36DB117C6584ECADAC9FE33B723E37F1D353655CE508F5A470C8914C`.

Release All không SkipBuild đạt 0 cảnh báo/0 lỗi mỗi kiến trúc. Mỗi ZIP có
281 payload, verifier/hash/PE đạt PowerShell 7 và 5.1; 3 executable khớp
output Release, HTML_ENTITIES_NOTICE.md khớp nguồn; resources 4/4 mỗi UI.
Thu thập notice/metadata cho 9 restored NuGet packages, vẫn cần rà soát
giấy phép trước phát hành. Logs dist/shared-update-beta-{x64,x86}.log.
Hồi quy mã trước đóng gói đạt 14/14 nhóm trên cả hai shell, xem FLUENT_UI_QA.md.

Không chạy/cài/ký/phát hành app, tải cập nhật thật, đổi settings/clipboard
hoặc Sandbox input cũ. Tải ZIP/cài đặt updater chưa thay, chưa xác minh
chữ ký; UI/network/máy sạch/nâng cấp thật còn chờ. ZIP cũ giữ nguyên.
Tài liệu BETA_PACKAGING.md bên trong ZIP là bản trước khi ghi nhận hash gói
này; dùng phần này của tài liệu workspace để đối chiếu artifact mới nhất.

## Artifact trước metadata dùng chung — service mở updater legacy

About/MainControl/startup dùng UpdateLaunchService: policy chặn Fluent trước
launch, updater/working directory theo thư mục executable, không thoát engine
khi Windows không chấp nhận mở updater. Không đổi updater stable engine-only
hoặc parser/network checkUpdate; chưa thực hiện cập nhật thật.

- x64: `dist/OpenKey-Fluent-beta-x64-4d6e3ae148994cc78d8785f13c9af9e1.zip`; SHA-256 `32089801D0FE9C9ADC164199BBFE83BE7ED7E6FA9B3D669E857D05615F976774`.
- x86: `dist/OpenKey-Fluent-beta-x86-946334b505fb47efad230f72f3d6480a.zip`; SHA-256 `75953C56E8477D049B29F07243587BEA1AD08C375E5D55A5284BFEE677A39DA1`.

77 injected launch cases và 31 updater/metadata/module-policy fixtures mỗi
kiến trúc đạt, cùng 14/14 nhóm hồi quy PS7/PS5.1. Bốn All Debug/Release
engine/updater/UI x64/x86 sạch; Debug x64 All static analysis sạch.
Beta Release All không SkipBuild sạch, 281 payload/hash/PE đạt hai shell,
resources 4/4 và notice khớp nguồn. Logs dist/update-launch-*.log.
Không mở/cài/ký/phát hành hoặc cập nhật app thật, không thay settings/Clipboard
hay Sandbox input cũ. Launch/dialog/cancel/lỗi thật, máy sạch và giấy phép vẫn
cần QA; launch accepted không chứng minh updater chạy/cập nhật thành công.

## Artifact trước update launch — bảo toàn format khi đọc Clipboard lỗi

Service dùng chung phân biệt format vắng/rỗng/đọc lỗi. Lỗi lấy handle,
kích thước/lock/terminator hoặc đăng ký HTML dừng trước EmptyClipboard;
không bỏ format lỗi khi format kia chuyển được. Unicode rỗng được giữ khi
chuyển HTML hợp lệ; chưa có rollback cho lỗi ghi sau clear.

- x64: `dist/OpenKey-Fluent-beta-x64-c560d5aefb6c4c018b2de35e63520e9a.zip`; SHA-256 `DA6FDBF67DF5879E29A4DE7B3C1366BBCFF5448DFD07F9B268BABA409AF40BA5`.
- x86: `dist/OpenKey-Fluent-beta-x86-a65a3066b68c4e3e9178f12cc9eb4653.zip`; SHA-256 `24049F6AB67B86AAC77FB55122596754E6E8F238CF40B75EB01956D00532FF64`.

3.575 Clipboard fake mỗi kiến trúc, 175 pure-text/20 engine và 14/14 nhóm
hồi quy đạt PS7/PS5.1. Bốn All Debug/Release engine/updater/UI x64/x86
sạch; Debug x64 All có static analysis. Beta Release All không SkipBuild
sạch; 281 payload/hash/PE mỗi ZIP đạt hai shell, resources 4/4 và notice khớp.
Logs dist/clipboard-read-*.log. Không mở/cài/ký/phát hành app, đổi Clipboard/
settings thật hoặc Sandbox input cũ. Delayed rendering/hotkey/tray/paste HTML
thật và máy sạch vẫn cần QA; chưa bảo toàn mọi format ngoài Unicode/HTML.

## Artifact trước read failure — chuyển mã nhanh legacy dùng service chung

Hotkey/tray/dialog Win32 dùng LegacyQuickConvert → ConvertService với HWND
engine và snapshot cấu hình legacy. UI Fluent không link globals engine.
HTML dùng parser/offset/entity chung, không chuyển cả header/thẻ qua converter.
HTML bảng mã legacy hoặc fragment không hỗ trợ bị từ chối trước clear.

- x64: `dist/OpenKey-Fluent-beta-x64-5384e43574144234b412e7736f6594ac.zip`; SHA-256 `FB3DF3E736DD782211BE6399A4E0E9B206A68F2901F96A53EF113CF5F8466C56`.
- x86: `dist/OpenKey-Fluent-beta-x86-7ac753acf87f4ab9a3fd7b96a91804f1.zip`; SHA-256 `BFF5AA0C13601D8269343CBEA40814F4E3FE0DBA131C0A13E9A6784A2CA7187C`.

3.543 Clipboard fake mỗi kiến trúc, 175 pure-text/20 engine và 14/14 nhóm
hồi quy đạt PS7/PS5.1 trên mã cuối. Bốn All Debug/Release engine/updater/UI
x64/x86 sạch; Debug x64 All có static analysis. Beta Release All không SkipBuild
sạch, 281 payload/hash/PE mỗi ZIP đạt hai shell, resources 4/4 và notice khớp.
Logs dist/quick-convert-final-*.log, dist/quick-convert-beta-*.log.
Không mở/cài/ký/phát hành app hoặc đổi Clipboard/settings/Sandbox input trước;
người dùng tự thoát engine Debug để giải phóng binary. Hotkey/tray/HTML thật,
máy sạch, quyền phân phối và rollback khi SetClipboardData lỗi vẫn cần QA sau.

## Artifact trước bridge legacy — literal reference recovery

Ampersand/tên không khớp và entity số không có digit hiện được giữ như text,
chuyển casing và escape lại, không giải mã đệ quy. Selection trong literal
được ánh xạ; entity hợp lệ vẫn không cho cắt giữa. Xem FLUENT_UI_QA.md.

- x64: `dist/OpenKey-Fluent-beta-x64-71c037481c7d4099ba8b91254c8efefc.zip`; SHA-256 `646950AB016381B3445B5C93E44A0FDAA9506D1552D5AD2124CA117964637FB9`.
- x86: `dist/OpenKey-Fluent-beta-x86-3bfd56745e124aa4bd714555d679cdc3.zip`; SHA-256 `95F36DA1C5A23A37E9065181AA3290E29A81B0CC0D1E0C7CA1D5D664C17EF723`.

2.732 Clipboard mỗi kiến trúc, 175 pure-text/20 engine và 14/14 nhóm hồi quy
đạt PS7/PS5.1. Bốn UI Debug/Release, static analysis Debug x64 và Release All
build sạch. 281 payload/hash/PE mỗi ZIP đạt hai shell, binary resources 4/4
mỗi kiến trúc, notice WHATWG khớp nguồn. Logs dist/cfhtml-literal-*.log.
Không mở/cài/ký/phát hành app, đổi settings/Clipboard thật hoặc Sandbox input cũ.
Máy sạch, paste HTML thật và rà soát giấy phép vẫn cần kiểm thử sau.

## Artifact trước literal recovery — numeric recovery

Mã nguồn hiện hỗ trợ entity số thiếu dấu chấm phẩy, số dài không overflow,
U+FFFD cho null/surrogate/ngoài Unicode và mapping C1 của HTML. Selection vẫn
không được cắt giữa entity; attributes/context không đổi và casing legacy giữ nguyên.
Notice WHATWG đã bổ sung bảng C1.

- x64: `dist/OpenKey-Fluent-beta-x64-ff37a17640ee4b37bc3ec809e93bedf2.zip`; SHA-256 `5AA6B304674C76497B4949F33A0592945FA055619EFE4325D43B37FE059E34AF`.
- x86: `dist/OpenKey-Fluent-beta-x86-dcc6bcb8951d43049f85ca14a2a6d328.zip`; SHA-256 `DE452352EBBAE0F88B1CD0C1ED8D080C025A3696A4B2899D23AEC585225BAC8F`.

2.641 Clipboard mỗi kiến trúc, 175 pure-text/20 engine và 14/14 regression suites
đạt PS7/PS5.1. Bốn UI Debug/Release, static analysis Debug x64 và Release All sạch.
281 payload/hash/PE đạt hai shell, 4/4 binary resources mỗi kiến trúc, notice khớp
nguồn. Logs dist/cfhtml-numeric-*.log. Không mở/cài/ký/phát hành app hoặc đổi dữ liệu
người dùng; runtime máy sạch/Clipboard thật và rà soát giấy phép vẫn cần QA.
Gói và Sandbox input trước giữ nguyên.

## Artifact trước numeric recovery — alias HTML legacy

Mã nguồn hiện hỗ trợ thêm 106 alias thiếu dấu chấm phẩy trong text node,
longest-match và selection preview từ toàn text node. 2.452 ca Clipboard mỗi
kiến trúc, 14/14 regression suites PS7/PS5.1 và bốn build UI đều đạt;
static analysis Debug x64 không có cảnh báo/lỗi. Notice WHATWG đã cập nhật.

- x64: `dist/OpenKey-Fluent-beta-x64-459635ba5d5d4341b37322b9b39d6872.zip`; SHA-256 `8E76BD2E63729AE80C52DFC7AB8D3A2447CFCBEB45B3BEA11E1E11261DC0F816`.
- x86: `dist/OpenKey-Fluent-beta-x86-bca1656ee4c54c5d9957299951298798.zip`; SHA-256 `F747C94EDE181FB94D1B4BA92414F3A7CB5C2FA4BF250B44F272F1EAE79DD53F`.

Release All không SkipBuild đạt 0 cảnh báo/0 lỗi; 281 payload/hash/PE đạt
PS7/PS5.1, resource binary 4/4 mỗi kiến trúc và notice khớp nguồn.
Logs dist/cfhtml-legacy-beta-{x64,x86}.log.
Không chạy/cài/ký/phát hành app hoặc đổi settings; runtime máy sạch, Clipboard
thật và giấy phép toàn bundle vẫn cần QA. Gói/Sandbox input cũ được giữ nguyên.

## Artifact trước alias legacy — toàn bảng named entities

Release All x64/x86 không SkipBuild, chứa bản vá trước và 2.125 named entities
có dấu chấm phẩy (93 tên hai Unicode scalar), dùng offline. Lookup phân biệt
hoa/thường; không triển khai HTML recovery/CSS layout hoặc bảng mã HTML legacy.
HTML_ENTITIES_NOTICE.md ghi nguồn WHATWG, biến đổi bảng và BSD-3-Clause.

- x64: `dist/OpenKey-Fluent-beta-x64-12e90a54648b4a33b9a0fc8cc2e8a0f3.zip`; SHA-256 `20D540B0A041CCF698B7FFA170613483F9E64CD32D8F61E3167BD624251E6DD0`.
- x86: `dist/OpenKey-Fluent-beta-x86-d8be7d32418d43efbaa68ef5fbdda660.zip`; SHA-256 `6EC001659AFA209F31CFDAEDA795758AA356EC29423CE2B55A70845880F4654E`.

2.292/2.292 Clipboard mỗi kiến trúc, 175 pure-text và 20 engine đạt;
14/14 regression suites đạt PS7/PS5.1. ZIP 281 payload/hash/PE đạt PS7/PS5.1,
binary resources 4/4 mỗi kiến trúc, metadata/notices 9 NuGet package và notice
WHATWG riêng. Bốn build UI và Release All 0 cảnh báo/0 lỗi;
logs dist/cfhtml-named-*.log. Không chạy/cài/ký/phát hành app, không đổi settings.
VC++ runtime chưa bundle, máy sạch/UI/Clipboard thật và rà soát giấy phép vẫn
cần nghiệm thu; gói và Sandbox input cũ giữ nguyên.

## Artifact trước toàn bảng named entities — CF_HTML casing

Release All x64/x86 không SkipBuild, chứa bản vá trước và casing đầu câu/từng từ
xuyên text node. Inline/comment giữ state, selection preview không thay state;
br/paragraph và danh sách block tag dùng boundary logic, chưa đọc CSS/layout.

- x64: `dist/OpenKey-Fluent-beta-x64-79985bd4380140acb1c1a38f553e3a4e.zip`; SHA-256 `2715853A6B1A944A53399873F6F9528866F237EF79B877A49501DD3850A3D670`.
- x86: `dist/OpenKey-Fluent-beta-x86-672ccba57ca046c4bd79c45b75232213.zip`; SHA-256 `6F34DDFEDC425816B9E3D6DFE3F8DF8936D549917D8C2B6E4DF6696A1A237A26`.

137/137 Clipboard mỗi kiến trúc và 14/14 regression suites đạt PS7/PS5.1.
Mỗi ZIP 280 payload/hash/PE đạt hai shell, resource binary 4/4, notices 9 package.
Build Debug/Release UI và Release All 0 cảnh báo/0 lỗi; logs dist/cfhtml-casing-*.log.
Không chạy/cài/phát hành app; gói chưa ký, VC++ runtime chưa bundle, máy sạch,
UI/HTML paste thật và giấy phép còn chờ QA. Gói/Sandbox input cũ không tự cập nhật.

## Artifact trước bản vá casing — CF_HTML entity

Tạo từ Release All x64/x86 không SkipBuild, chứa các bản vá trước cùng xử lý
numeric/common named entity, escape lại text và giữ selection/xuống dòng.
Entity ngoài tập con được hỗ trợ vẫn bị từ chối an toàn, không phải HTML parser đầy đủ.

- x64: `dist/OpenKey-Fluent-beta-x64-61f3b13de3d44549ae88eb628f88359c.zip`; SHA-256 `9C0C63A949206A6DDE93C3B16D132F4F4FF12CBE4A0488DD5D20B8BAFED26576`.
- x86: `dist/OpenKey-Fluent-beta-x86-8e34bb95bdd4429bb064ebfe6d7a3c47.zip`; SHA-256 `C0572423C01DFABA18DB7C07183F093D0556CCB8E66C99EFC640E7A984332845`.

109/109 ca Clipboard mỗi kiến trúc và 14/14 regression suites đạt trên PS7/PS5.1.
Mỗi ZIP xác minh 280 payload/hash/PE trên hai shell; 9 package notices,
4/4 resource binary checks trước gom ZIP. Release All 0 cảnh báo/0 lỗi, log
dist/cfhtml-entities-beta-x64.log và x86.log. Không chạy/cài/phát hành app;
gói chưa ký, VC++ runtime chưa bundle, máy sạch và HTML paste thật còn chờ QA.
Các ZIP và đầu vào Sandbox trước đây không tự cập nhật.

## Artifact trước bản vá entity — High Contrast và CF_HTML context/selection

Đóng gói từ Release All x64/x86, không SkipBuild; chứa bản vá khởi động
HighContrastChanged và CF_HTML UTF-8/byte offsets/context -1/selection.

- x64: `dist/OpenKey-Fluent-beta-x64-c0863f5ec0574c39b921d7a809048d43.zip`; SHA-256 `1DF8E1B0DF092C6F6C39A7CC52256C8A8673FEE4309AEB34FD6075F76A63989A`.
- x86: `dist/OpenKey-Fluent-beta-x86-39116bfdcd1a486e9452fed92e725c9c.zip`; SHA-256 `B52450812F59B430B0AA25DD2813B7405F7568B920A1CAB62F7F1C426C2A5AE2`.

Mỗi gói xác minh 280 payload/hash/PE trên PS7 và PS5.1; 9 package notices.
Release All 0 cảnh báo/0 lỗi; log dist/cfhtml-selection-beta-x64.log và x86.log.
56 ca Clipboard giả mỗi kiến trúc và toàn bộ 14 regression suites đạt trên cả
hai shell. Không chạy/cài app khi kiểm ZIP; gói chưa ký, VC++ runtime chưa bundle,
máy sạch/UI/HTML paste thật và giấy phép vẫn cần nghiệm thu. Không phát hành stable.
Đầu vào Sandbox đã chuẩn bị trước đây vẫn là gói cũ, không tự cập nhật.

## Bản nguồn cập nhật — 2026-10-10

UI hiện có bản vá bắt lỗi đăng ký HighContrastChanged lúc khởi động và xử lý
CF_HTML giữ markup, tính lại offset byte UTF-8, hỗ trợ context -1 và selection.
Đã hỗ trợ numeric entity và amp/lt/gt/quot/apos/nbsp, escape lại text và giữ
selection sau chuyển đổi. Casing đã giữ trạng thái qua thẻ inline/comment;
br/paragraph/danh sách block tag dùng boundary logic, không mô phỏng CSS/layout.
Entity ngoài tập con và bảng mã HTML legacy chưa hỗ trợ, bị từ chối trước khi xóa Clipboard.
ZIP tạo trước các thay đổi này
không tự cập nhật. Kiểm thử HTML thật và khởi động UI vẫn cần nghiệm thu.

## Artifact trước bản vá High Contrast/CF_HTML — 2026-10-10

Chứa các thay đổi theme/titlebar/geometry, resource/style chung, Mica fallback,
reduced-motion navigation, bảo vệ thao tác/export và cache draft Gõ tắt/Chuyển mã,
tắt history, icon nhúng và cảnh báo phụ thuộc Chromium/chính tả nâng cao song ngữ.
Đóng gói từ Release All, không SkipBuild; gói cũ và đầu vào Sandbox giữ nguyên.

- x64: `dist/OpenKey-Fluent-beta-x64-819edd07c16c4865aa92ec5374b383bc.zip`; SHA-256 `54353BE1750615928159D7149B7A7CDA8060265414A02ACD9FE662FB066FC1F8`.
- x86: `dist/OpenKey-Fluent-beta-x86-4d600eb956b044f3a27fffa94f63108e.zip`; SHA-256 `AAAAC96A45A6EACCE2BCA35E1CA6E5419B960F96BE379B512715B22DEB9EFDDB`.

Cả hai xác minh 280 payload/hash/PE bằng PS7 và PS5.1; metadata/notices của 9 package.
13 regression suites / 1218 checks mỗi shell đạt. Debug x64 static analysis sạch,
Debug x86 và Release All x64/x86 build 0 cảnh báo/0 lỗi. Checker PE/icon/manifest
4/4 mỗi gói đạt trước gom ZIP. Logs dist/ui-dependency-beta-x64.log và x86.log.
Không chạy app khi kiểm ZIP. UI, máy sạch và giấy phép chưa hoàn tất; chỉ thử nội bộ,
không phát hành stable. Sandbox đã tạo trước đây vẫn dùng ZIP cache/history/icon cũ.

## Tạo và kiểm tra gói

### Chuẩn bị Windows Sandbox

Chạy tại root repository (thay đường dẫn và hash bằng gói cần thử):

```powershell
.\prepare-sandbox.ps1 -ZipPath <file.zip> -ExpectedSha256 <sha256> -Platform x64
```

Script xác minh ZIP trước, tạo phiên riêng `dist/sandbox-<GUID>` và trả đường dẫn
`OpenKey.wsb` cùng thư mục `Logs`. Không build lại binary, mở Sandbox, bật Windows
feature, tải/cài runtime hoặc đổi cấu hình host. Người kiểm thử chủ động mở `.wsb`.
Đầu vào chỉ đọc; chỉ thư mục Logs riêng cho phép guest ghi. Không đặt dữ liệu quý
trong Logs. Mạng và clipboard host/guest tắt; vGPU dùng Default.

Guest xác minh lại ZIP, giải nén vào ổ guest và kiểm tra VC++ runtime. Thiếu runtime
thì log `prerequisite-missing` và không chạy app; cần chuẩn bị installer offline
Microsoft riêng trước khi kiểm thử tiếp, script chưa hỗ trợ đưa installer vào phiên.
Process UI còn sống sau ba giây chỉ là `ui-running-smoke-only`, không phải nghiệm thu
GUI hoặc gõ tiếng Việt. Engine không tự chạy. Xem [hướng dẫn đầy đủ](Sandbox/README.md).

Hai cấu hình đã chuẩn bị ngày 2026-10-10 từ ZIP hiện tại, chưa được mở:

- x64: `dist/sandbox-77f36d02cbeb4ecb894041c71dcd4525/OpenKey.wsb`.
- x86: `dist/sandbox-39cdae6d47084155b9c4b425123490fe/OpenKey.wsb`.

### Đóng gói

Tạo gói Release bằng `./package-beta.ps1 x64` hoặc `./package-beta.ps1 x86`.
Script gọi `build.ps1 Release <platform> All`, tạo thư mục/ZIP riêng dưới `dist`,
kiểm tra kiến trúc ba executable và SHA-256 từng payload trong ZIP.
`-SkipBuild` chỉ dùng để kiểm tra đóng gói output đã build; không bảo đảm mã nguồn mới nhất.

Gói gồm engine `OpenKey64.exe` hoặc `OpenKey32.exe`, `OpenKeyUpdate.exe`,
thư mục `OpenKey.UI` cùng toàn bộ runtime/resource output (không gồm PDB/lib/obj),
giấy phép và manifest kiểm kê. Không đổi registry, startup hay macro người dùng.
SHA-256 chỉ kiểm tra tính toàn vẹn, không phải chữ ký xác thực nhà phát hành.

Kiểm tra ZIP độc lập bằng `./verify-beta.ps1 -ZipPath <duong-dan.zip>`.
Verifier đọc manifest và từng entry trực tiếp trong ZIP, không giải nén hay chạy app;
kiểm tra tệp bắt buộc, hash, kích thước, đường dẫn, entry trùng và PE header/kiến trúc
của ba executable. Thêm `-ExpectedArchitecture x64`/`x86` khi cần.
Nếu nhận SHA-256 ZIP từ một kênh đáng tin cậy, truyền `-ExpectedSha256 <hash>` để
đối chiếu cả archive. Manifest không ký có thể bị sửa cùng payload; verifier không
thể xác thực nhà phát hành chỉ bằng các hash bên trong ZIP.

Giải nén toàn bộ ZIP vào một thư mục riêng; thoát OpenKey cũ trước khi chạy
engine tương ứng. Mở bảng điều khiển từ tray để kiểm tra Fluent UI.
Không di chuyển riêng UI executable ra khỏi thư mục resource/runtime của nó.
Máy đích cần Microsoft Visual C++ Redistributable phù hợp kiến trúc/toolset;
script chưa tải, cài hoặc đóng gói redistributable.

Trước khi chạy, dùng `./check-prerequisites.ps1` trong thư mục giải nén.
Script đọc kiến trúc và phiên bản runtime tối thiểu từ manifest, chỉ kiểm tra
registry, không tải/cài bất cứ thứ gì. `-ReportOnly` trả kết quả mà không báo lỗi
khi thiếu runtime. Kiểm tra này không chứng minh DLL tải được hay app chạy được.
Tải runtime từ [trang chính thức Microsoft](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170):
[x64](https://aka.ms/vc14/vc_redist.x64.exe) hoặc [x86](https://aka.ms/vc14/vc_redist.x86.exe).
Runtime phải phù hợp kiến trúc app và không cũ hơn bộ công cụ build.

Manifest hiện mặc định tối thiểu 14.51.36231.0 cho toolset đang kiểm thử.
Khi đổi toolset, người đóng gói phải đối chiếu và truyền `-MinimumVcRuntimeVersion`
phù hợp; script không tự suy ra toolset thực tế của output cũ khi dùng SkipBuild.

Windows App SDK đã cấu hình self-contained, nhưng điều đó không tự loại bỏ
phụ thuộc C++ runtime. Xem [deployment self-contained của Microsoft](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps).
Yêu cầu Windows và khả năng chạy trên máy sạch phải được xác minh thực tế;
không suy ra chỉ từ việc build thành công trên máy có Visual Studio.

Không dùng chức năng cập nhật tự động trong bản beta này: updater hiện tải gói
stable và chỉ thay engine phù hợp kiến trúc build của updater (x64: OpenKey64.exe,
x86: OpenKey32.exe), không nâng cấp đồng bộ UI/runtime. Engine và updater phải cùng
kiến trúc như trong gói beta được script kiểm tra; không trộn updater khác kiến trúc.
Fluent UI hiện chỉ mở trang phát hành sau khi người dùng chọn xem bản mới, không
chạy updater hoặc thoát engine. Engine Win32 và updater kiểm tra bundle marker
(package-manifest.json/OpenKey.UI) để chặn luồng cập nhật engine-only trong gói beta;
updater cũng kiểm tra thư mục đích trước khi tải. Không coi các marker là cơ chế
xác thực/chữ ký hoặc thay thế cập nhật đồng bộ toàn bundle.
Không phát hành ZIP này vào kênh stable. Chưa kiểm thử nâng cấp/rollback,
máy sạch, DPI/Narrator hoặc ký số. Đây là artifact thử nghiệm, không phải bản phát hành hoàn chỉnh.

Trước khi phân phối ra ngoài cần rà soát giấy phép/thông báo bản quyền runtime
đi kèm, bổ sung nguồn tương ứng theo GPL và chốt chiến lược cập nhật toàn bộ bundle.

Script đóng gói thu thập `ThirdPartyNotices/inventory.json`, nuspec và các tệp
license/notice từ dependency đã restore. Inventory gồm cả công cụ chỉ dùng khi
build, chưa ánh xạ từng binary runtime về giấy phép. Một số package không khai
báo license file; inventory ghi rõ cần kiểm tra thủ công. Việc sao chép notices
không xác nhận quyền phân phối hay đã hoàn thành nghĩa vụ giấy phép. Gói vẫn chỉ
dành cho thử nghiệm nội bộ và còn cần rà soát trước khi đưa ra ngoài.
