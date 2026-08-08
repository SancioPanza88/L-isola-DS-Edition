/* ============================================================
   L'ISOLA - porting NDS della logica di game.js (web)
   Dati: 30 carte + 12 eventi. Regole identiche alla versione web.
   ============================================================ */

#include <stdio.h>
#include <string.h>
#include "game.h"

Stato G;

LogVoce g_log[LOG_MAX];
int g_nlog = 0;

int g_evento_id = 0;
bool g_pending_choice = false;
Scelta g_scelta;
bool g_autotest = false;
bool g_riepilogo = false;
char g_riep_righe[12][48];
int g_riep_tipo[12];
int g_riep_nrighe = 0;
int g_riep_pv = 0;
int g_punteggio_ultimo = 0;
int g_finale_pos = -1;
int g_top5[5][3];
int g_top5_n = 0;

/* ---- RNG (xorshift32, algoritmo documentato) ---- */
static unsigned int rng_state = 0x9E3779B9u;

void rng_seed(unsigned int s) { rng_state = s ? s : 0x9E3779B9u; }

static unsigned int rng_next(void) {
    unsigned int x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}

static int rng_below(int n) { return (int)(rng_next() % (unsigned int)n); }

/* ---- Risorse ---- */

static const char *R_NOMI[R_COUNT] = { "Cibo", "Acqua", "Legna", "Erbe" };

const char *risorsa_nome(int t) { return (t >= 0 && t < R_COUNT) ? R_NOMI[t] : "?"; }

/* ---- Carte (30) ---- */

typedef struct {
    int id;
    const char *nome;
    const char *effetto;
    int tipo;
} CartaInfo;

static const CartaInfo CARDS[MAX_CARTE] = {
    { 1,  "Raccolta di bacche",   "+2 Cibo",                       TIPO_AZIONE },
    { 2,  "Pesca con la lancia",   "+2 Cibo",                       TIPO_AZIONE },
    { 3,  "Trappola per conigli",  "+2 Cibo",                       TIPO_AZIONE },
    { 4,  "Cocco dell'isola",      "+1 Cibo, +1 Acqua",             TIPO_AZIONE },
    { 5,  "Fontana d'acqua",       "+2 Acqua",                      TIPO_AZIONE },
    { 6,  "Scavo del pozzo",       "+2 Acqua",                      TIPO_AZIONE },
    { 7,  "Raccolta legna",        "+2 Legna",                      TIPO_AZIONE },
    { 8,  "Erbe medicinali",       "+2 Erbe",                       TIPO_AZIONE },
    { 9,  "Riposo",                "+1 PV",                         TIPO_AZIONE },
    { 10, "Esplorazione",          "Peschi 1 carta",                TIPO_AZIONE },
    { 11, "Pane di radici",        "+1 Cibo, +1 Erbe",              TIPO_AZIONE },
    { 12, "Frutta tropicale",      "+2 Cibo",                       TIPO_AZIONE },
    { 13, "Accetta",               "Azioni Legna: +1 extra",        TIPO_STRUMENTO },
    { 14, "Rete da pesca",         "Fine giornata: +1 Cibo",        TIPO_STRUMENTO },
    { 15, "Pentola di coccio",     "1 Legna -> +1 Cibo (1/giorno)", TIPO_STRUMENTO },
    { 16, "Capanna",               "Protegge da Tempesta e Freddo", TIPO_STRUMENTO },
    { 17, "Fal\xF2",               "Il Freddo non ti danneggia",    TIPO_STRUMENTO },
    { 18, "Accendino",             "Tempesta: basta 1 Legna",       TIPO_STRUMENTO },
    { 19, "Amaca",                 "Fine giornata: +1 PV",          TIPO_STRUMENTO },
    { 20, "Zaino grande",          "Peschi 6 carte",                TIPO_STRUMENTO },
    { 21, "Naufrago pescatore",    "Ogni 3 giorni: +2 Cibo",        TIPO_PERSONA },
    { 22, "Anziana guaritrice",    "Ogni 4 giorni: +2 PV",          TIPO_PERSONA },
    { 23, "Cane da guardia",       "Il Ladro non ruba",             TIPO_PERSONA },
    { 24, "Ragazzo scalatore",     "Ogni 2 giorni: +1 carta",       TIPO_PERSONA },
    { 25, "Cuciniera",             "Fine giornata: +1 PV se hai mangiato", TIPO_PERSONA },
    { 26, "Tesoro del relitto",    "+3 Cibo, +2 Legna",             TIPO_AZIONE },
    { 27, "Medicina",              "+3 PV",                         TIPO_AZIONE },
    { 28, "Mappa del naufragio",   "+2 azioni oggi",                TIPO_AZIONE },
    { 29, "Bottiglia col messaggio", "Domani: +2 carte",            TIPO_AZIONE },
    { 30, "Giorno fortunato",      "+2 carte, +1 azione",           TIPO_AZIONE },
};

