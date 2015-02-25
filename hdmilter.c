/*# define SMFI_VERSION   2*/

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <libmilter/mfapi.h>
#include "config.h"
#include "throttle.h"

/*
 * http://www.postfix.org/MILTER_README.html
 * http://www.postfix.org/postconf.5.html
 */
struct mlfiPriv {
	char *authname;
	char *authtype;
	char *clientaddr;
	char *qid;
	throttle_t *thro;
};

#define VERTEXT "HD-Filter-Version"
#define VERSION "0.1"
#define LOGFLAG LOG_DAEMON|LOG_INFO

#define MLFIPRIV ((struct mlfiPriv *) smfi_getpriv(ctx))

static sfsistat mlfi_cleanup(SMFICTX *, sfsistat);

static sfsistat mlfi_connect(SMFICTX *ctx, char *host, _SOCK_ADDR *hostaddr) {
	struct mlfiPriv *priv;
	char *clientaddr;

	priv = (struct mlfiPriv *) calloc(1, sizeof *priv);
	if (!priv) {
		/* can't accept this message right now */
		syslog(LOGFLAG, "connect from %s: oom", host);
		/*return SMFIS_TEMPFAIL;*/
		abort();
	}
	priv->thro = throttle_init();
	if (!priv->thro) {
		syslog(LOGFLAG, "init throttle for %s: oom", host);
		abort();
	}
	/* save the private data */
	smfi_setpriv(ctx, priv);

	/* check remote address */
	clientaddr = smfi_getsymval(ctx, "client_addr");
	priv->clientaddr = (clientaddr) ? strdup(clientaddr):strdup("");
	syslog(LOGFLAG, "[%p]connect from %s", priv, priv->clientaddr);
	if (!throttle_test_address(priv->thro, priv->clientaddr)) {
		syslog(LOGFLAG, "[%p]rejected addr:%s", priv, priv->clientaddr);
		return SMFIS_REJECT;
	}

	return SMFIS_CONTINUE;
}

/*static sfsistat mlfi_helo(SMFICTX *ctx, char*helohost)*/

static sfsistat mlfi_envfrom(SMFICTX *ctx, char **envfrom) {
	struct mlfiPriv *priv;
	char *authtype, *authname;

	priv = MLFIPRIV;
	if (!priv) {
		syslog(LOGFLAG, "WARNING:envfrom without private data");
		return SMFIS_CONTINUE;
	}
	authtype = smfi_getsymval(ctx, "auth_type");
	authname = smfi_getsymval(ctx, "auth_authen");
	if (priv->authtype && authtype && !strcmp(priv->authtype,authtype)) {
		// nothing todo
	} else {
		if (priv->authtype)
			free(priv->authtype);
		priv->authtype = (authtype)?strdup(authtype):strdup("");
	}
	if (priv->authname && authname && !strcmp(priv->authname,authname)) {
		// nothing todo
	} else {
		if (priv->authname)
			free(priv->authname);
		priv->authname = (authname)?strdup(authname):strdup("");
	}
	
	syslog(LOGFLAG,"[%p]client %s:%s:%s", priv, priv->clientaddr,
		priv->authtype,priv->authname);
	return SMFIS_CONTINUE;
}

static int domain_copy(char *dst, int dlen, char *src)
{
	int off = 0;
	while (off+1<dlen && *(src+off)) {
		int c = *(src+off);
		if (c=='.' || c=='@' || isalpha(c) || isdigit(c)) {
			dst[off++] = c;
		} else {
			break;
		}
	}
	dst[off++] = '\0';
	return off;
}

static sfsistat mlfi_envrcpt(SMFICTX *ctx, char **envrcpt) {
#define DOMAINLEN 128
   char **env;
   int nrcpt;
   char domain[DOMAINLEN];
   struct mlfiPriv *priv = MLFIPRIV;

if (!priv) {
	syslog(LOGFLAG, "WARNING:envrcpt without private data");
	return SMFIS_CONTINUE;
}
 	nrcpt = 0;
	env = envrcpt;
	while (*env) {
		char *rcpt;
		char *domainptr;
		
		rcpt = *env;
		env++;
		domainptr = strstr(rcpt, "@");
		if (!domainptr) {
			continue;
		}
		syslog(LOGFLAG, "[%p]rcpt:%s", priv, rcpt);
		domain_copy(domain, DOMAINLEN, domainptr);
		if (strstr(CONF_LOCAL_DOMAIN, domain)) {
			continue;
		}
		nrcpt++;
	}
   
   /*syslog(LOGFLAG, "num of rcpt:%d", nrcpt);*/
   if (nrcpt && !throttle_test_user(priv->thro, priv->authname, nrcpt)) {
   	syslog(LOGFLAG,"[%p]discarded user:%s", priv, priv->authname);
	return SMFIS_DISCARD;
   }
   return SMFIS_CONTINUE;
}

