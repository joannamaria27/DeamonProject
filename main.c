#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <sys/syslog.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <time.h>
#include <utime.h>

bool is_dir(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISDIR(buf.st_mode);
}
bool is_file(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISREG(buf.st_mode);
}


typedef struct Pliki
{
    char nazwaPliku[60];
    //char *typPliku;
    char dataPliku[60];
   // float rozmiarPliku;
    struct Pliki * nastepny;
}Pliki;


void dodawanie(Pliki ** glowa, char nazwa[], char data[])//, char typ[]), float rozmiar)
{
    Pliki *p,*e;
    e=(Pliki*)calloc(1,sizeof(Pliki));
    e->nastepny=NULL;
    strcpy(e->nazwaPliku,nazwa);
    strcpy(e->dataPliku,data);
    //e->typPliku=typ;
    //e->rozmiarPliku=rozmiar;
    p=*glowa;
    if(p!=NULL)
    {
        while(p->nastepny!=NULL)
            p=p->nastepny;
        p->nastepny=e;        
    }
    else
        *glowa=e;
}

void wypisz_liste(Pliki* lista)
{

     Pliki* wsk = lista;

     if(lista == NULL)
     printf("LISTA JEST PUSTA");
     else
     {
     int i = 1;
     while( wsk != NULL)
     {
            printf("%d %s %s \n", i, wsk->nazwaPliku, wsk->dataPliku);
            wsk=wsk->nastepny;
            i++;
     }
     }
}

void kopiowaie(char * plikZrodlowy, char * plikDocelowy)
{
    int plikZ = open(plikZrodlowy,O_RDONLY); 
    int plikD = open(plikDocelowy, O_CREAT |O_TRUNC| O_RDWR  ,777);
	if(plikZ<0)
	{
	    printf ("błąd: "); 
        exit(0);
    }
    int ileOdcz=0;
    int ileZapis=0;
    char buffor[200];
    do
    {   
        memset(buffor,0,sizeof(buffor));
        ileOdcz=read(plikZ,buffor,sizeof(buffor));    
    }
    while(ileOdcz>=197);
    close(plikZ);
    ileZapis=write(plikD,buffor,ileOdcz);
    close(plikD);
    syslog(LOG_INFO,"plik %s został skopiowany poprzez mapowanie",plikZrodlowy);
}
void kopiowanie_mmap(char *sciezkaZ, char *sciezkaD){
    
    int plikZ, plikD;
    char *zr, *doc;
    //struct stat s;
    size_t rozmiarPliku;

    /* mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
    start - określa adres, w którym chcemy widzieć odwzorowanie pliku. Nie jest wymagane i nie zawsze jest przestrzegane
    przez system operacyjny.
    length - liczba bajtów jaką chcemy odwzorować w pamięci.
    prot - flagi określające uprawnienia jakie chcemy nadać obszarowi pamięci, np. tylko do odczytu, etc.
    flags - dodatkowe flagi określające sposób działania wywołania mmap.
    fd - deskryptor pliku, który chcemy odwzorować w pamięci.
    offset - liczba określająca od którego miejsca w pliku chcemy rozpocząć odwzorowywanie.*/

    plikZ = open(sciezkaZ, O_RDONLY);
    
    if(sciezkaZ<0)
	{
	    printf ("błąd: %s ", strerror (errno)); 
	    exit(1);
   	}
    rozmiarPliku = sys_lseek(plikZ, 0, SEEK_END);
    zr = mmap(NULL, rozmiarPliku, PROT_READ, MAP_PRIVATE, plikZ, 0);

    plikD = open(sciezkaD, O_RDWR | O_CREAT, 777);
    ftruncate(plikD, rozmiarPliku);
    doc = mmap(NULL, rozmiarPliku, PROT_READ | PROT_WRITE, MAP_SHARED, plikD, 0);

    /*mmap, munmap - mapowanie lub usunięcie mapowania plików lub urządzeń w pamięci

    memcpy (void* dest, const void* src, size_t size);
    dest - wskaźnik do obiektu docelowego.
    src - wskaźnik do obiektu źródłowego.
    size - liczba bajtów do skopiowania.

    munmap(void *start, size_t length);*/

    memcpy(doc, zr, rozmiarPliku);
    munmap(zr, rozmiarPliku);
    munmap(doc, rozmiarPliku);

    close(plikZ);
    close(plikD);
}

bool czas_modyfikacji (char *plikZ, char* plikD)
{
    struct stat filestatZ;
    struct stat filestatD;
    
    stat(plikZ, &filestatZ);
    stat(plikD, &filestatD);
    
     return (filestatZ.st_mtim.tv_sec < filestatD.st_mtim.tv_sec);
    
}

void zmiana_daty(char *plikZ, char *plikD)
{
    struct stat filestat;
    struct utimbuf nowa_data; // to specify new access and modification times for a file
    
    stat(plikZ, &filestat);
    
    nowa_data.actime = filestat.st_atim.tv_sec;
    nowa_data.modtime = filestat.st_mtim.tv_sec;
    
    utime(plikD, &nowa_data);   // zmienia czas dostepu i modyfikacji pliku
    chmod(plikD, filestat.st_mode);

}

volatile int sygnal = 1;

