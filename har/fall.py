"""
Phat hien te nga TU CHUOI NHAN dau ra cua bo phan loai 12 lop.

Y tuong: HAPT khong co nhan FALL, nhung 12 nhan cua no da du de phan biet
"nam xuong chu dong" voi "bong dung dang o tu the dung/di roi thanh LAYING":

    Nam xuong binh thuong :  STANDING -> STAND_TO_LIE -> LAYING
                             SITTING  -> SIT_TO_LIE   -> LAYING
    Te nga                :  WALKING/STANDING -> (khong co nhan transition) -> LAYING
                             kem theo mot cu soc gia toc (free-fall roi va dap)

State machine (file nay va firmware/Core/Src/fall_detect.c la ban sao cua nhau):

    IDLE ──soc gia toc / nhay thang sang LAYING──► SHOCK
    SHOCK ──LAYING lien tiep du lau──► FALLEN  (BAO DONG)
    SHOCK ──thay nhan SIT_TO_LIE / STAND_TO_LIE──► IDLE   (nam chu dong, khong bao)
    SHOCK ──quay lai dung/di──► IDLE                      (vap nhe, tu dung day)
    FALLEN ──roi khoi LAYING du lau──► IDLE               (da dung day / duoc do day)
"""
from dataclasses import dataclass

import numpy as np

from . import config as cfg
from . import dsp

STATE_IDLE = 0
STATE_SHOCK = 1
STATE_FALLEN = 2
STATE_NAMES = {STATE_IDLE: 'IDLE', STATE_SHOCK: 'SHOCK', STATE_FALLEN: 'FALLEN'}

# 1 cua so truot = STEP/FS = 1.28 s
WINDOW_PERIOD = cfg.STEP / cfg.FS


@dataclass(frozen=True)
class FallParams:
    """Tat ca nguong. scripts/export_model.py ghi nguyen bo nay sang C dang #define."""
    freefall_g: float = 0.60     # |a| tut duoi muc nay -> dang roi tu do
    impact_g: float = 2.00       # |a| vot qua muc nay  -> cu dap xuong san

    # 'both' = phai co CA rot tu do VA cu dap. Bat buoc phai tinh tren tin hieu
    # da loc median 3 (dung shock_windows()), neu khong thi nhieu cua HAPT lam
    # 12.9% cua so tu the tinh thoa dieu kien -> 77 bao nham. Xem DECISIONS.md muc 7.
    shock_rule: str = 'both'     # 'both' | 'any' | 'labels' (chi dung chuoi nhan)
    confirm_windows: int = 4     # so cua so toi da cho SHOCK cho xac nhan (~5.1 s)
    laying_confirm: int = 2      # so cua so LAYING lien tiep de chot la te nga (~2.6 s)
    voluntary_guard: int = 4     # sau 1 nhan *_TO_LIE thi LAYING duoc coi la hop le (~5.1 s)
    recover_windows: int = 2     # roi khoi LAYING bao nhieu cua so thi go bao dong


def window_shock_stats(win_acc):
    """win_acc: (n_win, WINDOW, 3) gia toc TONG (g) -> (a_min, a_max) cua |a| tren moi cua so.

    Dung gia toc tong (co ca trong luc) chu khong phai body_acc: luc dung yen |a| ~ 1g,
    roi tu do |a| ~ 0g, dap san |a| vot len vai g. Day la dai luong vat ly quen thuoc
    trong cac bai bao fall detection.
    """
    mag = np.linalg.norm(win_acc, axis=2)
    return mag.min(axis=1), mag.max(axis=1)


def shock_windows(acc, use_median=True):
    """Raw acc (N,3) don vi g -> (a_min, a_max) cho tung cua so.

    use_median=True (mac dinh) loc median 3 truoc: BAT BUOC tren du lieu that,
    xem docs/DECISIONS.md muc 7.
    """
    a = dsp.median3(acc) if use_median else np.asarray(acc, dtype=np.float64)
    w = dsp.make_windows(a)
    if w is None:
        return None, None
    return window_shock_stats(w)


