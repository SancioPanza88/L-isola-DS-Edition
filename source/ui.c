/* ============================================================
   Interfaccia NDS: schermo superiore (stato) e inferiore (touch)
   Rendering a richiesta (nessun frame loop): si ridisegna solo
   quando serve. Colori e font definiti qui sotto.
   ============================================================ */

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "assets.h"
#include "font_8x8.h"
#include "music_loop.h"
#include "ui.h"

/* ---- Colori (RGB15 ufficiale di libnds, con bit 15 = opacita').
   Pannelli medio-chiari: le immagini scure risultano sempre visibili. ---- */
#define C_BG      (BIT(15) | RGB15(4,8,13))     /* blu oceano scuro */
#define C_PANEL   (BIT(15) | RGB15(6,11,18))
#define C_PANEL2  (BIT(15) | RGB15(5,9,14))
#define C_BORDO   (BIT(15) | RGB15(14,22,30))
#define C_TESTO   (BIT(15) | RGB15(31,30,25))   /* pergamena */
#define C_SOFT    (BIT(15) | RGB15(20,24,27))
#define C_BENE    (BIT(15) | RGB15(20,30,19))
#define C_DANNO   (BIT(15) | RGB15(31,20,16))
#define C_ACCENTO (BIT(15) | RGB15(31,26,12))
#define C_DIM     (BIT(15) | RGB15(13,15,17))
#define C_CUORE   (BIT(15) | RGB15(30,10,10))

/* ---- Stato UI ---- */
static u16 *mainbuf;
static u16 *subbuf;
static int schermo = SCHERMO_TITOLO;
static int mano_win = 0;
static int test_ok = 0, test_fail = 0;

/* ---- Suono: musica in loop + click (API libnds, vedi nds/arm9/sound.h) ---- */
static int mus_id = -1;

static void musica_start(void) {
    if (mus_id < 0)
        mus_id = soundPlaySample(music_loop, SoundFormat_16Bit,
                                 sizeof(music_loop), MUSIC_RATE, 70, 64, true, 0);
}

static void musica_stop(void) {
    if (mus_id >= 0) { soundKill(mus_id); mus_id = -1; }
}

static void sfx_clic(void) {
    soundPlaySample(sfx_click, SoundFormat_16Bit,
                    sizeof(sfx_click), MUSIC_RATE, 100, 64, false, 0);
}

/* ---- Regole (schermata titolo e regole) ---- */
static const char *REGOLE_TESTO =
    "1. Sopravvivi 21 giorni: al 22 la nave arriva e vinci. PV a 0 = sconfitta.\n"
    "2. Ogni mattina un EVENTO, buono o cattivo.\n"
    "3. Peschi 5 CARTE (6 con lo Zaino grande): 1 carta = 1 azione, ne hai 3.\n"
    "4. AZIONE = effetto subito. STRUMENTO/PERSONA restano nell'accampamento.\n"
    "5. A sera consumi 1 CIBO e 1 ACQUA: se manca, -1 PV.\n"
    "6. Risorse max 9. Le ERBE curano Malattia e Insetti.\n"
    "7. Capanna e Fal\xF2 proteggono da Tempesta e Freddo; il Cane dal Ladro.\n"
    "8. Rete: +1 Cibo a sera. Amaca: +1 PV. Cuciniera: +1 PV se hai mangiato.\n"
    "9. Le carte in mano a sera finiscono nello scarto, poi tornano nel mazzo.\n"
    "10. Il gioco salva da solo: CONTINUA riprende la partita.";

/* ---- Funzioni di disegno ----
   Line-height: ogni riga di testo occupa LINE_H px (glifo 8 px + 2 di aria),
   cosi' le righe non si toccano mai. Tutti i panelli passano un rettangolo
   di clip: il testo non esce mai dal proprio riquadro. */

#define LINE_GSH 8  /* altezza del glifo */
#define LINE_H   10 /* step verticale tra le righe di testo */

static void fill_screen(u16 *buf, u16 col) {
    int x, y;
    for (y = 0; y < 192; y++)
        for (x = 0; x < 256; x++)
            buf[y * 256 + x] = col;
}

