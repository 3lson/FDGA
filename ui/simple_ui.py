import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import numpy as np
import subprocess

def parse_input(input_str):
    iterations = []
    current_clusters = []

    for line in input_str.strip().splitlines():
        line = line.strip()
        if line.startswith("ITERATION"):
            current_clusters = []
        elif line.startswith("END"):
            iterations.append(current_clusters)
        elif line:
            parts = line.split(':')
            centroid_part = parts[0].strip()
            points_part = parts[1].strip() if len(parts) > 1 else ''

            centroid_name, centroid_coords = centroid_part.split(' ', 1)
            centroid_x, centroid_y = map(float, centroid_coords.strip('()').split(','))

            points = []
            if points_part:
                for point_str in points_part.replace('(', '').split(')'):
                    if point_str.strip():
                        x, y = map(float, point_str.strip().split(','))
                        points.append((x, y))

            current_clusters.append({
                'name': centroid_name,
                'centroid': (centroid_x, centroid_y),
                'points': points
            })
    return iterations


def generate_visualization(clusters, width=640, height=480):
    img = np.ones((height, width, 3), dtype=np.uint8) * 255
    plot_width, plot_height = 400, 300
    plot_x = (width - plot_width) // 2
    plot_y = (height - plot_height) // 2

    x_min, x_max = 0, 20
    y_min, y_max = 0, 20

    def scale(x, y):
        px = plot_x + int((x - x_min) / (x_max - x_min) * plot_width)
        py = plot_y + plot_height - int((y - y_min) / (y_max - y_min) * plot_height)
        return px, py

    def inv_scale(px, py):
        x = x_min + (px - plot_x) * (x_max - x_min) / plot_width
        y = y_min + (plot_y + plot_height - py) * (y_max - y_min) / plot_height
        return x, y

    light_colors = [
        [255, 0, 0],
        [0, 255, 0],
        [0, 0, 255]
    ]

    for y in range(plot_y, plot_y + plot_height):
        for x in range(plot_x, plot_x + plot_width):
            data_x, data_y = inv_scale(x, y)
            min_dist = float('inf')
            closest_cluster = 0
            for i, cluster in enumerate(clusters):
                cx, cy = cluster['centroid']
                dist = (data_x - cx)**2 + (data_y - cy)**2
                if dist < min_dist:
                    min_dist = dist
                    closest_cluster = i
            img[y, x] = light_colors[closest_cluster]


    for cluster in clusters:
        for x, y in cluster['points']:
            px, py = scale(x, y)
            img[py-2:py+3, px-2:px+3] = 0
        cx, cy = cluster['centroid']
        px, py = scale(cx, cy)
        img[py-3:py+4, px-3:px+4] = 0

    return img


def run_gui():
    root = tk.Tk()
    root.title("K-Means Visualization")
    root.geometry("640x480")
    root.resizable(False, False)

    canvas = tk.Canvas(root, width=640, height=480)
    canvas.pack()

    # define axis
    x_min, x_max = 0, 20
    y_min, y_max = 0, 20
    plot_x = 120
    plot_y = 90
    plot_width = 400
    plot_height = 300

    # initialize white backgraound
    img = np.ones((480, 640, 3), dtype=np.uint8) * 255
    tk_img = ImageTk.PhotoImage(Image.fromarray(img))

    # background and axis
    canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
    canvas.image = tk_img
    # generated poitns list
    points = []  
    points_generated = False


    def draw_axes():
        # X-axis
        canvas.create_line(120, 390, 520, 390, fill='black', tags='plot')
        # Y-axis
        canvas.create_line(120, 390, 120, 90, fill='black', tags='plot')
        #arrows
        canvas.create_line(520, 390, 515, 385, fill='black', tags='plot')
        canvas.create_line(520, 390, 515, 395, fill='black', tags='plot')
        canvas.create_line(120, 90, 115, 95, fill='black', tags='plot')
        canvas.create_line(120, 90, 125, 95, fill='black', tags='plot')

    # draws the clustersand points on canvas
    def draw_image():
        nonlocal img, tk_img, points_generated
        if not points_generated:
            messagebox.showwarning("Warning", "Please generate points before starting.")
            return

        # Compile and run the C code
        subprocess.run(['gcc', 'k_means2.c', '-o', 'k_means2'], capture_output=True, text=True)
        subprocess.run(['./k_means2'], capture_output=True, text=True)

        # Read the full output file with all iterations
        with open('output.txt', 'r') as f:
            input_data = f.read()
        all_iterations = parse_input(input_data)

        animate(0, all_iterations)
    # Animate iterations one by one
    def animate(i, all_iterations):
        nonlocal img, tk_img
        if i >= len(all_iterations):
            return
        pixel_array = generate_visualization(all_iterations[i])
        img = pixel_array.copy()
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        canvas.delete('plot')
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()
        root.after(500, lambda: animate(i + 1, all_iterations))  # Wait 1 second between frames

    def reset_canvas():
        nonlocal img, tk_img, points_generated
        points_generated = False
        points.clear()
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        canvas.delete('plot')
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()

    # coordinate labels
    coord_label = tk.Label(root, text="Coordinates: ", font=("Arial", 10))
    coord_label.place(x=260, y=450)


    def show_coordinates(event):
        if 120 <= event.x <= 520 and 90 <= event.y <= 390:
            x = x_min + (event.x - plot_x) * (x_max - x_min) / plot_width
            y = y_min + (plot_y + plot_height - event.y) * (y_max - y_min) / plot_height
            coord_label.config(text=f"Coordinates: ({x:.2f}, {y:.2f})")
        else:
            coord_label.config(text="Coordinates: ")

    canvas.bind('<Motion>', show_coordinates)

    def gen_points():
        nonlocal img, tk_img, points_generated
        points_generated = True
        points.clear()
        # reset image to white background
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        for i in range(100):
            x = np.random.uniform(x_min, x_max)
            y = np.random.uniform(y_min, y_max)
            px = int(plot_x + (x - x_min) / (x_max - x_min) * plot_width)
            py = int(plot_y + plot_height - (y - y_min) / (y_max - y_min) * plot_height)
            points.append((x, y))
            # 5x5 black squares as points
            img[py - 2:py + 3, px - 2:px + 3] = [0, 0, 0]
        f = open('points.txt', 'w')
        for point in points:
            f.write(f"{point[0]:.2f} {point[1]:.2f}\n")
        f.close()
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()

    # buttons
    button_start = tk.Button(root, text="Start", command=draw_image)
    canvas.create_window(600, 440, anchor='se', window=button_start)

    button_reset = tk.Button(root, text="Reset", command=reset_canvas)
    canvas.create_window(40, 440, anchor='sw', window=button_reset)

    button_gen_points = tk.Button(root, text="Generate Points", command=gen_points)
    canvas.create_window(320, 440, anchor='s', window=button_gen_points)

    draw_axes()
    root.mainloop()

run_gui()
