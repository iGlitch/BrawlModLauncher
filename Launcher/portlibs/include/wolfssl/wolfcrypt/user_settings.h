#ifndef _WII_USER_SETTINGS_H_
#define _WII_USER_SETTINGS_H_

#define DEVKITPRO
#define WC_NO_HARDEN
#define NO_OLD_SSL_NAMES
#define WOLFSSL_SMALL_STACK
#define SINGLE_THREADED

/* Networking / I/O */
#define WOLFSSL_USER_IO
#define WOLFSSL_NO_SOCKETS   /* <-- important: don't include sys/socket.h */
#define NO_WRITEV            /* <-- important: don't include sys/uio.h */
#define HAVE_SNI
#define HAVE_ECC
#define HAVE_SUPPORTED_CURVES
#define HAVE_TLS_EXTENSIONS      /* SNI needs TLS extensions; HAVE_SNI alone may be insufficient */
#define WOLFSSL_TLS12            /* keep handshake to TLS 1.2 client method (what we call) */
#define NO_DEV_RANDOM
#endif /* _WII_USER_SETTINGS_H_ */