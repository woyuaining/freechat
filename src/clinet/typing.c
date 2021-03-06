#include "typing.h"
#include "search.h"
#include "gui.h"
#include "fttp.h"
#include "user_id.h"
#include "config.h"
#include "cmdhis.h"

#define CMD_NUM 12
extern int state;
extern int g_bottom_line;
extern pthread_t *global_display_thread;
static struct user_list *current;
/*
 * the history of the command sarted with ':'.
 */
static command *current_cmd = NULL;
/*
 * this is the command typing flag
 */
bool writting = false;
/*
 * the advanced command table
 */
char cmd_table[CMD_NUM][MAXCMDLENGTH] = {
	":login", ":logout", ":info", ":edit", ":refresh", ":log", ":lock",
	":locked", ":unlock", ":server", ":clear", ":register"
};

/*
 *Search history command upwards.if command found,
 *copy it to cmd, and set it's lenngth to len.
 */
void cmd_up(WINDOW *win, char *cmd, int *len)
{
	if (current_cmd == NULL) {
		return;
	}
	memset(cmd, 0x0, LENGTH_MESSAGE);
	*len = 0;

	if (!writting) {
		if (current_cmd->prev)
			current_cmd = current_cmd->prev;
	} else {
		writting = false;
	}
	memcpy(cmd, current_cmd->cmd.cmd, strlen(current_cmd->cmd.cmd));
	*len = strlen(current_cmd->cmd.cmd);
	werase(win);
	wprintw(win, "%s\n", current_cmd->cmd.cmd);
	wrefresh(win);
}

/*
 *Search history command downwards.if command found,
 *copy it to cmd, and set it's lenngth to len.
 */
void cmd_down(WINDOW *win, char *cmd, int *len)
{
	if (current_cmd == NULL)
		return;

	memset(cmd, 0x0, LENGTH_MESSAGE);
	*len = 0;
	if (current_cmd->next) 
		current_cmd = current_cmd->next;

	memcpy(cmd, current_cmd->cmd.cmd, strlen(current_cmd->cmd.cmd));
	*len = strlen(current_cmd->cmd.cmd);
	werase(win);
	wprintw(win, "%s\n", current_cmd->cmd.cmd);
	wrefresh(win);
}

/*
 *use the tab key to complete the command already in history.
 *if found the command, copy the cmd to cmd, and set it's length
 *to len.
 */
int cmd_tab(int start, WINDOW *win, char *cmd, int *len)
{
	int i = 0;

	if (start >= CMD_NUM)
		return start;

	for (i = start; i < CMD_NUM; i++) {
		if (!strncmp(cmd, cmd_table[i], strlen(cmd))) {
			memset(cmd, 0x0, LENGTH_MESSAGE);
			memcpy(cmd, cmd_table[i], strlen(cmd_table[i]));
			*len = strlen(cmd_table[i]);
			werase(win);
			wprintw(win, "%s\n", cmd);
			wrefresh(win);
			return i;
		}
	}
	return start;
}

/*
 * return the bytes number of one 
 * Chinese character
 */
static int get_cn_len(void)
{
	char *s = "测试";
	return strlen(s)/2;
}

/*
 * give a hint and wait for user's confirmation.
 */
void wait_for_confirm(WINDOW *win)
{
	int len;
	char *notes = "Press any key to confirm this notes!";

	len = strlen(notes);

	mvwaddstr(win, 3, 1, notes);
	wattron(win, A_REVERSE);
	mvwaddstr(win, 3, len + 2, "OK");
	wattroff(win, A_REVERSE);
	wgetch(win);
}

/*
 * get the confirm options from user input,options are:
 * y indicate yes and return true
 * n inicate no and return false.
 */
