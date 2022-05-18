
/******************************************** INIT SERVER ********************************************/          

//______LIBRARIES______________________________________________________________________________________

  #include "../libs/session/session.h"
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
  #define LISTENER_QUEUE_STRLEN        50
  #define INCOMING_MSG_STRLEN          70
  #define OUTCOMING_MSG_STRLEN         82
  #define TIME                        600
  #define BUFFER_STRLEN               150

  //______GLOBAL VARIABLES_______________________________________________________________________________

  Coord timer;
  Coord status[GRAPHICS_STATUS_HEIGHT];
  Coord field[GRAPHICS_FIELD_HEIGHT][GRAPHICS_FIELD_WIDTH];
  PlayersCounter playersCounter;
  PlayersInfo playersInfo;
  Chat chat;
  LpSession session = NULL;
  int listenerSocket;
  int listenerPort;

  int logs;
  int database;
  int fieldGenerator;

  LpClientInfo listClientInfo = NULL;
  LpClientInfoToJoin headClientInfoToJoinQueue = NULL;
  LpClientInfoToJoin tailClientInfoToJoinQueue = NULL;

  int sessionNumOfPackets = 0;

  int idSession = 0;
  int totalConnections = 0;
  int totalDisconnections = 0;
  int waitingClients = 0;
  int connectedClients = 0;
  int difficulty = 0;

  pthread_mutex_t mutexCursor = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexClientInfo = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexDatabase = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexUpdate = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexTimer = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t mutexLogs = PTHREAD_MUTEX_INITIALIZER;

  pthread_cond_t condMatchmaker = PTHREAD_COND_INITIALIZER;
  pthread_cond_t condSession = PTHREAD_COND_INITIALIZER;

  //______FUNCTIONS DECLARATION__________________________________________________________________________

  int initGUI(char*, char*);
  int mapping(char*);
  int initFile(void);
  int connection(void);
  void* updateChat(void*);
  void* connectionRequestsManagement(void*);
  void* listenerClient(void*);
  void* noecho(void*);
  LpMsg allocMsg(char*, int, bool);
  void sendMsg(LpClientInfo, char*);
  void sendMsgToAll(LpClientInfo, char*);
  void sendMsgToSession(LpClientInfo, char*);
  bool login(LpClientInfo);
  bool signin(LpClientInfo);
  int initField(void);
  void shuffleLocations(char[], int);
  void shufflePackets(Packet[], int);
  void shuffleSpawn(Spawn[], int);
  int initSession(void);
  void* sessionManagement(void*);
  void* timeProvider(void*);
  void* matchmaker(void*);
  void sendBasicInformation(LpClientInfo);
  void disconnectionManagement(LpClientInfo);
  void movePlayer(LpClientInfo, char[]);
  void actionPlayer(LpClientInfo, char[]);
  void signalHandler(int);
  void* updatePlayersInfo(void*);



  int main(void){

    pthread_t tidConnectionRequestsManagement;
    pthread_t tidSessionManagement;

    srand(time(NULL));

    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, SIG_IGN);

    if(initFile()){
      beep();
      setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_WHITE);
      printf("\n <!> Errore durante l'inizializzazione del Server.\n\n");
      reset();
      return 1;
    }

    if(connection()){
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

    pthread_create(&tidConnectionRequestsManagement, NULL, connectionRequestsManagement, NULL);

    while(true){
      if(initSession()){
        char msg[GRAPHICS_CHAT_WIDTH];
        LpMsg msgChat;
        pthread_t tidUpdateChat;
        sprintf(msg, "Errore durante l'inizializzazione della sessione!");
        if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true)) != NULL){
          pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
        }
        pthread_join(tidUpdateChat, NULL);
        raise(SIGINT);
      }
      pthread_create(&tidSessionManagement, NULL, sessionManagement, NULL);
      pthread_join(tidSessionManagement, NULL);
    }

    return 0;

  }

