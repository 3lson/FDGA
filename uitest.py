import cv2
import numpy as np
from cvzone.HandTrackingModule import HandDetector
from PIL import Image

# ------------------------
# Parsing and Static Plotting Functions
# ------------------------

def parse_input(input_str):
    clusters = []
    for line in input_str.strip().split('\n'):
        if not line.strip():
            continue
        parts = line.split(':')
        centroid_part = parts[0].strip()
        points_part = parts[1].strip() if len(parts) > 1 else ''
        
        # Parse centroid (e.g., "C0 (2.5, 2.5)")
        centroid_name, centroid_coords = centroid_part.split(' ', 1)
        centroid_x, centroid_y = map(float, centroid_coords.strip('()').split(','))
        
        # Parse points (e.g., "(2.0, 2.0) (3.0, 3.0) ...")
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
    # Gather all data points (both centroids and cluster points)
    all_points = [c['centroid'] for c in clusters]
    for cluster in clusters:
        all_points.extend(cluster['points'])
    
    # Compute data bounds
    x_vals = [p[0] for p in all_points]
    y_vals = [p[1] for p in all_points]
    x_min, x_max = min(x_vals), max(x_vals)
    y_min, y_max = min(y_vals), max(y_vals)
    
    def to_pixel(x, y):
        # Map data coordinates to pixel coordinates in the plot area.
        if (x_max - x_min) == 0 or (y_max - y_min) == 0:
            return 0, 0
        px = int(plot_width * (x - x_min) / (x_max - x_min))
        py = int(plot_height * (1 - (y - y_min) / (y_max - y_min)))
        return px, py
    
    def to_data(px, py):
        x = x_min + (px / plot_width) * (x_max - x_min)
        y = y_min + (1 - py / plot_height) * (y_max - y_min)
        return x, y
    
    return to_pixel, to_data, (x_min, x_max, y_min, y_max)

def generate_visualization(clusters, width=640, height=480):
    # Create a blank white canvas.
    img = np.ones((height, width, 3), dtype=np.uint8) * 255
    
    # Define plot area dimensions and position.
    plot_width, plot_height = 400, 300
    plot_x = (width - plot_width) // 2
    plot_y = (height - plot_height) // 2
    
    # Get scaling functions.
    to_pixel, to_data, _ = calculate_plot_scale(clusters, plot_width, plot_height)
    
    # Define colors for clusters.
    cluster_colors = [
        [255, 0, 0],
        [0, 255, 0],
        [0, 0, 255]
    ]
    
    # For each pixel in the plot area, assign a color based on the nearest centroid.
    for y in range(plot_height):
        for x in range(plot_width):
            data_x, data_y = to_data(x, y)
            min_distance = float('inf')
            closest_idx = 0
            for i, cluster in enumerate(clusters):
                cx, cy = cluster['centroid']
                distance = (data_x - cx)**2 + (data_y - cy)**2
                if distance < min_distance:
                    min_distance = distance
                    closest_idx = i
            img[plot_y + y, plot_x + x] = cluster_colors[closest_idx]
    
    # Draw axes with arrowheads.
    axis_y = plot_y + plot_height
    cv2.line(img, (plot_x, axis_y), (plot_x + plot_width, axis_y), (0, 0, 0), 2)  # X-axis
    cv2.line(img, (plot_x, plot_y), (plot_x, plot_y + plot_height), (0, 0, 0), 2)  # Y-axis
    
    arrow_size = 5
    # X-axis arrow at right end.
    cv2.line(img, (plot_x + plot_width, axis_y),
             (plot_x + plot_width - arrow_size, axis_y - arrow_size), (0, 0, 0), 2)
    cv2.line(img, (plot_x + plot_width, axis_y),
             (plot_x + plot_width - arrow_size, axis_y + arrow_size), (0, 0, 0), 2)
    # Y-axis arrow at top.
    cv2.line(img, (plot_x, plot_y),
             (plot_x - arrow_size, plot_y + arrow_size), (0, 0, 0), 2)
    cv2.line(img, (plot_x, plot_y),
             (plot_x + arrow_size, plot_y + arrow_size), (0, 0, 0), 2)
    
    # Draw data points and centroids.
    for cluster in clusters:
        for point in cluster['points']:
            px, py = to_pixel(*point)
            cv2.rectangle(img, (plot_x + px - 2, plot_y + py - 2),
                          (plot_x + px + 2, plot_y + py + 2), (0, 0, 0), -1)
        cx, cy = cluster['centroid']
        px, py = to_pixel(cx, cy)
        cv2.rectangle(img, (plot_x + px - 3, plot_y + py - 3),
                      (plot_x + px + 3, plot_y + py + 3), (0, 0, 0), -1)
    
    return img

