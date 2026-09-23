import joblib
import numpy as np
from sklearn.metrics import confusion_matrix
import sys
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLASSIFIER_PATH = os.path.join(BASE_DIR, "models", "classifier.pkl")
VAL_DATA_PATH = os.path.join(BASE_DIR, "models", "X_val.csv")
VAL_LABELS_PATH = os.path.join(BASE_DIR, "models", "y_val.csv")

LABELS = [
    "WALKING", "WALKING_UPSTAIRS", "WALKING_DOWNSTAIRS",
    "SITTING", "STANDING", "LAYING",
    "STAND_TO_SIT", "SIT_TO_STAND", "SIT_TO_LIE",
    "LIE_TO_SIT", "STAND_TO_LIE", "LIE_TO_STAND",
]

def main():
    clf = joblib.load(CLASSIFIER_PATH)
    scaler = joblib.load(os.path.join(BASE_DIR, "models", "scaler.pkl"))
    
    X_val = np.loadtxt(VAL_DATA_PATH, delimiter=",")
    X_val_scaled = scaler.transform(X_val)
    y_val = np.loadtxt(VAL_LABELS_PATH, delimiter=",").astype(int).ravel() - 1
    
    probs = clf.predict_proba(X_val_scaled)
    
    y_pred_base = np.argmax(probs, axis=1)
    
    target_idx = 5 # LAYING
    threshold = 0.05
    
    mask = probs[:, target_idx] > threshold
    y_pred_thresh = np.empty_like(y_pred_base)
    y_pred_thresh[mask] = target_idx
    
    rem_probs = probs.copy()
    rem_probs[:, target_idx] = -1.0
    y_pred_thresh[~mask] = np.argmax(rem_probs[~mask], axis=1)
    
    print("--- BASELINE CONFUSION MATRIX ---")
    cm_base = confusion_matrix(y_val, y_pred_base, labels=range(12))
    print(cm_base)
    
    print("\n--- THRESHOLD 0.05 CONFUSION MATRIX ---")
    cm_thresh = confusion_matrix(y_val, y_pred_thresh, labels=range(12))
    print(cm_thresh)
    
    print("\n--- RAW COUNTS FOR THRESHOLD 0.05 ---")
    is_true = (y_val == target_idx)
    is_pred = (y_pred_thresh == target_idx)
    tp = np.sum(is_true & is_pred)
    fn = np.sum(is_true & ~is_pred)
    fp = np.sum(~is_true & is_pred)
    tn = np.sum(~is_true & ~is_pred)
    print(f"TP: {tp}, FN: {fn}, FP: {fp}, TN: {tn}")

if __name__ == '__main__':
    main()
