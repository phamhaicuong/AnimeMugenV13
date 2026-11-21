# Anime MUGEN V13 – PSP v0.1 (CHƯA BIÊN DỊCH / CHƯA KIỂM TRA TRÊN PPSSPP)

Trạng thái trung thực: đây là mã nguồn. Chưa có EBOOT.PBP vì môi trường tạo ra nó không có PSPSDK.
Cú pháp C đã được kiểm tra bằng gcc với header giả; chưa chạy thật trên PSP/PPSSPP.

## Biên dịch (chọn 1 cách, đều miễn phí)
**A. GitHub Actions (dễ nhất trên Windows):** tạo repo GitHub, đẩy thư mục này lên, vào tab Actions > "Build PSP EBOOT" > tải artifact `AnimeMugenV13.zip`.
**B. Docker Desktop:** `docker run --rm -v "%cd%":/src -w /src ghcr.io/pspdev/pspdev:latest make`

## Cài vào iPhone
Giải nén ZIP, chép thư mục `PSP/GAME/AnimeMugenV13` (có EBOOT.PBP, characters, stages) vào thư mục PPSSPP > PSP > GAME bằng ứng dụng Tệp.

## Thêm nhân vật / sàn đấu (không cần biên dịch lại)
- Nhân vật: tạo `characters/<tên>/char.txt` với các dòng `name=`, `color=RRGGBB`, `hp=`, `power=`, `speed=`. Game tự quét khi khởi động và sau mỗi trận (tối đa 512).
- Sàn đấu: `stages/<tên>/stage.txt` với `name=`, `sky=RRGGBB`, `ground=RRGGBB`.

## Điều khiển
D-pad/analog: di chuyển · X: nhảy · Vuông: đánh · Tam giác: chiêu đặc biệt (25 năng lượng) · Tròn (giữ): đỡ · R: Ultimate (đủ 100 năng lượng) · Start: bỏ qua màn VS.

## Chưa có trong v0.1 (nói rõ để không hiểu nhầm)
- Nhân vật là khối màu, chưa có sprite/animation. Nhân vật MUGEN (.def/.sff/.air/.cns) KHÔNG chạy trực tiếp trên PSP; cần bộ chuyển đổi SFF→sprite PSP (kế hoạch v0.2) và chỉ giữ được một phần đòn/hiệu ứng, vì CNS là ngôn ngữ script riêng.
- Chưa có âm thanh/nhạc (v0.3, cần thư viện audio PSP).
- Chưa có ảnh đại diện, chuyển cảnh nâng cao, 2 người chơi.
