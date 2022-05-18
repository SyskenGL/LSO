
/******************************************** INIT SESSION HEADER ********************************************/

  #include "../cltinf/clientinfo.h"
  #include "../graphics/graphics.h"
  #include <stdbool.h>
  #include <pthread.h>



  #define SESSION_PLAYERS_STRLEN         5
  #define SESSION_PACKETS_STRLEN        21
  #define SESSION_LOCATIONS_STRLEN       5
  #define SESSION_SPAWN_STRLEN           8

  #define SESSION_GRASS_VALUE            0
  #define SESSION_WALL_VALUE             1
  #define SESSION_LOCATION_VALUE         2
  #define SESSION_PACKET_VALUE           3
  #define SESSION_PLAYER_VALUE           4



  /**
    * @attribute currRows: Riga corrente.
    * @attribute currCols: Colonna corrente.
    */

  struct Spawn{
    int currRows;
    int currCols;
  };
  typedef struct Spawn Spawn;


  /**
    * @attribute currRows: Riga corrente.
    * @attribute currCols: Colonna corrente.
    * @attribute name: Nome della locazione.
    */

  struct Location{
    int currRows;
    int currCols;
    char name;
  };
  typedef struct Location Location;



  /**
    * @attribute currRows: Riga corrente.
    * @attribute currCols: Colonna corrente.
    * @attribute location: Locazione a cui trasportare il pacchetto.
    * @attribute delivered: Flag che indica se il pacchetto è stato consegnato.
    */

  struct Packet{
    int currRows;
    int currCols;
    int location;
    bool delivered;
  };
  typedef struct Packet Packet;



  /**
    * @attribute clients: Clients connessi alla sessione.
    * @attribute field: Campo di gioco.
    * @attribute packets: Pacchetti della sessione.
    * @attribute locations: Locazioni della sessione.
    * @attribute spawn: Spawns della sessione.
    * @attribute joinable: Flag che indica se la sessione è accessibile.
    * @attribute numOfPlayers: Numero di giocatori correnti.
    * @attribute numOfPackets: Numero di pacchetti correnti.
    * @attribute mutexSession: mutex di sessione.
    */

  struct Session{
    LpClientInfo clients[SESSION_PLAYERS_STRLEN];
    int field[GRAPHICS_FIELD_HEIGHT][GRAPHICS_FIELD_WIDTH];
    Packet packets[SESSION_PACKETS_STRLEN];
    Location locations[SESSION_LOCATIONS_STRLEN];
    Spawn spawn[SESSION_SPAWN_STRLEN];
    bool joinable;
    int numOfPlayers;
    int numOfPackets;
    pthread_mutex_t mutexSession;
  };
  typedef struct Session Session;
  typedef Session* LpSession;



  /**
    * @param void.
    *
    * @return LpSession: Puntatore alla sessione allocata.
    *
    * La funzione newSession alloca una nuova sessione e la inizializza.
    */

  LpSession newSession();



  /**
    * @param session: Puntatore alla sessione.
    * @param clientInfo: Puntatore al ClientInfo da eliminare dalla sessione.
    *
    * @return void.
    *
    * La funzione deleteClientFromSession elimina dalla lista dei clients della sessione
    * il client specificato dal puntatore clientInfo.
    */

  void deleteClientFromSession(LpSession, LpClientInfo);



  /**
    * @param session: Puntatore alla sessione.
    * @param clientInfo: Puntatore al ClientInfo da inserire dalla sessione.
    *
    * @return void.
    *
    * La funzione insertClientInSession inserisce nella lista dei clients della sessione
    * il client specificato dal puntatore clientInfo.
    */

  void insertClientInSession(LpSession, LpClientInfo);

/******************************************** END SESSION HEADER ********************************************/
