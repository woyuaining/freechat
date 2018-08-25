#include "typing.h"
#include "c_string.h"
#include "search.h"

extern int state;
extern int g_bottom_line;
extern pthread_t *global_display_thread;
static struct user_list *current;

void remove_current_select(struct client *p)
{
	if (!current) {
		wprintw(p->gui.input, "freechat>> No user has been selected.");
		wrefresh(p->gui.input);
		usleep(500000);
		werase(p->gui.input);
		wrefresh(p->gui.input);
		return;
	}

	wprintw(p->gui.input, "freechat>> Unselect %s\n", current->nickname);
	current = NULL;
	usleep(500000);
	werase(p->gui.single_line);
	wrefresh(p->gui.single_line);
	werase(p->gui.input);
	wrefresh(p->gui.input);
}

void update_current_select(struct client *p)
{
	struct user_list *tmp = NULL;
	char name[MAXNAMESIZE] = {0x0};
	WINDOW *display = NULL;

	if (!p)
		return;
	display = p->gui.input;

	wprintw(display, "freechat>>(select user:)");
	wrefresh(display);

	wscanw(display, " %[^\n]s", name);
	while (strlen(name) > 200) {
		werase(display);
		wprintw(display, "freechat>> Message cannot more than 200 characters.\n");
		wprintw(display, "freechat>> (select user:)");
		wrefresh(display);
		wscanw(display, " %[^\n]s", name);
	}

	tmp = user_list_find(client.user, name);
	if (!tmp) {
		wprintw(display, "the user '%s' is not in current list,^A to show all list\n");
		wrefresh(display);
		usleep(500000);
		werase(display);
		return;
	}

	current = tmp;
	werase(p->gui.single_line);
	wprintw(p->gui.single_line, "freechat>> chating with %s\n", current->nickname);
	wrefresh(p->gui.single_line);
	werase(display);
}

void display_online_user(struct client *client)
{
	if (!client)
		return;
	int idx = 1;
	uint8_t show[MAXNAMESIZE + 4] = {0x0};

	struct user_list *tmp = client->user;

	draw_new(client->gui.display, "freechat>> Friends List:");
	while (tmp) {
		sprintf(&show[0], "#%d. %s", idx++, tmp->nickname);
		draw_new(client->gui.display, &show[0]);
		tmp = tmp->next;
	}
}

