import QtQuick

QtObject {
    required property string result
    readonly property string text: ({offline: "Đang offline; không đọc hoặc áp dụng cấu hình.",
        loading: "Đang đọc cấu hình từ Fcitx5…", loaded: "Đã đọc cấu hình runtime của addon OpenKey.",
        applying: "Đang áp dụng và đọc lại để xác nhận…",
        confirmed: "Đã xác nhận thiết lập trong runtime. Cấu hình đã lưu được hiển thị riêng; chưa xác nhận lưu file thành công.",
        conflict: "Thiết lập này đã đổi ở nơi khác. Nhấn Đọc lại trước khi thử tiếp.",
        unavailable: "Không kết nối được Fcitx5 hiện có. Giao diện không tự khởi động daemon.",
        error: "Không đọc được cấu hình/schema tương thích. Chưa gửi thay đổi.",
        unsupported: "Addon đang chạy chưa hỗ trợ thiết lập này. Cập nhật addon rồi nhấn Đọc lại; chưa gửi thay đổi.",
        unconfirmed: "Chưa xác nhận kết quả; thay đổi có thể đã được áp dụng. Nhấn Đọc lại, không tự gửi lại.",
        invalid: "Giá trị thiết lập không hợp lệ. Nhấn Đọc lại."})[result] || "Chưa xác định"
}
