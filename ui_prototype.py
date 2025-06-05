import cv2
import numpy as np
from cvzone.HandTrackingModule import HandDetector
import json
import time
from collections import deque

class KMeansPointCollector:
    def __init__(self, canvas_width=720, canvas_height=720):  # Made square
        self.canvas_width = canvas_width
        self.canvas_height = canvas_height
        
        # Create a blank canvas with coordinate grid
        self.canvas = self.create_coordinate_canvas()
        
        # Point collection with undo functionality
        self.collected_points = []  # Will store (x, y) coordinates
        self.point_history = []  # Stack for undo functionality
        self.max_points = 3
        
        # State management
        self.mode = "INITIAL"  # INITIAL, ADJUSTING
        self.adjusting_point_index = 0
        self.initial_points_saved = False
        
        # Hand tracking
        self.detector = HandDetector(detectionCon=0.8, maxHands=2)
        
        # Gesture state tracking
        self.gesture_cooldown = 0
        self.cooldown_frames = 15
        self.prev_fingers = None
        
        # Exit confirmation
        self.exit_hold_time = 0
        self.exit_hold_required = 30
        
        # Index finger gesture tracking (for tap vs hold)
        self.index_down_start_time = 0
        self.index_was_down = False
        self.tap_threshold = 10  # frames - quick tap vs hold
        self.hold_threshold = 35  # frames - time needed for undo
        self.index_hold_confirmed = False  # Flag to prevent multiple undo triggers
        
        # Virtual cursor position with jitter reduction
        self.cursor_pos = [canvas_width // 2, canvas_height // 2]
        self.cursor_history = deque(maxlen=5)  # Keep last 5 positions for smoothing
        self.cursor_smoothing = 0.3  # Lower = more smoothing
        
        # Hand detection status
        self.hands_detected = False
        self.no_hands_message_time = 0
        
        # Point placement confirmation (optional)
        self.confirm_placement = False  # Set to True if you want confirmation
        self.pending_point = None
        
        # Window setup flag to prevent flickering
        self.window_initialized = False
        
        print("K-means Point Collector")
        print("=" * 62)
        print("Hand Gestures:")
        print("   • Peace sign (index + middle UP) = Cursor mode")
        print("   • From cursor: TAP index finger = Place/Adjust point")
        print("   • From cursor: HOLD index finger (1.5s) = Undo last point")
        print("   • From cursor: Drop MIDDLE finger = Toggle mode")
        print("   • Thumb + Pinky UP = Clear all points")
        print("   • BOTH hands all fingers UP (hold 1s) = EXIT")
        print("")
        print("Workflow:")
        print("   1. Use peace sign to move cursor")
        print("   2. Quick tap index finger to place points (3 total)")
        print("   3. Hold index finger down (1.5s) to undo last point")
        print("   4. Points auto-save after 3rd point")
        print("   5. Drop middle finger to enter adjustment mode")
        print("   6. In adjustment mode, tap index finger to adjust each point")
        print("")
        print("Press 'q' to force quit")
    
    def create_coordinate_canvas(self):
        """Create a canvas with square coordinate grid and axes"""
        canvas = np.ones((self.canvas_height, self.canvas_width, 3), dtype=np.uint8) * 240
        
        # Calculate grid spacing for square coordinates
        # Use smaller spacing to create more grid lines for better precision
        grid_spacing = 30  # Smaller spacing for more precision
        grid_color = (200, 200, 200)
        
        # Vertical grid lines
        for x in range(0, self.canvas_width, grid_spacing):
            cv2.line(canvas, (x, 0), (x, self.canvas_height), grid_color, 1)
        
        # Horizontal grid lines
        for y in range(0, self.canvas_height, grid_spacing):
            cv2.line(canvas, (0, y), (self.canvas_width, y), grid_color, 1)
        
        # Draw main axes
        center_x, center_y = self.canvas_width // 2, self.canvas_height // 2
        
        # X-axis (horizontal)
        cv2.line(canvas, (0, center_y), (self.canvas_width, center_y), (0, 0, 0), 2)
        # Y-axis (vertical)
        cv2.line(canvas, (center_x, 0), (center_x, self.canvas_height), (0, 0, 0), 2)
        
        # Add axis labels
        cv2.putText(canvas, "X", (self.canvas_width - 20, center_y - 10), 
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)
        cv2.putText(canvas, "Y", (center_x + 10, 20), 
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)
        
        # Add coordinate values (every 2 grid lines to avoid clutter)
        major_spacing = grid_spacing * 2
        for i, x in enumerate(range(0, self.canvas_width, major_spacing)):
            if x != center_x:
                coord_val = (x - center_x) // grid_spacing
                cv2.putText(canvas, str(coord_val), (x - 8, center_y + 20), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.4, (100, 100, 100), 1)
        
        for i, y in enumerate(range(0, self.canvas_height, major_spacing)):
            if y != center_y:
                coord_val = (center_y - y) // grid_spacing
                cv2.putText(canvas, str(coord_val), (center_x + 8, y + 8), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.4, (100, 100, 100), 1)
        
        return canvas
    
    def screen_to_data_coordinates(self, screen_x, screen_y):
        """Convert screen coordinates to data coordinates"""
        center_x, center_y = self.canvas_width // 2, self.canvas_height // 2
        grid_spacing = 30
        
        data_x = (screen_x - center_x) / grid_spacing
        data_y = (center_y - screen_y) / grid_spacing
        
        return data_x, data_y
    
    def data_to_screen_coordinates(self, data_x, data_y):
        """Convert data coordinates to screen coordinates"""
        center_x, center_y = self.canvas_width // 2, self.canvas_height // 2
        grid_spacing = 30
        
        screen_x = int(center_x + data_x * grid_spacing)
        screen_y = int(center_y - data_y * grid_spacing)
        
        return screen_x, screen_y
    
    def smooth_cursor_position(self, new_x, new_y):
        """Apply smoothing to cursor position to reduce jitter"""
        # Add new position to history
        self.cursor_history.append((new_x, new_y))
        
        if len(self.cursor_history) < 2:
            return new_x, new_y
        
        # Calculate weighted average with more weight on recent positions
        weights = np.array([0.1, 0.15, 0.2, 0.25, 0.3])[-len(self.cursor_history):]
        weights = weights / weights.sum()
        
        avg_x = sum(pos[0] * weight for pos, weight in zip(self.cursor_history, weights))
        avg_y = sum(pos[1] * weight for pos, weight in zip(self.cursor_history, weights))
        
        # Apply exponential smoothing for additional stability
        smooth_x = self.cursor_pos[0] * (1 - self.cursor_smoothing) + avg_x * self.cursor_smoothing
        smooth_y = self.cursor_pos[1] * (1 - self.cursor_smoothing) + avg_y * self.cursor_smoothing
        
        return int(smooth_x), int(smooth_y)
    
    def process_index_finger_gesture(self, current_fingers):
        """Process index finger tap vs hold gestures with validation"""
        # Validate the "index down" state (only middle finger up from peace sign)
        index_is_down = self.validate_finger_state(current_fingers, [0, 0, 1, 0, 0])
    
        # Additional safety: check if we're coming from a valid peace sign state
        peace_sign = [0, 1, 1, 0, 0]
        if not hasattr(self, 'was_peace_sign'):
            self.was_peace_sign = False
    
        if current_fingers == peace_sign:
            self.was_peace_sign = True
    
        # Only process index down if we were recently in peace sign mode
        if index_is_down and not self.was_peace_sign:
            return None
    
        if index_is_down:
            if not self.index_was_down:
                self.index_down_start_time = 0
                self.index_was_down = True
                self.index_hold_confirmed = False
            else:
                self.index_down_start_time += 1
            
            if (self.index_down_start_time >= self.hold_threshold and 
                not self.index_hold_confirmed):
                self.index_hold_confirmed = True
                return "UNDO"
        else:
            if self.index_was_down:
                if (self.index_down_start_time < self.tap_threshold and 
                    not self.index_hold_confirmed):
                    self.index_was_down = False
                    self.index_down_start_time = 0
                    self.was_peace_sign = False  # Reset peace sign state
                    return "TAP"
                else:
                    self.index_was_down = False
                    self.index_down_start_time = 0
                    self.index_hold_confirmed = False
                    self.was_peace_sign = False
        
        return None
    
    def detect_gesture_change(self, current_fingers):
        """Detect when finger state changes for gesture recognition"""
        if self.prev_fingers is None:
            self.prev_fingers = current_fingers
            return None
        
        gesture = None
        
        # Peace sign (index + middle up) = cursor mode
        if current_fingers == [0, 1, 1, 0, 0]:
            gesture = "CURSOR_MODE"
        
        # From peace sign, drop middle finger = toggle mode
        elif self.prev_fingers == [0, 1, 1, 0, 0] and current_fingers == [0, 1, 0, 0, 0]:
            gesture = "TOGGLE_MODE"
        
        # Thumb + Pinky = clear points
        elif current_fingers == [1, 0, 0, 0, 1]:
            gesture = "CLEAR_POINTS"
        
        self.prev_fingers = current_fingers
        return gesture
    
    def validate_finger_state(self, fingers, required_pattern, tolerance_frames=3):
        """Validate finger state with temporal consistency"""
        if not hasattr(self, 'finger_state_history'):
            self.finger_state_history = deque(maxlen=tolerance_frames)
    
        self.finger_state_history.append(fingers)
    
        # Need at least 2 frames for validation
        if len(self.finger_state_history) < 2:
            return False
    
        # Check if the required pattern appears in recent frames
        pattern_matches = sum(1 for state in self.finger_state_history if state == required_pattern)
        return pattern_matches >= (len(self.finger_state_history) // 2)
    
    def undo_last_point(self):
        """Undo the last placed or adjusted point"""
        if not self.point_history:
            return False
    
        last_action = self.point_history.pop()
    
        if last_action[0] == 'add':
         # Undo point placement
            if self.collected_points:
                removed_point = self.collected_points.pop()
                print(f"Undid point placement: ({removed_point[0]:.2f}, {removed_point[1]:.2f})")
            
                # If we had 3 points and now have 2, reset the saved state
                if len(self.collected_points) == 2 and self.initial_points_saved:
                    self.initial_points_saved = False
                    self.mode = "INITIAL"
                return True
            
        elif last_action[0] == 'adjust':
            # Undo point adjustment
            old_point, point_index = last_action[1], last_action[2]
            self.collected_points[point_index] = old_point
            print(f"Undid adjustment of point {point_index + 1}: restored to ({old_point[0]:.2f}, {old_point[1]:.2f})")
            return True
    
        return False
    
    def validate_finger_separation(self, landmarks, finger1_tip, finger2_tip, min_distance=40):
        """Ensure fingers are sufficiently separated"""
        try:
            tip1 = landmarks[finger1_tip]
            tip2 = landmarks[finger2_tip]
            distance = np.sqrt((tip1[0] - tip2[0])**2 + (tip1[1] - tip2[1])**2)
            return distance > min_distance
        except (IndexError, KeyError):
            return False
        
    
    
    def draw_cursor(self, frame, x, y, color=(0, 255, 0)):
        """Draw crosshair cursor at given position"""
        # Ensure cursor is within bounds
        if 0 <= x < self.canvas_width and 0 <= y < self.canvas_height:
            # Crosshair lines
            cv2.line(frame, (max(0, x - 15), y), (min(self.canvas_width-1, x + 15), y), color, 2)
            cv2.line(frame, (x, max(0, y - 15)), (x, min(self.canvas_height-1, y + 15)), color, 2)
            # Center dot
            cv2.circle(frame, (x, y), 3, color, -1)
    
    def draw_points(self, frame):
        """Draw collected points on the frame"""
        colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
        
        for i, (data_x, data_y) in enumerate(self.collected_points):
            screen_x, screen_y = self.data_to_screen_coordinates(data_x, data_y)
            
            # Highlight currently adjusting point
            if self.mode == "ADJUSTING" and i == self.adjusting_point_index:
                pulse = int(20 * (0.5 + 0.5 * np.sin(time.time() * 8)))
                cv2.circle(frame, (screen_x, screen_y), 12 + pulse//4, (255, 255, 0), 3)
            
            # Draw point
            color = colors[i % len(colors)]
            cv2.circle(frame, (screen_x, screen_y), 8, color, -1)
            cv2.circle(frame, (screen_x, screen_y), 8, (0, 0, 0), 2)
            
            # Add point number
            cv2.putText(frame, str(i + 1), (screen_x - 4, screen_y + 4), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)
    
    def draw_hold_progress(self, frame):
        """Draw progress bar for hold gestures"""
        if self.index_was_down and self.index_down_start_time > 0:
            # Position at top center
            bar_width = 150
            bar_height = 8
            bar_x = (self.canvas_width - bar_width) // 2
            bar_y = 15
            
            # Background
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + bar_width, bar_y + bar_height), (100, 100, 100), -1)
            
            # Progress
            progress = min(self.index_down_start_time / self.hold_threshold, 1.0)
            progress_width = int(progress * bar_width)
            
            if progress < 1.0:
                color = (0, 255, 255)  # Yellow while building up
                text = "Hold to UNDO"
            else:
                color = (0, 255, 0)  # Green when ready
                text = "Release to UNDO"
                
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + progress_width, bar_y + bar_height), color, -1)
            
            # Text
            cv2.putText(frame, text, (bar_x, bar_y - 5), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
    
    def save_points(self, suffix=""):
        """Save collected points to JSON file with error handling"""
        if not self.collected_points:
            print("No points to save!")
            return False
        
        filename = f'kmeans_centroids{suffix}.json'
        
        data = {
            'centroids': [
                {'x': float(x), 'y': float(y)} 
                for x, y in self.collected_points
            ],
            'timestamp': time.time(),
            'readable_time': time.strftime('%Y-%m-%d %H:%M:%S'),
            'total_points': len(self.collected_points),
            'mode': self.mode,
            'adjustment_cycle': self.adjusting_point_index if self.mode == "ADJUSTING" else None
        }
        
        try:
            with open(filename, 'w') as f:
                json.dump(data, f, indent=2)
            print(f"Points saved to {filename}")
            return True
        except PermissionError:
            print(f"Permission denied: Cannot write to {filename}")
            return False
        except Exception as e:
            print(f"Failed to save points: {e}")
            return False
    
    def draw_status_panel(self, frame, current_gesture, cursor_active, exit_gesture_detected):
        """Draw consolidated status panel at top of frame"""
        # Status background - positioned to not overlap with instructions
        panel_height = 120
        cv2.rectangle(frame, (0, 0), (self.canvas_width, panel_height), (0, 0, 0), -1)
        cv2.rectangle(frame, (0, 0), (self.canvas_width, panel_height), (255, 255, 255), 1)
        
        # Status information
        mode_color = (255, 0, 255) if self.mode == "ADJUSTING" else (0, 255, 0)
        cursor_status = "ACTIVE" if cursor_active else "INACTIVE"
        
        status_texts = [
            f"Mode: {self.mode}",
            f"Points: {len(self.collected_points)}/{self.max_points}",
            f"Cursor: {cursor_status}",
            f"Action: {current_gesture}",
        ]
        
        if self.mode == "ADJUSTING":
            status_texts.append(f"Adjusting: Point {self.adjusting_point_index + 1}")
        
        # Draw status texts in two columns
        for i, text in enumerate(status_texts):
            if i < 3:
                x_pos = 10
                y_pos = 20 + i * 18
            else:
                x_pos = 350
                y_pos = 20 + (i - 3) * 18
            
            color = mode_color if "Mode:" in text else (255, 255, 255)
            cv2.putText(frame, text, (x_pos, y_pos), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
        
        # Draw hold progress bar in status panel
        self.draw_hold_progress(frame)
        
        # Draw exit progress bar if needed
        if exit_gesture_detected and self.exit_hold_time > 0:
            bar_width = 200
            bar_height = 12
            bar_x = (self.canvas_width - bar_width) // 2
            bar_y = 90
            
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + bar_width, bar_y + bar_height), (100, 100, 100), -1)
            progress_width = int((self.exit_hold_time / self.exit_hold_required) * bar_width)
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + progress_width, bar_y + bar_height), (0, 0, 255), -1)
            cv2.putText(frame, "Hold both hands up to exit", (bar_x - 40, bar_y - 5), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
    
    def draw_instructions(self, frame):
        """Draw mode-specific instructions at bottom of frame"""
        # Instructions background - positioned at bottom to avoid overlap
        instruction_start_y = self.canvas_height - 90
        cv2.rectangle(frame, (0, instruction_start_y), (self.canvas_width, self.canvas_height), (50, 50, 50), -1)
        cv2.rectangle(frame, (0, instruction_start_y), (self.canvas_width, self.canvas_height), (100, 100, 100), 1)
        
        if self.mode == "INITIAL":
            instructions = [
                "Peace sign: Move cursor",
                "TAP INDEX: Place point",
                "HOLD INDEX (1.5s): Undo last point",
                "Drop MIDDLE: Start adjusting (after 3 points)",
                "Thumb+Pinky: Clear all points"
            ]
        else:
            instructions = [
                f"Adjusting Point {self.adjusting_point_index + 1}",
                "Peace sign: Move cursor",
                "TAP INDEX: Adjust current point", 
                "HOLD INDEX (1.5s): Undo adjustment",
                "Drop MIDDLE: Exit adjustment mode"
            ]
        
        for i, instruction in enumerate(instructions):
            y_pos = instruction_start_y + 15 + i * 14
            cv2.putText(frame, instruction, (10, y_pos), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
    
    def initialize_window(self):
        """Initialize the window once to prevent flickering"""
        if not self.window_initialized:
            cv2.namedWindow("K-means Point Collector", cv2.WINDOW_NORMAL)
            cv2.resizeWindow("K-means Point Collector", 960, 720)
            self.window_initialized = True
    
    def run(self):
        """Main application loop with enhanced error handling"""
        try:
            cap = cv2.VideoCapture(0)
            if not cap.isOpened():
                print("Error: Could not open camera. Please check your camera connection.")
                return
            
            cap.set(3, 640)  # Keep camera at lower resolution for performance
            cap.set(4, 480)
            
            # Initialize window once
            self.initialize_window()
            
            while True:
                success, camera_frame = cap.read()
                if not success:
                    print("Error: Failed to read from camera")
                    break
                
                camera_frame = cv2.flip(camera_frame, 1)
                
                try:
                    hands, camera_frame = self.detector.findHands(camera_frame, flipType=False)
                except Exception as e:
                    print(f"Hand detection error: {e}")
                    hands = []
                
                # Create working canvas
                display_frame = self.canvas.copy()
                
                # Update hand detection status
                self.hands_detected = len(hands) > 0
                if not self.hands_detected:
                    self.no_hands_message_time = time.time()
                
                # Process hand gestures
                current_gesture = "None"
                exit_gesture_detected = False
                cursor_active = False
                
                if hands and self.gesture_cooldown == 0:
                    # Check for exit gesture first (both hands, all fingers up)
                    if len(hands) == 2:
                        try:
                            both_hands_all_fingers = all(
                                sum(self.detector.fingersUp(hand)) == 5 for hand in hands
                            )
                            if both_hands_all_fingers:
                                exit_gesture_detected = True
                                self.exit_hold_time += 1
                                current_gesture = f"EXITING... {self.exit_hold_time}/{self.exit_hold_required}"
                                
                                if self.exit_hold_time >= self.exit_hold_required:
                                    print("Exit confirmed - saving final results...")
                                    self.save_points("_final")
                                    break
                        except Exception as e:
                            print(f"Error processing exit gesture: {e}")
                    
                    # Reset exit timer if gesture not detected
                    if not exit_gesture_detected:
                        self.exit_hold_time = 0
                    
                    # Process single hand gestures
                    if len(hands) >= 1 and not exit_gesture_detected:
                        try:
                            hand = hands[0]
                            fingers = self.detector.fingersUp(hand)
                            
                            # Process index finger tap/hold gestures
                            index_action = self.process_index_finger_gesture(fingers)
                            
                            if index_action == "UNDO":
                                if self.undo_last_point():
                                    current_gesture = "UNDID LAST POINT"
                                else:
                                    current_gesture = "NOTHING TO UNDO"
                                self.gesture_cooldown = self.cooldown_frames
                            
                            elif index_action == "TAP":
                                if self.mode == "INITIAL":
                                    if len(self.collected_points) < self.max_points:
                                        data_x, data_y = self.screen_to_data_coordinates(self.cursor_pos[0], self.cursor_pos[1])
                                        self.collected_points.append((data_x, data_y))
                                        self.point_history.append(('add', (data_x, data_y)))
                                        current_gesture = f"POINT {len(self.collected_points)} PLACED"
                                        print(f"Point {len(self.collected_points)}: ({data_x:.2f}, {data_y:.2f})")
                                        
                                        if len(self.collected_points) == 3:
                                            if self.save_points("_initial"):
                                                self.initial_points_saved = True
                                                print("Initial 3 points complete! Drop middle finger to start adjusting.")
                                        
                                        self.gesture_cooldown = self.cooldown_frames
                                    else:
                                        current_gesture = "3 POINTS COMPLETE"
                                
                                elif self.mode == "ADJUSTING":
                                    data_x, data_y = self.screen_to_data_coordinates(self.cursor_pos[0], self.cursor_pos[1])
                                    old_point = self.collected_points[self.adjusting_point_index]
                                    self.collected_points[self.adjusting_point_index] = (data_x, data_y)
                                    self.point_history.append(('adjust', old_point, self.adjusting_point_index))
                                    current_gesture = f"ADJUSTED POINT {self.adjusting_point_index + 1}"
                                    print(f"Point {self.adjusting_point_index + 1} adjusted: ({data_x:.2f}, {data_y:.2f})")
                                    
                                    self.save_points(f"_adjust_{self.adjusting_point_index + 1}")
                                    
                                    self.adjusting_point_index = (self.adjusting_point_index + 1) % 3
                                    print(f"Now adjusting point {self.adjusting_point_index + 1}")
                                    
                                    self.gesture_cooldown = self.cooldown_frames
                            
                            # Detect other gesture changes
                            gesture_action = self.detect_gesture_change(fingers)
                            
                            # Handle cursor mode
                            if fingers == [0, 1, 1, 0, 0]:  # Peace sign - cursor mode
                                cursor_active = True
                                current_gesture = "CURSOR MODE"
                                
                                # Use middle finger tip for cursor
                                middle_tip = hand["lmList"][12]
                                # Map from camera resolution to canvas resolution
                                raw_x = int(np.interp(middle_tip[0], [0, 640], [0, self.canvas_width]))
                                raw_y = int(np.interp(middle_tip[1], [0, 480], [0, self.canvas_height]))
                                
                                # Apply smoothing and clamping
                                smooth_x, smooth_y = self.smooth_cursor_position(raw_x, raw_y)
                                self.cursor_pos = [
                                    max(0, min(self.canvas_width - 1, smooth_x)),
                                    max(0, min(self.canvas_height - 1, smooth_y))
                                ]
                            
                            # Handle other gesture actions
                            if gesture_action:
                                if gesture_action == "TOGGLE_MODE":
                                    if len(self.collected_points) == 3 and self.initial_points_saved:
                                        if self.mode == "INITIAL":
                                            self.mode = "ADJUSTING"
                                            self.adjusting_point_index = 0
                                            current_gesture = "ADJUSTMENT MODE"
                                            print("Entered adjustment mode - adjusting point 1")
                                        else:
                                            self.mode = "INITIAL"
                                            current_gesture = "INITIAL MODE"
                                            print("Returned to initial mode")
                                        self.gesture_cooldown = self.cooldown_frames
                                    else:
                                        current_gesture = "NEED 3 POINTS FIRST"
                                
                                elif gesture_action == "CLEAR_POINTS":
                                    self.collected_points = []
                                    self.point_history = []
                                    self.mode = "INITIAL"
                                    self.adjusting_point_index = 0
                                    self.initial_points_saved = False
                                    current_gesture = "POINTS CLEARED"
                                    print("All points cleared - back to initial mode")
                                    self.gesture_cooldown = self.cooldown_frames
                        
                        except Exception as e:
                            print(f"Error processing hand gesture: {e}")
                            current_gesture = "GESTURE ERROR"
                
                # Reset states if no hands detected
                if not hands:
                    self.exit_hold_time = 0
                    self.prev_fingers = None
                    self.index_was_down = False
                    self.index_down_start_time = 0
                    self.index_hold_confirmed = False
                
                # Decrease cooldown
                if self.gesture_cooldown > 0:
                    self.gesture_cooldown -= 1
                
                # Determine cursor color based on mode and state
                if not self.hands_detected:
                    cursor_color = (100, 100, 100)  # Gray when no hands
                elif cursor_active:
                    cursor_color = (0, 255, 0)  # Green when active
                elif self.mode == "ADJUSTING":
                    cursor_color = (255, 0, 255)  # Purple in adjustment mode
                else:
                    cursor_color = (100, 100, 100)  # Gray when inactive
                
                # Draw cursor
                self.draw_cursor(display_frame, self.cursor_pos[0], self.cursor_pos[1], cursor_color)
                
                # Draw collected points
                self.draw_points(display_frame)
                
                # Draw status panel at top
                self.draw_status_panel(display_frame, current_gesture, cursor_active, exit_gesture_detected)
                
                # Draw instructions at bottom
                self.draw_instructions(display_frame)
                
                # Show no hands warning in middle area (not overlapping with status or instructions)
                if not self.hands_detected and time.time() - self.no_hands_message_time < 3:
                    warning_y = self.canvas_height // 2
                    cv2.putText(display_frame, "NO HANDS DETECTED", (self.canvas_width // 2 - 100, warning_y), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
                
                # Display frames
                cv2.imshow("Camera Feed", camera_frame)
                cv2.namedWindow("K-means Point Collector", cv2.WINDOW_NORMAL)
                cv2.resizeWindow("K-means Point Collector", 960, 720)
                cv2.imshow("K-means Point Collector", display_frame)
                
                # Exit on 'q' key
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        
        except Exception as e:
            print(f"Critical error in main loop: {e}")
        finally:
            # Cleanup
            try:
                cap.release()
                cv2.destroyAllWindows()
            except:
                pass
            
            # Final summary
            if self.collected_points:
                print("\nFinal Results:")
                print("=" * 30)
                for i, (x, y) in enumerate(self.collected_points):
                    print(f"Centroid {i+1}: ({x:.3f}, {y:.3f})")
                
                # Save final points if we have any
                if len(self.collected_points) > 0:
                    self.save_points("_final")
            else:
                print("\nNo points collected")

if __name__ == "__main__":
    try:
        collector = KMeansPointCollector()
        collector.run()
    except KeyboardInterrupt:
        print("\nApplication interrupted by user")
    except Exception as e:
        print(f"Application failed to start: {e}")
        print("Please check your camera connection and cvzone installation")