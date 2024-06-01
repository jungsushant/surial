/***includes***/

#include <asm-generic/ioctls.h>
#include <termios.h>
#include <unistd.h>
#include <termio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/ioctl.h>


/*** defines***/

#define CTRL_KEY(k) ((k) & 0x1f)


/***data***/

struct editorConfig{
  int screenrows;
  int screencolumns;

struct termios orig_termios;
};

struct editorConfig E;





/***terminal***/

void die(const char *s) {
    write(STDOUT_FILENO,"\x1b[2J", 4);
  write(STDOUT_FILENO,"\x1b[H", 4);
  perror(s);
  exit(1);
}
void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");

}

char editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1))!=1) {
        if (nread == -1 ) die("read");
    }
    return c;
}

int getWindowSize(int *rows, int *columns){
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    editorReadKey();
    return -1;

  } else {
  *columns = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
  }
}
/***output***/


// This function is used to draw tildes in the first of every row
void editorDrawRows(){
  int y;
  for(y=0; y<E.screenrows;y++){
write(STDOUT_FILENO, "~\r\n", 3);
  }
}

// This function is used to clear the screen
// /x1b is a escape sequence
void editorRefreshScreen(){
  write(STDOUT_FILENO,"\x1b[2J", 4);
  write(STDOUT_FILENO,"\x1b[H", 4);
  editorDrawRows();
  write(STDOUT_FILENO,"\x1b[H",4);

}





/***input***/
void editorProcessKeyPress(){
    char c = editorReadKey();


    switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO,"\x1b[2J", 4);
      write(STDOUT_FILENO,"\x1b[H", 4);
        exit(0);
        break;
    }
}

void enableRawMode(){
   if( tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;


    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG );
    raw.c_iflag &= ~(BRKINT | INPCK| ISTRIP | IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    if(tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw)== -1) die("tccsetattr");

}

/***init***/

void initEditor(){

  if(getWindowSize(&E.screenrows, &E.screencolumns)== -1) die("getWindowSize");
}
int main() {
  initEditor();
  enableRawMode();
  while (1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}