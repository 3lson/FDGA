from multiprocessing import Process
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import numpy as np
import subprocess
import ui_virtual_mouse 
import os
import pyautogui

# Screen dimensions
SCREEN_W, SCREEN_H = pyautogui.size()

# Target window size
WINDOW_WIDTH = 640
WINDOW_HEIGHT = 480

# Positions
GUI_POS_X = 645
GUI_POS_Y = 0


def get_geometry_string(x, y, width=WINDOW_WIDTH, height=WINDOW_HEIGHT):
    return f"{width}x{height}+{x}+{y}"

# Plotting area size (proportional to window)
PLOT_WIDTH = int(WINDOW_WIDTH * 0.5)
PLOT_HEIGHT = int(WINDOW_HEIGHT * 0.5)
PLOT_X = (WINDOW_WIDTH - PLOT_WIDTH) // 2
PLOT_Y = (WINDOW_HEIGHT - PLOT_HEIGHT) // 2



# Environment setup
env = os.environ.copy()
env["PATH"] += r";C:\msys64\mingw64\bin"
script_dir = os.path.dirname(os.path.abspath(__file__))
c_file_path = os.path.join(script_dir, 'k_means3.c')
exe_path = os.path.join(script_dir, 'k_means3')


def parse_input(input_str):
    """Parse the output from the C k-means implementation."""
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
    """Generate a visual representation of the clusters."""
    img = np.ones((height, width, 3), dtype=np.uint8) * 255
    plot_width, plot_height = 400, 300
    plot_x = (width - plot_width) // 2
    plot_y = (height - plot_height) // 2

    x_min, x_max = 0, 10
    y_min, y_max = 0, 10

    def scale(x, y):
        """Convert data coordinates to pixel coordinates."""
        px = plot_x + int((x - x_min) / (x_max - x_min) * plot_width)
        py = plot_y + plot_height - int((y - y_min) / (y_max - y_min) * plot_height)
        return px, py

    def inv_scale(px, py):
        """Convert pixel coordinates to data coordinates."""
        x = x_min + (px - plot_x) * (x_max - x_min) / plot_width
        y = y_min + (plot_y + plot_height - py) * (y_max - y_min) / plot_height
        return x, y

    # Color scheme for clusters
    light_colors = [
        [255, 0, 0],    # Red
        [0, 255, 0],    # Green
        [0, 0, 255]     # Blue
    ]

    # Color each pixel based on nearest centroid
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

    # Draw data points
    for cluster in clusters:
        for x, y in cluster['points']:
            px, py = scale(x, y)
            img[py-2:py+3, px-2:px+3] = 0  # Black squares for points
        
        # Draw centroids
        cx, cy = cluster['centroid']
        px, py = scale(cx, cy)
        img[py-3:py+4, px-3:px+4] = 0  # Larger black squares for centroids

    return img