class FallDetector:
    """Nap tung cua so theo thu tu thoi gian. update() tra ve su kien hoac None."""

    def __init__(self, params=None):
        self.p = params or FallParams()
        self.reset()

    def reset(self):
        self.state = STATE_IDLE
        self.prev_label = 0
        self._guard = 0          # so cua so con lai coi LAYING la "nam chu dong"
        self._timer = 0          # dem nguoc trong SHOCK
        self._laying = 0         # so cua so LAYING lien tiep
        self._recover = 0

    # -- noi bo ------------------------------------------------------------
    def _is_shock(self, a_min, a_max):
        p = self.p
        if p.shock_rule == 'labels':
            return False
        if a_min is None or a_max is None:
            return False
        free = a_min <= p.freefall_g
        hit = a_max >= p.impact_g
        return (free and hit) if p.shock_rule == 'both' else (free or hit)

    def _abrupt_to_laying(self, label):
        """Nhay thang tu tu the chua nam sang LAYING ma khong qua nhan transition."""
        return (label == cfg.LABEL_LAYING
                and self.prev_label in cfg.LABELS_ACTIVE
                and self._guard == 0)

    # -- API ---------------------------------------------------------------
    def update(self, label, a_min=None, a_max=None):
        """label: 1..12. a_min/a_max: |a| (g) nho nhat / lon nhat trong cua so.

        Tra ve 'FALL' | 'RECOVERED' | None.
        """
        p = self.p
        event = None

        if self._guard > 0:
            self._guard -= 1
        if label in cfg.LABELS_LIE_DOWN_TRANSITION:
            self._guard = p.voluntary_guard        # nam xuong chu dong -> mien tru

        shock = self._is_shock(a_min, a_max)

        if self.state == STATE_IDLE:
            if shock or self._abrupt_to_laying(label):
                self.state = STATE_SHOCK
                self._timer = p.confirm_windows
                self._laying = 1 if label == cfg.LABEL_LAYING else 0
                if self._laying >= p.laying_confirm:
                    self.state = STATE_FALLEN
                    event = 'FALL'

        elif self.state == STATE_SHOCK:
            self._timer -= 1
            if label in cfg.LABELS_LIE_DOWN_TRANSITION or self._guard > 0:
                self.reset_soft()                   # nam xuong chu dong -> huy nghi ngo
            elif label == cfg.LABEL_LAYING:
                self._laying += 1
                if self._laying >= p.laying_confirm:
                    self.state = STATE_FALLEN
                    self._recover = 0
                    event = 'FALL'
            elif label in cfg.LABELS_UPRIGHT:
                self.reset_soft()                   # da dung day duoc -> khong phai te nang
            elif self._timer <= 0:
                self.reset_soft()

        elif self.state == STATE_FALLEN:
            if label == cfg.LABEL_LAYING:
                self._recover = 0
            else:
                self._recover += 1
                if self._recover >= p.recover_windows:
                    self.reset_soft()
                    event = 'RECOVERED'

        self.prev_label = label
        return event

    def reset_soft(self):
        """Ve IDLE nhung giu prev_label va guard."""
        self.state = STATE_IDLE
        self._timer = 0
        self._laying = 0
        self._recover = 0


def run_sequence(labels, acc=None, params=None, win_acc=None, use_median=True):
    """Chay state machine tren ca chuoi cua so.

    acc     : tin hieu gia toc LIEN TUC (N,3) don vi g -> tu loc median + cat cua so.
              Day la duong dung; dung acc= chu khong phai win_acc=.
    win_acc : (n_win, WINDOW, 3) da cat cua so san, KHONG loc median them.
    acc=win_acc=None -> chay thuan chuoi nhan (khong co thong tin soc).

    Tra ve (states, events) voi events la list (index_cua_so, thoi_diem_s, ten_su_kien).
    """
    a_min = a_max = None
    if acc is not None:
        a_min, a_max = shock_windows(acc, use_median=use_median)
    elif win_acc is not None:
        a_min, a_max = window_shock_stats(win_acc)

    if a_min is None:
        # Khong co thong tin gia toc -> chi chay theo chuoi nhan
        a_min = a_max = [None] * len(labels)

    det = FallDetector(params)

    states, events = [], []
    for i, lab in enumerate(labels):
        ev = det.update(int(lab), a_min[i], a_max[i])
        states.append(det.state)
        if ev:
            events.append((i, i * WINDOW_PERIOD, ev))
    return states, events