static void fill_rect(int x, int y, int w, int h, u16 col, u16 *buf) {
    int xx, yy;
    for (yy = 0; yy < h; yy++) {
        if (y + yy < 0 || y + yy >= 192) continue;
        for (xx = 0; xx < w; xx++) {
            if (x + xx < 0 || x + xx >= 256) continue;
            buf[(y + yy) * 256 + x + xx] = col;
        }
    }
}

static void rect_bordo(int x, int y, int w, int h, u16 bordo, u16 riemp, u16 *buf) {
    fill_rect(x, y, w, h, bordo, buf);
    fill_rect(x + 1, y + 1, w - 2, h - 2, riemp, buf);
}

static void img_blit(int x, int y, int idx, u16 *buf) {
    int w = asset[idx].w, h = asset[idx].h;
    const unsigned short *src = asset[idx].px;
    int yy;
    if (x < 0 || y < 0 || x + w > 256 || y + h > 192) return;
    for (yy = 0; yy < h; yy++)
        memcpy(&buf[(y + yy) * 256 + x], &src[yy * w], (size_t)w * 2);
}

/* testo: niente clip = intero schermo. '\n' = nuova riga a LINE_H. */
static void testo_clip(int x, int y, const char *s, u16 col, u16 *buf,
                       int cx, int cy, int cw, int ch);
static int testo_wrap_clip(int x, int y, int maxw, const char *s, u16 col,
                           int maxlines, u16 *buf, int cx, int cy, int cw, int ch);

static void testo(int x, int y, const char *s, u16 col, u16 *buf) {
    testo_clip(x, y, s, col, buf, 0, 0, 256, 192);
}

static void testo_clip(int x, int y, const char *s, u16 col, u16 *buf,
                       int cx, int cy, int cw, int ch) {
    int x0 = x, y0 = y;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        int i;
        if (c == '\n') { x0 = x; y0 += LINE_H; continue; }
        if (y0 + LINE_GSH > cy + ch) return; /* niente piu' righe dentro il clip */
        if (x0 < 0 || y0 < 0 || y0 + LINE_GSH > 192) { x0 += LINE_GSH; continue; }
        for (i = 0; i < LINE_GSH; i++) {
            unsigned char riga = font_8x8[c][i];
            int b;
            for (b = 0; b < LINE_GSH; b++) {
                int px = x0 + b, py = y0 + i;
                if (!(riga & (0x80 >> b))) continue;
                if (px < cx || px >= cx + cw || py < cy || py >= cy + ch) continue;
                buf[py * 256 + px] = col;
            }
        }
        x0 += LINE_GSH;
    }
}

/* testo a capo PER PAROLE INTERE: una parola non viene mai spezzata a meta'.
   Se una singola parola e' piu' larga della riga, staziona su una riga sola e
   il clip laterale la tiene comunque dentro il pannello. */
static int testo_wrap_clip(int x, int y, int maxw, const char *s, u16 col,
                           int maxlines, u16 *buf, int cx, int cy, int cw, int ch) {
    char riga[64];
    const char *p = s;
    int nrighe = 0;

    riga[0] = '\0';
    while (*p && nrighe < maxlines) {
        if (*p == '\n') {
            if (riga[0]) {
                if (y + (nrighe + 1) * LINE_H > cy + ch) break;
                testo_clip(x, y + nrighe * LINE_H, riga, col, buf, cx, cy, cw, ch);
                nrighe++;
                riga[0] = '\0';
            }
            p++;
            continue;
        }
        if (*p == ' ') { p++; continue; } /* spazi multipli = uno solo */

        {
            int wlen = 0;
            while (p[wlen] && p[wlen] != ' ' && p[wlen] != '\n' && wlen < 60) wlen++;
            {
                int curv = (int)strlen(riga);
                if (curv > 0 && (curv + 1 + wlen) * LINE_GSH > maxw) {
                    if (y + (nrighe + 1) * LINE_H > cy + ch) break;
                    testo_clip(x, y + nrighe * LINE_H, riga, col, buf, cx, cy, cw, ch);
                    nrighe++;
                    riga[0] = '\0';
                    curv = 0;
                }
                if (curv > 0) { riga[curv] = ' '; riga[curv + 1] = '\0'; }
                memcpy(riga + strlen(riga), p, (size_t)wlen);
                riga[strlen(riga) + wlen] = '\0';
            }
            p += wlen;
        }
    }
    if (riga[0] && nrighe < maxlines &&
        y + (nrighe + 1) * LINE_H <= cy + ch) {
        testo_clip(x, y + nrighe * LINE_H, riga, col, buf, cx, cy, cw, ch);
        nrighe++;
    }
    return nrighe;
}

