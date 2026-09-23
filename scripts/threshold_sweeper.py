#!/usr/bin/env python3
"""
threshold_sweeper.py
=====================
Quét ngưỡng xác suất (threshold) cho lớp LAYING (và tuỳ chọn STAND_TO_LIE)
trong mô hình Logistic Regression, nhằm tìm điểm cân bằng Recall vs.
False Positive Rate (FPR) trước khi quyết định có cần retrain với
Weighted Cross-Entropy hay không.

Ý tưởng: thay vì argmax thuần tuý, ta áp dụng luật:
    if prob[LAYING] > threshold:
        predicted = LAYING
    else:
        predicted = argmax(các lớp còn lại)

Điều này mô phỏng chính xác thứ AI_Predict() trong C sẽ làm nếu bạn
hạ ngưỡng quyết định, KHÔNG cần train lại model.
"""

import joblib
import numpy as np
import matplotlib.pyplot as plt
import os
import csv

# ============================================================
# 1. CẤU HÌNH — SỬA CÁC ĐƯỜNG DẪN NÀY CHO KHỚP VỚI DỰ ÁN CỦA BẠN
# ============================================================

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLASSIFIER_PATH = os.path.join(BASE_DIR, "models", "classifier.pkl")
SCALER_PATH = os.path.join(BASE_DIR, "models", "scaler.pkl")
# TODO: Nguoi dung can luu tap validation vao 2 file nay roi moi chay duoc
VAL_DATA_PATH = os.path.join(BASE_DIR, "models", "X_val.csv")
VAL_LABELS_PATH = os.path.join(BASE_DIR, "models", "y_val.csv")

# Thứ tự nhãn HAPT chuẩn (12 lớp)
LABELS = [
    "WALKING", "WALKING_UPSTAIRS", "WALKING_DOWNSTAIRS",
    "SITTING", "STANDING", "LAYING",
    "STAND_TO_SIT", "SIT_TO_STAND", "SIT_TO_LIE",
    "LIE_TO_SIT", "STAND_TO_LIE", "LIE_TO_STAND",
]

TARGET_CLASS = "LAYING"
THRESHOLDS = np.arange(0.05, 1.00, 0.05)
MAX_ACCEPTABLE_FPR = 0.15

# ============================================================
# 2. LOAD MODEL, SCALER, VALIDATION SET
# ============================================================

def load_artifacts():
    clf = joblib.load(CLASSIFIER_PATH)
    scaler = joblib.load(SCALER_PATH)

    if not os.path.exists(VAL_DATA_PATH) or not os.path.exists(VAL_LABELS_PATH):
        print(f"LỖI: Khong tim thay {VAL_DATA_PATH} hoac {VAL_LABELS_PATH}.")
        print("Vui long trich xuat tap validation va luu thanh file CSV truoc khi chay script nay.")
        exit(1)

    X_val = np.loadtxt(VAL_DATA_PATH, delimiter=",")
    y_val = np.loadtxt(VAL_LABELS_PATH, delimiter=",").astype(int).ravel() - 1

    return clf, scaler, X_val, y_val


# ============================================================
# 3. HÀM ÁP DỤNG LUẬT THRESHOLD (mô phỏng đúng logic sẽ viết trong C)
# ============================================================

def predict_with_threshold(probs: np.ndarray, target_idx: int, threshold: float) -> np.ndarray:
    n_samples, n_classes = probs.shape
    preds = np.empty(n_samples, dtype=int)

    target_probs = probs[:, target_idx]
    mask_target = target_probs > threshold

    preds[mask_target] = target_idx

    remaining_probs = probs.copy()
    remaining_probs[:, target_idx] = -1.0
    preds[~mask_target] = np.argmax(remaining_probs[~mask_target], axis=1)

    return preds

# ============================================================
# 4. TÍNH RECALL / FPR / F1 CHO TỪNG THRESHOLD
# ============================================================

