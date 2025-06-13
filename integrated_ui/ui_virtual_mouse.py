import cv2
import numpy as np
from cvzone.HandTrackingModule import HandDetector
import pyautogui
import time
from collections import deque

class VirtualMouse:
    def __init__(self):
        # Hand tracking
        self.detector = HandDetector(detectionCon=0.8, maxHands=2)
        
        # Gesture state tracking
        self.gesture_cooldown = 0
        self.cooldown_frames = 15
        self.prev_fingers = None
        
        # Index finger gesture tracking (for tap vs hold)
        self.index_down_start_time = 0
        self.index_was_down = False
        self.tap_threshold = 10  # frames - quick tap vs hold
        self.hold_threshold = 35  # frames - time needed for hold action
        self.index_hold_confirmed = False
        
        # Mouse position smoothing
        self.mouse_history = deque(maxlen=5)
        self.mouse_smoothing = 0.3
        
        # Hand detection status
        self.hands_detected = False
        self.no_hands_message_time = 0
        
        # Exit confirmation
        self.exit_hold_time = 0
        self.exit_hold_required = 30
        
        # Screen dimensions
        self.screen_width, self.screen_height = pyautogui.size()
        
        # Camera dimensions (for mapping)
        self.cam_width = 640
        self.cam_height = 480
        
        # Safety settings
        pyautogui.FAILSAFE = True
        pyautogui.PAUSE = 0.01
        
        print("Virtual Mouse Controller")
        print("=" * 50)
        print("Hand Gestures:")
        print("   • Peace sign (index + middle UP) = Move mouse cursor")
        print("   • From cursor mode: DROP index finger = LEFT CLICK")
        print("   • BOTH hands all fingers UP (hold 1s) = EXIT")
        print("")
        print("Workflow:")
        print("   1. Use peace sign to move mouse cursor")
        print("   2. Drop index finger briefly to left-click")
        print("   3. Return to peace sign to continue cursor control")
        print("")
        print("Press 'q' to force quit")

        # self.cv_pos_x = CV_POS_X
        # self.cv_pos_y = CV_POS_Y
    
    def smooth_mouse_position(self, new_x, new_y):
        """Apply smoothing to mouse position to reduce jitter"""
        self.mouse_history.append((new_x, new_y))
        
        if len(self.mouse_history) < 2:
            return new_x, new_y
        
        weights = np.array([0.1, 0.15, 0.2, 0.25, 0.3])[-len(self.mouse_history):]
        weights = weights / weights.sum()
        
        avg_x = sum(pos[0] * weight for pos, weight in zip(self.mouse_history, weights))
        avg_y = sum(pos[1] * weight for pos, weight in zip(self.mouse_history, weights))
        
        return int(avg_x), int(avg_y)
    
    def move_mouse(self, finger_tip):
        """Move mouse cursor based on finger position"""
        if finger_tip is not None:
            # Map finger position to screen coordinates
            x = int(np.interp(finger_tip[0], [0, self.cam_width], [0, self.screen_width]))
            y = int(np.interp(finger_tip[1], [0, self.cam_height], [0, self.screen_height]))
            
            # Apply smoothing
            smooth_x, smooth_y = self.smooth_mouse_position(x, y)
            
            # Ensure coordinates are within screen bounds
            smooth_x = max(0, min(self.screen_width - 1, smooth_x))
            smooth_y = max(0, min(self.screen_height - 1, smooth_y))
            
            # Move the actual mouse cursor
            pyautogui.moveTo(smooth_x, smooth_y)
            
            return smooth_x, smooth_y
        return None, None
    
    def process_index_finger_gesture(self, current_fingers):
        """Process index finger tap vs hold gestures"""
        index_is_down = self.validate_finger_state(current_fingers, [0, 0, 1, 0, 0])
    
        if not hasattr(self, 'was_peace_sign'):
            self.was_peace_sign = False
    
        peace_sign = [0, 1, 1, 0, 0]
        if current_fingers == peace_sign:
            self.was_peace_sign = True
    
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
                return "HOLD"
        else:
            if self.index_was_down:
                if (self.index_down_start_time < self.tap_threshold and 
                    not self.index_hold_confirmed):
                    self.index_was_down = False
                    self.index_down_start_time = 0
                    self.was_peace_sign = False
                    return "TAP"
                else:
                    self.index_was_down = False
                    self.index_down_start_time = 0
                    self.index_hold_confirmed = False
                    self.was_peace_sign = False
        
        return None
    
    def validate_finger_state(self, fingers, required_pattern, tolerance_frames=3):
        """Validate finger state with temporal consistency"""
        if not hasattr(self, 'finger_state_history'):
            self.finger_state_history = deque(maxlen=tolerance_frames)
    
        self.finger_state_history.append(fingers)
    
        if len(self.finger_state_history) < 2:
            return False
    
        pattern_matches = sum(1 for state in self.finger_state_history if state == required_pattern)
        return pattern_matches >= (len(self.finger_state_history) // 2)
    
    def draw_status_overlay(self, frame, current_gesture, cursor_active, exit_gesture_detected, mouse_pos):
        """Draw status information on camera frame"""
        # Draw semi-transparent overlay
        overlay = frame.copy()
        cv2.rectangle(overlay, (10, 10), (300, 120), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.7, frame, 0.3, 0, frame)
        
        # Status text
        status_texts = [
            f"Mouse Control: {'ACTIVE' if cursor_active else 'INACTIVE'}",
            f"Action: {current_gesture}",
            f"Mouse Position: {mouse_pos if mouse_pos[0] else 'N/A'}",
            f"Screen: {self.screen_width}x{self.screen_height}"
        ]
        
        for i, text in enumerate(status_texts):
            color = (0, 255, 0) if "ACTIVE" in text else (255, 255, 255)
            cv2.putText(frame, text, (15, 30 + i * 20), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
        
        # Draw hold progress bar if needed
        # if self.index_was_down and self.index_down_start_time > 0:
        #     bar_width = 200
        #     bar_height = 8
        #     bar_x = (frame.shape[1] - bar_width) // 2
        #     bar_y = 30
            
        #     cv2.rectangle(frame, (bar_x, bar_y), (bar_x + bar_width, bar_y + bar_height), (100, 100, 100), -1)
            
        #     progress = min(self.index_down_start_time / self.hold_threshold, 1.0)
        #     progress_width = int(progress * bar_width)
            
        #     color = (0, 255, 255) if progress < 1.0 else (0, 255, 0)
        #     cv2.rectangle(frame, (bar_x, bar_y), (bar_x + progress_width, bar_y + bar_height), color, -1)
            
        #     text = "Hold Progress" if progress < 1.0 else "Hold Complete"
        #     cv2.putText(frame, text, (bar_x, bar_y - 5), 
        #                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
        
        # Draw exit progress bar if needed
        if exit_gesture_detected and self.exit_hold_time > 0:
            bar_width = 200
            bar_height = 12
            bar_x = (frame.shape[1] - bar_width) // 2
            bar_y = 60
            
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + bar_width, bar_y + bar_height), (100, 100, 100), -1)
            progress_width = int((self.exit_hold_time / self.exit_hold_required) * bar_width)
            cv2.rectangle(frame, (bar_x, bar_y), (bar_x + progress_width, bar_y + bar_height), (0, 0, 255), -1)
            cv2.putText(frame, "Hold both hands up to exit", (bar_x - 40, bar_y - 5), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
    
    # def draw_cursor(self, frame, x, y, color=(0, 255, 0)):
    #     """Draw crosshair cursor at given position"""
    #     # Ensure cursor is within bounds
    #     if 0 <= x < self.canvas_width and 0 <= y < self.canvas_height:
    #         # Crosshair lines
    #         cv2.line(frame, (max(0, x - 15), y), (min(self.canvas_width-1, x + 15), y), color, 2)
    #         cv2.line(frame, (x, max(0, y - 15)), (x, min(self.canvas_height-1, y + 15)), color, 2)
    #         # Center dot
    #         cv2.circle(frame, (x, y), 3, color, -1)

    def draw_instructions(self, frame):
        """Draw instructions at bottom of frame"""
        instructions = [
            "Peace sign: Control mouse",
            "Drop INDEX finger: LEFT CLICK",
            "Both hands up: Exit"
        ]
        
        # Semi-transparent background
        overlay = frame.copy()
        start_y = frame.shape[0] - 80
        cv2.rectangle(overlay, (0, start_y), (frame.shape[1], frame.shape[0]), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.7, frame, 0.3, 0, frame)
        
        for i, instruction in enumerate(instructions):
            y_pos = start_y + 20 + i * 18
            cv2.putText(frame, instruction, (10, y_pos), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    
    def run(self):
        """Main application loop"""
        try:
            cap = cv2.VideoCapture(0)
            if not cap.isOpened():
                print("Error: Could not open camera")
                return
            
            cap.set(3, self.cam_width)
            cap.set(4, self.cam_height)

            # cv2.namedWindow("Virtual Mouse Controller", cv2.WINDOW_NORMAL)
            # cv2.moveWindow("Virtual Mouse Controller", self.cv_pos_x, self.cv_pos_y)
            
            while True:
                success, frame = cap.read()
                if not success:
                    print("Error: Failed to read from camera")
                    break
                
                frame = cv2.flip(frame, 1)
                
                try:
                    hands, frame = self.detector.findHands(frame, flipType=False)
                except Exception as e:
                    print(f"Hand detection error: {e}")
                    hands = []
                
                self.hands_detected = len(hands) > 0
                if not self.hands_detected:
                    self.no_hands_message_time = time.time()
                
                current_gesture = "None"
                exit_gesture_detected = False
                cursor_active = False
                mouse_pos = (None, None)
                
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
                                    print("Exit confirmed")
                                    break
                        except Exception as e:
                            print(f"Error processing exit gesture: {e}")
                    
                    if not exit_gesture_detected:
                        self.exit_hold_time = 0
                    
                    # Process single hand gestures
                    if len(hands) >= 1 and not exit_gesture_detected:
                        try:
                            hand = hands[0]
                            fingers = self.detector.fingersUp(hand)
                            
                            # Process index finger tap/hold gestures
                            index_action = self.process_index_finger_gesture(fingers)
                            
                            if index_action == "TAP":
                                # Perform left click at current mouse position
                                pyautogui.click()
                                current_gesture = "LEFT CLICK!"
                                print(f"Left click performed")
                                self.gesture_cooldown = self.cooldown_frames
                            
                            elif index_action == "HOLD":
                                current_gesture = "HOLD DETECTED"
                                self.gesture_cooldown = self.cooldown_frames
                            
                            # Handle cursor mode (peace sign)
                            if fingers == [0, 1, 1, 0, 0]:
                                cursor_active = True
                                current_gesture = "MOUSE CONTROL"
                                
                                # Use middle finger tip for mouse control
                                middle_tip = hand["lmList"][12]  # Middle finger tip
                                mouse_pos = self.move_mouse(middle_tip)
                        
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

                # Draw cursor
                # self.draw_cursor(display_frame, self.cursor_pos[0], self.cursor_pos[1], yellow)
                
                # Draw status overlay
                self.draw_status_overlay(frame, current_gesture, cursor_active, exit_gesture_detected, mouse_pos)
                
                # Draw instructions
                self.draw_instructions(frame)
                
                # Show no hands warning
                if not self.hands_detected and time.time() - self.no_hands_message_time < 3:
                    warning_y = frame.shape[0] // 2
                    cv2.putText(frame, "NO HANDS DETECTED", (frame.shape[1] // 2 - 100, warning_y), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
                
                # Display frame
                cv2.imshow("Virtual Mouse Controller", frame)
                
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        
        except Exception as e:
            print(f"Critical error: {e}")
        finally:
            try:
                cap.release()
                cv2.destroyAllWindows()
            except:
                pass
            
            print("Virtual mouse controller stopped")

if __name__ == "__main__":
    try:
        controller = VirtualMouse()
        controller.run()
    except KeyboardInterrupt:
        print("\nApplication interrupted by user")
    except Exception as e:
        print(f"Application failed to start: {e}")
        print("Please check required packages: pip install opencv-python cvzone pyautogui")