def run_gui():
    """Main GUI application for K-means visualization."""
    root = tk.Tk()
    root.title("K-Means Visualization")
    root.geometry(get_geometry_string(GUI_POS_X, GUI_POS_Y, WINDOW_WIDTH, WINDOW_HEIGHT))
    root.resizable(False, False)

    # Use parameterized dimensions
    canvas = tk.Canvas(root, width=WINDOW_WIDTH, height=WINDOW_HEIGHT)
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
        """Draw coordinate axes on the canvas."""
        # axes
        canvas.create_line(plot_x, plot_y + plot_height, plot_x + plot_width, plot_y + plot_height, fill='black', tags='plot')  # X-axis
        canvas.create_line(plot_x, plot_y + plot_height, plot_x, plot_y, fill='black', tags='plot') 

        
        # Arrows
        canvas.create_line(520, 390, 515, 385, fill='black', tags='plot')
        canvas.create_line(520, 390, 515, 395, fill='black', tags='plot')
        canvas.create_line(120, 90, 115, 95, fill='black', tags='plot')
        canvas.create_line(120, 90, 125, 95, fill='black', tags='plot')

    def draw_image():
        """Execute the K-means algorithm and start animation."""
        nonlocal img, tk_img, points_generated
        
        if not points_generated:
            messagebox.showwarning("Warning", "Please generate points before starting.")
            return

        # Compile and run the C code
        compilation = subprocess.run(
            ['gcc', c_file_path, '-o', exe_path],
            env=env,
            capture_output=True,
            text=True
        )
        
        if compilation.returncode != 0:
            messagebox.showerror("Compilation Error", compilation.stderr)
            return
        
        subprocess.run(['./k_means3'], capture_output=True, text=True)

        # Read the full output file with all iterations
        with open('output.txt', 'r') as f:
            input_data = f.read()
        
        all_iterations = parse_input(input_data)
        animate(0, all_iterations)

    def animate(i, all_iterations):
        """Animate K-means iterations."""
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
        
        # Schedule next frame
        root.after(500, lambda: animate(i + 1, all_iterations))

    def reset_canvas():
        """Reset the canvas and clear all data."""
        nonlocal img, tk_img, points_generated, num_centroids
        
        num_centroids = 0
        points_generated = False
        points.clear()
        
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        
        # Clear files
        open("clicked_points.txt", "w").close()
        
        canvas.delete('plot')
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()

    # Coordinate display
    coord_label = tk.Label(root, text="Coordinates: ", font=("Georgia", 10))
    coord_label.place(x=260, y=450)

    def get_centroids(event):
        """Handle mouse clicks to place centroids."""
        nonlocal img, tk_img, points_generated, num_centroids, points

        if 120 <= event.x <= 520 and 90 <= event.y <= 390:
            # Convert pixel coordinates to data coordinates
            x = x_min + (event.x - plot_x) * (x_max - x_min) / plot_width
            y = y_min + (plot_y + plot_height - event.y) * (y_max - y_min) / plot_height
            px = int(plot_x + (x - x_min) / (x_max - x_min) * plot_width)
            py = int(plot_y + plot_height - (y - y_min) / (y_max - y_min) * plot_height)

            # Draw centroid
            img[py - 4:py + 4, px - 4:px + 4] = [100, 100, 100]  # Gray box
            num_centroids += 1

            # Handle maximum of 3 centroids
            if num_centroids > 3:
                with open("clicked_points.txt", "r") as file:
                    lines = file.readlines()

                if lines:
                    # Remove the oldest centroid visually
                    x_str, y_str = lines[0].strip().split()
                    x1 = float(x_str)
                    y1 = float(y_str)
                    px1 = int(plot_x + (x1 - x_min) / (x_max - x_min) * plot_width)
                    py1 = int(plot_y + plot_height - (y1 - y_min) / (y_max - y_min) * plot_height)
                    
                    # Clear old centroid
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
                    
                    # Update file
                    with open("clicked_points.txt", "w") as file:
                        file.writelines(lines[1:])  # Remove first line
                    
                    with open("clicked_points.txt", "a") as file:
                        file.write(f"{x:.2f} {y:.2f}\n")  # Append new centroid
            else:
                with open("clicked_points.txt", "a") as file:
                    file.write(f"{x:.2f} {y:.2f}\n")

            # Redraw all points
            with open("points.txt", "r") as file:
                lines2 = file.readlines()
            
            for i in range(20):
                x2_str, y2_str = lines2[i].strip().split()
                x2 = float(x2_str)
                y2 = float(y2_str)
                px2 = int(plot_x + (x2 - x_min) / (x_max - x_min) * plot_width)
                py2 = int(plot_y + plot_height - (y2 - y_min) / (y_max - y_min) * plot_height)
                points.append((x2, y2))
                # Draw 5x5 black squares as points
                img[py2 - 2:py2 + 3, px2 - 2:px2 + 3] = [0, 0, 0]

            # Refresh canvas
            tk_img = ImageTk.PhotoImage(Image.fromarray(img))
            canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
            canvas.image = tk_img
            draw_axes()

    def show_coordinates(event):
        """Display mouse coordinates."""
        if 120 <= event.x <= 520 and 90 <= event.y <= 390:
            x = x_min + (event.x - plot_x) * (x_max - x_min) / plot_width
            y = y_min + (plot_y + plot_height - event.y) * (y_max - y_min) / plot_height
            coord_label.config(text=f"Coordinates: ({x:.2f}, {y:.2f})")
        else:
            coord_label.config(text="Coordinates: ")

    def gen_points():
        """Generate random data points."""
        nonlocal img, tk_img, points_generated, points
        
        points_generated = True
        points.clear()
        
        # Reset image to white background
        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        
        # Generate 20 random points
        for i in range(20):
            x = np.random.uniform(x_min, x_max)
            y = np.random.uniform(y_min, y_max)
            px = int(plot_x + (x - x_min) / (x_max - x_min) * plot_width)
            py = int(plot_y + plot_height - (y - y_min) / (y_max - y_min) * plot_height)
            points.append((x, y))
            # Draw 5x5 black squares as points
            img[py - 2:py + 3, px - 2:px + 3] = [0, 0, 0]
        
        # Save points to file
        with open('points.txt', 'w') as f:
            for point in points:
                f.write(f"{point[0]:.2f} {point[1]:.2f}\n")
        
        # Update canvas
        tk_img = ImageTk.PhotoImage(Image.fromarray(img))
        canvas.create_image(0, 0, anchor='nw', image=tk_img, tags='plot')
        canvas.image = tk_img
        draw_axes()

    def on_exit():
        """Clean up files on exit."""
        open("clicked_points.txt", "w").close()
        open("points.txt", "w").close()
        open("output.txt", "w").close()
        root.destroy()

    # Event bindings
    canvas.bind('<Motion>', show_coordinates)
    canvas.bind('<Button-1>', get_centroids)
    root.protocol("WM_DELETE_WINDOW", on_exit)

    # Create buttons
    button_start = tk.Button(root, text="Start", command=draw_image)
    canvas.create_window(600, 440, anchor='se', window=button_start)

    button_reset = tk.Button(root, text="Reset", command=reset_canvas)
    canvas.create_window(40, 440, anchor='sw', window=button_reset)

    button_gen_points = tk.Button(root, text="Generate Points", command=gen_points)
    canvas.create_window(320, 440, anchor='s', window=button_gen_points)

    # Initialize display
    draw_axes()
    root.mainloop()