static void tronca(char *out, size_t outn, const char *s, int maxch) {
    int len = 0;
    while (*s && len < maxch) { out[len++] = *s++; }
    if (*s && len + 2 <= (int)outn - 1) { out[len++] = '.'; out[len++] = '.'; }
    out[len] = '\0';
}

/* ---- Bottoni: disegno + hit test ---- */

typedef struct {
    int x, y, w, h;
} Btn;

static bool in_btn(const Btn *b, int x, int y) {
    return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}

static void draw_btn(const Btn *b, const char *label, u16 bord, u16 testo_col, u16 *buf) {
    char t[24];
    rect_bordo(b->x, b->y, b->w, b->h, bord, C_PANEL, buf);
    tronca(t, sizeof(t), label, (b->w - 4) / 8);
    testo(b->x + (b->w - (int)strlen(t) * 8) / 2, b->y + (b->h - 8) / 2, t, testo_col, buf);
}

/* ---- Cuori PV (5x4, due file da 10) ---- */
static void cuori(int x, int y, u16 *buf) {
    static const unsigned char PAT[4] = { 0x0E, 0x1F, 0x0F, 0x06 };
    int i, yy;
    for (yy = 0; yy < 4; yy++) {
        for (i = 0; i < 5; i++) {
            if (PAT[yy] & (0x10 >> i)) buf[(y + yy) * 256 + x + i] = C_CUORE;
        }
    }
}

/* ============================================================
   Schermo SUPERIORE: bacheca di gioco
   ============================================================ */

static void draw_top_board(void) {
    char m[40];
    int i;

    fill_screen(mainbuf, C_BG);

    /* intestazione */
    fill_rect(0, 0, 256, 13, C_PANEL2, mainbuf);
    testo(2, 2, "L'ISOLA", C_ACCENTO, mainbuf);
    if (G.giorno > 0) {
        snprintf(m, sizeof(m), "AZIONI %d", G.azioni);
        testo(62, 2, m, G.azioni > 0 ? C_TESTO : C_DANNO, mainbuf);
        snprintf(m, sizeof(m), "GIORNO %d/%d", G.giorno, GIORNI_VITTORIA);
        testo(256 - (int)strlen(m) * 8 - 2, 2, m, C_TESTO, mainbuf);
    } else {
        testo(256 - 10 * 8 - 2, 2, "L'ISOLA DS", C_SOFT, mainbuf);
    }

    /* PV + cuori */
    snprintf(m, sizeof(m), "PV %2d/%d", G.pv, PV_MAX);
    testo(2, 16, m, C_TESTO, mainbuf);
    for (i = 0; i < PV_MAX; i++) {
        int hx = 46 + (i % 10) * 7;
        int hy = 16 + (i / 10) * 6;
        if (i >= G.pv) {
            fill_rect(hx, hy, 5, 4, C_DIM, mainbuf);
        } else {
            cuori(hx, hy, mainbuf);
        }
    }

    /* risorse (26..42): ogni voce ha icona e contatore nello stesso riquadro */
    for (i = 0; i < R_COUNT; i++) {
        int x = 4 + i * 64;
        char t[40];
        img_blit(x, 27, ASSET_ICON_R1 + i, mainbuf);
        snprintf(t, sizeof(t), "%.4s%d", risorsa_nome(i), G.risorse[i]);
        testo(x + 18, 28, t, G.risorse[i] == 0 ? C_DANNO : C_TESTO, mainbuf);
    }

    /* pannello EVENTO (45..103) */
    rect_bordo(2, 45, 252, 58, C_BORDO, C_PANEL, mainbuf);
    testo(4, 47, "EVENTO", C_ACCENTO, mainbuf);
    if (G.giorno > 0 && g_evento_id >= 1 && g_evento_id <= 12) {
        img_blit(4, 57, ASSET_ICON_E1 + (g_evento_id - 1), mainbuf);
        testo_wrap_clip(30, 56, 200, evento_testo(g_evento_id), C_TESTO, 5,
                        mainbuf, 30, 45, 222, 58);
    }

    /* accampamento (104..162): carte intere, mai sovrapposte.
       Al massimo 4 carte affiancate + indicatore di altre. */
    rect_bordo(2, 104, 252, 58, C_BORDO, C_PANEL, mainbuf);
    testo(4, 106, "ACCAMPAMENTO", C_ACCENTO, mainbuf);
    if (G.nacc == 0) {
        testo(4, 116, "Nessun attivo. Gioca STRUMENTI o", C_SOFT, mainbuf);
        testo(4, 126, "PERSONE per metterli qui.", C_SOFT, mainbuf);
    } else {
        int mostrate = G.nacc > 4 ? 4 : G.nacc;
        for (i = 0; i < mostrate; i++) {
            img_blit(2 + i * 46, 114, ASSET_CARD_1 + (G.acc[i] - 1), mainbuf);
        }
        if (G.nacc > 4) {
            snprintf(m, sizeof(m), "+%d", G.nacc - 4);
            testo(240 - (int)strlen(m) * LINE_GSH, 122, m, C_SOFT, mainbuf);
        }
    }

    /* diario (158..191): testo a capo e sempre dentro il pannello */
    rect_bordo(2, 158, 252, 34, C_BORDO, C_PANEL, mainbuf);
    testo(4, 160, "DIARIO", C_ACCENTO, mainbuf);
    {
        int vis = g_nlog > 2 ? 2 : g_nlog;
        for (i = 0; i < vis; i++) {
            int idx = g_nlog - vis + i;
            u16 col = C_TESTO;
            char t[40];
            if (g_log[idx].tipo == 1) col = C_BENE;
            else if (g_log[idx].tipo == 2) col = C_DANNO;
            else if (g_log[idx].tipo == 3) col = C_ACCENTO;
            else if (g_log[idx].tipo == 4) col = C_SOFT;
            tronca(t, sizeof(t), g_log[idx].testo, 31);
            testo_wrap_clip(4, 170 + i * LINE_H, 244, t, col, 1,
                            mainbuf, 2, 158, 252, 33);
        }
    }
}

