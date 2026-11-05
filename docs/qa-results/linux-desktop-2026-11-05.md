# OpenKey Linux desktop QA — 2026-11-05 (Asia/Saigon)

Environment: Ubuntu GNOME Wayland (`ubuntu:GNOME`), Fcitx5 5.1.19, branch `fix/ubuntu-direct-focus`, package `fcitx5-openkey 3.1.0~alpha.11-1`.

## Automated regression

| Suite | Result |
|---|---|
| build/linux-addon CTest (FCITX5+SERVICE+X11_HOTKEY) | **16/16 PASS** |
| build/ubuntu-deb CTest + verify_linux_deb | **8/8 PASS**, DEB SHA-256 verified |
| build/linux-ui CTest | **BLOCKED** — thiếu `qt6-base-dev` / `qt6-declarative-dev` (chưa cài) |

## v0.1 unchecked (không phải hạng mục kiểm thử)

- Thay `std::wstring_convert` — công việc mã nguồn (còn warning deprecate khi build).
- Tách API platform khỏi engine — công việc mã nguồn.

## v0.2 — Cài desktop / phát hiện IM / focus-out

| Ca | Kết quả | Ghi chú |
|---|---|---|
| Cài gói `.deb` alpha.11 | **PASS** | `pkexec apt-get install` |
| File addon/IM trên hệ thống | **PASS** | `openkey.so`, `openkey.conf`, `openkey-telex/vni.conf` |
| Addon enabled trong Fcitx5 | **PASS** | `fcitx5-diagnose`: OpenKey 0.2.2 enabled |
| AvailableInputMethods | **PASS** | `openkey-telex`, `openkey-vni` sau Refresh |
| Thêm vào nhóm Default | **PASS** | profile có 4 IM |
| Chuyển `fcitx5-remote -s openkey-telex` | **PASS** | current=`openkey-telex` |
| Focus-out / gõ thật | **PASS** | Người dùng: Field A `tiếng` rồi `đường`, Field B trống, không lặp; harness GTK Direct/Telex |

InputStyle hiện tại: `Direct` (file `~/.config/fcitx5/conf/openkey.conf`).


## Ca chức năng (UBUNTU_INPUT_QA) — GNOME Wayland / GTK harness / Direct / Telex+VNI

| Ca | Kết quả | Ghi chú |
|---|---|---|
| Cài + phát hiện IM + focus-out | **PASS** | xem bảng trên |
| Telex `tieengs vieetj` + Space | **PASS** | Người dùng: tiếng việt |
| VNI `tie6ng1 vie6t5` + Space | **PASS** | Người dùng: tiếng việt |
| Chữ hoa `TIEENGS` | **PASS** | Người dùng: TIẾNG |
| Backspace / đặt lại dấu | **PASS** | Người dùng: cả hai ca đúng |
| Escape | **PASS** | Người dùng: hủy theo dõi từ, không xóa sai |
| Space/Enter/dấu câu | **FAIL** | Enter/Space/dấu câu đều không cắt từ; ví dụ `xax`+Enter+`hooij` → `xã hoọi` |
| Điều hướng | **PARTIAL** | Left/Right PASS; Home/End FAIL |
| Shortcut | **FAIL** | Nuốt shortcut hoặc lặp từ |
| Focus quay lại (đã gồm trong focus-out) | **PASS** | `dduowngf` → đường |
| Đổi kiểu gõ Telex↔VNI | **PASS (by design)** | Đổi giữa từ phải gõ lại từ đầu; không coi là bug |
| Password/Sensitive | **PASS** | Không biến đổi tiếng Việt |


## Ma trận ứng dụng — GNOME Wayland / Direct / Telex

| Ứng dụng | Kết quả | Ghi chú |
|---|---|---|
| Firefox address bar | **FAIL** | Gõ ngay sau focus, không toggle EN–VI |
| Chrome address bar | **FAIL** | Gõ ngay sau focus, không toggle EN–VI |
| VS Code | **PARTIAL** | Editor/gợi ý hầu hết OK; terminal trong VS Code FAIL |
| LibreOffice | **FAIL** | Đa phần mất chữ cái đầu; ví dụ `tieengs` → `iếng` |
| GTK editor (harness) | **PASS** (ca chức năng chính) | Enter/Space/dấu câu/Home/End/shortcut FAIL như trên |
| Qt editor | **FAIL** | Mất chữ hoặc không chuyển dấu |

## Ma trận môi trường (Direct / Telex)

| Môi trường | Firefox/Chrome | VS Code | LibreOffice | GTK editor | Qt editor |
|---|---|---|---|---|---|
| GNOME Wayland | **FAIL** (address bar) | **PARTIAL** (editor OK, terminal FAIL) | **FAIL** (mất chữ đầu) | **PASS*** | **FAIL** |
| GNOME X11 | **N/A** | **N/A** | **N/A** | **N/A** | **N/A** (bỏ qua theo người dùng; máy dùng Wayland) |

