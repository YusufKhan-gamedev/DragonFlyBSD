/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)mount_lfs.c	8.3 (Berkeley) 3/27/94
 * $FreeBSD: src/sbin/mount_ext2fs/mount_ext2fs.c,v 1.11 1999/10/09 11:54:09 phk Exp $
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <mntopts.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <inttypes.h>
#include <libutil.h>

#include <vfs/tarfs/tarfs_args.h>

/* XXX: TODO: Add the 'as' argument here, trivial */
struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_SYNC,
	MOPT_UPDATE,
	MOPT_NULL
};

static void	usage(void) __dead2;

static gid_t
a_gid(char *s)
{
	struct group *gr;
	char *gname;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		gid = gr->gr_gid;
	else {
		for (gname = s; *s && isdigit(*s); ++s);
		if (!*s)
			gid = atoi(gname);
		else
			errx(EX_NOUSER, "unknown group id: %s", gname);
	}
	return (gid);
}

static uid_t
a_uid(char *s)
{
        struct passwd *pw;
        char *uname;
        uid_t uid;
        
        if ((pw = getpwnam(s)) != NULL)
                uid = pw->pw_uid;
        else {
                for (uname = s; *s && isdigit(*s); ++s);
		if (!*s)
			uid = atoi(uname);
		else
			errx(EX_NOUSER, "unknown user id: %s", uname);
	}
	return (uid);
}

static mode_t
a_mask(char *s)
{
	int done, rv = 0;
	char *ep;

	done = 0;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(s, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(EX_USAGE, "invalid file mode: %s", s);
	return (rv);
}

int
main(int argc, char **argv)
{
	struct tarfs_args args;
	int ch, mntflags;
	char *fs_name, mntpath[MAXPATHLEN];
	struct vfsconf vfc;
	struct stat sb;
	int gidset, modeset, uidset; /* Ought to be 'bool'. */
	gid_t gid = 0;
	uid_t uid = 0;
	mode_t mode = 0;
	int error;

	mntflags = 0;
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case 'g':
			gid = a_gid(optarg);
			gidset = 1;
			break;
		case 'm':
			mode = a_mask(optarg);
			modeset = 1;
			break;
		case 'u':
			uid = a_uid(optarg);
			uidset = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

        args.from = argv[0];	/* the name of the device file */
	fs_name = argv[1];	/* the mount point */

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	checkpath(fs_name, mntpath);
	rmslashes(args.from, args.from);

#define DEFAULT_ROOTUID	-2
	args.root_uid = DEFAULT_ROOTUID;

        if (stat(args.from, &sb) == -1)
                err(EXIT_FAILURE, "cannot stat `%s'", args.from);

	args.root_uid = uidset ? uid : sb.st_uid;
	args.root_gid = gidset ? gid : sb.st_gid;
	args.root_mode = modeset ? mode : sb.st_mode;

	error = getvfsbyname("tarfs", &vfc);
	if (error && vfsisloadable("tarfs")) {
		if (vfsload("tarfs")) {
			err(EX_OSERR, "vfsload(tarfs)");
		}
		endvfsent();	/* flush cache */
		error = getvfsbyname("tarfs", &vfc);
	}
	if (error)
		errx(EX_OSERR, "tarfs filesystem is not available");

	if (mount(vfc.vfc_name, mntpath, mntflags, &args) < 0)
		err(EX_OSERR, "%s", args.from);
	exit(0);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: mount_tarfs [-o options] [-m mode] [-u user] tarball\n");
	exit(EX_USAGE);
}