def evaluate_threshold(y_true: np.ndarray, y_pred: np.ndarray, target_idx: int) -> dict:
    is_target_true = (y_true == target_idx)
    is_target_pred = (y_pred == target_idx)

    tp = np.sum(is_target_true & is_target_pred)
    fn = np.sum(is_target_true & ~is_target_pred)
    fp = np.sum(~is_target_true & is_target_pred)
    tn = np.sum(~is_target_true & ~is_target_pred)

    recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0.0
    precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    f1 = (2 * precision * recall / (precision + recall)) if (precision + recall) > 0 else 0.0

    return {"recall": recall, "fpr": fpr, "precision": precision, "f1": f1,
            "tp": tp, "fn": fn, "fp": fp, "tn": tn}

# ============================================================
# 5. MAIN
# ============================================================

def main():
    clf, scaler, X_val, y_val = load_artifacts()
    target_idx = LABELS.index(TARGET_CLASS)

    X_val_scaled = scaler.transform(X_val)
    probs = clf.predict_proba(X_val_scaled)

    results = []
    for t in THRESHOLDS:
        y_pred = predict_with_threshold(probs, target_idx, t)
        metrics = evaluate_threshold(y_val, y_pred, target_idx)
        metrics["threshold"] = round(float(t), 2)
        results.append(metrics)

    # Save results to CSV manually
    keys = ["threshold", "recall", "fpr", "precision", "f1", "tp", "fn", "fp", "tn"]
    with open("threshold_sweep_results.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        writer.writerows(results)

    print(f"Đã lưu bảng kết quả chi tiết: threshold_sweep_results.csv\n")
    print(f"{'threshold':>10} {'recall':>8} {'fpr':>8} {'f1':>8}")
    for r in results:
        print(f"{r['threshold']:10.2f} {r['recall']:8.3f} {r['fpr']:8.3f} {r['f1']:8.3f}")

    y_pred_baseline = np.argmax(probs, axis=1)
    baseline_metrics = evaluate_threshold(y_val, y_pred_baseline, target_idx)
    print(f"\n--- Baseline (argmax thuần, không hạ ngưỡng) ---")
    print(f"Recall({TARGET_CLASS}) = {baseline_metrics['recall']:.3f} | "
          f"FPR = {baseline_metrics['fpr']:.3f} | F1 = {baseline_metrics['f1']:.3f}")

    valid = [r for r in results if r["fpr"] <= MAX_ACCEPTABLE_FPR]
    if valid:
        sweet_spot = max(valid, key=lambda x: x["recall"])
        print(f"\n--- Sweet Spot (Recall cao nhất với FPR <= {MAX_ACCEPTABLE_FPR}) ---")
        print(f"Threshold = {sweet_spot['threshold']} | "
              f"Recall = {sweet_spot['recall']:.3f} | FPR = {sweet_spot['fpr']:.3f} | "
              f"F1 = {sweet_spot['f1']:.3f}")
    else:
        print(f"\n⚠️ Không có threshold nào đạt FPR <= {MAX_ACCEPTABLE_FPR}.")

    thresholds_arr = [r["threshold"] for r in results]
    recall_arr = [r["recall"] for r in results]
    fpr_arr = [r["fpr"] for r in results]

    fig, ax1 = plt.subplots(figsize=(8, 5))
    ax1.plot(thresholds_arr, recall_arr, marker="o", color="tab:blue", label="Recall")
    ax1.set_xlabel("Threshold")
    ax1.set_ylabel("Recall", color="tab:blue")
    ax1.tick_params(axis="y", labelcolor="tab:blue")
    ax1.axhline(baseline_metrics["recall"], color="tab:blue", linestyle="--", alpha=0.4)

    ax2 = ax1.twinx()
    ax2.plot(thresholds_arr, fpr_arr, marker="s", color="tab:red", label="FPR")
    ax2.set_ylabel("False Positive Rate", color="tab:red")
    ax2.tick_params(axis="y", labelcolor="tab:red")
    ax2.axhline(MAX_ACCEPTABLE_FPR, color="tab:red", linestyle="--", alpha=0.4)

    plt.title(f"Trade-off Recall vs FPR — lớp {TARGET_CLASS}")
    fig.tight_layout()
    plt.savefig("trade_off_curve.png", dpi=150)
    print(f"\nĐã lưu biểu đồ: trade_off_curve.png")

if __name__ == "__main__":
    main()
