/***includes***/

#include <asm-generic/ioctls.h>
#include <termios.h>
#include <unistd.h>
#include <termio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>


/*** defines***/

#define SURIAL_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)


/***data***/

struct editorConfig{
  int cx, cy;
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
int getCursorPosition(int *rows , int *columns){
  char buf[32];
  unsigned int i = 0;


  if( write(STDOUT_FILENO, "\x1b[6n", 4) !=4 ) return -1;


  while(i< sizeof(buf) -1 ){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';
  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  
  
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, columns) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *columns){
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, columns);

  } else {
  *columns = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
  }
}

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}



void abAppend(struct abuf *ab, const char *s, int len){
  char *new = realloc(ab->b, ab->len +len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;


}

void abFree(struct abuf *ab){
  free(ab->b);
}
/***output***/


// This function is used to draw tildes in the first of every row
void editorDrawRows(struct abuf *ab){
  int y;
  for(y=0; y<E.screenrows;y++){

    if (y == E.screenrows / 3) {
      char welcome[30];
      int welcomelen = snprintf(welcome, sizeof(welcome), "Surial Editor --version %s", SURIAL_VERSION);
      if (welcomelen > E.screencolumns) welcomelen = E.screencolumns;
      abAppend(ab, welcome, welcomelen);
    } else {

    abAppend(ab, "~", 1);
    }
    abAppend(ab, "\x1b[K", 3);
if( y< E.screenrows -1){
    abAppend(ab, "\r\n", 2);
  
}

  }
}

// This function is used to clear the screen
// /x1b is a escape sequence
void editorRefreshScreen(){
  struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
  editorDrawRows(&ab);
      char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));
      abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}





/***input***/

void editorMoveCursor(char c){

  switch (c) {
  case 'a':
      E.cx--;
      break;
    case 'd':
      E.cx++;
      break;
    case 'w':
      E.cy--;
      break;
    case 's':
      E.cy++;
      break;
  }
}
void editorProcessKeyPress(){
    char c = editorReadKey();


    switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO,"\x1b[2J", 4);
      write(STDOUT_FILENO,"\x1b[H", 4);
        exit(0);
        break;
    case 'a':
    case 's':
    case 'd':
    case 'w':
      editorMoveCursor(c);
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
  E.cx = 0;
  E.cy = 0;

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