static sfsistat mlfi_data(SMFICTX *ctx) {
	struct mlfiPriv *priv = MLFIPRIV;
	char *qid;

	if (!priv) {
		syslog(LOGFLAG, "WARNING:data without private data");
		return SMFIS_CONTINUE;
	}

	qid = smfi_getsymval(ctx, "i");
	if (priv->qid)
		free(priv->qid);
	priv->qid = (qid)?strdup(qid):strdup("");
	syslog(LOGFLAG,"[%p]qid %s", priv, priv->qid);
	return SMFIS_CONTINUE;
}

/*static sfsistat mlfi_header(SMFICTX *ctx, char *headerf, char *headerv)*/

/*sfsistat mlfi_eoh(SMFICTX *ctx) */

/*sfsistat mlfi_body(SMFICTX *ctx, u_char *bodyp, size_t bodylen)*/

sfsistat mlfi_eom(SMFICTX *ctx) {
	/*struct mlfiPriv *priv = MLFIPRIV;*/
	return SMFIS_CONTINUE;
}

sfsistat mlfi_close(SMFICTX *ctx) {
	return mlfi_cleanup(ctx, SMFIS_ACCEPT);
}

sfsistat mlfi_abort(SMFICTX *ctx) {
	return mlfi_cleanup(ctx, SMFIS_CONTINUE);
}

sfsistat mlfi_cleanup(SMFICTX *ctx, sfsistat rc) {
	struct mlfiPriv *priv = MLFIPRIV;
	if (priv) {
		syslog(LOGFLAG, "[%p]cleanup", priv);
		if (priv->thro) {
			throttle_destroy(priv->thro);
		}
		if (priv->clientaddr)
			free(priv->clientaddr);
		if (priv->authtype)
			free(priv->authtype);
		if (priv->authname)
			free(priv->authname);
		if (priv->qid)
			free(priv->qid);
		free(priv);
		smfi_setpriv(ctx, NULL);
	}

	return(rc);
}

struct smfiDesc smfilter = {
	"HDFilter",	/* filter name */
	SMFI_VERSION,	/* version code -- do not change */
	SMFIF_NONE,	/* flags */
	mlfi_connect,	/* connection info filter */
	NULL,		/* SMTP HELO command filter */
	mlfi_envfrom,	/* envelope sender filter */
	mlfi_envrcpt,	/* envelope recipient filter */
	NULL,	/* header filter */
	NULL,	/* end of header */
	NULL,	/* body block filter */
	mlfi_eom,	/* end of message */
	mlfi_abort,	/* message aborted */
	mlfi_close,	/* connection cleanup */
	NULL,		/* unknown SMTP commands */
	mlfi_data,		/* DATA command */
	NULL		/* Once, at the start of each SMTP connection */
};


int main(int argc, char *argv[]) {
	int c, fd;
	int fg = 0;
	const char *args = "p:f";

	/* Process command line options */
	while ((c = getopt(argc, argv, args)) != -1) {
		switch (c) {
		  case 'p':
			if (optarg == NULL || *optarg == '\0') {
				(void) fprintf(stderr, "Illegal conn: %s\n",
					       optarg);
				exit(EX_USAGE);
			}
			(void) smfi_setconn(optarg);
			break;

		case 'f':
			fg = 1;
			break;

		}
	}

	if (smfi_register(smfilter) == MI_FAILURE) {
		fprintf(stderr, "smfi_register failed\n");
		exit(EX_UNAVAILABLE);
	}

	if (fg) {
		return smfi_main();
	}

	if (fork() == 0) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		fd = open("/dev/tty", O_RDWR);
		if (fd >= 0) {
			ioctl(fd, TIOCNOTTY, 0);
			close(fd);
		}

		return smfi_main();
	}
	return 0;
}