const char *carta_nome(int id) { return (id >= 1 && id <= 30) ? CARDS[id - 1].nome : "?"; }
const char *carta_effetto(int id) { return (id >= 1 && id <= 30) ? CARDS[id - 1].effetto : ""; }
int carta_tipo(int id) { return (id >= 1 && id <= 30) ? CARDS[id - 1].tipo : TIPO_AZIONE; }

bool ha_carta(int id) {
    int i;
    for (i = 0; i < G.nacc; i++) if (G.acc[i] == id) return true;
    return false;
}

/* ---- Eventi (12) ---- */

typedef struct {
    int id;
    const char *nome;
    const char *testo;
    int negativo;
} EventoInfo;

static const EventoInfo EVENTS[12] = {
    { 1,  "Tempesta",
      "Il cielo si oscura: devi rinforzare il campo. Spendi 2 Legna (1 con "
      "l'Accendino) oppure perdi 1 PV. Con la Capanna sei al sicuro.", 1 },
    { 2,  "Freddo notturno",
      "La notte \xE8 gelida. Senza Fal\xF2 o Capanna perdi 1 PV.", 1 },
    { 3,  "Pioggia",
      "Piove a dirotto: raccogli acqua fresca. +2 Acqua.", 0 },
    { 4,  "Ladro",
      "Un ladro si intrufola di notte e ti ruba 1 Cibo. Il Cane da "
      "guardia lo scaccia.", 1 },
    { 5,  "Malattia",
      "Ti ammali. Spendi 1 Erba per curarti, oppure subisci -1 PV.", 1 },
    { 6,  "Mare calmo",
      "Il mare \xE8 calmo e limpido. Con la Rete da pesca attiva "
      "ottieni +1 Cibo.", 0 },
    { 7,  "Nido di tartarughe",
      "Trovi un nido di tartarughe sulla spiaggia. +2 Cibo.", 0 },
    { 8,  "Nave lontana",
      "Avvisti una nave all'orizzonte. Domani avrai +1 carta in mano.", 0 },
    { 9,  "Grandi onde",
      "Grandi onde spazzano la riva: -1 Cibo.", 1 },
    { 10, "Giorno di sole",
      "Il sole splende e ti riempie di energia: +1 azione oggi.", 0 },
    { 11, "Sciame di insetti",
      "Uno sciame di insetti ti tormenta. Spendi 1 Erba per allontanarlo, "
      "oppure subisci -1 PV.", 1 },
    { 12, "Frutta di stagione",
      "Trovi frutta matura e acqua di cocco: +1 Cibo e +1 Acqua.", 0 },
};

const char *evento_nome(int id) { return (id >= 1 && id <= 12) ? EVENTS[id - 1].nome : "?"; }
const char *evento_testo(int id) { return (id >= 1 && id <= 12) ? EVENTS[id - 1].testo : ""; }

/* ---- Obiettivi (6, ne vengono pescati 3 per partita) ---- */

static const char *OBIETTIVI[N_OBIETTIVI] = {
    "Arriva al giorno 16 con almeno 10 PV",
    "Termina con almeno 5 STRUMENTI e PERSONE attivi",
    "Finisci con 9 Cibo e 9 Acqua",
    "Gioca almeno 8 carte STRUMENTO o PERSONA",
    "Accendi il Fal\xF2 entro il giorno 8",
    "Termina con almeno 1 Erba in riserva",
};

const char *g_obiettivo_testo(int idx) { return (idx >= 0 && idx < N_OBIETTIVI) ? OBIETTIVI[idx] : "?"; }

bool g_obiettivo_fatto(int idx) {
    switch (idx) {
        case 0: return G.giorno >= 16 && G.pv >= 10;
        case 1: return G.nacc >= 5;
        case 2: return G.risorse[R_CIPO] >= 9 && G.risorse[R_ACQUA] >= 9;
        case 3: return G.strumentiGiocati >= 8;
        case 4: return ha_carta(17) && G.giorno >= 8;
        case 5: return G.risorse[R_ERBE] >= 1;
    }
    return false;
}

/* ---- Diario ---- */

