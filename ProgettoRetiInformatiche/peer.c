#include <time.h>
#include "utility.h"
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h> 
#define __USE_XOPEN
//Definizione di costanti
#define STDIN 0

//Visibilità globale
char parole[4][40];
int primoVicino,secondoVicino,connected;
struct sockaddr_in ds,peer;
FILE *ptr;
char currentDate[12];
struct sockaddr_in ngbr;
int accpt1, accpt2;
uint16_t porta;

//Dichiarazione di funzioni chiamate nel main ma definite sotto
void help();
void start(int sd);
void add();
void gestisciMsgDS(int sd);
int gestisciMsgPeer(int listener);
void get();
void stop(int sd);
void scambioDati(int sdN);
void cls(int sd);
void creaRegistro(int peer, char *data, int T, int N, int CT, int CN);
bool scriviSuFile(char* percorso, char elemento[N_ELEM][DIM_ELEM]);
FILE* creaRegistroOdierno(int porta,char *strData);
bool verificaEsistenza(char *path_aggr,char data[N_ELEM][DIM_ELEM]);
bool verificaEsistenzaVar(char *path_aggr_tot,int num_diff,char **data_var);
bool connettiTCP(int Vicino,char tipoRichiesta,char tipoDato,int temp,int _porta,char* load,int *valoreEntry,char **data_var);
bool inviaRecord();

int main(int argc, char* argv[]){
    int ret,sd,maxFD,sdN,listener,data[3];
    time_t today;
    //Set dati per creare dei registri già esistenti
    char *date[]={"10-8-2021.txt","12-8-2021.txt","14-8-2021.txt","16-8-2021.txt"};
    int T, N, CT=1570, CN=513,i,j;
    char *comando;
    char strData[12],app[5];
    char portaStr[20];
    struct sockaddr_in local;
    int numParole;
    struct tm* timeinfo;
    fd_set msg;
    FD_ZERO(&msg);
    //Conta le parole extra rispetto al comando, aiuta a verificare la digitazione corretta del comando
    numParole=0;
    //Viene settata ad 1 quando la start si completa con successo
    connected=0;
    comando=0;
    accpt1=0;
    accpt2=0;
    sdN=0;
    ptr=NULL;
    comando=(char*)malloc(sizeof(char)*40);
    //Prendo la porta adibita al peer dai parametri di entrata
    strcpy(portaStr,argv[1]);
    porta=atoi(argv[1]);
    //Creo la cartella (se non esiste per ogni peer)
    opendir(portaStr);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(portaStr,0x0777);
        opendir(portaStr);
    }
    //Creo i files per i vari peer
    for(j=0; j<4; j++){
        for(i = 5001; i < 5006; i++){
            switch (i){
                case 5001:
                    T = 10;
                    N = 7;
                    break;
                case 5002:
                    T = 750;
                    N = 500;
                    break;
                case 5003:
                    T = 300;
                    N = 100;
                    break;
                case 5004:
                    T = 550;
                    N = 70;
                    break;
                case 5005:
                    T = 65;
                    N = 40;
                    break;                  
                }
            if(i == 5001){
                    if(j == 3)
                        creaRegistro(i,date[j], T, N, CT, 0);
                    else
                        creaRegistro(i,date[j], T, N, 0, 0);         
            }
            else if(i == 5002){
                if(j == 1)
                    creaRegistro(i,date[j], T, N, CT, CN);
                else
                    creaRegistro(i,date[j], T, N, 0, 0);
            }
            else if(i == 5003){
                if(j == 0)
                    creaRegistro(i,date[j], T, N, 0, CN);
                else
                    creaRegistro(i,date[j], T, N, 0, 0);
            }
            else{
                creaRegistro(i,date[j], T, N, 0, 0);
                if(j == 2 && i == 5004)
                    creaRegistro(i,date[j], 165, N, 0, 0);
            }
        } 
    }
    //Popolazione eseguita, creo il registro di oggi
    time(&today);
    timeinfo = localtime(&today);
    data[0] = timeinfo->tm_mday;
    data[1] = timeinfo->tm_mon+1;
    data[2] = timeinfo->tm_year+1900;
    memset(app,0,strlen(app));
    memset(strData,0,strlen(strData));
    sprintf(strData,"%d",data[0]);
    strcat(strData,"-");
    sprintf(app,"%d",data[1]);
    strcat(strData,app);
    strcat(strData,"-");
    memset(app,0,strlen(app));
    sprintf(app,"%d",data[2]);
    strcat(strData,app);
    ptr=creaRegistroOdierno(porta,strData);
    //Creo la cartella contenente i dati di questo peer salvati su altri peer
    //Prendo la porta adibita al peer dai parametri di entrata
    strcpy(portaStr,argv[1]);
    strcat(portaStr,"/saved/");
    //Creo la cartella (se non esiste per ogni peer)
    opendir(portaStr);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(portaStr,0x0777);
        opendir(portaStr);
    }
    //Inizializzo la struttura dedicata al socket locale
    local.sin_family=AF_INET;
    local.sin_port=htons(porta);
    local.sin_addr.s_addr= INADDR_ANY;
    //Socket per connssione TCP tra i peer
    listener = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    ret = bind(listener, (struct sockaddr *)&local, sizeof(local));
    if(ret < 0){
        perror("Bind non riuscita: ");
        exit(EXIT_FAILURE);
    }
    //Creo il socket del peer, lo aggancio e lo metto in ascolto
    sd = socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,0);
    ret = bind(sd, (struct sockaddr*)&local, sizeof(local));
    if(ret<0)
    {
      //Gestione errore
      perror("Bind non riuscita: ");
      exit(EXIT_FAILURE);
    }

    //Metto il socket in ascolto con una coda di 10
    listen(listener,10);
    
    printf("*********** COVID PEER STARTED ***********\n\n\n");
    //Apro il file su cui devo scrivere i dati che salvo
    //fopen() Come si può dire ad un peer diverso di aprire un file diverso? Devo preparare una stringa che rappresenti il path e che risulti
    //        diverso per ogni peer
    //Preparo il set per aggiungerci tutte le cose che la select deve controllare
    maxFD = (sd > STDIN)?sd:STDIN;
    maxFD = (maxFD > listener)? maxFD:listener;
    //Ciclo infinito per gestire le richieste
    while(1){
        printf("*********** COVID PEER ***********\n");
        printf("Digita un comando:\n");
        printf("1) help\n");
        printf("2) start DS_Addr DS_Port\n");
        printf("3) add type quantity\n");
        printf("4) get aggr type period\n");
        printf("5) stop\n");
        memset(comando,0,40);
        //Preparo la select per prendere il primo pronto tra lo stdin e il socket di ascolto per la connessione dei peer
        while(1){
            FD_ZERO(&msg);
            FD_SET(sd,&msg);
            FD_SET(listener,&msg);
            FD_SET(STDIN,&msg);
            if(select(maxFD+1,&msg,NULL,NULL,NULL)){
                if(FD_ISSET(sd, &msg)){
                    gestisciMsgDS(sd);
                    continue;
                }
                else if(FD_ISSET(listener,&msg)){
                    //Funzione di accettazione richiesta di connessione
                    sdN=gestisciMsgPeer(listener);
                    //Eseguo lo scambio dati sul nuovo socket dedicato alla connessione TCP
                    scambioDati(sdN);
                    //Reset del socket listener
                    continue;
                }
                else if(FD_ISSET(STDIN,&msg))
                    break;
            }
        }
        fgets(comando, 40, stdin);
        if(strncmp("help", comando,4) == 0){
            help();
            getchar();
        }
        else if(strncmp("start ",comando,6)==0){
            numParole=parsing(comando);
            if(numParole==2){
                start(sd);
                //Procedo a creare una connessione TCP con i peer passati dal DS (Sempre se ve ne sono)
                /* ... */
            }
            else
                printf("Formato errato, riprova\n");
        }
        else if(strncmp("add ",comando,4)==0){
            numParole=parsing(comando);
            if(numParole==2){
                add();
            }
            else
                printf("Formato errato, riprova\n");
        }
        else if(strncmp("get ",comando,4)==0){
            numParole=parsing(comando);
            if(numParole==4){
                get();
            }
            else
                printf("Formato errato, riprova\n");
        }
        else if(strncmp("stop",comando,4)==0){
            stop(sd);
            if(connected==1)
                printf("Operazione di notifica al DS e invio dei dati ai peer completata, premi un pulsante per terminare...\n");
            else
                printf("Non eri connesso alla rete, nessuna operazione da effettuare. Premi un pulsante per terminare...\n");
            getchar();
            exit(EXIT_SUCCESS);
        }
        else if(strncmp("shw",comando,3)==0){
            printf("%d  -%d\n",primoVicino,secondoVicino);
        }
        else{
            printf("Hai digitato un comando non comprensibile\n");
        }
    }
}
/* Funzione dedicata a gestire il comando 'help', mostra a schermo i possibili comandi con le funzionalità che rappresentano */
void help(){
    printf("Descrizione comandi:\n");
    printf("-->   start DS_addr DS_Port: Richiede al DS in ascolto all'indirizzo DS_addr:DS_Port specificato di connettersi alla rete di peer\n");
    printf("-->   add type quantity : Aggiunge la entry al register del peer se la entry inserita appartiene ad un register al momento aperto\n");
    printf("-->   get aggr type period : Richiede ai peer vicini l'invio di un dato aggregato, o i dati che mancano. Se non è sufficiente eseguirà un flooding di richieste\n");
    printf("-->   stop : Permette al peer di uscire dalla rete. Prima di farlo notifica al DS la sua volontà e condivide i dati raccolti ai due neighbor prima di salvarli su file\n");
    return;        
}


