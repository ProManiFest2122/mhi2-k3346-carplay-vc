#ifndef K2161_MSGTABLE_HOOK_H
#define K2161_MSGTABLE_HOOK_H

int k2161_msgtable_try_install_base(void *driver_base);
int k2161_msgtable_ready(void);
/* K3346 Etap 6: grupa 0x41 (0x415A/0x4170/0x4171); wymaga gotowej tabeli RGI */
int k3346_msgtable_tel_try_install_base(void *driver_base);
int k3346_msgtable_tel_ready(void);

#endif