void g_log_linea(const char *msg, int tipo) {
    if (g_nlog >= LOG_MAX) {
        int i;
        for (i = 1; i < LOG_MAX; i++) g_log[i - 1] = g_log[i];
        g_nlog = LOG_MAX - 1;
    }
    strncpy(g_log[g_nlog].testo, msg, LOG_LEN - 1);
    g_log[g_nlog].testo[LOG_LEN - 1] = '\0';
    g_log[g_nlog].tipo = tipo;
    g_nlog++;
}

/* ---- Operazioni di base ---- */

int g_r_add(int t, int n) {
    int prima = G.risorse[t];
    G.risorse[t] = prima + n;
    if (G.risorse[t] > MAX_RISORSA) G.risorse[t] = MAX_RISORSA;
    if (G.risorse[t] >= MAX_RISORSA && prima + n > MAX_RISORSA) {
        char m[64];
        snprintf(m, sizeof(m), "Il %s ha raggiunto il massimo (9).", risorsa_nome(t));
        g_log_linea(m, 0);
    }
    return G.risorse[t] - prima;
}

int g_r_spendi(int t, int n) {
    int speso = G.risorse[t] < n ? G.risorse[t] : n;
    G.risorse[t] -= speso;
    return speso;
}

void g_cura(int n) {
    int prima = G.pv;
    G.pv += n;
    if (G.pv > PV_MAX) G.pv = PV_MAX;
    (void)prima;
}

void g_pv_danno(int n, const char *motivo, bool immediato) {
    int prima = G.pv;
    G.pv -= n;
    if (G.pv < 0) G.pv = 0;
    if (motivo && prima - G.pv > 0) {
        char m[LOG_LEN];
        snprintf(m, sizeof(m), "%s: -%d PV.", motivo, prima - G.pv);
        g_log_linea(m, 2);
    }
    if (immediato && G.pv <= 0) g_fine_partita(false);
}

static void rimescola(void) {
    int i;
    for (i = 0; i < G.nscarto; i++) G.mazzo[i] = G.scarto[i];
    G.nmazzo = G.nscarto;
    G.nscarto = 0;
    for (i = G.nmazzo - 1; i > 0; i--) {
        int j = rng_below(i + 1);
        int t = G.mazzo[i]; G.mazzo[i] = G.mazzo[j]; G.mazzo[j] = t;
    }
}

/* ---- Flusso di gioco ---- */

static void pesca_mano(void) {
    int n = 5;
    if (ha_carta(20)) n++;
    if (G.domaniCarte > 0) { n += G.domaniCarte; G.domaniCarte = 0; }
    if (ha_carta(24) && G.giorno % 2 == 0) n++;
    g_pesca_carte(n);
    {
        char m[LOG_LEN];
        snprintf(m, sizeof(m), "Peschi %d carte.", n);
        g_log_linea(m, 0);
    }
}

void g_pesca_carte(int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (G.nmazzo == 0) {
            if (G.nscarto == 0) break;
            rimescola();
            g_log_linea("Il mazzo \xE8 finito: rimescoli lo scarto.", 0);
        }
        G.nmazzo--;
        if (G.nmano < MAX_MANO) G.mano[G.nmano++] = G.mazzo[G.nmazzo];
    }
}

void g_continua_giorno(void) {
    char m[LOG_LEN];
    if (G.fine) return;
    pesca_mano();
    G.azioni = 3 + G.bonusAzioniOggi;
    G.bloccoFine = false;
    snprintf(m, sizeof(m), "Hai %d azioni disponibili oggi.", G.azioni);
    g_log_linea(m, 0);
}

