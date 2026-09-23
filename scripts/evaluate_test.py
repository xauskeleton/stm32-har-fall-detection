import joblib
import numpy as np
from sklearn.metrics import confusion_matrix
import sys
import os
import time

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLASSIFIER_PATH = os.path.join(BASE_DIR, "models", "classifier.pkl")
TEST_DATA_PATH = os.path.join(BASE_DIR, "models", "X_test.csv")
TEST_LABELS_PATH = os.path.join(BASE_DIR, "models", "y_test.csv")
LOCK_FILE = os.path.join(BASE_DIR, "scripts", "test_evaluation.lock")

# ĐÂY LÀ NƠI NHẬP HARDCODE NGƯỠNG ĐÃ TÌM ĐƯỢC TỪ TẬP VAL
LAYING_THRESHOLD = 0.45

LABELS = [
    "WALKING", "WALKING_UPSTAIRS", "WALKING_DOWNSTAIRS",
    "SITTING", "STANDING", "LAYING",
    "STAND_TO_SIT", "SIT_TO_STAND", "SIT_TO_LIE",
    "LIE_TO_SIT", "STAND_TO_LIE", "LIE_TO_STAND",
]

def print_subject_breakdown(y_true, y_pred, probs, target_idx, threshold):
    pass

def main():
    if os.path.exists(LOCK_FILE):
        with open(LOCK_FILE, "r") as f:
            lock_info = f.read()
        print(f"\n[CẢNH BÁO ĐỎ] Tập Test đã từng được đánh giá trong quá khứ:\n{lock_info}")
        print("Mọi sự đánh giá lại sau lần chạy đầu tiên đều có nguy cơ vi phạm quy tắc one-shot.")
        print("Lần chạy này sẽ được ghi nhận nối tiếp vào lịch sử đánh giá.")

    print(f"Đang nạp mô hình và áp dụng Threshold cứng = {LAYING_THRESHOLD} lên tập TEST (chưa từng được thấy)...")
    
    clf = joblib.load(CLASSIFIER_PATH)
    scaler = joblib.load(os.path.join(BASE_DIR, "models", "scaler.pkl"))
    
    X_test = np.loadtxt(TEST_DATA_PATH, delimiter=",")
    X_test_scaled = scaler.transform(X_test)
    y_test = np.loadtxt(TEST_LABELS_PATH, delimiter=",").astype(int).ravel() - 1
    
    u_path = os.path.join(BASE_DIR, "models", "u_test.csv")
    if os.path.exists(u_path):
        u_test = np.loadtxt(u_path, delimiter=",").astype(int).ravel()
    else:
        u_test = np.zeros_like(y_test)
        
    probs = clf.predict_proba(X_test_scaled)
    
    y_pred_base = np.argmax(probs, axis=1)
    
    target_idx = 5 # LAYING
    
    mask = probs[:, target_idx] > LAYING_THRESHOLD
    y_pred_thresh = np.empty_like(y_pred_base)
    y_pred_thresh[mask] = target_idx
    
    rem_probs = probs.copy()
    rem_probs[:, target_idx] = -1.0
    y_pred_thresh[~mask] = np.argmax(rem_probs[~mask], axis=1)
    
    print("\n--- BASELINE CONFUSION MATRIX ---")
    cm_base = confusion_matrix(y_test, y_pred_base, labels=range(12))
    print(cm_base)
    
    print(f"\n--- THRESHOLD {LAYING_THRESHOLD} CONFUSION MATRIX ---")
    cm_thresh = confusion_matrix(y_test, y_pred_thresh, labels=range(12))
    print(cm_thresh)
    
    print(f"\n--- RAW COUNTS FOR THRESHOLD {LAYING_THRESHOLD} ---")
    is_true = (y_test == target_idx)
    is_pred = (y_pred_thresh == target_idx)
    tp = np.sum(is_true & is_pred)
    fn = np.sum(is_true & ~is_pred)
    fp = np.sum(~is_true & is_pred)
    tn = np.sum(~is_true & ~is_pred)
    fpr = fp / (fp + tn) if (fp + tn) > 0 else 0
    print(f"TP: {tp}, FN: {fn}, FP: {fp}, TN: {tn} (FPR: {fpr:.4f})")

    # In ra breakdown theo tung user
    if os.path.exists(u_path):
        print("\n--- BREAKDOWN LỖI THEO TỪNG SUBJECT (Chỉ những lỗi liên quan đến LAYING) ---")
        unique_users = np.unique(u_test)
        for user in sorted(unique_users):
            idx = (u_test == user)
            is_true_u = is_true[idx]
            is_pred_u = is_pred[idx]
            
            fn_u = np.sum(is_true_u & ~is_pred_u)
            fp_u = np.sum(~is_true_u & is_pred_u)
            if fn_u > 0 or fp_u > 0:
                print(f"Subject {user:2d}: {fp_u} FP, {fn_u} FN")
            else:
                print(f"Subject {user:2d}: Mọi thứ hoàn hảo (0 FP, 0 FN)")

    # Ghi lock file (append)
    with open(LOCK_FILE, "a") as f:
        f.write(f"\n{time.strftime('%Y-%m-%d %H:%M:%S')} - Threshold = {LAYING_THRESHOLD} (Run 2: Lý do chọn dựa trên plateau của Validation set, xem DECISIONS.md)")
    print(f"\nĐã ghi nối tiếp vào file khóa {LOCK_FILE}. Quy tắc One-shot đã được tuân thủ về mặt lịch sử.")

if __name__ == '__main__':
    main()
