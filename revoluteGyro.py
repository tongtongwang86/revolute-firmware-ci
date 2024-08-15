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

# Function to parse the serial data for quaternions
def parse_data(line):
    try:
        data = line.decode('utf-8').strip().split(',')
        r = float(data[0].split(':')[1])
        i = float(data[1].split(':')[1])
        j = float(data[2].split(':')[1])
        k = float(data[3].split(':')[1])
        return r, i, j, k
    except:
        return None, None, None, None

def swap_quaternion_axes(q):
    """ Swap yaw with pitch, roll with yaw, and pitch with roll. """
    r, i, j, k = q
    return r, j, k, i  # Swap roll (i) with yaw (k), yaw (k) with pitch (j), and pitch (j) with roll (i)

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

    # Initialize the 4x4 identity matrix
    rotation_matrix = np.identity(4)

    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    ser.close()
                    quit()

            if ser.in_waiting > 0:
                line = ser.readline()
                r, i, j, k = parse_data(line)
                if r is not None and i is not None and j is not None and k is not None:
                    # Swap yaw with pitch, roll with yaw, and pitch with roll
                    r, i, j, k = swap_quaternion_axes((r, i, j, k))

                    # Create a rotation object using the swapped quaternion
                    rotation = R.from_quat([i, j, k, r])
                    # Convert quaternion to 3x3 rotation matrix
                    rotation_matrix_3x3 = rotation.as_matrix()

                    # Expand to 4x4 matrix
                    rotation_matrix[:3, :3] = rotation_matrix_3x3
                    rotation_matrix[3, :] = [0, 0, 0, 1]

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
            glPushMatrix()
            # Apply the 4x4 rotation matrix
            glMultMatrixf(rotation_matrix.T)  # Transpose because OpenGL expects column-major order
            draw_colored_cube()
            glPopMatrix()

            pygame.display.flip()
            pygame.time.wait(10)  # Adjust sleep for smoother rotation
    finally:
        ser.close()

if __name__ == "__main__":
    main()