\* GTK harness: các ca chức năng chính PASS; Enter/Space/dấu câu, Home/End, shortcut FAIL (xem bảng trên).

## v0.3 — Tray / hotkey (GNOME Wayland)

| Ca | Kết quả | Ghi chú |
|---|---|---|
| Icon/menu panel GNOME | **PASS** | Icon T/Telex hiện trên panel; tray-status=registered |
| Quit tray (giữ bộ gõ) | **PASS** | Icon biến mất; gõ tiếng Việt vẫn OK |
| Chuyển Telex/VNI từ menu | **PASS** | Đổi VNI↔Telex, icon N↔T, gõ tiếp ổn |
| Icon T/N/E + tooltip | **PASS** | T/N/E đổi đúng theo menu |
| Vòng Telex → English → Telex/VNI | **PASS** | Icon E rồi T/N; gõ không dấu khi E, có dấu khi về Telex/VNI |
| Menu Cấu hình Fcitx5 | **PASS** | Cửa sổ Fcitx5 hiện; đóng lại gõ ổn |
| Hotkey mở cấu hình | **N/A** | Wayland chưa hỗ trợ hotkey X11 |

## Tóm tắt phiên 2026-11-05 (Asia/Saigon)

- Wayland Direct: GTK harness chính PASS (Enter/Space/punct, Home/End, shortcut FAIL); Firefox/Chrome address bar FAIL; VS Code PARTIAL (terminal FAIL); LibreOffice/Qt FAIL (mất chữ đầu).
- X11: N/A theo người dùng.
- Tray v0.3 GNOME: icon, Telex/VNI, English cycle, Quit, Configure **PASS**; hotkey cấu hình **N/A** (Wayland).
- UI CTest: BLOCKED thiếu Qt6 `-dev`.

## Ghi chú sửa mã nguồn (alpha.12) — chưa retest desktop

Đã sửa trên máy local (branch `fix/ubuntu-direct-focus`), **không** khẳng định PASS desktop cho đến khi người dùng gõ lại.

### Thay đổi hành vi Direct
1. **Enter / Home / End / mũi tên / shortcut**: không `filterAndAccept` + `forwardKey` khi đang chờ ACK (Wayland thường mất phím). Optimistic ACK, flush hàng đợi composing, để phím thật tới ứng dụng.
2. **Ký tự Direct đầu tiên** trên context mới: giao phím thật cho client (không `commitString`), các ký tự sau vẫn Direct — nhằm LibreOffice/Qt/address bar mất chữ đầu.
3. **Direct + không Preedit capability** (nhiều terminal): passthrough English, không nuốt phím vào composition ẩn. Preedit style vẫn dùng server preedit.
4. Headless: thêm ca Space/Enter/dấu câu cắt từ, Home/End, Ctrl+A, pending+Enter, no-preedit passthrough.

### CTest
`build/linux-addon`: **16/16 PASS** sau sửa.

### Cài thử alpha.12 (cần sudo/pkexec của người dùng)
```bash
cd /mnt/data/project/test/OpenKey
sha256sum -c 'dist/linux/fcitx5-openkey_3.1.0~alpha.12-1_amd64.deb.sha256'
pkexec apt-get install -y './dist/linux/fcitx5-openkey_3.1.0~alpha.12-1_amd64.deb'
# rồi restart Fcitx5 / đăng xuất-nhập lại session nếu cần
fcitx5-remote -r
```

### Checklist retest (map từng FAIL cũ)
| Ca cũ | Cách thử lại |
|---|---|
| Space/Enter/dấu câu | `xax`+Enter+`hooij` → kỳ vọng `xã` rồi dòng mới `hội`; tương tự Space/`.` |
| Home/End | Gõ từ Direct rồi Home/End — caret phải nhảy, không nuốt phím |
| Shortcut | Gõ đang soạn rồi Ctrl+A/C/V — không nuốt, không lặp từ |
| LibreOffice / Qt | `tieengs` ngay sau focus → `tiếng` (không mất `t`) |
| Firefox/Chrome address | Focus thanh địa chỉ, gõ ngay không toggle EN–VI |
| VS Code terminal | Gõ trong terminal: không mất phím (có thể chỉ English nếu thiếu capability) |

Tray: không đụng trong bản này.

## Alpha.13 fix scope (post alpha.12 desktop retest)

Desktop alpha.12 still **FAIL** on GNOME Wayland for: **Enter**, **Home/End**, **Chrome address bar**, **Qt editor**, and **LibreOffice** (first-char drop / Direct). Space/shortcuts/Firefox/VS Code terminal — do not regress if they improved on alpha.12.

