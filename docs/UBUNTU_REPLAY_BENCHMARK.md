# Đo replay của Composition

Đây là microbenchmark CPU cho adapter replay, không phải độ trễ khi gõ trên
desktop và không đo D-Bus, compositor, ACK, gợi ý code hoặc undo/redo.
Số đo lịch sử dưới đây dùng engine global; alpha.50 đã có context riêng cho
Composition. Lookup table vẫn dùng trên event thread trong addon.

```bash
cmake -S . -B build/ubuntu-deb -DOPENKEY_BUILD_BENCHMARKS=ON
cmake --build build/ubuntu-deb --target openkey_replay_benchmark
build/ubuntu-deb/openkey_replay_benchmark
```

Mỗi ca chạy 500 vòng, bỏ 20 vòng warm-up. Mỗi mẫu đo một append gồm replay
lịch sử hiện tại và tạo text; case từ dài tăng dần đến giới hạn 256 sự kiện.
Đầu ra JSON chỉ dùng fixture cố định, không đọc phím/nội dung ứng dụng.

Kết quả tham khảo 2026-11-07, Ubuntu 26.04 amd64, GCC 15.2, Release, máy đang
chạy các công việc khác. Đơn vị µs/append; số đo có jitter và không phải SLA.

| Ca | Số mẫu | p50 | p95 | p99 | Max |
| --- | ---: | ---: | ---: | ---: | ---: |
| Telex word | 2880 | 0.205 | 0.577 | 0.627 | 84.644 |
| VNI word | 2880 | 0.177 | 0.481 | 0.518 | 59.903 |
| Word tối đa | 122880 | 4.830 | 9.004 | 17.069 | 4393.669 |

Max cao hơn percentile nhiều cho thấy không nên suy ra deadline desktop từ
một lần chạy. Đây là số đo trước khi có context riêng;
không dùng số đo này để bỏ guard ACK hoặc công bố frontend đã tương thích.

## Context so với replay — 2026-11-08

Cùng lệnh ở trên hiện chạy cả Composition context và bản replay tham chiếu
đóng băng trước refactor. 500 vòng, bỏ 20 warm-up, đo append và tạo text.
Ubuntu 26.04 amd64/GCC 15.2/Release; máy có tác vụ khác, đơn vị µs/append.

| Cách xử lý / ca | Số mẫu | p50 | p95 | p99 | Max |
| --- | ---: | ---: | ---: | ---: | ---: |
| Context Telex | 2880 | 0.065 | 0.168 | 0.189 | 38.027 |
| Context VNI | 2880 | 0.064 | 0.168 | 0.175 | 0.209 |
| Context từ tối đa | 122880 | 0.203 | 0.346 | 0.524 | 3014.923 |
| Replay Telex | 2880 | 0.164 | 0.376 | 0.431 | 2017.601 |
| Replay VNI | 2880 | 0.157 | 0.365 | 0.376 | 1.881 |
| Replay từ tối đa | 122880 | 5.044 | 10.115 | 19.496 | 6044.109 |

Context tránh replay theo từng phím; đổi tùy chọn vẫn replay lịch sử tối đa
256 sự kiện. Không đo chi phí copy context nhánh Direct, frontend, ACK, D-Bus
hoặc latency toàn hệ thống. Max nhiễu lớn; không suy ra SLA gõ nhanh từ số đo.