/*Funzione deidicata a svolgere le funzioni del comando start. Deve inviare un messaggio al DS per richiedere di connettersi e ricevere un ACK
    che nel nostro caso conterrà le porte (dato che siamo in localhost) dei peer neighbor, le salva e torna al main*/
void start(int sd){
    if(connected==1){
        printf("Sei gia connesso alla rete\n");
        return;
    }
    int ret,tentativi,numPar;
    unsigned int len;
    char *buffer,*app;
    buffer=0,app=0;
    buffer = (char*)malloc(sizeof(char)*35);
    app = (char*)malloc(sizeof(char)*35);
    memset(buffer,0,35);
    memset(app,0,35);
    buffer="CON\0";
    tentativi=0;
    //Preparo il socket che deve contenere l'ip
    ds.sin_family=AF_INET;
    ds.sin_port=htons(atoi(parole[1]));
    ds.sin_addr.s_addr=inet_addr(parole[0]);
    len=sizeof(ds);
    //Mando la richiesta al DS specificato di entrare in rete, ha 10s per rispondermi
    while(1){
        sendto(sd,buffer,strlen(buffer),0,(struct sockaddr*)&ds, sizeof(ds));
        //Sospensione di 1 secondo per permettere al DS di rispondermi
        sleep(1);
        //Tentativo di ricezione del messaggio
        ret = recvfrom(sd, app, 35, 0, (struct sockaddr*) &ds,&len);
        //ret>=0 => Messaggio ricevuto con successo, posso procedere ad interpretare il messaggio
        if(ret>=0){
            break;
        }
        //Limite massimo di 10 secondi, permette al peer di non sospendersi all'infinito nel caso di inserimento sbagliato di indirizzo del
        //DS o della porta del DS
        if(tentativi==10){
            printf("Impossibile contattare il ds specificato, ritenta\n");
            return;
        }
        tentativi++;
        printf("Tentativo: %d\n",tentativi);
    }
    //Comportamento in base ad app ricevuto
    
    //Mi sono connesso alla rete, il DS ha ricevuto la mia richiesta e mi ha risposto con i miei vicini (Se ce ne sono), non ho più bisogno di eseguire la start
    connected=1;
    numPar=0;
    //Vado a salvare i neighbor ricevuti nelle variabili adibite a contenere le porte dei neighbor
    //Caso in cui sia il primo peer connesso alla rete
    if(strncmp(app,"NULL",4)==0){
        primoVicino=-1;
        secondoVicino=-1;
        return;
    }
    //Eseguo il parsing della stringa ricevuta
    numPar=parsingMsg(app);
    numPar++;
    //numPar conterrà il numero di parole della stringa che potranno essere 2 (se ci sono almeno altri 2 peer nella rete) o 1 (se sono il secondo peer a connettersi)
    if(numPar==1){
        primoVicino=atoi(parole[0]);
        secondoVicino=-1;
    }
    if(numPar==2){
        primoVicino=atoi(parole[0]);
        secondoVicino=atoi(parole[1]);
    }
    //Sono in rete, fine della funzione della start

    printf("Connessione effettuata con successo.\nACK ricevuto con neighbor: %d - %d\n",primoVicino,secondoVicino);
    return;
}

/* Aggiunge una entry relativa al register non ancora chiuso (dalle 18 del giorno prima alle 18 di oggi), la entry avrà formato "type: quantity;".
   Il record viene inoltre salvato in un altro register nella cartella saved al fine di non perderere la tracciabilità dei casi e temponi inseriti dal singolo peer dato
   che se un peer esegue la stop invia i propri dati al primo vicino e per coerenza deve eliminarli dal suo register*/
void add(){
    char path[DIM_PATH],pathSaved[DIM_PATH],file_name[DIM_ELEM];
    char elementi[N_ELEM][DIM_ELEM];
    time_t _time;
    struct tm *today;
    int app,i,val;
    bool esiste;
    //Creo il path totale
    memset(path,0,sizeof(path));
    memset(pathSaved,0,sizeof(pathSaved));
    sprintf(path,"%d/",porta);
    sprintf(pathSaved,"%d/saved/",porta);
    //Preparo la stringa della data odierna
    time(&_time);
    today=localtime(&_time);
    //Se sono passate le ore 18 devo salvarlo nel register del giorno dopo
    if(today->tm_hour>18){
        today->tm_mday++;
    }
    mktime(today);
    memset(file_name,0,sizeof(file_name));
    sprintf(file_name,"%d-%d-%d.txt",today->tm_mday,today->tm_mon+1,today->tm_year+1900);
    strcat(path,file_name);
    strcat(pathSaved,file_name);
    //Stringhe relative al percorso pronte
    val=atoi(parole[1]);
    esiste=verificaEsistenza(path,elementi);
    if(esiste){
        printf("Registro esistente, aggiorno l'entry\n");
        for(i=0;i<2;i++){
            app=prendi_entry(i,2,elementi);
        }
        if(strcmp(parole[0],"T")==0){
            sprintf(elementi[0],"T:%d;\n",app+val);
        }
        else
            sprintf(elementi[1],"N:%d;\n",app+val);
    }
    //Nel caso in cui non trovi il registro odierno lo creo e lo inizializzo col valore inserito
    else{
        if(strcmp(parole[0],"T")==0){
            sprintf(elementi[0],"T:%d;\n",val);
            sprintf(elementi[1],"N:0;\n");
        }
        else{
            sprintf(elementi[0],"T:0;\n");
            sprintf(elementi[1],"N:%d;\n",val);
        }
        sprintf(elementi[2],"CT:0;\n");
        sprintf(elementi[3],"CN:0;\n");
    }
    esiste=scriviSuFile(path,elementi);
    if(!esiste)
        printf("Impossibile scrivere nel register odierno, ritenta\n");
    //Adesso aggiorno il register del peer nella sezione saved
    memset(elementi,0,sizeof(elementi));
    esiste=verificaEsistenza(pathSaved,elementi);
    if(esiste){
        printf("Registro esistente, aggiorno l'entry\n");
        for(i=0;i<2;i++){
            app=prendi_entry(i,2,elementi);
        }
        if(strcmp(parole[0],"T")==0){
            sprintf(elementi[0],"T:%d;\n",app+val);
        }
        else
            sprintf(elementi[1],"N:%d;\n",app+val);
    }
    //Nel caso in cui non trovi il registro odierno lo creo e lo inizializzo col valore inserito
    else{
        if(strcmp(parole[0],"T")==0){
            sprintf(elementi[0],"T:%d;\n",val);
            sprintf(elementi[1],"N:0;\n");
        }
        else{
            sprintf(elementi[0],"T:0;\n");
            sprintf(elementi[1],"N:%d;\n",val);
        }
        sprintf(elementi[2],"CT:0;\n");
        sprintf(elementi[3],"CN:0;\n");
    }
    esiste=scriviSuFile(pathSaved,elementi);
    if(!esiste)
        printf("Impossibile scrivere nel register (saved) odierno, ritenta\n");
    return;
}
/*Funzione che si occupa di cercare dentro la cartella 'aggr' del peer corrente per capire se il dato aggregato 'V' già è stato calcolato
    @path_aggr percorso a cui posso trovare i file che cerco
    @num_diff è il numero di differenze da trovare
    @data_var variabile iun cui inserire i dati se trovati
*/
bool verificaEsistenzaVar(char *path_aggr_tot,int num_diff,char **data_var){
    char* line=NULL; 
    int i=0;
    size_t dim_buf=0;
    FILE * f=fopen(path_aggr_tot,"r");
    if(!f){
        return false;
    }
    for(i=0;i<num_diff;i++){
        getline(&line,&dim_buf,f);
        strcpy(data_var[i],line);
    }
    free(line);
    fclose(f);
    return true;
}

