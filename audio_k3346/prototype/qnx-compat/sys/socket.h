/* Audio uses only shutdown; all network setup uses stock AirPlay exports. */
#ifndef MHI2_SOCKET_COMPAT_H
#define MHI2_SOCKET_COMPAT_H
#define SHUT_RDWR 2
int shutdown(int, int);
#endif
