/* ============================================================
   Salvataggio su microSD (libfat): file accanto al .nds
   - lisola_save.dat  : partita in corso
   - lisola_top5.dat  : classifica Top 5
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include "game.h"
#include "save.h"

/*
   I file vengono creati con percorsi relativi: dopo fatInitDefault il
   processo lavora gia nella cartella della scheda (root "fat:/" oppure
   la cartella in cui si trova il .nds).
*/
#define SAVE_MAGIC   "LISLSV01"
#define SAVE_FILENAME "lisola_save.dat"
#define TOP5_FILENAME "lisola_top5.dat"

typedef struct {
    char magic[8];
    int giorno;
    int pv;
    int risorse[R_COUNT];
    int nmazzo; int mazzo[MAX_MAZZO];
    int nscarto; int scarto[MAX_SCARTO];
    int nmano; int mano[MAX_MANO];
    int nacc; int acc[MAX_ACC];
    int azioni;
    int bonusAzioniOggi;
    int domaniCarte;
    int pentolaUsata;
    int ultimoEvento;
    int negativiDiFila;
    int strumentiGiocati;
    int nobb; int obiettivi[MAX_OBIETTIVI];
} SaveData;

static bool fat_ok = false;

bool save_init(void) {
    fat_ok = fatInitDefault();
    g_carica_top5();
    return fat_ok;
}

bool g_salva(void) {
    SaveData d;
    FILE *f;
    if (G.fine || G.modale || G.giorno == 0) return false;
    memset(&d, 0, sizeof(d));
    memcpy(d.magic, SAVE_MAGIC, 8);
    d.giorno = G.giorno;
    d.pv = G.pv;
    memcpy(d.risorse, G.risorse, sizeof(d.risorse));
    d.nmazzo = G.nmazzo; memcpy(d.mazzo, G.mazzo, sizeof(d.mazzo));
    d.nscarto = G.nscarto; memcpy(d.scarto, G.scarto, sizeof(d.scarto));
    d.nmano = G.nmano; memcpy(d.mano, G.mano, sizeof(d.mano));
    d.nacc = G.nacc; memcpy(d.acc, G.acc, sizeof(d.acc));
    d.azioni = G.azioni;
    d.bonusAzioniOggi = G.bonusAzioniOggi;
    d.domaniCarte = G.domaniCarte;
    d.pentolaUsata = G.pentolaUsata ? 1 : 0;
    d.ultimoEvento = G.ultimoEvento;
    d.negativiDiFila = G.negativiDiFila;
    d.strumentiGiocati = G.strumentiGiocati;
    d.nobb = G.nobb; memcpy(d.obiettivi, G.obiettivi, sizeof(d.obiettivi));
    if (!fat_ok) return false;
    f = fopen(SAVE_FILENAME, "wb");
    if (!f) return false;
    fwrite(&d, sizeof(d), 1, f);
    fclose(f);
    return true;
}

static bool dati_validi(const SaveData *d) {
    return memcmp(d->magic, SAVE_MAGIC, 8) == 0
        && d->giorno >= 1 && d->giorno <= GIORNI_VITTORIA
        && d->pv >= 0 && d->pv <= PV_MAX
        && d->nmazzo >= 0 && d->nmazzo <= MAX_MAZZO
        && d->nscarto >= 0 && d->nscarto <= MAX_SCARTO
        && d->nmano >= 0 && d->nmano <= MAX_MANO
        && d->nacc >= 0 && d->nacc <= MAX_ACC
        && d->nobb >= 0 && d->nobb <= MAX_OBIETTIVI;
}

bool g_carica(void) {
    SaveData d;
    FILE *f;
    if (!fat_ok) return false;
    f = fopen(SAVE_FILENAME, "rb");
    if (!f) return false;
    if (fread(&d, sizeof(d), 1, f) != 1) { fclose(f); return false; }
    fclose(f);
    if (!dati_validi(&d)) return false;
    memset(&G, 0, sizeof(G));
    G.giorno = d.giorno;
    G.pv = d.pv;
    memcpy(G.risorse, d.risorse, sizeof(G.risorse));
    G.nmazzo = d.nmazzo; memcpy(G.mazzo, d.mazzo, sizeof(G.mazzo));
    G.nscarto = d.nscarto; memcpy(G.scarto, d.scarto, sizeof(G.scarto));
    G.nmano = d.nmano; memcpy(G.mano, d.mano, sizeof(G.mano));
    G.nacc = d.nacc; memcpy(G.acc, d.acc, sizeof(G.acc));
    G.azioni = d.azioni;
    G.bonusAzioniOggi = d.bonusAzioniOggi;
    G.domaniCarte = d.domaniCarte;
    G.pentolaUsata = d.pentolaUsata != 0;
    G.ultimoEvento = d.ultimoEvento;
    G.negativiDiFila = d.negativiDiFila;
    G.strumentiGiocati = d.strumentiGiocati;
    G.nobb = d.nobb; memcpy(G.obiettivi, d.obiettivi, sizeof(G.obiettivi));
    g_log_linea("Hai ripreso la partita dal giorno precedente.", 3);
    return true;
}

bool g_salva_esiste(void) {
    FILE *f;
    if (!fat_ok) return false;
    f = fopen(SAVE_FILENAME, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void g_salva_elimina(void) {
    if (!fat_ok) return;
    remove(SAVE_FILENAME);
}

/* ---- Top 5 ---- */

void g_carica_top5(void) {
    FILE *f;
    int p, g, v;
    g_top5_n = 0;
    if (!fat_ok) return;
    f = fopen(TOP5_FILENAME, "r");
    if (!f) return;
    while (g_top5_n < 5 && fscanf(f, "%d %d %d", &p, &g, &v) == 3) {
        g_top5[g_top5_n][0] = p;
        g_top5[g_top5_n][1] = g;
        g_top5[g_top5_n][2] = v;
        g_top5_n++;
    }
    fclose(f);
}

void g_salva_top5(void) {
    FILE *f;
    int i;
    if (!fat_ok) return;
    f = fopen(TOP5_FILENAME, "w");
    if (!f) return;
    for (i = 0; i < g_top5_n; i++)
        fprintf(f, "%d %d %d\n", g_top5[i][0], g_top5[i][1], g_top5[i][2]);
    fclose(f);
}

/* Ritorna la posizione in classifica (0-4) o -1 se fuori dalla top 5 */
int g_registra_record(int punti, int giorni, int pv) {
    int pos = -1, i;
    g_carica_top5();
    for (i = 0; i < 5; i++) {
        if (g_top5[i][0] < punti) break;
    }
    if (i < 5) {
        int j;
        for (j = 4; j > i; j--) {
            g_top5[j][0] = g_top5[j - 1][0];
            g_top5[j][1] = g_top5[j - 1][1];
            g_top5[j][2] = g_top5[j - 1][2];
        }
        g_top5[i][0] = punti;
        g_top5[i][1] = giorni;
        g_top5[i][2] = pv;
        if (g_top5_n < 5) g_top5_n++;
        pos = i;
    }
    g_salva_top5();
    return pos;
}
