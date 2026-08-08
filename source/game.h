#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#define MAX_RISORSA     9
#define PV_MAX          20
#define GIORNI_VITTORIA 21
#define MAX_CARTE       30
#define MAX_MAZZO       30
#define MAX_SCARTO      30
#define MAX_MANO        12
#define MAX_ACC         13
#define MAX_OBIETTIVI   3
#define N_OBIETTIVI     6

enum { TIPO_AZIONE = 0, TIPO_STRUMENTO = 1, TIPO_PERSONA = 2 };
enum { R_CIPO = 0, R_ACQUA = 1, R_LEGNA = 2, R_ERBE = 3, R_COUNT = 4 };

typedef struct {
    int giorno;
    int pv;
    int risorse[R_COUNT];
    int mazzo[MAX_MAZZO]; int nmazzo;
    int scarto[MAX_SCARTO]; int nscarto;
    int mano[MAX_MANO]; int nmano;
    int acc[MAX_ACC]; int nacc;
    int azioni;
    int bonusAzioniOggi;
    int domaniCarte;
    bool pentolaUsata;
    bool bloccoFine;
    bool modale;
    bool fine;
    bool vittoria;
    int ultimoEvento;
    int negativiDiFila;
    int strumentiGiocati;
    int obiettivi[MAX_OBIETTIVI]; int nobb;
} Stato;

/* Scelta da mostrare all'utente durante un evento */
typedef struct {
    int tipo;            /* 1 = tempesta, 2 = malattia, 3 = sciame */
    int costo;           /* legna richiesta dalla tempesta */
    const char *titolo;
    const char *testo;
    const char *op1;
    const char *op2;
} Scelta;

/* Diario di bordo (testo consumato dalla UI) */
#define LOG_MAX   60
#define LOG_LEN   64
typedef struct { char testo[LOG_LEN]; int tipo; } LogVoce;
/* tipo: 0 info, 1 bene, 2 danno, 3 evento, 4 obiettivo */

extern Stato G;
extern LogVoce g_log[LOG_MAX];
extern int g_nlog;

extern int g_evento_id;
extern bool g_pending_choice;
extern Scelta g_scelta;
extern bool g_autotest;
extern bool g_riepilogo;               /* riepilogo di fine giornata pronto */
extern char g_riep_righe[12][48];
extern int g_riep_tipo[12];            /* 0 info, 1 bene, 2 danno */
extern int g_riep_nrighe;
extern int g_riep_pv;
extern int g_punteggio_ultimo;
extern int g_finale_pos;               /* posizione top5 della partita appena finita */
extern int g_top5[5][3];               /* punti, giorni, pv */
extern int g_top5_n;

const char *risorsa_nome(int t);
const char *carta_nome(int id);
const char *carta_effetto(int id);
int carta_tipo(int id);
const char *evento_nome(int id);
const char *evento_testo(int id);
bool ha_carta(int id);

void g_log_linea(const char *msg, int tipo);
int g_r_add(int t, int n);
int g_r_spendi(int t, int n);
void g_cura(int n);
void g_pv_danno(int n, const char *motivo, bool immediato);

void g_nuova_partita(void);
void g_nuovo_giorno(void);
void g_continua_giorno(void);
void g_pesca_carte(int n);
bool g_gioca_carta(int id);
void g_usa_pentola(void);
void g_fine_giornata(void);
void g_scelta_rispondi(int op);        /* 0 = op1, 1 = op2 */
void g_riepilogo_avanti(void);
void g_fine_partita(bool vittoria);

const char *g_obiettivo_testo(int idx);
bool g_obiettivo_fatto(int idx);
int g_calcola_punteggio(int *obiettivi_fatti);
int g_registra_record(int punti, int giorni, int pv);

void rng_seed(unsigned int s);
void g_test_automatici(int *ok, int *fail);

/* salvataggio (implementato in save.c) */
bool g_salva(void);
bool g_carica(void);
bool g_salva_esiste(void);
void g_salva_elimina(void);
void g_carica_top5(void);
void g_salva_top5(void);

#endif