bool get_user_option(WINDOW *win, char *tips)
{
	bool status = false;

	if (!win || !tips)
		return status;

	mvwaddstr(win, 3, 1, tips);
	wattron(win, A_REVERSE);
	mvwaddstr(win, 3, strlen(tips) + 2, "yes/no");
	wattroff(win, A_REVERSE);
	
	while (1) {
		int ch = wgetch(win);
		if (ch == 'y') {
			status = true;
			break;
		} else if (ch == 'n') {
			status = false;
			break;
		}
	}

	return status;
}

/*
 * judge weather the user is currently selected user.
 */
bool user_is_current(struct user_list *user)
{
	return (user == current);
}

/*
 * unselect the currently selected user.
 */
void remove_current_select(struct client *p, bool confirm)
{
	if (!confirm) {
		current = NULL;
		werase(p->gui.single_line);
		wbkgd(p->gui.single_line, COLOR_PAIR(4));
		wrefresh(p->gui.single_line);
		return;
	}
	if (!current) {
		wprintw(p->gui.input, "freechat>> No user has been selected.");
		wrefresh(p->gui.input);
		wait_for_confirm(p->gui.input);
		werase(p->gui.input);
		wrefresh(p->gui.input);
		return;
	}

	wprintw(p->gui.input, "freechat>> Unselect %s\n", current->user->name);
	wrefresh(p->gui.input);
	if (get_user_option(p->gui.input, 
			"Confirm to unselect the current user? ")) {
		current = NULL;
		werase(p->gui.single_line);
		wbkgd(p->gui.single_line, COLOR_PAIR(4));
		wrefresh(p->gui.single_line);
	}
	werase(p->gui.input);
	wrefresh(p->gui.input);
}

/*
 * update the currently selected user.
 */
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
		wprintw(display, 
				"the user '%s' is not in current list,^A to show all list\n",
				name);
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}

	current = tmp;
	werase(p->gui.single_line);
    wbkgd(p->gui.single_line, COLOR_PAIR(3));
	wprintw(p->gui.single_line, "freechat>> chating with %s\n", current->user->name);
	wrefresh(p->gui.single_line);
	werase(display);
}

/*
 * handle command for send file.
 */