/*Funzione che si occupa di cercare dentro la cartella del peer corrente per capire se il dato cercato esiste
    @path percorso a cui posso trovare i file che cerco
    @data variabile iun cui inserire i dati se trovati
*/
bool verificaEsistenza(char *path,char data[N_ELEM][DIM_ELEM]){
    //Conterrà la linea da leggere
    char* line=NULL; 
    int i=0;
    size_t dim_buf=0;
    FILE * f=fopen(path,"r");
    //Se l'apertura non è riuscita è perchè il file non esiste, ritorno false e procedo a calcolare il dato aggregato
    if(!f){
        return false;
    }
    //Il file esiste ed è stato aperto, leggo le righe contenenti il valore ricercato
    for(i=0;i<N_ELEM;i++){
        getline(&line,&dim_buf,f);
        strcpy(data[i],line);
    }
    free(line);
    fclose(f);
    return true;
}
/*Funzione che si occupa di scrivere su file i dati aggregati va differenziata in quanto i dati potrebbero essere su più righe
  @percorso è il path del file su cui devo scrivere
  @num_linee sono le linee da scrivere
  @data è la variabile contenente i dati da scrivere*/
bool scriviAggr(char* percorso,int num_linee,char **data){
    FILE * f=fopen(percorso, "w+");
    int n,i;
    //File inesistente
    if(!f)
        return false;
    //File esistente, scrivo i dati
    for(i=0;i<num_linee;i++){
        //Ogni giro sovrascrivo n con la lunghezza di questa row
        n=strlen(data[i]);
        //Scrivo sul file
        fwrite(data[i],1,n,f);
    }  
    //Chiudo il file 
    fclose(f);
    //Operazione effettuata con successo
    return true;    
}


//Funzione che si occupa di scrivere i vari elementi nei register
/*@percorso e' il path in cui andremo a scrivere le entry
  @elemento sono le varie entry da inserire */
bool scriviSuFile(char* percorso, char elemento[N_ELEM][DIM_ELEM]){
    int i,len,ret;
    //Tento di aprire il file nel caso lo creo
    FILE *file=fopen(percorso, "w+");
    for(i=0;i<4;i++){
        if(!file)
            return false;
        len=strlen(elemento[i]);
        ret=fwrite(elemento[i],1,len,file);
        if(ret<=0)
            perror("Errore: ");
    }
    fclose(file);
    return true;
}