/* ---- Schermo SUPERIORE a titolo: regole ---- */

static void draw_top_title(void) {
    fill_screen(mainbuf, C_BG);
    testo(4, 4, "L'ISOLA - SOPRAVVIVENZA", C_ACCENTO, mainbuf);
    testo_wrap_clip(4, 20, 248, REGOLE_TESTO, C_TESTO, 17,
                    mainbuf, 4, 18, 248, 172);
}

/* ============================================================
   Schermo INFERIORE
   ============================================================ */

/* nome carta su 2 righe da max 7 caratteri: taglio per parole intere */
static void card_nome_righe(const char *nome, char *r1, char *r2) {
    int i = 0, n = 0;
    r1[0] = '\0';
    r2[0] = '\0';
    while (nome[i]) {
        int wlen = 0;
        while (nome[i + wlen] && nome[i + wlen] != ' ' && wlen < 12) wlen++;
        int next = i + wlen;
        if (nome[next] == ' ') next++;
        if (wlen == 0) { i = next; continue; }
        if (n > 0 && (n + 1 + wlen) > 7) break; /* prima riga piena */
        if (n > 0) r1[n++] = ' ';
        memcpy(r1 + n, nome + i, (size_t)wlen);
        n += wlen;
        i = next;
        if (n >= 7) break;
    }
    r1[n] = '\0';
    if (nome[i]) {
        int n2 = 0;
        while (nome[i] && n2 < 7) r2[n2++] = nome[i++];
        if (nome[i]) r2[n2 - 1] = '.';
        r2[n2] = '\0';
    }
}