void send_file(struct client *p)
{
	struct user_list *tmp = NULL;
	char name[MAXNAMESIZE] = {0x0};
	WINDOW *display = NULL;

	if (!p)
		return;

	display = p->gui.input;

	tmp = current;
	if (tmp == NULL) {
		werase(display);
		wprintw(display, "freechat>> Please select contact first!\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}


	wprintw(display, "freechat>>(file path:)");
	wrefresh(display);

	wscanw(display, " %[^\n]s", name);
	while (strlen(name) > 200) {
		werase(display);
		wprintw(display, "freechat>> Message cannot more than 200 characters.\n");
		wprintw(display, "freechat>> (file path:)");
		wrefresh(display);
		wscanw(display, " %[^\n]s", name);
	}

	werase(display);
	wprintw(display, "freechat>> Sending file %s\n", name);
	wrefresh(display);
	wait_for_confirm(display);
	werase(display);
}

/*
 * handle command for search key word.
 */
void search_text(struct client *p)
{
	char text[MAXNAMESIZE] = {0x0};
	WINDOW *display = NULL;

	if (!p)
		return;
	display = p->gui.input;

	wprintw(display, "freechat>>(search text:)");
	wrefresh(display);

	wscanw(display, " %[^\n]s", text);
	while (strlen(text) > 200) {
		werase(display);
		wprintw(display, "freechat>> Message cannot more than 200 characters.\n");
		wprintw(display, "freechat>> (search text:)");
		wrefresh(display);
		wscanw(display, " %[^\n]s", text);
	}

	if (strlen(text) == 0) {
		werase(display);
		return;
	}

	if (search(text, p->gui.display)) {
		wprintw(display, "the text '%s' you are searching not fount in current reccord.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);
	wrefresh(display);
}

/*
 * display all the online user or the message disabled user
 * flag is true indicate all of the online user and flag is
 * false indicate message disabled user.
 */
void display_online_user(struct client *client, bool flag)
{
	if (!client)
		return;
	int idx = 1;
	uint8_t show[MAXNAMESIZE + 20] = {0x0};
	char *notes = NULL;

	struct user_list *tmp = client->user;

	draw_new(client->gui.display, "freechat>> Friends List:");
	while (tmp) {
		if (idx == 1)
			notes = "(myself)";
		else
			notes = (tmp->user->sex == 2) ? "(room)" : "(person)";
		if (flag) {
			sprintf((char *)&show[0], "#%d. %s%s", idx, tmp->user->name, notes);
			draw_new(client->gui.display, (char *)&show[0]);
			memset(&show[0], 0x0, sizeof(show));
		} else {
			if (tmp->enable == flag) {
				sprintf((char *)&show[0], "#%d. %s%s", idx++, tmp->user->name, notes);
				draw_new(client->gui.display, (char *)&show[0]);
				memset(&show[0], 0x0, sizeof(show));
			}
		}
		tmp = tmp->next;
		idx++;
	}
}

/*
 * judge weather the port is valid
 */
bool port_is_valid(uint16_t port)
{
	return ((port > 1024) && (port < 65535));
}

/*
 * judge weather the IP address is valid
 */
bool ip_is_valid(char *ip)
{
	int a,b,c,d;
	bool status = false;
	char temp[16] = {0x0};

	if (!ip)
		return status;

	if (sscanf(ip, "%d.%d.%d.%d ", &a, &b, &c, &d) == 4
			&& a >= 0 && a <= 255 && b >= 0 && b <= 255
			&& c >= 0 && c <= 255 && d >= 0 && d <= 255) {
		sprintf(temp, "%d.%d.%d.%d", a, b, c, d);
		if (strcmp(temp, ip) == 0) {
			status = true;
		}
	}

	return status;
}

/*
 * judge weather the string of birthday is valid
 */
bool birthday_is_valid(char *birthday)
{
	int year, mon, day;
	bool status = false;
	char temp[12] = {0x0};

	if (!birthday)
		return status;

	if (sscanf(birthday, "%d-%d-%d", &year, &mon, &day) == 3
			&& year >= 1970 && year <= 2020 && mon >= 1 && mon <= 12
			&& day >= 1 && day <= 31) {
		sprintf(temp, "%04d-%02d-%02d", year, mon, day);
		if (strcmp(temp, birthday) == 0) {
			status = true;
		}
	}

	return status;
}

void connect_server(struct client *p)
{
	char ip[16] = {0};
	uint16_t port = 0;
	WINDOW *display = NULL;

	if (!p)
		return;
	display = p->gui.input;

	/*get server ip address*/
	wprintw(display, "freechat>>[server ip(null is default):]");
	wrefresh(display);

	wscanw(display, " %[^\n]s", &ip[0]);
	if ((strlen(ip) > 16) || (!(ip_is_valid(&ip[0])) && strlen(ip) > 0)) {
		werase(display);
		wprintw(display, "freechat>> Invalid IP address.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);
	
	/*get server port*/
	wprintw(display, "freechat>>[server port(null is default):]");
	wrefresh(display);

	wscanw(display, "%u", &port);
	if (!(port_is_valid(port)) && port != 0) {
		werase(display);
		wprintw(display, "freechat>> Invalid port.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);

	/*
	 * if not input, set the default server ip and port
	 */
	if (strlen(ip) == 0)
		memcpy(ip, p->info.serverip, 16);
	if (port == 0)
		port = p->info.serverport;

	/*
	 * here connect to the server.if connect successfully,
	 * we should add the server to current user list.
	 */
	/*debug*/
	draw_new(p->gui.display, ip);
	char tmp[12] = {0};
	sprintf(tmp, "%u", port);
	draw_new(p->gui.display, tmp);
}

/*
 * display user details
 */
void contact_info(struct client *p)
{
	struct user_list *tmp = NULL;
	char name[MAXNAMESIZE] = {0x0};
	WINDOW *display = NULL;
	char *type[6] = {0};

	if (!p)
		return;
	display = p->gui.input;

	wprintw(display, "freechat>>(select user:)");
	wrefresh(display);

	wscanw(display, " %[^\n]s", name);
	if (strlen(name) > MAXNAMESIZE) {
		werase(display);
		wprintw(display, "freechat>> Invalid name.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}

	tmp = user_list_find(p->user, name);
	if (!tmp) {
		wprintw(display, 
				"the user '%s' is not in current list,^A to show all list\n",
				name);
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);

	/* display the user info on display */
	display = p->gui.display;

	if (tmp->user->sex == 0)
		memcpy(type, "boy", strlen("boy"));
	else if (tmp->user->sex == 1)
		memcpy(type, "girl", strlen("girl"));
	else
		memcpy(type, "room", strlen("room"));

	wprintw(display, "\nfreechat>> User Info:\n");
	wprintw(display, "freechat>> name:%s\n", tmp->user->name);
	wprintw(display, "freechat>> sex:%s\n", type);
	wprintw(display, "freechat>> birthday:%s\n", tmp->user->birthday);
	wprintw(display, "freechat>> signature:%s\n\n", tmp->user->signature);
	wrefresh(display);
}

void edit_myself(struct client *p)
{
	char name[FTTPMAXNAMESIZE] = {0x0};
	char signature[FTTPMAXSIGNATURESIZE] = {0x0};
	char birthday[FTTPMAXBIRTHDAYSIZE] = {0x0};
	uint8_t sex = 0;
	WINDOW *display = NULL;
	bool changed = true;
	bool update = false;

	if (!p)
		return;
	display = p->gui.input;

	/*get name*/
	wprintw(display, "freechat>>[name(null is default):]");
	wrefresh(display);

	wscanw(display, " %[^\n]s", &name[0]);
	if (strlen(name) == 0) {
		changed = false;
	} else if (strlen(name) >= FTTPMAXNAMESIZE) {
		werase(display);
		wprintw(display, "freechat>> The name is too long.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		goto out;
	}
	werase(display);
	if (changed) {
		update = true;
		memcpy(p->user->user->name, name, FTTPMAXNAMESIZE);
		p->user->user->name[strlen(name)] = '\0';
		update_name_config((char *)p->user->user->name, p->cfg_path);
	}
	
	/*get signature*/
	wprintw(display, "freechat>>[signature(null is default):]");
	wrefresh(display);

	wscanw(display, " %[^\n]s", &signature[0]);
	if (strlen(signature) == 0) {
		changed = false;
	} else if (strlen(signature) >= FTTPMAXSIGNATURESIZE) {
		werase(display);
		wprintw(display, "freechat>> The signature is too long.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		goto out;
	}
	werase(display);
	if (changed) {
		update = true;
		memcpy(p->user->user->signature, signature, FTTPMAXNAMESIZE);
		p->user->user->signature[strlen(signature)] = '\0';
		update_signature_config((char *)p->user->user->signature, p->cfg_path);
	}

	/*get birthday*/
	wprintw(display, "freechat>>[birthday format:year-mon-day(null is default):]");
	wrefresh(display);

	wscanw(display, " %[^\n]s", &birthday[0]);
	if (strlen(birthday) == 0) {
		changed = false;
	} else if ((strlen(birthday) >= FTTPMAXBIRTHDAYSIZE)
		   	|| (!birthday_is_valid(birthday))) {
		werase(display);
		wprintw(display, "freechat>> The birthday is invalid.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		goto out;
	}
	werase(display);
	if (changed) {
		update = true;
		memcpy(p->user->user->birthday, birthday, FTTPMAXNAMESIZE);
		p->user->user->birthday[strlen(birthday)] = '\0';
		update_birthday_config((char *)p->user->user->birthday, p->cfg_path);
	}
	/*get server port*/
	wprintw(display, "freechat>>[sex:0 for boy, 1 for girl(null is default):]");
	wrefresh(display);

	wscanw(display, "%d", &sex);
	if (sex == 0) {
		changed = false;
	} else if ((sex != 0) && (sex != 1)) {
		werase(display);
		wprintw(display, "freechat>> Invalid sex.\n");
		wprintw(display, "freechat>> input 0 for boy, 1 for girl.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		goto out;
	}
	werase(display);
	if (changed) {
		update = true;
		p->user->user->sex = sex;
		update_sex_config((uint8_t)p->user->user->sex, p->cfg_path);
	}

out:
	if (update) {
		struct fttp_addr dest;
		fttp_get_broadcast_address(&dest);
		send_user_rsp(&dest, p->user->user);

		/* tips */
		werase(display);
		wprintw(display, "Set your info successed!\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		wrefresh(display);
	}
}

void refresh_list(struct client *p)
{
	struct user_list *head = p->user;
	struct user_list *tmp = head->next;

	/*if current is selected, unselect it*/
	if (current) {
		current = NULL;
		werase(p->gui.single_line);
		wbkgd(p->gui.single_line, COLOR_PAIR(4));
		wrefresh(p->gui.single_line);
	}

	/*delete all user except myself and chat room*/
	while (tmp) {
		head->next = tmp->next;

		fttp_user_id_del(tmp->user->id);
		free(tmp->user);
		tmp->user = NULL;

		free(tmp);
		tmp = head->next;
	}

	/*send user request to broad cast*/
	send_user_req();
}

void save_log(struct client *p)
{
}

void set_msg_enable_flag(struct client *p, bool flag)
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
	if (strlen(name) > MAXNAMESIZE) {
		werase(display);
		wprintw(display, "freechat>> Invalid name.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}

	tmp = user_list_find(p->user, name);
	if (!tmp) {
		wprintw(display, 
				"the user '%s' is not in current list,^A to show all list\n",
				name);
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);

	/*clear the message enable flag*/
	tmp->enable = flag;
}

void disabled_list(struct client *p)
{
	display_online_user(p, false);
}

void set_server(struct client *p)
{
	char ip[16] = {0};
	uint16_t port = 0;
	WINDOW *display = NULL;
	bool ip_changed = true;
	bool port_changed = true;

	if (!p)
		return;
	display = p->gui.input;

	/*get server ip address*/
	wprintw(display, "freechat>>[server ip(null is default):]");
	wrefresh(display);

	wscanw(display, " %[^\n]s", &ip[0]);
	if (strlen(ip) == 0) {
		ip_changed = false;
	} else if ((strlen(ip) > 16) || !(ip_is_valid(&ip[0]))) {
		werase(display);
		wprintw(display, "freechat>> Invalid IP address.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);
	if (ip_changed) {
		memcpy(p->info.serverip, ip, strlen(ip));
		p->info.serverip[strlen(ip)] = '\0';
	}
	
	/*get server port*/
	wprintw(display, "freechat>>[server port(null is default):]");
	wrefresh(display);

	wscanw(display, "%u", &port);
	if (port == 0) {
		port_changed = false;
	} else if (!(port_is_valid(port))) {
		werase(display);
		wprintw(display, "freechat>> Invalid port.\n");
		wrefresh(display);
		wait_for_confirm(display);
		werase(display);
		return;
	}
	werase(display);
	if (port_changed)
		p->info.serverport = port;

	/*
	 * here set the server ip and port to configure file.
	 */
	update_serverip_config((char *)p->info.serverip, p->cfg_path);
	update_serverport_config(p->info.serverport, p->cfg_path);

	/*
	 * tips
	 */
	werase(display);
	wprintw(display, "Set server info successed!\n");
	wrefresh(display);
	wait_for_confirm(display);
	werase(display);
}

void clean_display(WINDOW *display)
{
	werase(display);
	wrefresh(display);
}

/*
 * This function handles the advanced options.
 */
void advanced_options(char *cmd, struct client *p,
		command **cmd_head)
{
	if (!cmd || !p)
		return;
	/*
	 * connect to server
	 * show a contact's detail info
	 * edit my info
	 * fresh contact list
	 * save history log
	 * disable contact's message
	 * show disabled list
	 * enable contact
	 * set server info
	 * clear the display
	 */
	if (strcasecmp(cmd, ":login") == 0) {
		werase(p->gui.input);
		connect_server(p);
	} else if (strcasecmp(cmd, ":logout") == 0) {
		werase(p->gui.input);
		/**/
	} else if (strcasecmp(cmd, ":info") == 0) {
		werase(p->gui.input);
		contact_info(p);
	} else if (strcasecmp(cmd, ":edit") == 0) {
		werase(p->gui.input);
		edit_myself(p);
	} else if (strcasecmp(cmd, ":refresh") == 0) {
		werase(p->gui.input);
		refresh_list(p);
	} else if (strcasecmp(cmd, ":log") == 0) {
		werase(p->gui.input);
		save_log(p);
	} else if (strcasecmp(cmd, ":lock") == 0) {
		werase(p->gui.input);
		set_msg_enable_flag(p, false);
	} else if (strcasecmp(cmd, ":locked") == 0) {
		werase(p->gui.input);
		disabled_list(p);
	} else if (strcasecmp(cmd, ":unlock") == 0) {
		werase(p->gui.input);
		set_msg_enable_flag(p, true);
	} else if (strcasecmp(cmd, ":server") == 0) {
		werase(p->gui.input);
		set_server(p);
	} else if (strcasecmp(cmd, ":clear") == 0) {
		werase(p->gui.input);
		clean_display(p->gui.display);
	} else if (strcasecmp(cmd, ":register") == 0) {
		werase(p->gui.input);
		/**/
	} else {
		wprintw(p->gui.input, "freechat>> Invalid command!\n");
		wrefresh(p->gui.input);
		wait_for_confirm(p->gui.input);
		werase(p->gui.input);
		return;
	}

	if (cmd_link_add(cmd_head, cmd))
		current_cmd = cmd_link_get_current();
}

void* typing_func(void *arg) 
{
	int cn_len = 0; /*bytes number of one Chinese character*/
	int cmd = 0;
	struct client *p = (struct client *)arg;
    char message_buffer[LENGTH_MESSAGE];
    char message_buffer_2[LENGTH_MESSAGE];
	int typing_len = 0;
	command *head = NULL;

	/*enable keypad*/
	keypad(p->gui.input, TRUE);
	cn_len = get_cn_len();

	/*init the link head of cmd history*/
	cmd_link_init(head);
	int pos = 0;

	while (state == 0) {
        /*clear the message buffer*/
        strcpy(message_buffer, "");
        strcpy(message_buffer_2, "");
		typing_len = 0;

		cmd = wgetch(p->gui.input);
		if (cmd == '\n')
			continue;
		switch (cmd) {
		case 1:		/*^A show all online contact*/
			display_online_user(p, true);
			werase(p->gui.input);
			continue;
		case 4:		/*^D down page */
			screen_scroll(p->gui.display, 1, (int)get_display_height() - 2);
			werase(p->gui.input);
			continue;
		case 5:		/*^E*/
			state = 1;
			werase(p->gui.input);
			continue;
		case 6:		/*^F send file*/
			werase(p->gui.input);
			send_file(p);
			continue;
		case 7:		/*^G*/
			werase(p->gui.input);
			show_base_info(p->gui.display);
			continue;
		case 15:	/*^O unselect*/
			werase(p->gui.input);
			remove_current_select(p, true);
			continue;
		case 18:	/*^R	up one page*/
			werase(p->gui.input);
			screen_scroll(p->gui.display, 0, (int)get_display_height() - 2);
			continue;
		case 20:	/*^T find text*/
			werase(p->gui.input);
			search_text(p);
			continue;
		case 23:		/*^W latest line*/
			werase(p->gui.input);
			draw_new(p->gui.display, NULL);
			continue;
		case 25:	/*^Y select a contact*/
			werase(p->gui.input);
			update_current_select(p);
			continue;
		case KEY_UP:
			screen_scroll(p->gui.display, 0, 1);
			continue;
		case KEY_DOWN:
			screen_scroll(p->gui.display, 1, 1);
			continue;
		default:
			message_buffer[0] = (char)cmd;
			typing_len = 1;

			while (1) {
				cmd = wgetch(p->gui.input);
				if (cmd == '\n') {
					/*end with typing*/
					message_buffer[typing_len] = '\0';
					break;
				} else if (cmd == 263 || cmd == 8 || cmd == 127) {
					/*handle back space*/
					
					/*here for Chinese delete handle*/
					if ((message_buffer[typing_len - 1] & 0x80)
							|| (message_buffer[typing_len - 1] >= 0x80)) {
						typing_len -= cn_len; /*utf-8 linux mode*/
					} else {
						typing_len--;
					}

					message_buffer[typing_len] = '\0';
					werase(p->gui.input);

					/*all input char is deleted*/
					if (typing_len <= 0)
						break;
					wprintw(p->gui.input, "%s", message_buffer);
					wrefresh(p->gui.input);
				} else if (cmd == KEY_UP) {
					if (message_buffer[0] == ':')
						cmd_up(p->gui.input, message_buffer, &typing_len);
					continue;
				} else if (cmd == KEY_DOWN) {
					if (message_buffer[0] == ':')
						cmd_down(p->gui.input, message_buffer, &typing_len);
					continue;
				} else if (cmd == 9 && message_buffer[0] == ':') {
					message_buffer[typing_len] = '\0';
					pos = cmd_tab(0, p->gui.input, message_buffer, &typing_len);
					if (pos >= CMD_NUM)
						pos = 0;
					continue;
				} else {
					writting = true;
					message_buffer[typing_len++] = cmd;
				}
			}

			if (strlen(message_buffer) > 200) {
				werase(p->gui.input);
				wprintw(p->gui.input, "%s\n", "freechat>> Message cannot more than 200 characters.");
				wrefresh(p->gui.input);
				wait_for_confirm(p->gui.input);
				werase(p->gui.input);
				continue;
			}
			/*no input or input is all be deleted by back space*/
			if (typing_len == 0)
				continue;

			if (message_buffer[0] == ':') {
				advanced_options(&message_buffer[0], p, &head);
				continue;
			}

			if (!current) {
				wprintw(p->gui.input, "%s\n", "freechat>> select a user to send or input correct cmd");
				wrefresh(p->gui.input);
				wait_for_confirm(p->gui.input);
				werase(p->gui.input);
				continue;
			}
			break;
		}

		/*if the type(sex) is not room*/
		if(send_text_udp((uint8_t *)message_buffer, strlen(message_buffer), 
					current->user->id, p->user->user->id) <= 0) {
			wprintw(p->gui.input, "%s\n", "freechat>> Send failed");
			wrefresh(p->gui.input);
			wait_for_confirm(p->gui.input);
			werase(p->gui.input);
			continue;
		}
        //Draw_new line to display message
        strcpy(message_buffer_2, "Me>> ");
        strcat(message_buffer_2, message_buffer);
        draw_new(p->gui.display, message_buffer_2);
		werase(p->gui.input);
	}

	cmd_link_destroy(&head);
    pthread_cancel(*global_display_thread);
	return NULL;
}

