#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>

void button_init(void);

/* Detach the interrupt and cancel any pending click/hold work. Called before
 * the button is re-armed as a System OFF wake source. */
void button_uninit(void);

#endif // BUTTON_H