//______FUNCTION FOR MANAGMENT GRAPHICS AND ACTION CLIENT___________________________________________________

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
  pthread_t tidNoEcho;
  pthread_t tidUpdateChat;
  LpMsg msgChat;
  char msg[GRAPHICS_CHAT_WIDTH];

  setCursor(false);
  pthread_create(&tidNoEcho, NULL, noecho, NULL);

  if(mapping(blueprint)){
    return 1;
  }

  if((guiFile = open(gui, O_RDONLY)) == -1){
    return 1;
  }

  clean();
  while((bytesReaded = read(guiFile, basicComponent, 1)) > 0){
    basicComponent[bytesReaded] = '\0';
    printf("%s", basicComponent);
  }

  sprintf(msg, "Sono in ascolto sulla porta %d!", listenerPort);
  if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_BLUE, false)) != NULL){
    pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
  }

  pthread_mutex_lock(&mutexCursor);
  moveto(status[0].posX, status[0].posY);
  printf(" \u25cf Porta: %10d", listenerPort);
  moveto(status[1].posX, status[1].posY);
  printf(" \u25cf ID Sessione: %4.2d", idSession);
  moveto(status[2].posX, status[2].posY);
  printf(" \u25cf Connected: %6.2d", connectedClients);
  moveto(status[3].posX, status[3].posY);
  printf(" \u25cf Waiting: %8.2d", waitingClients);
  moveto(status[4].posX, status[4].posY);
  printf(" \u25cf Total CN: %7.2d", totalConnections);
  moveto(status[5].posX, status[5].posY);
  printf(" \u25cf Total DC: %7.2d", totalDisconnections);
  moveto(status[6].posX, status[6].posY);
  printf(" \u25cf Difficoltà: %5.2d", difficulty);
  pthread_mutex_unlock(&mutexCursor);

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
  int currStatusPosition = 0;

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
      case GRAPHICS_PIN_STATUS:
        status[currStatusPosition].posX = currColsScreen;
        status[currStatusPosition].posY = currRowsScreen;
        currStatusPosition += 1;
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
   * @param arg: NULL.
   *
   * @return: return 1 in caso di errore e 0 altrimenti.
   *
   * La funzione noecho blocca l'input utente.
   *
  */
 void* noecho(void* arg){
  while(true){
    getch();
  }
 }


 /**
   * @param void.
   *
   * @return: 1 in caso di errore 0 altrimenti
   *
   * la funzione initField legge i valori dal file fieldGenerator
   * e inizializza i valori della matrici con quelli letti dal file.
   *
   */
 int initField(void){

  int bytesReaded;
  char character[1];
  int currRows = 0;
  int curCols = 0;
  int gameDifficulty;
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  char logsBuffer[BUFFER_STRLEN];

  lseek(fieldGenerator, 0, SEEK_SET);

  do{
    /*Leggo carattere per caratter il file fieldGenerator per inizializzare la matrice */
    if((bytesReaded = read(fieldGenerator, character, 1)) > 0){
      if(character[0] != '\n'){
        session->field[curCols][currRows++] = atoi(character);
      } else{
          curCols++;
          currRows=0;
      }
    }
  }while(bytesReaded != 0);

  if(bytesReaded == -1){
    return 1;
  } else{

      char locationName[5] = {'A', 'B', 'C', 'D', 'E'};
      int currLocation = 0;
      int currPacket = 0;
      int currSpawn = 0;
      int wallSpawnPercentage = 0;

      /* Assegno la percentuale si spawn di muri a seconda della difficoltà*/
      if(difficulty == 0){
        gameDifficulty = rand()%10+1;
        pthread_mutex_lock(&mutexCursor);
        moveto(status[6].posX, status[6].posY);
        printf(" \u25cf Difficoltà: %5.2d", gameDifficulty);
        pthread_mutex_unlock(&mutexCursor);
      } else{
          gameDifficulty = difficulty;
      }
      switch (gameDifficulty){
        case 0:
          wallSpawnPercentage = rand()%10+1;
        break;
        case 1:
          wallSpawnPercentage = 1;
        break;
        case 2:
          wallSpawnPercentage = 2;
        break;
        case 3:
          wallSpawnPercentage = 3;
        break;
        case 4:
          wallSpawnPercentage = 4;
        break;
        case 5:
          wallSpawnPercentage = 5;
        break;
        case 6:
          wallSpawnPercentage = 6;
        break;
        case 7:
          wallSpawnPercentage = 7;
        break;
        case 8:
          wallSpawnPercentage = 8;
        break;
        case 9:
          wallSpawnPercentage = 9;
        break;
        case 10:
          wallSpawnPercentage = 10;
        break;
     }
     sprintf(msg, "Difficoltà della nuova sessione: %d.", gameDifficulty);
     if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_BLUE, false))){
       pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
     }

    pthread_mutex_lock(&mutexLogs);
    sprintf(logsBuffer, "Difficoltà della sessione [%d]: %d\n.", idSession, gameDifficulty);
    if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
      if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
    }
    pthread_mutex_unlock(&mutexLogs);
   /*Inizializzo i valori e le posizioni di ogni oggetto nella matrice della sessione*/
    for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
      for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
        switch (session->field[i][j]) {
          case 1:
            if(rand()%10+1 < wallSpawnPercentage){
              session->field[i][j] = SESSION_WALL_VALUE;
            } else{
                session->field[i][j] = SESSION_GRASS_VALUE;
            }
          break;
          case 2:
            session->field[i][j] = SESSION_LOCATION_VALUE;
            session->locations[currLocation].currRows = i;
            session->locations[currLocation].currCols = j;
            currLocation += 1;
          break;
          case 3:
            session->field[i][j] = SESSION_PACKET_VALUE;
            session->packets[currPacket].currRows = i;
            session->packets[currPacket].currCols = j;
            session->packets[currPacket].location = rand()%5;
            currPacket += 1;
          break;
          case 4:
            session->spawn[currSpawn].currRows = i;
            session->spawn[currSpawn].currCols = j;
            currSpawn += 1;
          break;
        }
     }

   }

   /* Randomizzo il numero di packet che verrà stampato*/
   session->numOfPackets = rand()%18+4;
   sessionNumOfPackets = session->numOfPackets;
   /* Randomizzo le posizioni delle locazioni da  stampare */
   shuffleLocations(locationName, SESSION_LOCATIONS_STRLEN);
   for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
     session->locations[i].name = locationName[i];
   }

   /* Randomizzo la posizione dei packet da stampare */
   shufflePackets(session->packets, SESSION_PACKETS_STRLEN);
   for(int i=session->numOfPackets; i<SESSION_PACKETS_STRLEN; i++){
     session->field[session->packets[i].currRows][session->packets[i].currCols] = SESSION_GRASS_VALUE;
   }
   /* Randomizzo la posizione degli spawn da stampare */
   for(int i=0; i<SESSION_SPAWN_STRLEN; i++){
     session->field[session->spawn[i].currRows][session->spawn[i].currCols] = SESSION_GRASS_VALUE;
   }
  }
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

  pthread_mutex_lock(&mutexUpdate);

  if(chat.currMsgPosition == GRAPHICS_CHAT_HEIGHT-1 && chat.msgBox[chat.currMsgPosition].isEmpty == false){
    for(int currMsg = 0; currMsg < GRAPHICS_CHAT_HEIGHT-1; currMsg++){
      strcpy(chat.msgBox[currMsg].msg, chat.msgBox[currMsg+1].msg);
      chat.msgBox[currMsg].color = chat.msgBox[currMsg+1].color;
    }
    strcpy(chat.msgBox[GRAPHICS_CHAT_HEIGHT-1].msg, msg);
    chat.msgBox[GRAPHICS_CHAT_HEIGHT-1].color = color;
    for(int currMsg = 0; currMsg < GRAPHICS_CHAT_HEIGHT; currMsg++){
      pthread_mutex_lock(&mutexCursor);
      moveto(chat.msgBox[currMsg].coords.posX, chat.msgBox[currMsg].coords.posY);
      printf("                                                                                  │    │");
      fflush(stdout);
      moveto(chat.msgBox[currMsg].coords.posX, chat.msgBox[currMsg].coords.posY);
      setColor(chat.msgBox[currMsg].color, 0);
      printf("%s", chat.msgBox[currMsg].msg);
      fflush(stdout);
      reset();
      pthread_mutex_unlock(&mutexCursor);
    }
   } else{
       chat.msgBox[chat.currMsgPosition].isEmpty = false;
       chat.msgBox[chat.currMsgPosition].color = color;
       strcpy(chat.msgBox[chat.currMsgPosition].msg, msg);
       pthread_mutex_lock(&mutexCursor);
       moveto(chat.msgBox[chat.currMsgPosition].coords.posX, chat.msgBox[chat.currMsgPosition].coords.posY);
       printf("                                                                                  │    │");
       fflush(stdout);
       moveto(chat.msgBox[chat.currMsgPosition].coords.posX, chat.msgBox[chat.currMsgPosition].coords.posY);
       setColor(color, 0);
       printf("%s", msg);
       fflush(stdout);
       reset();
       pthread_mutex_unlock(&mutexCursor);

      if(chat.currMsgPosition != GRAPHICS_CHAT_HEIGHT-1){
        chat.currMsgPosition += 1;
      }
  }
  pthread_mutex_unlock(&mutexUpdate);

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
   * La funzione timeProvider aggiorna e stampa il timer di fine
   * partita.
   *
   */
 void* timeProvider(void* arg){

  int totalSeconds = TIME;
  int seconds;
  int minutes;
  char msg[GRAPHICS_CHAT_WIDTH];
  bool finished = false;

  alarm(TIME);

  /* Ciclo finche il tempo non è scaduto o i tutti packet sono stati consegnati nelle locazioni */
  while((totalSeconds = alarm(0)) >= 0 && sessionNumOfPackets > 0 && !finished){
    if(totalSeconds == 0){
      finished = true;
    }

    pthread_mutex_lock(&mutexTimer);
    alarm(totalSeconds);
    sprintf(msg, "$time %d", totalSeconds);
    sendMsgToSession(NULL, msg);
    pthread_mutex_unlock(&mutexTimer);
    pthread_mutex_lock(&mutexCursor);
    /* Calcolo minuti */
    minutes = totalSeconds / 60;
    /* Calcolo secondi */
    seconds = totalSeconds % 60;
    /* Muovo il cursore sulla posizione del timer e stampo il tempo ad ogni ciclo del while  */
    /* che cambia colore ad ogni range di secondi .                                          */
    moveto(timer.posX, timer.posY);
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
    pthread_mutex_unlock(&mutexCursor);

    sleep(1);
  }
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

  char* msg = (char*)arg;
  char* saveptr;
  char* cmd = strtok_r(msg, " ", &saveptr);
  char* username;
  int packetsDelivered;
  char buffer[GRAPHICS_TEXTFIELD_STRLEN];
  int focusedPlayer = 0;

  pthread_mutex_lock(&mutexUpdate);
  if(!strcmp(cmd, "$disconnected") && playersCounter.value > 0){
    username = strtok_r(NULL, " ", &saveptr);
    playersCounter.value -= 1;
    pthread_mutex_lock(&mutexCursor);
    moveto(playersCounter.coords.posX, playersCounter.coords.posY);
    printf("%d", playersCounter.value);
    fflush(stdout);
    pthread_mutex_unlock(&mutexCursor);

    while(strcmp(username, playersInfo.playerBox[focusedPlayer].username) != 0){
      focusedPlayer += 1;
    }

    for(int player = focusedPlayer; player < playersInfo.currPlayerPosition; player++){
      strcpy(playersInfo.playerBox[player].username, playersInfo.playerBox[player+1].username);
      playersInfo.playerBox[player].packetsDelivered = playersInfo.playerBox[player+1].packetsDelivered;
      pthread_mutex_lock(&mutexCursor);
      moveto(playersInfo.playerBox[player].coords.posX, playersInfo.playerBox[player].coords.posY);
      printf("                     │    │");
      fflush(stdout);
      memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
      moveto(playersInfo.playerBox[player].coords.posX, playersInfo.playerBox[player].coords.posY);
      sprintf(buffer, " \u25ba %-13s[%.2d] ", playersInfo.playerBox[player].username, playersInfo.playerBox[player].packetsDelivered);
      printf("%s", buffer);
      fflush(stdout);
      pthread_mutex_unlock(&mutexCursor);
    }

    pthread_mutex_lock(&mutexCursor);
    moveto(playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posX, playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posY);
    printf("                     │    │");
    fflush(stdout);
    playersInfo.currPlayerPosition -= 1;
    pthread_mutex_unlock(&mutexCursor);

  } else if(!strcmp(cmd, "$connected")){
      username = strtok_r(NULL, " ", &saveptr);
      packetsDelivered = atoi(strtok_r(NULL, " ", &saveptr));
      memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
      sprintf(buffer, " \u25ba %-13s[%.2d] ", username, packetsDelivered);
      playersCounter.value += 1;
      pthread_mutex_lock(&mutexCursor);
      moveto(playersCounter.coords.posX, playersCounter.coords.posY);
      printf("%d", playersCounter.value);
      fflush(stdout);
      pthread_mutex_unlock(&mutexCursor);
      playersInfo.currPlayerPosition += 1;
      strcpy(playersInfo.playerBox[playersInfo.currPlayerPosition].username, username);
      playersInfo.playerBox[playersInfo.currPlayerPosition].packetsDelivered = packetsDelivered;
      pthread_mutex_lock(&mutexCursor);
      moveto(playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posX, playersInfo.playerBox[playersInfo.currPlayerPosition].coords.posY);
      printf("%s", buffer);
      fflush(stdout);
      pthread_mutex_unlock(&mutexCursor);
  } else if(!strcmp(cmd, "$delivered")){
      username = strtok_r(NULL, " ", &saveptr);

      while(strcmp(username, playersInfo.playerBox[focusedPlayer].username) != 0){
        focusedPlayer += 1;
      }

      playersInfo.playerBox[focusedPlayer].packetsDelivered += 1;
      memset(buffer, '\0', GRAPHICS_TEXTFIELD_STRLEN);
      sprintf(buffer, " \u25ba %-13s[%.2d] ", username, playersInfo.playerBox[focusedPlayer].packetsDelivered);
      pthread_mutex_lock(&mutexCursor);
      moveto(playersInfo.playerBox[focusedPlayer].coords.posX, playersInfo.playerBox[focusedPlayer].coords.posY);
      printf("                     │    │");
      fflush(stdout);
      moveto(playersInfo.playerBox[focusedPlayer].coords.posX, playersInfo.playerBox[focusedPlayer].coords.posY);
      printf("%s", buffer);
      fflush(stdout);
      pthread_mutex_unlock(&mutexCursor);
  }
  pthread_mutex_unlock(&mutexUpdate);
  free(arg);

 }


 /**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   *
   * @return:void.
   *
   * La funzione sendBasicInformation si occupa di mandare a sendMsg
   * le informazioni riguardo alle posizioni dove stampare gli oggetti
   * della mappa al client e alla stampa sulla grafica del server
   * dell'oggetto del client entranto in game.
   *
   */
 void sendBasicInformation(LpClientInfo clientInfo){

  char msg[GRAPHICS_CHAT_WIDTH];
  int spawnIndex = 0;

  /* Mando al client le posizioni dei packet che poi stamperà graficamente sulla sua mappa */
  for(int i=0; i<session->numOfPackets; i++){
    if(!session->packets[i].delivered){
      sprintf(msg, "$add packet %d %d", session->packets[i].currRows, session->packets[i].currCols);
      sendMsg(clientInfo, msg);
    }
  }

  /* Mando al client le posizioni e il nome delle locazioni che poi stamperà graficamente sulla sua mappa */
  for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
    sprintf(msg, "$add location %d %d %c", session->locations[i].currRows, session->locations[i].currCols, session->locations[i].name);
    sendMsg(clientInfo, msg);
  }

  /* Randomizzo gli spawn */
  shuffleSpawn(session->spawn, SESSION_SPAWN_STRLEN);

  /* Cerco uno spawn libero */
  while(spawnIndex < SESSION_SPAWN_STRLEN && session->field[session->spawn[spawnIndex].currRows][session->spawn[spawnIndex].currCols] != SESSION_GRASS_VALUE){
    spawnIndex += 1;
  }

  /* Assegno alla posizione dello spawn presa il valore del player */
  session->field[session->spawn[spawnIndex].currRows][session->spawn[spawnIndex].currCols] = SESSION_PLAYER_VALUE;

  pthread_mutex_lock(&mutexClientInfo);
  /* Assegno i valori della posizione di spawn del player nell'apposita struttura che contiene le informazioni riguardo al client */
  clientInfo->currRows = session->spawn[spawnIndex].currRows;
  clientInfo->currCols = session->spawn[spawnIndex].currCols;
  pthread_mutex_unlock(&mutexClientInfo);
  /* Invio a tutti gli altri client la posizione del player appena entrato in modo da aggiornare la mappa */
  sprintf(msg, "$add player %d %d", session->spawn[spawnIndex].currRows, session->spawn[spawnIndex].currCols);
  sendMsgToSession(clientInfo, msg);
  pthread_mutex_lock(&mutexCursor);
  moveto(field[session->spawn[spawnIndex].currRows][session->spawn[spawnIndex].currCols].posX, field[session->spawn[spawnIndex].currRows][session->spawn[spawnIndex].currCols].posY);
  setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
  printf("%s", SYMBOL_PLAYER);
  reset();
  pthread_mutex_unlock(&mutexCursor);
  /* Mando al client in questione la sua posizione in modo che aggiorni la sua mappa */
  sprintf(msg, "$add you %d %d", session->spawn[spawnIndex].currRows, session->spawn[spawnIndex].currCols);
  sendMsg(clientInfo, msg);

  /* Mando al client in questione la posizione di tutti i player in game per aggiornare la sua mappa */
  for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
    if(session->clients[i] != NULL && session->clients[i] != clientInfo){
      sprintf(msg, "$add player %d %d", session->clients[i]->currRows, session->clients[i]->currCols);
      sendMsg(clientInfo, msg);
    }
  }
}


 /**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   * @param incomingMsg: array che contiene il comando utilizzato dal client.
   *
   * @return: void.
   *
   * La funzione movePlayer si occupa di gestire i movimenti del client
   * all'interno della mappa.
   *
   */
 void movePlayer(LpClientInfo clientInfo, char incomingMsg[]){

  LpClientInfo clientToJoin;
  char msg[GRAPHICS_CHAT_WIDTH];

  /* Verifico che il comando sia di tipo @S */
  if(!strcmp(incomingMsg, "@S")){
    if(clientInfo->currRows+1 == GRAPHICS_FIELD_HEIGHT){
      sendMsg(clientInfo, "$Server: Attento, stavi per cadere giù!");
    } else{
        /* Se il comando è @S verifico cosa si trova nella posizione al suo sud */
        switch(session->field[clientInfo->currRows+1][clientInfo->currCols]){
          /* Se c'è dell erba allora sposto il player di una posizione verso sud e invio il messaggio con le posizioni aggiornate ai client in sessione */
          case SESSION_GRASS_VALUE:
            pthread_mutex_lock(&(session->mutexSession));
            session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_GRASS_VALUE;
            session->field[clientInfo->currRows+1][clientInfo->currCols] = SESSION_PLAYER_VALUE;
            pthread_mutex_unlock(&(session->mutexSession));
            clientInfo->currRows += 1;
            sprintf(msg, "$add you %d %d", clientInfo->currRows, clientInfo->currCols);
            sendMsg(clientInfo, msg);
            sprintf(msg, "$add player %d %d", clientInfo->currRows, clientInfo->currCols);
            sendMsgToSession(clientInfo, msg);
            sprintf(msg, "$update %d %d", clientInfo->currRows-1, clientInfo->currCols);
            sendMsgToSession(NULL, msg);
            pthread_mutex_lock(&mutexCursor);
            moveto(field[clientInfo->currRows-1][clientInfo->currCols].posX, field[clientInfo->currRows-1][clientInfo->currCols].posY);
            setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
            printf("%s", SYMBOL_GRASS);
            moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
            setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
            printf("%s", SYMBOL_PLAYER);
            reset();
            pthread_mutex_unlock(&mutexCursor);
          break;
          /* Se c'è il muro invio al client il messaggio di presenza del muro , e delle cordinate del muro per poter aggiornare la sua mappa */
          case SESSION_WALL_VALUE:
            sendMsg(clientInfo, "$Server: Ouch! Hai sbattuto contro un muro!");
            sprintf(msg, "$add wall %d %d", clientInfo->currRows+1, clientInfo->currCols);
            sendMsg(clientInfo, msg);
          break;
           /* Se c'è una locazione invio un messaggio al client di presenza di locazione */
          case SESSION_LOCATION_VALUE:
            sendMsg(clientInfo, "$Server: Hei, hai un pacco per me?");
          break;
          /* Se c'è un packet invio un messaggio al client di presenza del packet */
          case SESSION_PACKET_VALUE:
            sendMsg(clientInfo, "$Server: Wow! Hai trovato un pacco, prendilo!");
          break;
          /* Se c'è un player invio un messaggio al client della presenza di un player */
          case SESSION_PLAYER_VALUE:
            sendMsg(clientInfo, "$Server: Alt! Un altro giocatore ti impedisce il passaggio!");
          break;
        }
   }
   /* Verifico che il comando sia di tipo @N */
 } else if(!strcmp(incomingMsg, "@N")){
     if(clientInfo->currRows-1 == -1){
       sendMsg(clientInfo, "$Server: Attento, stavi per cadere giù!");
     } else{
         /* Se il comando è @N verifico cosa si trova nella posizione al suo nord */
         switch(session->field[clientInfo->currRows-1][clientInfo->currCols]){
           /* Se c'è dell erba allora sposto il player di una posizione verso nord e invio il messaggio con le posizioni aggiornate ai client in sessione */
           case SESSION_GRASS_VALUE:
             pthread_mutex_lock(&(session->mutexSession));
             session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_GRASS_VALUE;
             session->field[clientInfo->currRows-1][clientInfo->currCols] = SESSION_PLAYER_VALUE;
             pthread_mutex_unlock(&(session->mutexSession));
             clientInfo->currRows -= 1;
             sprintf(msg, "$add you %d %d", clientInfo->currRows, clientInfo->currCols);
             sendMsg(clientInfo, msg);
             sprintf(msg, "$add player %d %d", clientInfo->currRows, clientInfo->currCols);
             sendMsgToSession(clientInfo, msg);
             sprintf(msg, "$update %d %d", clientInfo->currRows+1, clientInfo->currCols);
             sendMsgToSession(NULL, msg);
             pthread_mutex_lock(&mutexCursor);
             moveto(field[clientInfo->currRows+1][clientInfo->currCols].posX, field[clientInfo->currRows+1][clientInfo->currCols].posY);
             setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
             printf("%s", SYMBOL_GRASS);
             moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
             setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
             printf("%s", SYMBOL_PLAYER);
             reset();
             pthread_mutex_unlock(&mutexCursor);
           break;
           /* Se c'è il muro invio al client il messaggio di presenza del muro , e delle cordinate del muro per poter aggiornare la sua mappa */
          case SESSION_WALL_VALUE:
            sendMsg(clientInfo, "$Server: Ouch! Hai sbattuto contro un muro!");
            sprintf(msg, "$add wall %d %d", clientInfo->currRows-1, clientInfo->currCols);
            sendMsg(clientInfo, msg);
          break;
          /* Se c'è una locazione invio un messaggio di presenza di locazione */
          case SESSION_LOCATION_VALUE:
            sendMsg(clientInfo, "$Server: Hei, hai un pacco per me?");
          break;
          /* Se c'è un packet invio un messaggio al client di presenza del packet */
          case SESSION_PACKET_VALUE:
            sendMsg(clientInfo, "$Server: Wow! Hai trovato un pacco, prendilo!");
          break;
          /* Se c'è un player invio un messaggio al client della presenza di un player */
          case SESSION_PLAYER_VALUE:
            sendMsg(clientInfo, "$Server: Alt! Un altro giocatore ti impedisce il passaggio!");
          break;
       }
   }
   /* Verifico che il comando sia di tipo @E */
 } else if(!strcmp(incomingMsg, "@E")){
     if(clientInfo->currCols+1 == GRAPHICS_FIELD_WIDTH){
       sendMsg(clientInfo, "$Server: Attento, stavi per cadere giù!");
     } else{
         /* Se il comando è @N verifico cosa si trova nella posizione al suo est */
         switch(session->field[clientInfo->currRows][clientInfo->currCols+1]){
           /* Se c'è dell erba allora sposto il player di una posizione verso est e invio il messaggio con le posizioni aggiornate ai client in sessione */
           case SESSION_GRASS_VALUE:
             pthread_mutex_lock(&(session->mutexSession));
             session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_GRASS_VALUE;
             session->field[clientInfo->currRows][clientInfo->currCols+1] = SESSION_PLAYER_VALUE;
             pthread_mutex_unlock(&(session->mutexSession));
             clientInfo->currCols += 1;
             sprintf(msg, "$add you %d %d", clientInfo->currRows, clientInfo->currCols);
             sendMsg(clientInfo, msg);
             sprintf(msg, "$add player %d %d", clientInfo->currRows, clientInfo->currCols);
             sendMsgToSession(clientInfo, msg);
             sprintf(msg, "$update %d %d", clientInfo->currRows, clientInfo->currCols-1);
             sendMsgToSession(NULL, msg);
             pthread_mutex_lock(&mutexCursor);
             moveto(field[clientInfo->currRows][clientInfo->currCols-1].posX, field[clientInfo->currRows][clientInfo->currCols-1].posY);
             setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
             printf("%s", SYMBOL_GRASS);
             moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
             setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
             printf("%s", SYMBOL_PLAYER);
             reset();
             pthread_mutex_unlock(&mutexCursor);
           break;
           /* Se c'è il muro invio al client il messaggio di presenza del muro , e delle cordinate del muro per poter aggiornare la sua mappa */
           case SESSION_WALL_VALUE:
             sendMsg(clientInfo, "$Server: Ouch! Hai sbattuto contro un muro!");
             sprintf(msg, "$add wall %d %d", clientInfo->currRows, clientInfo->currCols+1);
             sendMsg(clientInfo, msg);
           break;
           /* Se c'è una locazione invio un messaggio di presenza di locazione */
           case SESSION_LOCATION_VALUE:
             sendMsg(clientInfo, "$Server: Hei, hai un pacco per me?");
           break;
           /* Se c'è un packet invio un messaggio al client di presenza del packet */
           case SESSION_PACKET_VALUE:
             sendMsg(clientInfo, "$Server: Wow! Hai trovato un pacco, prendilo!");
           break;
           /* Se c'è un player invio un messaggio al client della presenza di un player */
           case SESSION_PLAYER_VALUE:
             sendMsg(clientInfo, "$Server: Alt! Un altro giocatore ti impedisce il passaggio!");
           break;
        }
     }
    /* Verifico che il comando sia di tipo @O */
  } else if(!strcmp(incomingMsg, "@O")){
      if(clientInfo->currCols-1 == -1){
        sendMsg(clientInfo, "$Server: Attento, stavi per cadere giù!");
      } else{
          /* Se il comando è @N verifico cosa si trova nella posizione al suo ovest */
          switch(session->field[clientInfo->currRows][clientInfo->currCols-1]){
            /* Se c'è dell erba allora sposto il player di una posizione verso ovest e invio il messaggio con le posizioni aggiornate ai client in sessione */
            case SESSION_GRASS_VALUE:
              pthread_mutex_lock(&(session->mutexSession));
              session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_GRASS_VALUE;
              session->field[clientInfo->currRows][clientInfo->currCols-1] = SESSION_PLAYER_VALUE;
              pthread_mutex_unlock(&(session->mutexSession));
              clientInfo->currCols -= 1;
              sprintf(msg, "$add you %d %d", clientInfo->currRows, clientInfo->currCols);
              sendMsg(clientInfo, msg);
              sprintf(msg, "$add player %d %d", clientInfo->currRows, clientInfo->currCols);
              sendMsgToSession(clientInfo, msg);
              sprintf(msg, "$update %d %d", clientInfo->currRows, clientInfo->currCols+1);
              sendMsgToSession(NULL, msg);
              pthread_mutex_lock(&mutexCursor);
              moveto(field[clientInfo->currRows][clientInfo->currCols+1].posX, field[clientInfo->currRows][clientInfo->currCols+1].posY);
              setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
              printf("%s", SYMBOL_GRASS);
              moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
              setColor(GRAPHICS_FG_COLOR_BLACK, GRAPHICS_BG_COLOR_GREEN);
              printf("%s", SYMBOL_PLAYER);
              reset();
              pthread_mutex_unlock(&mutexCursor);
           break;
           /* Se c'è il muro invio al client il messaggio di presenza del muro , e delle cordinate del muro per poter aggiornare la sua mappa */
           case SESSION_WALL_VALUE:
             sendMsg(clientInfo, "$Server: Ouch! Hai sbattuto contro un muro!");
             sprintf(msg, "$add wall %d %d", clientInfo->currRows, clientInfo->currCols-1);
             sendMsg(clientInfo, msg);
           break;
           /* Se c'è una locazione invio un messaggio di presenza di locazione */
           case SESSION_LOCATION_VALUE:
             sendMsg(clientInfo, "$Server: Hei, hai un pacco per me?");
           break;
           /* Se c'è un packet invio un messaggio al client di presenza del packet */
           case SESSION_PACKET_VALUE:
             sendMsg(clientInfo, "$Server: Wow! Hai trovato un pacco, prendilo!");
           break;
           /* Se c'è un player invio un messaggio al client della presenza di un player */
           case SESSION_PLAYER_VALUE:
             sendMsg(clientInfo, "$Server: Alt! Un altro giocatore ti impedisce il passaggio!");
           break;
       }
     }
  }
 }


 /**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   * @param incomingMsg: array che contiene il comando utilizzato dal client.
   *
   * @return: void.
   *
   * La funzione actionPlayer si occupa di gestire il comando di presa e
   * di deposito del pacco da parte del client .
   *
   */
 void actionPlayer(LpClientInfo clientInfo, char incomingMsg[]){

  LpClientInfo clientToJoin;
  char msg[GRAPHICS_CHAT_WIDTH];
  int currRows;
  int currCols;
  char locationName;
  int location;
  char* dynamicBuffer;
  pthread_t tidUpdatePlayersInfo;
  time_t timestamp;
  char logsBuffer[BUFFER_STRLEN];
  pthread_t tidUpdateChat;
  LpMsg msgChat;

  /* Controllo se il comando è di tipo P */
  if(incomingMsg[1] == 'P'){
    switch (incomingMsg[2]) {
      /* Se la posizione dove prendere è S */
      case 'S':
        /*Se non è presente un pacco nella posizione verso sud allora invio un messaggio al client della non presenza del packet */
        if(clientInfo->currRows+1 == GRAPHICS_FIELD_HEIGHT || session->field[clientInfo->currRows+1][clientInfo->currCols] != SESSION_PACKET_VALUE){
          sendMsg(clientInfo, "$Server: Non c'è nulla qui!");
        } else{

            if(session->field[clientInfo->currRows+1][clientInfo->currCols] == SESSION_PACKET_VALUE){
              /* Verifico se il client è gia in possesso di un packet se si invio un messaggio di segnalazione */
              if(clientInfo->havePacket != -1){
                sendMsg(clientInfo, "$Server: Non essere avido! Hai già un pacchetto.");
              } else{
                 /* Altrimenti se non possiede packet aggiorno il valore ad erba e invio un messaggio ai client per aggiornare la mappa */
                  pthread_mutex_lock(&(session->mutexSession));
                  session->field[clientInfo->currRows+1][clientInfo->currCols] = SESSION_GRASS_VALUE;
                  pthread_mutex_unlock(&(session->mutexSession));
                  sprintf(msg, "$update %d %d", clientInfo->currRows+1, clientInfo->currCols);
                  sendMsgToSession(NULL, msg);
                  pthread_mutex_lock(&mutexCursor);
                  moveto(field[clientInfo->currRows+1][clientInfo->currCols].posX, field[clientInfo->currRows+1][clientInfo->currCols].posY);
                  setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
                  printf("%s", SYMBOL_GRASS);
                  reset();
                  pthread_mutex_unlock(&mutexCursor);

                  /* Ricerco la locazione dove portare il packet preso dal client */
                  for(int i=0; i<session->numOfPackets; i++){
                    if(session->packets[i].currRows == clientInfo->currRows+1 && session->packets[i].currCols == clientInfo->currCols){
                      clientInfo->havePacket = i;
                      session->packets[i].delivered = true;
                      currRows = session->locations[session->packets[i].location].currRows;
                      currCols = session->locations[session->packets[i].location].currCols;
                      locationName = session->locations[session->packets[i].location].name;
                      break;
                    }
                  }

                  /* Invio le informazione riguardo alla locazione al client per permettere di aggiornare la mappa */
                  sprintf(msg, "$add blink %d %d %c", currRows, currCols, locationName);
                  sendMsg(clientInfo, msg);
                  sprintf(msg, "$Server: Trasporta il pacchetto fino alla locazione %c!", locationName);
                  sendMsg(clientInfo, msg);
              }
            }
         }
      break;
      /* Se la posizione dove prendere è N */
      case 'N':
        /*Se non è presente un pacco nella posizione verso sud allora invio un messaggio al client della non presenza del packet */
        if(clientInfo->currRows-1 == -1 || session->field[clientInfo->currRows-1][clientInfo->currCols] != SESSION_PACKET_VALUE){
          sendMsg(clientInfo, "$Server: Non c'è nulla qui!");
        } else{
            if(session->field[clientInfo->currRows-1][clientInfo->currCols] == SESSION_PACKET_VALUE){
              /* Verifico se il client è gia in possesso di un packet se si invio un messaggio di segnalazione */
              if(clientInfo->havePacket != -1){
                sendMsg(clientInfo, "$Server: Non essere avido! Hai già un pacchetto.");
              } else{
                /* Altrimenti se non possiede packet aggiorno il valore ad erba e invio un messaggio ai client per aggiornare la mappa */
                pthread_mutex_lock(&(session->mutexSession));
                session->field[clientInfo->currRows-1][clientInfo->currCols] = SESSION_GRASS_VALUE;
                pthread_mutex_unlock(&(session->mutexSession));
                sprintf(msg, "$update %d %d", clientInfo->currRows-1, clientInfo->currCols);
                sendMsgToSession(NULL, msg);
                pthread_mutex_lock(&mutexCursor);
                moveto(field[clientInfo->currRows-1][clientInfo->currCols].posX, field[clientInfo->currRows-1][clientInfo->currCols].posY);
                setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
                printf("%s", SYMBOL_GRASS);
                reset();
                pthread_mutex_unlock(&mutexCursor);

                /* Ricerco la locazione dove portare il packet preso dal client */
                for(int i=0; i<session->numOfPackets; i++){
                  if(session->packets[i].currRows == clientInfo->currRows-1 && session->packets[i].currCols == clientInfo->currCols){
                    clientInfo->havePacket = i;
                    session->packets[i].delivered = true;
                    currRows = session->locations[session->packets[i].location].currRows;
                    currCols = session->locations[session->packets[i].location].currCols;
                    locationName = session->locations[session->packets[i].location].name;
                    break;
                  }
                }

                /* Invio le informazione riguardo alla locazione al client per permettere di aggiornare la mappa */
                sprintf(msg, "$add blink %d %d %c", currRows, currCols, locationName);
                sendMsg(clientInfo, msg);
                sprintf(msg, "$Server: Trasporta il pacchetto fino alla locazione %c!", locationName);
                sendMsg(clientInfo, msg);
              }
            }
          }
      break;
      /* Se la posizione dove prendere è E */
      case 'E':
        /*Se non è presente un pacco nella posizione verso sud allora invio un messaggio al client della non presenza del packet */
        if(clientInfo->currCols+1 == GRAPHICS_FIELD_WIDTH || session->field[clientInfo->currRows][clientInfo->currCols+1] != SESSION_PACKET_VALUE){
          sendMsg(clientInfo, "$Server: Non c'è nulla qui!");
        } else{
            if(session->field[clientInfo->currRows][clientInfo->currCols+1] == SESSION_PACKET_VALUE){
              /* Verifico se il client è gia in possesso di un packet se si invio un messaggio di segnalazione */
              if(clientInfo->havePacket != -1){
                sendMsg(clientInfo, "$Server: Non essere avido! Hai già un pacchetto.");
              } else{
                  /* Altrimenti se non possiede packet aggiorno il valore ad erba e invio un messaggio ai client per aggiornare la mappa */
                  pthread_mutex_lock(&(session->mutexSession));
                  session->field[clientInfo->currRows][clientInfo->currCols+1] = SESSION_GRASS_VALUE;
                  pthread_mutex_unlock(&(session->mutexSession));
                  sprintf(msg, "$update %d %d", clientInfo->currRows, clientInfo->currCols+1);
                  sendMsgToSession(NULL, msg);
                  pthread_mutex_lock(&mutexCursor);
                  moveto(field[clientInfo->currRows][clientInfo->currCols+1].posX, field[clientInfo->currRows][clientInfo->currCols+1].posY);
                  setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
                  printf("%s", SYMBOL_GRASS);
                  reset();
                  pthread_mutex_unlock(&mutexCursor);

                  /* Ricerco la locazione dove portare il packet preso dal client */
                  for(int i=0; i<session->numOfPackets; i++){
                    if(session->packets[i].currRows == clientInfo->currRows && session->packets[i].currCols == clientInfo->currCols+1){
                      clientInfo->havePacket = i;
                      session->packets[i].delivered = true;
                      currRows = session->locations[session->packets[i].location].currRows;
                      currCols = session->locations[session->packets[i].location].currCols;
                      locationName = session->locations[session->packets[i].location].name;
                      break;
                    }
                  }

                  /* Invio le informazione riguardo alla locazione al client per permettere di aggiornare la mappa */
                  sprintf(msg, "$add blink %d %d %c", currRows, currCols, locationName);
                  sendMsg(clientInfo, msg);
                  sprintf(msg, "$Server: Trasporta il pacchetto fino alla locazione %c!", locationName);
                  sendMsg(clientInfo, msg);
               }
             }
          }
      break;
      /* Se la posizione dove prendere è O */
      case 'O':
        /*Se non è presente un pacco nella posizione verso sud allora invio un messaggio al client della non presenza del packet */
        if(clientInfo->currCols-1 == -1 || session->field[clientInfo->currRows][clientInfo->currCols-1] != SESSION_PACKET_VALUE){
          sendMsg(clientInfo, "$Server: Non c'è nulla qui!");
        } else{
          if(session->field[clientInfo->currRows][clientInfo->currCols-1] == SESSION_PACKET_VALUE){
            /* Verifico se il client è gia in possesso di un packet se si invio un messaggio di segnalazione */
            if(clientInfo->havePacket != -1){
              sendMsg(clientInfo, "$Server: Non essere avido! Hai già un pacchetto.");
            } else{
                /* Altrimenti se non possiede packet aggiorno il valore ad erba e invio un messaggio ai client per aggiornare la mappa */
                pthread_mutex_lock(&(session->mutexSession));
                session->field[clientInfo->currRows][clientInfo->currCols-1] = SESSION_GRASS_VALUE;
                pthread_mutex_unlock(&(session->mutexSession));
                sprintf(msg, "$update %d %d", clientInfo->currRows, clientInfo->currCols-1);
                sendMsgToSession(NULL, msg);
                pthread_mutex_lock(&mutexCursor);
                moveto(field[clientInfo->currRows][clientInfo->currCols-1].posX, field[clientInfo->currRows][clientInfo->currCols-1].posY);
                setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
                printf("%s", SYMBOL_GRASS);
                reset();
                pthread_mutex_unlock(&mutexCursor);

                /* Ricerco la locazione dove portare il packet preso dal client */
                for(int i=0; i<session->numOfPackets; i++){
                  if(session->packets[i].currRows == clientInfo->currRows && session->packets[i].currCols == clientInfo->currCols-1){
                    clientInfo->havePacket = i;
                    session->packets[i].delivered = true;
                    currRows = session->locations[session->packets[i].location].currRows;
                    currCols = session->locations[session->packets[i].location].currCols;
                    locationName = session->locations[session->packets[i].location].name;
                    break;
                  }
                }

                /* Invio le informazione riguardo alla locazione al client per permettere di aggiornare la mappa */
                sprintf(msg, "$add blink %d %d %c", currRows, currCols, locationName);
                sendMsg(clientInfo, msg);
                sprintf(msg, "$Server: Trasporta il pacchetto fino alla locazione %c!", locationName);
                sendMsg(clientInfo, msg);
              }
            }
          }
      break;
    }
    /* Verifico se il comando è di tipo D */
  } else if(incomingMsg[1] == 'D'){
      /* Se la posizione dove prendere è S */
      switch (incomingMsg[2]) {
        case 'S':
          /* Verifico che il client sia in possesso di un packet, in caso negativo invio al client un messaggio di segnalazione */
          if(clientInfo->havePacket == -1){
            sendMsg(clientInfo, "$Server: Devi prima raccogliere un pacco!");
            /* Se possiede un pacco verifico se nella posizione verso sud sia presento un muro, in caso di esito positivo invio al client un messaggio di segnalazione */
          } else if(clientInfo->currRows+1 == GRAPHICS_FIELD_HEIGHT || (session->field[clientInfo->currRows+1][clientInfo->currCols] != SESSION_GRASS_VALUE
            && session->field[clientInfo->currRows+1][clientInfo->currCols] != SESSION_LOCATION_VALUE)){
              if(session->field[clientInfo->currRows+1][clientInfo->currCols] == SESSION_WALL_VALUE){
                sendMsg(clientInfo, "$Server: Non vorrai mica distruggere un muro con un pacchetto?!");
                sprintf(msg, "$add wall %d %d", clientInfo->currRows+1, clientInfo->currCols);
                sendMsg(clientInfo, msg);
                /* Se possiede un pacco verifico se nella posizione verso sud sia presento un player, in caso di esito positivo invio al client un messaggio di segnalazione */
              } else if(session->field[clientInfo->currRows+1][clientInfo->currCols] == SESSION_PLAYER_VALUE){
                  sendMsg(clientInfo, "$Server: Non puoi lanciare pacchetti contro gli altri giocatori!");
              } else{
                  sendMsg(clientInfo, "$Server: Non puoi posizionare il pacco qui!");
              }
         } else{
             /* Verifico se nella posizione verso sud sia presento dell'erba , in caso di esito invio un messaggio al client delle nuove posizione per poter aggiornare la mappa */
             if(session->field[clientInfo->currRows+1][clientInfo->currCols] == SESSION_GRASS_VALUE){
               pthread_mutex_lock(&(session->mutexSession));
               session->field[clientInfo->currRows+1][clientInfo->currCols] = SESSION_PACKET_VALUE;
               session->packets[clientInfo->havePacket].currRows = clientInfo->currRows+1;
               session->packets[clientInfo->havePacket].currCols = clientInfo->currCols;
               pthread_mutex_unlock(&(session->mutexSession));
               clientInfo->havePacket = -1;
               sendMsg(clientInfo, "$remove blink");
               sprintf(msg, "$add packet %d %d", clientInfo->currRows+1, clientInfo->currCols);
               sendMsgToSession(NULL, msg);
               pthread_mutex_lock(&mutexCursor);
               moveto(field[clientInfo->currRows+1][clientInfo->currCols].posX, field[clientInfo->currRows+1][clientInfo->currCols].posY);
               setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
               printf("%s", SYMBOL_PACKET);
               reset();
               pthread_mutex_unlock(&mutexCursor);
               /* Verifico se nella posizione verso sud sia presento una locazione */
          } else if(session->field[clientInfo->currRows+1][clientInfo->currCols] == SESSION_LOCATION_VALUE){
              for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
                if(session->locations[i].currRows == clientInfo->currRows+1 && session->locations[i].currCols == clientInfo->currCols){
                  location = i;
                  break;
                }
              }
              /* Controllo se la locazione è quella indicata dal packet, in caso di esito negativo invio al client un messaggio di segnalazione */
              if(session->packets[clientInfo->havePacket].location != location){
                sendMsg(clientInfo, "$Server: Attento! Hai sbagliato locazione, riprova!");
              } else{
                /* In caso di esito positivo aggiorno il numero di pacchi consegnati dal client e invio un messaggio alla sessione di avvenuta consegna */
                  clientInfo->packetsDelivered += 1;
                  clientInfo->havePacket = -1;
                  sendMsg(clientInfo, "$remove blink");
                  sprintf(msg, "$delivered %s", clientInfo->username);
                  dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
                  strcpy(dynamicBuffer, msg);
                  pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
                  sessionNumOfPackets -= 1;
                  sendMsgToSession(NULL, msg);
                  sprintf(msg, "$Server: %s ha consegnato un pacco, complimenti!", clientInfo->username);
                  sendMsgToSession(NULL, msg);
                  timestamp = time(NULL);
                  pthread_mutex_lock(&mutexLogs);
                  sprintf(logsBuffer, " > [Sessione %d] Pacco consegnato da %s - %s", idSession, clientInfo->username, ctime(&timestamp));
                  if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
                    if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
                      pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
                    }
                  }
                  pthread_mutex_unlock(&mutexLogs);
               }
            }
          }
       break;
       /* Se la posizione dove prendere è N */
       case 'N':
         /* Verifico che il client sia in possesso di un packet, in caso negativo invio un messaggio di segnalazione */
         if(clientInfo->havePacket == -1){
           sendMsg(clientInfo, "$Server: Devi prima raccogliere un pacco!");
           /* Se possiede un pacco verifico se nella posizione verso sud sia presento un muro, in caso di esito positivo invioal client un messaggio di segnalazione */
         } else if(clientInfo->currRows-1 == -1 || (session->field[clientInfo->currRows-1][clientInfo->currCols] != SESSION_GRASS_VALUE
           && session->field[clientInfo->currRows-1][clientInfo->currCols] != SESSION_LOCATION_VALUE)){
             if(session->field[clientInfo->currRows-1][clientInfo->currCols] == SESSION_WALL_VALUE){
               sendMsg(clientInfo, "$Server: Non vorrai mica distruggere un muro con un pacchetto?!");
               sprintf(msg, "$add wall %d %d", clientInfo->currRows-1, clientInfo->currCols);
               sendMsg(clientInfo, msg);
               /* Se possiede un pacco verifico se nella posizione verso sud sia presento un player, in caso di esito positivo invio al client un messaggio di segnalazione */
             } else if(session->field[clientInfo->currRows-1][clientInfo->currCols] == SESSION_PLAYER_VALUE){
                 sendMsg(clientInfo, "$Server: Non puoi lanciare pacchetti contro gli altri giocatori!");
             } else{
                 sendMsg(clientInfo, "$Server: Non puoi posizionare il pacco qui!");
             }
         } else{
             /* Verifico se nella posizione verso sud sia presento dell'erba , in caso di esito invio un messaggio al client delle nuove posizione per poter aggiornare la mappa */
             if(session->field[clientInfo->currRows-1][clientInfo->currCols] == SESSION_GRASS_VALUE){
               pthread_mutex_lock(&(session->mutexSession));
               session->field[clientInfo->currRows-1][clientInfo->currCols] = SESSION_PACKET_VALUE;
               session->packets[clientInfo->havePacket].currRows = clientInfo->currRows-1;
               session->packets[clientInfo->havePacket].currCols = clientInfo->currCols;
               pthread_mutex_unlock(&(session->mutexSession));
               clientInfo->havePacket = -1;
               sendMsg(clientInfo, "$remove blink");
               sprintf(msg, "$add packet %d %d", clientInfo->currRows-1, clientInfo->currCols);
               sendMsgToSession(NULL, msg);
               pthread_mutex_lock(&mutexCursor);
               moveto(field[clientInfo->currRows-1][clientInfo->currCols].posX, field[clientInfo->currRows-1][clientInfo->currCols].posY);
               setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
               printf("%s", SYMBOL_PACKET);
               reset();
               pthread_mutex_unlock(&mutexCursor);
               /* Verifico se nella posizione verso sud sia presento una locazione */
             } else if(session->field[clientInfo->currRows-1][clientInfo->currCols] == SESSION_LOCATION_VALUE){
                 for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
                   if(session->locations[i].currRows == clientInfo->currRows-1 && session->locations[i].currCols == clientInfo->currCols){
                     location = i;
                     break;
                   }
                 }
                /* Controllo se la locazione è quella indicata dal packet, in caso di esito negativo invio al client un messaggio di segnalazione */
                if(session->packets[clientInfo->havePacket].location != location){
                  sendMsg(clientInfo, "$Server: Attento! Hai sbagliato locazione, riprova!");
                } else{
                    /* In caso di esito positivo aggiorno il numero di pacchi consegnati dal client e invio un messaggio alla sessione di avvenuta consegna */
                    clientInfo->packetsDelivered += 1;
                    clientInfo->havePacket = -1;
                    sendMsg(clientInfo, "$remove blink");
                    sprintf(msg, "$delivered %s", clientInfo->username);
                    dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
                    strcpy(dynamicBuffer, msg);
                    pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
                    sessionNumOfPackets -= 1;
                    sendMsgToSession(NULL, msg);
                    sprintf(msg, "$Server: %s ha consegnato un pacco, complimenti!", clientInfo->username);
                    sendMsgToSession(NULL, msg);
                    timestamp = time(NULL);
                    pthread_mutex_lock(&mutexLogs);
                    sprintf(logsBuffer, " > [Sessione %d] Pacco consegnato da %s - %s", idSession, clientInfo->username, ctime(&timestamp));
                    if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
                      if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
                        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
                      }
                    }
                    pthread_mutex_unlock(&mutexLogs);
                }
             }
          }
      break;
      /* Se la posizione dove prendere è E */
      case 'E':
        /* Verifico che il client sia in possesso di un packet, in caso negativo invio un messaggio di segnalazione */
        if(clientInfo->havePacket == -1){
          sendMsg(clientInfo, "$Server: Devi prima raccogliere un pacco!");
          /* Se possiede un pacco verifico se nella posizione verso sud sia presento un muro, in caso di esito positivo invio al client un messaggio di segnalazione */
        } else if(clientInfo->currCols+1 == GRAPHICS_FIELD_WIDTH || (session->field[clientInfo->currRows][clientInfo->currCols+1] != SESSION_GRASS_VALUE
          && session->field[clientInfo->currRows][clientInfo->currCols+1] != SESSION_LOCATION_VALUE)){
            if(session->field[clientInfo->currRows][clientInfo->currCols+1] == SESSION_WALL_VALUE){
              sendMsg(clientInfo, "$Server: Non vorrai mica distruggere un muro con un pacchetto?!");
              sprintf(msg, "$add wall %d %d", clientInfo->currRows, clientInfo->currCols+1);
              sendMsg(clientInfo, msg);
              /* Se possiede un pacco verifico se nella posizione verso sud sia presento un player, in caso di esito positivo invio al client un messaggio di segnalazione */
            } else if(session->field[clientInfo->currRows][clientInfo->currCols+1] == SESSION_PLAYER_VALUE){
                sendMsg(clientInfo, "$Server: Non puoi lanciare pacchetti contro gli altri giocatori!");
            } else{
               sendMsg(clientInfo, "$Server: Non puoi posizionare il pacco qui!");
            }
        } else{
            /* Verifico se nella posizione verso sud sia presento dell'erba , in caso di esito invio un messaggio al client delle nuove posizione per poter aggiornare la mappa */
            if(session->field[clientInfo->currRows][clientInfo->currCols+1] == SESSION_GRASS_VALUE){
              pthread_mutex_lock(&(session->mutexSession));
              session->field[clientInfo->currRows][clientInfo->currCols+1] = SESSION_PACKET_VALUE;
              session->packets[clientInfo->havePacket].currRows = clientInfo->currRows;
              session->packets[clientInfo->havePacket].currCols = clientInfo->currCols+1;
              pthread_mutex_unlock(&(session->mutexSession));
              clientInfo->havePacket = -1;
              sendMsg(clientInfo, "$remove blink");
              sprintf(msg, "$add packet %d %d", clientInfo->currRows, clientInfo->currCols+1);
              pthread_mutex_lock(&mutexCursor);
              moveto(field[clientInfo->currRows][clientInfo->currCols+1].posX, field[clientInfo->currRows][clientInfo->currCols+1].posY);
              setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
              printf("%s", SYMBOL_PACKET);
              reset();
              pthread_mutex_unlock(&mutexCursor);
              sendMsgToSession(NULL, msg);
              /* Verifico se nella posizione verso sud sia presento una locazione */
            } else if(session->field[clientInfo->currRows][clientInfo->currCols+1] == SESSION_LOCATION_VALUE){
                for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
                  if(session->locations[i].currRows == clientInfo->currRows && session->locations[i].currCols == clientInfo->currCols+1){
                    location = i;
                    break;
                  }
                }
                /* Controllo se la locazione è quella indicata dal packet, in caso di esito negativo invio al client un messaggio di segnalazione */
                if(session->packets[clientInfo->havePacket].location != location){
                sendMsg(clientInfo, "$Server: Attento! Hai sbagliato locazione, riprova!");
                } else{
                    /* In caso di esito positivo aggiorno il numero di pacchi consegnati dal client e invio un messaggio alla sessione di avvenuta consegna */
                    clientInfo->packetsDelivered += 1;
                    clientInfo->havePacket = -1;
                    sendMsg(clientInfo, "$remove blink");
                    sprintf(msg, "$delivered %s", clientInfo->username);
                    dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
                    strcpy(dynamicBuffer, msg);
                    pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
                    sessionNumOfPackets -= 1;
                    sendMsgToSession(NULL, msg);
                    sprintf(msg, "$Server: %s ha consegnato un pacco, complimenti!", clientInfo->username);
                    sendMsgToSession(NULL, msg);
                    timestamp = time(NULL);
                    pthread_mutex_lock(&mutexLogs);
                    sprintf(logsBuffer, " > [Sessione %d] Pacco consegnato da %s - %s", idSession, clientInfo->username, ctime(&timestamp));
                    if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
                      if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
                        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
                      }
                    }
                    pthread_mutex_unlock(&mutexLogs);
                 }
            }
         }
      break;
      /* Se la posizione dove prendere è O */
      case 'O':
        /* Verifico che il client sia in possesso di un packet, in caso negativo invio un messaggio di segnalazione */
        if(clientInfo->havePacket == -1){
          sendMsg(clientInfo, "$Server: Devi prima raccogliere un pacco!");
          /* Se possiede un pacco verifico se nella posizione verso sud sia presento un muro, in caso di esito positivo invio al client un messaggio di segnalazione */
        } else if(clientInfo->currCols-1 == -1 || (session->field[clientInfo->currRows][clientInfo->currCols-1] != SESSION_GRASS_VALUE
          && session->field[clientInfo->currRows][clientInfo->currCols-1] != SESSION_LOCATION_VALUE)){
            if(session->field[clientInfo->currRows][clientInfo->currCols-1] == SESSION_WALL_VALUE){
              sendMsg(clientInfo, "$Server: Non vorrai mica distruggere un muro con un pacchetto?!");
              sprintf(msg, "$add wall %d %d", clientInfo->currRows, clientInfo->currCols-1);
              sendMsg(clientInfo, msg);
              /* Se possiede un pacco verifico se nella posizione verso sud sia presento un player, in caso di esito positivo invio al client un messaggio di segnalazione */
            } else if(session->field[clientInfo->currRows][clientInfo->currCols-1] == SESSION_PLAYER_VALUE){
                sendMsg(clientInfo, "$Server: Non puoi lanciare pacchetti contro gli altri giocatori!");
            } else{
                sendMsg(clientInfo, "$Server: Non puoi posizionare il pacco qui!");
            }
        } else{
            /* Verifico se nella posizione verso sud sia presento dell'erba , in caso di esito invio un messaggio al client delle nuove posizione per poter aggiornare la mappa */
            if(session->field[clientInfo->currRows][clientInfo->currCols-1] == SESSION_GRASS_VALUE){
              pthread_mutex_lock(&(session->mutexSession));
              session->field[clientInfo->currRows][clientInfo->currCols-1] = SESSION_PACKET_VALUE;
              session->packets[clientInfo->havePacket].currRows = clientInfo->currRows;
              session->packets[clientInfo->havePacket].currCols = clientInfo->currCols-1;
              pthread_mutex_unlock(&(session->mutexSession));
              clientInfo->havePacket = -1;
              sendMsg(clientInfo, "$remove blink");
              sprintf(msg, "$add packet %d %d", clientInfo->currRows, clientInfo->currCols-1);
              sendMsgToSession(NULL, msg);
              pthread_mutex_lock(&mutexCursor);
              moveto(field[clientInfo->currRows][clientInfo->currCols-1].posX, field[clientInfo->currRows][clientInfo->currCols-1].posY);
              setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
              printf("%s", SYMBOL_PACKET);
              reset();
              pthread_mutex_unlock(&mutexCursor);
              /* Verifico se nella posizione verso sud sia presento una locazione */
            } else if(session->field[clientInfo->currRows][clientInfo->currCols-1] == SESSION_LOCATION_VALUE){
                for(int i=0; i<SESSION_LOCATIONS_STRLEN; i++){
                  if(session->locations[i].currRows == clientInfo->currRows && session->locations[i].currCols == clientInfo->currCols-1){
                    location = i;
                    break;
                  }
                }
                /* Controllo se la locazione è quella indicata dal packet, in caso di esito negativo invio al client un messaggio di segnalazione */
                if(session->packets[clientInfo->havePacket].location != location){
                sendMsg(clientInfo, "$Server: Attento! Hai sbagliato locazione, riprova!");
                } else{
                    /* In caso di esito positivo aggiorno il numero di pacchi consegnati dal client e invio un messaggio alla sessione di avvenuta consegna */
                    clientInfo->packetsDelivered += 1;
                    clientInfo->havePacket = -1;
                    sendMsg(clientInfo, "$remove blink");
                    sprintf(msg, "$delivered %s", clientInfo->username);
                    dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
                    strcpy(dynamicBuffer, msg);
                    pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
                    sessionNumOfPackets -= 1;
                    sendMsgToSession(NULL, msg);
                    sprintf(msg, "$Server: %s ha consegnato un pacco, complimenti!", clientInfo->username);
                    sendMsgToSession(NULL, msg);
                    timestamp = time(NULL);
                    pthread_mutex_lock(&mutexLogs);
                    sprintf(logsBuffer, " > [Sessione %d] Pacco consegnato da %s - %s", idSession, clientInfo->username, ctime(&timestamp));
                    if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
                      if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
                        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
                      }
                    }
                    pthread_mutex_unlock(&mutexLogs);
                }
             }
          }
      break;
    }
   }
 }


