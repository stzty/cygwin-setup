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

/* The purpose of this file is to get the list of mirror sites and ask
   the user which mirror site they want to download from. */

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "msg.h"
#include "concat.h"
#include "mount.h"

#include "port.h"

#define NO_IDX (-1)
#define OTHER_IDX (-2)
static char **site_list = 0;
static int list_idx = NO_IDX;
static int mirror_idx = NO_IDX;

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), (mirror_idx != NO_IDX) ? 1 : 0);
}

static void
load_dialog (HWND h)
{
  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
  SendMessage (listbox, LB_SETCURSEL, list_idx, 0);
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
  list_idx = SendMessage (listbox, LB_GETCURSEL, 0, 0);
  if (list_idx == LB_ERR)
    {
      mirror_site = 0;
      mirror_idx = NO_IDX;
      list_idx = NO_IDX;
    }
  else
    {
      mirror_idx = SendMessage (listbox, LB_GETITEMDATA, list_idx, 0);
      if (mirror_idx == OTHER_IDX)
	mirror_site = 0;
      else
	mirror_site = site_list[mirror_idx];
    }
}

static void
get_root_dir ()
{
  int istext;
  int issystem;
  if (root_dir)
    return;
  root_dir = find_root_mount (&istext, &issystem);
}

void
save_site_url ()
{
  if (! MIRROR_SITE)
    return;

  get_root_dir ();
  if (! root_dir)
    return;
  
  FILE *f = fopen (concat (root_dir, "/etc/setup/last-mirror", 0), "wb");
  if (!f)
    return;
  fprintf (f, "%s\n", MIRROR_SITE);
  fclose (f);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_URL_LIST:
      save_dialog (h);
      check_if_enable_next (h);
      break;

    case IDOK:
      save_dialog (h);
      if (mirror_idx == OTHER_IDX)
	NEXT (IDD_OTHER_URL);
      else
	{
	  other_url = 0;
	  save_site_url ();
	  NEXT (IDD_S_LOAD_INI);
	}
      break;

    case IDC_BACK:
      save_dialog (h);
      NEXT (IDD_NET);
      break;

    case IDCANCEL:
      NEXT (0);
      break;
    }
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int i, j;
  HWND listbox;
  switch (message)
    {
    case WM_INITDIALOG:
      listbox = GetDlgItem (h, IDC_URL_LIST);
      for (i=0; site_list[i]; i++)
	{
	  j = SendMessage (listbox, LB_ADDSTRING, 0, (LPARAM)site_list[i]);
	  SendMessage (listbox, LB_SETITEMDATA, j, i);
	}
      j = SendMessage (listbox, LB_ADDSTRING, 0, (LPARAM)"Other URL");
      SendMessage (listbox, LB_SETITEMDATA, j, OTHER_IDX);
      load_dialog (h);
      return FALSE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

static int
get_site_list (HINSTANCE h)
{
  char mirror_url[1000];
  if (LoadString (h, IDS_MIRROR_LST, mirror_url, sizeof (mirror_url)) <= 0)
    return 1;
  char *mirrors = get_url_to_string (mirror_url);
  dismiss_url_status_dialog ();
  if (!mirrors)
    return 1;

  char *bol, *eol, *nl;

  
  /* null plus account for possibly missing NL plus account for "Other
    URL" from previous run. */
  int nmirrors = 3;

  for (bol=mirrors; *bol; bol++)
    if (*bol == '\n')
      nmirrors ++;

  site_list = (char **) malloc (nmirrors * sizeof (char *));
  nmirrors = 0;

  nl = mirrors;
  while (*nl)
    {
      bol = nl;
      for (eol = bol; *eol && *eol != '\n'; eol++) ;
      if (*eol)
	nl = eol+1;
      else
	nl = eol;
      while (eol > bol && eol[-1] == '\r')
	eol--;
      *eol = 0;
      if (bol[0] != '#' && bol[0] > ' ')
	{
	  site_list[nmirrors] = _strdup (bol);
	  char *semi = strchr (site_list[nmirrors], ';');
	  if (semi)
	    *semi = 0;
	  nmirrors++;
	}
    }
  site_list[nmirrors] = 0;

  return 0;
}

static void
get_initial_list_idx ()
{
  get_root_dir ();
  if (! root_dir)
    return;

  FILE *f = fopen (concat (root_dir, "/etc/setup/last-mirror", 0), "rt");
  if (!f)
    return;

  char site[1000];
  site[0]='\0';
  char * fg_ret = fgets (site, 1000, f);
  fclose (f);
  if (! fg_ret)
    return;

  char *eos = site + strlen (site) - 1;
  while (eos >= site && (*eos == '\n' || *eos == '\r'))
    *eos-- = '\0';

  if (eos < site)
    return;

  int i;
  for (i = 0; site_list[i]; i++)
    if (strcmp (site_list[i], site) == 0)
      break;

  if (! site_list[i])
    {
      site_list[i] = _strdup (site);
      site_list[i+1] = 0;
    }

  mirror_idx = list_idx = i;
}

void
do_site (HINSTANCE h)
{
  int rv = 0;

  if (site_list == 0)
    if (get_site_list (h))
      {
	NEXT (0);
	return;
      }

  get_initial_list_idx ();

  rv = DialogBox (h, MAKEINTRESOURCE (IDD_SITE), 0, dialog_proc);
  if (rv == -1)
    fatal (IDS_DIALOG_FAILED);
}