static void draw_card(int x, int y, int id) {
    char t[24];
    rect_bordo(x, y, 62, 80, C_BORDO, C_PANEL, subbuf);
    /* le tre aree della carta sono indipendenti e clippate al rettangolo */
    /* 1) NOME: 2 righe, parola per parola, ellissi se troppo lungo */
    {
        const char *nome = carta_nome(id);
        char r1[16], r2[16];
        card_nome_righe(nome, r1, r2);
        testo_clip(x + 3, y + 2, r1, C_TESTO, subbuf, x + 1, y + 1, 60, 19);
        if (r2[0]) testo_clip(x + 3, y + 12, r2, C_TESTO, subbuf, x + 1, y + 1, 60, 19);
    }
    /* 2) ART: immagine della carta (PNG convertito) nella propria banda */
    img_blit(x + 9, y + 20, ASSET_CARD_1 + (id - 1), subbuf);
    /* 3) EFFETTO/BONUS: 1 riga, clippata alla carta */
    tronca(t, sizeof(t), carta_effetto(id), 6);
    testo_clip(x + 3, y + 71, t, C_SOFT, subbuf, x + 1, y + 65, 60, 14);
}
    /* 2) ART: icona centrata sulla carta (banda y+20..y+64) */
    img_blit(x + 9, y + 20, ASSET_CARD_1 + (id - 1), subbuf);
    /* 3) EFFETTO: 1 riga sotto l'arte, clippata alla carta */
    tronca(t, sizeof(t), carta_effetto(id), 6);
    testo_clip(x + 3, y + 71, t, C_SOFT, subbuf, x + 1, y + 65, 60, 14);
}

static void draw_bottom_gioca(void) {
    int i, vis;

    fill_screen(subbuf, C_BG);

    if (G.nmano == 0) {
        testo(60, 88, "La tua mano \xE8 vuota.", C_SOFT, subbuf);
    } else {
        vis = G.nmano - mano_win;
        if (vis > 6) vis = 6;
        for (i = 0; i < vis; i++) {
            int col = i % 3, riga = i / 3;
            draw_card(4 + col * 66, 2 + riga * 82, G.mano[mano_win + i]);
        }
        if (G.nmano > 6) {
            /* frecce di scorrimento */
            rect_bordo(208, 30, 32, 12, C_BORDO, C_PANEL, subbuf);
            testo(220, 31, "^", C_TESTO, subbuf);
            rect_bordo(208, 120, 32, 12, C_BORDO, C_PANEL, subbuf);
            testo(220, 121, "v", C_TESTO, subbuf);
        }
    }

    /* barra comandi */
    if (ha_carta(15)) {
        Btn b;
        bool ok = !G.pentolaUsata && G.risorse[R_LEGNA] >= 1 && !G.fine && !G.modale;
        b.x = 4; b.y = 166; b.w = 76; b.h = 24;
        draw_btn(&b, "PENTOLA", ok ? C_ACCENTO : C_DIM, ok ? C_TESTO : C_DIM, subbuf);
    }
    {
        Btn b;
        bool ok = !G.fine && !G.modale && !G.bloccoFine;
        if (ha_carta(15)) { b.x = 84; b.w = 168; }
        else { b.x = 4; b.w = 248; }
        b.y = 166; b.h = 24;
        draw_btn(&b, "CONCLUDI GIORNATA", ok ? C_BENE : C_DIM, ok ? C_TESTO : C_DIM, subbuf);
    }
}

static void draw_bottom_scelta(void) {
    fill_screen(subbuf, C_BG);
    rect_bordo(8, 8, 240, 175, C_BORDO, C_PANEL, subbuf);
    testo_wrap_clip(12, 12, 224, g_scelta.titolo, C_ACCENTO, 2, subbuf, 8, 8, 240, 175);
    testo_wrap_clip(12, 30, 224, g_scelta.testo, C_TESTO, 9, subbuf, 8, 28, 240, 93);
    {
        Btn b1, b2;
        b1.x = 16; b1.y = 124; b1.w = 224; b1.h = 24;
        b2.x = 16; b2.y = 152; b2.w = 224; b2.h = 24;
        draw_btn(&b1, g_scelta.op1, C_ACCENTO, C_TESTO, subbuf);
        draw_btn(&b2, g_scelta.op2, C_DANNO, C_TESTO, subbuf);
    }
    testo(4, 184, "A = prima scelta   B = seconda", C_SOFT, subbuf);
}

