# AppImage giao diện — thử nghiệm

Gói chỉ chứa OpenKey Settings, thư viện Qt/QML và các thư viện phụ thuộc
được lấy từ máy build. Không thay gói addon Fcitx5, tray, cấu hình IM hoặc
đăng ký autostart. Không có cơ chế cập nhật tự động.

Build hiện tại dùng Ubuntu 26.04 amd64; **không bảo đảm chạy trên Ubuntu cũ**.
libc, loader, font hệ thống và driver đồ họa vẫn lấy từ host. Chạy đúng target
và nghiệm thu desktop riêng; smoke offscreen chỉ xác nhận nạp resource/plugin.

## Build

Sau khi build/test UI native, cung cấp appimagetool và type2 runtime x86_64:

```bash
python3 scripts/build-linux-appimage.py \
  --binary build/ubuntu-ui-deb/Sources/OpenKey/linux/ui/openkey-settings \
  --appimagetool build/appimage-tools/extracted/squashfs-root/AppRun \
  --runtime build/appimage-tools/runtime-x86_64 \
  --output dist/linux/OpenKey-Settings-preview.8-x86_64.AppImage
```

Script không tải tool hoặc cài dependency; cần Python 3, qmake6, ldd,
dpkg-query và Git. Không ghi đè output cũ; chọn đường dẫn mới nếu build lại.
AppDir được giữ trong build để đối chiếu. Tool chính thức:
[appimagetool](https://github.com/AppImage/appimagetool),
[type2 runtime](https://github.com/AppImage/type2-runtime).
Đối chiếu checksum từ nguồn phát hành trước khi chạy tool.

Tool đã lấy cho lần thử 2026-11-07 (continuous, hash cố định của lần tải):

| Thành phần | SHA-256 |
| --- | --- |
| appimagetool-x86_64.AppImage | e8c1169c4af66c3184d7ee110713d4be04befe671fa358e2c1591ee840553b03 |
| runtime-x86_64 | 617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46 |

## Chạy và giấy phép

Kiểm SHA-256 rồi chạy bằng tài khoản thường. Nếu máy không có FUSE, runtime
có đường chạy giải nén với `APPIMAGE_EXTRACT_AND_RUN=1`; có thể thử cùng
`--smoke-test` mà không truy vấn session hoặc ghi cấu hình.

Binary UI đang mở có thể đã nắm singleton `org.openkey.Settings`; đóng nó
trước khi mở AppImage để tránh chỉ kích hoạt lại phiên bản cũ.

OpenKey GPL-3.0: LICENSE và source tương ứng của OpenKey trong
`usr/share/doc/openkey-settings/` của AppDir/AppImage. Copyright notices của
thư viện được bundle ở `usr/share/licenses/`; Qt dùng thư viện shared, không
liên kết tĩnh. Có thể giải nén AppImage, thay thư viện tương thích và đóng gói
lại; không có kiểm tra chữ ký để khóa việc thay thư viện. Source thư viện Qt
và các dependency do nhà phân phối ghi trong các copyright notices tương ứng.

## Preview.9

Đã tạo `OpenKey-Settings-preview.9-x86_64.AppImage` từ binary UI mới; chứa
trang macro, editor phím tắt và quy tắc ứng dụng. Addon alpha.49 cài riêng.
AppDir và image extract-and-run đạt smoke offscreen; chưa nghiệm thu desktop.

## Preview.11

Lệnh build ở trên áp dụng với tên output `OpenKey-Settings-preview.11-x86_64.AppImage`.
Bản này thêm năm tùy chọn nâng cao, sao lưu IM/hotkey và chuyển file bằng
tiến trình con của binary UI. File là văn bản UTF-8, nguồn tối đa 16 MiB,
kết quả 64 MiB, timeout 30 giây; addon alpha.50 cài riêng.
Xem [hướng dẫn preview.11](UBUNTU_FEATURES_50.md). Smoke offscreen không
thay thế nghiệm thu AppImage/file chooser trên desktop.

Đã tạo preview.11 amd64: AppDir smoke và image thật extract-and-run smoke đạt.
Worker của image thật chuyển nguồn UTF-8 177.000 byte sang NFD đúng, giữ nguyên
nguồn; checksum image và hai gói DEB đều khớp. Chưa nghiệm thu desktop/FUSE.

## Preview.12

UI thêm ShowPage để menu service mới mở Gõ tắt/Chuyển mã/Giới thiệu trong
cửa sổ đang chạy. AppImage chỉ chứa UI; cần service mới riêng, xem
[hướng dẫn menu](UBUNTU_TRAY_PAGES.md). Đóng cửa sổ UI cũ trước khi mở image
để singleton thuộc binary mới; không tự restart bộ gõ.

Đã tạo image preview.12 amd64; AppDir và image thật đạt smoke offscreen qua
extract-and-run, SHA-256 khớp. Chưa nghiệm thu focus/menu và FUSE desktop.
