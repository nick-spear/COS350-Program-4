/********************************************************
    COS350 Program #4: scroll
    Author: Nickolas Spear
*********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define TAB_WIDTH 8

FILE *fp;                               // Storing active file stream
unsigned short term_width, term_height; // Terminal dimensions
struct termios orig_info;               // Backup terminal settings
int scroll_flag = 0;                    // Status flag for 'currently scrolling'
int EOF_flag = 0;                       // Status flag for 'end of file reached'
float scroll_speed = 2.0;               // Scroll speed

void user_input();
void term_handler(int signum);
void next_line(int signum);
void next_page(int init);
void set_scroll(float increase);
void remove_scroll();
void prompt();
void remove_prompt();

/********************************************************
  int main(int ac, char **av)

  Sets non-status-flag global variables and handles
  stream inputs.
*********************************************************/
int main( int ac , char ** av )
{
  struct winsize winsizestruct;
  struct sigaction term_action, scroll_action;

  /* Assigns handler for graceful exiting when
   * receiving interrupt signals.
   */
  term_action.sa_handler = term_handler;
  sigemptyset(&term_action.sa_mask);
  term_action.sa_flags = 0;
  sigaction(SIGINT, &term_action, NULL);

  /* Assigns alarm handler to print the next
   * line of text.
   */
  scroll_action.sa_handler = next_line;
  sigemptyset(&scroll_action.sa_mask);
  scroll_action.sa_flags = 0;
  sigaction(SIGALRM, &scroll_action, NULL);

  /* Retrieves terminal size and updates
   * global variables.
   */
  ioctl(1, TIOCGWINSZ, &winsizestruct);
  term_height = winsizestruct.ws_row;
  term_width = winsizestruct.ws_col;
  printf("width %d\n", term_width);
  /*
   *
   */
  if ( ac == 1 ) {
    if ( (fp = stdin) != NULL ) user_input();
  } else {
    while ( --ac ) {
      if ( (fp = fopen( *++av , "r" )) != NULL ) {
        user_input() ;
      	fclose( fp );
      } else {
        printf("Error: '%s' file not found.\n", *av);
        exit(1);
      }
    }
  }
}


/********************************************************
  void user_input()

  Handles user interaction for a given stream, reading
  from the keyboard device and setting/resetting
  terminal settings.
*********************************************************/
void user_input() {
  FILE  *fp_tty;
  char c;
  struct termios info;

  fp_tty = fopen("/dev/tty", "r" ); // Gets keyboard stream
  if ( fp_tty == NULL )
    exit(1);

  /* Retrieves terminal settings, stores original settings,
   * and assigns new settings.
   */
  tcgetattr(fileno(fp_tty),&orig_info);
  tcgetattr(fileno(fp_tty),&info);
  info.c_lflag &= ~ECHO ;
  info.c_lflag &= ~ICANON ;
  info.c_cc[VMIN] = 1;
  tcsetattr(fileno(fp_tty),TCSANOW,&info);

  next_page(1);

  while( (c = getc(fp_tty)) != 'q' ) // Reads user input from tty
    {
      switch (c) {
        case ' ':
          if (scroll_flag == 1) remove_scroll();
          next_page(0);
        case '\n':
        if (scroll_flag == 1) {
          remove_scroll();
        } else if (scroll_flag == 0) {
          set_scroll(1.0);
        }
        case 'f':
          if (scroll_flag == 1) set_scroll(0.8);
        case 's':
          if (scroll_flag == 1) set_scroll(1.2);
      }
    }

  EOF_flag = 0;
  scroll_flag = 0;
  scroll_speed = 2.0;

  tcsetattr(fileno(fp_tty),TCSANOW,&orig_info);
  printf("\n");
}


/********************************************************
  void next_line(int signum)

  Removes current prompt, prints out one line of text
  from file, and prints user prompt.
*********************************************************/
void next_line(int signum)
{
  int chars_on_line = 0;
  char read_char;

  remove_prompt();

  while ( chars_on_line < term_width ) {
    if ((fread(&read_char, 1, 1, fp) <= 0)) {
      EOF_flag = 1;
      break;
    }

    if (read_char == '\n') {
      putchar(read_char);
      break;
    } else if (read_char == '\t') { // Converts tabs to spacing
      int tab_size = TAB_WIDTH - (chars_on_line % TAB_WIDTH);
      if (chars_on_line + tab_size >= term_width) {
        putchar('\n');
        break;
      } else {
        chars_on_line += tab_size;
        while (tab_size > 0) {
          putchar(' ');
          tab_size--;
        }
      }
    } else {
      putchar(read_char);
      chars_on_line++;
    }
  }

  prompt();
}


