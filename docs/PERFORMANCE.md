# Kiểm tra hiệu năng OpenKey

## Engine độc lập

Chạy `./scripts/benchmark-engine.ps1 x64` và `./scripts/benchmark-engine.ps1 x86`.
Harness build `/O2`, kiểm tra correctness rồi đo bốn kịch bản Telex/VNI.
Đường dẫn báo cáo JSON được in sau khi chạy.

Đây là thời gian xử lý trong engine, không bao gồm keyboard hook,
SendInput, clipboard, ứng dụng nhận chữ hoặc giao diện Fluent. Percentile
trong báo cáo là percentile của trung bình batch, không phải độ trễ từng phím.
Không so sánh các lần chạy khác máy/cấu hình như một kết quả cải thiện.

## CPU và bộ nhớ tiến trình đang chạy

Lấy PID trong Task Manager hoặc dùng:

```powershell
Get-Process OpenKey,OpenKey32,OpenKey64,OpenKey.UI -ErrorAction SilentlyContinue |
    Select-Object Id,ProcessName,Path
```

Đo một tiến trình cụ thể (thay `1234` bằng PID thực tế):

```powershell
.\scripts\measure-runtime.ps1 -ProcessId 1234 -Seconds 15 -Scenario tray-idle
```

`Seconds` từ 2 đến 60; `Scenario` là `unspecified`, `tray-idle`, `ui-idle`
hoặc `typing`. Nhãn do người đo chọn, script không tự xác minh trạng thái.
Script chỉ đọc tiến trình, không mở/đóng OpenKey, không gửi phím, không thay
registry. Nếu tiến trình thoát trong lúc đo, phép đo dừng và không ghi báo cáo hoàn tất.
JSON riêng trong `dist/performance-*/runtime.json` chứa executable, SHA-256,
phiên bản, thời gian đo, mẫu thô và kết quả tổng hợp.

Working set là RAM resident; private bytes là bộ nhớ riêng đã commit,
không phải cùng một đại lượng. CPU được tính từ chênh lệch bộ đếm trong
khoảng đo thực tế, không lấy CPU tích lũy từ khi ứng dụng khởi động.
`cpuPercentOfOneCore` có thể vượt 100% với tiến trình đa luồng;
`cpuPercentOfMachine` chia tiếp cho số logical processor của môi trường đo.
Các khoảng đo ngắn có thể hiện 0 CPU do độ phân giải bộ đếm, không chứng minh
tiến trình không bao giờ sử dụng CPU. Debug và Release cần đo riêng.

Để so sánh, giữ nguyên máy, cấu hình nguồn điện, ứng dụng nền, thiết lập gõ,
kịch bản và thời lượng; lưu báo cáo trước/sau. Tránh chạy compiler/benchmark
khác cùng lúc lấy mẫu. Đo engine và UI riêng nếu cả hai đang mở.

## Các phép đo chưa được thay thế bằng harness

Ngày 2026-10-11, người dùng yêu cầu đóng hai hạng mục khởi động/bộ nhớ tray
và độ trễ gõ trong checklist, không yêu cầu đo nghiệm thu riêng thêm.
Đây là quyết định chốt phạm vi, không phải xác nhận đã có số đo khởi động
nguội hoặc so sánh độ trễ end-to-end với baseline. Các hướng dẫn dưới đây
được giữ để sử dụng khi cần.

- Khởi động nguội: đo trên máy sạch hoặc sau reboot với mốc sẵn sàng tray/UI
  được định nghĩa rõ; `Start-Process` trả về hoặc `WaitForInputIdle` không đủ
  chứng minh keyboard hook/tray đã sẵn sàng.
- Độ trễ gõ end-to-end: đo cùng ứng dụng đích, cùng keyboard layout, với UI
  đóng/mở và macro/clipboard tương ứng; cần baseline trên cùng máy.
- CPU/bộ nhớ bản Release khi chỉ chạy tray: đo riêng, không suy từ Debug.

`./scripts/test-runtime-performance.ps1` kiểm tra phép tính bằng fixture trên PowerShell
7/5.1; không gửi phím hoặc khởi chạy ứng dụng. Suite này cũng nằm trong
`./test-all.ps1 All`. Test đạt không đồng nghĩa nghiệm thu hiệu năng thực tế.
