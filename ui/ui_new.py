from multiprocessing import Process
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import numpy as np
import subprocess
import ui_virtual_mouse 
import os
import pyautogui

# weird code from neil




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

    x_min, x_max = 0, 10
    y_min, y_max = 0, 10

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

    # Use parameterized dimensions
    canvas = tk.Canvas(root, width=640, height=480)
    canvas.pack()

    # State variables
    num_centroids = 0 
    points = []  
    points_generated = False

    # Plot boundaries
    x_min, x_max = 0, 10
    y_min, y_max = 0, 10
    plot_x = 120
    plot_y = 90
    plot_width = 400
    plot_height = 300

    # Initialize background
    img = np.ones((480, 640, 3), dtype=np.uint8) * 255
    tk_img = ImageTk.PhotoImage(Image.fromarray(img))

    canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
    canvas.image = tk_img

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
        compilation = subprocess.run(['gcc', 'k_means3.c', '-o', 'k_means3'], capture_output=True, text=True)
        if compilation.returncode != 0:
            messagebox.showerror("Compilation Error", compilation.stderr)
            return
        subprocess.run(['./k_means3'], capture_output=True, text=True)

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
        nonlocal img, tk_img, points_generated, num_centroids
        num_centroids = 0
        points_generated = False
        points.clear()
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        open("clicked_points.txt", "w").close()
        canvas.delete('plot')
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()

    # coordinate labels
    coord_label = tk.Label(root, text="Coordinates: ", font=("Arial", 10))
    coord_label.place(x=260, y=450)
    def get_centroids(event):
        nonlocal img, tk_img, points_generated, num_centroids, points

        if 120 <= event.x <= 520 and 90 <= event.y <= 390:
            x = x_min + (event.x - plot_x) * (x_max - x_min) / plot_width
            y = y_min + (plot_y + plot_height - event.y) * (y_max - y_min) / plot_height
            px = int(plot_x + (x - x_min) / (x_max - x_min) * plot_width)
            py = int(plot_y + plot_height - (y - y_min) / (y_max - y_min) * plot_height)

            img[py - 4:py + 4, px - 4:px + 4] = [100, 100, 100]  # gray box
            num_centroids += 1

            if num_centroids > 3:
                with open("clicked_points.txt", "r") as file:
                    lines = file.readlines()

                if lines:
                    x_str, y_str = lines[0].strip().split()
                    x1 = float(x_str)
                    y1 = float(y_str)
                    px1 = int(plot_x + (x1 - x_min) / (x_max - x_min) * plot_width)
                    py1 = int(plot_y + plot_height - (y1 - y_min) / (y_max - y_min) * plot_height)
                    for i in range(5):
                        for j in range(5):
                            if np.all(img[py1 - i, px1 - j] == [100, 100, 100]):
                                img[py1 - i, px1 - j] = [255, 255, 255]
                            if np.all(img[py1 + i, px1 + j] == [100, 100, 100]):
                                img[py1 + i, px1 + j] = [255, 255, 255]
                            if np.all(img[py1 - i, px1 + j] == [100, 100, 100]):
                                img[py1 - i, px1 + j] = [255, 255, 255]
                            if np.all(img[py1 + i, px1 - j] == [100, 100, 100]):
                                img[py1 + i, px1 - j] = [255, 255, 255]
                    with open("clicked_points.txt", "w") as file:
                        file.writelines(lines[1:])  # remove first
                    with open("clicked_points.txt", "a") as file:
                        file.write(f"{x:.2f} {y:.2f}\n")  # append new
            else:
                with open("clicked_points.txt", "a") as file:
                    file.write(f"{x:.2f} {y:.2f}\n")

            with open("points.txt", "r") as file:
                lines2 = file.readlines()
            for i in range(20):
                x2_str, y2_str = lines2[i].strip().split()
                x2 = float(x2_str)
                y2 = float(y2_str)
                px2 = int(plot_x + (x2 - x_min) / (x_max - x_min) * plot_width)
                py2 = int(plot_y + plot_height - (y2 - y_min) / (y_max - y_min) * plot_height)
                points.append((x2, y2))
                # 5x5 black squares as points
                img[py2 - 2:py2 + 3, px2 - 2:px2 + 3] = [0, 0, 0]

            # Refresh canvas once
            tk_img = ImageTk.PhotoImage(Image.fromarray(img))
            canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
            canvas.image = tk_img
            draw_axes()


    def show_coordinates(event):
        if 120 <= event.x <= 520 and 90 <= event.y <= 390:
            x = x_min + (event.x - plot_x) * (x_max - x_min) / plot_width
            y = y_min + (plot_y + plot_height - event.y) * (y_max - y_min) / plot_height
            coord_label.config(text=f"Coordinates: ({x:.2f}, {y:.2f})")
        else:
            coord_label.config(text="Coordinates: ")

    canvas.bind('<Motion>', show_coordinates)

    def gen_points():
        nonlocal img, tk_img, points_generated, points
        points_generated = True
        points.clear()
        # reset image to white background
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        for i in range(20):
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

    def on_exit():
        open("clicked_points.txt", "w").close()
        open("points.txt", "w").close()
        open("output.txt", "w").close()
        root.destroy()  # This closes the window

    # Register the custom exit function
    root.protocol("WM_DELETE_WINDOW", on_exit)

    # buttons
    button_start = tk.Button(root, text="Start", command=draw_image)
    canvas.create_window(600, 440, anchor='se', window=button_start)

    button_reset = tk.Button(root, text="Reset", command=reset_canvas)
    canvas.create_window(40, 440, anchor='sw', window=button_reset)

    button_gen_points = tk.Button(root, text="Generate Points", command=gen_points)
    canvas.create_window(320, 440, anchor='s', window=button_gen_points)
    
    canvas.bind('<Button-1>', get_centroids)


    draw_axes()
    root.mainloop()