int demon(int argc, char **argv)  
{
    if(argc<=1 ) // dod arg by zmienic
    {
        //printf ("błąd: %s ", strerror (errno)); 
        printf("za mało argumnetów\n ");
	    return 1;
    }
	
    if(argc>5) //jesli jest wiecej niż 3 arg
    {
        printf("za duzo argumnetów\n ");
        return 1;
    }

    float czas;

    if((is_dir(argv[2]) == false) && (is_dir(argv[1]) == false))
    {
        printf("Scieżka zródłowa i docelowa nie jest katalogiem\n");
      //  return -1;
    }
    if(is_dir(argv[1]) == false)
    {
        printf("Scieżka zródłowa nie jest katalogiem\n");
       // return -1;  
    }
    if(is_dir(argv[2]) == false)
    {
        printf("Scieżka docelowa nie jest katalogiem\n");
        // return -1;
    }
   
    printf("Oba żródła są katalogami\n");

    setlogmask (LOG_UPTO (LOG_NOTICE)); //maksymalny, najważniejszy log jaki można wysłać;
    openlog ("demon-projekt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);    //pierwszy arg nazwa programiu, pózniej co ma się wypisać oprócz samej wiadomosci chyba?

    void handler(int signum)
    {
        printf("signal!\n");
        sygnal=0;
    }
    signal(SIGUSR1, handler);
	
	 struct dirent *plikZ; //wskazuje na element w katalogu; przechowuje rózne informacje
    DIR * sciezkaZ; //reprezentuje strumień sciezki
    struct dirent *plikD;
    DIR * sciezkaD;

char sc[50];
char t[ 100 ] = "";
char * ar2;
char * ar1;

//pliki zrodlowe
    if((sciezkaZ = opendir (argv[1]))!=NULL) //otwiera strumień do katalogu
    {        
        while((plikZ = readdir (sciezkaZ))!=NULL)  //zwraca wskaznik do struktury reprez. plik
        {
            strcpy(sc,argv[1]);
            strcat(sc,"/");
            ar1=strcat(sc,plikZ->d_name);
            struct stat sb;
            stat(ar1, &sb);
            strftime(t, 100, "%d/%m/%Y %H:%M:%S", localtime( &sb.st_mtime));
            if (!S_ISREG(sb.st_mode)) 
            {

                continue;
            } 
            else
            {
                dodawanie(&plikiZr, plikZ->d_name, t);
            }
            
        }
        closedir(sciezkaZ); //zamyka strumien sciezki
    }
    else
    {
        printf("Bład otwarcia podanej scieżki zrodlowej\n");
        exit(1);
    } 
	
	
	//wypisanie listy
printf("-------------------------------------------------\n");
wypisz_liste(plikiZr);
printf("-------------------------------------------------\n");
wypisz_liste(plikiDoc);

// signal(SIGUSR1, ignor);//ignoruje demona

Pliki * plikiZrKopia;
Pliki * plikiDocKopia;
plikiZrKopia = plikiZr->nastepny;
plikiDocKopia = plikiDoc->nastepny;
int sk=0;


    while(plikiZr!=NULL)
    {   
        plikiDoc=plikiDocKopia;
        sk=0;        
        while((plikiDoc!=NULL) && (sk==0))
        {             
            if(strcmp(plikiZr->nazwaPliku,plikiDoc->nazwaPliku)==0)
            {
                if(strcmp(plikiZr->dataPliku,plikiDoc->dataPliku)!=0)
                {
                    char sc6[50];
                    char sc7[50];
                    strcpy(sc6,argv[1]);
                    strcat(sc6,"/");
                    strcat(sc6,plikiZr->nazwaPliku);
                    strcpy(sc7,argv[2]);
                    strcat(sc7,"/");
                    strcat(sc7,plikiDoc->nazwaPliku);                    
                    kopiowaie(sc6,sc7);
                    zmiana_daty(sc6,sc7);
                    sk=1;
                    //break;
                }                                
            } 
            else
            {

                if(plikiDoc->nastepny==NULL && sk==0)
                {
                    char sc3[50];
                    char sc5[50];
                    strcpy(sc3,argv[1]);
                    strcat(sc3,"/");
                    strcat(sc3,plikiZr->nazwaPliku);
                    strcpy(sc5,argv[2]);
                    strcat(sc5,"/");
                    strcat(sc5,plikiZr->nazwaPliku);
                    kopiowaie(sc3,sc5);
                }
            }   
            plikiDoc=plikiDoc->nastepny;   
        }
        plikiZr=plikiZr->nastepny;
    }


    if(sygnal==0)
    {
        czas = 0;
        //nie działa log
        printf("Dem0n budzi się\n");
        sleep(czas);
        syslog (LOG_NOTICE, "Demon został obudzony poprzez sygnal\n");
    }
    else
    {
        czas=5;
        if(argc==4)  //działa :D
        {
            czas=atof(argv[3]);
            printf("%f\n ",czas);
        }   
        printf("%f \n",czas);
        sleep(czas);
        syslog (LOG_NOTICE, "Demon został obudzony po %f minutach\n",czas);
        closelog (); 
    }

//  while (1) 
//  { 
 //tu działa :)
        pid_t p;
        p=fork();
    
        if (p < 0) //obsługa błędów pid
        {
            exit(EXIT_FAILURE);
        }
        if(p==0)
        {
            //execvp("firefox", argv);
            printf("Dziaaaaaała\n");
            sleep(10);

 
            //brak pliku w folderze docelowym
            //kopiowanie pliku do katalogu docelowego (open,read)

            //działa ale nie czysci na koniec bufora
                   // kopiowanie(argv[1],argv[2]);
        
                    //zamiast wyswietlania - kopiowanie

                    //zmienić jeszcze to co sie zapisuje
                    syslog(LOG_NOTICE, "plik został skopiowany\n");
            
            
        }

      // plikiZ=plikiZ->next;

    closelog ();   //zamkniecie logu

    exit(EXIT_SUCCESS);


}
