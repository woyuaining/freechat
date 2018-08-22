#ifndef _CLIENT_H_
#define _CLIENT_H_
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "screen.h"

#define LENGHT_USERNAME 255
#define LENGHT_MESSAGE 255

struct gui {
	WINDOW *display;
	WINDOW *input;
	uint8_t input_line;
};

struct info {
};

struct client {
	struct gui gui;
	struct info info;
};


#endif
