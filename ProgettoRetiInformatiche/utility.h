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
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

//Sezione costanti 
#define DIM_PATH 58
#define DIM_PATH_AGGR_T 50
#define DIM_PATH_AGGR_V 28
#define N_ELEMENTI 4
#define DIM_INFO 10
#define DIM_AGGR_N 40
#define DIM_PATH_AGG 31
#define DIM_PATH_TOT 58
#define N_ELEM 4
#define DIM_ELEM 15
#define DIM_AGGR_FILENAME 90
#define DIM_DATA 7
#define VAR_DIM 30
#define RESPONSE_LEN 13
#define FILE_NAME_LEN 16

/*Strutture necessarie al funzionamento*/
struct messaggio{
    uint8_t info;
    uint16_t porta;
    uint8_t tipo;
    uint8_t *indirizzo;
    uint8_t *load;
    uint32_t date;
};
/*Funzione che la inizializza, i parametri passatigli rappresentano gli omonimi campi della struttura messaggio*/
struct messaggio *crea_msg(uint8_t info, uint16_t _porta, uint8_t tipo, uint8_t *indirizzo, uint32_t date, uint8_t *load){
    struct messaggio* msg=(struct messaggio *)malloc(sizeof(struct messaggio));
    msg->info=info;
    msg->porta=htons(_porta);
    msg->tipo=tipo;
    msg->indirizzo=indirizzo;
    //Data per cui vogliamo i dati o per cui mandiamo i dati
    msg->date=htonl(date);
    //E' il payload
    msg->load=load;
    return msg;
}

struct variazione_data{
    double diff_inizio;
    char date[FILE_NAME_LEN];
    int data;
};

/*Funzione che si occupa di prendere da un file una linea specificata a partire dal carattere specificato*/
int prendi_entry(int linea, int n_carattere, char data[][DIM_ELEM]){
    int valore,i=0;
    char str[DIM_DATA];  //Sette cifre per esprimere il dato
    memset(str,0,DIM_DATA);
    while(data[linea][n_carattere]!=';'){
        str[i]=data[linea][n_carattere];
        n_carattere++;
        i++;
    }
    valore=atoi(str);
    return valore;
}

extern char parole[][40];

int parsing(char *stringa){
    char* word=NULL;
    char space=' ';
    int numParole,i;
    i=0;
    numParole=0;
    //Verifico che mi sia stata passata una stringa
    if(stringa==NULL){
        printf("Stringa passata in maniera errata al parser\n");
        return -1;
    }
    //Conto il numero di parole presenti nella stringa passata
    for(i=0;i<strlen(stringa)-1;i++){
        if(stringa[i]==space && i!=strlen(stringa)-1 && stringa[i+1]!=' ')
            numParole++;
    }
    i=0;
    word=strtok(stringa,&space);
    //Adesso devo procedere a tagliare ogni singola parola
    while(i!=numParole){
        word=strtok(NULL,&space);
        strcpy(parole[i],word);
        i++;
    }
    return numParole;
}

int parsingMsg(char *stringa){
    char* word=NULL;
    char space=' ';
    int numParole,i,ret;
    i=0;
    numParole=0,ret=0;
    //Verifico che mi sia stata passata una stringa
    if(stringa==NULL){
        printf("Stringa passata in maniera errata al parser\n");
        return -1;
    }
    //Conto il numero di parole presenti nella stringa passata
    for(i=0;i<strlen(stringa)-1;i++){
        if(i==0 && strlen(stringa)>1){
            numParole++;
        }
        if(stringa[i]==space && i!=strlen(stringa)-1 && stringa[i+1]!=' ')
            numParole=numParole+1;
    }
    ret=numParole;
    i=0;
    //Adesso devo procedere a tagliare ogni singola parola
    while(i!=numParole){
        if(i==0){
            word=strtok(stringa,&space);
            strcpy(parole[i],word);
            i++;
            continue;
        }
        word=strtok(NULL,&space);
        strcpy(parole[i],word);
        i++;
    }
    return ret-1;
}

//Funzione di libreria "time.h" necessaria a convertire una stringa in un tm
char *strptime(const char *buf, const char *format, struct tm *tm);