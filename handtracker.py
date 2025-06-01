import cv2
from cvzone.HandTrackingModule import HandDetector

cap = cv2.VideoCapture(0)
detector = HandDetector(detectionCon=0.8, maxHands=1)

while True:
    success, img = cap.read()
    img = cv2.flip(img, 1)  # Flip only the display, not detection
    hands, img = detector.findHands(img, flipType=False)



    if hands:
        lmList = hands[0]['lmList']  # List of 21 hand landmarks
        index_finger_tip = lmList[8]  # x, y, z
        cv2.circle(img, (index_finger_tip[0], index_finger_tip[1]), 10, (0, 255, 0), cv2.FILLED)

    cv2.imshow("Hand Tracking", img)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