def start_screen():
    start_root = tk.Tk()
    start_root.title("K-Means Clustering - Introduction")
    start_root.geometry("640x480")
    start_root.resizable(False, False)

    # Intro text
    intro_text = (
        "Welcome to the K-Means Clustering Visualizer!\n\n"
        "Raise your hand to camera level to begin"
        "Point your palm to the screen and lift your thumb, index, and middle finger to use the cursor.\n\n"
        "Quickly move your index finger down and then up as if clicking a mouse to left click."
        "This tool helps you understand how the K-Means clustering algorithm works.\n\n"
        "1. Generate random points using the 'Generate Points' button.\n"
        "2. Click to place initial centroids on the graph (max 3 at a time).\n"
        "3. Click 'Start' to run the clustering algorithm implemented in C.\n"
        "4. Watch as the algorithm updates cluster assignments and centroid positions over iterations.\n\n"
        "Click 'Continue' to proceed to the interactive interface."
    )

    text_label = tk.Label(start_root, text=intro_text, wraplength=600, justify="left", font=("Arial", 12))
    text_label.pack(padx=20, pady=40)

    # Continue button
    continue_btn = tk.Button(start_root, text="Continue →", font=("Arial", 12), command=lambda: [start_root.destroy(), run_gui()])
    continue_btn.pack(side='right', padx=20, pady=20)

    start_root.mainloop()




def main():
    try:
        # Start K-means GUI in a separate process
        kmeans_proc = Process(target=start_screen)
        kmeans_proc.start()

        # Start the virtual mouse gesture controller
        controller = ui_virtual_mouse.VirtualMouse()
        controller.run()

        # Wait for the K-means GUI to finish
        kmeans_proc.join()

    except KeyboardInterrupt:
        print("\nApplication interrupted by user")
    finally:
        print("Application closed")


if __name__ == "__main__":
    main()
