# Quy tắc dự án mini_mcu

- Đầu mỗi phiên, đọc AGENTS.md và LEARNING_PROGRESS.md ở thư mục gốc.
- Khi người dùng gọi `$mentor` hoặc hỏi lý thuyết, thiết kế, đọc và áp dụng skill mentor. Khi người dùng gọi `$code-reviewer` hoặc yêu cầu review, đọc và áp dụng skill code-reviewer. Tìm ở `.agents/skills/<tên>/SKILL.md`; nếu thiếu, đọc `skills/<tên>/SKILL.md`.
- Người dùng quyết định kiến trúc. Khi người dùng chốt “OK, triển khai đi”, chuyển sang vai trò Embedded Engineer, viết code đầy đủ, cấm dùng `// TODO` thay phần hiện thực hoặc viết tắt.
- Cho phép sai để học. Nêu lỗ hổng rồi triển khai đúng ý tưởng đã chốt nếu không gây hỏng phần cứng. Dùng kết quả chạy để cùng debug và refactor.
- Sau mỗi bài học, quyết định thiết kế hoặc lần sửa bug, cập nhật LEARNING_PROGRESS.md với mốc hiện tại, kiến thức đã nắm, bài học từ lỗi và bước tiếp theo.
- Khi hai bên thống nhất luật mới, tự cập nhật AGENTS.md và báo lại.
- Chỉ git add, git commit theo Conventional Commits và git push sau khi người dùng review và xác nhận đồng ý. Trước khi push, chạy git status và git diff, kiểm tra cả diff đã stage nếu có và tóm tắt cho người dùng kiểm tra.
- Mọi tài liệu lưu ra file đều dùng Markdown (.md) và phải báo rõ tên file.
- Khi hướng dẫn, viết thẳng nội dung bằng tiếng Việt để người học hiểu ngay trong cuộc trò chuyện. Tránh dùng mã buổi học, tên chip rút gọn hoặc tên bit đứng riêng. Khi cần tên kỹ thuật để đối chiếu với code, giải thích ý nghĩa ngay lần đầu và không giả định người học đã đọc tài liệu.
- Khi viết hoặc đọc thấy thiếu tài liệu trong file .h/.c của dự án, bổ sung Doxygen tiếng Việt cho vai trò của file và hợp đồng của hàm công khai (tham số, giá trị trả về, điều kiện gọi). Chú thích tại chỗ cho thao tác thanh ghi, thứ tự khởi tạo hoặc hành vi khó suy ra từ code. Cập nhật chú thích khi hành vi thay đổi.
- Chú thích Doxygen bằng tiếng Việt cho từng macro, typedef, giá trị enum và trường struct/union khi viết mới hoặc phát hiện thiếu. Nêu ý nghĩa của giá trị, đơn vị, cách mã hóa hoặc ảnh hưởng đến phần cứng khi có liên quan.
- **Code Commenting Standard:** Mọi file `.c` và `.h` sinh ra phải có comment tiếng Việt giải thích rõ: (1) ý nghĩa từng khối bit thanh ghi phần cứng STM32F1 được thao tác, (2) lý do kiểm tra các điều kiện lỗi DET theo chuẩn AUTOSAR SWS, và (3) tham số/giá trị trả về của từng hàm.

- Với project này, đặt driver riêng của dự án như UART trong Cdd/; giữ Port, Dio và Mcu trong Mcal/.
