#include <termios.h>
#include <unistd.h>
#include <termio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

struct termios orig_termios;

void disableRawMode(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

}

void enableRawMode(){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;


    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG );
    raw.c_iflag &= ~(BRKINT | INPCK| ISTRIP | IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);

    tcsetattr(STDERR_FILENO, TCSAFLUSH, &raw);

}
int main() {
    enableRawMode();
    char c;
    while(read(STDIN_FILENO,&c,1)==1 && c != 'q') {
        if(iscntrl(c)){
            printf("%d\r\n",c);
        } else {
            printf("%d ('%c')\r\n",c,c);
        }
    }
    return 0;
}