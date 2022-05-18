
/******************************************** INIT CLTINF HEADER ********************************************/

  #include <netinet/in.h>
  #include <time.h>
  #include <stdbool.h>
  #include <pthread.h>

  #define CLTINF_USERNAME_STRLEN   10
  #define CLTINF_PASSWORD_STRLEN   10

  #define CLTINF_GUEST              1
  #define CLTINF_REGISTERED         2
  #define CLTINF_LOGGED             3
  #define CLTINF_PLAYING            4



  /**
    * @attribute tidHandler: TID del thread principale che gestisce un determinato client.
    * @attribute clientSocket: Socket descriptor della socket associata ad un determinato client.
    * @attribute clientAddressIPv4: IPv4 del client.
    * @attribute username: Username del client.
    * @attribute timestamp: Ora della connessione
    * @attribute stato: LOGGED, REGISTERED, GUEST.
    * @attribute prevClientInfo: Puntatore al nodo clientInfo precedente.
    * @attribute nextClientInfo: Puntatore al nodo clientInfo successivo.
    * @attribute mutexSocket: Mutex associato alla socket.
    */

  struct ClientInfo{
    pthread_t tidHandler;
    int clientSocket;
    char clientAddressIPv4[INET_ADDRSTRLEN];
    char username[CLTINF_USERNAME_STRLEN];
    time_t timestamp;
    int packetsDelivered;
    int status;
    int currRows;
    int currCols;
    int havePacket;
    pthread_mutex_t mutexSocket;
    struct ClientInfo* prevClientInfo;
    struct ClientInfo* nextClientInfo;
  };
  typedef struct ClientInfo ClientInfo;
  typedef ClientInfo* LpClientInfo;



  /**
    * @attribute clientInfo: Puntatore alla struttura clientInfo.
    * @attribute prevClientInfoToJoin: Puntatore al nodo ClientInfoToJoin precedente.
    * @attribute prevClientInfoToJoin: Puntatore al nodo ClientInfoToJoin successivo.
    *
    */

  struct ClientInfoToJoin{
    LpClientInfo clientInfo;
    struct ClientInfoToJoin* nextClientInfoToJoin;
  };
  typedef struct ClientInfoToJoin ClientInfoToJoin;
  typedef ClientInfoToJoin* LpClientInfoToJoin;



  /**
    * @param clientInfo: Puntatore alla struttura clientInfo associata.
    *
    * @return newClientInfoToJoin: Puntatore al nodo newClientInfoToJoin allocato.
    *
    * La funzione newClientInfoToJoin alloca un nuovo nodo clientInfoToJoin con gli attributi passati
    * in ingresso.
    */

  LpClientInfoToJoin newClientInfoToJoin(LpClientInfo);



  /**
    * @param headClientInfoToJoin: Puntatore alla testa della coda di ClientInfoToJoin.
    *
    * @return bool: true (vuota) | false (non vuota).
    *
    * La funzione isEmptyClientInfoToJoin verifica se la coda passata in ingresso
    * è vuota oppure no.
    */

  bool isEmptyClientInfoToJoin(LpClientInfoToJoin);



  /**
    * @param headClientInfoToJoin: Doppio puntatore alla testa della coda di ClientInfoToJoin.
    * @param tailClientInfoToJoin: Doppio puntatore alla coda della coda di ClientInfoToJoin.
    * @param newClientInfoToJoin: Puntatore al nodo ClientInfoToJoin da accodare.
    *
    * @return void.
    *
    * La funzione enqueueClientInfoToJoin accoda il nodo ClientInfoToJoin passato in ingresso.
    */

  void enqueueClientInfoToJoin(LpClientInfoToJoin*, LpClientInfoToJoin*, LpClientInfoToJoin);



  /**
    * @param headClientInfoToJoin: Doppio puntatore alla testa della coda di ClientInfoToJoin.
    * @param tailClientInfoToJoin: Doppio puntatore alla coda della coda di ClientInfoToJoin.
    *
    * @return LpClientInfo: Puntatore alla struttura ClientInfo associata al nodo eliminato.
    *
    * La funzione dequeueClientInfoToJoin deaccoda la testa della coda.
    */

  LpClientInfo dequeueClientInfoToJoin(LpClientInfoToJoin*, LpClientInfoToJoin*);



  /**
    * @param headClientInfoToJoin: Doppio puntatore alla testa della coda di ClientInfoToJoin.
    * @param tailClientInfoToJoin: Doppio puntatore alla coda della coda di ClientInfoToJoin.
    * @param targetedClientInfo: Doppio puntatore al nodo clientInfo da eliminare.
    *
    * @return void.
    *
    * La funzione deleteClientInfoToJoin elimina il nodo specificato da targetedClientInfo dalla
    * coda in cui risiede lo stesso.
    */

  void deleteClientInfoToJoin(LpClientInfoToJoin*, LpClientInfoToJoin*, LpClientInfo);



  /**
    * @param tidHandler: TID del thread principale che gestisce un determinato client.
    * @param clientSocket: Socket descriptor della socket associata ad un determinato client.
    * @param clientAddressIPv4: IPv4 del client.
    * @param username: Username del client.
    *
    * @return newClientInfo: Puntatore al nodo clientInfo allocato.
    *
    * La funzione newClientInfo alloca un nuovo nodo clientInfo con gli attributi passati
    * in ingresso.
    */

  LpClientInfo newClientInfo(int, char[], char[]);



  /**
    * @param listClientInfo: Doppio puntatore alla lista doppiamente concatenata di clientInfo.
    * @param newClientInfo: Nodo clientInfo da inserire.
    *
    * @return void.
    *
    * La funzione insertClientInfo inserisce un nuovo nodo clientInfo in testa alla lista
    * listClientInfo.
    */

  void insertClientInfo(LpClientInfo*, LpClientInfo);



  /**
    * @param targetedClientInfo: Doppio puntatore al nodo clientInfo da eliminare.
    *
    * @return void.
    *
    * La funzione deleteClientInfo elimina il nodo specificato da targetedClientInfo dalla
    * lista in cui risiede lo stesso.
    */

  void deleteClientInfo(LpClientInfo*, LpClientInfo*);

/******************************************** END CLTINF HEADER ********************************************/
