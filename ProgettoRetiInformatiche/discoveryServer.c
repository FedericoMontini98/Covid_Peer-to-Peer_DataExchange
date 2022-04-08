#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

//Definizione di strutture
// 1: definisce l'elemento da inserire nella coda circolare dei peer, conterrà l'IP, la porta e il puntatore all'elemento 
//    successivo
struct codaPeer{
  char* ipPeer;
  short porta;
  struct codaPeer* prossimo;
};
// 2: rendo global la variabile che conta il numero di peer attualmente connessi
int numPeer;
// 3: rendo di visibilità globale il puntatore alla testa della coda di processi collegati
struct codaPeer* testa;
// 4: serve per visualizzare come circolare la coda, altrimenti non potrei collegare l'ultimo peer al primo
struct codaPeer* coda;

//Dichiarazione funzioni definite sotto
void help();
void showpeers();
void showneighborSpec(int peerNum);
void showneighbor();
void esc(int sd);
void ascoltopeer(int sd);
void inserisciPeer(char* ip,uint16_t porta);
int removePeer(uint16_t porta);
char* neighbors(char* string,uint16_t portaConn);
int cercaVicini(int* primoVicino,int* secondoVicino,int sogg);
void updatePeer(int sd, int* vicino1, int* vicino2);

//Definizione di Costanti
#define BUF_SIZE 1024   //Dimensione dei buffer
#define RES_LEN 4       //'CON\0' dimensione minima del pacchetto di arrivo
#define POLLING_TIME 5  //Tempo di attesa in cui la primitiva non bloccante si sospende prima di cercare nuovi pacchetti
#define SIGTERM 15      //Segnale per terminare un processo
#define CLOSE_LENGHT 4  //Lunghezza del segnale di terminazione che il DS invia ai peer
#define STDIN 0

int main(int argc, char* argv[]) {
  int ret,sd,maxFD;
  char* comando;
  fd_set read;
  /* Azzero i set */
  FD_ZERO(&read);
  numPeer=0;
  testa=NULL;
  coda=NULL;
  /* Alloco memoria dedicata al comando da inserire */
  comando = (char*)malloc(sizeof(char) * 30);
  /* Struttura descrittore del socket del DS */
  struct sockaddr_in my_addr;
  /* Creo il socket (non bloccante) e preparo il sockaddr_in alla bind */
  sd = socket(AF_INET,SOCK_DGRAM,0);
  /* Alloco la memoria per il campo my_addr e lo inizializzo per l'UDP*/
  unsigned short porta=atoi(argv[1]);
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family= AF_INET;
  my_addr.sin_port= htons(porta);
  my_addr.sin_addr.s_addr= INADDR_ANY;
  //Aggancio del socket, lo connetto ad internet
  ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
  if(ret<0)
    {
      //Gestione errore
      perror("Bind non riuscita: ");
      exit(0);
    }
  //Verifica se il socket del processo padre va chiuso oppure no... sd.sock_close();
  printf("*********** DS COVID STARTED ***********\n\n\n");
  maxFD = (sd > STDIN)?sd:STDIN;
  while(1)
  {
    printf("*********** DS COVID ***********\n");
    printf("Digita un comando:\n");
    printf("1) help --> mostra i dettagli dei comandi \n");
    printf("2) showpeers --> mostra un elenco dei peer connessi\n");
    printf("3) showneighbor <peer> --> mostra i neighbor di un peer\n");
    printf("4) esc --> chiude il DS\n");
    memset(comando,0,30);
    //Preparo la select per prendere il primo pronto tra lo stdin e il socket di ascolto per la connessione dei peer
    while(1){
      FD_ZERO(&read);
      FD_SET(sd,&read);
      FD_SET(STDIN,&read);
      if(select(maxFD+1,&read,NULL,NULL,NULL)){
        if(FD_ISSET(sd, &read)){
          ascoltopeer(sd);
          continue;
        }
        if(FD_ISSET(STDIN, &read))
          break;
      }
    }
    fgets(comando, 29, stdin);
    if(strncmp("help", comando,4) == 0){
      help();
      getchar();
    }
    else if(strncmp("showpeers",comando,9)==0){
      showpeers();
      getchar();
    }    
    else if(strncmp("showneighbor",comando,12)==0){
        if(strlen(comando)<=13){
          showneighbor();
          getchar();
        }
        else if(strlen(comando)>=17 && atoi(&comando[13])>0 && atoi(&comando[13])<65535){
          showneighborSpec(atoi(&comando[13]));
          getchar();
        }
        else
        printf("Formato non comprensibile, riprova\n");
    }
    else if(strncmp("esc",comando,3)==0){
        esc(sd);
      }
    else{
      printf("Comando errato, riprova\n");
    }
    //aggiungo il descrittore del socket al set
  }
}