/********************************************************
  void next_page(int init)

  Prints lines (exactly as next_line does) until
  terminal window is full. If init is set to 0, next_page
  assumes a prompt must first be remove.
*********************************************************/
void next_page(int init)
{
  if (init == 0) remove_prompt();

  for(int i = 0; i < term_height - 1; i++) {
    int chars_on_line = 0;
    char read_char;
    while ( chars_on_line < term_width ) {
      if ((fread(&read_char, 1, 1, fp) <= 0)) {
        EOF_flag = 1;
        break;
      }

      if (read_char == '\n') {
        putchar(read_char);
        break;
      } else if (read_char == '\t') {
        int tab_size = TAB_WIDTH - (chars_on_line % TAB_WIDTH);
        if (chars_on_line + tab_size >= term_width) {
          putchar('\n');
          break;
        } else {
          chars_on_line += tab_size;
          while (tab_size > 0) {
            putchar(' ');
            tab_size--;
          }
        }
      } else {
        putchar(read_char);
        chars_on_line++;
      }
    }
  }
  prompt();
}


/********************************************************
  void prompt()

  Prints prompt to user based on which status flags
  are set.
*********************************************************/
void prompt()
{
  if (EOF_flag == 1) {
    printf("\033[7m Scroll -- Reached end of file. \033[0m");
  } else if (scroll_flag == 0) {
    printf("\033[7m Scroll -- Press Enter to begin scrolling. \033[0m");
  } else if (scroll_flag == 1) {
    printf("\033[7m Scroll -- Scrolling every %5.2f seconds. \033[0m", scroll_speed);
  }
}


/********************************************************
  void remove_prompt()

  Enters backspace a specific number of times based on
  which status flags are currently set.
*********************************************************/
void remove_prompt()
{
  int i;

  if (EOF_flag == 1) {
    i = 32;
  } else if (scroll_flag == 1) {
    i = 44;
  } else if (scroll_flag == 0) {
    i = 43;
  }

  while (i > 0) {
    printf("\b");
    i--;
  }

  printf("\033[0K");
}


/********************************************************
  void term_handler(int signum)

  Gracefully hands interrupt signals by resetting
  terminal settings to original before exit.
*********************************************************/
void term_handler(int signum)
{
  tcsetattr(fileno(fopen("/dev/tty","r")),TCSANOW,&orig_info);
  printf("\n");
  exit(0);
}


/********************************************************
  void set_scroll(float increase)

  Sets ITIMER_REAL interval timer to repeat every
  (scroll_speed * increase) seconds. Sets scroll_speed
  to (scroll_speed * increase), and sets scroll_flag to 1.
*********************************************************/
void set_scroll(float increase) {
  remove_prompt();
  scroll_flag = 1;
  scroll_speed *= increase;

  long sec_long = (long)scroll_speed;
  long usec_long = ((long)(scroll_speed * 1000000)) % 1000000;

  struct itimerval scroll_timer;
  scroll_timer.it_value.tv_sec = (time_t)sec_long;
  scroll_timer.it_value.tv_usec = (suseconds_t)usec_long;
  scroll_timer.it_interval.tv_sec = (time_t)sec_long;
  scroll_timer.it_interval.tv_usec = (suseconds_t)usec_long;

  prompt();

  setitimer(ITIMER_REAL, &scroll_timer, NULL);
}


/********************************************************
  void remove_scroll()

  Removes ITIMER_REAL interval timer, resets prompt,
  sets scroll flag to 0, and resets scroll speed.
*********************************************************/
void remove_scroll() {
  struct itimerval scroll_timer;
  getitimer(ITIMER_REAL, &scroll_timer);
  scroll_timer.it_value.tv_sec = 0;
  scroll_timer.it_value.tv_usec = 0;
  scroll_timer.it_interval.tv_sec = 0;
  scroll_timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &scroll_timer, NULL);

  remove_prompt();
  scroll_speed = 2.0;
  scroll_flag = 0;
  prompt();
}
