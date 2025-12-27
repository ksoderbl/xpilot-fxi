/* -*-C-*-
 *
 * Project :	 TRACE
 *
 * File    :	 socklib.h
 *
 * Description
 *
 * Copyright (C) 1991 by Arne Helme, The TRACE project
 *
 * Rights to use this source is granted for all non-commercial and research
 * uses. Creation of derivate forms of this software may be subject to
 * restriction. Please obtain written permission from the author.
 *
 * This software is provided "as is" without any express or implied warranty.
 *
 * RCS:     
 *
 * Revision 1.1.1.1  1992/05/11  12:32:34  bjoerns
 * XPilot v1.0
 *
 * Revision 1.2  91/10/02  08:38:20  08:38:20  arne (Arne Helme)
 * "ANSI C prototypes added."
 *
 * Revision 1.1  91/10/02  08:34:53  08:34:53  arne (Arne Helme)
 * Initial revision
 *
 */

#ifndef _SOCKLIB_INCLUDED
#define _SOCKLIB_INCLUDED

/* Error values and their meanings */
#define SL_ESOCKET		0	/* socket system call error */
#define SL_EBIND		1	/* bind system call error */
#define SL_ELISTEN		2	/* listen system call error */
#define SL_EHOSTNAME		3	/* Invalid host name format */
#define SL_ECONNECT		5	/* connect system call error */
#define SL_ESHUTD		6	/* shutdown system call error */
#define SL_ECLOSE		7	/* close system call error */
#define SL_EWRONGHOST		8	/* message arrived from unspec. host */
#define SL_ENORESP		9	/* No response */
#define SL_ERECEIVE		10	/* Receive error */
#define SL_ETIMEOUT		11	/* Timeout */
#define SL_EADDR		12	/* Invalid dotted decimal address */

extern int32_t sl_errno, sl_timeout_s, sl_timeout_us, sl_default_retries,
		sl_broadcast_enabled;

extern void SetTimeout(int32_t, int32_t);
extern int32_t CreateServerSocket(int32_t);
extern int32_t GetPortNum(int32_t);
extern char *GetSockAddr(int32_t);
extern int32_t GetRemoteHostName(int32_t, char *, int32_t);
extern int32_t CreateClientSocket(char *, int32_t);
extern int32_t CreateClientSocketNonBlocking(char *, int32_t);
extern int32_t SocketAccept(int32_t);
extern int32_t SocketLinger(int32_t);
extern int32_t SetSocketReceiveBufferSize(int32_t, int32_t);
extern int32_t SetSocketSendBufferSize(int32_t, int32_t);
extern int32_t SetSocketNoDelay(int32_t, int32_t);
extern int32_t SetSocketNonBlocking(int32_t, int32_t);
extern int32_t SetSocketBroadcast(int32_t, int32_t);
extern int32_t GetSocketError(int32_t);
extern int32_t SocketReadable(int32_t);
extern int32_t SocketRead(int32_t, char *, int32_t);
extern int32_t SocketWrite(int32_t, char *, int32_t);
extern int32_t SocketClose(int32_t);
extern int32_t CreateDgramSocket(int32_t);
extern int32_t CreateDgramAddrSocket(char *, int32_t);
extern int32_t DgramBind(int32_t fd, char *dotaddr, int32_t port);
extern int32_t DgramConnect(int32_t, char *, int32_t);
extern int32_t DgramSend(int32_t, char *, int32_t, char *, int32_t);
extern int32_t DgramReceiveAny(int32_t, char *, int32_t);
extern int32_t DgramReceive(int32_t, char *, char *, int32_t);
extern int32_t DgramReply(int32_t, char *, int32_t);
extern int32_t DgramRead(int32_t fd, char *rbuf, int32_t size);
extern int32_t DgramWrite(int32_t fd, char *wbuf, int32_t size);
extern int32_t DgramSendRec(int32_t, char *, int32_t, char *, int32_t, char *, int32_t);
extern char *DgramLastaddr(void);
extern char *DgramLastname(void);
extern int32_t DgramLastport(void);
extern void DgramClose(int32_t);
extern uint32_t GetInetAddr(char *name);
extern void GetLocalHostName(char *name, uint32_t size,
		int32_t search_domain_for_xpilot);
extern char *GetAddrByName(const char *name);
extern int32_t GetNameByAddr(const char *addr, char *name, int32_t size);

#if !defined(select) && defined(__linux__)
#define select(N, R, W, E, T)	select((N),		\
	(fd_set*)(R), (fd_set*)(W), (fd_set*)(E), (T))
#endif

#endif /* _SOCKLIB_INCLUDED */
