
/******************************************** INIT CLIENT ********************************************/


//______LIBRARIES______________________________________________________________________________________

  #include "../libs/graphics/graphics.h"
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
  #include <errno.h>
  #include <arpa/inet.h>
  #include <pthread.h>
  #include <stdbool.h>
  #include <string.h>
  #include <stdio.h>
  #include <signal.h>
  #include <unistd.h>
  #include <time.h>
  #include <fcntl.h>
  #include <ctype.h>
  #include <stdlib.h>

//______COSTANTS______________________________________________________________________________________

  #define SERVER_NOTIFY                '$'
  #define SESSION_NOTIFY               '%'
  #define SYMBOL_GRASS                 " "
  #define SYMBOL_PLAYER           "\u263b"
  #define SYMBOL_PACKET           "\u2666"
  #define SYMBOL_WALL             "\u2590"

//______GLOBAL VARIABLES_______________________________________________________________________________

  Coord timer;
  Coord textField;
  Coord cursor;
  Coord field[GRAPHICS_FIELD_HEIGHT][GRAPHICS_FIELD_WIDTH];
  PlayersCounter playersCounter;
  PlayersInfo playersInfo;
  Chat chat;

  int clientSocket;
  struct sockaddr_in address;

  pthread_mutex_t mutexCursor = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexChat = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexUpdate = PTHREAD_MUTEX_INITIALIZER;

  pthread_t tidBlink = 0;
  int rowToBlink;
  int colToBlink;
  char* locationToBlink;

  bool connected = false;

//______FUNCTIONS DECLARATION__________________________________________________________________________

  int initGUI(char*, char*);
  int mapping(char*);
  void goToTheTextField(void);
  void cleanTextField(void);
  void* listenerTextField(void*);
  bool isAlphabet(char);
  int connection(void);
  void* updateChat(void*);
  int trimMsg(char[]);
  LpMsg allocMsg(char*, int, bool);
  void sendMsg(char*);
  void* listenerServer(void*);
  int takeAction(char[]);
  void* updateTimer(void*);
  void* updatePlayersInfo(void*);
  void* updateField(void*);
  void signalHandler(int);



  int main(void){

    pthread_t tidListenerTextField;
    pthread_t tidListenerServer;

    srand(time(NULL));
    signal(SIGPIPE, SIG_IGN);

    clientSocket = socket(PF_INET, SOCK_STREAM, 0);
    if(connection() == 1){
      return 1;
    }

    signal(SIGSTOP, signalHandler);
    signal(SIGINT, signalHandler);

    if(initGUI("../gui/blueprint.txt", "../gui/gui.txt")){
      beep();
      setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_WHITE);
      printf("\n <!> Errore durante l'inizializzazione dell'interfaccia grafica.\n\n");
      reset();
      return 1;
    }

    pthread_create(&tidListenerTextField, NULL, listenerTextField, NULL);
    pthread_create(&tidListenerServer, NULL, listenerServer, NULL);
    pthread_join(tidListenerServer, NULL);
    pthread_cancel(tidListenerTextField);

    sleep(2);
    setCursor(true);
    clean();

    close(clientSocket);

    return 0;

  }

