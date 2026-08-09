#ifndef MULTI_H
#define MULTI_H

#include <stdbool.h>

/* Modalita' Wi-Fi (Ni-Fi, 2 console, senza access point).
   API verificate su dir: dswifi9.h (local multiplayer mode guide).
   Il protocollo e' minuscolo: ogni console invia il proprio stato
   (giorno, PV, punteggio, stato finale) e riceve quello dell'avversario.
   NOTA: il "Download Play" di Nintendo NON e' riproducibile da homebrew
   (serve la chiave RSA di Nintendo): questa e' la modalita' WiFi
   peer-to-peer documentata per l'homebrew DS. */

enum {
    MP_OFF = 0,          /* non usato */
    MP_HOST_WAIT,        /* host: in attesa che la seconda DS si unisca */
    MP_CLIENT_SEARCH,    /* client: alla ricerca del host */
    MP_CLIENT_CONNECT,   /* client: associamento in corso */
    MP_LINKED,           /* connessione attiva */
    MP_FAILED,           /* errore (es. nessun host trovato) */
};

/* esito dell'avversario (MpAvversario.esito) */
#define MP_ESITO_GIOCO 0 /* ancora in partita */
#define MP_ESITO_PERSO 1 /* avversario sconfitto */
#define MP_ESITO_VINTO 2 /* avversario ha vinto */

/* stato dell'avversario ricevuto via WiFi */
typedef struct {
    int giorno;
    int pv;
    int punteggio;
    int esito;           /* MP_ESITO_* */
    bool pronto;         /* true = ricevuto almeno un aggiornamento */
} MpAvversario;

/* giornata multi: rb mp_start_host(), mp_start_client() */
void mp_start_host(void);
void mp_start_client(void);
void mp_stop(void);

/* da chiamare ogni frame nel loop UI (avanza scan/polling/non blocca) */
void mp_poll(void);

/* vero quando le due console sono collegate e la partita inizia */
bool mp_linked(void);

/* manda lo stato corrente della partita locale all'avversario */
void mp_send_status(void);

/* stato corrente della macchina MP (per disegnare la schermata WiFi) */
int mp_stato(void);
bool mp_is_host(void);

/* stato dell'avversario ricevuto (per il pannello e il verdetto) */
const MpAvversario *mp_avversario(void);

/* esito per la schermata finale: -1 = n/d, 0 = il locale ha perso,
   1 = ha vinto (confronta vittoria/punti quando entrambi finiti) */
int mp_verdetto(bool mia_vittoria);

#endif