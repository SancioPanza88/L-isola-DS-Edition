// Shims per le funzioni BlocksDS usate dalla fork dswifi (non presenti
// nel libnds9.a classico di devkitPro, su cui si linka il gioco).

#include <nds.h>

void cothread_yield_irq(uint32_t flag)
{
	// il gioco non usa cothread: attendi l'IRQ specificata a livello basso
	(void)flag;
	swiWaitForVBlank();
}

void libndsCrash(const char *msg)
{
	(void)msg;
	while (1) {
		swiWaitForVBlank();
	}
}