/* Effetto immediato di una carta AZIONE */
static void carta_usa(int id) {
    char m[LOG_LEN];
    switch (id) {
        case 1:  g_r_add(R_CIPO, 2);   g_log_linea("Raccolta di bacche: +2 Cibo.", 1); break;
        case 2:  g_r_add(R_CIPO, 2);   g_log_linea("Pesca con la lancia: +2 Cibo.", 1); break;
        case 3:  g_r_add(R_CIPO, 2);   g_log_linea("Trappola per conigli: +2 Cibo.", 1); break;
        case 4:  g_r_add(R_CIPO, 1); g_r_add(R_ACQUA, 1);
                 g_log_linea("Cocco dell'isola: +1 Cibo, +1 Acqua.", 1); break;
        case 5:  g_r_add(R_ACQUA, 2);  g_log_linea("Fontana d'acqua: +2 Acqua.", 1); break;
        case 6:  g_r_add(R_ACQUA, 2);  g_log_linea("Scavo del pozzo: +2 Acqua.", 1); break;
        case 7: {
            int n = 2 + (ha_carta(13) ? 1 : 0);
            g_r_add(R_LEGNA, n);
            snprintf(m, sizeof(m), "Raccolta legna: +%d Legna.", n);
            g_log_linea(m, 1);
            break;
        }
        case 8:  g_r_add(R_ERBE, 2);   g_log_linea("Erbe medicinali: +2 Erbe.", 1); break;
        case 9:  g_cura(1);            g_log_linea("Riposo: +1 PV.", 1); break;
        case 10: g_log_linea("Esplorazione: peschi 1 carta dal mazzo.", 0);
                 g_pesca_carte(1); break;
        case 11: g_r_add(R_CIPO, 1); g_r_add(R_ERBE, 1);
                 g_log_linea("Pane di radici: +1 Cibo, +1 Erbe.", 1); break;
        case 12: g_r_add(R_CIPO, 2);   g_log_linea("Frutta tropicale: +2 Cibo.", 1); break;
        case 26: g_r_add(R_CIPO, 3); g_r_add(R_LEGNA, 2);
                 g_log_linea("Tesoro del relitto: +3 Cibo, +2 Legna.", 1); break;
        case 27: g_cura(3);            g_log_linea("Medicina: +3 PV.", 1); break;
        case 28: G.azioni += 2;        g_log_linea("Mappa del naufragio: +2 azioni oggi.", 1); break;
        case 29: G.domaniCarte += 2;   g_log_linea("Bottiglia col messaggio: domani +2 carte.", 1); break;
        case 30: G.azioni += 1;
                 g_log_linea("Giorno fortunato: peschi 2 carte e hai +1 azione.", 1);
                 g_pesca_carte(2); break;
    }
}

bool g_gioca_carta(int id) {
    int idx;
    char m[LOG_LEN];
    if (G.fine || G.modale) return false;
    for (idx = 0; idx < G.nmano; idx++) if (G.mano[idx] == id) break;
    if (idx >= G.nmano) return false;
    if (G.azioni <= 0) return false;
    for (idx = 0; idx < G.nmano; idx++) if (G.mano[idx] == id) break;
    {
        int i;
        for (i = idx; i < G.nmano - 1; i++) G.mano[i] = G.mano[i + 1];
        G.nmano--;
    }
    G.azioni--;
    snprintf(m, sizeof(m), "Giochi: %s.", carta_nome(id));
    g_log_linea(m, 0);
    if (carta_tipo(id) == TIPO_AZIONE) {
        carta_usa(id);
        if (G.nscarto < MAX_SCARTO) G.scarto[G.nscarto++] = id;
    } else {
        if (G.nacc < MAX_ACC) G.acc[G.nacc++] = id;
        G.strumentiGiocati++;
        snprintf(m, sizeof(m), "%s \xE8 ora attivo nell'accampamento.", carta_nome(id));
        g_log_linea(m, 1);
    }
    return true;
}

void g_usa_pentola(void) {
    if (G.fine || G.modale) return;
    if (!ha_carta(15)) return;
    if (G.pentolaUsata) { g_log_linea("Pentola di coccio: gia usata oggi.", 0); return; }
    if (G.risorse[R_LEGNA] < 1) { g_log_linea("Pentola di coccio: non hai Legna!", 2); return; }
    G.risorse[R_LEGNA]--;
    g_r_add(R_CIPO, 1);
    G.pentolaUsata = true;
    g_log_linea("Pentola di coccio: converti 1 Legna in +1 Cibo.", 1);
}

/* ---- Eventi ---- */

void g_fine_partita(bool vittoria) {
    int fatti;
    if (G.fine) return;
    G.fine = true;
    G.vittoria = vittoria;
    g_punteggio_ultimo = g_calcola_punteggio(&fatti);
    g_finale_pos = g_registra_record(g_punteggio_ultimo, G.giorno, G.pv);
}

int g_calcola_punteggio(int *obiettivi_fatti) {
    int fatti = 0, i;
    for (i = 0; i < G.nobb; i++) if (g_obiettivo_fatto(G.obiettivi[i])) fatti++;
    if (obiettivi_fatti) *obiettivi_fatti = fatti;
    return G.pv * 10
        + (G.risorse[R_CIPO] + G.risorse[R_ACQUA] + G.risorse[R_LEGNA] + G.risorse[R_ERBE]) * 2
        + G.nacc * 5
        + G.giorno * 3
        + fatti * 25;
}

/* Risolve l'evento corrente; true = risolto (giornata continua),
   false = serve una scelta dell'utente. */
