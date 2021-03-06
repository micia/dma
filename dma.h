/*
 * Copyright (c) 2008 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Simon 'corecode' Schubert <corecode@fs.ei.tum.de> and
 * Matthias Schmidt <matthias@dragonflybsd.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DMA_H
#define DMA_H

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <netdb.h>
#include <stddef.h> /*size_t*/

#define VERSION	"DragonFly Mail Agent " DMA_VERSION

#define BUF_SIZE	2048
#define ESMTPBUF_SIZE   8192
#define ERRMSG_SIZE	200
#define USERNAME_SIZE	50
#define MIN_RETRY	300		/* 5 minutes */
#define MAX_RETRY	(3*60*60)	/* retry at least every 3 hours */
#define MAX_TIMEOUT	(5*24*60*60)	/* give up after 5 days */
#define SLEEP_TIMEOUT	30		/* check for queue flush every 30 seconds */
#ifndef PATH_MAX
#define PATH_MAX	1024		/* Max path len */
#endif
#define	SMTP_PORT	25		/* Default SMTP port */
#define CON_TIMEOUT	(5*60)		/* Connection timeout per RFC5321 */

#define VERBOSE         0x0001          /* Enable debug logging output to LOG_DEBUG */
#define STARTTLS        0x0002		/* StartTLS support required by the user*/
#define NOHELO          0x0004		/* Don't fallback to HELO if EHLO isn't supported*/
#define SECURETRANS     0x0008		/* SSL/TLS in general */
#define DEFER           0x0020		/* Defer mails */
#define INSECURE        0x0040		/* Allow plain login w/o encryption */
#define FULLBOUNCE      0x0080		/* Bounce the full message */
#define TLS_OPP         0x0100		/* Opportunistic STARTTLS */

#ifndef CONF_PATH
#error Please define CONF_PATH
#endif

#ifndef LIBEXEC_PATH
#error Please define LIBEXEC_PATH
#endif

#define SPOOL_FLUSHFILE	"flush"

#define DMA_ROOT_USER	"mail"
#define DMA_GROUP	"mail"

#ifndef MBOX_STRICT
#define MBOX_STRICT	0
#endif


struct stritem {
	SLIST_ENTRY(stritem) next;
	char *str;
};
SLIST_HEAD(strlist, stritem);

struct alias {
	LIST_ENTRY(alias) next;
	char *alias;
	struct strlist dests;
};
LIST_HEAD(aliases, alias);

struct qitem {
	LIST_ENTRY(qitem) next;
	const char *sender;
	char *addr;
	char *queuefn;
	char *mailfn;
	char *queueid;
	FILE *queuef;
	FILE *mailf;
	int remote;
};
LIST_HEAD(queueh, qitem);

struct queue {
	struct queueh queue;
	char *id;
	FILE *mailf;
	char *tmpf;
	const char *sender;
};

struct config {
	const char *smarthost;
	unsigned int port; /* should be unsigned for maximum compatibility (16 bit ints) */
	const char *aliases;
	const char *spooldir;
	const char *authpath;
	const char *certfile;
	int features;
	const char *mailname;
	const char *masquerade_host;
	const char *masquerade_user;
};

#define USESSL          0x0001          /* Use SSL for communication */
#define USESTARTTLS     0x0002          /* Has performed STARTTLS */
#define HASSTARTTLS     0x0004          /* STARTTLS advertised by the remote host */
#define AUTHPLAIN       0x0008          /* PLAIN authentication method support */
#define AUTHLOGIN       0x0010          /* LOGIN authentication method support */
#define AUTHCRAMMD5     0x0020          /* CRAM MD5 authentication method support */
#define ESMTPMASK       (HASSTARTTLS | AUTHPLAIN | AUTHLOGIN | AUTHCRAMMD5)

struct connection {
	int fd;
	SSL *ssl;
	int flags;
};


struct authuser {
	SLIST_ENTRY(authuser) next;
	char *login;
	char *password;
	char *host;
};
SLIST_HEAD(authusers, authuser);


struct mx_hostentry {
	char		host[MAXDNAME];
	char		addr[INET6_ADDRSTRLEN];
	int		pref;
	struct addrinfo	ai;
	struct sockaddr_storage	sa;
};


/* global variables */
extern struct aliases aliases;
extern struct config config;
extern struct strlist tmpfs;
extern struct authusers authusers;
extern char username[USERNAME_SIZE];
extern uid_t useruid;
extern const char *logident_base;

extern char neterr[ERRMSG_SIZE];
extern char errmsg[ERRMSG_SIZE];

/* aliases_parse.y */
int yyparse(void);
extern FILE *yyin;

/* conf.c */
void trim_line(char *);
void parse_conf(const char *);
void parse_authfile(const char *);

/* crypto.c */
void hmac_md5(unsigned char *, int, unsigned char *, int, unsigned char *);
int smtp_auth_md5(struct connection *, char *, char *) __attribute__((__nonnull__(1, 2, 3)));
int smtp_init_crypto(struct connection *) __attribute__((__nonnull__(1)));

/* dns.c */
int dns_get_mx_list(const char *, unsigned int, struct mx_hostentry **, int);

/* net.c */
char *ssl_errstr(void);
int read_remote(struct connection *, size_t *, char *) __attribute__((__nonnull__(1)));
ssize_t send_remote_command(struct connection *, const char*, ...)  __attribute__((__nonnull__(1, 2), __format__ (__printf__, 2, 3)));
int deliver_remote(struct qitem *);

/* base64.c */
int base64_encode(const void *, int, char **);
int base64_decode(const char *, void *);

/* dma.c */
#define EXPAND_ADDR	1
#define EXPAND_WILDCARD	2
int add_recp(struct queue *, const char *, int);
void run_queue(struct queue *);

/* spool.c */
int newspoolf(struct queue *);
int linkspool(struct queue *);
int load_queue(struct queue *);
void delqueue(struct qitem *);
int acquirespool(struct qitem *);
void dropspool(struct queue *, struct qitem *);
int flushqueue_since(unsigned int);
int flushqueue_signal(void);

/* local.c */
int deliver_local(struct qitem *);

/* mail.c */
void bounce(struct qitem *, const char *);
int readmail(struct queue *, int, int);

/* util.c */
const char *hostname(void);
void setlogident(const char *, ...) __attribute__((__format__ (__printf__, 1, 2)));
void errlog(int, const char *, ...) __attribute__((__format__ (__printf__, 2, 3)));
void errlogx(int, const char *, ...) __attribute__((__format__ (__printf__, 2, 3)));
void set_username(void);
void deltmp(void);
int do_timeout(int, int);
int open_locked(const char *, int, ...);
char *rfc822date(void);
int strprefixcmp(const char *, const char *);
void init_random(void);

#endif
