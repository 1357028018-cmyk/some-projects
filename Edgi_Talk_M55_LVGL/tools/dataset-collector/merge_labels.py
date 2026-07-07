import os, shutil

SRC = r"C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\dataset-collector\lc1"
DST = r"C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\dataset-collector\lc1_merged"

MERGE_MAP = {
    "left_head": "supine",
    "right_head": "supine",
}

os.makedirs(DST, exist_ok=True)

count = {"total": 0, "changed": 0}
for d in sorted(os.listdir(SRC)):
    src_dir = os.path.join(SRC, d)
    if not os.path.isdir(src_dir):
        continue
    label_path = os.path.join(src_dir, "sensor_data.label")
    if not os.path.isfile(label_path):
        continue

    with open(label_path, "r") as f:
        lines = f.readlines()
    header = lines[0]
    data = lines[1].strip()
    parts = data.split(",")
    label = parts[2]

    if label in MERGE_MAP:
        new_label = MERGE_MAP[label]
        parts[2] = new_label
        new_data = ",".join(parts)
        changed = True
    else:
        new_data = data
        changed = False

    dst_dir = os.path.join(DST, d)
    os.makedirs(dst_dir, exist_ok=True)

    with open(os.path.join(dst_dir, "sensor_data.label"), "w") as f:
        f.write(header)
        f.write(new_data + "\n")

    shutil.copy2(os.path.join(src_dir, "sensor_data.data"),
                 os.path.join(dst_dir, "sensor_data.data"))

    count["total"] += 1
    if changed:
        count["changed"] += 1

print(f"Done: {count['total']} samples, {count['changed']} relabeled (left_head+right_head → supine)")
print(f"Output: {DST}")
