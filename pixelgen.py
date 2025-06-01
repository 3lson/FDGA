import numpy as np
from PIL import Image

# taht needs chnaging too later when input is fixed for now luckily chat worked it out how to change my old weird inout into sensible stuff
def parse_input(input_str):
    clusters = []
    for line in input_str.strip().split('\n'):
        if not line.strip():
            continue
        parts = line.split(':')
        centroid_part = parts[0].strip()
        points_part = parts[1].strip() if len(parts) > 1 else ''
        
        # Parse centroid
        centroid_name, centroid_coords = centroid_part.split(' ', 1)
        centroid_x, centroid_y = map(float, centroid_coords.strip('()').split(','))
        
        # Parse points
        points = []
        if points_part:
            for point_str in points_part.replace('(', '').split(')'):
                if point_str.strip():
                    x, y = map(float, point_str.strip().split(','))
                    points.append((x, y))
        
        clusters.append({
            'name': centroid_name,
            'centroid': (centroid_x, centroid_y),
            'points': points
        })
    return clusters

def calculate_plot_scale(clusters, plot_width, plot_height):

    # Extracts all points and all clusters into a list neeed to change that cause input for now is wrong
    all_points = [c['centroid'] for c in clusters]
    for cluster in clusters:
        all_points.extend(cluster['points'])
    
    # Separate x and y values
    x_vals = [p[0] for p in all_points]
    y_vals = [p[1] for p in all_points]
    x_min, x_max = min(x_vals), max(x_vals)
    y_min, y_max = min(y_vals), max(y_vals)
    
    # Scaling function scales from x,y coordinates into pixels
    def to_pixel(x, y):

        px = int(plot_width * (x - x_min) / (x_max - x_min))
        py = int(plot_height * (1 - (y - y_min) / (y_max - y_min)))
        return px, py
    
    # Inverse scaling function does the reveres of the above function
    def to_data(px, py):

        x = x_min + (px / plot_width) * (x_max - x_min)
        y = y_min + (1 - py / plot_height) * (y_max - y_min)
        return x, y
    
    return to_pixel, to_data, (x_min, x_max, y_min, y_max)

def generate_visualization(clusters, width=640, height=480):
    # Create blank white image
    img = np.ones((height, width, 3), dtype=np.uint8) * 255
    
    # Define plot area
    plot_width, plot_height = 400, 300
    plot_x = (width - plot_width) // 2
    plot_y = (height - plot_height) // 2
    
    # Get scaling functions and data bounds
    to_pixel, to_data, _ = calculate_plot_scale(clusters, plot_width, plot_height)
    
    # Cluster colors
    cluster_colors = [
        [255, 0, 0],   # Red
        [0, 255, 0],   # Green
        [0, 0, 255]    # Blue
    ]
    
    # Color regions by nearest centroid
    for y in range(plot_height):
        for x in range(plot_width):
            # Convert to data coordinates
            data_x, data_y = to_data(x, y)
            
            # Find closest cluster
            min_distance = float('inf')
            closest_idx = 0
            for i, cluster in enumerate(clusters):
                cx, cy = cluster['centroid']
                distance = (data_x - cx)**2 + (data_y - cy)**2
                if distance < min_distance:
                    min_distance = distance
                    closest_idx = i
            
            # Color pixel
            img[plot_y + y, plot_x + x] = cluster_colors[closest_idx]
    
    # Draw axes
    # X-axis
    axis_y = plot_y + plot_height
    img[axis_y-1:axis_y+1, plot_x:plot_x+plot_width] = 0
    
    # Y-axis
    axis_x = plot_x
    img[plot_y:plot_y+plot_height, axis_x-1:axis_x+1] = 0

    # Draw arrowheads
    arrow_size = 5
    # X-arrow
    for i in range(arrow_size+1):
        img[axis_y - i, plot_x+plot_width - i] = 0  
        img[axis_y + i, plot_x+plot_width - i] = 0  
    
    # Y-arrow
    for i in range(arrow_size+1):
        img[plot_y + i, axis_x + i] = 0  
        img[plot_y + i, axis_x - i] = 0 
    
    # Draw points and centroids
    for cluster in clusters:
        # Draw data points
        for point in cluster['points']:
            px, py = to_pixel(*point)
            img[plot_y + py - 2:plot_y + py + 3, 
                plot_x + px - 2:plot_x + px + 3] = 0
        
        # Draw centroid
        cx, cy = cluster['centroid']
        px, py = to_pixel(cx, cy)
        img[plot_y + py - 3:plot_y + py + 4, 
            plot_x + px - 3:plot_x + px + 4] = 0
    
    return img

def save_pixel_array(img, filename):
    with open(filename, 'w') as f:
        f.write('[\n')
        for row in img:
            # need to change that to 8 bit values instea dof a numbers 
            pixels = ', '.join(f'[{r}, {g}, {b}]' for r, g, b in row)
            f.write(f' [{pixels}],\n')
        f.write(']\n')

# Data for now need to change to the input file once I work out how to convert the data there to points and centroids
input_data = """
C0 (2.5, 2.5): (2.0, 2.0) (3.0, 3.0) (2.5, 2.0)
C1 (2.5, 7.5): (2.0, 7.0) (3.0, 8.0) (2.5, 7.5)
C2 (7.5, 5.0): (7.0, 4.5) (8.0, 5.5) (7.5, 5.0)
"""


clusters = parse_input(input_data)
image_data = generate_visualization(clusters)
save_pixel_array(image_data, 'kmeans_pixels.txt')
Image.fromarray(image_data).save('kmeans.png')