static void draw_bottom_riepilogo(void) {
    char m[48];
    int i;
    fill_screen(subbuf, C_BG);
    rect_bordo(8, 8, 240, 175, C_BORDO, C_PANEL, subbuf);
    snprintf(m, sizeof(m), "GIORNO %d - RIEPILOGO", G.giorno);
    testo(12, 12, m, C_ACCENTO, subbuf);
    for (i = 0; i < g_riep_nrighe && i < 11; i++) {
        u16 col = g_riep_tipo[i] == 1 ? C_BENE : (g_riep_tipo[i] == 2 ? C_DANNO : C_TESTO);
        testo_clip(12, 24 + i * LINE_H, g_riep_righe[i], col, subbuf, 8, 24, 240, 126);
    }
    snprintf(m, sizeof(m), "PVita: %d / %d", g_riep_pv, PV_MAX);
    testo_clip(12, 24 + g_riep_nrighe * LINE_H + 2, m, C_TESTO, subbuf, 8, 24, 240, 126);
    {
        Btn b;
        b.x = 60; b.y = 154; b.w = 136; b.h = 24;
        draw_btn(&b, "AVANTI", C_BENE, C_TESTO, subbuf);
    }
    testo(4, 184, "A = avanti", C_SOFT, subbuf);
}

static void draw_bottom_finale(void) {
    char m[48];
    int i, y;
    const int x = 12;

    fill_screen(subbuf, C_BG);
    /* intestazione */
    if (G.vittoria) {
        testo(x, 8, "VITTORIA!", C_BENE, subbuf);
        testo(x, 18, "La nave \xE8 arrivata al giorno 22!", C_TESTO, subbuf);
    } else {
        testo(x, 8, "SCONFITTA", C_DANNO, subbuf);
        testo(x, 18, "I PV sono a 0: l'isola vince.", C_TESTO, subbuf);
    }

    snprintf(m, sizeof(m), "Giorni: %d / %d", G.giorno, GIORNI_VITTORIA);
    testo(x, 31, m, C_TESTO, subbuf);
    snprintf(m, sizeof(m), "Cibo/Acqua/Legna/Erbe: %d %d %d %d",
             G.risorse[R_CIPO], G.risorse[R_ACQUA], G.risorse[R_LEGNA], G.risorse[R_ERBE]);
    testo(x, 41, m, C_TESTO, subbuf);
    snprintf(m, sizeof(m), "Accampamento: %d attivi", G.nacc);
    testo(x, 51, m, C_TESTO, subbuf);

    /* obiettivi (max 3 righe) */
    y = 62;
    for (i = 0; i < G.nobb; i++) {
        int fatto = g_obiettivo_fatto(G.obiettivi[i]) ? 1 : 0;
        snprintf(m, sizeof(m), "[%c] %s", fatto ? 'X' : ' ', g_obiettivo_testo(G.obiettivi[i]));
        tronca(m, sizeof(m), m, 29);
        testo_clip(x, y + i * LINE_H, m, fatto ? C_BENE : C_SOFT, subbuf, 8, 62, 240, 30);
    }

    snprintf(m, sizeof(m), "PUNTEGGIO: %d", g_punteggio_ultimo);
    testo(x, 94, m, C_ACCENTO, subbuf);
    if (g_finale_pos >= 0) testo(x, 104, "In TOP 5!", C_BENE, subbuf);

    testo(x, 114, "TOP 5:", C_ACCENTO, subbuf);
    y = 124;
    for (i = 0; i < g_top5_n && i < 4; i++) {
        snprintf(m, sizeof(m), "%d) %d pt - %d gg - %d PV",
                 i + 1, g_top5[i][0], g_top5[i][1], g_top5[i][2]);
        testo_clip(x, y + i * LINE_H, m, C_TESTO, subbuf, 8, 124, 240, 40);
    }

    {
        Btn b;
        b.x = 44; b.y = 164; b.w = 168; b.h = 24;
        draw_btn(&b, "GIOCA ANCORA", C_ACCENTO, C_TESTO, subbuf);
    }
}

