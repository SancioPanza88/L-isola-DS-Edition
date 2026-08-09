/* ============================================================
   Multiplayer locale Wi-Fi (2 console, senza access point):
   API "local multiplayer" di DSWifi (libdswifi9). Flusso:
   HOST: Wifi_InitDefault(INIT_ONLY) ->
         Wifi_MultiplayerHostMode() -> canale + beacon + attesa ->
         Wifi_MultiplayerAllowNewClients(false) -> collegato.
   CLIENT: init -> Wifi_MultiplayerClientMode() -> Wifi_ScanMode()
         -> trova il beacon (game_id) -> Wifi_ConnectOpenAP()
         -> Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED.
   Ogni console invia la propria situazione (giorno, PV, punteggio,
   esito) e riceve quella dell'avversario. Gli handler girano in
   IRQ: niente printf/malloc li', solo copie in buffer volatili.
   ============================================================ */

#include <nds.h>
#include <dswifi9.h>
#include <string.h>
#include "multi.h"
#include "game.h"

#define MP_CHANNEL   7
#define MP_GAME_ID   0x4C495331u   /* "LIS1" */
#define MP_SSID      "LISOLA1"

typedef struct {
    u8  cmd;        /* 0 = stato, 1 = via! (host -> client) */
    s8  giorno;
    s8  pv;
    u8  esito;      /* 0 in gioco, 1 perso, 2 vinto */
    u8  pad;
    u16 punteggio;
} MpStatusPkt;                     /* 8 byte */

#define PKT_CMD  ((int)sizeof(MpStatusPkt))

static int  g_stato   = MP_OFF;
static int  g_fase    = 0;
static bool g_is_host = false;
static bool g_linked  = false;
static bool g_go_sent = false;     /* host ha gia' mandato il start */

static volatile MpStatusPkt g_rx;
static volatile bool g_rx_ready = false;
static volatile bool g_go_recv = false; /* client ha ricevuto il start */

static MpAvversario g_av;

/* ---------------- handler IRQ HOST (da client) ---------------- */
static void client_pkt(Wifi_MPPacketType type, int aid, int base, int len) {
    MpStatusPkt t;
    (void)aid;
    if (type != WIFI_MPTYPE_REPLY) return;
    if (len != PKT_CMD) return;
    Wifi_RxRawReadPacket((u32)base, (u32)len, &t);
    g_rx = t;
    g_rx_ready = true;
}

/* ---------------- handler IRQ CLIENT (da host) ---------------- */
static void host_pkt(Wifi_MPPacketType type, int base, int len) {
    MpStatusPkt t;
    if (type != WIFI_MPTYPE_CMD && type != WIFI_MPTYPE_DATA) return;
    if (len != PKT_CMD) return;
    Wifi_RxRawReadPacket((u32)base, (u32)len, &t);
    if (t.cmd == 1) { g_go_recv = true; return; }
    g_rx = t;
    g_rx_ready = true;
}

static void aggiorna_avversario(const MpStatusPkt *p) {
    g_av.giorno    = p->giorno;
    g_av.pv        = p->pv;
    g_av.esito     = p->esito;
    g_av.punteggio = p->punteggio;
    g_av.pronto    = true;
}

/* ==================== API pubblica ==================== */

void mp_start_host(void) {
    g_is_host = true;
    g_stato   = MP_HOST_WAIT;
    g_fase    = 0;
    g_linked  = false;
    g_go_sent = false;
    g_av.pronto = false;
    g_rx_ready  = false;
    if (!Wifi_CheckInit()) Wifi_InitDefault(INIT_ONLY);
    Wifi_MultiplayerFromClientSetPacketHandler(client_pkt);
}

void mp_start_client(void) {
    g_is_host = false;
    g_stato   = MP_CLIENT_SEARCH;
    g_fase    = 0;
    g_linked  = false;
    g_av.pronto = false;
    g_rx_ready  = false;
    g_go_recv   = false;
    if (!Wifi_CheckInit()) Wifi_InitDefault(INIT_ONLY);
    Wifi_MultiplayerFromHostSetPacketHandler(host_pkt);
}

void mp_stop(void) {
    g_stato  = MP_OFF;
    g_linked = false;
}

int  mp_stato(void)   { return g_stato; }
bool mp_is_host(void) { return g_is_host; }
bool mp_linked(void)  { return g_linked; }

const MpAvversario *mp_avversario(void) { return &g_av; }

/* ------------------------ invio stato ------------------------ */
static void invia_pkt(u8 cmd, u8 esito) {
    MpStatusPkt p;

    if (!g_linked) return;

    p.cmd       = cmd;
    p.giorno    = (s8)(G.giorno > 127 ? 127 : G.giorno);
    p.pv        = (s8)(G.pv > 127 ? 127 : G.pv);
    p.esito     = esito;
    p.pad       = 0;
    p.punteggio = (u16)(G.fine ? g_punteggio_ultimo
                               : g_calcola_punteggio(NULL));

    if (g_is_host)
        Wifi_MultiplayerHostCmdTxFrame(&p, (size_t)PKT_CMD);
    else
        Wifi_MultiplayerClientReplyTxFrame(&p, (size_t)PKT_CMD);
}

