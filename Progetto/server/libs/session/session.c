
/******************************************** INIT SESSION ********************************************/

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <sys/types.h>
  #include "session.h"



  /**
    * @param void.
    *
    * @return LpSession: Puntatore alla sessione allocata.
    *
    * La funzione newSession alloca una nuova sessione e la inizializza.
    */

  LpSession newSession(){
    LpSession newSession = (LpSession)malloc(sizeof(Session));
    if(newSession != NULL){
      for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
        newSession->clients[i] = NULL;
      }
      for(int i=0; i<SESSION_PACKETS_STRLEN; i++){
        newSession->packets[i].delivered = false;
      }
      for(int i=0; i<GRAPHICS_FIELD_HEIGHT; i++){
        for(int j=0; j<GRAPHICS_FIELD_WIDTH; j++){
          newSession->field[i][j] = SESSION_GRASS_VALUE;
        }
      }
      newSession->numOfPlayers = 0;
      pthread_mutex_init(&(newSession->mutexSession), NULL);
    }
    return newSession;
  }



  /**
    * @param session: Puntatore alla sessione.
    * @param clientInfo: Puntatore al ClientInfo da eliminare dalla sessione.
    *
    * @return void.
    *
    * La funzione deleteClientFromSession elimina dalla lista dei clients della sessione
    * il client specificato dal puntatore clientInfo.
    */

  void deleteClientFromSession(LpSession session, LpClientInfo clientInfo){
    for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
      if(session->clients[i] == clientInfo){
        session->numOfPlayers -= 1;
        session->clients[i] = NULL;
        break;
      }
    }
  }



  /**
    * @param session: Puntatore alla sessione.
    * @param clientInfo: Puntatore al ClientInfo da inserire dalla sessione.
    *
    * @return void.
    *
    * La funzione insertClientInSession inserisce nella lista dei clients della sessione
    * il client specificato dal puntatore clientInfo.
    */

  void insertClientInSession(LpSession session, LpClientInfo clientInfo){
    for(int i=0; i<SESSION_PLAYERS_STRLEN; i++){
      if(session->clients[i] == NULL){
        session->numOfPlayers += 1;
        session->clients[i] = clientInfo;
        break;
      }
    }
  }

/******************************************** END SESSION ********************************************/
