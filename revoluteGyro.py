import serial
import time
import numpy as np
from scipy.spatial.transform import Rotation as R
from OpenGL.GL import *
from OpenGL.GLUT import *
from OpenGL.GLU import *
import pygame
from pygame.locals import *

# Set up the serial connection (adjust '/dev/tty.usbmodem...' to your device's serial port)
ser = serial.Serial('/dev/tty.usbmodem101', 9600, timeout=1)

# Function to parse the serial data
def parse_data(line):
    try:
        data = line.decode('utf-8').strip().split(',')
        x = float(data[0].split(':')[1])
        y = float(data[1].split(':')[1])
        z = float(data[2].split(':')[1])
        return x, y, z
    except:
        return None, None, None

def euler_to_radians(roll, pitch, yaw):
    return np.radians([roll, pitch, yaw])

def draw_cylinder(radius=1.0, height=1.0, num_slices=32):
    # Draw top face (different color)
    glColor3f(1.0, 0.0, 0.0)  # Red color
    glBegin(GL_TRIANGLE_FAN)
    glVertex3f(0.0, height / 2, 0.0)  # Center of the top face
    for i in range(num_slices + 1):
        angle = 2 * np.pi * i / num_slices
        x = radius * np.cos(angle)
        y = height / 2
        z = radius * np.sin(angle)
        glVertex3f(x, y, z)
    glEnd()

    # Draw bottom face (different color)
    glColor3f(0.0, 0.0, 1.0)  # Blue color
    glBegin(GL_TRIANGLE_FAN)
    glVertex3f(0.0, -height / 2, 0.0)  # Center of the bottom face
    for i in range(num_slices + 1):
        angle = 2 * np.pi * i / num_slices
        x = radius * np.cos(angle)
        y = -height / 2
        z = radius * np.sin(angle)
        glVertex3f(x, y, z)
    glEnd()

    # Draw the side faces
    glColor3f(0.0, 1.0, 0.0)  # Green color
    glBegin(GL_QUAD_STRIP)
    for i in range(num_slices + 1):
        angle = 2 * np.pi * i / num_slices
        x = radius * np.cos(angle)
        z = radius * np.sin(angle)
        glVertex3f(x, -height / 2, z)  # Bottom vertex
        glVertex3f(x, height / 2, z)   # Top vertex
    glEnd()

def main():
    pygame.init()
    display = (800, 600)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    gluPerspective(45, (display[0] / display[1]), 0.1, 50.0)
    glTranslatef(0.0, 0.0, -5)

    roll, pitch, yaw = 0, 0, 0

    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    ser.close()
                    quit()

            if ser.in_waiting > 0:
                line = ser.readline()
                x, y, z = parse_data(line)
                if x is not None and y is not None and z is not None:
                    roll, pitch, yaw = euler_to_radians(x, y, z)

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
            glPushMatrix()
            glRotatef(np.degrees(-pitch), 1, 0, 0)
            glRotatef(np.degrees(yaw), 0, 1, 0)
            glRotatef(np.degrees(roll), 0, 0, 1)
            draw_cylinder()
            glPopMatrix()

            pygame.display.flip()
            pygame.time.wait(10)  # Adjust sleep for smoother rotation
    finally:
        ser.close()

if __name__ == "__main__":
    main()
