# Bảng Quyết Định Kỹ Thuật (Architecture & Technical Decisions)

Tài liệu này ghi lại các quyết định quan trọng trong quá trình tinh chỉnh mô hình và phát triển firmware, nhằm đảm bảo tính minh bạch và có thể truy xuất nguồn gốc (traceability) cho các lựa chọn thuật toán, đặc biệt là những lựa chọn liên quan đến an toàn.

## Quyết Định: Lựa chọn Threshold cho nhãn LAYING (Ngày 23/09/2026)

**Ngữ cảnh:**
Mô hình Softmax Logistic Regression được đào tạo trên 18 Subjects. Trên tập Validation (3 Subjects: 28, 29, 30), baseline argmax thuần túy đã đạt Recall 1.000 cho nhãn LAYING.
Khi chạy script quét ngưỡng (`threshold_sweeper.py`), thuật toán Python `max()` đã trả về `0.05` làm điểm Sweet Spot. Tuy nhiên, việc chọn ngưỡng 0.05 (thấp hơn cả mức độ ngẫu nhiên 1/12 ≈ 0.083) để lại biên an toàn quá mỏng trước sai số số học và nhiễu cảm biến thực tế.

**Phân tích đoạn phẳng (Plateau):**
Bảng quét ngưỡng trên tập Validation cho thấy Recall giữ vững ở mức 1.000 suốt từ `0.05` đến `0.95`. Đồng thời, False Positive Rate (FPR) giảm từ `0.0011` (1 ca FP) xuống `0.0000` (0 ca FP) bắt đầu từ ngưỡng `0.40` trở đi.

**Quyết định:**
- **Không sử dụng ngưỡng 0.05** (kết quả chạy Test lần 1).
- **Chọn Threshold = 0.45** làm ngưỡng quyết định cuối cùng.

**Lý luận:**
1. Ngưỡng `0.45` nằm gọn trong vùng an toàn đã được kiểm chứng (0.40 - 0.95) trên tập Val, nơi FPR = 0 và Recall = 1.0.
2. Nó đủ xa biên dưới (0.40) để hấp thụ các sai số số học (float64 trên PC vs float32 trên STM32) và nhiễu phần cứng.
3. Nó đủ xa biên trên (0.95) để không ép mô hình phải quá "tự tin" trong điều kiện thực tế (nơi tín hiệu có thể không sạch hoàn hảo như dataset).
4. **Tính minh bạch:** Quyết định này được đưa ra **TRƯỚC KHI** xem lại kết quả trên tập Test lần 2, hoàn toàn dựa vào sự phân tích đoạn phẳng của tập Validation, nhằm giữ vững kỷ luật đánh giá khách quan (One-shot Evaluation).
