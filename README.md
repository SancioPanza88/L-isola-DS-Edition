# L'ISOLA — Sopravvivenza (NDS Edition)

Porting **homebrew per Nintendo DS** del gioco di carte web [L'Isola](https://github.com/SancioPanza88/L-Isola): sopravvivi 21 giorni su un'isola deserta e al giorno 22 arriva la nave di soccorso.

Scritto in **C con libnds** (niente emulatori di JS): logica identica alla versione web (30 carte, 12 eventi, obiettivi, punteggio, Top 5), grafica dai PNG originali convertita a 16 bit, salvataggio automatico sulla microSD.

## Scaricare il .nds

Il file `LISOLA.nds` viene compilato automaticamente da **GitHub Actions**:

1. Apri il repo su GitHub → tab **Actions**.
2. Clicca sull'ultimo workflow *"Build L'ISOLA NDS"* riuscito.
3. In fondo alla pagina scarica l'artifact **LISOLA.nds**.

## Giocare sull'hardware

- **DS / DS Lite / DSi con flashcart (R4 e simili)**: copia `LISOLA.nds` nella microSD e avvialo dal kernel (YSMenu, Wood, ecc.).
- **3DS / DSi moddati**: avvia il file con **TWiLight Menu**.
- **Su PC**: apri `LISOLA.nds` con **melonDS** o **DeSmuME** (touch simulato col mouse).

## Comandi

| Input | Azione |
|---|---|
| **Touch (schermo inferiore)** | Tocca una carta per giocarla, i pulsanti in basso per Pentola/Concludi giornata, le frecce per scorrere la mano |
| **A** | Conferma (avanti / prima scelta) |
| **B** | Pentola di coccio / seconda scelta / torna al menu |
| **START** | Torna al menu (salva) |

## Salvataggio

La partita viene salvata automaticamente su `lisola_save.dat` (stessa cartella del .nds, serve una flashcart con DLDI). La classifica Top 5 sta in `lisola_top5.dat`. Il pulsante **CONTINUA** riprende la partita.

## Autoverifica

All'avvio il gioco esegue l'autotest (stesso set di verifiche di `index.html?test`, senza DOM): il risultato è mostrato a schermo nella schermata titolo (es. `Autotest: 17 OK 0 FAIL`). Se compare un FAIL, apri un issue.

## Compilare da solo

Serve devkitPro (devkitARM + libnds + libfat). Nella cartella del progetto:

```sh
make clean
make
```

La build ufficiale gira in GitHub Actions (container `devkitpro/devkitarm`). Gli asset (`source/assets.h`, `source/font_8x8.h`) sono già generati e committati; per rigenerarli: `python tools/convert_assets.py` e `python tools/make_font.py` (serve Pillow).

## Struttura

```
L-isola-DS-Edition/
├── .github/workflows/build.yml   build automatica -> artifact LISOLA.nds
├── Makefile                      template ufficiale devkitARM
├── icon.bmp                      icona 32x32 per ndstool (generata)
├── art/                          PNG originali del gioco web
├── source/
│   ├── main.c                    init libnds, due schermi bitmap, loop
│   ├── game.c / game.h           porting della logica (carte, eventi, regole)
│   ├── ui.c                      rendering 2 schermi + touch
│   ├── save.c                    salvataggio/record su microSD (libfat)
│   ├── assets.h                  PNG convertiti in RGB555 (generato)
│   └── font_8x8.h                font bitmap 8x8 (generato)
└── tools/                        script Python di conversione asset/font
```
