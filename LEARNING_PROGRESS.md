# Tiến độ học tập · Portfolio MCU Automotive

## Trạng thái hiện tại

- Ngày khởi tạo hồ sơ 18/11/2026. Theo [Roadmap v3](Roadmap_MCU_Automotive_v3.md), dự án đang ở giai đoạn A, đầu S1 (17/11 theo lịch gốc). Ngày 20/11 đã chốt thiết kế và build firmware S1. Chưa có buổi thực hành nào được xác nhận hoàn thành.
- Đã chốt dùng GDB trong WSL2, kết nối OpenOCD trên Windows qua ST-LINK. Đã xác nhận OpenOCD trên Windows kết nối được board thứ nhất.
- S1 đã có `gdb-multiarch` trong WSL2 và các file startup GCC, header, system trong `~/STM32CubeF1`. Người học xác nhận đã cài driver ST-LINK và giải nén OpenOCD trên Windows. Board thứ nhất đã kết nối SWD ở 100 kHz sau khi người học xóa flash bằng ST-LINK Utility. OpenOCD đọc DPIDR 0x1ba01477, nhận Cortex-M3 r1p1 và halt thành công. Đã nạp mini_mcu_s1.elf lần đầu, OpenOCD báo Verified OK và reset. Device ID 0x10006412, flash báo 32 KiB trong khi linker S1 giả định 64 KiB. Người học xác nhận board đang cắm là STM32F103C6T6, board còn lại là STM32F103C8T6. Firmware vừa nạp dùng cấu hình C8. Đã chốt và build riêng C6/C8 bằng make MCU=c6 hoặc MCU=c8. Đã kiểm tra map, vector reset và MSP lần lượt 0x20002800 và 0x20005000.
- Mốc gần nhất là M1 ở cuối S8, khi bxCAN chạy loopback, luồng dữ liệu đi qua `Can → CanIf → ComLite` và kiểm thử host của `ComLite` đạt. M1+ ở S11 kiểm chứng trên bus thật với hai board.
- Ban đầu workspace có `AGENTS.md`, hai tài liệu CAN và roadmap. Chưa có firmware, Makefile, linker script, startup hay test. Theo roadmap, ring buffer cần tái sử dụng ở S3 nằm trong dự án cũ.
- Git không nhận diện được repository trong phiên này dù thư mục `.git/` hiện diện. WSL2 có `make`, `gcc` của host. Người học đã chạy thành công `arm-none-eabi-gcc --version` (14.2.1) và `arm-none-eabi-objcopy --version` (2.45.50.20251209). Chưa xác nhận USB-UART hoặc loại transceiver CAN.

## Kiến thức và kỹ năng đã nắm vững

Đã đối chiếu [CAN Phần 1](CAN_Phan1_Nen_tang_giao_thuc.md) và [CAN Phần 2](CAN_Phan2_FD_DBC_UDS_Linux.md) với lộ trình. Chưa ghi nhận kỹ năng mới đã được người học chứng minh qua bài tập, log, phép đo hoặc unit test của dự án này.

Người học đã thực hiện được thao tác kết nối và halt Cortex-M3 bằng OpenOCD trên Windows.

## Sai lầm đã gặp và bài học rút ra

Lần link đầu kéo theo startup mặc định của toolchain và cảnh báo syscall của newlib. Makefile hiện dùng startup CMSIS, chọn rõ crti/crtn cho hàm khởi tạo C. Build cuối thành công, bảng vector ở 0x08000000, SP ban đầu 0x20005000 và vector reset trỏ tới Reset_Handler ở chế độ Thumb.

Lần nạp đầu dùng ELF C8 trên board C6. Verify chỉ xác nhận dữ liệu flash; cần đối chiếu mã chip và đỉnh stack trong bảng vector.

## Việc cần làm tiếp theo

1. Đã chốt startup CMSIS, module board quản lý clock và LED, main điều khiển nhịp nháy. Firmware đã build cho STM32F103C8 (64 KiB flash, 20 KiB SRAM).
2. Cuối buổi, người học xác nhận LED PC13 trên C6 đã nháy. Người học muốn hiểu lại các bước đã làm. Buổi sau bắt đầu từ main() và lần theo một lần bật/tắt LED, sau đó nối lại các bước build, nạp và reset. Còn chờ đọc RCC_CFGR.SWS, thử nạp lại và kiểm tra board C8.
3. Ghi kết quả thực tế rồi chuyển sang S2 (USART1, SysTick 1 ms và std_types.h). Chờ người học review trước khi commit.

## Mốc tiếp tục ngày 20/11/2026 sau khi tổ chức lại hướng dẫn

S1 của M1 đã có src/main.c, board/board.c, board/board.h, Makefile, linker script riêng C6/C8 và thư mục build/. LED C6 đã nháy theo xác nhận trước đây. Git status ở thư mục gốc vẫn báo không nhận diện được repository. Hai skill hiện nằm ở skills/ vì .agents/ chỉ cho đọc trong phiên này.

Nội dung ôn S1 là đường đi từ Board_SetLed tới GPIOC_BSRR và cực tính active-low của PC13, cùng sự khác nhau giữa yêu cầu chọn clock (SW) và nguồn thực sự đang chạy (SWS). Chờ người học giải thích và kiểm tra SWS trên board. Thiết kế hiện tại dùng module board với busy-wait. Phương án đưa SysTick từ S2 lên S1 đang ở mức thảo luận.