bool g_evento_risolvi(void) {
    static char scelta_op1[32];
    char m[LOG_LEN];
    switch (g_evento_id) {
        case 1: { /* Tempesta */
            int costo;
            if (ha_carta(16)) {
                g_log_linea("Tempesta: la Capanna ti protegge.", 1);
                break;
            }
            costo = ha_carta(18) ? 1 : 2;
            if (G.risorse[R_LEGNA] >= costo) {
                g_pending_choice = true;
                g_scelta.tipo = 1;
                g_scelta.costo = costo;
                g_scelta.titolo = "TEMPESTA!";
                g_scelta.testo = "Spendi Legna per rinforzare il campo, oppure subisci -1 PV.";
                snprintf(scelta_op1, sizeof(scelta_op1), "Spendi %d Legna", costo);
                g_scelta.op1 = scelta_op1;
                g_scelta.op2 = "Subisci -1 PV";
                return false;
            }
            g_log_linea("Tempesta: non hai Legna a sufficienza!", 2);
            g_pv_danno(1, "Tempesta", true);
            break;
        }
        case 2: /* Freddo notturno */
            if (ha_carta(17) || ha_carta(16)) {
                g_log_linea("Freddo notturno: il Fal\xF2 (o la Capanna) ti scalda.", 1);
            } else {
                g_pv_danno(1, "Freddo notturno", true);
            }
            break;
        case 3:
            g_r_add(R_ACQUA, 2);
            g_log_linea("Pioggia: +2 Acqua.", 1);
            break;
        case 4: /* Ladro */
            if (ha_carta(23)) {
                g_log_linea("Ladro: il Cane da guardia lo scaccia. Niente furto.", 1);
            } else if (g_r_spendi(R_CIPO, 1) == 1) {
                g_log_linea("Ladro: ti ruba 1 Cibo.", 2);
            } else {
                g_log_linea("Ladro: non hai Cibo da rubare.", 0);
            }
            break;
        case 5: /* Malattia */
            if (G.risorse[R_ERBE] >= 1) {
                g_pending_choice = true;
                g_scelta.tipo = 2;
                g_scelta.costo = 1;
                g_scelta.titolo = "MALATTIA!";
                g_scelta.testo = "Spendi 1 Erba per curarti, oppure subisci -1 PV.";
                g_scelta.op1 = "Spendi 1 Erba";
                g_scelta.op2 = "Subisci -1 PV";
                return false;
            }
            g_log_linea("Malattia: non hai Erbe!", 2);
            g_pv_danno(1, "Malattia", true);
            break;
        case 6: /* Mare calmo */
            if (ha_carta(14)) {
                g_r_add(R_CIPO, 1);
                g_log_linea("Mare calmo: la Rete da pesca ti porta +1 Cibo.", 1);
            } else {
                g_log_linea("Mare calmo: niente da pescare senza una rete.", 0);
            }
            break;
        case 7:
            g_r_add(R_CIPO, 2);
            g_log_linea("Nido di tartarughe: +2 Cibo.", 1);
            break;
        case 8:
            G.domaniCarte += 1;
            g_log_linea("Nave lontana: domani avrai +1 carta in mano.", 1);
            break;
        case 9: /* Grandi onde */
            if (g_r_spendi(R_CIPO, 1) == 1) g_log_linea("Grandi onde: -1 Cibo.", 2);
            else g_log_linea("Grandi onde: non hai Cibo da perdere.", 0);
            break;
        case 10:
            G.bonusAzioniOggi += 1;
            g_log_linea("Giorno di sole: +1 azione oggi.", 1);
            break;
        case 11: /* Sciame di insetti */
            if (G.risorse[R_ERBE] >= 1) {
                g_pending_choice = true;
                g_scelta.tipo = 3;
                g_scelta.costo = 1;
                g_scelta.titolo = "SCIAME DI INSETTI!";
                g_scelta.testo = "Spendi 1 Erba per allontanarli, oppure subisci -1 PV.";
                g_scelta.op1 = "Spendi 1 Erba";
                g_scelta.op2 = "Subisci -1 PV";
                return false;
            }
            g_log_linea("Sciame di insetti: non hai Erbe!", 2);
            g_pv_danno(1, "Sciame di insetti", true);
            break;
        case 12:
            g_r_add(R_CIPO, 1);
            g_r_add(R_ACQUA, 1);
            g_log_linea("Frutta di stagione: +1 Cibo, +1 Acqua.", 1);
            break;
    }
    g_continua_giorno();
    return true;
}