Root cause treated in alpha.13: on GNOME/ibus Wayland, unaccepted keys are often dropped. Pass-through (`return` without accept) was wrong.

alpha.13 delivery:
- Enter / Home/End / shortcuts → end Direct word, `filterAndAccept` + `forwardKey`
- First Direct character (LibreOffice/Qt/Chrome wake) → `filterAndAccept` + `forwardKey` (then Surrounding `commitString`/`deleteSurroundingText`)
- Empty client preedit handshake on Direct activate / first Surrounding edit when Preedit capability is present

Package: `fcitx5-openkey 3.1.0~alpha.13` / addon `0.2.4`. **No desktop PASS claim until user retest.**

## Alpha.13 desktop REGRESSION (urgent) → rollback + alpha.14

User (Asia/Saigon ~10:18): after alpha.13 install, **ALL cases FAIL worse** — keys swallowed / not displayed, Enter does not insert newline.

Cause: alpha.13 `filterAndAccept` + `forwardKey` for Enter/Home/End/shortcuts and first Direct character. On real GNOME Wayland (ibus frontend), `forwardKey` does not reliably re-deliver; accepting the key swallows it.

Immediate relief: reinstalled **`fcitx5-openkey 3.1.0~alpha.11-1`** (confirmed via dpkg). openkey-telex active.

alpha.14 approach (must not repeat swallow):
- Enter / Home/End / shortcuts → `endDirectWord` (clear ownership) then **leave key unaccepted** (native delivery)
- First Direct character → Surrounding `commitString` only (no accept+forwardKey)
- Soft empty client-preedit handshake only (does not accept the key)
- Known alpha.11/12 FAILs (LibreOffice first-char, Chrome address bar, Qt, word-cut nuance) may remain; **basic typing and Enter newline must work**

Package target: `3.1.0~alpha.14` / addon `0.2.5`. No desktop PASS until user retest after install.

## Alpha.14 retest → alpha.15 (LibreOffice/Qt first-char)

User: alpha.14 basic typing + Enter newline **PASS** (no swallow). LibreOffice and Qt still drop/miss characters (first-char).

alpha.15 (keeps alpha.14 Enter/nav **unaccepted** after `endDirectWord`; no accept+forwardKey):
1. **LibreOffice** (`soffice.bin` / `soffice` / `libreoffice`): Direct setting falls back to **Preedit** (SurroundingText unreliable on Wayland).
2. **Direct first letter** after focus (collapsed caret, Qt and others): leave key **unaccepted** for native insert; `prepare`+`assumeAcked` syncs Surrounding tracker without `commitString` / without accept+forwardKey.

Retest focus: LibreOffice Writer `tieengs` → `tiếng` (not `iếng`); Qt editor first letters; confirm Enter newline still OK on GTK/Direct.


## Alpha.15 desktop retest (Asia/Saigon)

Installed `fcitx5-openkey 3.1.0~alpha.15-1` (addon `0.2.6`). User retest:

| Ca | Kết quả | Ghi chú |
|---|---|---|
| Enter / gõ cơ bản (so với .13/.14) | **PASS** (giữ từ .14) | Không regression nuốt phím kiểu .13 |
| LibreOffice Writer Direct/Preedit fallback | **FAIL** | Vẫn fail theo người dùng sau alpha.15 |
| Qt editor | **FAIL** | Vẫn fail theo người dùng sau alpha.15 |

Không claim desktop PASS. Giữ máy trên alpha.15 trừ khi user yêu cầu rollback.

## Alpha.16 — LibreOffice/Qt mất chữ đầu (Enter giữ nguyên)

Enter/Home/shortcut vẫn **không** `filterAndAccept` + `forwardKey`.

Alpha.15 fallback theo tên `soffice.bin` không chạy trên GNOME Wayland: ibus báo `program=gnome-shell`. Ký tự Direct đầu để unaccepted rồi chờ ACK; LibreOffice/Qt nuốt phím đó và không ACK, hàng đợi sau đó bị `forwardKey` (mất hoặc không dấu) → `tieengs` thành `iếng`.

Alpha.16 (`3.1.0~alpha.16` / addon `0.2.7`):
- Tên process LibreOffice rộng hơn (`libreoffice*`, `swriter`/`scalc`/`simpress`) khi frontend gửi đúng tên.
- Nếu ký tự native đầu **không ACK trong 250 ms**: không `forwardKey`. Ghép hàng đợi thành **preedit** cả từ (kể cả chữ đầu). Context đó ở lại Preedit đến lần focus sau.
- Khi commit, chỉ xóa chữ native đầu nếu surrounding chứng minh đúng một ký tự đó đã được chèn. Không đụng Enter.