//______FUNCTION FOR MANAGMENT OPERATION CLIENT _________________________________________________________________


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

  int bytesReaded;
  char buffer[BUFFER_STRLEN];
  int port;
  bool error;
  struct sockaddr_in serverAddress;

  resizeTerminal(GRAPHICS_SCREEN_HEIGHT, GRAPHICS_SCREEN_WIDTH);
  setDefaultBackground(GRAPHICS_BG_COLOR_WHITE);
  setDefaultForeground(GRAPHICS_FG_COLOR_BLACK);
  reset();
  memset(&serverAddress, '0', sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  clean();
  printf("\n » Inserire la porta del server: ");
  fflush(stdout);

  do{
    setCursor(true);
    error = false;
    if((bytesReaded = read(STDIN_FILENO, buffer, BUFFER_STRLEN)) == -1){
      setColor(GRAPHICS_FG_COLOR_RED, 0);
      perror("\n    <!> Errore read");
      reset();
      return 1;
    }
    buffer[bytesReaded] = '\0';
    /* Verifico se la porta fornita è valida */
    for(int i=0; i<strlen(buffer)-1; i++){
      if(buffer[i] < '0' || buffer[i] > '9'){
        error = true;
        break;
      }
    }
    if(error){
      setColor(GRAPHICS_FG_COLOR_RED, 0);
      printf("\n   <!> Porta non valida, caratteri non ammessi - riprovare: ");
      reset();
      fflush(stdout);
    } else{
        if((port = atoi(buffer)) == 0 || !(port >= 0 && port <= 65535)){
          setColor(GRAPHICS_FG_COLOR_RED, 0);
          printf("\n   <!> Porta non valida, out of range - riprovare: ");
          reset();
          fflush(stdout);
          error = true;
        } else{
          listenerPort = port;
          setColor(GRAPHICS_FG_COLOR_GREEN, 0);
          setCursor(false);
          printf("\n    » Porta %d inserita con successo.\n", port);
          reset();
          fflush(stdout);
          serverAddress.sin_port = htons(atoi(buffer));
        }

        if(!error){
          listenerSocket = socket(PF_INET, SOCK_STREAM, 0);
          /* Assegno un'indirizzo alla socket del server */
          if(bind(listenerSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
             setColor(GRAPHICS_FG_COLOR_RED, 0);
             setCursor(false);
             perror("\n <!> Errore bind");
             puts("");
             reset();
             sleep(1);
             error = true;
          }

          /* Rimango in ascolto di richieste di connessione da parte di client */
          if(listen(listenerSocket, LISTENER_QUEUE_STRLEN) == -1){
             setColor(GRAPHICS_FG_COLOR_RED, 0);
             setCursor(false);
             perror("\n <!> Errore listen");
             puts("");
             reset();
             sleep(1);
             return 1;
          }
        }
    }
  }while(error != false);

  printf("\n » Inserire la difficoltà [0 <-> 10 => (0 = Random | 1 = No wall <-> 10 = Estrema)]: ");
  fflush(stdout);
  do{
    setCursor(true);
    error = false;
    if((bytesReaded = read(STDIN_FILENO, buffer, BUFFER_STRLEN)) == -1){
      setColor(GRAPHICS_FG_COLOR_RED, 0);
      perror("\n    <!> Errore read");
      reset();
      return 1;
    }

    buffer[bytesReaded] = '\0';

    for(int i=0; i<strlen(buffer)-1; i++){
      if(buffer[i] < '0' || buffer[i] > '9'){
        error = true;
        break;
      }
    }

    if(error){
      setColor(GRAPHICS_FG_COLOR_RED, 0);
      printf("\n   <!> Difficoltà non valida, caratteri non ammessi - riprovare: ");
      reset();
      fflush(stdout);
    } else{
        if(atoi(buffer) < 0 || atoi(buffer) > 10){
          setColor(GRAPHICS_FG_COLOR_RED, 0);
          printf("\n   <!> Difficoltà non valida, out of range - riprovare: ");
          reset();
          fflush(stdout);
          error = true;
        } else{
          difficulty = atoi(buffer);
          setColor(GRAPHICS_FG_COLOR_GREEN, 0);
          setCursor(false);
          if(difficulty != 0){
            printf("\n    » Difficoltà %d inserita con successo.\n", difficulty);
          } else{
            printf("\n    » Difficoltà randomica inserita con successo.\n");
          }
          reset();
          fflush(stdout);
          sleep(1);
      }
    }

  }while(error != false);
  return 0;

 }


 /**
   * @param arg : NULL.
   *
   * @return: puntatore a void.
   *
   * La funzione connectionRequestsManagement gestisce le operazione di
   * connessione alla socket del server da parte dei client.
   *
   */
 void* connectionRequestsManagement(void* arg){

  int connectSocket;
  pthread_t tidListenerClient;
  struct sockaddr_in clientAddress;
  char clientAddressIPv4[INET_ADDRSTRLEN];
  int clientAddressSize = sizeof(clientAddress);
  LpClientInfo clientInfo;
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  char logsBuffer[BUFFER_STRLEN];

  while(true){
    /* Accetto le richieste di connessione da parte dei client */
    if((connectSocket = accept(listenerSocket, (struct sockaddr*)&clientAddress, &clientAddressSize)) == -1){
      if((msgChat = allocMsg(" <!> Impossibile accettare una richiesta di connessione.", GRAPHICS_FG_COLOR_RED, true))){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
    } else{
        inet_ntop(AF_INET, &clientAddress, clientAddressIPv4, INET_ADDRSTRLEN);
        clientInfo = NULL;
    }
    /* Creao una struttura clientInfo che conterrà tutte le informazioni riguardo al client */
    if((clientInfo = newClientInfo(connectSocket, clientAddressIPv4, "")) == NULL){
      sprintf(msg, " <!> Impossibile allocare memoria, %s disconnesso.", clientAddressIPv4);
      if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true))){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
    } else{
        /* Aggiorno le strutture che contengono informazioni sullo status attuale */
        pthread_mutex_lock(&mutexClientInfo);
        insertClientInfo(&listClientInfo, clientInfo);
        pthread_mutex_unlock(&mutexClientInfo);
        /* Creao un thread che gestirà l'accesso del client al Server */
        pthread_create(&tidListenerClient, NULL, listenerClient, clientInfo);
        clientInfo->tidHandler = tidListenerClient;
        sprintf(msg, "Nuova connessione accettata: [Client: %s] - %s", clientAddressIPv4, ctime(&(clientInfo->timestamp)));
        totalConnections += 1;
        pthread_mutex_lock(&mutexCursor);
        moveto(status[4].posX, status[4].posY);
        printf(" \u25cf Total CN: %7.2d", totalConnections);
        pthread_mutex_unlock(&mutexCursor);
        connectedClients += 1;
        pthread_mutex_lock(&mutexCursor);
        moveto(status[2].posX, status[2].posY);
        printf(" \u25cf Connected: %6.2d", connectedClients);
        pthread_mutex_unlock(&mutexCursor);
        msg[strlen(msg)-1] = '\0';
        if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_GREEN, false))){
          pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
        }

        pthread_mutex_lock(&mutexLogs);
        sprintf(logsBuffer, " > Nuova connessione accettata: [Client: %s] - %s", clientAddressIPv4, ctime(&(clientInfo->timestamp)));
        if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
          if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
            pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
          }
        }
        pthread_mutex_unlock(&mutexLogs);

    }
    memset(msg, '\0', GRAPHICS_CHAT_WIDTH);
  }
 }


 /**
   * @param void.
   *
   * @return: return 1 in caso di errore e 0 altrimenti.
   *
   * La funzione initFile apre i file di logs, database
   * e di playground.
   *
  */
 int initFile(void){

  /* Apro il file di log */
  if((logs = open("../files/logs.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1){
    return 1;
  }

  /* Apro il file di database */
  if((database = open("../files/database.txt", O_RDWR | O_CREAT | O_APPEND, S_IRWXU)) == -1){
    return 1;
  }

  /* Apro il file generatore di playground */
  if((fieldGenerator = open("../files/field.txt", O_RDONLY)) == -1){
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
  if((msgChat = (Msg*)malloc(sizeof(Msg))) != NULL){
    strcpy(msgChat->msg, msg);
    msgChat->color = color;
    msgChat->error = error;
  }
  return msgChat;

 }


/**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   * @param outcomingMsg: Messaggio da scrivere da scrivere al client.
   *
   * @return: void.
   *
   * La funzione sendMsg inoltra il messaggio catturato
   * dal listenerTextField al client.
   *
   */
 void sendMsg(LpClientInfo clientInfo, char* outcomingMsg){

  char buffer[OUTCOMING_MSG_STRLEN];
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;

  memset(buffer, '\0', OUTCOMING_MSG_STRLEN);
  strcpy(buffer, outcomingMsg);
  pthread_mutex_lock(&(clientInfo->mutexSocket));
  write(clientInfo->clientSocket, buffer, OUTCOMING_MSG_STRLEN);
  pthread_mutex_unlock(&(clientInfo->mutexSocket));
  }


 /**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   * @param outcomingMsg: Messaggio da scrivere da scrivere al client.
   *
   * @return: void.
   *
   * La funzione sendMsgToSession inoltra il messaggio catturato
   * dal listenerTextField a tutti i client presenti nel game
   * della sessione.
   *
   */
 void sendMsgToSession(LpClientInfo clientInfo, char* outcomingMsg){
  pthread_mutex_lock(&mutexClientInfo);
  for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
    if(session->clients[i] != NULL && session->clients[i] != clientInfo){
      sendMsg(session->clients[i], outcomingMsg);
    }
  }
  pthread_mutex_unlock(&mutexClientInfo);

 }


 /**
   * @param clientInfo: puntatore della struttura che contiene informazioni
   * del client.
   * @param outcomingMsg: Messaggio da scrivere da scrivere al client..
   *
   * @return: void.
   *
   * La funzione sendMsgToAll manda il messaggio a tutti i client presenti
   * in sessione.
   *
   */
 void sendMsgToAll(LpClientInfo clientInfo, char* outcomingMsg){
  pthread_mutex_lock(&mutexClientInfo);
  LpClientInfo tmp = listClientInfo;
  while(tmp != NULL){
    if((tmp->status == CLTINF_LOGGED || tmp->status == CLTINF_PLAYING) && tmp != clientInfo){
      sendMsg(tmp, outcomingMsg);
    }
    tmp = tmp->nextClientInfo;
  }
  pthread_mutex_unlock(&mutexClientInfo);
 }


 /**
   * @param toShuffle: array di caratteri da randomizzare.
   * @param dim : dimensione dell'array toShuffle.
   *
   * @return: void.
   *
   * La funzione shuffleLocations si occupa di randomizzare
   * le posizioni delle locazioni.
   *
   */
 void shuffleLocations(char toShuffle[], int dim){
  int index = dim;
  while(index > 1){
    int indexShuffle = rand()%index;
    index -= 1;
    char tmp = toShuffle[indexShuffle];
    toShuffle[indexShuffle] = toShuffle[index];
    toShuffle[index] = tmp;
  }
 }


 /**
   * @param toShuffle: array di caratteri da randomizzare.
   * @param dim : dimensione dell'array toShuffle.
   *
   * @return: void.
   *
   * La funzione shufflePackets si occupa di randomizzare
   * le posizioni dei packets.
   *
   */
 void shufflePackets(Packet toShuffle[], int dim){
  int index = dim;
  while(index > 1){
    int indexShuffle = rand()%index;
    index -= 1;
    Packet tmp = toShuffle[indexShuffle];
    toShuffle[indexShuffle] = toShuffle[index];
    toShuffle[index] = tmp;
  }
 }


 /**
   * @param toShuffle: array di caratteri da randomizzare.
   * @param dim : dimensione dell'array toShuffle.
   *
   * @return: void.
   *
   * La funzione shuffleSpawn si occupa di randomizzare
   * le posizioni degli spawn dei player.
   *
   */
 void shuffleSpawn(Spawn toShuffle[], int dim){
  int index = dim;
  while(index > 1){
    int indexShuffle = rand()%index;
    index -= 1;
    Spawn tmp = toShuffle[indexShuffle];
    toShuffle[indexShuffle] = toShuffle[index];
    toShuffle[index] = tmp;
  }
 }


 /**
   * @param clientinfo: Nodo ClientInfo con informazioni sul client.
   *
   * @return: true in caso di errore, false altrimenti.
   *
   * La funzione login gestisce il login da parte
   * del client al Server.
   *
   */
 bool login(LpClientInfo clientInfo){

  bool passed = false;                                  /* Flag che indica il superamento di una fase */
  char incomingMsg[INCOMING_MSG_STRLEN];                /* Buffer per messaggi in entrata             */
  char record[GRAPHICS_CHAT_WIDTH];                           /* Buffer per la lettura di un record del DB  */
  char username[CLTINF_USERNAME_STRLEN];           /* Buffer per username del client             */
  char password[CLTINF_PASSWORD_STRLEN];           /* Buffer per password del client             */
  char incomingRecord[GRAPHICS_CHAT_WIDTH];                   /* Username + Password inseriti dall'utente   */
  int recordIndex;                                      /* Indice di posizione del buffer record      */
  char character[1];                                    /* Array per la lettura dei dati dal DB       */
  int bytesReaded;                                      /* Numero di bytes letti dalla read           */
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  bool alreadyLogged;
  LpClientInfoToJoin clientInfoToJoin;
  char logsBuffer[BUFFER_STRLEN];

  memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);

  do{
    sendMsg(clientInfo, "$Server: Inserisci l'username.");
    memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);

    do{
      passed = true;
      /* Leggo l'username inserito dal client */
      if((bytesReaded = read(clientInfo->clientSocket, incomingMsg, INCOMING_MSG_STRLEN)) <= 0){
        return false;
      }
      incomingMsg[bytesReaded] = '\0';

      if(!strcmp(incomingMsg, "@exit")){
        return true;
      } else{
          if(strlen(incomingMsg) > CLTINF_USERNAME_STRLEN){
            sendMsg(clientInfo, "$Server: Attenzione, username troppo lungo - [massimo 10 caratteri], riprovare.");
            passed = false;
          } else{
              for(int i=0; i<strlen(incomingMsg); i++){
                if(incomingMsg[i] == ' '){
                  sendMsg(clientInfo, "$Server: Attenzione, gli spazi non sono consentiti, riprovare.");
                  passed = false;
                  break;
                }
              }
             if(passed != false){
               strcpy(username, incomingMsg);
             }
        }
        memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
     }
   }while(passed != true);

   sendMsg(clientInfo, "$Server: Inserisci la password. ");
   memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);

   do{
     passed = true;
     /* Leggo la password inserita dal client */
     if((bytesReaded = read(clientInfo->clientSocket, incomingMsg, INCOMING_MSG_STRLEN)) <= 0){
       return false;
     }
     incomingMsg[bytesReaded] = '\0';

     /* Verifico se l'utente ha inserito il comando di uscita */
     if(!strcmp(incomingMsg, "@exit")){
       return true;
     } else{
         /* Controllo se la password supera la lunghezza di 10 caratteri stabiliti  */
         if(strlen(incomingMsg) > CLTINF_PASSWORD_STRLEN){
           sendMsg(clientInfo, "$Server: Attenzione, password troppo lunga - [massimo 10 caratteri], riprovare.");
           passed = false;
         } else{
            /* Controllo che nella password non siano presenti caratteri di spazio */
            for(int i=0; i<strlen(incomingMsg); i++){
              if(incomingMsg[i] == ' '){
                sendMsg(clientInfo, "$Server: Attenzione, gli spazi non sono consentiti, riprovare.");
                passed = false;
                break;
              }
            }
            if(passed != false){
              strcpy(password, incomingMsg);
            }
        }
        memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
     }
   }while(passed != true);

   memset(incomingRecord, '\0', CLTINF_USERNAME_STRLEN*2);
   sprintf(incomingRecord, "%s %s", username, password);
   pthread_mutex_lock(&mutexDatabase);
   lseek(database, 0, SEEK_SET);
   memset(record, '\0', GRAPHICS_CHAT_WIDTH);
   recordIndex = 0;

   /* Ciclo sul file database per verificare che lo username non esista già */
   do{
     /* La lettura avviene un carattere alla volta fintanto non viene trovato un newline */
     alreadyLogged = false;
     if((bytesReaded = read(database, character, 1)) > 0){
       /* Il carattere è diverso da un newline? */
       if(character[0] != '\n'){
         record[recordIndex++] = character[0];
       } else{
           if(!strcmp(record, incomingRecord)){
             pthread_mutex_lock(&mutexClientInfo);
             LpClientInfo tmp = listClientInfo;
             while(tmp != NULL && alreadyLogged == false){
               if((tmp->status == CLTINF_LOGGED || tmp->status == CLTINF_PLAYING) && !strcmp(tmp->username, username) && tmp != clientInfo){
                 sendMsg(clientInfo, "$Server: L'utente specificato è già connesso al Server.");
                 sleep(1);
                 alreadyLogged = true;
               }
               tmp = tmp->nextClientInfo;
             }
             pthread_mutex_unlock(&mutexClientInfo);

             if(alreadyLogged == false){
               clientInfo->status = CLTINF_LOGGED;
               strcpy(clientInfo->username, username);
               pthread_mutex_lock(&mutexClientInfo);
               clientInfoToJoin = newClientInfoToJoin(clientInfo);
               if(clientInfoToJoin != NULL){
                 enqueueClientInfoToJoin(&headClientInfoToJoinQueue, &tailClientInfoToJoinQueue, clientInfoToJoin);
                 waitingClients += 1;
                 pthread_mutex_lock(&mutexCursor);
                 moveto(status[3].posX, status[3].posY);
                 printf(" \u25cf Waiting: %8.2d", waitingClients);
                 pthread_mutex_unlock(&mutexCursor);
               } else{
                   return false;
               }
               pthread_mutex_unlock(&mutexClientInfo);
               sprintf(msg, "Login del client %s avvenuto con successo - %s", clientInfo->clientAddressIPv4, ctime(&(clientInfo->timestamp)));
               if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_GREEN, false))){
                 pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
               }

               pthread_mutex_lock(&mutexLogs);
               sprintf(logsBuffer, " > Login effettuato con successo: [Client: %s - %s] - %s", clientInfo->clientAddressIPv4, clientInfo->username, ctime(&(clientInfo->timestamp)));
               if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
                 if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
                   pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
                 }
               }
               pthread_mutex_unlock(&mutexLogs);
               sendMsg(clientInfo, "$Server: Login effettuato con successo.");
               sleep(1);
               sendMsg(clientInfo, "$Server: In attesa che si liberi una sessione...");
               sleep(1);
            }
            break;
         } else{
             /* Ripulisco le strutture di appoggio */
             memset(record, '\0', GRAPHICS_CHAT_WIDTH);
             recordIndex = 0;
         }
       }
     }
   }while(bytesReaded != 0);
   pthread_mutex_unlock(&mutexDatabase);

   if(clientInfo->status != CLTINF_PLAYING && clientInfo->status != CLTINF_LOGGED && alreadyLogged != true){
     sendMsg(clientInfo, "$Server: Username o password errati, riprovare.");
     sleep(1);
   }

 }while(clientInfo->status != CLTINF_LOGGED && clientInfo->status != CLTINF_PLAYING);

 return false;

}


 /**
   * @param clientinfo: Nodo ClientInfo con informazioni sul client.
   *
   * @return: 1 in caso di errore, 0 altrimenti.
   *
   * La funzione signin gestisce il signin da parte
   * del client al Server. Verifica inoltre che l'username del client
   * sia univoco.
   *
   */
 bool signin(LpClientInfo clientInfo){

  bool passed = false;                          /* Flag che indica il superamento di una fase */
  char incomingMsg[INCOMING_MSG_STRLEN];        /* Buffer per messaggi in entrata             */
  char record[GRAPHICS_CHAT_WIDTH];             /* Buffer per la lettura di un record del DB  */
  char username[CLTINF_USERNAME_STRLEN];        /* Buffer per username del client             */
  char password[CLTINF_PASSWORD_STRLEN];        /* Buffer per password del client             */
  int recordIndex;                              /* Indice di posizione del buffer record      */
  char character[1];                            /* Array per la lettura dei dati dal DB       */
  char* usernameDB;                             /* Username estratto dal DB                   */
  int bytesReaded;                              /* Numero di bytes letti dalla read           */
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  time_t timestamp;
  char logsBuffer[BUFFER_STRLEN];

  memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
  sendMsg(clientInfo, "$Server: Inserisci l'username - massimo 10 caratteri.");

  /* Ciclo finchè il client non inserisce un username che rispetti le condizioni di lunghezza, e che non sia già esistente */
  do{
    passed = true;
    /* Leggo l'username inserito dal client */
    if((bytesReaded = read(clientInfo->clientSocket, incomingMsg, INCOMING_MSG_STRLEN)) <= 0){
      return false;
    }
    incomingMsg[bytesReaded] = '\0';

    /* Verifico se l'utente ha inserito il comando di uscita */
    if(!strcmp(incomingMsg, "@exit")){
      return true;
    } else{
        /* Controllo se lo username supera la lunghezza di 10 caratteri stabiliti */
        if(strlen(incomingMsg) > CLTINF_USERNAME_STRLEN){
          sendMsg(clientInfo, "$Server: Attenzione, username troppo lungo - [massimo 10 caratteri], riprovare.");
          passed = false;
       } else{
          /* Controllo se nell'username siano presenti caratteri di spazio */
          for(int i=0; i<strlen(incomingMsg); i++){
            if(incomingMsg[i] == ' '){
              sendMsg(clientInfo, "$Server: Attenzione, gli spazi non sono consentiti, riprovare.");
              passed = false;
              break;
            }
          }
          /* Nel caso in cui l'username fosse ancora valido proseguo con le verifiche */
          if(passed != false){
            pthread_mutex_lock(&mutexDatabase);
            lseek(database, 0, SEEK_SET);
            memset(record, '\0', GRAPHICS_CHAT_WIDTH);
            recordIndex = 0;

            /* Ciclo sul file database per verificare che lo username non esista già */
            do{
              /* La lettura avviene un carattere alla volta fintanto non viene trovato un newline oppure \0 */
              if((bytesReaded = read(database, character, 1)) > 0){
                /* Il carattere è diverso da un newline oppure un fine stringa? */
                if(character[0] != '\n'){
                  record[recordIndex++] = character[0];
                } else{
                    record[recordIndex] = '\0';
                    usernameDB = strtok(record, " ");
                    /* Controllo se l'username esista gia nel database, se sì, stampo un errore e riclico il do */
                    if(!strcmp(usernameDB, incomingMsg)){
                      sendMsg(clientInfo, "$Server: Attenzione, username non disponibile, riprovare.");
                      passed = false;
                      break;
                    } else{
                       /* Ripulisco le strutture di appoggio */
                       memset(record, '\0', GRAPHICS_CHAT_WIDTH);
                       recordIndex = 0;
                    }
               }
             }
           }while(bytesReaded != 0);

           /* Verifico l'eventuale presenza di errori */
           if(bytesReaded == -1){
             sprintf(msg, " <!> Impossibile leggere dati dal Database.");
             if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true))){
               pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
             }
             sleep(2);
             /* La terminazione è necessariamente bruta a causa dell'impossibilità di catturare    */
             /* l'exit status del thread chiamante signin (numero di utenti non deterministico).   */
             /* Chiaramente una soluzione sarebbe potuta essere una lista di tid, tuttavia         */
             /* sarebbe stata un'implementazione "inutilmente" costosa, considerando che in certi  */
             /* casi la terminazione è obbligatoria.                                               */
             pthread_kill(pthread_self(), SIGTERM);
          } else if(passed != false){
              strcpy(username, incomingMsg);
          }
          pthread_mutex_unlock(&mutexDatabase);
        }
      }
      memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
    }
   }while(passed != true);

   sendMsg(clientInfo, "$Server: Inserisci la password - massimo 10 caratteri.");
   memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);

   do{
     passed = true;
     /* Leggo la password inserita dal client */
     if((bytesReaded = read(clientInfo->clientSocket, incomingMsg, INCOMING_MSG_STRLEN)) <= 0){
       return false;
     }
     incomingMsg[bytesReaded] = '\0';

     /* Verifico se l'utente ha inserito il comando di uscita */
     if(!strcmp(incomingMsg, "@exit")){
       return true;
     } else{
         /* Controllo se la password supera la lunghezza di 10 caratteri stabiliti  */
         if(strlen(incomingMsg) > CLTINF_PASSWORD_STRLEN){
           sendMsg(clientInfo, "$Server: Attenzione, password troppo lunga - [massimo 10 caratteri], riprovare.");
           passed = false;
         } else{
             /* Controllo che nella password non siano presenti caratteri di spazio */
             for(int i=0; i<strlen(incomingMsg); i++){
               if(incomingMsg[i] == ' '){
                 sendMsg(clientInfo, "$Server: Attenzione, gli spazi non sono consentiti, riprovare.");
                 passed = false;
                 break;
              }
            }
            if(passed != false){
              strcpy(password, incomingMsg);
            }
        }
        memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
     }
   }while(passed != true);

   memset(record, '\0', GRAPHICS_CHAT_WIDTH);
   /* Concateno username e password in un solo buffer */
   sprintf(record, "%s %s\n", username, password);
   pthread_mutex_lock(&mutexDatabase);

   /* Scrivo i dati di registazione del client nel database */
   if(write(database, record, strlen(record)) == -1){
     sprintf(msg, " <!> Impossibile scrivere dati nel Database.");
     if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true))){
       pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
     }
     sleep(2);
     /* La terminazione è necessariamente bruta a causa dell'impossibilità di catturare    */
     /* l'exit status del thread chiamante signin (numero di utenti non deterministico)    */
     /* Chiaramente una soluzione sarebbe potuta essere una lista di tid, tuttavia         */
     /* sarebbe stata un'implementazione "inutilmente" costosa, considerando che in certi  */
     /* casi la terminazione è obbligatoria.                                               */
     pthread_kill(pthread_self(), SIGTERM);
   }
   pthread_mutex_unlock(&mutexDatabase);
   /* Registrazione effettuata con successo */
   timestamp = time(NULL);
   pthread_mutex_lock(&mutexLogs);
   sprintf(logsBuffer, " > Registrazione del client %s avvenuta con successo - %s", clientInfo->clientAddressIPv4, ctime(&(clientInfo->timestamp)));
   if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
     if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
       pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
     }
   }
   pthread_mutex_unlock(&mutexLogs);
   sprintf(msg, "Registrazione del client %s avvenuta con successo.", clientInfo->clientAddressIPv4);
   if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_GREEN, false))){
     pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
   }
   sendMsg(clientInfo, "$Server: Registrazione avvenuta con successo, ora puoi effettuare il login.");
   clientInfo->status = CLTINF_REGISTERED;

   return false;

 }


 /**
   * @param clientinfo : arg ->  puntatore della struttura che contiene informazioni
   * del client connesso alla socket del server.
   *
   * @return: puntatore a void.
   *
   * La funzione listenerClient si occupa di gestirele azioni di ascolto
   * dei messaggi da parte del client, e l'invio dei comandi utilizzabili
   * al client.
   *
   */
 void* listenerClient(void* arg){

  LpClientInfo clientInfo = (LpClientInfo)arg;
  char incomingMsg[INCOMING_MSG_STRLEN];         /* Buffer messaggio in entrata                */
  int bytesReaded;                               /* Numero di bytes letti dalla read           */
  LpMsg msgChat;
  char msg[GRAPHICS_CHAT_WIDTH];
  pthread_t tidUpdateChat;
  bool exited = false;

  /* Invio il messaggio di benvenuto */
  sendMsg(clientInfo, "$Server: Benvenuto/a! Digita @cmd per conoscere i comandi.");
  memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);

  while(exited != true && (bytesReaded = read(clientInfo->clientSocket, incomingMsg, INCOMING_MSG_STRLEN)) > 0){
    incomingMsg[bytesReaded] = '\0';

    switch (clientInfo->status) {
      case CLTINF_GUEST:
        if(!strcmp(incomingMsg, "@cmd")){
          sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @login - @signin - @exit]");
        } else if(!strcmp(incomingMsg, "@signin")){
            exited = signin(clientInfo);
            if(exited){
              sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @login - @signin - @exit]");
              exited = false;
            }
        } else if(!strcmp(incomingMsg, "@login")){
            exited = login(clientInfo);
            if(exited){
              sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @login - @signin - @exit]");
              exited = false;
           }
        } else if(!strcmp(incomingMsg, "@exit")){
            exited = true;
        } else if(incomingMsg[0] == '@'){
            sendMsg(clientInfo, "$Server: Comando non disponibile, riprova!");
        } else{
            sendMsg(clientInfo, "$Server: Per utilizzare la chat devi prima effettuare il login!");
        }
      break;

      case CLTINF_REGISTERED:
        if(incomingMsg[0] == '@'){
          if(!strcmp(incomingMsg, "@cmd")){
            sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @login - @exit]");
          } else if(!strcmp(incomingMsg, "@login")){
              exited = login(clientInfo);
              if(exited){
              sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @login - @exit]");
              exited = false;
              }
          } else if(!strcmp(incomingMsg, "@exit")){
              exited = true;
          } else if(incomingMsg[0] == '@'){
              sendMsg(clientInfo, "$Server: Comando non disponibile, riprova!");
          } else{
              sendMsg(clientInfo, "$Server: Per utilizzare la chat devi prima effettuare il login!");
          }
        }
      break;

      case CLTINF_LOGGED:
        if(incomingMsg[0] == '@'){
          if(!strcmp(incomingMsg, "@cmd")){
            sendMsg(clientInfo, "$Server: Comandi disponibili -> [@cmd - @time - @exit]");
          } else if(!strcmp(incomingMsg, "@exit")){
              exited = true;
          } else if(!strcmp(incomingMsg, "@time")){
              if(session->joinable){
                int queueLen;
                pthread_mutex_lock(&mutexTimer);
                int totalSeconds = alarm(0);
                alarm(totalSeconds);
                pthread_mutex_unlock(&mutexTimer);
                LpClientInfoToJoin tmp = headClientInfoToJoinQueue;
                while(tmp != NULL){
                  queueLen += 1;
                  tmp = tmp->nextClientInfoToJoin;
                }
                totalSeconds += (queueLen/5)*TIME;
                int minutes = totalSeconds / 60;
                int seconds = totalSeconds % 60;
                if(minutes == 0){
                  sprintf(msg, "$Server: Giocherai all'incirca tra: %.2d secondi.", seconds);
                } else{
                  sprintf(msg, "$Server: Giocherai all'incirca tra: %.2d:%.2d minuti.", minutes, seconds);
                }
                sendMsg(clientInfo, msg);
             } else{
                 sendMsg(clientInfo, "$Server: Comando attualmente non disponibile.");
             }
          } else{
              sendMsg(clientInfo, "$Server: Comando non disponibile, riprova!");
          }
        } else{
            sprintf(msg, "%s: %s", clientInfo->username, incomingMsg);
            sendMsgToAll(clientInfo, msg);
        }
      break;

      case CLTINF_PLAYING:
        if(incomingMsg[0] == '@'){
          if(!strcmp(incomingMsg, "@exit")){
          exited = true;
          } else if(!strcmp(incomingMsg, "@S") || !strcmp(incomingMsg, "@N") || !strcmp(incomingMsg, "@O") || !strcmp(incomingMsg, "@E")){
              movePlayer(clientInfo, incomingMsg);
          } else if(!strcmp(incomingMsg, "@PS") || !strcmp(incomingMsg, "@PN") || !strcmp(incomingMsg, "@PO") || !strcmp(incomingMsg, "@PE") ||
                    !strcmp(incomingMsg, "@DS") || !strcmp(incomingMsg, "@DN") || !strcmp(incomingMsg, "@DO") || !strcmp(incomingMsg, "@DE")){
              actionPlayer(clientInfo, incomingMsg);
          } else if(!strcmp(incomingMsg, "@cmd")){
              sendMsg(clientInfo, "$Server: Comandi disponibili -> [@S - @N - @E - @O - @P(S/N/E/O) - @D(S/N/E/O)]");
          }
        } else if(incomingMsg[0] == '%'){
            int index = 0;
            for(index=0; index<strlen(incomingMsg)-1; index++){
              incomingMsg[index] = incomingMsg[index+1];
            }
            incomingMsg[index] = '\0';
            sprintf(msg, "%%%s: %s", clientInfo->username, incomingMsg);
            sendMsgToSession(clientInfo, msg);
        } else{
            sprintf(msg, "%s: %s", clientInfo->username, incomingMsg);
            sendMsgToAll(clientInfo, msg);
        }
      break;
    }
    memset(incomingMsg, '\0', INCOMING_MSG_STRLEN);
  }
  if(exited){
    sendMsg(clientInfo, "$Server: Torna presto! Disconnessione in corso...");
  }

  disconnectionManagement(clientInfo);

 }


 /**
   * @param clientinfo: puntatore della struttura che contiene informazioni del client.
   *
   * @return: puntatore a void.
   *
   * La funzione disconnectionManagement gestisce le operazioni di aggiornamento
   * mappa alla disconnessione di tale client, e alla comunicazione agli altri client
   * della disconnessione di esso.
   *
   */
 void disconnectionManagement(LpClientInfo clientInfo){

  pthread_t tidUpdateChat;
  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  char* dynamicBuffer;
  pthread_t tidUpdatePlayersInfo;
  time_t timestamp = time(NULL);
  char logsBuffer[BUFFER_STRLEN];

  pthread_mutex_lock(&(session->mutexSession));
  /* Verifico lo stato del client */
  if(clientInfo->status == CLTINF_PLAYING){
    /* Se è in possesso di un packet allora*/
    if(clientInfo->havePacket != -1){
      /* Inserisco nella posizione del player disconnesso il packet e invio un messaggio di segnalazione a tutti i client in sessione di gioco */
      sprintf(msg, "$add packet %d %d", clientInfo->currRows, clientInfo->currCols);
      sendMsgToSession(NULL, msg);
      sessionNumOfPackets += 1;
      session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_PACKET_VALUE;
      session->packets[clientInfo->havePacket].currRows = clientInfo->currRows;
      session->packets[clientInfo->havePacket].currCols = clientInfo->currCols;
      pthread_mutex_lock(&mutexCursor);
      moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
      setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
      printf("%s", SYMBOL_PACKET);
      reset();
      pthread_mutex_unlock(&mutexCursor);
     } else{
        /* Altrimenti nella posizione del player disconnesso inserisco l'erba e invio un messaggio di segnalazione a tutti i client in sessione di gioco */
         session->field[clientInfo->currRows][clientInfo->currCols] = SESSION_GRASS_VALUE;
         sprintf(msg, "$update %d %d", clientInfo->currRows, clientInfo->currCols);
         sendMsgToSession(NULL, msg);
         pthread_mutex_lock(&mutexCursor);
         moveto(field[clientInfo->currRows][clientInfo->currCols].posX, field[clientInfo->currRows][clientInfo->currCols].posY);
         setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
         printf("%s", SYMBOL_GRASS);
         reset();
         pthread_mutex_unlock(&mutexCursor);
     }
     /* Elimino il client dalla coda della sessione */
     deleteClientFromSession(session, clientInfo);
     /* Invio un messaggio di segnalazione della disconnessione del client a tutti i client in sessione */
     sprintf(msg, "$disconnected %s", clientInfo->username);
     dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
     strcpy(dynamicBuffer, msg);
     pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
     sendMsgToSession(NULL, msg);
     pthread_cond_signal(&condMatchmaker);
   }
   pthread_mutex_unlock(&(session->mutexSession));
   pthread_mutex_lock(&mutexLogs);

   if(clientInfo->status == CLTINF_LOGGED || clientInfo->status == CLTINF_PLAYING){
     sprintf(logsBuffer, " > Il client %s - %s si è disconnesso dal Server - %s", clientInfo->username, clientInfo->clientAddressIPv4, ctime(&timestamp));
   } else{
     sprintf(logsBuffer, " > Il client %s si è disconnesso dal Server - %s", clientInfo->clientAddressIPv4, ctime(&timestamp));
   }

   if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
     if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
       pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
     }
   }

   pthread_mutex_unlock(&mutexLogs);
   /* Stampo nella chat del server il nome e l'orario della disconnessione del client */
   sprintf(msg, "Il client %s si è disconnesso dal Server - %s", clientInfo->clientAddressIPv4, ctime(&timestamp));
   if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, false))){
     pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
   }

   /* Incremento il numero di disconnessione nel server e aggiorno la grafica */
   totalDisconnections += 1;
   pthread_mutex_lock(&mutexCursor);
   moveto(status[5].posX, status[5].posY);
   printf(" \u25cf Total DC: %7.2d", totalDisconnections);
   pthread_mutex_unlock(&mutexCursor);
   connectedClients -= 1;
   pthread_mutex_lock(&mutexCursor);
   moveto(status[2].posX, status[2].posY);
   printf(" \u25cf Connected: %6.2d", connectedClients);
   pthread_mutex_unlock(&mutexCursor);
   pthread_mutex_lock(&mutexClientInfo);

   /* Verifico se lo stato del client era di tipo LOGGED*/
   if(clientInfo->status == CLTINF_LOGGED){
     /* In esito positivo lo elimino dalla coda Join */
     deleteClientInfoToJoin(&headClientInfoToJoinQueue, &tailClientInfoToJoinQueue, clientInfo);
     waitingClients -= 1;
     pthread_mutex_lock(&mutexCursor);
     moveto(status[3].posX, status[3].posY);
     printf(" \u25cf Waiting: %8.2d", waitingClients);
     pthread_mutex_unlock(&mutexCursor);
   }

   /* Chiudo la socket di comunicazione */
   close(clientInfo->clientSocket);
   /* Elimino dalla lista dei client , il client disconnesso */
   deleteClientInfo(&listClientInfo, &clientInfo);
   pthread_mutex_unlock(&mutexClientInfo);

 }


 /**
   * @param void.
   *
   * @return: puntatore a void.
   *
   * La funzione initSession si occupa di creare un nuova sessione, e di rigenerare la mappa
   * a ogni nuova sessione creata.
   *
   */
 int initSession(void){

  int currLocation = 0;
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  char msg[GRAPHICS_CHAT_WIDTH];
  char logsBuffer[BUFFER_STRLEN];

  sleep(1);

  /* Verifico che la sessione sia diversa da NULL , in caso di esito positivo eseguo una free della sessione */
  if(session != NULL){
    free(session);
  }

  /* Creo una nuova sessione */
  session = newSession();
  idSession += 1;
  pthread_mutex_lock(&mutexCursor);
  moveto(status[1].posX, status[1].posY);
  printf(" \u25cf ID Sessione: %4.2d", idSession);
  pthread_mutex_unlock(&mutexCursor);
  pthread_mutex_lock(&mutexCursor);

  /* Pulisco la grafica della matrice */
  for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
    for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
      moveto(field[i][j].posX, field[i][j].posY);
      setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
      printf(" ");
      reset();
    }
  }

  pthread_mutex_unlock(&mutexCursor);
  pthread_mutex_lock(&mutexLogs);
  sprintf(logsBuffer, " > Nuova sessione generata -> ID Sessione: %d\n", idSession);
  if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
    if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
      pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
    }
  }
  pthread_mutex_unlock(&mutexLogs);

  if(session != NULL){

    /* Inizializzo i valori della matrice */
    if(initField()){
      return 1;
    }
    /* Stampo al terminale la matrice */
    for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
      for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
        pthread_mutex_lock(&mutexCursor);
        moveto(field[i][j].posX, field[i][j].posY);
        switch (session->field[i][j]){
          case 0:
            setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
            printf("%s", SYMBOL_GRASS);
            reset();
          break;
          case 1:
            setColor(GRAPHICS_FG_COLOR_BLUE, GRAPHICS_BG_COLOR_BLACK);
            printf("%s", SYMBOL_WALL);
            reset();
          break;
          case 2:
            setColor(GRAPHICS_FG_COLOR_WHITE, GRAPHICS_BG_COLOR_MAGENTA);
            printf("%c", session->locations[currLocation].name);
            reset();
            currLocation += 1;
          break;
          case 3:
            setColor(GRAPHICS_FG_COLOR_RED, GRAPHICS_BG_COLOR_GREEN);
            printf("%s", SYMBOL_PACKET);
            reset();
          break;
          case 4:
            setColor(GRAPHICS_FG_COLOR_GREEN, GRAPHICS_BG_COLOR_GREEN);
            printf("%s", SYMBOL_GRASS);
            reset();
          break;
       }

       fflush(stdout);
       pthread_mutex_unlock(&mutexCursor);
     }
   }
   currLocation = 0;
   pthread_mutex_lock(&mutexLogs);
   sprintf(logsBuffer, " > Mappa per la sessione %d, generata: \n", idSession);
   if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
     if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
       pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
    }
   sprintf(logsBuffer, "\n");
   write(logs, logsBuffer, strlen(logsBuffer));
   /* Scrivo nel file di log la matrice generata dalla sessione */
   for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
     for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
       if(j==0){
         sprintf(logsBuffer, " ");
         write(logs, logsBuffer, strlen(logsBuffer));
       }
     switch (session->field[i][j]){
       case 0:
         sprintf(logsBuffer, "%s", SYMBOL_GRASS);
       break;
       case 1:
         sprintf(logsBuffer, "%s", SYMBOL_WALL);
       break;
       case 2:
         sprintf(logsBuffer, "%c", session->locations[currLocation].name);
         currLocation += 1;
       break;
       case 3:
         sprintf(logsBuffer, "%s", SYMBOL_PACKET);
       break;
       case 4:
         sprintf(logsBuffer, "%s", SYMBOL_GRASS);
      break;
     }
     write(logs, logsBuffer, strlen(logsBuffer));
    }
    sprintf(logsBuffer, "\n");
    write(logs, logsBuffer, strlen(logsBuffer));
  }
  sprintf(logsBuffer, "\n");
  write(logs, logsBuffer, strlen(logsBuffer));
  pthread_mutex_unlock(&mutexLogs);

  if((msgChat = allocMsg("Mappa per la sessione corrente generata!", GRAPHICS_FG_COLOR_BLUE, false)) != NULL){
    pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
   }

  } else{
    return 1;
  }

  return 0;
 }


 /**
   * @param arg: NULL.
   *
   * @return: puntatore a void.
   *
   * La funzione sessionManagement si occupa dell operazioni di assegnazioni degli esiti
   * di gioco e della disconnessione dei client a fine del tempo della sessione.
   *
   */
 void* sessionManagement(void* arg){

  pthread_t tidMatchmaker;
  pthread_t tidUpdateChat;
  pthread_t tidTimeProvider;
  pthread_t tidUpdatePlayersInfo;
  LpClientInfoToJoin clientInfoToJoin;
  char msg[GRAPHICS_CHAT_WIDTH];
  int maxPackets = 0;
  int client = -1;
  int occurrence = 1;
  LpMsg msgChat;
  char logsBuffer[BUFFER_STRLEN];
  char* dynamicBuffer;

  session->joinable = true;
  pthread_create(&tidMatchmaker, NULL, matchmaker, NULL);
  pthread_mutex_lock(&(session->mutexSession));

  /* Il thread dorme finchè non entra un client */
  while(session->numOfPlayers < 1){
    pthread_cond_wait(&condSession, &(session->mutexSession));
  }

  /* Appena entra un client in sessione avvia il  thread che gestisce le operazioni di durata di tempo della sessione */
  pthread_mutex_unlock(&(session->mutexSession));
  pthread_create(&tidTimeProvider, NULL, timeProvider, NULL);
  pthread_join(tidTimeProvider, NULL);
  session->joinable = false;
  pthread_cond_signal(&condMatchmaker);
  pthread_join(tidMatchmaker, NULL);
  pthread_mutex_lock(&mutexClientInfo);
  /* A fine del tempo verifica chi è il vincitore */
  for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
    if(session->clients[i] != NULL){
      if(session->clients[i]->packetsDelivered > maxPackets){
        maxPackets = session->clients[i]->packetsDelivered;
        occurrence = 1;
        client = i;
      } else if(session->clients[i]->packetsDelivered == maxPackets){
          occurrence += 1;
      }
    }
  }
  pthread_mutex_unlock(&mutexClientInfo);
  /* Se c'è un vincitore invia un messaggio di segnalazione a tutti i client nella sessione di gioco */
  if(maxPackets > 0 && occurrence == 1){
    sprintf(msg, "$Server: Abbiamo un vincitore: %s!", session->clients[client]->username);
    sendMsgToSession(NULL, msg);
    pthread_mutex_lock(&mutexLogs);
    sprintf(logsBuffer, " > [Sessione %d] Vincitore: %s -> Numero di pacchetti consegnati: %d.\n", idSession, session->clients[client]->username, session->clients[client]->packetsDelivered);
    if(write(logs, logsBuffer, strlen(logsBuffer)) == -1){
      if((msgChat = allocMsg("Errore durante la scrittura nel file di logs.", GRAPHICS_FG_COLOR_RED, true)) != NULL){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
    }
    pthread_mutex_unlock(&mutexLogs);
    /* Altrimenti nel caso non sia stato raccolto un packet invia un messaggio di segnalazione a tutti i client nella sessione di gioco */
  } else if(maxPackets == 0){
      sprintf(msg, "$Server: Che delusione, nessuno ha raccolto un pacco!");
      sendMsgToSession(NULL, msg);
    /* Altrimenti nel caso non ci sia un vincitore invia un messaggio di segnalazione a tutti i client nella sessione di gioco */
  } else if(maxPackets > 0 && occurrence > 1){
      sprintf(msg, "$Server: Non abbiamo nessun vincitore, partita equilibrata!");
      sendMsgToSession(NULL, msg);
  }
  sleep(3);
  sprintf(msg, "$Server: Disconnessione dalla sessione corrente in corso...");
  sendMsgToSession(NULL, msg);
  pthread_mutex_lock(&(session->mutexSession));
  /* Invia un messaggio di reset a tutti i client , e li disconnette dalla sessione rimettendoli nella coda di Join */
  for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
    if(session->clients[i] != NULL){
      session->clients[i]->havePacket = -1;
      sendMsgToSession(NULL, "$reset");
      sprintf(msg, "$disconnected %s", session->clients[i]->username);
      sendMsgToSession(NULL, msg);
      dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
      strcpy(dynamicBuffer, msg);
      pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
      session->clients[i]->status = CLTINF_LOGGED;
      clientInfoToJoin = newClientInfoToJoin(session->clients[i]);

      if(clientInfoToJoin != NULL){
        pthread_mutex_lock(&mutexClientInfo);
        enqueueClientInfoToJoin(&headClientInfoToJoinQueue, &tailClientInfoToJoinQueue, clientInfoToJoin);
        pthread_mutex_unlock(&mutexClientInfo);
        waitingClients += 1;
        pthread_mutex_lock(&mutexCursor);
        moveto(status[3].posX, status[3].posY);
        printf(" \u25cf Waiting: %8.2d", waitingClients);
        pthread_mutex_unlock(&mutexCursor);
      } else{
          close(session->clients[i]->clientSocket);
      }
   }
  }
  /* Invia un messaggio a tutti i client connessi al server di una nuova rigenerazione di sessione */
  pthread_mutex_unlock(&(session->mutexSession));
  sprintf(msg, "$Server: Generazione di una nuova sessione in corso...");
  sendMsgToAll(NULL, msg);
  sleep(1);
  sprintf(msg, "$Server: In attesa che si liberi una sessione...");
  sendMsgToAll(NULL, msg);
  sleep(2);

 }


 /**
   * @param arg : NULL.
   *
   * @return: puntatore a void.
   *
   * La funzione matchmaker si occupa di gestire delle operazioni
   * di inserimento dei client joinabili nella sessione.
   *
   */
 void* matchmaker(void* arg){

  LpClientInfo clientToJoin;
  char msg[GRAPHICS_CHAT_WIDTH];
  bool joined = false;
  char* dynamicBuffer;
  pthread_t tidUpdatePlayersInfo;

  while(true){
    pthread_mutex_lock(&session->mutexSession);

    /* Il thread rimane in attessa appena si è raggiunto il numero massimo di player in sessione */
    while(session->numOfPlayers > SESSION_PLAYERS_STRLEN-1 && session->joinable != false){
      pthread_cond_wait(&condMatchmaker, &(session->mutexSession));
    }

    /* Se la sesssione è joinable allora */
    if(session->joinable){
      pthread_mutex_lock(&mutexClientInfo);
      /* Verifica se sia presente un player nella coda Join , in caso di esito positivo lo inserisce in sessione */
      if(headClientInfoToJoinQueue != NULL){
        clientToJoin = dequeueClientInfoToJoin(&headClientInfoToJoinQueue, &tailClientInfoToJoinQueue);
        waitingClients -= 1;
        pthread_mutex_lock(&mutexCursor);
        moveto(status[3].posX, status[3].posY);
        printf(" \u25cf Waiting: %8.2d", waitingClients);
        pthread_mutex_unlock(&mutexCursor);
        clientToJoin->status = CLTINF_PLAYING;
        clientToJoin->packetsDelivered = 0;
        sendMsg(clientToJoin, "$remove blink");
        sendMsg(clientToJoin, "$reset");
        sleep(2);
        sendMsg(clientToJoin, "$Server: Che il gioco abbia inizio! Buon divertimento!");
        insertClientInSession(session, clientToJoin);
        joined = true;
        pthread_cond_signal(&condSession);
      }
      pthread_mutex_unlock(&mutexClientInfo);
      /* Informa al nuovo client nella sessione tutti gli utenti attualmente in sessione */
      if(joined){
        for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
          if(session->clients[i] != NULL && session->clients[i] != clientToJoin){
            sprintf(msg, "$connected %s %d", session->clients[i]->username, session->clients[i]->packetsDelivered);
            sendMsg(clientToJoin, msg);
          }
        }
        /* Invia un messaggio a tutti gli utenti in sessione dell'aggiunta del nuovo client alla sessione */
        sprintf(msg, "$connected %s %d", clientToJoin->username, clientToJoin->packetsDelivered);
        dynamicBuffer = (char*)calloc(GRAPHICS_CHAT_WIDTH, sizeof(char));
        strcpy(dynamicBuffer, msg);
        pthread_create(&tidUpdatePlayersInfo, NULL, updatePlayersInfo, dynamicBuffer);
        sendBasicInformation(clientToJoin);
        sendMsgToSession(NULL, msg);
        sprintf(msg, "$Server: Tremate, %s si è unito alla sessione!", clientToJoin->username);
        sendMsgToSession(clientToJoin, msg);
        joined = false;
      }
      pthread_mutex_unlock(&session->mutexSession);
    } else{
        pthread_mutex_unlock(&session->mutexSession);
        pthread_exit(NULL);
    }
  }
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

  char msg[GRAPHICS_CHAT_WIDTH];
  LpMsg msgChat;
  pthread_t tidUpdateChat;
  LpClientInfo tmp;

  switch(signal){
    case SIGINT:
      sprintf(msg, "Disconnessione del server in corso!");
      if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true))){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
      close(listenerSocket);
      close(logs);
      close(database);
      close(fieldGenerator);
      tmp = listClientInfo;
      while(tmp != NULL){
        close(tmp->clientSocket);
        tmp = tmp->nextClientInfo;
      }
      sleep(2);
      clean();
      setCursor(true);
      raise(SIGTERM);
    break;
    case SIGSTOP:
      sprintf(msg, "Disconnessione del server in corso!");
      if((msgChat = allocMsg(msg, GRAPHICS_FG_COLOR_RED, true))){
        pthread_create(&tidUpdateChat, NULL, updateChat, msgChat);
      }
      close(listenerSocket);
      close(logs);
      close(database);
      close(fieldGenerator);
      tmp = listClientInfo;
      while(tmp != NULL){
        close(tmp->clientSocket);
        tmp = tmp->nextClientInfo;
      }
      sleep(2);
      clean();
      setCursor(true);
      raise(SIGTERM);
    break;
  }
 }

 /******************************************** END SERVER ********************************************/