/* Funzione che si occupa di gestire l'omonimo comando, in pratica cerca ed evenutalmente calcola i dati aggregati richiesti nelle date richieste */
void get(){
    //Variabili "indice" per cicli
    int i,j,n;
    //variabile lunghezza per stringa
    unsigned int len;
    int num_diff=0,insert,conta_date=0;
    //Variabili per settare correttamente gli * che possono essere inseriti nel periodo
    time_t oggi;
    struct tm * info;
    struct tm tm={0};
    time_t dataInizio,dataFine;
    //Variabili contenenti i path dei file
    char path_aggr[DIM_PATH],file[DIM_AGGR_FILENAME];
    char path_aggr_tot[DIM_PATH_AGGR_T];
    char path_reg[DIM_PATH];
    char path_app[DIM_PATH];
    char index_c[20],var_date[20];
    int valoreDaAggiungere=0;
    int tot=0,nrow=0,val=0,*variazioni=NULL;
    //Variabile contenente i dati già calcolati (Se ci sono)
    char data[N_ELEM][DIM_ELEM];
    time_t index;
    char tipoDato;
    //Variabili di appoggio funzionalità algoritmo
    char **data_tot= NULL;
    char **data_var= NULL;
    //Variabili booleane di controllo
    bool esiste=false,statoConn=false,aggr_chiesto=false;
    if(!connected){
        printf("Errore: non puoi eseguire una get senza prima connetterti al Discovery Server, esegui prima il comando 'start'\n\n");
        return;
    }
    memset(path_reg,0,sizeof(path_reg));
    memset(path_app,0,sizeof(path_app));
    if(strcmp(parole[1],"T")==0)
        tipoDato='T';
    else if(strcmp(parole[1],"N")==0)
        tipoDato='N';
    else{
        printf("Tipo dato sconosciuto\n");
        return;
    }

    //Sistemo le date nel formato che il programma si attende "dd-mm-yyyy"
    for(i=2;i<4;i++){
        //Caso in cui venga inserito un asterisco come data iniziale o data finale
        if((strcmp(parole[i],"*")==0) || (strcmp(parole[i],"*\n")==0) ){
            if(i==2){
                sprintf(parole[2],"%s", "4-3-2020");
                continue;
            }
            else{
                time(&oggi);
                info = localtime(&oggi);
                sprintf(parole[3],"%d-%d-%d",info->tm_mday,((info->tm_mon)+1),((info->tm_year)+1900));
                continue;
                }
        }
        j=0;
        len=strlen(parole[i]);
        //Caso in cui la data venga inserita manualmente dall'utente: Devo verificarne il formato
        for(;1;j++){
            if((i==2 && j==len) || (i==3 && j==len-1))
                break;
            //Finche trovo valori compresi tra 0 e 9 oppure il carattere di separazione va tutto bene
            if((parole[i][j]>='0' && parole[i][j]<='9') || parole[i][j]=='-')
                continue;
            //Rimuovo il carattere di ritorno carrello che si trova in fondo alla seconda data
            else if(parole[i][j]=='\n')
                parole[i][j]=' ';
            //Se trovo un carattere diverso di spaziatura lo sostituisco
            else
                parole[i][j]='-';
        }
    }
    //Cerco o creo il file contenente i dati aggregati
    memset(path_aggr,0,DIM_PATH);
    sprintf(path_aggr,"%d/aggr/%s/",porta,parole[1]);
    sprintf(path_reg,"%d/",porta);
    if(strncmp("T",parole[0],1)==0)
        sprintf(file,"tot_%s_%s.txt",parole[2],parole[3]);
    else if(strncmp("V",parole[0],1)==0)
        sprintf(file,"var_%s_%s.txt",parole[2],parole[3]);
    else{
        printf("*********************************************************************************************\n");
        printf("Tipo di richiesta sconosciuta, i due tipi di aggregati possibili sono T:totale e V:Variazione\n");
        printf("*********************************************************************************************\n\n");
        return;
    }
    strcpy(path_aggr_tot,path_aggr);
    strcat(path_aggr_tot,file);
    //Adesso devo trasformare le stringhe delle due date in effettive date
    if (strptime(parole[2], "%d-%m-%Y", &tm) == NULL){
        printf("Errore: conversione data non riuscita\n");
    }
    dataInizio = mktime(&tm);
    if(dataInizio == -1){
        printf("Errore: conversione data non riuscita\n");
    }
    if (strptime(parole[3], "%d-%m-%Y", &tm) == NULL){
        printf("Errore: conversione data non riuscita\n");
    }
    dataFine = mktime(&tm);
    if(dataFine == -1){
        printf("errore nella conversione della data\n");
    }
    //Procedo ad eseguire le operazioni per l'aggregazione
    if(strncmp("T",parole[0],1)==0){
        /*Verifico se il dato aggregato è già stato calcolato*/
        memset(data,0,sizeof(data));
        esiste=verificaEsistenza(path_aggr_tot,data);
        if(esiste){
            printf("***********************************************\n");
            printf("Dato già calcolato\n TOT: %s\n", data[0]);
            printf("***********************************************\n\n");
        }
        //File non trovato, devo ancora calcolarlo
        else{
            bool first=true;
            data_tot = malloc(1* sizeof(char*));
            data_tot[0] = malloc((DIM_ELEM+1) * sizeof(char));
            printf("\n*************************************\n");
            printf("***** Dato non ancora calcolato *****\n");
            printf("*************************************\n\n");

            //Trasformo di nuovo la data di inizio per tenerla come variabile di appoggio da scorrere
            if(strptime(parole[2],"%d-%m-%Y", &tm)==NULL)
                printf("Creazione data indice non riuscita\n\n");
            index=mktime(&tm);
            if(index==-1)
                printf("mktime iniziale non riuscita\n\n");
            //Scorro le date
            while(true){
                if(first)
                    first=false;
                else{
                    tm.tm_mday = tm.tm_mday+1;
                    index=mktime(&tm);
                    if(index==-1)
                        printf("mktime ciclica non riuscita\n\n");
                }
                sprintf(index_c,"%d-%d-%d.txt", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
                //Verifico che la data sia correttamente nell'intervallo di ricerca
                if((difftime(index,dataInizio)>=0) && (difftime(index,dataFine)<=0)){
                    //Preparo il path del file da cercare
                    memset(path_app,0,sizeof(path_app));
                    strcpy(path_app,path_reg);
                    strcat(path_app,index_c);
                    //Cerco l'esistenza del file
repeat_tot:         printf("Verifico: %s\n",path_app);
                    esiste=verificaEsistenza(path_app,data);
                    if(!esiste){
                        printf("File non trovato!\n");
                        creaRegistro(porta,index_c,0,0,0,0);
                        goto repeat_tot;
                    }
                    else
                        printf("*** Dato nel registro del giorno trovato ***\n");
                    //Controllo quale riga devo estrapolare
                    if(strcmp("T",parole[1])==0)
                        nrow=2;
                    else 
                        nrow=3;
                    valoreDaAggiungere=prendi_entry(nrow, 3 ,data);
                    //Se il dato è 0 devo chiedere ai peer della rete se hanno già il dato aggregato altrimenti di calcolarlo
                    if(valoreDaAggiungere==0){
                        statoConn=0;
                        printf("**********************************************************************************************\n");
                        printf("Dato aggregato relativo a questa data non ancora calcolato, mando la richiesta agli altri peer\n");
                        printf("**********************************************************************************************\n\n");
                        memset(file,0,sizeof(file));
                        if(strncmp("T",parole[1],1)==0)
                            sprintf(file,"tot_%s_%s.txt",parole[2],parole[3]);
                        else if(strncmp("N",parole[1],1)==0)
                            sprintf(file,"var_%s_%s.txt",parole[2],parole[3]);
                        if(!aggr_chiesto){
                            printf("*** RICHIEDO IL DATO AGGREGATO TOTALE ***\n");
                            for(i=0;i<2;i++){
                                if(i==0 && primoVicino!=-1)
                                    statoConn=connettiTCP(primoVicino,'T',tipoDato,0,0,file,&val,data_var);
                                if(i!=0 && secondoVicino!=-1)
                                    statoConn=connettiTCP(secondoVicino,'T',tipoDato,0,0,file,&val,data_var);
                                if(statoConn){
                                    valoreDaAggiungere=val;
                                    break;
                                }
                            }
                            aggr_chiesto=true;
                        }
                        if(statoConn){
                            printf("*** DATO AGGREGATO TROVATO ***\n");
                            tot=valoreDaAggiungere;
                            break;
                        }
                        else{
                            //Caso in cui ho già chiesto se l'aggregato è già stato calcolato, procedo a chiedere i dati parziali o l'eventuale flooding
                            printf("*** DATO AGGREGATO NON TROVATO, PROCEDO A RICHIEDERE I DATI PARZIALI ***\n");
                            statoConn=connettiTCP(primoVicino,'P',tipoDato,0,0,index_c,&val,data_var);
                            if(!statoConn)
                                statoConn=connettiTCP(secondoVicino,'P',tipoDato,0,0,index_c,&val,data_var);
                            if(statoConn){
                                printf("*** DATO PARZIALE TROVATO ***\n");
                                valoreDaAggiungere=val;
                            }
                            if(!statoConn){
                                printf("*** DATO PARZIALE NON TROVATO ***\n");
                                if(strcmp(parole[1],"T")==0)
                                    nrow=0;
                                else
                                    nrow=1;
                                //Prendo il mio valore e richiedo il flooding
                                printf("*** ESEGUO IL FLOODING ***\n");
                                valoreDaAggiungere=prendi_entry(nrow,2,data);
                                statoConn=connettiTCP(primoVicino,'F',tipoDato,valoreDaAggiungere,porta,index_c,&val,data_var);
                                valoreDaAggiungere=val;
                                printf("*** FLOODING ESEGUITO CON SUCCESSO ***\n");
                            }
                            if(strcmp(parole[1],"T")==0)
                                sprintf(data[2],"CT:%d;\n",valoreDaAggiungere);
                            else
                                sprintf(data[3],"CN:%d;\n",valoreDaAggiungere);
                        }
                        //Salviamo il valore nel register
                        esiste=scriviSuFile(path_app,data);
                        if(!esiste)
                            printf("Salvataggio su file non riuscito\n");
                    }
                    tot=tot+valoreDaAggiungere;
                }
                else
                    break;
            }   
            sprintf(data_tot[0],"%d",tot);
            esiste=scriviAggr(path_aggr_tot,1,data_tot);
            if(!esiste)
                printf("Impossibile salvare il dato!\n");
            else
                printf("Dato salvato con successo\n");
            printf("********************************\n");
            printf("Dato aggregato risultante: %d \n",tot);
            printf("********************************\n\n");
            printf("*** OPERAZIONE TERMINATA ***\n\n");
        }
    }
    else{
        //Trasformo di nuovo la data di inizio per tenerla come variabile di appoggio da scorrere
        bool first=true;
        if(strptime(parole[2],"%d-%m-%Y", &tm)==NULL)
            printf("Creazione data indice non riuscita\n\n");
        index=mktime(&tm);
        if(index==-1)
            printf("mktime iniziale non riuscita\n\n");
        //Scorro le date
        while(true){
            if(first)
                first=false;
            else{
                tm.tm_mday = tm.tm_mday+1;
                index=mktime(&tm);
                if(index==-1)
                    printf("mktime ciclica non riuscita\n\n");
            }
            //Verifico che la data sia correttamente nell'intervallo di ricerca
            if((difftime(index,dataInizio)>=0) && (difftime(index,dataFine)<=0)){
                num_diff++;
            }
            else
                break;
        }
        //Alloco l'array dinamico in cui salvare 
        data_var=malloc(num_diff * sizeof(char*));
        for(i=0;i<num_diff;i++){
            data_var[i]=malloc((VAR_DIM+1)*sizeof(char));
        }
        //Verifico se il dato aggregato è già stato calcolato
        first=true;
        memset(path_reg,0,sizeof(path_reg));
        memset(path_app,0,sizeof(path_app));
        sprintf(path_reg,"%d/",porta);
        sprintf(path_app,path_reg);
        //Alloco anche un array contenente le variazioni nel caso in cui vadano calcolate
        variazioni=(int*)malloc(DIM_DATA*(num_diff-1)*sizeof(int));
        //Alloco un array di strutture che conterrà la variazione tra la data di partenza e la
        //data corrispondente al register,la data e il dato di variazione
        struct variazione_data *date_var=malloc(num_diff * sizeof(struct variazione_data));
        //Tento di aprire il file
        esiste=verificaEsistenzaVar(path_aggr_tot,num_diff,data_var);
        if(esiste){
            printf("***********************************************\n");
            printf("************* Dato già calcolato **************");
            printf("***********************************************\n\n");
            //Procedo a stamparli a schermo
            for(i=0;i<num_diff;i++)
                printf("Dato: %s",data_var[i]);
        }
        //Apertura non riuscita, il file non esiste e il dato non è già stato calcolato
        else{
            //Trasformo di nuovo la data di inizio per tenerla come variabile di appoggio da scorrere
            if(strptime(parole[2],"%d-%m-%Y", &tm)==NULL)
                printf("Creazione data indice non riuscita\n\n");
            index=mktime(&tm);
            if(index==-1)
                printf("mktime iniziale non riuscita\n\n");
            i=0;
            //Creo un ciclo che mi scorra tutti i file appartenenti all'intervallo
            while(true){
                if(first)
                    first=false;
                else{
                    tm.tm_mday = tm.tm_mday+1;
                    index=mktime(&tm);
                    if(index==-1)
                        printf("mktime ciclica non riuscita\n\n");
                }
                sprintf(index_c,"%d-%d-%d.txt", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
                //Verifico che la data in esame rientri nell'intervallo di analisi
                if((difftime(index,dataInizio)>=0) && (difftime(index,dataFine)<=0)){
                    //Prendo la seconda data dell'intervallo
                    sprintf(var_date,"%d-%d-%d", tm.tm_mday,(tm.tm_mon+1),(tm.tm_year)+1900);
                    //Devo riordinare le strutture in base a 'diff_inizio' in quanto non posso essere sicuro dell'ordine con cui leggo le struct
                    insert=0;
                    //Verifico che l'array non sia vuoto
                    if(conta_date!=0){
                        //Scorro l'array fino al numero di elementi
                        for(j=0;j<=i;j++){
                            //Se la differenza con la start_date è maggiore della attuale o non ho raggiunto
                            //L'ultimo elemento di incrementa la posizione di inserimento
                            if(difftime(index,dataInizio)>date_var[j].diff_inizio && j!=conta_date)
                                insert++;
                            else
                                break;
                        }
                        //Adesso metto in ordine gli elementi successivi
                        for(n=i;n>insert;n--){
                            date_var[n].diff_inizio = date_var[n-1].diff_inizio;
                            strcpy(date_var[n].date,date_var[n-1].date);
                            date_var[n].data=date_var[n-1].data;
                        }
                    }
                    //Inserimento ordinato nell'array di struct
                    date_var[insert].diff_inizio=difftime(index,dataInizio);
                    strcpy(date_var[insert].date,var_date);
                    //Per sicurezza ogni volta azzero la variabile di appoggio in cui scrivo il path
                    memset(path_app,0,sizeof(path_app));
                    strcpy(path_app,path_reg); /* porta/ */
                    strcat(path_app,index_c);  /* porta/data.txt */
repeat_var:
                    printf("Apro il file al percorso: %s\n",path_app);
                    esiste=verificaEsistenza(path_app,data);
                    if(!esiste){
                        printf("File non trovato!\n");
                        creaRegistro(porta,index_c,0,0,0,0);
                        goto repeat_var;
                    }
                    //Controllo quale riga devo estrapolare (T= tamponi fatti N=num casi)
                    if(strcmp("T",parole[1])==0)
                        nrow=2;
                    else 
                        nrow=3;
                    //Prendo il valore, se il file è stato appena creato il valore sarà inizializzato a 0
                    valoreDaAggiungere=prendi_entry(nrow, 3 ,data);
                    if(valoreDaAggiungere==0){
                        printf("**********************************************************************************************\n");
                        printf("Dato aggregato relativo a questa data non ancora calcolato, mando la richiesta agli altri peer\n");
                        printf("**********************************************************************************************\n\n");
                        statoConn=0;
                        //Chiedo ai vicini se pssiedono il dato aggregato già calcolato
                        if(!aggr_chiesto){
                            statoConn=false;
                            for(j=0;j<2;j++){
                                if(j==0 && primoVicino!=-1)
                                    statoConn=connettiTCP(primoVicino,'V',tipoDato,num_diff,0,file,&val,data_var);
                                else if(j!=0 && secondoVicino!=-1)
                                    statoConn=connettiTCP(secondoVicino,'V',tipoDato,num_diff,0,file,&val,data_var);
                                if(statoConn)
                                    goto salva;
                            }
                            aggr_chiesto=true;
                        }
                        //Se arrivo qua significa che i miei vicini non hanno il dato aggregato già calcolato, procedo a richiedere tutti i dati parziali di oggi
                        for(j=0;j<2;j++){
                                if(j==0 && primoVicino!=-1)
                                    statoConn=connettiTCP(primoVicino,'P',tipoDato,0,0,index_c,&val,data_var);
                                else if(j!=0 && secondoVicino!=-1)
                                    statoConn=connettiTCP(secondoVicino,'P',tipoDato,0,0,index_c,&val,data_var);
                                if(statoConn){
                                    valoreDaAggiungere=val;
                                    break;
                                }
                        }
                        if(!statoConn){
                            if(strcmp(parole[1],"T")==0)
                                    nrow=0;
                                else
                                    nrow=1;
                                valoreDaAggiungere=prendi_entry(nrow, 2 ,data);
                                statoConn=connettiTCP(primoVicino,'F',tipoDato,valoreDaAggiungere,porta,index_c,&val,data_var);
                                valoreDaAggiungere=val;
                        }
                       if(strcmp(parole[1],"T")==0)
                            sprintf(data[2],"CT:%d;\n",valoreDaAggiungere);
                        else
                            sprintf(data[3],"CN:%d;\n",valoreDaAggiungere);
                        esiste = scriviSuFile(path_app, data);
                        if(!esiste)
                            printf("Salvataggio su file non riuscito!\n\n");

                    }
                    date_var[insert].data=valoreDaAggiungere;
                    conta_date++;
                    i++;
                }
                else
                    break;
            }
            //Stampo i dati trovati nel vettore data
            for(j=0;j<num_diff-1;j++){
                variazioni[j] =date_var[j+1].data - date_var[j].data;
                sprintf(data_var[j],"%s_%s: %d;\n",date_var[j].date,date_var[j+1].date, variazioni[j]);
                printf("********************************\n");
                printf("Dato aggregato risultante per la coppia: %d è: %d \n",(j+1),variazioni[j]);
                printf("********************************\n");
            }
salva:
            esiste=scriviAggr(path_aggr_tot,num_diff-1,data_var);
            if(!esiste)
                printf("Salvataggio dato aggregato non riuscito\n");
            else
                printf("Salvataggio dato aggregato eseguito con successo\n");
            for(j=0;j<num_diff-1;j++){
                printf("%s",data_var[j]);
            }
            printf("\n *** OPERAZIONE TERMINATA ***\n");
        }

    }
}


/*Funzione dedicata a gestire i messaggi ricevuti dal DS, un peer riceverà un messaggio dal DS quando un altro peer connessosi alla rete andrà
  a modificare i neighbor di questo, il DS dovrà occuparsi di notificare ai peer la modifica nell'anello e dirgli quali sono i nuovi vicini
  @sd è il socket su cui rimango in ascolto di messaggi del DS
  */
void gestisciMsgDS(int sd){
    int ret,numPar;
    unsigned int len;
    char* app;
    app=(char*)malloc(sizeof(char)*20);
    len=sizeof(ds);
    numPar=0;
    //Ricevo il messaggio contenente i nuovi peer
    while(1){
        ret=recvfrom(sd, app, 35, 0, (struct sockaddr*) &ds,&len);
        if(ret>=0)
            break;
    }
    numPar=parsingMsg(app);
    if(numPar==0 && strcmp(parole[0],"CLS")==0){
        cls(sd);
        printf("Richiesa di chiusura ricevuta dal DS, termino...\n");
        getchar();
        exit(EXIT_SUCCESS);
    }
    //Se ero il primo peer ad entrare in rete mi arriverà un messaggio contenente un solo nuovo peer (Il secondo entrato)
    if(numPar==1 && strcmp(parole[1],"NULL")==0){
        primoVicino=-1;
        secondoVicino=-1;
    }
    //Nel caso in cui il DS si stia chiudendo mi arrivera un messaggio di Close "CLS"
    else if(numPar==1){
        primoVicino=atoi(parole[1]);
        secondoVicino=-1;
    } 
    //Se la rete è già con almeno due peer riceverò il numero di porta dei due nuovi vicini
    else if(numPar==2){
        primoVicino=atoi(parole[1]);
        secondoVicino=atoi(parole[2]);
    }
    printf("\nAggiornamento dei neighbor ricevuto.\n");
    printf("I miei nuovi neighbor sono: %d    - %d\n",primoVicino,secondoVicino);
    return;
}

/*Funzione che si occupa di svolgere le funzioni associate al comando 'STOP', notifica la volontà di terminare al DS e invia
    i dati raccolti fino ad adesso agli altri peer dopo di che termina 
    @sd è la socket tramite cui devo comunicare al DS la mia volontà di terminare
    */
void stop(int sd){
    int ret;
    unsigned int len;
    char* buffer,*rec;
    buffer=(char*)malloc(sizeof(char)*4);
    rec=(char*)malloc(sizeof(char)*3);
    memset(buffer,0,4);
    memset(rec,0,3);
    buffer="CLS\0";
    len=sizeof(ds);
    /* Operazione di invio dati raccolti ai peer vicini ... */
    inviaRecord();
    /* Operazione di notifica al DS della terminazione */
    if(connected){
        while(1){
            sendto(sd,buffer,strlen(buffer),0,(struct sockaddr*)&ds, len);
            //Mi attendo un ACK come conferma della ricezione del messaggio di terminazione
            sleep(1);
            ret=recvfrom(sd, rec, 3, 0, (struct sockaddr*)&ds,&len);
            if(ret<0){
                printf("Ritento\n");
                continue;
            }
            break;
        }
        printf("Ricevuto: %s\n",rec);
    }
    printf("Chiudo il registro\n");
    ret=fclose(ptr);
    if(ret==0)
        printf("Registro chiuso con successo\n");
    else
        printf("Impossibile chiudere il registro, le modifiche andranno perdute\n");
    return;
}
/* Funzione che si occupa di gestire la richiesta dal parte del DS di chiusura*/
void cls(int sd){
    int len,ret;
    char* buffer;
    /* Se non ho mai eseguito la start posso terminare */
    if(connected==0){
        printf("Chiudo il registro\n");
        ret=fclose(ptr);
        if(ret==0)
            printf("Registro chiuso con successo\n");
        else
            printf("Impossibile chiudere il registro, le modifiche andranno perdute\n");
        return;
    }
    buffer=(char*)malloc(sizeof(char)*3);
    memset(buffer,0,4);
    buffer="OK\0";
    len=sizeof(ds);
    //Notifico la ricezione al DS e termino la connessione
    while(1){
        ret=sendto(sd,buffer,strlen(buffer),0,(struct sockaddr*)&ds, len);
        if(ret>0)
            break;
        else
            printf("Invio ACK non riuscito, ritento\n");
    }
    printf("Chiudo il registro\n");
    ret=fclose(ptr);
    if(ret==0)
        printf("Registro chiuso con successo\n");
    else
        printf("Impossibile chiudere il registro, le modifiche andranno perdute\n");
    return;
}


/*Funzione che si occupa di gestire le richieste di connessione TCP che arrivano sul socket di ascolto TCP
@listener è il parametro contenente il numero naturale che rappresenta la socket di ascolto*/
int gestisciMsgPeer(int listener){
    int ret;
    unsigned int len;
    len=sizeof(ngbr);
    ret=accept(listener, (struct sockaddr *)&ngbr,&len);
    printf("*******************************************\n");
    printf("Richiesta di connessione tra peer ricevuta\n");
    printf("*******************************************\n\n");
    return ret;
}
/*Funzione che si occupa di gestire lo scambio dei dati tra i peer
@sdN è il numero naturale che identifica la socket su cui avviene la connessione TCP*/
void scambioDati(int sdN){
    int msg,ret,risposta,date=0,j,riga,peersEntry=-1;
    unsigned int dim;
    uint16_t porta_richiedente;
    char *tipo_richiesta,*info;
    char data[DIM_PATH_AGG],**date_var;
    char variabile[(VAR_DIM+1)*sizeof(char)];
    char files_path[DIM_PATH_TOT];
    char sezioni[N_ELEMENTI][DIM_ELEM];   //Le varie parti ricevute via TCP
    bool esiste=false;
    memset(sezioni,0,sizeof(sezioni));
    printf("Scambio dati tra peer in atto!\n\n");
    msg=-1;
    //Inizio a ricevere i messaggi a comune tra le tre categorie di richieste
    tipo_richiesta=(char*)malloc(sizeof(char));
    info=(char*)malloc(sizeof(char));
    dim=sizeof(info);
    memset(info,0,dim);
    dim=sizeof(tipo_richiesta);
    memset(tipo_richiesta,0,dim);
    //Ricezione tipo di richiesta
    ret=recv(sdN,tipo_richiesta,sizeof(uint8_t),0);
    if(ret<=0)
        perror("Errore: ");
    printf("Primo pacchetto ricevuto: -%c-\n",*tipo_richiesta);
    //Ricezione tipo di dato da aggregare (Tamponi o Casi)
    ret=recv(sdN,info,1,0);
    if(ret<=0)
        perror("Errore: ");
    printf("Secondo pacchetto ricevuto: -%c-\n",*info);
    //Ricezione payload
    ret=recv(sdN,data,29,0);
    if(ret<=0)
        perror("Errore: ");
    printf("Terzo pacchetto ricevuto: - %s -\n",data);
    if(*tipo_richiesta=='T'){ //Codifica del carattere 'T'
        printf("**** RICHIESTA AGGREGAZIONE TOTALE ****\n\n");
        memset(files_path,0,sizeof(files_path));
        if(*info=='T')
            sprintf(files_path, "%d/aggr/T/",porta);
        else
            sprintf(files_path, "%d/aggr/N/", porta);
        //Concateno il campo data al path del file, conterrà una stringa che rappresenta il file aggregato che il peer ci ha richiesto
        strcat(files_path,data);
        printf("Cerco il file: %s\n",files_path);
        esiste=verificaEsistenza(files_path,sezioni);
        if(esiste){
            risposta=atoi(sezioni[0]);
            printf("Dato aggregato trovato!\n\n");
        }
        else{
            risposta=-1;
            printf("Dato aggregato non trovato!\n\n");
        }
        risposta=htonl(risposta);
        ret=send(sdN,&risposta,sizeof(risposta),0);
        if(ret<0)
            perror("Impossibile inviare la risposta: ");
        printf("**** OPERAZIONE EFFETTUATA CON SUCCESSO ****\n\n");
    }

    if(*tipo_richiesta=='V'){
        printf("**** RICHIESTA AGGREGAZIONE VARIAZIONE ****\n\n");
        //Ricevo pacchetto specifico
        ret=recv(sdN,&date,sizeof(uint32_t),0);
        if(ret<=0)
            perror("Errore: ");
        date=ntohl(date);
        printf("Quarto pacchetto ricevuto: %d\n",date);
        date_var=malloc(date *sizeof(char*));
        //Alloco vettore in cui salvare i dati
        for(j=0;j<date;j++)
            date_var[j]=malloc((VAR_DIM+1)*sizeof(char));
        memset(files_path,0,sizeof(files_path));
        //Verifico se devo cercare tamponi o casi e creo il path del file da cercare
        if(*info=='T')
            sprintf(files_path,"%d/aggr/T/",porta);
        else
            sprintf(files_path,"%d/aggr/N/",porta);
        strcat(files_path,data);
        printf("Cerco il file: %s\n",files_path);
        esiste=verificaEsistenzaVar(files_path,date,date_var);
        //Se lo trovo prelevo i dati
        if(esiste){
            for(j=0;j<date;j++){
                sprintf(variabile, "%s", date_var[j]);
                ret=send(sdN,variabile, VAR_DIM, 0);
                if(ret<0)
                    perror("Errore: ");
            }
            printf("Dato variazione trovato ed inviato\n");
            goto free;
        }
        //Se non lo trovo restituisco un messaggio di errore
        strcpy(variabile,"NONE");
        printf("Dato variazione non trovato\n");
        ret=send(sdN,variabile,VAR_DIM,0);
        if(ret<0)
            perror("Errore: ");
        //Sezione dedicata a liberare le variabili allocate dinamicamente
free:
        for(j=0;j<date;j++)
            free(date_var[j]);
        free(date_var); 
        printf("**** OPERAZIONE EFFETTUATA CON SUCCESSO ****\n\n");
    }
    if(*tipo_richiesta=='P'){
        int n=0,tot=-1;
        printf("**** RICHIESTA DATI COMPLESSIVI ****\n\n");
        //Richiesta dati complessivi
        risposta=-1;
        memset(files_path,0,sizeof(files_path));
        //Preparo il percorso
        sprintf(files_path,"%d/",porta);
        strcat(files_path,data);
        esiste=verificaEsistenza(files_path,sezioni);
        if(esiste){
            if(*info=='T')
                riga=2;
            else
                riga=3;
            n=prendi_entry(riga,3,sezioni);
            if(n!=0)
                tot=n;
            else
                printf("Dato non trovato,lo notifico\n\n");
        }
        //Se lo trovo restituisco il valore altrimenti restituisco -1
        msg=htonl(tot);
        ret=send(sdN,&msg,sizeof(msg),0);
        if(ret<0)
            perror("Errore: ");
        printf("**** OPERAZIONE EFFETTUATA CON SUCCESSO ****\n\n");
    }

    if(*tipo_richiesta=='S'){
        int n_char,n;
        printf("**** RICHIESTA SALVATAGGIO DATI ****\n\n");
        //Pacchetti specifici
        ret=recv(sdN,&date,sizeof(uint32_t),0);
        if(ret<=0)
            perror("Errore: ");
        date=ntohl(date);
        printf("Quarto pacchetto ricevuto: %d\n",date);
        ret=recv(sdN,&porta_richiedente,sizeof(uint16_t),0);
        if(ret<0)
            perror("Errore: ");
        porta_richiedente=ntohs(porta_richiedente);
        printf("Quinto pacchetto ricevuto: %d\n",porta_richiedente);
        //Procedo ad eseguire il salvataggio, preparo la stringa del file
        memset(files_path,0,sizeof(files_path));
        sprintf(files_path,"%d/",porta);
        strcat(files_path,data);
        printf("%s\n",files_path);
        //Prendo i dati sul register di oggi di questo peer
        esiste=verificaEsistenza(files_path,sezioni);
        //Cerco di capire quale riga devo scrivere e quale carattere in quella riga
        if(esiste){
            if(*info=='T')
                if(porta_richiedente==1)
                    riga=0;
                else
                    riga=2;
            else
                if(porta_richiedente==1)
                    riga=1;
                else
                    riga=3;
            //Carattere da scrivere
            if(porta_richiedente==1)
                n_char=2;
            else
                n_char=3;
            n=prendi_entry(riga,n_char,sezioni);
            //Aggiorno i dati che avevo sul register con l'unione di quelli e quelli inviati dal peer che sta uscendo dalla rete
            //N.B. arriva un dato alla volta quindi questa sezione di codice alla chiusura sarà eseguita per ogni dato da salvare, i dati non inviati verranno salvati con lo 
            //stesso valore che avevano prima
            if(porta_richiedente==1 && *info=='T')
                sprintf(sezioni[0],"T:%d;\n",n+date);
            else if(porta_richiedente==1 && *info!='T')
                sprintf(sezioni[1],"N:%d;\n",n+date);
            else if(porta_richiedente!=1 && *info=='T')
                sprintf(sezioni[2],"CT:%d;\n",n+date);
            else
                sprintf(sezioni[3],"CN:%d;\n",n+date);
            //Salvo il risultato sul file
            risposta=1;
            scriviSuFile(files_path,sezioni);
            ret=send(sdN,&risposta,sizeof(risposta),0);
            if(ret<0)
                perror("Errore: ");
            return;
        }
        //Se il register non esiste va creato opportunamente con le entry arrivate
        if(*info=='T' && porta_richiedente==1)
            creaRegistro(porta, data, date, 0,0,0);
        else if(*info!='T' && porta_richiedente==1)
            creaRegistro(porta, data, 0, date, 0, 0);
        else if(*info=='T' && porta_richiedente!=1)
            creaRegistro(porta, data, 0, 0, date, 0);
        else
            creaRegistro(porta, data, 0, 0, 0, date);
        printf("**** OPERAZIONE EFFETTUATA CON SUCCESSO ****\n\n");
    }
    //Sezione che gestisce le richieste di flooding, valutano se hanno già calcolato il dato parziale aggregato e, nel caso lo inviano, altrimenti sommano al valore arrivato
    //Con la richiesta di flooding quello che individuano nel loro register di quel giorno
    if(*tipo_richiesta=='F'){
        int n,tot=-1;
        printf("**** RICHIESTA FLOODING ****\n\n");
        //Ricezione pacchetti specifici
        ret=recv(sdN,&date,sizeof(uint32_t),0);
        if(ret<=0)
            perror("Errore: ");
        date=ntohl(date);
        printf("Quarto pacchetto ricevuto: %d\n",date);
        ret=recv(sdN,&porta_richiedente,sizeof(uint16_t),0);
        if(ret<0)
            perror("Errore: ");
        porta_richiedente=ntohs(porta_richiedente);
        printf("Quinto pacchetto ricevuto: %d\n",porta_richiedente);
        memset(files_path,0,sizeof(files_path));
        sprintf(files_path,"%d/",porta);
        strcat(files_path,data);
        //Verifico l'esistenza del file
        esiste=verificaEsistenza(files_path,sezioni);
        if(esiste){
            if(*info=='T')
                riga=2;
            else
                riga=3;
            //Cerco il dato totale
            n=prendi_entry(riga,3,sezioni);
            if(n!=0){
                printf("Ho trovato il valore aggregato per il giorno di oggi: %d\n",n);
                tot=n;
            }
            //Se non lo trovo prendo il parziale e proseguo
            else{
                printf("Ho cercato il valore aggregato ma ho trovato: %d\n",n);
                if(*info=='T')
                    riga=0;
                else
                    riga=1;
                n=prendi_entry(riga,2,sezioni);
                printf("Ho cercato il valore di questo peer sul file %s e ho trovato: %d\n",files_path,n);
                date=date+n;
                printf("Dato complessivo che trasmetto: %d\n",date);
            }
        }
        risposta=date;
        if(tot!=-1){
            printf("Trovato dato complessivo, fermo il flooding\n");
            risposta=tot;
        }
        else if(tot==-1 && primoVicino!=porta_richiedente){
            printf("Flooding non completo, proseguo il ciclo\n");
            connettiTCP(primoVicino,*tipo_richiesta,*info,date,porta_richiedente,data,&peersEntry,date_var);
            risposta=peersEntry;
        }
        else
            printf("Dato complessivo calcolato, fermo il flooding\n");
        risposta=htonl(risposta);
        ret=send(sdN,&risposta,sizeof(risposta),0);
        if(ret<0)
            perror("Errore: ");
        printf("**** OPERAZIONE EFFETTUATA CON SUCCESSO ****\n\n");
    }
    free(info);
    free(tipo_richiesta);
}
//Funzione che si occupa di creare il registro e preparare e inserire il dataset base
/*
@peer la porta del peer per cui creare la cartella
@data le date per cui caricare alcuni valori
@T N° tamponi eseguiri
@N N° casi registrati
@CT N° complessivo tamponi per la data 'data'
@CN N° complessivo casi per la data 'data'
*/
void creaRegistro(int peer, char *data, int T, int N, int CT, int CN){
    // array di appoggio per la costruzione del path completo, per accedere al file
    char strPeer[8];
    char path_completo[DIM_PATH_TOT];
    char elemento[N_ELEM][DIM_ELEM];
    bool scrittura = false;
    sprintf(strPeer,"%d",peer);
    opendir(strPeer);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(strPeer,0x0777);
        opendir(strPeer);
    }
    //Preparo la stringa da scrivere sul file
    sprintf(path_completo, "%d/%s", peer, data);
    sprintf(elemento[0], "T:%d;\n", T);
    sprintf(elemento[1], "N:%d;\n", N);
    sprintf(elemento[2], "CT:%d;\n", CT);
    sprintf(elemento[3], "CN:%d;", CN);
    //Tento la scrittura sul file, se il file non viene trovato viene creato
    scrittura = scriviSuFile(path_completo, elemento);
    if(!scrittura){      
        printf("Non è stato possibile aggiornare/creare la entry\n");
    }
    //Creo le altre cartelle in cui inserire gli aggregati
    sprintf(path_completo,"%d/aggr",peer);
    opendir(path_completo);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(path_completo,0x0777);
        opendir(path_completo);
    }
    //Creo la cartella degli aggregati relativa ai tamponi
    sprintf(path_completo,"%d/aggr/T",peer);
    opendir(path_completo);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(path_completo,0x0777);
        opendir(path_completo);
    }
    //Creo la cartella degli aggregati relativa ai casi
    sprintf(path_completo,"%d/aggr/N",peer);
    opendir(path_completo);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(path_completo,0x0777);
        opendir(path_completo);
    }
}
//Funzione che si occupa di creare il registro per la data di oggi
/*
@porta è l'intero che rappresenta la porta del peer in esecuzione
@strData è la data di oggi scritta nel formato "dd-mm-yyyy" necessaria per nominare il file
*/
FILE* creaRegistroOdierno(int porta,char *strData){
    // array di appoggio per la costruzione del path completo, per accedere al file
    FILE *file;
    char path_completo[DIM_PATH_TOT];
    char path_aggr[DIM_PATH_TOT];
    char elemento[N_ELEM][DIM_ELEM];
    bool scrittura = false;
    //Creo la cartella degli aggregati relativa ai casi
    sprintf(path_aggr,"%d/aggr/N",porta);
    opendir(path_aggr);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(path_aggr,0x0777);
        opendir(path_aggr);
    }
    //Creo la cartella degli aggregati relativa ai tamponi
    sprintf(path_aggr,"%d/aggr/T",porta);
    opendir(path_aggr);
    //La directory non esiste
    if(ENOENT == errno){
        printf("Cartella non trovata,la creo\n");
        mkdir(path_aggr,0x0777);
        opendir(path_aggr);
    }
    strcat(strData,".txt");
    sprintf(path_completo, "%d/%s", porta, strData);
    //Tento di aprire il file nel caso lo creo
    file=fopen(path_completo, "w+");
    //Preparo la stringa da scrivere sul file
    sprintf(elemento[0], "T:0;\n");
    sprintf(elemento[1], "N:0;\n");
    sprintf(elemento[2], "CT:0;\n");
    sprintf(elemento[3], "CN:0;\n");
    //Tento la scrittura sul file, se il file non viene trovato viene creato
    scrittura = scriviSuFile(path_completo, elemento);
    if(!scrittura){      
        printf("Non è stato possibile aggiornare/creare la entry\n");
    }
    return file;
}