def save_pixel_array(img, filename):
    with open(filename, 'w') as f:
        f.write('[\n')
        for row in img:
            pixels = ', '.join(f'[{r}, {g}, {b}]' for r, g, b in row)
            f.write(f' [{pixels}],\n')
        f.write(']\n')

# Helper functions for coordinate transformation
def screen_to_world(screen_x, screen_y, zoom, offset_x, offset_y, canvas_width, canvas_height):
    """Convert screen coordinates to world coordinates (original image space)"""
    # Calculate the size of the crop region in world space
    crop_w = canvas_width / zoom
    crop_h = canvas_height / zoom
    
    # Calculate the world space crop region
    center_x = canvas_width // 2 + offset_x
    center_y = canvas_height // 2 + offset_y
    crop_x = center_x - crop_w // 2
    crop_y = center_y - crop_h // 2
    
    # Convert screen coordinates to world coordinates
    world_x = crop_x + (screen_x / canvas_width) * crop_w
    world_y = crop_y + (screen_y / canvas_height) * crop_h
    
    return world_x, world_y

def world_to_screen(world_x, world_y, zoom, offset_x, offset_y, canvas_width, canvas_height):
    """Convert world coordinates to screen coordinates"""
    # Calculate the size of the crop region in world space
    crop_w = canvas_width / zoom
    crop_h = canvas_height / zoom
    
    # Calculate the world space crop region
    center_x = canvas_width // 2 + offset_x
    center_y = canvas_height // 2 + offset_y
    crop_x = center_x - crop_w // 2
    crop_y = center_y - crop_h // 2
    
    # Convert world coordinates to screen coordinates
    screen_x = ((world_x - crop_x) / crop_w) * canvas_width
    screen_y = ((world_y - crop_y) / crop_h) * canvas_height
    
    return int(screen_x), int(screen_y)

# ------------------------
# Data and Static Visualization Generation
# ------------------------

input_data = """
C0 (2.5, 2.5): (2.0, 2.0) (3.0, 3.0) (2.5, 2.0)
C1 (2.5, 7.5): (2.0, 7.0) (3.0, 8.0) (2.5, 7.5)
C2 (7.5, 5.0): (7.0, 4.5) (8.0, 5.5) (7.5, 5.0)
"""

clusters = parse_input(input_data)
static_img = generate_visualization(clusters, width=640, height=480)
save_pixel_array(static_img, 'kmeans_pixels.txt')
Image.fromarray(static_img).save('kmeans.png')

# ------------------------
# Interactive Zoom and Pan Setup (Improved Version with Zoom-Scaled Drawing)
# ------------------------

CANVAS_WIDTH, CANVAS_HEIGHT = 640, 480

# Initialize zoom and pan variables
zoom = 1.0  
offset_x = 0.0  
offset_y = 0.0  

# Drawing variables - now store in world coordinates
drawing_enabled = False
drawing_points = []  # List of strokes, each stroke is a list of (world_x, world_y) coordinates
current_drawing_stroke = []  # Current stroke being drawn in world coordinates

# Set up video capture and the hand detector with higher maxHands for left/right detection
cap = cv2.VideoCapture(0)
cap.set(3, CANVAS_WIDTH)
cap.set(4, CANVAS_HEIGHT)
detector = HandDetector(detectionCon=0.7, maxHands=2)  # Detect up to 2 hands

# Improved sensitivity values:
pan_speed = 5.0  # Panning speed
zoom_speed = 0.15  # Smoother zoom changes
max_zoom = 8.0   # Maximum zoom level
min_zoom = 1   # Minimum zoom level

print("Enhanced Hand Gesture Controls:")
print("👉 RIGHT hand peace sign (palm facing you) = Zoom IN")
print("👈 LEFT hand peace sign (palm facing you) = Zoom OUT")
print("👆 Point with index finger = DRAW on graph")
print("☝️ 1 finger (fist + thumb) = Pan UP")
print("✌️ 2 fingers (peace sign back of hand) = Pan DOWN") 
print("🤟 3 fingers = Pan LEFT")
print("🖖 4 fingers = Pan RIGHT")
print("✋ 5 fingers = Reset view")
print("🖐️ 6 fingers (both hands open) = Clear drawings")
print("Press 'q' to quit")

# Gesture state tracking to prevent rapid firing
gesture_cooldown = 0
COOLDOWN_FRAMES = 5

