/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to manage all the desktop setup, such
   as start menu, batch files, desktop icons, and shortcuts.  Note
   that unlike other do_* functions, this one is called directly from
   install.cc */

#include "win32.h"
#include <shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "resource.h"
#include "msg.h"
#include "state.h"
#include "concat.h"
#include "mkdir.h"
#include "dialog.h"

#include "port.h"

extern "C" {
  void make_link_2 (char *exepath, char *args, char *icon, char *lname);
};

static OSVERSIONINFO verinfo;

/* Lines starting with '@' are conditionals - include 'N' for NT,
   '5' for Win95, '8' for Win98, '*' for all, like this:
	echo foo
	@N8
	echo NT or 98
	@*
   */

static char *etc_profile[] = {
  "PATH=\"/bin:/usr/bin:/usr/local/bin:$PATH\"",
  "unset DOSDRIVE",
  "unset DOSDIR",
  "unset TMPDIR",
  "unset TMP",
  "",
  "USER=`id -un`",
  "",
  "# Set up USER's home directory",
  "if [ -z \"$HOME\" ]; then",
  "  HOME=/home/$USER",
  "fi",
  "",
  "if [ ! -d $HOME ]; then",
  "  mkdir -p $HOME",
  "fi",
  "",
  "export HOME USER",
  "",
  "for i in /etc/profile.d/*.sh ; do",
  "  if [ -f $i ]; then",
  "    . $i",
  "  fi",
  "done",
  "",
  "for i in /etc/postinstall/*.sh ; do",
  "  if [ -f $i ]; then",
  "    echo Running post-install script $i...",
  "    . $i",
  "    mv $i $i.done",
  "  fi",
  "done",
  "",
  "export MAKE_MODE=unix",
  "export PS1='\033]0;\\w\a",
  "\033[32m\\u@\\h \033[33m\\w\033[0m",
  "$ '",
  "",
  "cd $HOME",
  0
};

static char *postinstall_script[] = {
  "if [ ! -f /etc/passwd ]; then",
  "  echo Creating /etc/passwd",
  "  mkpasswd -l >/etc/passwd 2>/dev/null",
  "fi",
  "if [ ! -f /etc/group ]; then",
  "  echo Creating /etc/group",
  "  mkgroup -l >/etc/group 2>/dev/null",
  "fi",
  0
};

#define COMMAND9XARGS "/E:4096 /c"
#define COMMAND9XEXE  "\\command.com"

static char *batname;
static char *iconname;

static char *
backslash (char *s)
{
  for (char *t = s; *t; t++)
    if (*t == '/')
      *t = '\\';
  return s;
}

static void
make_link (char *linkpath, char *title, char *target)
{
  char argbuf[_MAX_PATH];
  char *fname = concat (linkpath, "/", title, ".lnk", 0);

  if (_access (fname, 0) == 0)
    return; /* already exists */

  msg ("make_link %s, %s, %s\n", fname, title, target);

  mkdir_p (0, fname);

  char *exepath, *args;

  /* If we are running Win9x, build a command line. */
  if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
      exepath = target;
      args = "";
    }
  else
    {
      char *pccmd;
      char windir[MAX_PATH];

      GetWindowsDirectory (windir, sizeof (windir));
      exepath = concat (windir, COMMAND9XEXE, 0);
      sprintf (argbuf, "%s %s", COMMAND9XARGS, target);
      args = argbuf;
    }

  msg ("make_link_2 (%s, %s, %s, %s)", exepath, args, iconname, fname);
  make_link_2 (exepath, args, iconname, fname);
}

static void
start_menu (char *title, char *target)
{
  char path[_MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL, issystem ? CSIDL_COMMON_PROGRAMS : CSIDL_PROGRAMS, &id);
  SHGetPathFromIDList (id, path);
  strcat (path, "/Cygnus Solutions");
  make_link (path, title, target);
}

static void
desktop_icon (char *title, char *target)
{
  char path[_MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL, issystem ? CSIDL_DESKTOP : CSIDL_COMMON_DESKTOPDIRECTORY, &id);
  SHGetPathFromIDList (id, path);
  make_link (path, title, target);
}

static void
make_cygwin_bat ()
{
  batname = backslash (concat (root_dir, "/cygwin.bat", 0));

  /* if the batch file exists, don't overwrite it */
  if (_access (batname, 0) == 0)
    return;

  FILE *bat = fopen (batname, "wt");
  if (!bat)
    return;

  fprintf (bat, "@echo off\n\n");

  fprintf (bat, "%.2s\n", root_dir);
  fprintf (bat, "chdir %s\n\n", backslash (concat (root_dir+2, "/bin", 0)));

  fprintf (bat, "bash --login -i\n");

  fclose (bat);
}

static void
make_etc_profile ()
{
  char *fname = concat (root_dir, "/etc/profile", 0);

  /* if the file exists, don't overwrite it */
  if (_access (fname, 0) == 0)
    return;

  char os;
  switch (verinfo.dwPlatformId)
    {
      case VER_PLATFORM_WIN32_NT:
	os = 'N';
	break;
      case VER_PLATFORM_WIN32_WINDOWS:
	if (verinfo.dwMinorVersion == 0)
	  os = '5';
	else
	  os = '8';
	break;
      default:
	os = '?';
	break;
    }
  msg ("os is %c", os);

  FILE *p = fopen (fname, "wb");
  if (!p)
    return;

  int i, allow=1;
  for (i=0; etc_profile[i]; i++)
    {
      if (etc_profile[i][0] == '@')
	{
	  allow = 0;
	  msg ("profile: %s", etc_profile[i]);
	  for (char *cp = etc_profile[i]+1; *cp; cp++)
	    if (*cp == os || *cp == '*')
	      allow = 1;
	  msg ("allow is %d\n", allow);
	}
      else if (allow)
	fprintf (p, "%s\n", etc_profile[i]);
    }

  fclose (p);
}

static void
make_postinstall_script ()
{
  if (verinfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
    return;

  char *fname = concat (root_dir, "/etc/postinstall/00-setup.sh", 0);
  mkdir_p (0, fname);

  FILE *p = fopen (fname, "wb");
  if (!p)
    return;

  int i;
  for (i=0; postinstall_script[i]; i++)
    fprintf (p, "%s\n", postinstall_script[i]);

  fclose (p);
}

static void
save_icon ()
{
  iconname = backslash (concat (root_dir, "/cygwin.ico", 0));

  HRSRC rsrc = FindResource (NULL, "CYGWIN.ICON", "FILE");
  if (rsrc == NULL)
    {
      fatal ("FindResource failed");
    }
  HGLOBAL res = LoadResource (NULL, rsrc);
  char *data = (char *) LockResource (res);
  int len = SizeofResource (NULL, rsrc);

  FILE *f = fopen (iconname, "wb");
  if (f)
    {
      fwrite (data, 1, len, f);
      fclose (f);
    }
}

void
do_desktop (HINSTANCE h)
{
  CoInitialize (NULL);

  verinfo.dwOSVersionInfoSize = sizeof (verinfo);
  GetVersionEx (&verinfo);

  save_icon ();

  make_cygwin_bat ();
  make_etc_profile ();
  make_postinstall_script ();

  start_menu ("Cygwin 1.1 Bash Shell", batname);

  desktop_icon ("Cygwin", batname);
}
