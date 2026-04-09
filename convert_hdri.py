import cv2
import numpy as np

def sample_equirect(img, direction):
    direction = direction / np.linalg.norm(direction)
    x, y, z = direction
    u = 0.5 + np.arctan2(z, x) / (2 * np.pi)
    v = 0.5 - np.arcsin(np.clip(y, -1, 1)) / np.pi
    h, w = img.shape[:2]
    px = np.clip(int(u * w), 0, w - 1)
    py = np.clip(int(v * h), 0, h - 1)
    return img[py, px]

def make_face(img, size, right, up, forward):
    face = np.zeros((size, size, 3), dtype=np.float32)
    for y in range(size):
        for x in range(size):
            u = (x + 0.5) / size * 2 - 1
            v = (y + 0.5) / size * 2 - 1
            direction = np.array(forward) + u * np.array(right) + v * np.array(up)
            face[y, x] = sample_equirect(img, direction)
    return face

def save_face(face, path):
    face_tonemapped = face / (face + 1.0)
    face_gamma = np.power(np.clip(face_tonemapped, 0, 1), 1/2.2)
    face_uint8 = (face_gamma * 255).astype(np.uint8)
    cv2.imwrite(path, cv2.cvtColor(face_uint8, cv2.COLOR_RGB2BGR))

# OpenCV can read HDR natively
img = cv2.imread("assets/skybox.hdr", cv2.IMREAD_ANYDEPTH | cv2.IMREAD_COLOR)
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB).astype(np.float32)

SIZE = 512

faces = {
    "skybox_right":  ([ 0, 0,-1], [0,-1, 0], [ 1, 0, 0]),
    "skybox_left":   ([ 0, 0, 1], [0,-1, 0], [-1, 0, 0]),
    "skybox_top":    ([ 1, 0, 0], [0, 0, 1], [ 0, 1, 0]),
    "skybox_bottom": ([ 1, 0, 0], [0, 0,-1], [ 0,-1, 0]),
    "skybox_front":  ([ 1, 0, 0], [0,-1, 0], [ 0, 0, 1]),
    "skybox_back":   ([-1, 0, 0], [0,-1, 0], [ 0, 0,-1]),
}

for name, (right, up, forward) in faces.items():
    print(f"Converting {name}...")
    face = make_face(img, SIZE, right, up, forward)
    save_face(face, f"assets/{name}.png")
    print(f"Saved assets/{name}.png")

print("Done!")