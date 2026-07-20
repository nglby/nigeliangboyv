import cv2
import numpy as np
import json

ALPHA_CENTER = 0.30
MIN_MATERIAL_HEIGHT = 30
MIN_MATERIAL_WIDTH = 15
MAX_MATERIAL_RATIO = 0.80


def detect_top_bottom(gray_frame):
    Heng, Shu = gray_frame.shape
    _, binary = cv2.threshold(
        gray_frame, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    dark_ratio = ((binary == 0).mean(axis=1))
    smooth = np.convolve(dark_ratio, np.ones(15)/15, mode='same')

    gap = int(smooth.argmin())
    top = 0
    bottom = 0
    for i in range(gap, -1, -1):
        if smooth[i] > 0.3:
            top = i
            break
    for i in range(gap, Heng):
        if smooth[i] > 0.4:
            bottom = i
            break

    if top == 0 or bottom == 0 or (bottom - top) < MIN_MATERIAL_HEIGHT or (bottom - top) > Heng * MAX_MATERIAL_RATIO:
        return 0, 0
    else:
        return top, bottom


def detect_left_right(gray, top, bottom):
    middle_row = (top + bottom)//2
    height = bottom - top
    half_height = height//4
    y1 = middle_row - half_height
    y2 = middle_row + half_height

    num = gray[y1:y2, :]  # y1到y2行，所有列
    edges = cv2.Canny(num, 15, 25)

    project = edges.mean(axis=0)
    left = 0
    right = 0
    Heng, Shu = gray.shape

    for i in range(Shu):
        if project[i] > 0:
            left = i
            break
    for i in range(200,left,-1):
        if project[i] > 0:
            right = i
            break

    if left == 0 or right == 0 or (right - left) < MIN_MATERIAL_WIDTH:
        return 0, 0
    else:
        return left, right

def draw_roi_overlay(gray, top, bottom, left, right):
    result = gray.copy()
    cv2.rectangle(result, (left, top), (right, bottom),
                  (0, 255, 0), 2)  # 2->线条粗细
    text_x = right + 10
    text_y = (top + bottom) // 2

    cv2.putText(result, f"{bottom - top} px", (text_x, text_y),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
    return result


def process_video(video_path, output_path):
    cap = cv2.VideoCapture(video_path)
    fps = cap.get(cv2.CAP_PROP_FPS)
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")  # 创建一个写手
    out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
    results = {}
    frame_idx = 0
    while True:
        ok, frame = cap.read()
        if not ok:
            break
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        top, bottom = detect_top_bottom(gray)
        #print(f"检测到: top={top}, bottom={bottom}")
        if top and bottom:
            print(f"材料高度: {bottom - top} px")
        left, right = detect_left_right(gray, top, bottom)
        #print(f"检测到: left={left}, right={right}")
        if left and right:
            print(f"材料长度: {right - left} px")
        results[frame_idx] = (top,bottom,left,right)
        frame_idx+=1
        result = draw_roi_overlay(frame, top, bottom, left, right)
        out.write(result)

    cap.release()
    out.release()
    return results

def compute_error(pred_frames, label_data):
    total_frames = label_data["total_frames"]
    label_dict = {}
    for item in label_data["frames"]:
        label_dict[item["frame"]] = (item["top"], item["bottom"],
                                      item["left"], item["right"])#从json取
    detected = 0             
    sum_top = 0
    sum_bottom = 0
    sum_left = 0
    sum_right = 0

    for f in range(total_frames):
        if f not in pred_frames:
            continue
        pt,pb,pl,pr = pred_frames[f]
        if pt == 0 and pb == 0:
            continue
        detected +=1
        lt,lb,ll,lr = label_dict[f]

        sum_top+=abs(pt-lt)
        sum_bottom += abs(pb - lb)
        sum_left   += abs(pl - ll)
        sum_right  += abs(pr - lr)

    if detected != 0:
        return (detected, total_frames,
        sum_top / detected,
        sum_bottom / detected,
        sum_left / detected,
        sum_right / detected)

def main():
    for i in range(1,5):
        video_path = f"data/video/{i}.mp4"
        output_path = f"{i}_result.mp4"
        pred = process_video(video_path, output_path)
        with open(f"data/label/{i}.json") as f:
            label = json.load(f)
        detected, total, mae_top, mae_bottom, mae_left, mae_right = compute_error(pred, label)
        rate = detected / total * 100

        print(f"{i}.mp4 | 检出: {detected}/{total} ({rate:.1f}%) | "
              f"top: {mae_top:.1f} bottom: {mae_bottom:.1f} "
              f"left: {mae_left:.1f} right: {mae_right:.1f} px")

if __name__ == "__main__":
    main()
