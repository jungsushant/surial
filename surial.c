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

// Here we declare an enum that asssign numerial values to the char a,d,w,s
enum editorKey {
  ARROW_LEFT = 'a',
  ARROW_RIGHT = 'd',
  ARROW_UP = 'w',
  ARROW_DOWN = 's'
};


/***data***/

//This is the editor's global configuration that defines cursor x&y position, screen rows and cols and termial attributes
struct editorConfig{
  int cx, cy;
  int screenrows;
  int screencolumns;

struct termios orig_termios;
};

// E is the structure for our editor's configuration
struct editorConfig E;





/***terminal***/

// die function exit's  the function with respective error taken in account
void die(const char *s) {
    write(STDOUT_FILENO,"\x1b[2J", 4);
  write(STDOUT_FILENO,"\x1b[H", 4);
  perror(s);
  exit(1);
}

// disableRawMode sets attributes of original termial when exiting the program
void disableRawMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");

}

// editor ReadKey reads each keypress. It also reads arrows.
int editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1))!=1) {
        if (nread == -1 ) die("read");
    }
      if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      switch (seq[1]) {
         case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

// getCursorPosition get the cursor position in rows and column. It uses the function \x1b[6n which return's cursor position.
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


// getWindowSize sets the total no of rows and columns available in the window in the global configuration.
// we use winsize to get the rows and cols if winsize is not available we use
// function \x1b[[999C\x1b[[999B to go 999 times down and 999 times right to count the rows and columns
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


// We append all characters and their respective length to be written out at once when refreshing the screen
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}


// This function takes the buffer's address , char and len.
// It expands previous memory of buffer using the length provided.
// then it appends the new string to the end of memory location of previous buffer. 
void abAppend(struct abuf *ab, const char *s, int len){
  char *new = realloc(ab->b, ab->len +len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;


}

// This function is used to free the used memory space by abFree after writing out the buffer
void abFree(struct abuf *ab){
  free(ab->b);
}
/***output***/


// This function is used to draw tildes in the first of every row and prints welcome message.
// Every thing is appended in the buffer before the final write.
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

// This function is used to refresh the screen and write new content.
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


// this function is used to move the cursor up, down, right & left. 
// It changes the global configuration accordingly. 
void editorMoveCursor(int key){

  switch (key) {
    case ARROW_LEFT:
      E.cx--;
      break;
    case ARROW_RIGHT:
      E.cx++;
      break;
    case ARROW_UP:
      E.cy--;
      break;
    case ARROW_DOWN:
      E.cy++;
      break;
  }
}

// EditorProcessKeyPress processes the keypress and quits the program if it's ctrl+q and moves the cursor it it's arrow keys. 
void editorProcessKeyPress(){
    int c = editorReadKey();


    switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO,"\x1b[2J", 4);
      write(STDOUT_FILENO,"\x1b[H", 4);
        exit(0);
        break;
      case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
    }
}

// enableRawMode set's off different flags of the terminal to make the terminal more customizable
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

// init Editor initializes the required value of the editor.
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
