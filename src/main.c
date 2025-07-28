#include <zephyr/kernel.h>
#include "hog2.h"

void main(void)
{
    sensor_init();
	hog_init();
	// hog_button_loop() now runs in its own thread
}