def start_screen():
    """Display tutorial screens before starting the main application."""
    tutorial_pages = [
        {
            "title": "Raise your hand to camera level to begin",
            "content": "Point your palm to the screen and lift your thumb, index, and middle finger to use the cursor.\n\n"
                      "Quickly move your index finger down and then up as if clicking a mouse to left click."
        },
        {
            "title": "What is K-Means Clustering?",
            "content": "K-means is the most important unsupervised machine learning algorithm.\n\n"
                      "It groups unlabeled data into k clusters.\n\n"
                      "Each cluster contains similar data and is defined by a central point called a centroid."
        },
        {
            "title": "Key Idea",
            "content": "Minimize the distance between points and their centroid.\n\n"
                      "This reduces intra-cluster variance.\n\n"
                      "Good clustering means tight, well-separated groups."
        },
        {
            "title": "How Does It Work? (Part 1)",
            "content": "1. Choose how many clusters (k) are suitable.\n\n"
                      "2. Place k centroids randomly.\n\n"
                      "3. The algorithm assigns each datapoint to its nearest centroid.\n\n"
                      "4. Recalculate each centroid as the mean of its points."
        },
        {
            "title": "Convergence Criteria",
            "content": "The algorithm stops when cluster assignments no longer change, or when centroids shift less than a small threshold."
        },
        {
            "title": "Why Use K-Means?",
            "content": "✓ Simple and fast\n"
                      "✓ Works well on large datasets\n"
                      "✓ Easy to implement and scale\n"
                      "✓ A strong baseline method"
        },
        {
            "title": "Limitations of K-Means",
            "content": "✗ Requires specifying k\n"
                      "✗ Assumes clusters of similar size and shape \n"
                      "✗ Sensitive to outliers and centroid initialization"
        },
        {
            "title": "Improving K-Means",
            "content": "• Use k-means++ for better initial centroids\n"
                      "• Run multiple times and pick the best result\n"
                      "• Normalize features to balance dimensions"
        },
        {
            "title": "Where It's Used",
            "content": "Common applications include:\n\n"
                      "• Image compression\n"
                      "• Customer segmentation\n"
                      "• Market research\n"
                      "• Document clustering"
        },
        {
            "title": "More Use Cases",
            "content": "Also useful in:\n\n"
                      "• Gene expression analysis\n"
                      "• Anomaly detection\n"
                      "• Dimensionality reduction (as preprocessing)\n"
                      "• Feature engineering"
        },
        {
            "title": "What You'll Do",
            "content": "Next, you'll:\n\n"
                      "• Generate random points\n"
                      "• Place 3 centroids by clicking\n"
                      "• Watch the algorithm move your centroids as one iteration of the algorithm is calculated."
        },
        {
            "title": "Let's Get Started!",
            "content": "Ready to explore K-means?\n\n"
                      "Click below to launch the visualizer and begin."
        }
    ]
    
  
    current_page = 0
    
    def show_page(page_index):
        """Display a specific tutorial page."""
        nonlocal current_page
        current_page = page_index
        
        # Clear the window
        for widget in start_root.winfo_children():
            widget.destroy()
        
        page = tutorial_pages[page_index]
        
        # Title
        title_label = tk.Label(
            start_root, 
            text=page["title"], 
            font=("Georgia", 15, "bold"), 
            fg="navy"
        )
        title_label.pack(pady=(40, 20))
        
        # Content
        content_label = tk.Label(
            start_root, 
            text=page["content"], 
            font=("Georgia", 11), 
            wraplength=550, 
            justify="left"
        )
        content_label.pack(pady=20, padx=40)
        
        # Progress indicator
        progress_text = f"Page {page_index + 1} of {len(tutorial_pages)}"
        progress_label = tk.Label(
            start_root, 
            text=progress_text, 
            font=("Georgia", 9), 
            fg="gray"
        )
        progress_label.pack(pady=10)
        
        # Button frame
        button_frame = tk.Frame(start_root)
        button_frame.pack(side='bottom', fill='x', padx=20, pady=20)
        
        # Previous button (if not first page)
        if page_index > 0:
            prev_btn = tk.Button(
                button_frame, 
                text="← Previous", 
                font=("Georgia", 10), 
                command=lambda: show_page(page_index - 1)
            )
            prev_btn.pack(side='left')
        
        # Skip button (if not last page)
        if page_index < len(tutorial_pages) - 1:
            skip_btn = tk.Button(
                button_frame, 
                text="Skip Tutorial", 
                font=("Georgia", 10), 
                command=lambda: [start_root.destroy(), run_gui()]
            )
            skip_btn.pack(side='left', padx=(20, 0))
        
        # Next/Start button
        if page_index < len(tutorial_pages) - 1:
            next_btn = tk.Button(
                button_frame, 
                text="Next →", 
                font=("Georgia", 10), 
                command=lambda: show_page(page_index + 1)
            )
            next_btn.pack(side='right')
        else:
            start_btn = tk.Button(
                button_frame, 
                text="Start Visualizer →", 
                font=("Georgia", 12, "bold"), 
                bg="lightblue", 
                command=lambda: [start_root.destroy(), run_gui()]
            )
            start_btn.pack(side='right')
    
    start_root = tk.Tk()
    start_root.title("K-Means Clustering - Tutorial")
    start_root.geometry(get_geometry_string(GUI_POS_X, GUI_POS_Y, WINDOW_WIDTH, WINDOW_HEIGHT))
    start_root.resizable(False, False)
    
    # Show first page
    show_page(0)
    start_root.mainloop()


def main():
    """Main application entry point."""
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