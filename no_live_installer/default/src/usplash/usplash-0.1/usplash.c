/* USplash - a trivial framebuffer splashscreen application */
/* Copyright Matthew Garrett, 2005. Released under the terms of the GNU
   General Public License version 2.1 or later */

#include <string.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

#include "bogl.h"
#include "usplash.h"

#ifdef DEBUG
FILE* logfile;
#endif

int left_edge, top_edge;

#define BACKGROUND_COLOUR 0
#define PROGRESSBAR_COLOUR 1
#define TEXT_BACKGROUND 0
#define TEXT_FOREGROUND 2
#define RED 13

#define TEXT_X1 (left_edge + 136)
#define TEXT_X2 (left_edge + 504)
#define TEXT_Y1 (top_edge + 300)
#define TEXT_Y2 (top_edge + 450)
#define LINE_HEIGHT 15

#define PROGRESS_BAR (top_edge + 260)

#define TEXT_WIDTH TEXT_X2-TEXT_X1
#define TEXT_HEIGHT TEXT_Y2-TEXT_Y1

int pipe_fd;
char command[4096];

extern struct bogl_font font_helvB10;
struct bogl_pixmap* pixmap_usplash_artwork;

void draw_progress(int percentage);
void draw_text(char *string, int length);
void draw_image(struct bogl_pixmap *pixmap);
void event_loop();
void text_clear();
void switch_console(int screen);

int saved_vt=0;
int new_vt=0;
int timeout=15;

void cleanup() {

	if (saved_vt!=0) {
                struct vt_stat state;
                int fd;

                fd = open("/dev/console", O_RDWR);
                ioctl(fd,VT_GETSTATE,&state);
                close(fd);

                if (state.v_active == new_vt) {
                        // We're still on the console to which we switched,
                        // so switch back
                        switch_console(saved_vt);
                }
	}
}

int main (int argc, char** argv) {
	int err;
	void *handle;

#ifdef DEBUG
	logfile = fopen ("/dev/usplash_log","w+");
#endif

	if (argc>1) {
		if (strcmp(argv[1],"-c")==0) 
			switch_console(8);
	}

	chdir("/dev");

	if (mkfifo(USPLASH_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))
	{
		if (errno!=EEXIST) {
			perror("mkfifo");
			cleanup();
			exit(1);
		}
	}

	pipe_fd = open(USPLASH_FIFO,O_RDONLY|O_NONBLOCK);

	if (pipe_fd==-1) {
		perror("pipe open");
		cleanup();
		exit(2);
	}

	err=bogl_init();

	if (!err) {
		fprintf(stderr,"%d",err);
		cleanup();
		exit (2);
	}

	handle = dlopen("/usr/lib/usplash/usplash-artwork.so", RTLD_LAZY);
	if (!handle) {
		exit(1);
	}
	pixmap_usplash_artwork = (struct bogl_pixmap *)dlsym(handle, "pixmap_usplash_artwork");
	
	bogl_set_palette (0, 16, pixmap_usplash_artwork->palette);

	left_edge = (bogl_xres - 640) / 2;
	top_edge  = (bogl_yres - 480) / 2;

	draw_image(pixmap_usplash_artwork);

	text_clear();

	event_loop();

	bogl_done();

	cleanup();

	return 0;
}

void switch_console(int screen) {
	char vtname[10];
	int fd;
	struct vt_stat state;

	fd = open("/dev/console", O_RDWR);
	ioctl(fd,VT_GETSTATE,&state);
	saved_vt = state.v_active;
	close(fd);

	sprintf(vtname, "/dev/tty%d",screen);
	fd = open(vtname, O_RDWR);
	ioctl(fd,VT_ACTIVATE,screen);
        new_vt = screen;
	ioctl(fd,VT_WAITACTIVE,screen);
	close(fd);

	return;
}

void text_clear() {
	bogl_clear(TEXT_X1, TEXT_Y1, TEXT_X2, TEXT_Y2, TEXT_BACKGROUND);
}


