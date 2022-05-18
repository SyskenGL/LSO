
/******************************************** INIT GRAPHICS HEADER ********************************************/

  #include <stdbool.h>



  /* Altezza e larghezza chat */
  #define GRAPHICS_CHAT_HEIGHT           9
  #define GRAPHICS_CHAT_WIDTH           82

  /* Altezza del box status del server */
  #define GRAPHICS_STATUS_HEIGHT         7

  /* Altezza e larghezza screen */
  #define GRAPHICS_SCREEN_HEIGHT        37
  #define GRAPHICS_SCREEN_WIDTH         97

  /* Altezza e larghezza playground */
  #define GRAPHICS_FIELD_HEIGHT         15
  #define GRAPHICS_FIELD_WIDTH          57

  /* Altezza dell'info-box dei players */
  #define GRAPHICS_PLAYERSINFO_HEIGHT    5

  /* Codici stile testo */
  #define GRAPHICS_TEXT_DEFAULT          0
  #define GRAPHICS_TEXT_BOLD             1
  #define GRAPHICS_TEXT_UNDERSCORE       4

  /* Codici colore background */
  #define GRAPHICS_FG_COLOR_BLACK       30
  #define GRAPHICS_FG_COLOR_RED         31
  #define GRAPHICS_FG_COLOR_GREEN       32
  #define GRAPHICS_FG_COLOR_YELLOW      33
  #define GRAPHICS_FG_COLOR_BLUE        34
  #define GRAPHICS_FG_COLOR_MAGENTA     35
  #define GRAPHICS_FG_COLOR_CYAN        36
  #define GRAPHICS_FG_COLOR_WHITE       37


  /* Codici colore background */
  #define GRAPHICS_BG_COLOR_BLACK       40
  #define GRAPHICS_BG_COLOR_RED         41
  #define GRAPHICS_BG_COLOR_GREEN       42
  #define GRAPHICS_BG_COLOR_YELLOW      43
  #define GRAPHICS_BG_COLOR_BLUE        44
  #define GRAPHICS_BG_COLOR_MAGENTA     45
  #define GRAPHICS_BG_COLOR_CYAN        46
  #define GRAPHICS_BG_COLOR_WHITE       47


  /* Lunghezza nome utente */
  #define GRAPHICS_USERNAME_STRLEN      10

  /* Pin di aggancio per la scansione dell'hud */
  #define GRAPHICS_PIN_FIELD            'x'
  #define GRAPHICS_PIN_CHAT             '!'
  #define GRAPHICS_PIN_TIMER            '#'
  #define GRAPHICS_PIN_COUNTER          '@'
  #define GRAPHICS_PIN_PLAYERS          '+'
  #define GRAPHICS_PIN_STATUS           '?'
  #define GRAPHICS_PIN_START            '>'

  #define GRAPHICS_KEY_ENTER             10
  #define GRAPHICS_KEY_BACKSPACE        127

  #define GRAPHICS_TEXTFIELD_STRLEN      70


  extern int defaultColorBackground;
  extern int defaultColorForeground;



  /**
    * @attr posX: Posizione X nel terminale.
    * @attr posY: Posizione Y nel terminale.
    *
    */
  struct Coord{
    int posX;
    int posY;
  };
  typedef struct Coord Coord;


  /**
    * @attr posX: Posizione X nel terminale.
    * @attr posY: Posizione Y nel terminale.
    *
    */
  struct PlayersCounter{
    Coord coords;
    int value;
  };
  typedef struct PlayersCounter PlayersCounter;


  /**
    * @attr coords: Posizione della componente nel terminale.
    * @attr isEmpty: Flag che indica se la box è piena oppure vuota.
    * @attr color: Colore di stampa del messaggio.
    * @attr msg: Messaggio contenuto nel box.
    *
    */
  struct MsgBox{
    Coord coords;
    bool isEmpty;
    int color;
    char msg[GRAPHICS_CHAT_WIDTH];
  };
  typedef struct MsgBox MsgBox;


  /**
    * @attr msgBox[]: Array di box messaggi di cui è composta la chat.
    * @attr nextMsgPosition: Posizione prevista per il prossimo messaggio in arrivo.
    *
    */
  struct Chat{
    MsgBox msgBox[GRAPHICS_CHAT_HEIGHT];
    int currMsgPosition;
  };
  typedef struct Chat Chat;


  /**
    * @attr coords: Posizione della componente nel terminale.
    * @attr packetsDelivered: Numero di pacchetti consegnati.
    * @attr username: Username del player.
    *
    */
  struct PlayerBox{
    Coord coords;
    int packetsDelivered;
    char username[GRAPHICS_USERNAME_STRLEN];
  };
  typedef struct PlayerBox PlayerBox;


  /**
    * @attr playerBox[]: Array di box player di cui è composta l'info-box dei players connessi alla sessione.
    * @attr currPlayerPosition: Posizione del "cursore" nell'info-box dei players connessi alla sessione.
    *
    */
  struct PlayersInfo{
    PlayerBox playerBox[GRAPHICS_PLAYERSINFO_HEIGHT];
    int currPlayerPosition;
  };
  typedef struct PlayersInfo PlayersInfo;


  /**
    * @attr msg: Messaggio da stampare sulla chat.
    * @attr color: Colore del testo.
    * @attr error: Indica se il messaggio è una notifica di errore.
    *
    */
  struct Msg{
    char msg[GRAPHICS_CHAT_WIDTH];
    int color;
    bool error;
  };
  typedef struct Msg Msg;
  typedef struct Msg* LpMsg;


  /**
    * @param void.
    * @return typed: Carattere premuto dall'utente.
    *
    * La funzione getch cattura il carattere premuto dall'utente e non
    * ne effettua l'echo sul terminale.
    */
  int getch(void);


  /**
    * @param posX: Posizione X nel terminale (colonna).
    * @param posY: Posizione Y nel terminale (riga).
    * @return void.
    *
    * La funzione moveto sposta il cursore alla posizione indicata.
    */
  void moveto(int, int);


  /**
    * @param setted: Valore booleano -> true: visibile | false: invisibile.
    * @return void.
    *
    * La funzione setCursor imposta il cursore a visibile o invisibile
    * a seconda del valore del parametro setted.
    */
  void setCursor(bool);


  /**
    * @param void.
    * @return void.
    *
    * La funzione beep emette il suono predefinito dal terminale.
    */
  void beep(void);


  /**
    * @param void.
    * @return void.
    *
    * La funzione clean pulisce il terminale.
    */
  void clean(void);


  /**
    * @param foreground: Colore da impostare per il foreground.
    * @param background: Colore da impostare per il background.
    * @return void.
    *
    * La funzione setColor imposta il colore del foreground e quello
    * del background del terminale. Se il colore non è riconosciuto
    * allora viene impostato quello di default.
    */
  void setColor(int, int);


  /**
    * @param void.
    * @return void.
    *
    * La funzione reset imposta il colore del foreground e quello
    * del background a default.
    */
  void reset(void);


  /**
    * @param color: Colore di default per il foreground.
    * @return void.
    *
    * La funzione setDefaultForeground imposta il colore di default
    * del testo (foreground) del terminale.
    */
  void setDefaultForeground(int);


  /**
    * @param color: Colore di default per il background.
    * @return void.
    *
    * La funzione setDefaultForeground imposta il colore di default
    * dello sfondo (background) del terminale.
    */
  void setDefaultBackground(int);


  /**
    * @param rows: Numero di righe della finestra.
    * @param cols: Numero di colonne della finestra.
    * @return void.
    *
    * La funzione resizeTerminal effettua il ridimensionamento
    * del terminale.
    */
  void resizeTerminal(int, int);


  /**
    * @param style: Stile del testo.
    * @return void.
    *
    * La funzione setTextStyle imposta lo stile del testo.
    */
  void setTextStyle(int);

/******************************************** END GRAPHICS HEADER ********************************************/