Chưa claim desktop PASS. Retest: LibreOffice Writer và Qt editor, focus rồi gõ `tieengs` → `tiếng` (có thể gạch chân preedit). GTK/Enter không nuốt phím.

## Alpha.17 — Cursor chat nuốt `tieeps`

Trên Cursor (Electron, Wayland) chữ Direct đầu là phím unaccepted nên hiện ra, nhưng các phím sau bị nhận vào hàng đợi vô hình đến khi ACK hoặc 250 ms. Chat không vẽ preedit nên `tieeps` không thành `tiếp`. Các từ ACK kịp vẫn gõ được.

Alpha.17 (`3.1.0~alpha.17` / addon `0.2.8`):
- Phím chữ/Backspace kế tiếp, khi chữ native đầu còn pending: `commitString` phần đuôi ngay (`t` đã vào ô soạn, đuôi `iếp`). Không `forwardKey`, không xếp hàng.
- Dấu câu/space trong lúc chữ đầu còn pending đi thẳng vào app. Space sau từ đã commit cũng không nuốt; Backspace vẫn mở lại từ để thêm dấu.
- Enter/Home/shortcut vẫn unaccepted.
- ACK muộn không xóa từ đang sở hữu. Đổi vùng chọn thì bỏ sở hữu, không xóa chữ đã commit.
- Một mình chữ đầu, không có phím sau: vẫn chờ 250 ms rồi đưa lên preedit (alpha.16).

Headless: `openkey_direct_tests` pass. Chưa claim desktop PASS. Retest trong khung chat Cursor: gõ `tieeps` phải thấy `tiếp` ngay khi gõ, không chờ. Enter xuống dòng và gõ GTK cơ bản không nuốt phím. LibreOffice/Qt alpha.16 vẫn chưa được xác nhận trên desktop.

## Alpha.18 — `tiee…` trong Cursor chat

Alpha.17 đã cài. `tieeps`, `tieengs`, `tieenf`, `tieenx`, `tieen` vẫn không ra chữ. Các từ này đều có `ee` → `ê`. Chat không áp dụng `deleteSurroundingText` + `commitString` của từng phím ở bước đó, và phím đã bị nhận nên không hiện.

Alpha.18 (`3.1.0~alpha.18` / addon `0.2.9`):
- Phím chữ tiếp theo, khi chữ đầu còn chưa ACK: hiện **cả âm tiết bằng preedit** ngay (`ti`, `tie`, `tiê`, `tiên`, `tiếng`…), không `commitString` từng mảnh.
- Space/Enter/dấu câu mới ghi từ vào ô soạn. Enter vẫn unaccepted.
- GTK vẫn đi Direct khi ACK kịp, không chuyển preedit.

Chưa claim desktop PASS. Retest trong khung chat: gõ `tieen`, `tieeps`, `tieengs`, `tieenf`, `tieenx`. Kỳ vọng thấy từ có dấu gạch chân ngay khi gõ, và khi nhấn Space thì từ nằm trong câu. Enter xuống dòng không nuốt.

## Alpha.19 — đổi gạch chân và chuyển EN/VI

Alpha.18 đã cài. Đổi từ gạch chân (Preedit) sang không gạch chân (Direct) thì gõ lúc được lúc không. Chuyển EN/VI xong thì không gõ được.

Nguyên nhân: bộ đệm phím recovery còn sau khi đổi kiểu gõ hoặc tắt/bật tiếng Việt, nên mọi phím sau bị nhận vào bộ đệm đó. Một âm ACK chậm còn bật `forcePreedit` mãi, nên Direct không còn là Direct.

Alpha.19 (`3.1.0~alpha.19` / addon `0.2.10`):
- Xóa recovery khi bật lại tiếng Việt, khi tắt bộ gõ, khi reset, và khi đổi tùy chọn.
- Sau khi một âm được gửi đi, âm sau theo lại InputStyle. Direct không bị gạch chân suốt cả ô.
- Direct mà surrounding chưa sẵn: phím đi thẳng vào app, không giấu trong preedit. LibreOffice vẫn dùng preedit.
- Enter/Home/shortcut vẫn unaccepted. Không `forwardKey`.

Headless: 16/16 trên `build/linux-addon`. Chưa claim desktop PASS. Retest: đổi Preedit ↔ Direct rồi gõ `tieen`; chuyển EN rồi về VI rồi gõ `tieeps`. Enter xuống dòng không nuốt.

## Alpha.20 — từ đầu trong Cursor chat

Alpha.19 đã cài, chế độ không gạch chân. Focus khung chat Cursor, từ đầu vẫn gạch chân và sai: `tieengs` → `eengs`. Space rồi `tieengs` → `tiếng`. `chees` → `chees` (không thành `chế`).