//Funzione adibita all'inserimento di un peer nella lista contenente i peer della rete
void inserisciPeer(char* ip,uint16_t porta){
  struct codaPeer* punt;
  punt=(struct codaPeer*)malloc(sizeof(struct codaPeer));
  //Creo la struttura e la inizializzo
  punt->ipPeer=(char*)malloc(sizeof(char)*15);
  if(punt->ipPeer==0){
    printf("Allocazione stringa per ip fallita\n");
  }
  strcpy(punt->ipPeer,ip);
  punt->porta=porta;
  punt->prossimo=NULL;
  numPeer++;
  //Se la coda è ancora vuota lo metto come primo e termino
  if(testa==NULL){
    testa=punt;
    coda=punt;
    return;
  }
  struct codaPeer* prec=NULL;
  for(struct codaPeer* cursore=testa; cursore!=NULL;cursore=cursore->prossimo){
    //Quando trovo in coda una porta maggiore devo inserirlo al posto di questo facendo scalare la restante coda
    if(punt->porta==cursore->porta){
      printf("Porta in conflitto\n");
      free(punt);
      return;
    }
    if(punt->porta<cursore->porta){
      if(cursore==testa){
        //Se siamo in testa alla coda
        testa=punt;
        punt->prossimo=cursore;
        return;
      }
      else{
        punt->prossimo=cursore;
        prec->prossimo=punt;
        return;
      }
    }
    prec=cursore;
  }
  //Arrivato a questo punto se non ho ancora fatto nulla significa che il nuovo peer va messo in coda
  coda->prossimo=punt;
  coda=punt;
  return;
}


//Funzione adibita alla rimozione di un peer dalla lista dei peer connessi sia in caso di disconnessione del peer
//Sia di terminazione forzata causada dal DS
int removePeer(uint16_t porta){
  struct codaPeer* cursore,*prec;
  if(numPeer==0){
    printf("Nessun peer connesso, impossibile rimuovere\n");
    return 0;
  }
  if(numPeer==1 && testa->porta==porta){
    free(testa);
    testa=NULL;
    coda=NULL;
    numPeer--;
    return 1;
  }
  //Caso particolare rimozione elemento in testa
  if(testa->porta==porta){
    cursore=testa;
    testa=testa->prossimo;
    free(cursore);
    numPeer--;
    return 1;
  }
  //Caso generico, elemento in lista
  for(cursore=testa;cursore!=NULL;cursore=cursore->prossimo){
    if(cursore->porta==porta){
      prec->prossimo=cursore->prossimo;
      if(cursore==coda)
        coda=prec;
      free(cursore);
      numPeer--;
      return 1;
    }
    prec=cursore;
  }
  printf("Non sono riuscito a rimuovere il peer verifica che sia nella rete\n");
  return 0;
}

/* Funzione ascoltopeer(), funzione eseguita solo dal processo figlio che si dedicherà ad ascoltare
   ed interpretare le informazioni inviate dai peer. Gestirà la coda dei peer collegati alla rete e notificherà
   ai peer la necessità/possibilità di collegarsi ad un nuovo peer eventualmente scollegandosi da uno gia 
   connesso*/