static void draw_bottom_titolo(void) {
    char m[48];
    Btn b;
    img_blit(0, 0, ASSET_SFONDO, subbuf);
    img_blit(80, 8, ASSET_LOGO, subbuf);
    testo(100, 62, "L'ISOLA", C_ACCENTO, subbuf);
    testo(76, 72, "SOPRAVVIVENZA", C_TESTO, subbuf);

    b.x = 28; b.y = 96; b.w = 200; b.h = 26;
    draw_btn(&b, "NUOVA PARTITA", C_ACCENTO, C_TESTO, subbuf);
    if (g_salva_esiste()) {
        b.y = 126;
        draw_btn(&b, "CONTINUA", C_BENE, C_TESTO, subbuf);
    }
    b.y = 156;
    draw_btn(&b, "REGOLE", C_BORDO, C_TESTO, subbuf);

    snprintf(m, sizeof(m), "Autotest: %d OK %d FAIL", test_ok, test_fail);
    testo(4, 184, m, test_fail == 0 ? C_BENE : C_DANNO, subbuf);
}

static void draw_bottom_regole(void) {
    fill_screen(subbuf, C_BG);
    testo(12, 8, "REGOLE", C_ACCENTO, subbuf);
    testo_wrap_clip(12, 20, 232, REGOLE_TESTO, C_TESTO, 16,
                    subbuf, 12, 20, 232, 160);
    testo(4, 184, "B: torna al menu", C_SOFT, subbuf);
}

static void draw_top(void) {
    if (schermo == SCHERMO_TITOLO || schermo == SCHERMO_REGOLE) draw_top_title();
    else draw_top_board();
}

static void draw_bottom(void) {
    switch (schermo) {
        case SCHERMO_TITOLO: draw_bottom_titolo(); break;
        case SCHERMO_GIOCA: draw_bottom_gioca(); break;
        case SCHERMO_SCELTA: draw_bottom_scelta(); break;
        case SCHERMO_RIEPILOGO: draw_bottom_riepilogo(); break;
        case SCHERMO_FINALE: draw_bottom_finale(); break;
        case SCHERMO_REGOLE: draw_bottom_regole(); break;
    }
}

/* ============================================================
   Azioni di gioco e transizioni di schermo
   ============================================================ */

static void eval_state(void) {
    if (G.fine) schermo = SCHERMO_FINALE;
    else if (g_pending_choice) schermo = SCHERMO_SCELTA;
    else if (g_riepilogo) schermo = SCHERMO_RIEPILOGO;
    else schermo = SCHERMO_GIOCA;
}

static void nuova_partita_ui(void) {
    g_salva_elimina();
    g_nuova_partita();
    mano_win = 0;
    eval_state();
}

static void concludi_ui(void) {
    if (G.fine || G.modale || G.bloccoFine) return;
    g_fine_giornata();
    eval_state();
    g_salva();
}

static void gioca_card_tap(int idx) {
    if (G.fine || G.modale) return;
    if (idx < 0 || idx >= G.nmano) return;
    if (G.azioni <= 0) {
        g_log_linea("Niente azioni: puoi concludere la giornata.", 0);
        return;
    }
    if (g_gioca_carta(G.mano[idx])) {
        g_salva();
        if (G.fine) eval_state();
    }
}

static void tap(int x, int y) {
    Btn b;

    switch (schermo) {
        case SCHERMO_TITOLO:
            b.x = 28; b.y = 96; b.w = 200; b.h = 26;
            if (in_btn(&b, x, y)) { nuova_partita_ui(); return; }
            if (g_salva_esiste()) {
                b.y = 126;
                if (in_btn(&b, x, y)) {
                    if (g_carica()) {
                        mano_win = 0;
                        eval_state();
                    }
                    return;
                }
            }
            b.y = 156;
            if (in_btn(&b, x, y)) schermo = SCHERMO_REGOLE;
            return;

        case SCHERMO_GIOCA:
            if (G.fine || G.modale) return;
            /* scorrimento mano */
            if (G.nmano > 6) {
                b.x = 208; b.y = 30; b.w = 32; b.h = 12;
                if (in_btn(&b, x, y)) { if (mano_win > 0) mano_win -= 3; return; }
                b.y = 120;
                if (in_btn(&b, x, y)) {
                    int maxw = G.nmano - 6;
                    if (mano_win < maxw) mano_win += 3;
                    return;
                }
            }
            /* bottoni */
            if (ha_carta(15)) {
                b.x = 4; b.y = 166; b.w = 76; b.h = 24;
                if (in_btn(&b, x, y)) { g_usa_pentola(); g_salva(); return; }
                b.x = 84; b.w = 168;
            } else {
                b.x = 4; b.w = 248;
            }
            b.y = 166; b.h = 24;
            if (in_btn(&b, x, y)) { concludi_ui(); return; }
            /* carta */
            if (x >= 4 && x < 202 && y >= 2 && y < 164) {
                int col = (x - 4) / 66;
                int riga = (y - 2) / 82;
                int idx = mano_win + col + riga * 3;
                gioca_card_tap(idx);
            }
            return;

        case SCHERMO_SCELTA:
            b.x = 16; b.y = 124; b.w = 224; b.h = 24;
            if (in_btn(&b, x, y)) {
                g_scelta_rispondi(0);
                g_salva();
                eval_state();
                return;
            }
            b.y = 152;
            if (in_btn(&b, x, y)) {
                g_scelta_rispondi(1);
                g_salva();
                eval_state();
            }
            return;

        case SCHERMO_RIEPILOGO:
            b.x = 60; b.y = 154; b.w = 136; b.h = 24;
            if (in_btn(&b, x, y)) {
                g_riepilogo_avanti();
                g_salva();
                eval_state();
            }
            return;

        case SCHERMO_FINALE:
            b.x = 44; b.y = 164; b.w = 168; b.h = 24;
            if (in_btn(&b, x, y)) nuova_partita_ui();
            return;

        case SCHERMO_REGOLE:
            schermo = SCHERMO_TITOLO;
            return;
    }
}

