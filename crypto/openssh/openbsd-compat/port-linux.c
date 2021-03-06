/* $Id: port-linux.c,v 1.8 2010/03/01 04:52:50 dtucker Exp $ */

/*
 * Copyright (c) 2005 Daniel Walsh <dwalsh@redhat.com>
 * Copyright (c) 2006 Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Linux-specific portability code - just SELinux support at present
 */

#include "includes.h"

#if defined(WITH_SELINUX) || defined(LINUX_OOM_ADJUST)
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "xmalloc.h"
#include "port-linux.h"

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/flask.h>
#include <selinux/get_context_list.h>

/* Wrapper around is_selinux_enabled() to log its return value once only */
int
ssh_selinux_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		enabled = is_selinux_enabled();
		debug("SELinux support %s", enabled ? "enabled" : "disabled");
	}

	return (enabled);
}

/* Return the default security context for the given username */
static security_context_t
ssh_selinux_getctxbyname(char *pwname)
{
	security_context_t sc;
	char *sename = NULL, *lvl = NULL;
	int r;

#ifdef HAVE_GETSEUSERBYNAME
	if (getseuserbyname(pwname, &sename, &lvl) != 0)
		return NULL;
#else
	sename = pwname;
	lvl = NULL;
#endif

#ifdef HAVE_GET_DEFAULT_CONTEXT_WITH_LEVEL
	r = get_default_context_with_level(sename, lvl, NULL, &sc);
#else
	r = get_default_context(sename, NULL, &sc);
#endif

	if (r != 0) {
		switch (security_getenforce()) {
		case -1:
			fatal("%s: ssh_selinux_getctxbyname: "
			    "security_getenforce() failed", __func__);
		case 0:
			error("%s: Failed to get default SELinux security "
			    "context for %s", __func__, pwname);
			break;
		default:
			fatal("%s: Failed to get default SELinux security "
			    "context for %s (in enforcing mode)",
			    __func__, pwname);
		}
	}

#ifdef HAVE_GETSEUSERBYNAME
	if (sename != NULL)
		xfree(sename);
	if (lvl != NULL)
		xfree(lvl);
#endif

	return (sc);
}

/* Set the execution context to the default for the specified user */
void
ssh_selinux_setup_exec_context(char *pwname)
{
	security_context_t user_ctx = NULL;

	if (!ssh_selinux_enabled())
		return;

	debug3("%s: setting execution context", __func__);

	user_ctx = ssh_selinux_getctxbyname(pwname);
	if (setexeccon(user_ctx) != 0) {
		switch (security_getenforce()) {
		case -1:
			fatal("%s: security_getenforce() failed", __func__);
		case 0:
			error("%s: Failed to set SELinux execution "
			    "context for %s", __func__, pwname);
			break;
		default:
			fatal("%s: Failed to set SELinux execution context "
			    "for %s (in enforcing mode)", __func__, pwname);
		}
	}
	if (user_ctx != NULL)
		freecon(user_ctx);

	debug3("%s: done", __func__);
}

/* Set the TTY context for the specified user */
void
ssh_selinux_setup_pty(char *pwname, const char *tty)
{
	security_context_t new_tty_ctx = NULL;
	security_context_t user_ctx = NULL;
	security_context_t old_tty_ctx = NULL;

	if (!ssh_selinux_enabled())
		return;

	debug3("%s: setting TTY context on %s", __func__, tty);

	user_ctx = ssh_selinux_getctxbyname(pwname);

	/* XXX: should these calls fatal() upon failure in enforcing mode? */

	if (getfilecon(tty, &old_tty_ctx) == -1) {
		error("%s: getfilecon: %s", __func__, strerror(errno));
		goto out;
	}

	if (security_compute_relabel(user_ctx, old_tty_ctx,
	    SECCLASS_CHR_FILE, &new_tty_ctx) != 0) {
		error("%s: security_compute_relabel: %s",
		    __func__, strerror(errno));
		goto out;
	}

	if (setfilecon(tty, new_tty_ctx) != 0)
		error("%s: setfilecon: %s", __func__, strerror(errno));
 out:
	if (new_tty_ctx != NULL)
		freecon(new_tty_ctx);
	if (old_tty_ctx != NULL)
		freecon(old_tty_ctx);
	if (user_ctx != NULL)
		freecon(user_ctx);
	debug3("%s: done", __func__);
}

void
ssh_selinux_change_context(const char *newname)
{
	int len, newlen;
	char *oldctx, *newctx, *cx;

	if (!ssh_selinux_enabled())
		return;

	if (getcon((security_context_t *)&oldctx) < 0) {
		logit("%s: getcon failed with %s", __func__, strerror (errno));
		return;
	}
	if ((cx = index(oldctx, ':')) == NULL || (cx = index(cx + 1, ':')) ==
	    NULL) {
		logit ("%s: unparseable context %s", __func__, oldctx);
		return;
	}

	newlen = strlen(oldctx) + strlen(newname) + 1;
	newctx = xmalloc(newlen);
	len = cx - oldctx + 1;
	memcpy(newctx, oldctx, len);
	strlcpy(newctx + len, newname, newlen - len);
	if ((cx = index(cx + 1, ':')))
		strlcat(newctx, cx, newlen);
	debug3("%s: setting context from '%s' to '%s'", __func__, oldctx,
	    newctx);
	if (setcon(newctx) < 0)
		logit("%s: setcon failed with %s", __func__, strerror (errno));
	xfree(oldctx);
	xfree(newctx);
}
#endif /* WITH_SELINUX */

#ifdef LINUX_OOM_ADJUST
#define OOM_ADJ_PATH	"/proc/self/oom_adj"
/*
 * The magic "don't kill me", as documented in eg:
 * http://lxr.linux.no/#linux+v2.6.32/Documentation/filesystems/proc.txt
 */
#define OOM_ADJ_NOKILL	-17

static int oom_adj_save = INT_MIN;

/*
 * Tell the kernel's out-of-memory killer to avoid sshd.
 * Returns the previous oom_adj value or zero.
 */
void
oom_adjust_setup(void)
{
	FILE *fp;

	debug3("%s", __func__);
	if ((fp = fopen(OOM_ADJ_PATH, "r+")) != NULL) {
		if (fscanf(fp, "%d", &oom_adj_save) != 1)
			verbose("error reading %s: %s", OOM_ADJ_PATH, strerror(errno));
		else {
			rewind(fp);
			if (fprintf(fp, "%d\n", OOM_ADJ_NOKILL) <= 0)
				verbose("error writing %s: %s",
				    OOM_ADJ_PATH, strerror(errno));
			else
				verbose("Set %s from %d to %d",
				    OOM_ADJ_PATH, oom_adj_save, OOM_ADJ_NOKILL);
		}
		fclose(fp);
	}
}

/* Restore the saved OOM adjustment */
void
oom_adjust_restore(void)
{
	FILE *fp;

	debug3("%s", __func__);
	if (oom_adj_save == INT_MIN || (fp = fopen(OOM_ADJ_PATH, "w")) == NULL)
		return;

	if (fprintf(fp, "%d\n", oom_adj_save) <= 0)
		verbose("error writing %s: %s", OOM_ADJ_PATH, strerror(errno));
	else
		verbose("Set %s to %d", OOM_ADJ_PATH, oom_adj_save);

	fclose(fp);
	return;
}
#endif /* LINUX_OOM_ADJUST */
#endif /* WITH_SELINUX || LINUX_OOM_ADJUST */