/* Funzione che si occupa di creare e gestire la connessione TCP tra i peer
   @Vicino intero che rappresenta la porta da contattare
   @tipoRichiesta rappresenta il tipo di richiesta che intendiamo fare
   @temp intero da inviare
   @load stringa da inviare
   @valoreEntry è il dato per cui facciamo richiesta ai peer
   @data_var array dinamico che contiene le variazioni del dato aggregatodata_var
*/
bool connettiTCP(int Vicino,char tipoRichiesta,char tipoDato,int temp,int _porta,char* load,int *valoreEntry,char **data_var){
    int sdTCP,i,ret,n;
    struct sockaddr_in ngbrSocket;
    int len=sizeof(struct messaggio);
    struct messaggio *msg=(struct messaggio*)malloc(len+1);
    int risultato=0;
    bool return_value=false;
    char var[VAR_DIM];
    //Creo una connessione TCP col vicino
    sdTCP=socket(AF_INET,SOCK_STREAM,0);
    memset(&ngbrSocket,0,sizeof(ngbrSocket));
    ngbrSocket.sin_family=AF_INET;
    ngbrSocket.sin_port=htons(Vicino);
    ngbrSocket.sin_addr.s_addr=INADDR_ANY;
    printf("********************\n");
    printf("FASE DI SCAMBIO DATI\n");
    printf("********************\n\n");
    //Instauro la connessione TCP
    ret=connect(sdTCP,(struct sockaddr *)&ngbrSocket,sizeof(ngbrSocket));
    if(ret<0){
        perror("Errore, connessione non riuscita: \n");
        return false;
    }
    printf("Connessione avvenuta con successo!\n");
    //Richiesta flooding o salvataggio
    if(tipoRichiesta=='F' || tipoRichiesta=='S'){
        msg=crea_msg((uint8_t)tipoDato,(uint16_t)_porta,(uint8_t)tipoRichiesta, NULL,(uint32_t)temp,(uint8_t*)load);
    }
    //Richiesta dato variazione
    else if(tipoRichiesta=='V'){
        msg=crea_msg((uint8_t)tipoDato,0,(uint8_t)tipoRichiesta, NULL,(uint32_t)temp,(uint8_t*)load);
    }
    //Richiesta salvataggio dati per uscita dalla rete
    else{
        msg=crea_msg((uint8_t)tipoDato,0,(uint8_t)tipoRichiesta, NULL,0,(uint8_t*)load);
    }
    //Invio le varie parti di messaggio (parti a comune tra tutti i pacchetti)
    ret=send(sdTCP,&(msg->tipo),sizeof(uint8_t), 0);
    if(ret<0){
        printf("Errore durante l'invio del pacchetto n°1\n");
        exit(0);   
    }
    ret=send(sdTCP,&(msg->info),sizeof(uint8_t), 0);
    if(ret<0){
        printf("Errore durante l'invio del pacchetto n°2\n");
        exit(0);  
    }
    n=strlen(load);
    ret=send(sdTCP,(msg->load),29, 0);
    if(ret<n){
        printf("Errore durante l'invio del pacchetto n°3: Dati inviati-> %d\n",n);
        exit(0);
    }
    //Richieste specifiche Per V, S e F
    if(tipoRichiesta=='V' || tipoRichiesta=='F' || tipoRichiesta=='S'){
        ret=send(sdTCP,&(msg->date),sizeof(uint32_t), 0);
        if(ret<0){
            printf("Errore durante l'invio del pacchetto n°4\n");
            exit(0);
        }
        //Dati da inviare solo per le richieste di tipo F o S
        if(tipoRichiesta=='F' || tipoRichiesta=='S'){
            ret=send(sdTCP,&(msg->porta),sizeof(uint16_t), 0);
            if(ret<0){
                printf("Errore durante l'invio del pacchetto n°5\n");
                exit(0);
            }
        }
    }
    printf("Pacchetti inviati con successo attendo l'elaborazione\n\n");

    if(tipoRichiesta=='V'){
        ret=recv(sdTCP,var,VAR_DIM,0);
        if(ret<0){
            printf("Errore durante la ricezione!\n");
            exit(0);
        }
        else if(strcmp(var,"NONE")==0){
            return_value=false;
            printf("Dato non presente!\n\n");
        }
        else{
            //salvo il dato trovato
            strcpy(data_var[0],var);
            //ricevo i restanti dati
            for(i=0;i<temp;i++){
                memset(var,0,sizeof(var));
                ret=recv(sdTCP,var,VAR_DIM,0);
                if(ret<0){
                    printf("Errore durante la ricezione!\n");
                    exit(0);
                }
                strcpy(data_var[i],var);
            }
            printf("Dato ricevuto con successo\n\n");
            return_value=true;
        }
    }
    else{
        ret=recv(sdTCP,&risultato,RESPONSE_LEN,0);
        if(ret<0){
            printf("Errore durante la ricezione!\n");
            exit(0);
        }
        *valoreEntry=ntohl(risultato);
        if(*valoreEntry==-1){
            return_value=false;
            printf("Dato non presente!\n\n");
        }
        else{
            if(tipoRichiesta=='S')
                printf("Entry del register di oggi condivise con successo\n\n");
            else
                printf("Ricevuto dato: %d \n",*valoreEntry);
            return_value=true;
        }
    }
    close(sdTCP);
    printf("Scambio dati terminato\n\n");
    return return_value;
}