/* tasti di comodo (A/B/START) */

static void tasto_a(void) {
    switch (schermo) {
        case SCHERMO_TITOLO: nuova_partita_ui(); break;
        case SCHERMO_GIOCA: concludi_ui(); break;
        case SCHERMO_SCELTA:
            g_scelta_rispondi(0);
            g_salva();
            eval_state();
            break;
        case SCHERMO_RIEPILOGO:
            g_riepilogo_avanti();
            g_salva();
            eval_state();
            break;
        case SCHERMO_FINALE: nuova_partita_ui(); break;
        case SCHERMO_REGOLE: schermo = SCHERMO_TITOLO; break;
    }
}

static void tasto_b(void) {
    switch (schermo) {
        case SCHERMO_TITOLO:
            if (g_salva_esiste() && g_carica()) {
                mano_win = 0;
                eval_state();
            }
            break;
        case SCHERMO_GIOCA:
            if (ha_carta(15) && !G.fine && !G.modale) { g_usa_pentola(); g_salva(); }
            break;
        case SCHERMO_SCELTA:
            g_scelta_rispondi(1);
            g_salva();
            eval_state();
            break;
        case SCHERMO_REGOLE: schermo = SCHERMO_TITOLO; break;
        case SCHERMO_FINALE: break;
        case SCHERMO_RIEPILOGO: break;
    }
}

static void tasto_start(void) {
    if (schermo != SCHERMO_TITOLO) {
        g_salva();
        schermo = SCHERMO_TITOLO;
    }
}

/* ============================================================
   Init e loop
   ============================================================ */

void ui_init(int bg_top, int bg_sub, int ok, int fail) {
    mainbuf = (u16 *)bgGetGfxPtr(bg_top);
    subbuf = (u16 *)bgGetGfxPtr(bg_sub);
    test_ok = ok;
    test_fail = fail;
    musica_start();
    draw_top();
    draw_bottom();
}

void ui_loop(void) {
    static bool seeded = false;
    static int ui_frames = 0;
    bool redraw = false;
    u32 down;

    ui_frames++;
    scanKeys();
    down = keysDown();

    /* al primo input si mescola il seme del RNG (varieta tra le partite) */
    if (down && !seeded) {
        seeded = true;
        rng_seed((unsigned int)ui_frames * 0x9E3779B9u + (down << 7));
    }

    if (down & KEY_TOUCH) {
        touchPosition tp;
        touchRead(&tp);
        tap(tp.px, tp.py);
        redraw = true;
    }
    if (down & KEY_A) { tasto_a(); redraw = true; }
    if (down & KEY_B) { tasto_b(); redraw = true; }
    if (down & KEY_START) { tasto_start(); redraw = true; }

    if (down) sfx_clic();

    if (redraw) {
        if (schermo == SCHERMO_TITOLO) musica_start(); else musica_stop();
        draw_top();
        draw_bottom();
    }
}
