# -*- coding: utf-8 -*-
"""
Created on Sat May 16 23:26:24 2026

@author: igokraj
"""

import cv2
import numpy as np
import serial
import time
from keras.models import load_model

# ==========================================
# SERIAL PORT CONFIGURATION
SERIAL_PORT = 'COM3'  # Change to the correct port
# ==========================================

print("Initializing connection with STM32...")
try:
    stm32 = serial.Serial(SERIAL_PORT, 115200, timeout=1)
    time.sleep(2)
except Exception as e:
    print(f"Serial port error: {e}")
    exit()

print("Loading model...")
model = load_model("keras_model.h5", compile=False)
class_names = open("labels.txt", "r").readlines()

camera = cv2.VideoCapture(0)
np.set_printoptions(suppress=True)

print("Camera started! System ready.")

# --- NEW VARIABLES FOR TIME TRACKING ---
gesture_start_time = 0      # Saves the moment the gesture is first detected
is_gesture_active = False   # Stores the state of whether the gesture is currently held
required_time = 3.0         # How many seconds the gesture must be held
# ---------------------------------------

while True:
    ret, image = camera.read()
    if not ret:
        continue

    display_image = image.copy()

    height, width, _ = image.shape
    min_dim = min(height, width)
    start_x = (width - min_dim) // 2
    start_y = (height - min_dim) // 2
    image = image[start_y:start_y+min_dim, start_x:start_x+min_dim]
    
    image = cv2.resize(image, (224, 224), interpolation=cv2.INTER_AREA)
    image = np.asarray(image, dtype=np.float32).reshape(1, 224, 224, 3)
    image = (image / 127.5) - 1

    prediction = model.predict(image, verbose=0)
    index = np.argmax(prediction)
    class_name = class_names[index].strip()
    confidence_score = prediction[0][index]

    # --- NEW GESTURE HOLDING LOGIC ---
    # If the correct gesture is detected with confidence > 90%
    if index == 0 and confidence_score > 0.90:  
        if not is_gesture_active:
            # First moment of gesture detection - start the timer
            is_gesture_active = True
            gesture_start_time = time.time()
            print("Starting to count time...")
        
        else:
            # Gesture is being held - check how much time has passed
            held_time = time.time() - gesture_start_time
            
            # Display progress on the camera window
            cv2.putText(display_image, f"Hold: {int(held_time)}/{int(required_time)} s", 
                        (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 165, 255), 2)
            
            # If the required 3 seconds have passed
            if held_time >= required_time:
                print("Gesture detected for 3s! Sending signal to STM32...")
                stm32.write('G'.encode('utf-8'))
                
                # Mark success on the screen
                cv2.putText(display_image, "SUCCESS!", (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                cv2.imshow("Gesture Recognition", display_image)
                cv2.waitKey(1) # Refresh the window to show the SUCCESS text
                
                # Pause the loop for a moment and reset the flag,
                # to avoid sending the command multiple times in a row
                time.sleep(3)
                is_gesture_active = False
                
    else:
        # If there is no gesture on the frame (or confidence dropped) - reset the timer!
        if is_gesture_active:
            print("Gesture interrupted! Timer reset.")
            is_gesture_active = False
    # -----------------------------------

    # Display standard detection information
    cv2.putText(display_image, f"Detected: {class_name[2:]} ({str(np.round(confidence_score * 100))[:-2]}%)", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.imshow("Gesture Recognition", display_image)

    keyboard_input = cv2.waitKey(1)
    if keyboard_input == 27:
        break

camera.release()
cv2.destroyAllWindows()
stm32.close()