while True:
    success, cam_img = cap.read()
    if not success:
        break

    # Flip the camera image for mirror view.
    cam_img = cv2.flip(cam_img, 1)
    hands, cam_img = detector.findHands(cam_img, flipType=False)
    
    # Add gesture instructions on camera feed
    cv2.putText(cam_img, "Enhanced Hand Gestures:", (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
    cv2.putText(cam_img, "Peace (palm toward you) = Zoom", (10, 45), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1)
    cv2.putText(cam_img, "Point finger = Draw", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1)
    cv2.putText(cam_img, "1/2/3/4 fingers = Pan", (10, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1)
    cv2.putText(cam_img, "5 fingers = Reset", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1)
    
    # Decrease cooldown counter
    if gesture_cooldown > 0:
        gesture_cooldown -= 1
    
    current_gesture = "None"
    
    if hands and gesture_cooldown == 0:
        # Check for drawing gesture first (pointing finger)
        drawing_detected = False
        
        for hand in hands:
            fingers = detector.fingersUp(hand)
            lmList = hand["lmList"]
            
            # Check for pointing gesture (only index finger up)
            if fingers == [0, 1, 0, 0, 0]:  # Only index finger up
                # Get the index finger tip position
                index_tip = lmList[8]
                finger_x, finger_y = index_tip[0], index_tip[1]
                
                # Map finger position to screen coordinates
                draw_x = int((finger_x / CANVAS_WIDTH) * CANVAS_WIDTH)
                draw_y = int((finger_y / CANVAS_HEIGHT) * CANVAS_HEIGHT)
                
                # Ensure coordinates are within bounds
                draw_x = max(0, min(CANVAS_WIDTH - 1, draw_x))
                draw_y = max(0, min(CANVAS_HEIGHT - 1, draw_y))
                
                # Convert screen coordinates to world coordinates
                world_x, world_y = screen_to_world(draw_x, draw_y, zoom, offset_x, offset_y, CANVAS_WIDTH, CANVAS_HEIGHT)
                
                # Add to current stroke in world coordinates
                current_drawing_stroke.append((world_x, world_y))
                drawing_detected = True
                current_gesture = "DRAWING"
                break
        
        # If we were drawing but no longer pointing, save the stroke
        if not drawing_detected and len(current_drawing_stroke) > 0:
            drawing_points.append(current_drawing_stroke.copy())
            current_drawing_stroke = []
        
        # Check for zoom gestures (peace signs with left/right hand detection)
        zoom_detected = False
        
        if not drawing_detected:  # Only check zoom if not drawing
            for hand in hands:
                fingers = detector.fingersUp(hand)
                hand_type = hand["type"]  # "Left" or "Right"
                
                # Check for peace sign 
                if fingers == [0, 1, 1, 0, 0]:  
                    if hand_type == "Right":  # Right hand peace = Zoom IN
                        zoom = min(max_zoom, zoom + zoom_speed)
                        current_gesture = "RIGHT PEACE - ZOOM IN"
                        zoom_detected = True
                        gesture_cooldown = COOLDOWN_FRAMES
                        break
                    elif hand_type == "Left":  # Left hand peace = Zoom OUT
                        zoom = max(min_zoom, zoom - zoom_speed)
                        current_gesture = "LEFT PEACE - ZOOM OUT"
                        zoom_detected = True
                        gesture_cooldown = COOLDOWN_FRAMES
                        break
        
        # If no zoom or drawing gesture detected, check for panning gestures
        if not zoom_detected and not drawing_detected and len(hands) > 0:
            # Use the first hand for panning gestures
            hand = hands[0]
            fingers = detector.fingersUp(hand)
            fingers_count = sum(fingers)
            
            # Check for clear drawings gesture (both hands with 5 fingers each)
            if len(hands) == 2 and all(sum(detector.fingersUp(h)) == 5 for h in hands):
                drawing_points = []
                current_drawing_stroke = []
                current_gesture = "CLEAR DRAWINGS"
                gesture_cooldown = COOLDOWN_FRAMES
            
            # Pan based on number of fingers - ADJUSTED for better recognition
            elif fingers == [1, 0, 0, 0, 0]:  # Only thumb up = Pan UP
                offset_y -= pan_speed
                current_gesture = "THUMB UP - PAN UP"
                gesture_cooldown = COOLDOWN_FRAMES // 2
                
            elif fingers == [1, 0, 0, 0, 1]:  # YOLO = Pan DOWN
                offset_y += pan_speed
                current_gesture = "PEACE BACK - PAN DOWN"
                gesture_cooldown = COOLDOWN_FRAMES // 2
                
            elif fingers == [0, 1, 1, 1, 0]:  # 3 fingers = Pan LEFT
                offset_x -= pan_speed
                current_gesture = "3 FINGERS - PAN LEFT"
                gesture_cooldown = COOLDOWN_FRAMES // 2
                
            elif fingers == [0, 1, 1, 1, 1]:  # 4 fingers = Pan RIGHT
                offset_x += pan_speed
                current_gesture = "4 FINGERS - PAN RIGHT"
                gesture_cooldown = COOLDOWN_FRAMES // 2
                
            elif fingers_count == 5:  # 5 fingers = Reset
                zoom = 1.0
                offset_x = 0.0
                offset_y = 0.0
                current_gesture = "5 FINGERS - RESET VIEW"
                gesture_cooldown = COOLDOWN_FRAMES
    
    # Display current gesture
    if current_gesture != "None":
        cv2.putText(cam_img, current_gesture, (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    
    # Show hand type labels
    if hands:
        for hand in hands:
            hand_center = hand["center"]
            hand_type = hand["type"]
            cv2.putText(cam_img, hand_type, (hand_center[0]-30, hand_center[1]-20), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 0, 255), 2)
    
    # --- Apply Zoom and Pan ---
    # Calculate the size of the crop region
    crop_w = max(1, int(CANVAS_WIDTH / zoom))
    crop_h = max(1, int(CANVAS_HEIGHT / zoom))
    
    # Calculate crop center with pan offset
    center_x = CANVAS_WIDTH // 2 + int(offset_x)
    center_y = CANVAS_HEIGHT // 2 + int(offset_y)
    
    # Constrain the crop region to stay within image bounds
    crop_x = max(0, min(CANVAS_WIDTH - crop_w, center_x - crop_w // 2))
    crop_y = max(0, min(CANVAS_HEIGHT - crop_h, center_y - crop_h // 2))
    
    # Update offsets to match the constrained crop position
    actual_center_x = crop_x + crop_w // 2
    actual_center_y = crop_y + crop_h // 2
    offset_x = actual_center_x - CANVAS_WIDTH // 2
    offset_y = actual_center_y - CANVAS_HEIGHT // 2
    
    # Extract and resize the cropped region
    try:
        cropped_view = static_img[crop_y:crop_y+crop_h, crop_x:crop_x+crop_w]
        if cropped_view.size > 0:
            interactive_view = cv2.resize(cropped_view, (CANVAS_WIDTH, CANVAS_HEIGHT))
        else:
            interactive_view = static_img
    except:
        interactive_view = static_img
    
    # Add zoom level and pan info
    zoom_text = f"Zoom: {zoom:.1f}x"
    pan_text = f"Pan: ({offset_x:.0f}, {offset_y:.0f})"
    drawing_text = f"Strokes: {len(drawing_points)}"
    
    cv2.putText(interactive_view, zoom_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 2)
    cv2.putText(interactive_view, zoom_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)
    cv2.putText(interactive_view, pan_text, (10, 55), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)
    cv2.putText(interactive_view, pan_text, (10, 55), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    cv2.putText(interactive_view, drawing_text, (10, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)
    cv2.putText(interactive_view, drawing_text, (10, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    
    # Draw all completed strokes - convert from world coordinates to screen coordinates
    for stroke in drawing_points:
        if len(stroke) > 1:
            screen_points = []
            for world_x, world_y in stroke:
                screen_x, screen_y = world_to_screen(world_x, world_y, zoom, offset_x, offset_y, CANVAS_WIDTH, CANVAS_HEIGHT)
                # Only add points that are visible on screen
                if 0 <= screen_x < CANVAS_WIDTH and 0 <= screen_y < CANVAS_HEIGHT:
                    screen_points.append((screen_x, screen_y))
            
            # Draw lines between consecutive visible points
            for i in range(len(screen_points) - 1):
                cv2.line(interactive_view, screen_points[i], screen_points[i + 1], (255, 0, 255), 3)  # Magenta lines
    
    # Draw current stroke being drawn - convert from world coordinates to screen coordinates
    if len(current_drawing_stroke) > 1:
        screen_points = []
        for world_x, world_y in current_drawing_stroke:
            screen_x, screen_y = world_to_screen(world_x, world_y, zoom, offset_x, offset_y, CANVAS_WIDTH, CANVAS_HEIGHT)
            # Only add points that are visible on screen
            if 0 <= screen_x < CANVAS_WIDTH and 0 <= screen_y < CANVAS_HEIGHT:
                screen_points.append((screen_x, screen_y))
        
        # Draw lines between consecutive visible points
        for i in range(len(screen_points) - 1):
            cv2.line(interactive_view, screen_points[i], screen_points[i + 1], (0, 255, 255), 3)  # Yellow lines
    
    # Display both windows
    cv2.imshow("Camera Feed", cam_img)
    cv2.imshow("Cluster Viewer", interactive_view)
    
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()