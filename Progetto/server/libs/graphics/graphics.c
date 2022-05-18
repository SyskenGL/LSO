
/******************************************** INIT GRAPHICS ********************************************/

  #include <stdio.h>
  #include <termios.h>
  #include <unistd.h>
  #include <pthread.h>
  #include "graphics.h"



  int defaultColorForeground = GRAPHICS_FG_COLOR_BLACK;
  int defaultColorBackground = GRAPHICS_BG_COLOR_WHITE;
  pthread_t tidNoechoThread = 0;



  /**
    * @param color: Colore di default per il foreground.
    * @return void.
    *
    * La funzione setDefaultForeground imposta il colore di default
    * del testo (foreground) del terminale.
    */
  void setDefaultForeground(int color){
    defaultColorForeground = color;
  }


  /**
    * @param color: Colore di default per il background.
    * @return void.
    *
    * La funzione setDefaultForeground imposta il colore di default
    * dello sfondo (background) del terminale.
    */
  void setDefaultBackground(int color){
    defaultColorBackground = color;
  }


  /**
    * @param foreground: Colore da impostare per il foreground.
    * @param background: Colore da impostare per il background.
    * @return void.
    *
    * La funzione setColor imposta il colore del foreground e quello
    * del background del terminale. Se il colore non è riconosciuto
    * allora viene impostato quello di default.
    */
  void setColor(int foreground, int background){
    if(foreground >= GRAPHICS_FG_COLOR_BLACK && foreground <= GRAPHICS_FG_COLOR_WHITE){
      printf("\033[%dm", foreground);
    } else{
      printf("\033[%dm", defaultColorForeground);
    }
    if(background >= GRAPHICS_BG_COLOR_BLACK && background <= GRAPHICS_BG_COLOR_WHITE){
      printf("\033[%dm", background);
    } else{
      printf("\033[%dm", defaultColorBackground);
    }
    fflush(stdout);
  }


  /**
    * @param void.
    * @return void.
    *
    * La funzione reset imposta il colore del foreground e quello
    * del background a default.
    */
  void reset(void){
    setColor(defaultColorForeground, defaultColorBackground);
  }


  /**
    * @param void.
    * @return typed: Carattere premuto dall'utente.
    *
    * La funzione getch cattura il carattere premuto dall'utente e non
    * ne effettua l'echo sul terminale.
    */
  int getch(void){
    struct termios oldattr, newattr;
    int typed;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    typed = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return typed;
  }


  /**
    * @param posX: Posizione X nel terminale (colonna).
    * @param posY: Posizione Y nel terminale (riga).
    * @return void.
    *
    * La funzione moveto sposta il cursore alla posizione indicata.
    */
  void moveto(int posX, int posY){
    printf("\033[%d;%dH", posY, posX);
    fflush(stdout);
  }


  /**
    * @param setted: Valore booleano -> true: visibile | false: invisibile.
    * @return void.
    *
    * La funzione setCursor imposta il cursore a visibile o invisibile
    * a seconda del valore del parametro setted.
    */
  void setCursor(bool setted){
    if(setted){
      printf("\e[?25h");
    } else{
      printf("\e[?25l");
    }
    fflush(stdout);
  }


  /**
    * @param void.
    * @return void.
    *
    * La funzione beep emette il suono predefinito dal terminale.
    */
  void beep(void){
    printf("\a");
    fflush(stdout);
  }


  /**
    * @param void.
    * @return void.
    *
    * La funzione clean pulisce il terminale.
    */
  void clean(void){
    printf("\033[2J");
    moveto(1,1);
    fflush(stdout);
  }


  /**
    * @param rows: Numero di righe della finestra.
    * @param cols: Numero di colonne della finestra.
    * @return void.
    *
    * La funzione resizeTerminal effettua il ridimensionamento
    * del terminale.
    */
  void resizeTerminal(int rows, int cols){
    printf("\e[8;%d;%dt", rows, cols);
    fflush(stdout);
  }


  /**
    * @param setted: Valore booleano -> true: underscore | false: no underscore.
    * @return void.
    *
    * La funzione setUnderscore imposta l'underscore del testo a true oppure
    * a false.
    */
  void setTextStyle(int style){
    if(style == GRAPHICS_TEXT_DEFAULT || style == GRAPHICS_TEXT_BOLD
            || style == GRAPHICS_TEXT_UNDERSCORE){
      printf("\033[%dm", style);
    } else{
      printf("\033[%dm", GRAPHICS_TEXT_DEFAULT);
    }
  }

/******************************************** END GRAPHICS ********************************************/