Chat gửi surrounding text không khớp từ đang gõ, nên âm `ti` bị xóa. Phím sau không vào bộ gõ và hiện nguyên `eengs`. Khi surrounding không dùng được, cả từ `chees` cũng đi thẳng vào ô soạn.

Alpha.20 (`3.1.0~alpha.20` / addon `0.2.11`):
- Không xóa âm đang gõ chỉ vì surrounding text đổi. Enter/Escape mới bỏ âm đó nếu con trỏ đã rời chỗ.
- Client có preedit thì chữ vẫn được chuyển (`chees` → `chế`), không chèn thô.
- Enter/Home/shortcut vẫn unaccepted. Không `forwardKey`.

Headless: direct + fcitx tests pass. Chưa claim desktop PASS. Retest trong khung chat, chế độ không gạch chân: focus rồi gõ `tieengs` phải ra `tiếng`; `chees` phải ra `chế`. Enter xuống dòng không nuốt.

## Alpha.21 — `goõ`, `chees`, `chaan` trên ibus

Alpha.20 đã cài. Câu gõ ở chế độ không gạch chân ra: `goõ tiếng việt trong chees độ không gạch chaan`. Kỳ vọng: `gõ tiếng việt trong chế độ không gạch chân`.

`gox` thành `goõ` vì dấu được chèn thêm mà chữ `o` không bị xóa. `chees` và `chaan` không vào bộ gõ nên giữ nguyên. Cầu ibus của GNOME nhận `commitString` nhưng bỏ `deleteSurroundingText`.

Alpha.21 (`3.1.0~alpha.21` / addon `0.2.12`): với frontend `ibus`, cả âm được ghép rồi chèn một lần khi Space hoặc Enter. Không xóa từng ký tự. Enter vẫn unaccepted.

Headless: câu ibus ra đúng `gõ tiếng việt trong chế độ không gạch chân`, không lần xóa surrounding. Chưa claim desktop PASS. Retest đúng câu trên trong khung chat. Trong lúc đang gõ một từ có thể vẫn thấy gạch chân; sau Space từ phải đứng đúng và hết gạch chân.

## Alpha.22 — chế độ không gạch chân vẫn hiện gạch chân

Alpha.21 đã cài. Chế độ có gạch chân gõ được. Chế độ không gạch chân vẫn hiện gạch chân vì frontend `ibus` gửi preedit cả từ.

Alpha.22 (`3.1.0~alpha.22` / addon `0.2.13`): với frontend `ibus` và InputStyle Direct, không gửi preedit. Mỗi phím chữ thay cụm vừa chèn bằng từ đã ghép. Space/Enter không chèn lại từ đó. Enter/Home/shortcut vẫn unaccepted.

Headless: câu ibus ra `gõ tiếng việt trong chế độ không gạch chân`, `nonemptyPreedits == 0`. Chưa claim desktop PASS. Retest trong khung chat, chế độ không gạch chân: không có gạch chân, câu trên phải đúng. Nếu Cursor bỏ `deleteSurroundingText`, từ sẽ bị lặp; khi đó chỉ chế độ gạch chân sửa được dấu.

## Alpha.23 — `kêkết`, `viêviệt` khi không gạch chân

Alpha.22 đã cài. Cùng một tin nhắn: đoạn đầu đúng (`gõ tiếng việt ở chế độ không gạch chân`), sau đó `kết` thành `kêkết`, `việt` thành `viêviệt`. Khung soạn thảo code cũng ra `soản` thay vì `soạn`. Cursor bỏ `deleteSurroundingText` lúc được lúc không, nên từ mới bị nối sau âm cũ.

Alpha.23 (`3.1.0~alpha.23` / addon `0.2.14`): frontend `ibus`, InputStyle Direct, không gửi preedit và không gọi `deleteSurroundingText`. Âm được giữ đến Space, Enter hoặc dấu câu rồi `commitString` một lần. Enter/Home/shortcut vẫn unaccepted.

Headless: `keets` + Space ra `kết `, `vieetj soanj ` ra `việt soạn `, không lần xóa surrounding, không preedit. Chưa claim desktop PASS. Retest cả khung chat và khung soạn thảo code, chế độ không gạch chân. Trong lúc gõ một từ, chữ hiện ra khi bấm Space. `soạn` gõ `soanj`; `soanr` ra `soản`.

## Alpha.24 — chữ chỉ hiện khi nhấn Space

Alpha.23 đã cài. Chế độ không gạch chân không hiện ký tự cho đến khi nhấn Space.

Alpha.24 (`3.1.0~alpha.24` / addon `0.2.15`): frontend `ibus`, InputStyle Direct, mỗi phím ghi từ đã ghép vào ô soạn. Không gửi preedit. Space/Enter không chèn lại từ đó. Enter vẫn unaccepted.

