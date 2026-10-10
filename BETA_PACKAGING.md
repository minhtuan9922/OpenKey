# Gói beta Fluent UI — thử nghiệm nội bộ

Đọc [hướng dẫn sao lưu, thử beta và rollback](MIGRATION_FLUENT_BETA.md) trước khi chạy.
Gói ở thư mục riêng vẫn dùng chung settings/macro với bản cũ của cùng tài khoản.

## Artifact mới nhất — alias HTML legacy

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