void mp_send_status(void) {
    u8 esito = MP_ESITO_GIOCO;
    if (!g_linked) return;
    if (G.fine) esito = G.vittoria ? MP_ESITO_VINTO : MP_ESITO_PERSO;
    if (g_is_host && !g_go_sent) {
        invia_pkt(1, esito);   /* start! (in arrivo anche il REPLY) */
        g_go_sent = true;
    } else {
        invia_pkt(0, esito);
    }
}

/* --------------------- esito finale vs ---------------------- */
/* -1 = non deciso / pari, 0 = perso, 1 = vinto */
int mp_verdetto(bool mia_vittoria) {
    int mine, other;

    if (!g_av.pronto || g_av.esito == MP_ESITO_GIOCO) return -1;
    mine  = mia_vittoria ? MP_ESITO_VINTO : MP_ESITO_PERSO;
    other = g_av.esito;

    if (mine != other) return (mine == MP_ESITO_VINTO) ? 1 : 0;
    if (g_av.punteggio == g_punteggio_ultimo) return -1;
    return (g_punteggio_ultimo > g_av.punteggio) ? 1 : 0;
}

/* ------------------- avanzamento (ogni frame) ------------------- */
/* solo operazioni non bloccanti: la connessione procede a passi,
   cosi' la UI resta fluida mentre si aspetta il secondo giocatore. */
void mp_poll(void) {
    static int conto = 0;
    int st, n, i;

    if (g_stato == MP_OFF) return;

    if (g_stato == MP_LINKED) {
        static int conto_inv = 0;
        if (g_rx_ready) {
            MpStatusPkt t = g_rx;
            g_rx_ready = false;
            aggiorna_avversario(&t);
        }
        if (g_linked && --conto_inv <= 0) {
            conto_inv = 15;   /* ~4 aggiornamenti al secondo */
            mp_send_status();
        }
        return;
    }

    /* ------------- HOST ------------- */
    if (g_is_host) {
        switch (g_fase) {
            case 0:
                if (Wifi_LibraryModeReady() &&
                    Wifi_MultiplayerHostMode(1, (size_t)PKT_CMD,
                                             (size_t)PKT_CMD) == 0)
                    g_fase = 1;
                break;
            case 1:
                if (!Wifi_LibraryModeReady()) break;
                Wifi_SetChannel(MP_CHANNEL);
                Wifi_MultiplayerAllowNewClients(true);
                Wifi_BeaconStart(MP_SSID, MP_GAME_ID);
                g_fase = 2;
                break;
            case 2:
                if (Wifi_MultiplayerGetNumClients() > 0) {
                    Wifi_MultiplayerAllowNewClients(false);
                    g_linked = true;
                    g_stato  = MP_LINKED;
                    g_fase   = 3;
                }
                break;
        }
        return;
    }

    /* ------------- CLIENT ------------- */
    switch (g_fase) {
        case 0:
            if (Wifi_LibraryModeReady() &&
                Wifi_MultiplayerClientMode((size_t)PKT_CMD) == 0)
                g_fase = 1;
            break;
        case 1:
            if (Wifi_LibraryModeReady()) {
                Wifi_ScanMode();
                g_fase = 2;
                conto = 0;
            }
            break;
        case 2: /* scansiona a intervalli: cerca il host */
            if (++conto < 8) break;
            conto = 0;
            n = Wifi_GetNumAP();
            for (i = 0; i < n; i++) {
                Wifi_AccessPoint ap;
                if (Wifi_GetAPData(i, &ap) != 0) continue;
                /* beacon del nostro gioco, e host che accetta */
                if (ap.nintendo.game_id == MP_GAME_ID &&
                    ap.nintendo.allows_connections) {
                    if (Wifi_ConnectOpenAP(&ap) == 0) {
                        g_stato = MP_CLIENT_CONNECT;
                        g_fase  = 3;
                    }
                    break;
                }
            }
            break;
        case 3: /* attesa associazione */
            st = Wifi_AssocStatus();
            if (st == ASSOCSTATUS_ASSOCIATED) {
                g_stato = MP_LINKED;
                g_fase  = 4;
            } else if (st == ASSOCSTATUS_CANNOTCONNECT) {
                g_stato = MP_CLIENT_SEARCH;
                g_fase  = 2;   /* riprova la scansione */
            }
            break;
        case 4: /* associati: il gioco parte solo col GO del host */
            if (Wifi_AssocStatus() != ASSOCSTATUS_ASSOCIATED) {
                g_stato  = MP_OFF;   /* host sparito: si torna al menù */
                g_fase   = 0;
                g_linked = false;
                break;
            }
            if (g_go_recv) g_linked = true;
            break;
        default: break;
    }
}