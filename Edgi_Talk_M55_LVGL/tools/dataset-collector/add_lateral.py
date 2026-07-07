import os, shutil

SRC = r"C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\dataset-collector\output_dataset2"
DST = r"C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\dataset-collector\lc1_merged"

keep_labels = {"left_lateral", "right_lateral"}

existing = [int(d.split("_")[1]) for d in os.listdir(DST) if d.startswith("HLPPDat_")]
next_seq = max(existing) + 1 if existing else 1

copied = 0
for d in sorted(os.listdir(SRC)):
    src_dir = os.path.join(SRC, d)
    if not os.path.isdir(src_dir):
        continue
    label_path = os.path.join(src_dir, "sensor_data.label")
    if not os.path.isfile(label_path):
        continue
    with open(label_path) as f:
        label = f.readlines()[1].strip().split(",")[2]
    if label not in keep_labels:
        continue
    dst_dir = os.path.join(DST, f"HLPPDat_{next_seq:04d}")
    os.makedirs(dst_dir, exist_ok=True)
    shutil.copy2(os.path.join(src_dir, "sensor_data.data"), os.path.join(dst_dir, "sensor_data.data"))
    shutil.copy2(label_path, os.path.join(dst_dir, "sensor_data.label"))
    next_seq += 1
    copied += 1

print(f"Copied {copied} lateral samples into {DST}")