Headless: gõ `keets` thì ô soạn đã là `kết` trước Space, preedit rỗng. Chưa claim desktop PASS. Retest: chữ phải hiện trong lúc gõ, không gạch chân, `keets` ra `kết`.

## Alpha.25 — `băngằng`

Alpha.24 đã cài. Hầu hết từ đúng. Thỉnh thoảng một từ lỗi: `bằng` ra `băngằng`. Dấu huyền thêm vào `ă` cần xóa `ăng`; Cursor bỏ lần xóa đó rồi chèn `ằng` phía sau.

Alpha.25 (`3.1.0~alpha.25` / addon `0.2.16`): khi chỉ thêm dấu lên nguyên âm đã có (`ă` → `ằ`, `ê` → `ế`), không xóa nguyên âm. Xóa phụ âm ASCII phía sau rồi gắn dấu kết hợp. Không gửi preedit. Enter vẫn unaccepted.

Headless: client bỏ mọi lần xóa có chữ có dấu, `bawngf` vẫn ra `bằng`, gõ tiếp `m` ra `bằngm`. Chưa claim desktop PASS. Retest câu có `bằng` (`bawngf`).

## Alpha.26 — `gõ`, `ích`

Alpha.25 gắn dấu kết hợp lên `a/e/i/o/u/y`, nên `gõ` hiện `gõ` và `ích` hiện `ích`. Khi Gõ nhanh Telex tắt, `icsc` ra `ícc` (`ích` là `ichs`, `isch` hoặc `icsh`). Khi Gõ nhanh Telex bật, `cc` thành `ch`, nên `icsc` ra `ích`.

Alpha.26 (`3.1.0~alpha.26` / addon `0.2.17`): nguyên âm ASCII được xóa và thay bằng chữ có dấu ghép sẵn. `ă/ê/ô/ơ/ư` vẫn gắn dấu kết hợp để khỏi lặp `băngằng`.

Headless: Quick Telex tắt, `gox ichs icsc` ra đúng `gõ ích ícc`, không có dấu kết hợp U+0301/U+0303. Quick Telex bật, frontend `ibus`, `icsc` ra `ích` rồi Space ra `ích `. Chưa claim desktop PASS. Cấu hình máy này đang `QuickTelex=True`, gói đã cài là alpha.26.

## Alpha.27 — Chrome `goo` thành `goô`

Alpha.26 đã cài. Trong Chrome, `goo` ra `goô`. Chrome bỏ `deleteSurroundingText`, nên `o` không bị xóa khi đổi thành `ô`.

Alpha.27 (`3.1.0~alpha.27` / addon `0.2.18`): ô nhập Chrome (có cờ hoa đầu câu, chính tả, URL hoặc email) gắn dấu mũ/sừng/breve vào nguyên âm ASCII, không xóa nguyên âm đó. Cursor chat không có các cờ này nên vẫn xóa và ghi chữ ghép sẵn (`gô`, `gõ`). Không gửi preedit. Enter vẫn unaccepted.

Headless: client bỏ mọi lần xóa, cờ `UppwercaseSentences`, `goo` không còn `goô`, NFC là `gô`; `goox aa ee ow uw aw ` NFC là `gỗ â ê ơ ư ă `. Cursor giả lập vẫn ra đúng chữ `gô` ghép sẵn. Chưa claim desktop PASS. Retest Chrome: `goo` phải thành `gô`.

## Alpha.28 — terminal VS Code/Cursor

Alpha.27 đã cài. Trong terminal VS Code/Cursor, `ddaay laf nooij dung nhaapj bawngf tieengs vieetj` ra `laà noôi dung nhaâp bang tieêng vieêt`. Editor vẫn đúng.

Nguyên nhân: terminal là xterm.js. Nó chỉ gửi phần chữ được chèn xuống shell, không gửi phần bị xóa. Direct trên ibus thay chữ bằng `deleteSurroundingText` + `commitString`, nên lần xóa không tới shell: `noo` → `noô`, `laf` → `laà`, còn `đ`, `ă` và dấu thanh bị mất. Ô nhập ẩn của xterm không có `autocomplete="off"` (Monaco có), nên Chromium báo cờ `WordCompletion`. Log diagnostics lúc người dùng gõ trong terminal có capabilities `262226` (có `WordCompletion`). Editor là `82`, chat là `1048658`.

Alpha.28 (`3.1.0~alpha.28` / addon `0.2.19`): trên ibus + Direct, khi ô nhập có cờ `Terminal`, có `WordCompletion` hoặc không có `SurroundingText`, OpenKey dùng **preedit** (chữ gạch chân trong lúc gõ). Không xóa, không ghi từng mảnh. Space/Enter/dấu câu ghi từ một lần rồi phím đi tiếp vào app, không bị nhận. Không `forwardKey`. Editor/chat giữ Direct như cũ.