### Quyết định cho buổi S1

Người học chọn hướng A cho buổi này, giữ busy-wait để kiểm chứng firmware hiện tại. Tiếp theo đọc RCC_CFGR.SWS trên C6. Đang thảo luận cách học AUTOSAR Classic qua module do Gemini soạn, chưa chốt phạm vi hay thay đổi firmware.

Định hướng học: viết từng module khi bài thực hành cần đến. S1 tiếp tục kiểm chứng C6; việc tách Dio, Port, Mcu hoặc module thời gian chưa được chốt.

Đang thảo luận vị trí của Dio trong kiến trúc AUTOSAR Classic. Chưa chốt thêm RTE hoặc đổi cấu trúc firmware.

## Tích hợp thư viện điều khiển chân số

- Ứng dụng chạy từ App/main.c. Module board giữ việc chọn nguồn xung nhịp, gọi Port_Init để cấu hình chân 13 cổng C và dùng Dio_WriteChannel để bật đèn ở mức thấp. Port đặt mức cao trước khi chuyển chân sang đầu ra.
- Sửa tên Platform_Types.h cho khớp lệnh include. Makefile chọn đúng nguồn ứng dụng và biên dịch cả các bảng cấu hình cùng dịch vụ ghi nhận lỗi Det.
- Đã build và liên kết thành công cho STM32F103C6T6 và STM32F103C8T6. Kết quả LED nháy trước đây thuộc firmware cũ. Bước tiếp theo là nạp bản mới đúng chip, quan sát đèn và kiểm tra nguồn xung nhịp thực tế.

Người học đã nạp firmware tích hợp Port và Dio lên STM32F103C6T6 qua ST-LINK. OpenOCD báo Verified OK, reset và người học quan sát LED nháy. Bước học tiếp theo là lần theo một lần bật đèn từ main tới chân vật lý.

Đang ôn vai trò của OpenOCD, Common, Det, Port và Dio qua một lần bật đèn. Chưa đánh dấu người học đã nắm vững; tiếp theo đối chiếu lời gọi trong board với phép ghi chân trong Dio.

## Tích hợp quản lý xung nhịp ngày 21/11/2026

Board_Init chuẩn bị chân đèn, khởi tạo Mcu rồi yêu cầu nguồn thạch anh ngoài 8 MHz. So sánh kết quả với E_OK để chuyển đúng sang bool. Đã build lại đầy đủ cho STM32F103C6T6 và STM32F103C8T6. Cần nạp lại để xác nhận nguồn xung và đèn trên bo với firmware mới.

Ngày 21/11: Người học báo test UART trên host đạt. Đã đối chiếu cấu trúc: Dio có đặc tả AUTOSAR Classic; UART là driver riêng, cách tách phần tính baud, bộ đệm và DMA do dự án quyết định. Kiểm thử phần cứng UART chưa được xác nhận.

Đã chuyển trọn 11 file driver UART sang Cdd/Uart và cập nhật đường dẫn build, test và clangd. Test host đạt 649 kiểm tra ở chế độ 8 bit và 666 ở chế độ 9 bit. Firmware build đạt trên cả hai cấu hình chip. Chưa kiểm chứng truyền/nhận trên bo.

## Gửi dòng khởi động qua CH340

Đã cấu hình chân PA9 phát bằng USART1, 115200 8N1, gửi mini_mcu ready một lần trước vòng nháy đèn. Build đạt trên cả hai chip; test host đạt 649/649 và 666/666. Chờ xác nhận dòng chữ trên cổng COM sau khi nạp và reset.

### Tạm dừng khi kiểm tra UART với Hercules

Người học đã nạp firmware phát dòng khởi động lên bo. CH340 xuất hiện ở COM4. Docklight và PowerShell báo lỗi khi mở cổng; người học cho biết Hercules kết nối được. Nhấn reset bo thấy đèn CH340 chớp nhưng Hercules chưa hiện chữ. Khi thử riêng CH340 qua USB với TXD nối RXD, bấm Send trong Hercules chưa thấy đèn chớp hay nhận lại dữ liệu. Chưa xác định được nguyên nhân. Khi tiếp tục, kiểm tra nút Open/Close trong tab Serial của Hercules hoặc xem ảnh toàn bộ tab trước khi thử lại.

### Docklight đã nhận được dòng khởi động ngày 21/11/2026

Sau khi cài lại driver CH340, người học xác nhận Docklight hiển thị đúng “mini_mcu ready”, khớp chuỗi khởi động của firmware. Đã kiểm chứng bo phát dữ liệu qua PA9 và máy tính nhận được qua CH340 ở 115200 baud, 8 bit dữ liệu, không parity, 1 bit dừng. Chiều nhận trên bo vẫn cần kiểm thử riêng.

### Kiểm tra nguồn xung thực tế qua Docklight

Firmware đọc hai bit trạng thái nguồn xung trong thanh ghi cấu hình xung và gửi kết quả sau dòng khởi động. Đã build cho STM32F103C6T6 và STM32F103C8T6, kiểm thử thư viện UART trên máy tính đạt 1.315/1.315. Bước tiếp theo là nạp đúng firmware cho bo, reset và ghi nhận dòng SYSCLK. Giá trị mong đợi khi chạy bằng thạch anh ngoài là HSE (SWS=01).
