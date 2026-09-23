import numpy as np
from har import dataset
from har import config as cfg

def main():
    raw_dir = dataset.find_rawdata('data_raw/RawData')
    raw, labels = dataset.load_raw(raw_dir)
    
    # Same config as the final training: groups=full, use_fft=True
    groups = cfg.GROUPS_FULL
    use_fft = True
    
    X_tr, y_tr, u_tr, X_val, y_val, u_val, X_te, y_te, u_te = dataset.build_features(
        raw, labels, mode='causal', use_butter=False, groups=groups, use_fft=use_fft)
    
    # Save validation set
    np.savetxt('models/X_val.csv', X_val, delimiter=',')
    np.savetxt('models/y_val.csv', y_val, delimiter=',', fmt='%d')
    np.savetxt('models/u_val.csv', u_val, delimiter=',', fmt='%d')

    # Save test set
    np.savetxt('models/X_test.csv', X_te, delimiter=',')
    np.savetxt('models/y_test.csv', y_te, delimiter=',', fmt='%d')
    np.savetxt('models/u_test.csv', u_te, delimiter=',', fmt='%d')
    
    print(f"Exported {X_val.shape[0]} validation samples to models/X_val.csv and models/y_val.csv")
    print(f"Exported {X_te.shape[0]} test samples to models/X_test.csv and models/y_test.csv")

if __name__ == '__main__':
    main()