void g_scelta_rispondi(int op) {
    char m[LOG_LEN];
    int tipo = g_scelta.tipo;
    g_pending_choice = false;
    switch (tipo) {
        case 1: /* Tempesta */
            if (op == 0) {
                g_r_spendi(R_LEGNA, g_scelta.costo);
                snprintf(m, sizeof(m), "Tempesta: spendi %d Legna.", g_scelta.costo);
                g_log_linea(m, 0);
            } else {
                g_pv_danno(1, "Tempesta", true);
            }
            break;
        case 2: /* Malattia */
            if (op == 0) { g_r_spendi(R_ERBE, 1); g_log_linea("Malattia: ti curi con 1 Erba.", 0); }
            else g_pv_danno(1, "Malattia", true);
            break;
        case 3: /* Sciame */
            if (op == 0) { g_r_spendi(R_ERBE, 1); g_log_linea("Sciame: li allontani con 1 Erba.", 0); }
            else g_pv_danno(1, "Sciame di insetti", true);
            break;
    }
    g_continua_giorno();
}

void g_pesca_evento(void) {
    int pool[64];
    int npool = 0;
    int i, k;
    char m[LOG_LEN];

    /* scelta a peso: 4 per evento, negativo escluso dopo 2 negativi di fila,
       peso 7 dopo il giorno 10, mai lo stesso evento due giorni di fila */
    for (i = 0; i < 12; i++) {
        int peso = 4;
        if (EVENTS[i].negativo) {
            if (G.negativiDiFila >= 2) peso = 0;
            if (G.giorno > 10) peso = 7;
        }
        for (k = 0; k < peso; k++) pool[npool++] = i;
    }
    if (G.ultimoEvento) {
        int senza[64], nsenza = 0;
        for (i = 0; i < npool; i++)
            if (EVENTS[pool[i]].id != G.ultimoEvento) senza[nsenza++] = pool[i];
        if (nsenza > 0) {
            npool = nsenza;
            for (i = 0; i < nsenza; i++) pool[i] = senza[i];
        }
    }
    if (npool == 0) {
        for (i = 0; i < 12; i++) pool[i] = i;
        npool = 12;
    }
    g_evento_id = EVENTS[pool[rng_below(npool)]].id;
    if (EVENTS[g_evento_id - 1].negativo) G.negativiDiFila++;
    else G.negativiDiFila = 0;
    G.ultimoEvento = g_evento_id;
    snprintf(m, sizeof(m), "Evento del giorno: %s.", EVENTS[g_evento_id - 1].nome);
    g_log_linea(m, 3);
    g_evento_risolvi();
}

static void passivi_giornalieri(void) {
    if (ha_carta(21) && G.giorno % 3 == 0) {
        g_r_add(R_CIPO, 2);
        g_log_linea("Il Naufrago pescatore ti porta +2 Cibo.", 1);
    }
    if (ha_carta(22) && G.giorno % 4 == 0) {
        g_cura(2);
        g_log_linea("L'Anziana guaritrice ti cura: +2 PV.", 1);
    }
}

void g_nuovo_giorno(void) {
    char m[LOG_LEN];
    if (G.fine) return;
    G.giorno++;
    G.pentolaUsata = false;
    G.azioni = 0;
    G.bonusAzioniOggi = 0;
    G.nmano = 0;
    snprintf(m, sizeof(m), "---- GIORNO %d ----", G.giorno);
    g_log_linea(m, 3);
    passivi_giornalieri();
    if (G.fine) return;
    g_pesca_evento();
}