Headless: 16/16 trên `build/linux-addon`. Có ca giả lập terminal bỏ mọi lần xóa với 3 bộ cờ: câu trên ra đúng, 0 lần xóa, Enter vẫn unaccepted. Chưa claim desktop PASS. Retest: gõ câu trên trong terminal Cursor/VS Code, kỳ vọng chữ gạch chân khi gõ và đúng sau Space/Enter. Gõ `ls` + Enter phải chạy lệnh. Smoke GTK: Space/Enter vẫn bình thường.

## Alpha.29 — terminal vẫn `dđaây laà`

Alpha.28 đã cài và Fcitx5 đã nạp `openkey.so` mới. Kết quả terminal: `dđaây laà noôi dung nhaâp bang tieêng vieêt`. Alpha.27 ra `laà…` và mất hẳn `đây`; alpha.28 hiện `dđaây`, tức là terminal đã nhận preedit mới. Nghĩa là cổng nhận diện đã bật, nhưng **preedit cũng bị lộ**: xterm.js so ô nhập ẩn sau mỗi phím và chỉ gửi phần mới thêm xuống shell, nên `d` → `đ` → `đa` → `đâ` → `đây` thành `dđaây`.

Alpha.29 (`3.1.0~alpha.29` / addon `0.2.20`): ô ibus + Direct có cờ `Terminal` hoặc `WordCompletion` không nhận client preedit, không xóa. Âm tiết giữ trong panel của Fcitx/GNOME (nếu panel hiện được); Space/Enter/dấu câu gửi **một lần** `commitString` cả từ, rồi phím đi tiếp vào app (unaccepted). Ô không có SurroundingText (không phải terminal) vẫn dùng client preedit như alpha.28.

Headless: 16/16. Ca terminal kiểm không có client preedit, không xóa, không `dđ`/`laà`. Chưa claim desktop PASS. Fcitx5 chạy với log `/tmp/openkey-fcitx5.log` (DirectDiagnostics bật); khi focus terminal, chạy `busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 DebugInfo` để xem cap (hex).

## Alpha.30 — rollback heuristic terminal, sửa `mer`

Alpha.29: terminal vẫn FAIL (`dđay laà …`); app thường hỏng: `mer` → `mr`, `mẻ` có dấu rời.

Log `/tmp/openkey-fcitx5-alpha29.log`: `fallback:commit-only-terminal` **không lần nào** chạy (không có ô nào báo `WordCompletion`/`Terminal`). `fallback:preedit-terminal` chạy ở cap `1048594`, `4114`, `18`: đó là ô bình thường lúc vừa focus, GNOME chưa báo SurroundingText. Phím đầu vào preedit rồi từ bị hỏng (`mr`). Terminal Cursor báo cùng cap với editor (`82`), nên không phân biệt được bằng cờ.

Dấu rời `mẻ`: từ alpha.27, ô có cờ hoa đầu câu bị coi là Chrome và nhận dấu kết hợp. Cursor chat (`1048658`) có cờ này và vẫn xóa được, nên hiện dấu rời.

Alpha.30 (`3.1.0~alpha.30` / addon `0.2.21`):
- Bỏ cổng `WordCompletion` và "thiếu SurroundingText". Chỉ cờ `Terminal` thật mới vào chế độ commit-only (hiện chưa ô nào báo).
- Heuristic Chrome chỉ còn `SpellCheck`/`Url`/`Email`. Cursor chat ra chữ ghép sẵn `mẻ`.
- Terminal VS Code/Cursor: **FAIL đã biết**, chưa sửa.

Headless 16/16: ibus Direct `mer` → `mẻ` (không U+0309, không preedit) với cap không surrounding, hoa đầu câu, WordCompletion, thường; SurroundingText đến sau phím đầu vẫn ra `mẻ`.

## Alpha.31 — luôn ghi chữ ghép sẵn (NFC)

Alpha.30: gõ được dấu nhưng dấu vẫn rời chữ. Log chỉ có cap `1048658` (Cursor chat), không qua nhánh Chrome. Nguồn dấu rời còn lại là nhánh alpha.25: thêm dấu lên `ă/ê/ô/ơ/ư` thì giữ nguyên âm và gắn dấu kết hợp (né `băngằng`). Nhánh Chrome alpha.27 cũng gắn dấu kết hợp cho ô có SpellCheck.

Alpha.31 (`3.1.0~alpha.31` / addon `0.2.22`):
- Bỏ nhánh dấu kết hợp alpha.25. Luôn xóa rồi ghi chữ ghép sẵn (`tiếng`, `bằng`, `mẻ`).
- Nhánh dấu kết hợp Chrome chỉ còn cho ô `Url`/`Email` (thanh địa chỉ).
- Rủi ro: lỗi `băngằng` thỉnh thoảng có thể quay lại trong Cursor chat; ô Chrome có chính tả có thể ra `goô`. Terminal vẫn FAIL.