/* Funzione che si occupa di prendere i record delregistro odierno e di condividerli ai neighbor della rete in modo da non creare dati aggregati i giorni successivi senza i suoi dati */
bool inviaRecord(){
    struct tm *info;
    time_t oggi;
    int data_scomposta[4],val;
    char path_saved[DIM_PATH],path_reg[DIM_PATH],data[15],path_tot[DIM_PATH];
    char valore[N_ELEMENTI][DIM_ELEM];
    int app[N_ELEMENTI];
    char ** elementi;
    bool esiste;
    int i,_porta;
    char tipoDato;
    time(&oggi);
    info=localtime(&oggi);
    data_scomposta[0]=info->tm_hour;
    data_scomposta[1]=info->tm_mday;
    data_scomposta[2]=info->tm_mon;
    data_scomposta[3]=info->tm_year;
    sprintf(data,"%d-%d-%d",data_scomposta[1],data_scomposta[2]+1,data_scomposta[3]+1900);
    //Verifico esista qualcuno con cui condividere i dati
    if(!connected){
        printf("Nessuno a cui inviare i propri dati, non sono connesso alla rete!\n");
        return false;
    }
    if(primoVicino==-1){
        printf("Nessuno a cui inviare i propri dati, sono l'unico peer connesso alla rete!\n");
        return false;
    }
    //Ci sono altri peer nella rete
    memset(path_saved,0,sizeof(path_saved));
    sprintf(path_saved,"%d/saved/",porta);
    memset(path_reg,0,sizeof(path_reg));
    sprintf(path_reg,"%d/",porta);
    //Variabile in cui appoggiare i valori letti
    elementi=malloc(1 * sizeof(char*));
    elementi[0]=malloc((RESPONSE_LEN+1)*sizeof(char));
    //Creo la stringa della data di oggi
    strcat(path_saved,data);
    strcat(path_reg,data);
    strcat(path_saved,".txt");
    strcat(path_reg,".txt");
    //Adesso prelevo i dati contenuti nel registro inseriti durante il corso di questa sessione
    esiste=verificaEsistenza(path_reg,valore);
    if(esiste){
        printf("file register data odierna esistente\n");
        for(i=0;i<N_ELEMENTI;i++){
            if(i<2)
                app[i]=prendi_entry(i,2,valore);
            else
                app[i]=prendi_entry(i,3,valore);
            printf("Prelevato dato sommato a quello precedente: %d\n",app[i]);
        }
    }
    //Invio i 4 valori prelevati al mio primo vicino per evitare che in caso di mio mancato rientro nella rete questi non vengano conteggiati nei dati aggregati
    sprintf(path_reg,"%s.txt",data);
    for(i=0;i<4;i++){
        if(i==0 || i==2)
            tipoDato='T';
        else
            tipoDato='N';
        if(i<2)
            _porta=1;
        else
            _porta=0;
        connettiTCP(primoVicino,'S',tipoDato,app[i],_porta,path_reg,&val,elementi);
    }
    //Azzero i valori sul register
    sprintf(path_tot,"%d/",porta);
    strcat(path_tot,path_reg);
    memset(valore,0,sizeof(valore));
    strcpy(valore[0],"T:0;\n");
    strcpy(valore[1],"N:0;\n");
    strcpy(valore[2],"CT:0;\n");
    strcpy(valore[3],"CN:0;\n");
    esiste=scriviSuFile(path_tot,valore);
    if(!esiste)
        printf("Azzeramento record sul file: %s non riuscito\n",path_reg);
    return true;
}