//______FUNCTION FOR MANAGMENT GRAPHICS___________________________________________________________________

 /**
   * @param bluerprint: path del file blueprint
   * che inizializza la matrice per la grafica finale.
   * @param gui:path del file della grafica finale
   * da andare a stampare.
   *
   * @return: 1 in caso di errore 0 altrimenti.
   *
   * La funzione initGUI apre in sola lettura il file che
   * contiene le grafica che poi stamperà sul rispettivo terminale.
   *
   */
 int initGUI(char* blueprint, char* gui){

  char basicComponent[1];
  int bytesReaded;
  int guiFile;

  setCursor(true);

  if(mapping(blueprint)){
    return 1;
  }

  if((guiFile = open(gui, O_RDONLY)) == -1){
    return 1;
  }

  clean();
  while((bytesReaded = read(guiFile, basicComponent, 1)) > 0){
    basicComponent[bytesReaded] = '\0';
    if(basicComponent[0] == 'x'){
      setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
      printf(" ");
      reset();
    } else{
      printf("%s", basicComponent);
    }
  }

  cursor.posX = textField.posX;
  cursor.posY = textField.posY;
  goToTheTextField();

}


 /**
   * @param blueprint: path del file che inizializza
   * la matrice per poter stampare poi sopra la grafica.
   *
   * @return: return 1 in caso di errore e 0 altrimenti.
   *
   * La funzione mappping apre in sola lettura lettura
   * il file che inizializza la matrice per la grafica.
   *
   */
 int mapping(char* blueprint){

  char basicComponent[1];
  int bytesReaded;
  int blueprintFile;
  int currRowsScreen = 1;
  int currColsScreen = 1;
  int currRowsField = 0;
  int currColsField = 0;
  int currPlayerPosition = 0;
  int currMsgPosition = 0;

  if((blueprintFile = open(blueprint, O_RDONLY)) == -1){
    return 1;
  }

  while((bytesReaded = read(blueprintFile, basicComponent, 1)) > 0){

    basicComponent[bytesReaded] = '\0';

    switch (basicComponent[0]) {
      case GRAPHICS_PIN_CHAT:
        chat.msgBox[currMsgPosition].coords.posX = currColsScreen;
        chat.msgBox[currMsgPosition].coords.posY = currRowsScreen;
        currMsgPosition += 1;
        break;
      case GRAPHICS_PIN_FIELD:
        field[currRowsField][currColsField].posX = currColsScreen;
        field[currRowsField][currColsField].posY = currRowsScreen;
        currColsField += 1;
        if(currColsField == GRAPHICS_FIELD_WIDTH){
          currRowsField += 1;
          currColsField = 0;
        }
        break;
      case GRAPHICS_PIN_TIMER:
        timer.posX = currColsScreen;
        timer.posY = currRowsScreen;
        break;
      case GRAPHICS_PIN_COUNTER:
        playersCounter.coords.posX = currColsScreen;
        playersCounter.coords.posY = currRowsScreen;
        break;
      case GRAPHICS_PIN_PLAYERS:
        playersInfo.playerBox[currPlayerPosition].coords.posX = currColsScreen;
        playersInfo.playerBox[currPlayerPosition].coords.posY = currRowsScreen;
        currPlayerPosition += 1;
        break;
      case GRAPHICS_PIN_TEXTFIELD:
        textField.posX = currColsScreen;
        textField.posY = currRowsScreen;
        break;
      case GRAPHICS_PIN_START:
        clean();
        break;
    }

    currColsScreen += 1;

    if(basicComponent[0] == '\n'){
      currColsScreen = 1;
      currRowsScreen += 1;
    }

  }

    if(bytesReaded == -1){
      return 1;
    }

    chat.currMsgPosition = 0;
    playersInfo.currPlayerPosition = -1;
    playersCounter.value = 0;

    for(int msg = 0; msg < GRAPHICS_CHAT_HEIGHT; msg++){
      chat.msgBox[msg].isEmpty = true;
    }

    close(blueprintFile);
    return 0;

  }


 /**
   * @param arg: msgChat -> messaggio da inserire in chat .
   *
   * @return: puntatore a void.
   *
   * La funzione updateChat aggiorna la chat stampando
   * eventuali nuovi messaggi. Nel caso in cui la chat fosse
   * piena viene effettuato lo shit della stessa dal basso
   * verso l'alto.
   *
   */
 void* updateChat(void* arg){

  LpMsg msgChat = (LpMsg)arg;
  char msg[GRAPHICS_CHAT_WIDTH];
  strcpy(msg, msgChat->msg);
  int color = msgChat->color;

  pthread_mutex_lock(&mutexChat);
  /* Controllo se la chat è piena */
  if(chat.currMsgPosition == GRAPHICS_CHAT_HEIGHT-1 && chat.msgBox[chat.currMsgPosition].isEmpty == false){
    /* Effettuo lo shift della chat */
    for(int currMsg = 0; currMsg < GRAPHICS_CHAT_HEIGHT-1; currMsg++){
      strcpy(chat.msgBox[currMsg].msg, chat.msgBox[currMsg+1].msg);
      chat.msgBox[currMsg].color = chat.msgBox[currMsg+1].color;
    }
    /* Copio il nuovo messaggio nella prima posizione a partire dal basso */
    strcpy(chat.msgBox[GRAPHICS_CHAT_HEIGHT-1].msg, msg);
    chat.msgBox[GRAPHICS_CHAT_HEIGHT-1].color = color;

    /* Ristampo l'intera chat shiftata */
    for(int currMsg = 0; currMsg < GRAPHICS_CHAT_HEIGHT; currMsg++){
      /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
      pthread_mutex_lock(&mutexCursor);
      setCursor(false);
      moveto(chat.msgBox[currMsg].coords.posX, chat.msgBox[currMsg].coords.posY);
      printf("                                                                                  │    │");
      fflush(stdout);
      moveto(chat.msgBox[currMsg].coords.posX, chat.msgBox[currMsg].coords.posY);
      setColor(chat.msgBox[currMsg].color, 0);
      printf("%s", chat.msgBox[currMsg].msg);
      fflush(stdout);
      reset();
      moveto(cursor.posX, cursor.posY);
      setCursor(true);
      pthread_mutex_unlock(&mutexCursor);
    }

  } else{

    /* Imposta il msgBox relativo al messaggio a pieno */
    chat.msgBox[chat.currMsgPosition].isEmpty = false;
    chat.msgBox[chat.currMsgPosition].color = color;
    strcpy(chat.msgBox[chat.currMsgPosition].msg, msg);

    /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
    pthread_mutex_lock(&mutexCursor);
    setCursor(false);
    moveto(chat.msgBox[chat.currMsgPosition].coords.posX, chat.msgBox[chat.currMsgPosition].coords.posY);
    printf("                                                                                  │    │");
    fflush(stdout);
    moveto(chat.msgBox[chat.currMsgPosition].coords.posX, chat.msgBox[chat.currMsgPosition].coords.posY);
    setColor(color, 0);
    printf("%s", msg);
    fflush(stdout);
    reset();
    moveto(cursor.posX, cursor.posY);
    setCursor(true);
    pthread_mutex_unlock(&mutexCursor);

    /* Incremento l'indice della chat */
    if(chat.currMsgPosition != GRAPHICS_CHAT_HEIGHT-1){
      chat.currMsgPosition += 1;
    }
  }

  pthread_mutex_unlock(&mutexChat);

  if(msgChat->error == true){
    beep();
  }

  free(msgChat);

 }


 /**
   * @param msg: arg -> Secondi totali.
   *
   * @return: puntatore a void.
   *
   * La funzione updateTimer aggiorna il timer di fine
   * partita.
   *
   */
 void* updateTimer(void* arg){

  int totalSeconds = *((int*)arg);

  int minutes = totalSeconds / 60;    /* Calcolo i minuti */
  int seconds = totalSeconds % 60;   /* Calcolo i secondi */

  pthread_mutex_lock(&mutexCursor);
  setCursor(false);
  moveto(timer.posX, timer.posY);

  /* Effettuo l'aggiornamento del timer */
  if(totalSeconds >= 300 && totalSeconds <= 600){
   setColor(GRAPHICS_FG_COLOR_GREEN, 0);
   printf("%.2d:%.2d", minutes, seconds);
   reset();
  } else if(totalSeconds >= 60 && totalSeconds <= 300){
   setColor(GRAPHICS_FG_COLOR_YELLOW, 0);
   printf("%.2d:%.2d", minutes, seconds);
   reset();
  } else{
    if(totalSeconds <= 10 && totalSeconds%2 == 0){
     beep();
     setColor(GRAPHICS_FG_COLOR_RED, 0);
     printf("%.2d:%.2d", minutes, seconds);
     reset();
    } else if(totalSeconds <= 10 && totalSeconds%2 != 0){
       beep();
       setColor(GRAPHICS_FG_COLOR_YELLOW, 0);
       printf("%.2d:%.2d", minutes, seconds);
       reset();
    } else{
       setColor(GRAPHICS_FG_COLOR_RED, 0);
       printf("%.2d:%.2d", minutes, seconds);
       reset();
    }
  }

   fflush(stdout);
   moveto(cursor.posX, cursor.posY);
   setCursor(true);
   pthread_mutex_unlock(&mutexCursor);
   free(arg);

 }


 /**
   * @param msg: arg -> Comando [$(dis)connected Username (packagesDelivered)].
   *
   * @return: puntatore a void.
   *
   * La funzione updatePlayersInfo aggiorna il counter
   * e la lista dei players attualmente connessi alla
   * sessione corrente.
   *
   */
 void* updatePlayersInfo(void* arg){

  char* msg = (char*)arg;                       /* Messaggio in entrata               */
  char* saveptr;
  char* cmd = strtok_r(msg, " ", &saveptr);     /* Comando effettivo                  */
  char* username;                               /* Username dell'utente (dis)connesso */
  int packetsDelivered;                         /* Numero di pacchetti consegnati     */
  char buffer[GRAPHICS_TEXTFIELD_STRLEN];
  int focusedPlayer = 0;                        /* Posizione del player puntato       */

  pthread_mutex_lock(&mutexUpdate);
  /* Se il comando è di tipo disconnessione allora */
  if(!strcmp(cmd, "$disconnected") && playersCounter.value > 0){
   /* Estraggo l'username */
   username = strtok_r(NULL, " ", &saveptr);
   /* Aggiorno il counter */
   playersCounter.value -= 1;
   pthread_mutex_lock(&mutexCursor);
   setCursor(false);
   moveto(playersCounter.coords.posX, playersCounter.coords.posY);
   printf("%d", playersCounter.value);
   fflush(stdout);
   moveto(cursor.posX, cursor.posY);
   setCursor(true);
   pthread_mutex_unlock(&mutexCursor);

   /* Aggiorno la lista dei players */
   /* Estraggo l'indice del player disconnesso */
   while(strcmp(username, playersInfo.playerBox[focusedPlayer].username) != 0){
     focusedPlayer += 1;
   }

   /* Scorro verso l'alto a partire dall'utente eliminato tutti gli utenti presenti in sessione(a livello grafico),  */
   /* mentre nell'array viene fatto un semplice shift a sinistra di tutti gli utenti che si trovano dopo             */
   /* l'utente eliminato                                                                                             */
   for(int player = focusedPlayer; player < playersInfo.currPlayerPosition; player++){
     strcpy(playersInfo.playerBox[player].username, playersInfo.playerBox[player+1].username);
     playersInfo.playerBox[player].packetsDelivered = playersInfo.playerBox[player+1].packetsDelivered;
     pthread_mutex_lock(&mutexCursor);
     setCursor(false);
     moveto(playersInfo.playerBox[player].coords.posX, playersInfo.playerBox[player].coords.posY);
     printf("                     │    │");
     fflush(stdout);
     memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
     moveto(playersInfo.playerBox[player].coords.posX, playersInfo.playerBox[player].coords.posY);
     sprintf(buffer, " \u25ba %-13s[%.2d] ", playersInfo.playerBox[player].username, playersInfo.playerBox[player].packetsDelivered);
     printf("%s", buffer);
     fflush(stdout);
     moveto(cursor.posX, cursor.posY);
     setCursor(true);
     pthread_mutex_unlock(&mutexCursor);
    }

    pthread_mutex_lock(&mutexCursor);
    setCursor(false);
    moveto(playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posX, playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posY);
    printf("                     │    │");
    fflush(stdout);
    playersInfo.currPlayerPosition -= 1;
    moveto(cursor.posX, cursor.posY);
    setCursor(true);
    pthread_mutex_unlock(&mutexCursor);

    /* Se il comando è di tipo connessione allora */
  } else if(!strcmp(cmd, "$connected")){
     /* Estraggo l'username */
     username = strtok_r(NULL, " ", &saveptr);
     packetsDelivered = atoi(strtok_r(NULL, " ", &saveptr));
     memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
     /* Unisco nel buffer info la stringa da stampare nella lista degli utenti in sessione */
     sprintf(buffer, " \u25ba %-13s[%.2d] ", username, packetsDelivered);
     /* Aggiorno il counter */
     playersCounter.value += 1;
     pthread_mutex_lock(&mutexCursor);
     setCursor(false);
     moveto(playersCounter.coords.posX, playersCounter.coords.posY);
     printf("%d", playersCounter.value);
     fflush(stdout);
     moveto(cursor.posX, cursor.posY);
     setCursor(true);
     pthread_mutex_unlock(&mutexCursor);
     playersInfo.currPlayerPosition += 1;
     strcpy(playersInfo.playerBox[playersInfo.currPlayerPosition].username, username);
     playersInfo.playerBox[playersInfo.currPlayerPosition].packetsDelivered = packetsDelivered;
     pthread_mutex_lock(&mutexCursor);
     setCursor(false);
     moveto(playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posX, playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posY);
     printf("%s", buffer);
     fflush(stdout);
     moveto(cursor.posX, cursor.posY);
     setCursor(true);
     pthread_mutex_unlock(&mutexCursor);

    /* Se il comando è di tipo Aggiornamento pacchetti allora */
  } else if(!strcmp(cmd, "$delivered")){
     /* Estraggo l'username */
     username = strtok_r(NULL, " ", &saveptr);

     /* Aggiorno la lista dei players                               */
     /* Estraggo l'indice del player che ha consegnato il pacchetto */
     while(strcmp(username, playersInfo.playerBox[focusedPlayer].username) != 0){
       focusedPlayer += 1;
     }

     /* Aggiorno il numero di pacchetti consegnati */
     playersInfo.playerBox[focusedPlayer].packetsDelivered += 1;
     memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
     /* Unisco nel buffer info la stringa da stampare nella lista degli utenti in sessione */
     sprintf(buffer, " \u25ba %-13s[%.2d] ", username, playersInfo.playerBox[focusedPlayer].packetsDelivered);
     pthread_mutex_lock(&mutexCursor);
     setCursor(false);
     moveto(playersInfo.playerBox[focusedPlayer].coords.posX, playersInfo.playerBox[focusedPlayer].coords.posY);
     printf("                     │    │");
     fflush(stdout);
     moveto(playersInfo.playerBox[focusedPlayer].coords.posX, playersInfo.playerBox[focusedPlayer].coords.posY);
     printf("%s", buffer);
     fflush(stdout);
     moveto(cursor.posX, cursor.posY);
     setCursor(true);
     pthread_mutex_unlock(&mutexCursor);
   }
     pthread_mutex_unlock(&mutexUpdate);
     free(arg);
 }


 /**
   * @param arg: -> NULL.
   *
   * @return: puntatore a void.
   *
   * La funzione updateField gestisce le funzioni di aggiunta e di aggiornamento
   * di oggeti della mappa (locazioni,pacchi ,ostacoli, player);
   *
   */
 void* updateField(void* arg){

  int currRow = 0;
  int currColumn = 0;
  char* saveptr;
  char* cmd;                    /* Messaggio di comando                    */
  char* type;                   /* Messaggio di tipo di oggetto            */
  char* msg = (char*)arg;       /* Messaggio in entrata                    */

  /* Estraggo il tipo di comando */
  cmd = strtok_r(msg, " ", &saveptr);

  /* Controllo che tipo di comando sia */
  if(!strcmp("$add", cmd)){
    type = strtok_r(NULL, " ", &saveptr);
    /* Se il comando è di tipo add e l'oggetto da aggiungere è un packet allora */
    if(!strcmp("packet", type)){
      currRow = atoi(strtok_r(NULL, " ", &saveptr));
      currColumn = atoi(strtok_r(NULL, " ", &saveptr));
      /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
      pthread_mutex_lock(&mutexCursor);
      setCursor(false);
      moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
      setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
      printf("%s", SYMBOL_PACKET);
      reset();
      fflush(stdout);
      moveto(cursor.posX, cursor.posY);
      setCursor(true);
      pthread_mutex_unlock(&mutexCursor);
      /* Se il comando è di tipo add e l'oggetto da aggiungere è una location allora */
    } else if(!strcmp("location", type)){
        currRow = atoi(strtok_r(NULL, " ", &saveptr));
        currColumn = atoi(strtok_r(NULL, " ", &saveptr));
        /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
        pthread_mutex_lock(&mutexCursor);
        setCursor(false);
        moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
        setColor(GRAPHICS_FG_COLOR_WHITE, GRAPHICS_BG_COLOR_MAGENTA);
        printf("%s", strtok_r(NULL, " ", &saveptr));
        reset();
        fflush(stdout);
        moveto(cursor.posX, cursor.posY);
        setCursor(true);
        pthread_mutex_unlock(&mutexCursor);
      /* Se il comando è di tipo add e l'oggetto da aggiungere è il mio omino allora */
    } else if(!strcmp("you", type)){
        currRow = atoi(strtok_r(NULL, " ", &saveptr));
        currColumn = atoi(strtok_r(NULL, " ", &saveptr));
        /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
        pthread_mutex_lock(&mutexCursor);
        setCursor(false);
        moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
        setColor(GRAPHICS_FG_COLOR_BLUE, GRAPHICS_BG_COLOR_GREEN);
        printf("%s", SYMBOL_PLAYER);
        reset();
        fflush(stdout);
        moveto(cursor.posX, cursor.posY);
        setCursor(true);
        pthread_mutex_unlock(&mutexCursor);
      /* Se il comando è di tipo add e l'oggetto da aggiungere è un player allora */
    } else if(!strcmp("player", type)){
        currRow = atoi(strtok_r(NULL, " ", &saveptr));
        currColumn = atoi(strtok_r(NULL, " ", &saveptr));
        /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
        pthread_mutex_lock(&mutexCursor);
        setCursor(false);
        moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
        setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
        printf("%s", SYMBOL_PLAYER);
        reset();
        fflush(stdout);
        moveto(cursor.posX, cursor.posY);
        setCursor(true);
        pthread_mutex_unlock(&mutexCursor);
      /* Se il comando è di tipo add e l'oggetto da aggiungere è un wall allora */
    } else if(!strcmp("wall", type)){
        currRow = atoi(strtok_r(NULL, " ", &saveptr));
        currColumn = atoi(strtok_r(NULL, " ", &saveptr));
        /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
        pthread_mutex_lock(&mutexCursor);
        setCursor(false);
        moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
        setColor(GRAPHICS_FG_COLOR_BLUE, GRAPHICS_BG_COLOR_BLACK);
        printf("%s", SYMBOL_WALL);
        reset();
        fflush(stdout);
        moveto(cursor.posX, cursor.posY);
        setCursor(true);
        pthread_mutex_unlock(&mutexCursor);
      /* Se il comando è di tipo add e l'oggetto da aggiungere è un blink allora */
    } else if(!strcmp("blink", type)){
        rowToBlink = atoi(strtok_r(NULL, " ", &saveptr));
        colToBlink = atoi(strtok_r(NULL, " ", &saveptr));
        locationToBlink = strtok_r(NULL, " ", &saveptr);
        tidBlink = pthread_self();
        bool status = true;
        while(true){
          pthread_mutex_lock(&mutexCursor);
          setCursor(false);
          moveto(field[rowToBlink][colToBlink].posX, field[rowToBlink][colToBlink].posY);
          if(status){
            setColor(GRAPHICS_FG_COLOR_MAGENTA, GRAPHICS_BG_COLOR_WHITE);
          } else{
            setColor(GRAPHICS_FG_COLOR_WHITE, GRAPHICS_BG_COLOR_MAGENTA);
          }
          printf("%s", locationToBlink);
          reset();
          moveto(cursor.posX, cursor.posY);
          setCursor(true);
          status = !status;
          pthread_mutex_unlock(&mutexCursor);
          sleep(1);
       }
    }
   /* Se il comando è di tipo update allora */
 } else if(!strcmp("$update", cmd)){
     currRow = atoi(strtok_r(NULL, " ", &saveptr));
     currColumn = atoi(strtok_r(NULL, " ", &saveptr));
     /* Pattern creato per gestire la stampa e il movimento del cursore dove stampare. */
     pthread_mutex_lock(&mutexCursor);
     setCursor(false);
     moveto(field[currRow][currColumn].posX, field[currRow][currColumn].posY);
     setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
     printf("%s", SYMBOL_GRASS);
     reset();
     fflush(stdout);
     moveto(cursor.posX, cursor.posY);
     setCursor(true);
     pthread_mutex_unlock(&mutexCursor);
 }

 free(msg);

 }


 /**
   * @param void.
   *
   * @return: void.
   *
   * La funzione cleanTextField ripulisce la text field
   * (write:).
   *
   */
 void cleanTextField(void){
  pthread_mutex_lock(&mutexCursor);
  moveto(textField.posX, textField.posY);
  printf("                                                                           │    │");
  fflush(stdout);
  pthread_mutex_unlock(&mutexCursor);
  goToTheTextField();
 }


 /**
   * @param void.
   *
   * @return: void.
   *
   * La funzione goToTheTextField muove il cursore sulla
   * text field (Write: ).
   *
   */
 void goToTheTextField(void){
  pthread_mutex_lock(&mutexCursor);
  cursor.posX = textField.posX;
  moveto(textField.posX, textField.posY);
  pthread_mutex_unlock(&mutexCursor);
 }


 /**
   * @param arg: -> NULL.
   *
   * @return: puntatore a void.
   *
   * La funzione listenerTextField si mette un ascolto di input
   * dalla text field (Write:). L'input "catturato" verrà  poi
   * processato da un'apposita funzione.
   *
   * Time wasted: 7h 49m >.<
   *
   */
 void* listenerTextField(void* arg){

  pthread_t tidUpdateChat;
  char msg[GRAPHICS_TEXTFIELD_STRLEN];
  char msgUser[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  char keyTyped;
  int currMsgCharacter = 0;

  while(true){
    currMsgCharacter = 0;
    while((keyTyped = getch()) != GRAPHICS_KEY_ENTER){
      if(isAlphabet(keyTyped)){
        if(keyTyped != GRAPHICS_KEY_BACKSPACE){
          if(currMsgCharacter < GRAPHICS_TEXTFIELD_STRLEN){
            msg[currMsgCharacter] = keyTyped;
            currMsgCharacter += 1;
            pthread_mutex_lock(&mutexCursor);
            cursor.posX += 1;
            printf("%c", keyTyped);
            fflush(stdout);
            pthread_mutex_unlock(&mutexCursor);
          } else{
              beep();
          }
        } else if(keyTyped == GRAPHICS_KEY_BACKSPACE && currMsgCharacter != 0){
            msg[currMsgCharacter] = '\0';
            pthread_mutex_lock(&mutexCursor);
            cursor.posX -= 1;
            moveto(cursor.posX, cursor.posY);
            printf(" ");
            moveto(cursor.posX, cursor.posY);
            fflush(stdout);
            pthread_mutex_unlock(&mutexCursor);
            currMsgCharacter -= 1;
        } else if(keyTyped == GRAPHICS_KEY_BACKSPACE && currMsgCharacter == 0){
            beep();
        }
      } else{
          beep();
      }
    }
    msg[currMsgCharacter] = '\0';
    cleanTextField();

    if(trimMsg(msg) != 1){
      memset(msgUser, '\0', GRAPHICS_CHAT_WIDTH);
      strcat(msgUser, "Me: \0");
      strcat(msgUser, msg);
      if((msgChat = allocMsg(msgUser, GRAPHICS_FG_COLOR_YELLOW, false)) != NULL){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
      sendMsg(msg);
    }
    memset(msg, '\0', GRAPHICS_TEXTFIELD_STRLEN);
  }
 }


//______FUNCTION FOR MANAGMENT OPERATION CLIENT _________________________________________________________________

/**
  * @param keyTyped: Tasto premuto.
  *
  * @return: true se il carattere è consentito, false altrimenti.
  *
  * La funzione isAlphabet verifica se il carattere premuto
  * dall'utente è consentito.
  *
  */
 bool isAlphabet(char keyTyped){
  if((keyTyped >= 32 && keyTyped <= 255) || keyTyped == GRAPHICS_KEY_ENTER){
    return true;
  }

  return false;

 }


 /**
   * @param void.
   *
   * @return: 1 in caso di errore, 0 in caso successo.
   *
   * La funzione connection effettua la connessione al server
   * mediante le specifiche fornite dall'utente.
   *
   */
 int connection(void){
  int bytesReaded = 0;                               /* Numero di bytes letti dalla read        */
  int port;                                          /* Porta inserita dal'utente               */
  char addressBuffer[GRAPHICS_TEXTFIELD_STRLEN];     /* Buffer per l'indirizzo del server       */
  char portBuffer[GRAPHICS_TEXTFIELD_STRLEN];        /* Buffer per la porta del server          */
  bool error = false;                                /* Flag che indica la presenza di errori   */

  address.sin_family = AF_INET;
  resizeTerminal(GRAPHICS_SCREEN_HEIGHT, GRAPHICS_SCREEN_WIDTH);
  setDefaultBackground(GRAPHICS_BG_COLOR_WHITE);
  setDefaultForeground(GRAPHICS_FG_COLOR_BLACK);
  reset();

  do{
    clean();
    /* Riabilito il cursore nel caso fosse disabilitato */
    setCursor(true);
    /* Richiedo l'indirizzo del server */
    printf("\n » Inserire l'indirizzo del server: ");
    fflush(stdout);
    do{
      error = false;
      if((bytesReaded = read(STDIN_FILENO, addressBuffer, GRAPHICS_TEXTFIELD_STRLEN)) == -1){
        setColor(GRAPHICS_FG_COLOR_RED, 0);
        perror("\n    <!> Errore read");
        reset();
        return 1;
      }
      addressBuffer[bytesReaded-1] = '\0';
      /* Verifico se l'indirizzo fornito è valido */
      if(inet_aton(addressBuffer, &address.sin_addr) == 0){
        setColor(GRAPHICS_FG_COLOR_RED, 0);
        printf("\n    <!> Indirizzo non valido - riprovare: ");
        reset();
        fflush(stdout);
        error = true;
      } else{
          setColor(GRAPHICS_FG_COLOR_GREEN, 0);
          printf("\n    » Indirizzo %s inserito con successo.\n", addressBuffer);
          reset();
      }
     }while(error != false);

    /* Richiedo la porta del server */
    printf("\n » Inserire la porta del server: ");
    fflush(stdout);
    do{
      error = false;
      if((bytesReaded = read(STDIN_FILENO, portBuffer, GRAPHICS_TEXTFIELD_STRLEN)) == -1){
        setColor(GRAPHICS_FG_COLOR_RED, 0);
        perror("\n    <!> Errore read");
        reset();
        return 1;
      }
      portBuffer[bytesReaded] = '\0';
      /* Verifico se la porta fornita è valida */
      if((port = atoi(portBuffer)) == 0 || !(port >= 0 && port <= 65535)){
        setColor(GRAPHICS_FG_COLOR_RED, 0);
        printf("\n    <!> Porta non valida - riprovare: ");
        reset();
        fflush(stdout);
        error = true;
      } else{
          setColor(GRAPHICS_FG_COLOR_GREEN, 0);
          printf("\n    » Porta %d inserita con successo.\n", port);
          reset();
          address.sin_port = htons(atoi(portBuffer));
      }
     }while(error != false);

      /* Provo ad effettuare la connessione al server */
      if(connect(clientSocket, (struct sockaddr*)&address, sizeof(address)) == -1){
        setColor(GRAPHICS_FG_COLOR_RED, 0);
        perror("\n <!> Errore connect, impossibile raggiungere il server - riprovare.");
        reset();
        error = true;
      } else{
          connected = true;
          setColor(GRAPHICS_FG_COLOR_BLUE, 0);
          printf("\n » Connessione al server [ADDR: %s - PORT: %d] effettuata con successo.\n", addressBuffer, port);
          reset();
      }

      /* Disabilito il cursore */
      setCursor(false);
      sleep(1);

   }while(error != false);

   return 0;
 }


 /**
   * @param msg[]: Messaggio da regolare.
   *
   * @return: 1 se il messaggio ottenuto è vuoto 0 altrimenti.
   *
   * La funzione trimMsg elimina eventuali spazi iniziali
   * e di coda e inoltre verifica se il messaggio è vuoto oppure
   * no.
   *
   */
 int trimMsg(char msg[]){

  int index = strlen(msg)-1;
  int tmp = 0;
  int len;

  /* Elimino gli spazi finali */
  while(msg[index] == ' '){
    msg[index] = '\0';
    index -= 1;
  }

  /* Elimino gli spazi inziali */
  index = 0;
  while(msg[index] == ' '){
    index += 1;
  }
  if(index != 0){
    tmp = 0;
    while(msg[tmp+index] != '\0'){
      msg[tmp] = msg[tmp+index];
      tmp++;
    }
    msg[tmp] = '\0';
  }

  /* $ è l'intestazione dei messaggi del Server */
  while(msg[0] == SERVER_NOTIFY){
    index = 0;
    for(index=0; index<strlen(msg)-1; index++){
      msg[index] = msg[index+1];
    }
    msg[index] = '\0';
  }

  /* Verifico se la stringa ottenuto è vuota */
  len = strlen(msg);
  if(len == 0){
    return 1;
  }

  return 0;

 }


 /**
   * @param msg: Messaggio da scrivere sulla chat.
   * @param colorPair: Colore con cui stampare il messaggio.
   * @param error: Indica se il messaggio Ã¨ di errore.
   *
   * @return: una struttura di tipo LpMsg.
   *
   * La funzione allocMsg alloca un nodo Msg per il thread
   * updateChat.
   *
   */
 LpMsg allocMsg(char* msg, int color, bool error){
  LpMsg msgChat;
  if((msgChat = (LpMsg)malloc(sizeof(Msg))) != NULL){
    strcpy(msgChat->msg, msg);
    msgChat->color = color;
    msgChat->error = error;
  }
  return msgChat;

 }


 /**
   * @param msg: messaggio da inoltrare al server.
   *
   * @return: void.
   *
   * La funzione sendMsg inoltra il messaggio catturato
   * dal listenerTextField al server.
   *
   */
 void sendMsg(char* msg){

  pthread_t tidUpdateChat;
  char buffer[GRAPHICS_TEXTFIELD_STRLEN];
  memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
  strcpy(buffer, msg);

  write(clientSocket, buffer, GRAPHICS_TEXTFIELD_STRLEN);

 }


 /**
   * @param arg -> NULL.
   *
    * @return: puntatore a void.
    *
    * La funzione listenerServer si mette un ascolto di messaggi
    * dal Server.
    *
    */
 void* listenerServer(void* arg){

  pthread_t tidUpdateChat;
  LpMsg msgChat;
  char incomingMsg[GRAPHICS_CHAT_WIDTH];
  int bytesReaded;
  int action;
  int index;

  memset(incomingMsg, '\0', GRAPHICS_CHAT_WIDTH);

  while((bytesReaded = read(clientSocket, incomingMsg, GRAPHICS_CHAT_WIDTH)) > 0){
    incomingMsg[bytesReaded] = '\0';
    if(incomingMsg[0] == SERVER_NOTIFY){
      if((action = takeAction(incomingMsg)) == 0){
        trimMsg(incomingMsg);
        if((msgChat = allocMsg(incomingMsg, GRAPHICS_FG_COLOR_BLUE, false)) != NULL){
          pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
        }
       } else if(action == -1){
           if((msgChat = allocMsg("<!> Errore calloc: impossibile allocare memoria.\0", GRAPHICS_FG_COLOR_RED, true)) != NULL){
             pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
           }
           sleep(2);
           return NULL;
        }
    } else if(incomingMsg[0] == SESSION_NOTIFY){
        index = 0;
        for(index=0; index<strlen(incomingMsg)-1; index++){
          incomingMsg[index] = incomingMsg[index+1];
        }
        incomingMsg[index] = '\0';

        if((msgChat = allocMsg(incomingMsg, GRAPHICS_FG_COLOR_MAGENTA, false)) != NULL){
          pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
        }

    } else{
        if((msgChat = allocMsg(incomingMsg, GRAPHICS_FG_COLOR_BLACK, false)) != NULL){
          pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
        }
    }
    memset(incomingMsg, '\0', GRAPHICS_CHAT_WIDTH);
  }

  /* In caso di fallimento esco */
  if((msgChat = allocMsg("Server: Sei stato disconnesso dal server...\0", GRAPHICS_FG_COLOR_RED, false)) != NULL){
    pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
  }

  return NULL;
 }


 /**
   * @param msg: Messaggio in entrata.
   *
   * @return: 1 nel caso il messaggio richiedeva azioni, 0 altrimenti, -1 in caso di errore.
   *
   * La funzione takeAction verifica se il messaggio in entrata
   * richiede di effettuare delle determinate azioni. In caso
   * affermativo provvede ad effettuarle.
   *
   */
 int takeAction(char msg[]){

  pthread_t tid;                      /* TID del thread che gestisce il comando        */
  char* dynamicBuffer;                /* Messaggio da passare ai thread                */
  char* saveptr;
  char buffer[GRAPHICS_CHAT_WIDTH];   /* Buffer utilizzato per la tokenization         */
  int action;

  if((dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char))) == NULL){
    return -1;
  }

  strcpy(dynamicBuffer, msg);
  strcpy(buffer, msg);
  char* cmd = strtok_r(buffer, " ", &saveptr);

  /* Estraggo la prima stringa del messaggio (comando) e verifico che comando sia */
  if(!strcmp(cmd, "$time")){
    int* time = (int*)malloc(sizeof(int));
    *time = atoi(strtok_r(NULL, " ", &saveptr));
    pthread_create(&tid, NULL, updateTimer, time);
    return 1;
  } else if(!strcmp(cmd, "$add") || !strcmp(cmd, "$update")){
      pthread_create(&tid, NULL, updateField, dynamicBuffer);
      return 1;
  } else if(!strcmp(cmd, "$disconnected") || !strcmp(cmd, "$connected") || !strcmp(cmd, "$delivered")){
      pthread_create(&tid, NULL, updatePlayersInfo, dynamicBuffer);
      return 1;
  } else if(!strcmp(cmd, "$remove")){
      char* type = strtok_r(NULL, " ", &saveptr);
      if(!strcmp(type, "blink")){
        if(tidBlink != 0){
          pthread_cancel(tidBlink);
          pthread_mutex_lock(&mutexCursor);
          setCursor(false);
          moveto(field[rowToBlink][colToBlink].posX, field[rowToBlink][colToBlink].posY);
          setColor(GRAPHICS_FG_COLOR_WHITE, GRAPHICS_BG_COLOR_MAGENTA);
          printf("%s", locationToBlink);
          reset();
          moveto(cursor.posX, cursor.posY);
          setCursor(true);
          pthread_mutex_unlock(&mutexCursor);
          tidBlink = 0;
        }
     }
     return 1;
  } else if(!strcmp(cmd, "$reset")){
      pthread_mutex_lock(&mutexCursor);
      setCursor(false);
      for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
        for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
          moveto(field[i][j].posX, field[i][j].posY);
          setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
          printf(" ");
          reset();
        }
      }
      moveto(cursor.posX, cursor.posY);
      setCursor(true);
      pthread_mutex_unlock(&mutexCursor);
      return 1;
  }
  return 0;

 }


 /**
   * @param signal: segnale catturato
   *
   * @return void.
   *
   * La funzione signalHandler cattura i segnali in questione
   * e ne detta il comportamento al loro verificarsi.
   *
   */
 void signalHandler(int signal){
  switch(signal){
    case SIGINT:
      sendMsg("@exit");
      break;
    case SIGSTOP:
      sendMsg("@exit");
    break;
  }
 }


/******************************************** END CLIENT ********************************************/