void ascoltopeer(int sd){
  int ret,primoVicino, secondoVicino, numVicini,app1,app2;
  char* buffer,*ip;
  struct sockaddr_in connecting_addr;
  ret=0;
  buffer=(char*)malloc(sizeof(char)*4);
  /* Devo ricevere un pacchetto di cui non so le dimensioni, potrebbe essere una richiesta di connessione di
     un nuovo peer oppure un inoltro di dati aggregati*/
  unsigned int addrlen;
  addrlen= sizeof(connecting_addr);
  ret = recvfrom(sd, buffer, 4, 0, (struct sockaddr*) &connecting_addr,&addrlen);
  if(ret<0){
    printf("Problema nella ricezione del messaggio\n");
    return;
    }
  //In questo momento sto gestendo un pacchetto e potrei anche dover manipolare delle code, il processo non deve essere arrestabile
  //Se arrivo qua significa che ho ricevuto un pacchetto, adesso devo interpretarlo
  //Se il messaggio ricevuto è 'CON\0' significa che il peer sta tentando di connettersi
  if(strncmp("CON\0",buffer,3)==0){
    int ret;
    static char *appoggio;
    static char* str,*agg;
    appoggio=(char*)malloc(sizeof(char)*35);
    str=(char*)malloc(10);
    agg=(char*)malloc(15);
    memset(agg,0,15);
    memset(appoggio,0,35);
    ip=(char*)malloc(sizeof(char)*16);
    inet_ntop(AF_INET,&connecting_addr.sin_addr,ip,16);
    inserisciPeer(ip,(uint16_t)ntohs(connecting_addr.sin_port));
    free(ip);
    //Adesso devo preparare una stringa contenente i numeri di porta dei peer vicini
    //Chiamo una funzione che si occupa di salvare nella stringa i numeri di porta dei peer vicini secondo il formato:
    // 'porta1 porta2'
    appoggio=neighbors(appoggio,(uint16_t)ntohs(connecting_addr.sin_port));
    while(1){
      ret=sendto(sd,appoggio,strlen(appoggio),0,(struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
    //Adesso che ho inserito il nuovo peer in rete e inviato i vicini che può contattare devo fare in modo che anche gli, al più due, altri
    //Sappiano che si devono disconnettere da un vicino (se hanno instaurato una connessione TCP) per connettersi al nuovo vicino

    //Caso in cui ci sia solo un peer nella rete, non importa che esegua del codice per cercare di aggiornare i neighbor dei suoi vicini
    if(numPeer==1){
      return;
    }

    //Caso con almeno due peer nella rete, cerco i vicini del peer appena connesso
    numVicini=cercaVicini(&primoVicino,&secondoVicino,(uint16_t)ntohs(connecting_addr.sin_port));
    //Adesso nelle due variabili ho il numero di porta dei peer a cui devo notificare la nuova variazione dei neighbor
    strcat(agg,"UP ");
    ret=cercaVicini(&app1,&app2,primoVicino);
    if(ret==1){
      sprintf(str,"%d",app1);
      strcat(agg,str);
    }
    else if(ret==2){
      sprintf(str,"%d ",app1);
      strcat(agg,str);
      sprintf(str,"%d",app2);
      strcat(agg,str);
    }
    else{
      printf("Bug, verifica il codice\n");
      return;
    }
    //Aggiorno la porta, l'ip rimarrà lo stesso dato che è tutto simulato in locale
    connecting_addr.sin_port=(uint16_t)ntohs(primoVicino);
    //Invio il Msg
    while(1){
      ret=sendto(sd,agg,strlen(agg),0,(struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
    //Se c'è un solo peer da aggiornare ho terminato
    if(numVicini==1)
      return;
    memset(agg,0,15);
    //Adesso aggiorno anche il secondo peer
    strcat(agg,"UP ");
    ret=cercaVicini(&app1,&app2,secondoVicino);
    if(ret==1){
      sprintf(str,"%d",app1);
      strcat(agg,str);
    }
    else if(ret==2){
      sprintf(str,"%d ",app1);
      strcat(agg,str);
      sprintf(str,"%d",app2);
      strcat(agg,str);
    }
    else{
      printf("Bug, verifica il codice\n");
      return;
    }
    //Invio il Msg
    connecting_addr.sin_port=(uint16_t)ntohs(secondoVicino);
    while(1){
      ret=sendto(sd,agg,strlen(agg),0,(struct sockaddr*)&connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
    free(agg);
    free(appoggio);
    free(str);
  }
    //Se il messaggio ricevuto è 'CLS\0' significa che il peer vuole disconnettersi dalla rete
  else if(strncmp("CLS",buffer,3)==0){
    //Nel caso in cui sia l'unico peer connesso lo rimuovo dalla lista e ritorno
    char buf[3],msg[18];
    char* appoggio;
    int sogg,app1,app2,numVicini,ret;
    strcpy(buf,"OK\0");
    appoggio=0;
    appoggio=(char*)malloc(sizeof(char)*16);
    memset(appoggio,0,16);
    char ip[16];
    inet_ntop(AF_INET,&connecting_addr.sin_addr,ip,16);
    while(1){
      ret=sendto(sd,buf,3,0,(struct sockaddr*) &connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
    //Prendo la porta del peer da rimuovere dalla lista
    sogg=(uint16_t)ntohs(connecting_addr.sin_port);
    //Se ero l'unico non ho nulla da notificare
    if(numPeer==1){
      removePeer(sogg);
      return;
    }
    //Cerco i vicini a cui devo notificare la modifica dei neighbor
    numVicini=cercaVicini(&app1,&app2,sogg);
    //Identificati i neighbor posso rimuovere il peer dalla lista
    removePeer(sogg);
    //Notifico ai vicini i/il nuovi/o neighbor
    appoggio=neighbors(appoggio,app1);
    strcpy(msg,"UP \0");
    strcat(msg,appoggio);
    connecting_addr.sin_port=(uint16_t)htons(app1);
    while(1){
      ret=sendto(sd,msg,18,0,(struct sockaddr*) &connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
    //C'è un secondo vicino o sono rimasti in due?
    if(numVicini==1)
      return;
    //Proseguo col secondo vicino
    appoggio=neighbors(appoggio,app2);
    strcpy(msg,"UP \0");
    strcat(msg,appoggio);
    connecting_addr.sin_port=(uint16_t)htons(app2);
    //Invio il messaggio
    while(1){
      ret=sendto(sd,msg,18,0,(struct sockaddr*) &connecting_addr, sizeof(connecting_addr));
      if(ret>=0){
        break;
      }
    }
  return;
  }
}

/* Funzione che mostra il significato dei comandi elencati e ciò che questi fanno */
void help(){
  printf("Descrizione comandi:\n");
  printf("-->   showpeers : Mostra l'elenco dei peer connessi alla rete, sono identificabili tramite il loro numero di porta\n");
  printf("-->   showneighbor <peer> : Mostra i neighbor di un peer specificato da 'peer', se questo parametro viene omesso si specifica i neighbor di tutti i peer\n");
  printf("-->   esc : Comando di terminazione del DS, invia un messaggio di terminazione anche ai peer\n");
  return;        
}

/* Funzione che svolge i compiti dell'omonimo comando: mostra un elenco di peer connessi identificandoli
   tramite il loro numero di porta*/
void showpeers(){
  int i=1;
  if(testa==NULL){
    printf("Nessun peer collegato al momento, riprova più tardi\n");
    return;
  }
  printf("          PeerIP    Porta\n");
  for(struct codaPeer* punt=testa; punt!=NULL;punt=punt->prossimo,i++){
    printf("%d:   %s    %d\n",i,punt->ipPeer,punt->porta);
  }
  return;
}

/* Funzione che svolge i compiti dell'omonimo comando (versione con parametro): mostra i vicini del peer 
   con la porta indicata dall'utente, se la porta non esiste restituisce un messaggio di errore */
void showneighborSpec(int peerNum){
  struct codaPeer* cursore;
  if(numPeer==1){
    printf("Solo un peer connesso al momento, nessun neighbor da mostrare\n");
    return;
  }
  //Unico caso in cui i vicini possano essere meno di due in una rete ad anello
  if(numPeer==2){
    if(testa->porta==peerNum){
      printf("Il peer %d ha solo il vicino %d a causa dei pochi peer\n",peerNum,coda->porta);
      return;
    }
    else if(coda->porta==peerNum){
      printf("Il peer %d ha solo il vicino %d a causa dei pochi peer\n",peerNum,testa->porta);
      return;
    }
    else{
      printf("Peer: %d non presente all'interno della rete, riprova più tardi\n",peerNum);
      return;
    }
  }
  //Devo scorrere la lista circolare fino a trovare il peer con la porta indicata
  for(cursore=testa;cursore->prossimo!=NULL;cursore=cursore->prossimo){
    //Caso 1° peer della lista, devo gestirlo come una coda circolare
    if(testa->porta==peerNum){
      printf("Il peer %d ha come vicini: %d e %d\n",peerNum,coda->porta,cursore->prossimo->porta);
      return; 
    }
    //Caso ultimo elemento della lista
    if(coda->porta==cursore->prossimo->porta && coda->porta==peerNum){
      printf("Il peer %d ha come vicini: %d e %d\n",peerNum,cursore->porta,testa->porta); 
      return;
    }
    //Se il prossimo peer è quello richiesto i vicini saranno "cursore e cursore + 2 posizioni nella lista"
    if(cursore->prossimo->porta==peerNum){
      printf("Il peer %d ha come vicini: %d e %d\n",peerNum,cursore->porta,cursore->prossimo->prossimo->porta);
      return;
    }
  }
  //Se non lo trovo il peer non esiste o non si è ancora connesso alla rete
  printf("Peer: %d non presente all'interno della rete, riprova più tardi\n",peerNum);
}

/* Funzione che svolge i compiti dell'omonimo comando (versione senza parametro): mostra i vicini di ogni 
   peer */
void showneighbor(){
  if(testa==NULL){
    printf("Nessun peer connesso al momento, ritenta più tardi");
    return;
  }
  if(numPeer==1){
    printf("Solo il peer %d connesso\n",testa->porta);
    return;
  }
  if(numPeer==2){
    printf("Il peer %d ha come unico vicino: %d\n",coda->porta,testa->porta);
    printf("Il peer %d ha come unico vicino: %d\n",testa->porta,coda->porta);
    return;
  }
  struct codaPeer* cursore;
  struct codaPeer* prec;
  //Ciclo in cui ad ogni giro mostro i vicini di ogni peer
  for(cursore=testa;cursore->prossimo!=NULL;cursore=cursore->prossimo){
      if(testa==cursore){
        printf("Il peer %d ha come vicini: %d e %d\n",testa->porta,coda->porta,cursore->prossimo->porta);
        prec=cursore;
        continue;
      }
      printf("Il peer %d ha come vicini: %d e %d\n",cursore->porta,prec->porta,cursore->prossimo->porta);
      prec=cursore;
      if(cursore->prossimo==coda){
        printf("Il peer %d ha come vicini: %d e %d\n",coda->porta,cursore->porta,testa->porta);
      }
    }
  return;
  }

/* Funzione di terminazione del DS, invia un messaggio di terminazione a tutti i peer che dovranno chiudersi
   a loro volta. Opzionalmente possono salvare le loro informazioni su un file ricaricato al boot del peer */
void esc(int sd){
  int esito, ret, len,vicino1,vicino2;
  unsigned int sock_len;
  char buffer[4];
  struct sockaddr_in recver;
  struct codaPeer* indice=testa;
  recver.sin_family = AF_INET;
  recver.sin_addr.s_addr = inet_addr("localhost");
  printf("/****   DISCONNESSIONE AVVIATA   ****/");
  while(indice!=NULL){
    vicino1=vicino2=-1;
    short porta=indice->porta;
    printf("Disconnetto il peer: %d\n",porta);
    cercaVicini(&vicino1,&vicino2,porta);
    //Rimuovo il peer dalla lista dei peer e libero la memoria da lui occupata
    esito=removePeer((uint16_t)porta);
    if(esito==0){
      printf("Impossibile rimuovere il peer %d\n",porta);
      indice=testa;
      continue;
    }
    //Procedo a notificare al peer la sua rimozione dalla lista a causa della chiusura del DS
    recver.sin_port=htons(porta);
    strcpy(buffer,"CLS\0");
    len=CLOSE_LENGHT;
    ret=-1;
    sock_len=sizeof(recver);
    while(1){
      ret=sendto(sd,buffer,len,0,(struct sockaddr*)&recver,sock_len);
      if(ret==CLOSE_LENGHT){
        break;
      }
      perror("Invio non riuscito, nuovo tentativo.\n");
      sleep(1);
    }
    //Messaggio inviato attendo la ricezione dell'ACK
    ret=recvfrom(sd, buffer,3,0,(struct sockaddr*)&recver,&sock_len);
    //Aggiorno i peer rimanenti dell'uscita del peer
    if(numPeer>0)
      updatePeer(sd,&vicino1,&vicino2);
    printf("********************************************************************\n");
    printf("Peer disconnesso con successo, procedo a disconnettere il successivo\n");
    printf("********************************************************************\n\n");
    sleep(2);
    indice=testa;
  }
  close(sd);
  exit(1);
}

//Funzione adibita a prendere i numeri di porta dei due peer neighbor del peer con porta portaConn
char* neighbors(char* string,uint16_t portaConn){
  struct codaPeer* punt, *prec;
  char* app;
  app=(char*)malloc(sizeof(char)*35);
  memset(app,0,35);
  //Caso in cui ci sia un solo peer al momento nella rete, non avrà neighbor quindi gli invio 'NULL\0'
  if(numPeer==1){
    string="NULL\0";
    return string;
  }
  if(numPeer==2){
    if(testa->porta==portaConn){
      sprintf(app,"%d",coda->porta);
      strcpy(string,app);
      return string;
    }
    if(coda->porta==portaConn){
      sprintf(app,"%d",testa->porta);
      strcat(string,app);
      return string;
    }
    free(app);
  }

  //Abbiamo più di un peer attualmente connesso, devo fare la ricerca nella lista
  for(punt=testa,prec=punt;punt!=NULL;punt=punt->prossimo){
    //Se quello a cui punta 'punt' ha la porta cercata significa che abbiamo trovato il peer cercato nella lista
    if(punt->porta==portaConn){
      //Significa che siamo ancora in testa, i suoi neighbor saranno punt->prossimo->porta e coda->porta
      if(prec==punt){
        sprintf(app,"%d",coda->porta);
        strcat(string,app);
        strcat(string," ");
        sprintf(app,"%d",punt->prossimo->porta);
        strcat(string,app);
        return string;
      }
      //Altrimenti caso in cui siamo sull'elemento in coda
      else if(punt==coda){
        sprintf(app,"%d",prec->porta);
        strcat(string,app);
        strcat(string," ");
        sprintf(app,"%d",testa->porta);
        strcat(string,app);
        return string;
      }
      //Caso generico nel mezzo alla lista
      else{
        sprintf(app,"%d",prec->porta);
        strcat(string,app);
        strcat(string," ");
        sprintf(app,"%d",punt->prossimo->porta);
        strcat(string,app);
        return string;
      }
    }
    prec=punt;
  }
  return string;
}


//Funzione che si occupa di cercare i vicini (se ce ne sono) del peer sogg e li restituisce nelle variabili passate per riferimento,
//restituisce il numero di vicini trovati
int cercaVicini(int* primoVicino,int* secondoVicino,int sogg){
  struct codaPeer* punt, *prec;
  if(numPeer==1){
    return 0;
  }
  if(numPeer==2 && testa->porta==sogg){
    *primoVicino=coda->porta;
    return 1;
  }
  if(numPeer==2 && coda->porta==sogg){
    *primoVicino=testa->porta;
    return 1;
  }
  punt=testa,prec=testa;
  //Cerco il peer neoentrato in rete
  for(;punt!=NULL;punt=punt->prossimo){
    if(punt->porta==sogg)
      break;
    prec=punt;
  }
  //prec punt al peer precedente in lista, mentre il successivo è accessibile tramite punt->prossimo->porta
  if(punt==testa){
    *primoVicino=coda->porta;
    *secondoVicino=punt->prossimo->porta;
  }
  else if(punt==coda){
    *primoVicino=prec->porta;
    *secondoVicino=testa->porta;
  }
  else{
    *primoVicino=prec->porta;
    *secondoVicino=punt->prossimo->porta;
  }
  return 2;
}

//Funzione che si occupa di notificare ai peer l'uscita di altri peer conseguentemente ad una esc del discoveryServer
void updatePeer(int sd,int* vicino1, int* vicino2){
  int up1,up2,n, addrlen, ret;
  static char * agg, *str;
  struct sockaddr_in connecting_addr;
  connecting_addr.sin_family = AF_INET;
  connecting_addr.sin_addr.s_addr = inet_addr("localhost");
  addrlen= sizeof(connecting_addr);
  str=(char*)malloc(15);
  agg=(char*)malloc(15);
  memset(agg,0,15);
  memset(str,0,15);
  strcat(agg,"UP ");
  n=cercaVicini(&up1,&up2,*vicino1);
  if(n==1){
      sprintf(str,"%d",up1);
      strcat(agg,str);
    }
    else if(n==2){
      sprintf(str,"%d ",up1);
      strcat(agg,str);
      sprintf(str,"%d",up2);
      strcat(agg,str);
    }
    else{
      sprintf(agg,"UP -1 -1");
    }
    connecting_addr.sin_port=(uint16_t)ntohs(*vicino1);
    while(1){
      ret=sendto(sd,agg,strlen(agg),0,(struct sockaddr*)&connecting_addr,addrlen);
      if(ret>0)
        break;
    }
  //Faccio lo stesso per il secondo vicino del peer appena uscito dalla rete
  strcpy(agg,"UP ");
  n=cercaVicini(&up1,&up2,*vicino2);
  if(n==1){
      sprintf(str,"%d",up1);
      strcat(agg,str);
    }
    else if(n==2){
      sprintf(str,"%d ",up1);
      strcat(agg,str);
      sprintf(str,"%d",up2);
      strcat(agg,str);
    }
    else{
      sprintf(agg,"UP -1 -1");
    }
    connecting_addr.sin_port=(uint16_t)ntohs(*vicino2);
    while(1){
      ret=sendto(sd,agg,strlen(agg),0,(struct sockaddr*)&connecting_addr,addrlen);
      if(ret>0)
        break;
    }
    return;
}
