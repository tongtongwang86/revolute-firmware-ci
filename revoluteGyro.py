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

def draw_colored_cube():
    glBegin(GL_QUADS)

    # Front face (red)
    glColor3f(1.0, 0.0, 0.0)
    glVertex3f(-1.0, -1.0,  1.0)
    glVertex3f( 1.0, -1.0,  1.0)
    glVertex3f( 1.0,  1.0,  1.0)
    glVertex3f(-1.0,  1.0,  1.0)

    # Back face (green)
    glColor3f(0.0, 1.0, 0.0)
    glVertex3f(-1.0, -1.0, -1.0)
    glVertex3f(-1.0,  1.0, -1.0)
    glVertex3f( 1.0,  1.0, -1.0)
    glVertex3f( 1.0, -1.0, -1.0)

    # Top face (blue)
    glColor3f(0.0, 0.0, 1.0)
    glVertex3f(-1.0,  1.0, -1.0)
    glVertex3f(-1.0,  1.0,  1.0)
    glVertex3f( 1.0,  1.0,  1.0)
    glVertex3f( 1.0,  1.0, -1.0)

    # Bottom face (yellow)
    glColor3f(1.0, 1.0, 0.0)
    glVertex3f(-1.0, -1.0, -1.0)
    glVertex3f( 1.0, -1.0, -1.0)
    glVertex3f( 1.0, -1.0,  1.0)
    glVertex3f(-1.0, -1.0,  1.0)

    # Right face (magenta)
    glColor3f(1.0, 0.0, 1.0)
    glVertex3f( 1.0, -1.0, -1.0)
    glVertex3f( 1.0,  1.0, -1.0)
    glVertex3f( 1.0,  1.0,  1.0)
    glVertex3f( 1.0, -1.0,  1.0)

    # Left face (cyan)
    glColor3f(0.0, 1.0, 1.0)
    glVertex3f(-1.0, -1.0, -1.0)
    glVertex3f(-1.0, -1.0,  1.0)
    glVertex3f(-1.0,  1.0,  1.0)
    glVertex3f(-1.0,  1.0, -1.0)

    glEnd()

def main():
    pygame.init()
    display = (800, 600)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    gluPerspective(45, (display[0] / display[1]), 0.1, 50.0)
    glTranslatef(0.0, 0.0, -5)

    # Enable depth testing
    glEnable(GL_DEPTH_TEST)
    glDepthFunc(GL_LESS)

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
            glRotatef(np.degrees(-pitch), 1, 0, 0)  # Normal X-axis rotation (Pitch)
            glRotatef(np.degrees(-yaw), 0, 1, 0)    # Inverted Y-axis rotation (Yaw)
            glRotatef(np.degrees(-roll), 0, 0, 1)    # Inverted Z-axis rotation (Roll)
            draw_colored_cube()
            glPopMatrix()

            pygame.display.flip()
            pygame.time.wait(10)  # Adjust sleep for smoother rotation
    finally:
        ser.close()

if __name__ == "__main__":
    main()