Headless 16/16: `mer bawngf tieengs vieetj nooij ddaay` ra đúng NFC, không có ký tự U+0300–U+036F, với cap thường, hoa đầu câu, hoa đầu câu + SpellCheck.

## Alpha.32 — candidate mất từ đầu sau Enter trên VS Code Wayland

Người dùng xác nhận sau mỗi Enter chạy lệnh, từ tiếp theo bị nuốt cho đến Space.
Đối chứng `abc def` → `' deè'`: mất cả ASCII đầu, không chỉ lỗi chuyển dấu.

Candidate alpha.32 / addon 0.2.23: đường ibus Direct cho phím chữ đầu sau Enter
đi native, không commitString hoặc synthesize phím. Chỉ nhận lại ownership nếu
surrounding khớp chính xác chữ native vừa nhập; không ACK thì raw phần còn lại
của từ để tránh nuốt/xóa nhầm, có thể không chuyển dấu từ đó. Không đoán terminal
bằng capability; tác động cả editor/chat ibus sau Enter. Không sửa lỗi `deè`.

Test boundary cũ đạt cả alpha.31, nên không tái hiện native. Regression mới yêu
cầu phím đầu unaccepted + không double-commit và fault injection bỏ IM commits
sau Enter cho đến native text key. Binary test mới với bản sao alpha.31 trong
thư mục tạm FAIL tại assert native-prime; candidate đạt. Mô hình này không phải
xterm.js thật, không được coi là desktop PASS.

Build alpha.32: 8/8 CTest, gói DEB/SHA-256, verifier dependency/payload và test
addon giải nén đạt. Không cài gói/restart Fcitx5. Nghiệm thu desktop còn chờ:
Enter → `abc xyz`, lặp nhiều dòng; Telex/VNI; ACK trễ; editor/chat, gợi ý và
Backspace. Không dùng gói thử cho lệnh quan trọng trước khi kiểm chứng.

## Alpha.33 — `ls` chỉ hiện `l` trên alpha.32

Người dùng xác nhận cài alpha.32: `abc xyz` giữ được từ đầu; `ls` chỉ hiện `l`.
Đây là FAIL còn lại, không coi alpha.32 đạt nghiệm thu terminal.

Engine Composition trả `ls` đúng. Candidate alpha.33 giữ toàn bộ chuỗi nối
thêm nguyên dạng đi native sau Enter, thay vì chỉ ký tự đầu rồi commitString.
Mỗi ký tự phải có surrounding ACK chính xác trước khi giữ quyền sửa từ;
không ACK thì raw như alpha.32. Gặp chuyển dấu thật vẫn dùng rewrite hiện có,
không tuyên bố sửa toàn bộ tiếng Việt trên xterm.js hoặc lỗi `deè`.

Regression `Enter → ls/git/cd` yêu cầu từng chữ unaccepted và không commit
trùng; FAIL alpha.32 trước patch, PASS bản mới. Test Composition độc lập các
từ trên đạt. Build alpha.33/addon 0.2.24: 8/8 CTest, verifier dependency/payload/
addon giải nén đạt. Chưa cài gói hoặc restart daemon; desktop còn chờ người dùng.

## Alpha.34 — mất từ đầu khi focus vào terminal

Desktop alpha.33: focus từ app khác hoặc editor vào terminal VS Code thì từ
đầu không hiển thị, Space mới gõ được. Dpkg xác nhận alpha.33 đang cài.

Candidate alpha.34/addon 0.2.25 mở rộng native start từ sau Enter sang mọi từ
ASCII nguyên dạng mới, vẫn giữ exact surrounding ACK và raw fallback khi
không xác nhận được. Không force English cho mọi từ; Telex/VNI chuyển dấu
thực sự vẫn qua rewrite cũ. Không claim sửa toàn bộ tiếng Việt trên xterm.js.

Regression focus-entry Telex/VNI FAIL alpha.33 trước patch, PASS bản mới:
không Enter trước `ls abc`, refocus `git`, đổi tài liệu trên cùng context khi
còn native draft, không double-commit khi focus-out. 8/8 CTest đạt ở lần đầu;
thêm guard biến đổi phím đầu `w` → `ư` vào lần đóng gói cuối và vẫn đạt 8/8.
Verifier dependency/payload/addon giải nén và checksum alpha.34 đạt; không tự
cài hoặc restart daemon. Desktop còn chờ:
click terminal từ app khác/editor, gõ `ls` ngay không Space; lặp nhiều lần,
Enter nhiều dòng, và retest editor/chat, completion, Backspace, Telex/VNI.
