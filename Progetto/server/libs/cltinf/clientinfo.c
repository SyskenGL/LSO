
/******************************************** INIT CLTINF ********************************************/

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <sys/types.h>
  #include "clientinfo.h"



  /**
    * @param clientInfo: Puntatore alla struttura clientInfo associata.
    *
    * @return newClientInfoToJoin: Puntatore al nodo newClientInfoToJoin allocato.
    *
    * La funzione newClientInfoToJoin alloca un nuovo nodo clientInfoToJoin con gli attributi passati
    * in ingresso.
    */

  LpClientInfoToJoin newClientInfoToJoin(LpClientInfo clientInfo){
    LpClientInfoToJoin newClientInfoToJoin = (LpClientInfoToJoin)malloc(sizeof(ClientInfoToJoin));
    if(newClientInfoToJoin != NULL){
      newClientInfoToJoin->clientInfo = clientInfo;
      newClientInfoToJoin->nextClientInfoToJoin = NULL;
    }
    return newClientInfoToJoin;
  }



  /**
    * @param headClientInfoToJoin: Puntatore alla testa della coda di ClientInfoToJoin.
    *
    * @return bool: true (vuota) | false (non vuota).
    *
    * La funzione isEmptyClientInfoToJoin verifica se la coda passata in ingresso
    * è vuota oppure no.
    */

  bool isEmptyClientInfoToJoin(LpClientInfoToJoin headClientInfoToJoin){
    if(headClientInfoToJoin != NULL){
      return false;
    }
    return true;
  }



  /**
    * @param headClientInfoToJoin: Doppio puntatore alla testa della coda di ClientInfoToJoin.
    * @param tailClientInfoToJoin: Doppio puntatore alla coda della coda di ClientInfoToJoin.
    * @param newClientInfoToJoin: Puntatore al nodo ClientInfoToJoin da accodare.
    *
    * @return void.
    *
    * La funzione enqueueClientInfoToJoin accoda il nodo ClientInfoToJoin passato in ingresso.
    */

   void enqueueClientInfoToJoin(LpClientInfoToJoin* headClientInfoToJoin, LpClientInfoToJoin* tailClientInfoToJoin, LpClientInfoToJoin newClientInfoToJoin){
     if(newClientInfoToJoin != NULL){
       if(isEmptyClientInfoToJoin(*headClientInfoToJoin)){
         *headClientInfoToJoin = newClientInfoToJoin;
       } else{
         (*tailClientInfoToJoin)->nextClientInfoToJoin = newClientInfoToJoin;
       }
       *tailClientInfoToJoin = newClientInfoToJoin;
     }
   }



   /**
     * @param headClientInfoToJoin: Doppio puntatore alla testa della coda di ClientInfoToJoin.
     * @param tailClientInfoToJoin: Doppio puntatore alla coda della coda di ClientInfoToJoin.
     *
     * @return LpClientInfo: Puntatore alla struttura ClientInfo associata al nodo eliminato.
     *
     * La funzione dequeueClientInfoToJoin deaccoda la testa della coda.
     */

   LpClientInfo dequeueClientInfoToJoin(LpClientInfoToJoin* headClientInfoToJoin, LpClientInfoToJoin* tailClientInfoToJoin){
     LpClientInfo tmp = NULL;
     if(!isEmptyClientInfoToJoin(*headClientInfoToJoin)){
       tmp = (*headClientInfoToJoin)->clientInfo;
       LpClientInfoToJoin tmpClientInfoToJoin = *headClientInfoToJoin;
       *headClientInfoToJoin = (*headClientInfoToJoin)->nextClientInfoToJoin;
       if(isEmptyClientInfoToJoin(*headClientInfoToJoin)){
         *tailClientInfoToJoin = NULL;
       }
       free(tmpClientInfoToJoin);
     }
     return tmp;
   }



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

   void deleteClientInfoToJoin(LpClientInfoToJoin* headClientInfoToJoin, LpClientInfoToJoin* tailClientInfoToJoin, LpClientInfo targetedClientInfo){
     LpClientInfoToJoin tmp = *headClientInfoToJoin;
     LpClientInfoToJoin prevClientInfoToJoin = NULL;
     while(tmp != NULL && tmp->clientInfo != targetedClientInfo){
       prevClientInfoToJoin = tmp;
       tmp = tmp->nextClientInfoToJoin;
     }
     if(tmp != NULL){
       if(tmp == *headClientInfoToJoin && tmp == *tailClientInfoToJoin){
         *headClientInfoToJoin = NULL;
         *tailClientInfoToJoin = NULL;
       } else{
         if(tmp == *headClientInfoToJoin){
           *headClientInfoToJoin = tmp->nextClientInfoToJoin;
         }
         if(tmp == *tailClientInfoToJoin){
           *tailClientInfoToJoin = prevClientInfoToJoin;
         }
         if(prevClientInfoToJoin != NULL){
           prevClientInfoToJoin->nextClientInfoToJoin = tmp->nextClientInfoToJoin;
         }
       }
       free(tmp);
     }
   }



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

   LpClientInfo newClientInfo(int clientSocket, char clientAddressIPv4[], char username[]){
     LpClientInfo newClientInfo = (LpClientInfo)malloc(sizeof(ClientInfo));
     if(newClientInfo != NULL){
       newClientInfo->clientSocket = clientSocket;
       strcpy(newClientInfo->clientAddressIPv4, clientAddressIPv4);
       strcpy(newClientInfo->username, username);
       newClientInfo->timestamp = time(NULL);
       newClientInfo->status = CLTINF_GUEST;
       newClientInfo->havePacket = -1;
       newClientInfo->prevClientInfo = NULL;
       newClientInfo->nextClientInfo = NULL;
       pthread_mutex_init(&(newClientInfo->mutexSocket), NULL);
     }
     return newClientInfo;
   }



   /**
     * @param listClientInfo: Doppio puntatore alla lista doppiamente concatenata di clientInfo.
     * @param newClientInfo: Nodo clientInfo da inserire.
     *
     * @return void.
     *
     * La funzione insertClientInfo inserisce un nuovo nodo clientInfo in testa alla lista
     * listClientInfo.
     */

   void insertClientInfo(LpClientInfo* listClientInfo, LpClientInfo newClientInfo){
     if(newClientInfo != NULL){
       newClientInfo->nextClientInfo = *listClientInfo;
       if(*listClientInfo != NULL){
         newClientInfo->prevClientInfo = (*listClientInfo)->prevClientInfo;
         (*listClientInfo)->prevClientInfo = newClientInfo;
         if(newClientInfo->prevClientInfo != NULL){
           newClientInfo->prevClientInfo->nextClientInfo = newClientInfo;
         }
       }
       *listClientInfo = newClientInfo;
     }
   }



   /**
     * @param targetedClientInfo: Doppio puntatore al nodo clientInfo da eliminare.
     *
     * @return void.
     *
     * La funzione deleteClientInfo elimina il nodo specificato da targetedClientInfo dalla
     * lista in cui risiede lo stesso.
     */

   void deleteClientInfo(LpClientInfo* listClientInfo, LpClientInfo* targetedClientInfo){
     if(targetedClientInfo != NULL){
       LpClientInfo tmp = *targetedClientInfo;
       *targetedClientInfo = (*targetedClientInfo)->nextClientInfo;
       if(tmp->prevClientInfo != NULL){
         tmp->prevClientInfo->nextClientInfo = *targetedClientInfo;
       }
       if(*targetedClientInfo != NULL){
         (*targetedClientInfo)->prevClientInfo = tmp->prevClientInfo;
       }
       if(*listClientInfo == tmp){
         *listClientInfo = *targetedClientInfo;
       }
       free(tmp);
     }
   }

/******************************************** END CLTINF ********************************************/