void g_nuova_partita(void) {
    int i, j;
    char m[LOG_LEN];
    memset(&G, 0, sizeof(G));
    G.pv = PV_MAX;
    G.risorse[R_CIPO] = 5;
    G.risorse[R_ACQUA] = 5;
    G.risorse[R_LEGNA] = 3;
    G.risorse[R_ERBE] = 1;
    G.nmazzo = MAX_CARTE;
    for (i = 0; i < MAX_CARTE; i++) G.mazzo[i] = i + 1;
    for (i = MAX_CARTE - 1; i > 0; i--) {
        j = rng_below(i + 1);
        { int t = G.mazzo[i]; G.mazzo[i] = G.mazzo[j]; G.mazzo[j] = t; }
    }
    /* 3 obiettivi pescati a caso tra 6 */
    {
        int ord[N_OBIETTIVI];
        for (i = 0; i < N_OBIETTIVI; i++) ord[i] = i;
        for (i = N_OBIETTIVI - 1; i > 0; i--) {
            j = rng_below(i + 1);
            { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
        }
        for (i = 0; i < MAX_OBIETTIVI; i++) G.obiettivi[i] = ord[i];
        G.nobb = MAX_OBIETTIVI;
    }
    g_nlog = 0;
    g_riepilogo = false;
    g_pending_choice = false;
    snprintf(m, sizeof(m), "Nuova partita: naufraghi sull'isola. Sopravvivi %d giorni!",
             GIORNI_VITTORIA);
    g_log_linea(m, 3);
    for (i = 0; i < G.nobb; i++) {
        snprintf(m, sizeof(m), "Obiettivo %d: %s", i + 1, g_obiettivo_testo(G.obiettivi[i]));
        g_log_linea(m, 4);
    }
    g_nuovo_giorno();
}

void g_fine_giornata(void) {
    char m[LOG_LEN];
    if (G.fine || G.modale || G.bloccoFine) return;
    G.bloccoFine = true;
    g_riep_nrighe = 0;
    g_log_linea("Fine giornata: consumi risorse e applichi i bonus.", 3);

    {
        int ciboConsumato = G.risorse[R_CIPO] >= 1;
        if (ciboConsumato) {
            G.risorse[R_CIPO]--;
            g_log_linea("Consumi 1 Cibo.", 0);
            strcpy(g_riep_righe[g_riep_nrighe], "Consumi 1 CIBO");
            g_riep_tipo[g_riep_nrighe++] = 0;
        } else {
            g_pv_danno(1, "Niente Cibo da mangiare", false);
            strcpy(g_riep_righe[g_riep_nrighe], "Niente CIBO: -1 PV");
            g_riep_tipo[g_riep_nrighe++] = 2;
        }
        if (G.risorse[R_ACQUA] >= 1) {
            G.risorse[R_ACQUA]--;
            g_log_linea("Consumi 1 Acqua.", 0);
            strcpy(g_riep_righe[g_riep_nrighe], "Consumi 1 ACQUA");
            g_riep_tipo[g_riep_nrighe++] = 0;
        } else {
            g_pv_danno(1, "Niente Acqua da bere", false);
            strcpy(g_riep_righe[g_riep_nrighe], "Niente ACQUA: -1 PV");
            g_riep_tipo[g_riep_nrighe++] = 2;
        }
        if (ha_carta(14)) {
            g_r_add(R_CIPO, 1);
            g_log_linea("La Rete da pesca ti porta +1 Cibo.", 1);
            strcpy(g_riep_righe[g_riep_nrighe], "Rete da pesca: +1 CIBO");
            g_riep_tipo[g_riep_nrighe++] = 1;
        }
        if (ha_carta(19)) {
            g_cura(1);
            g_log_linea("L'Amaca ti rigenera: +1 PV.", 1);
            strcpy(g_riep_righe[g_riep_nrighe], "Amaca: +1 PV");
            g_riep_tipo[g_riep_nrighe++] = 1;
        }
        if (ha_carta(25) && ciboConsumato) {
            g_cura(1);
            g_log_linea("La Cuciniera ti sfama a dovere: +1 PV.", 1);
            strcpy(g_riep_righe[g_riep_nrighe], "Cuciniera: +1 PV");
            g_riep_tipo[g_riep_nrighe++] = 1;
        }
        if (G.nmano > 0) {
            int i;
            snprintf(m, sizeof(m), "Scarti le %d carte rimaste in mano.", G.nmano);
            g_log_linea(m, 0);
            snprintf(m, sizeof(m), "Scarti %d carte in mano", G.nmano);
            strcpy(g_riep_righe[g_riep_nrighe], m);
            g_riep_tipo[g_riep_nrighe++] = 0;
            for (i = 0; i < G.nmano; i++) {
                if (G.nscarto < MAX_SCARTO) G.scarto[G.nscarto++] = G.mano[i];
            }
            G.nmano = 0;
        }
    }
    if (G.pv <= 0) { g_fine_partita(false); return; }
    if (G.giorno >= GIORNI_VITTORIA) { g_fine_partita(true); return; }
    g_riepilogo = true;
    G.modale = true;
    g_riep_pv = G.pv;
}

void g_riepilogo_avanti(void) {
    g_riepilogo = false;
    G.modale = false;
    G.bloccoFine = false;
    g_nuovo_giorno();
}

/* ---- Autoverifica (equivalente di index.html?test, senza DOM) ---- */

void g_test_automatici(int *ok, int *fail) {
    int okn = 0, failn = 0;
    int i, g0;
    g_autotest = true;

#define T(cond, nome) do { \
        if (cond) { okn++; } else { failn++; } \
        { char t2[LOG_LEN]; snprintf(t2, sizeof(t2), "[TEST] %s", nome); g_log_linea(t2, (cond) ? 1 : 2); } \
    } while (0)

#define AUTOLOOP() do { \
        int _g = 0; \
        while (_g < 12) { \
            _g++; \
            if (G.fine) break; \
            if (g_pending_choice) { g_scelta_rispondi(0); continue; } \
            if (g_riepilogo) { g_riepilogo_avanti(); continue; } \
            break; \
        } \
    } while (0)

    /* 1) una giornata completa */
    {
        int tenta = 0, az = -1, idA = -1, idS = -1;
        do {
            g_nuova_partita();
            while (g_pending_choice) g_scelta_rispondi(0);
            for (i = 0; i < G.nmano; i++) {
                int id = G.mano[i];
                if (carta_tipo(id) == TIPO_AZIONE && id != 28 && id != 30 && idA < 0) idA = i;
                if (carta_tipo(id) != TIPO_AZIONE && idS < 0) idS = i;
            }
            tenta++;
        } while (tenta < 20 && (idA < 0 || idS < 0));
        T(G.giorno == 1, "il gioco parte al giorno 1");
        T(G.nmano >= 5, "la mano ha 5 o piu carte");
        T(idA >= 0, "la mano contiene almeno una carta AZIONE");
        T(idS >= 0, "la mano contiene almeno uno STRUMENTO/PERSONA");
        if (idA >= 0) {
            int idA_id = G.mano[idA];
            int inMano = 0, inScarto = 0;
            az = G.azioni;
            g_gioca_carta(idA_id);
            T(G.azioni == az - 1, "giocare un'azione consuma 1 azione");
            for (i = 0; i < G.nmano; i++) if (G.mano[i] == idA_id) inMano = 1;
            for (i = 0; i < G.nscarto; i++) if (G.scarto[i] == idA_id) inScarto = 1;
            T(inScarto && !inMano, "l'azione finisce nello scarto");
        }
        if (idS >= 0) {
            int idS_id = G.mano[idS];
            int inAcc = 0;
            g_gioca_carta(idS_id);
            for (i = 0; i < G.nacc; i++) if (G.acc[i] == idS_id) inAcc = 1;
            T(inAcc, "lo strumento/persona va nell'accampamento");
        }
        g0 = G.giorno;
        g_fine_giornata();
        AUTOLOOP();
        T(G.fine ? G.giorno == 21 : G.giorno == g0 + 1,
          "concludere la giornata avanza al giorno successivo");
    }

    /* 2) risorse bloccate a 9 */
    G.risorse[R_CIPO] = 8;
    g_r_add(R_CIPO, 5);
    T(G.risorse[R_CIPO] == 9, "la risorsa si blocca a 9");
    g_r_add(R_CIPO, 1);
    T(G.risorse[R_CIPO] == 9, "la risorsa non supera mai 9");

    /* 3) mazzo finito: rimescolamento dello scarto */
    {
        int lenMano = G.nmano;
        G.nmazzo = 0;
        G.nscarto = 6;
        for (i = 0; i < 6; i++) G.scarto[i] = i + 2;
        g_pesca_carte(6);
        T(G.nmano == lenMano + 6 && G.nmazzo == 0 && G.nscarto == 0,
          "con lo scarto si rimescola e si pesca");
        lenMano = G.nmano;
        g_pesca_carte(3);
        T(G.nmano == lenMano, "mazzo e scarto vuoti: nessuna pesca");
    }

    /* 4) vittoria al giorno 21 */
    g_nuova_partita();
    while (g_pending_choice) g_scelta_rispondi(0);
    G.giorno = GIORNI_VITTORIA;
    G.pv = 5;
    for (i = 0; i < R_COUNT; i++) G.risorse[i] = 9;
    G.nmano = 0; G.nacc = 0; G.nscarto = 0; G.nmazzo = 0;
    G.bloccoFine = false;
    g_fine_giornata();
    T(G.fine && G.vittoria, "vittoria a fine giorno 21");

    /* 5) sconfitta a PV 0 */
    g_nuova_partita();
    while (g_pending_choice) g_scelta_rispondi(0);
    G.pv = 1;
    g_pv_danno(1, "verifica", true);
    T(G.fine && !G.vittoria, "sconfitta quando i PV arrivano a 0");

    g_autotest = false;
    if (ok) *ok = okn;
    if (fail) *fail = failn;
}
