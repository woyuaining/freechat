#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <langinfo.h>
#include <ctype.h>
#include "client.h"
#include "gui.h"
#include "search.h"
#include "display.h"
#include "typing.h"
#include "config.h"


pthread_t *global_typing_thread, *global_display_thread;
int state = 0, display_height;
char *version = "FreeChat V1.0.1";

struct client client;

static void print_usage(void)
{
	printf("%s\n", version);
	printf("Author:shadchen320@163.com\n");
	printf("Usage:\n");
	printf(" freechat [option [arg]]\n");
	printf(" options:\n"
			"-h,	show this help info\n"
			"-c,	specified the configure path\n"
			"-v,	show the version info\n");
	return;
}

void disable_signals(void)
{
	struct termios term = {0};

	tcgetattr(0, &term);
	term.c_lflag &= ~ISIG;
	tcsetattr(0, TCSANOW, &term);
}

void disable_extended_io(void)
{
	struct termios term = {0};

	tcgetattr(0, &term);
	term.c_lflag &= ~IEXTEN;
	term.c_oflag &= ~OPOST;
	tcsetattr(0, TCSANOW, &term);
}

void terminal_init(void)
{
	raw();
	nonl();
	noecho();
}


void process_exit(int signal_num)
{
    pthread_cancel(*global_display_thread);
    pthread_cancel(*global_typing_thread);
	cleanup_gui();
	exit(0);
}

int main(int argc , char *argv[]) 
{
	struct sigaction sig;
	struct client *p;
	int opt;
	char *optstring = "hvc:";
	bool cmd_path = false;
	pthread_t typing_thread, display_thread;

	p = &client;

	/* signal handler */
	sig.sa_handler = process_exit;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = 0;
	sigaction(SIGINT, &sig, NULL);

	terminal_init();

	if (argc >= 2) {
		while ((opt = getopt(argc, argv, optstring)) != -1) {
			switch (opt) {
			case 'h':
				print_usage();
				return 0;
			case 'v':
				printf("%s\n", version);
				return 0;
			case 'c':
				cmd_path = true;
				break;
			default:
				print_usage();
				return 0;
			}
		}
	}
	/* parser configure file */
	if (cmd_path) {
		if (config_parser(argv[2], &client)) {
			print_usage();
			fprintf(stderr, "init configure failed\n");
			exit(1);
		}
	} else {
		if (config_parser(CONFIGFILEPATH, &client)) {
			print_usage();
			fprintf(stderr, "init configure failed\n");
			exit(1);
		}
	}

	/* init graphyics */
	p->gui.input_line = 4;
	if (init_gui(p->gui.input_line)) {
		fprintf(stderr, "init gui failed!\n");
		exit(1);
	}
	p->gui.display = get_display();
	p->gui.input = get_typing();
	p->gui.single_line = get_single_line();
	show_base_info(p->gui.display);

	/* create two thread one for typing another for msg reciver and handle*/
    global_typing_thread = &typing_thread;
    global_display_thread = &display_thread;
    pthread_create(&typing_thread, NULL, (void *)typing_func, (void *)p);
    pthread_create(&display_thread, NULL, (void *)display_func, (void *)p);

    pthread_join( typing_thread, NULL);
    pthread_join( display_thread, NULL);

	cleanup_gui();

    return 0;
}