void draw_progress(int percentage) {
	// Blank out the previous contents
	if (percentage < 0 || percentage > 100) {
		return;
	}
	bogl_clear(left_edge+220,PROGRESS_BAR,left_edge+420,PROGRESS_BAR+10,BACKGROUND_COLOUR);
	bogl_clear(left_edge+220,PROGRESS_BAR,(left_edge+220+2*percentage),PROGRESS_BAR+10,PROGRESSBAR_COLOUR);
	return;
}	

void draw_status(char *string, int colour) {
	bogl_clear (TEXT_X2-50, TEXT_Y2-LINE_HEIGHT, 
		    TEXT_X2, TEXT_Y2, TEXT_BACKGROUND);
	bogl_text (TEXT_X2-50, TEXT_Y2-LINE_HEIGHT, string, strlen(string), 
		   colour, TEXT_BACKGROUND, 0, &font_helvB10);
	return;
}

void draw_text(char *string, int length) {		
	/* Move the existing text up */
	bogl_move(TEXT_X1,TEXT_Y1+LINE_HEIGHT,TEXT_X1,TEXT_Y1,TEXT_X2-TEXT_X1,
		  TEXT_HEIGHT-LINE_HEIGHT);
	/* Blank out the previous bottom contents */
	bogl_clear(TEXT_X1, TEXT_Y2-LINE_HEIGHT, TEXT_X2, TEXT_Y2, 
		   TEXT_BACKGROUND);
	bogl_text (TEXT_X1, TEXT_Y2-LINE_HEIGHT, string, length, 8, 
		   TEXT_BACKGROUND, 0, &font_helvB10);
	return;
}

void draw_image(struct bogl_pixmap *pixmap) {
	int colour_map[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	bogl_clear(0, 0, bogl_xres, bogl_yres, BACKGROUND_COLOUR);	
	bogl_put (left_edge, top_edge, pixmap, colour_map);
}
	
int parse_command(char *string, int length) {
	char *command;
	char *origstring = string;
	int parsed=0;

#ifdef DEBUG
	fprintf (logfile, "%s\n", string);
	fflush (logfile);
#endif

	parsed = strlen(string)+1;
	
	if (strcmp(string,"QUIT")==0) {
		return 1;
	}	

	command = strtok(string," ");

	if (strcmp(command,"TEXT")==0) {
		char *line = strtok(NULL,"\0");
		int length = strlen(line);		
		while (length>50) {
			draw_text(line,50);
			line+=50;
			length-=50;
		}
		draw_text(line,length);
	} else if (strcmp(command,"STATUS")==0) {
		draw_status(strtok(NULL,"\0"),0);
	} else if (strcmp(command,"SUCCESS")==0) {
		draw_status(strtok(NULL,"\0"),TEXT_FOREGROUND);
	} else if (strcmp(command,"FAILURE")==0) {
		draw_status(strtok(NULL,"\0"),RED);
	} else if (strcmp(command,"PROGRESS")==0) {
		draw_progress(atoi(strtok(NULL,"\0")));
	} else if (strcmp(command,"CLEAR")==0) {
		text_clear();
	} else if (strcmp(command,"TIMEOUT")==0) {
		timeout=(atoi(strtok(NULL,"\0")));
	} else if (strcmp(command,"QUIT")==0) {
		return 1;
	}

	if (parsed < length) {
		parse_command(origstring+parsed,length-parsed);
	}

	return 0;
}

void event_loop() {
	int err;
	ssize_t length;
	fd_set descriptors;
	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&descriptors);
	FD_SET(pipe_fd,&descriptors);

	while (1) {
		err = select (pipe_fd+1,&descriptors,NULL,NULL,&tv);
		if (err==-1) {
			return;
		}
		
		if (err==0) {
			/* Timeout */
			return;
		}
		length = read(pipe_fd, command, 4095);

		if (length==0) {
			goto out;
		}

		command[length]=0;

		if(parse_command(command,length)) {
			return;
		}

	out:
		close(pipe_fd);
		pipe_fd = open(USPLASH_FIFO,O_RDONLY|O_NONBLOCK);

		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		FD_ZERO(&descriptors);
		FD_SET(pipe_fd,&descriptors);
	}
	return;
}