void* typing_func(void *arg) 
{

    char message_buffer[LENGHT_MESSAGE];
    char message_buffer_2[LENGHT_MESSAGE];
    //char confirm_file[LENGHT_MESSAGE];
    char filename[LENGHT_MESSAGE];
    char ch;
    int buffer_int;
    FILE *fp;
	struct client *p = (struct client *)arg;

    while (state == 0) {

        //Reset string for get new message
        strcpy(message_buffer, "");
        strcpy(message_buffer_2, "");

        wscanw(p->gui.input, " %[^\n]s", message_buffer);
        while (strlen(message_buffer) > 200) {
            werase(p->gui.input);
            draw_new(p->gui.display, "freechat>> Message cannot more than 200 characters.");
            wscanw(p->gui.input, " %[^\n]s", message_buffer);
        }

		switch (message_buffer[0]) {
		case 1:		/*^A show all online contact*/
			display_online_user(p);
			werase(p->gui.input);
			continue;
		case 4:		/*^D down page */
			draw_old_line(p->gui.display, 2, (int)get_display_height() - 1);
			werase(p->gui.input);
			continue;
		case 5:		/*^E*/
			state = 1;
			werase(p->gui.input);
			break;
		case 6:		/*^F send file*/
			werase(p->gui.input);
			continue;
		case 7:		/*^G*/
			draw_new(p->gui.display, "freechat>> ### THIS IS HELP! ###");
			draw_new(p->gui.display, "freechat>> \":q!\" to exit program.");
			draw_new(p->gui.display, "freechat>> \"/talkto [nickname]\" to choose contact.");
			draw_new(p->gui.display, "freechat>> \"/untalk\" to remove contact that we are talking.");
			draw_new(p->gui.display, "freechat>> \"/upload [file]\" to upload file to client that you are talking.");
			draw_new(p->gui.display, "freechat>> \"/watline\" to show number of latest line");
			draw_new(p->gui.display, "freechat>> \"/up [amount of line]\" to scroll screen up n lines.");
			draw_new(p->gui.display, "freechat>> \"/down [amount of line]\" to scroll screen down n lines.");
			draw_new(p->gui.display, "freechat>> \"/find [word]\" to find number of line that word was display.");
			draw_new(p->gui.display, "freechat>> \"/contact\" to show all user on server.");
			werase(p->gui.input);
			continue;
		case 8:		/*^W latest line*/
			werase(p->gui.input);
			continue;
		case 15:	/*^O unselect*/
			werase(p->gui.input);
			remove_current_select(p);
			continue;
		case 18:	/*^R	up one page*/
			draw_old_line(p->gui.display, 1, (int)get_display_height() - 1);
			werase(p->gui.input);
			continue;
		case 20:	/*^T find text*/
			werase(p->gui.input);
			continue;
		case 25:	/*^Y select a contact*/
			werase(p->gui.input);
			update_current_select(p);
			continue;
		default:
			break;
		}

        //Draw_new line to display message
        strcpy(message_buffer_2, "you>> ");
        strcat(message_buffer_2, message_buffer);
        draw_new(p->gui.display, message_buffer_2);

        //Check exit command
        if (strcmp(message_buffer, ":q!") == 0) {
            //set state to stop all function
            state = 1;
        }
        else if (message_buffer[0] == '/') {

            if (split_strcmp(0, 6, "/upload", 0, 6, message_buffer)){

                split_str(8, strlen(message_buffer), message_buffer, filename);
                sprintf(message_buffer, "3freechat>> Sending file to you: %s", filename);
                //send_data(message_buffer);

                sleep(1);

                draw_new(p->gui.display, "freechat>> Uploading...");

                fp = fopen(filename, "r");
                while( ( ch = fgetc(fp) ) != EOF ){

                    sprintf(message_buffer, "4%c", ch);

                    if(send_data(message_buffer) == 0)
                        draw_new(p->gui.display, "freechat>> Send failed");

                }
                fclose(fp);

                sleep(1);

                strcpy(message_buffer, "5");
                send_data(message_buffer);
                draw_new(p->gui.display, "freechat>> Done!");

            }
            else if (split_strcmp(0, 2, "/up", 0, 2, message_buffer)){

                split_str(4, strlen(message_buffer), message_buffer, message_buffer_2);
                buffer_int = atoi(message_buffer_2);
                draw_old_line(p->gui.display, 1, buffer_int);

            }
            else if (split_strcmp(0, 4, "/down", 0, 4, message_buffer)){

                split_str(6, strlen(message_buffer), message_buffer, message_buffer_2);
                buffer_int = atoi(message_buffer_2);
                draw_old_line(p->gui.display, 2, buffer_int);

            }
            else if (split_strcmp(0, 4, "/help", 0, 4, message_buffer)){

                draw_new(p->gui.display, "freechat>> ### THIS IS HELP! ###");
                draw_new(p->gui.display, "freechat>> \":q!\" to exit program.");
                draw_new(p->gui.display, "freechat>> \"/talkto [nickname]\" to choose contact.");
                draw_new(p->gui.display, "freechat>> \"/untalk\" to remove contact that we are talking.");
                draw_new(p->gui.display, "freechat>> \"/upload [file]\" to upload file to client that you are talking.");
                draw_new(p->gui.display, "freechat>> \"/watline\" to show number of latest line");
                draw_new(p->gui.display, "freechat>> \"/up [amount of line]\" to scroll screen up n lines.");
                draw_new(p->gui.display, "freechat>> \"/down [amount of line]\" to scroll screen down n lines.");
                draw_new(p->gui.display, "freechat>> \"/find [word]\" to find number of line that word was display.");
                draw_new(p->gui.display, "freechat>> \"/contact\" to show all user on server.");

            }
            else if (split_strcmp(0, 4, "/find", 0, 4, message_buffer)){

                split_str(6, strlen(message_buffer) - 1, message_buffer, message_buffer_2);
                search(message_buffer_2, p->gui.display);

            }
            else if (split_strcmp(0, 7, "/watline", 0, 7, message_buffer)){

                //bottom_line come from buffer_screen.h
                sprintf(message_buffer, "freechat>> v This is lines number %d. v", g_bottom_line);
                draw_new(p->gui.display, message_buffer);

            }
            else if (
                    split_strcmp(0, 6, "/talkto", 0, 6, message_buffer) ||
                    split_strcmp(0, 6, "/untalk", 0, 6, message_buffer) ||
                    split_strcmp(0, 7, "/contact", 0, 7, message_buffer)) {

                sprintf(message_buffer_2, "0%s", message_buffer);
                send_data(message_buffer_2);
            }
            else {
                draw_new(p->gui.display, "freechat>> Command not found.");
            }
        }
        else {

			/*
			wprintw(p->gui.display, "%d\n", message_buffer[0]);
			wrefresh(p->gui.display);
			*/
            //Set protocal to send packet
            sprintf(message_buffer_2, "0%s", message_buffer);
            if(send_data(message_buffer_2) == 0) {
				wprintw(p->gui.input, "%s\n", "freechat>> Send failed");
				wrefresh(p->gui.input);
			}

        }

		usleep(500000);
        werase(p->gui.input);

    }

    pthread_cancel(*global_display_thread);
    return 0;

}
