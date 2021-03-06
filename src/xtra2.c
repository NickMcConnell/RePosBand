/*
 * File: xtra2.c
 * Purpose: Targetting, sorting, panel update
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "reposband.h"
#include "cmds.h"
#include "history.h"
#include "macro.h"
#include "object/tvalsval.h"
#include "target.h"

/* Private function that is shared by verify_panel() and center_panel() */
void verify_panel_int(bool centered);

/*
 * Advance experience levels and print experience
 */
void check_experience(void)
{
	/* Hack -- lower limit */
	if (p_ptr->exp < 0) p_ptr->exp = 0;

	/* Hack -- lower limit */
	if (p_ptr->max_exp < 0) p_ptr->max_exp = 0;

	/* Hack -- upper limit */
	if (p_ptr->exp > PY_MAX_EXP) p_ptr->exp = PY_MAX_EXP;

	/* Hack -- upper limit */
	if (p_ptr->max_exp > PY_MAX_EXP) p_ptr->max_exp = PY_MAX_EXP;


	/* Hack -- maintain "max" experience */
	if (p_ptr->exp > p_ptr->max_exp) p_ptr->max_exp = p_ptr->exp;

	/* Redraw experience */
	p_ptr->redraw |= (PR_EXP);

	/* Handle stuff */
	handle_stuff();


	/* Lose levels while possible */
	while ((p_ptr->lev > 1) &&
	       (p_ptr->exp < (player_exp[p_ptr->lev-2] *
	                      p_ptr->expfact / 100L)))
	{
		/* Lose a level */
		p_ptr->lev--;

		/* Update some stuff */
		p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

		/* Redraw some stuff */
		p_ptr->redraw |= (PR_LEV | PR_TITLE | PR_EXP);

		/* Handle stuff */
		handle_stuff();
	}


	/* Gain levels while possible */
	while ((p_ptr->lev < PY_MAX_LEVEL) &&
	       (p_ptr->exp >= (player_exp[p_ptr->lev-1] *
	                       p_ptr->expfact / 100L)))
	{
		char buf[80];

		/* Gain a level */
		p_ptr->lev++;

		/* Save the highest level */
		if (p_ptr->lev > p_ptr->max_lev) p_ptr->max_lev = p_ptr->lev;

		/* Log level updates */
		strnfmt(buf, sizeof(buf), "Reached level %d", p_ptr->lev);
		history_add(buf, HISTORY_GAIN_LEVEL, 0);

		/* Evolve -Simon */
		if ((p_ptr->lev >= rp_ptr->max_level) && (rp_ptr->next_form_indices[0] >= 0))
		{
			int choice;
			int i;
			if (rp_ptr->next_form_indices[1] == -1)
			{
				/* No choice */
				choice = 0;
			}
			else
			{
				char key;
				char buf[40];
			
				repeat:
				/* Give the menu */
				
				screen_save();
			
				prt(" Choose your new race:", 1, 20);
				prt("", 2, 20);
				for (i = 0; i < MAX_NEXT_FORMS; i++)
				{
					if (rp_ptr->next_form_indices[i] < 0) break;
					prt(format(" %c) %s", I2A(i), p_info[rp_ptr->next_form_indices[i]].name), i + 3, 20);
				}
				prt("", i + 3, 20);
				prt(" h) Help", i + 4, 20);
				prt("", i + 5, 20);
				
				while (1)
				{
					key = inkey();
					key = tolower(key);
					
					/* Help */
					if (key == 'h')
					{
						strnfmt(buf, sizeof(buf), "monrace.txt#%s", rp_ptr->name);
						show_file(buf, NULL, 0, 0);
						screen_load();
						goto repeat;
					}
					
					if (key >= 'a' && key < I2A(i)) break;
				}
			
				screen_load();
			
				choice = A2I(key);
			}
			p_ptr->prace = rp_ptr->next_form_indices[choice];
			rp_ptr = &p_info[p_ptr->prace];
			p_ptr->expfact = rp_ptr->r_exp + cp_ptr->c_exp;
			p_ptr->hitdie = rp_ptr->r_mhp + cp_ptr->c_mhp;
			do_cmd_redraw();
			msg_format("You transform into a %^s!", p_info[p_ptr->prace].name);
			check_experience();
			if (p_ptr->lev < p_ptr->max_lev && p_ptr->exp == p_ptr->max_exp)
				msg_format("You don't have enough XP to be level %d in this form!", p_ptr->max_lev);
		}		
		
		/* Message */
		message_format(MSG_LEVEL, p_ptr->lev, "Welcome to level %d.",
			p_ptr->lev);

		/* Add to social class */
		p_ptr->sc += randint1(2);
		if (p_ptr->sc > 150) p_ptr->sc = 150;
				
		/* Update some stuff */
		p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

		/* Redraw some stuff */
		p_ptr->redraw |= (PR_LEV | PR_TITLE | PR_EXP);

		/* Handle stuff */
		handle_stuff();
	}

	/* Gain max levels while possible */
	while ((p_ptr->max_lev < PY_MAX_LEVEL) &&
	       (p_ptr->max_exp >= (player_exp[p_ptr->max_lev-1] *
	                           p_ptr->expfact / 100L)))
	{
		/* Gain max level */
		p_ptr->max_lev++;

		/* Update some stuff */
		p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

		/* Redraw some stuff */
		p_ptr->redraw |= (PR_LEV | PR_TITLE | PR_EXP);

		/* Handle stuff */
		handle_stuff();
	}
}


/*
 * Gain experience
 */
void gain_exp(s32b amount)
{
	/* Gain some experience */
	p_ptr->exp += amount;

	/* Slowly recover from experience drainage */
	if (p_ptr->exp < p_ptr->max_exp)
	{
		/* Gain max experience (10%) */
		p_ptr->max_exp += amount / 10;
	}

	/* Check Experience */
	check_experience();
}


/*
 * Lose experience
 */
void lose_exp(s32b amount)
{
	/* Never drop below zero experience */
	if (amount > p_ptr->exp) amount = p_ptr->exp;

	/* Lose some experience */
	p_ptr->exp -= amount;

	/* Check Experience */
	check_experience();
}


/*
 * Modify the current panel to the given coordinates, adjusting only to
 * ensure the coordinates are legal, and return TRUE if anything done.
 *
 * The town should never be scrolled around.
 *
 * Note that monsters are no longer affected in any way by panel changes.
 *
 * As a total hack, whenever the current panel changes, we assume that
 * the "overhead view" window should be updated.
 */
bool modify_panel(term *t, int wy, int wx)
{
	int dungeon_hgt = (p_ptr->depth == 0) ? TOWN_HGT : DUNGEON_HGT;
	int dungeon_wid = (p_ptr->depth == 0) ? TOWN_WID : DUNGEON_WID;

	/* Verify wy, adjust if needed */
	if (wy > dungeon_hgt - SCREEN_HGT) wy = dungeon_hgt - SCREEN_HGT;
	if (wy < 0) wy = 0;

	/* Verify wx, adjust if needed */
	if (wx > dungeon_wid - SCREEN_WID) wx = dungeon_wid - SCREEN_WID;
	if (wx < 0) wx = 0;

	/* React to changes */
	if ((t->offset_y != wy) || (t->offset_x != wx))
	{
		/* Save wy, wx */
		t->offset_y = wy;
		t->offset_x = wx;

		/* Redraw map */
		p_ptr->redraw |= (PR_MAP);

		/* Redraw for big graphics */
		if ((tile_width > 1) || (tile_height > 1)) redraw_stuff();
      
		/* Changed */
		return (TRUE);
	}

	/* No change */
	return (FALSE);
}


/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return TRUE if any
 * such adjustment was performed.
 */
bool adjust_panel(int y, int x)
{
	bool changed = FALSE;

	int j;

	/* Scan windows */
	for (j = 0; j < REPOSBAND_TERM_MAX; j++)
	{
		int wx, wy;
		int screen_hgt, screen_wid;

		term *t = reposband_term[j];

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if ((j > 0) && !(op_ptr->window_flag[j] & PW_MAP)) continue;

		wy = t->offset_y;
		wx = t->offset_x;

		screen_hgt = (j == 0) ? SCREEN_HGT : t->hgt;
		screen_wid = (j == 0) ? SCREEN_WID : t->wid;

		/* Adjust as needed */
		while (y >= wy + screen_hgt) wy += screen_hgt / 2;
		while (y < wy) wy -= screen_hgt / 2;

		/* Adjust as needed */
		while (x >= wx + screen_wid) wx += screen_wid / 2;
		while (x < wx) wx -= screen_wid / 2;

		/* Use "modify_panel" */
		if (modify_panel(t, wy, wx)) changed = TRUE;
	}

	return (changed);
}


/*
 * Change the current panel to the panel lying in the given direction.
 *
 * Return TRUE if the panel was changed.
 */
bool change_panel(int dir)
{
	bool changed = FALSE;
	int j;

	/* Scan windows */
	for (j = 0; j < REPOSBAND_TERM_MAX; j++)
	{
		int screen_hgt, screen_wid;
		int wx, wy;

		term *t = reposband_term[j];

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if ((j > 0) && !(op_ptr->window_flag[j] & PW_MAP)) continue;

		screen_hgt = (j == 0) ? SCREEN_HGT : t->hgt;
		screen_wid = (j == 0) ? SCREEN_WID : t->wid;

		/* Shift by half a panel */
		wy = t->offset_y + ddy[dir] * screen_hgt / 2;
		wx = t->offset_x + ddx[dir] * screen_wid / 2;

		/* Use "modify_panel" */
		if (modify_panel(t, wy, wx)) changed = TRUE;
	}

	return (changed);
}


/*
 * Verify the current panel (relative to the player location).
 *
 * By default, when the player gets "too close" to the edge of the current
 * panel, the map scrolls one panel in that direction so that the player
 * is no longer so close to the edge.
 *
 * The "OPT(center_player)" option allows the current panel to always be centered
 * around the player, which is very expensive, and also has some interesting
 * gameplay ramifications.
 */
void verify_panel(void)
{
	verify_panel_int(OPT(center_player));
}

void center_panel(void)
{
	verify_panel_int(TRUE);
}

void verify_panel_int(bool centered)
{
	int wy, wx;
	int screen_hgt, screen_wid;

	int panel_wid, panel_hgt;

	int py = p_ptr->py;
	int px = p_ptr->px;

	int j;

	/* Scan windows */
	for (j = 0; j < REPOSBAND_TERM_MAX; j++)
	{
		term *t = reposband_term[j];

		/* No window */
		if (!t) continue;

		/* No relevant flags */
		if ((j > 0) && !(op_ptr->window_flag[j] & (PW_MAP))) continue;

		wy = t->offset_y;
		wx = t->offset_x;

		screen_hgt = (j == 0) ? SCREEN_HGT : t->hgt;
		screen_wid = (j == 0) ? SCREEN_WID : t->wid;

		panel_wid = screen_wid / 2;
		panel_hgt = screen_hgt / 2;


		/* Scroll screen vertically when off-center */
		if (centered && !p_ptr->running && (py != wy + panel_hgt))
			wy = py - panel_hgt;

		/* Scroll screen vertically when 3 grids from top/bottom edge */
		else if ((py < wy + 3) || (py >= wy + screen_hgt - 3))
			wy = py - panel_hgt;


		/* Scroll screen horizontally when off-center */
		if (centered && !p_ptr->running && (px != wx + panel_wid))
			wx = px - panel_wid;

		/* Scroll screen horizontally when 3 grids from left/right edge */
		else if ((px < wx + 3) || (px >= wx + screen_wid - 3))
			wx = px - panel_wid;


		/* Scroll if needed */
		modify_panel(t, wy, wx);
	}
}


/*
 * Given a "source" and "target" location, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int motion_dir(int y1, int x1, int y2, int x2)
{
	/* No movement required */
	if ((y1 == y2) && (x1 == x2)) return (5);

	/* South or North */
	if (x1 == x2) return ((y1 < y2) ? 2 : 8);

	/* East or West */
	if (y1 == y2) return ((x1 < x2) ? 6 : 4);

	/* South-east or South-west */
	if (y1 < y2) return ((x1 < x2) ? 3 : 1);

	/* North-east or North-west */
	if (y1 > y2) return ((x1 < x2) ? 9 : 7);

	/* Paranoia */
	return (5);
}


/*
 * Extract a direction (or zero) from a character
 */
int target_dir(char ch)
{
	int d = 0;

	int mode;

	cptr act;

	cptr s;


	/* Already a direction? */
	if (isdigit((unsigned char)ch))
	{
		d = D2I(ch);
	}
	else if (isarrow(ch))
	{
		switch (ch)
		{
			case ARROW_DOWN:	d = 2; break;
			case ARROW_LEFT:	d = 4; break;
			case ARROW_RIGHT:	d = 6; break;
			case ARROW_UP:		d = 8; break;
		}
	}
	else
	{
		/* Roguelike */
		if (OPT(rogue_like_commands))
		{
			mode = KEYMAP_MODE_ROGUE;
		}

		/* Original */
		else
		{
			mode = KEYMAP_MODE_ORIG;
		}

		/* Extract the action (if any) */
		act = keymap_act[mode][(byte)(ch)];

		/* Analyze */
		if (act)
		{
			/* Convert to a direction */
			for (s = act; *s; ++s)
			{
				/* Use any digits in keymap */
				if (isdigit((unsigned char)*s)) d = D2I(*s);
			}
		}
	}

	/* Paranoia */
	if (d == 5) d = 0;

	/* Return direction */
	return (d);
}


int dir_transitions[10][10] = 
{
	/* 0-> */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	/* 1-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 2-> */ { 0, 0, 2, 0, 1, 0, 3, 0, 5, 0 }, 
	/* 3-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 4-> */ { 0, 0, 1, 0, 4, 0, 5, 0, 7, 0 }, 
	/* 5-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 6-> */ { 0, 0, 3, 0, 5, 0, 6, 0, 9, 0 }, 
	/* 7-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 8-> */ { 0, 0, 5, 0, 7, 0, 9, 0, 8, 0 }, 
	/* 9-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};


/*
 * Get an "aiming direction" (1,2,3,4,6,7,8,9 or 5) from the user.
 *
 * Return TRUE if a direction was chosen, otherwise return FALSE.
 *
 * The direction "5" is special, and means "use current target".
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 *
 * Note that "Force Target", if set, will pre-empt user interaction,
 * if there is a usable target already set.
 *
 * Currently this function applies confusion directly.
 */
bool get_aim_dir(int *dp)
{
	/* Global direction */
	int dir = 0;
	
	ui_event_data ke;

	cptr p;

	/* Initialize */
	(*dp) = 0;

	/* Hack -- auto-target if requested */
	if (OPT(use_old_target) && target_okay() && !dir) dir = 5;

	/* Ask until satisfied */
	while (!dir)
	{
		/* Choose a prompt */
		if (!target_okay())
			p = "Direction ('*' or <click> to target, \"'\" for closest, Escape to cancel)? ";
		else
			p = "Direction ('5' for target, '*' or <click> to re-target, Escape to cancel)? ";

		/* Get a command (or Cancel) */
		if (!get_com_ex(p, &ke)) break;

		if (ke.type == EVT_MOUSE)
		{
			if (target_set_interactive(TARGET_KILL, KEY_GRID_X(ke), KEY_GRID_Y(ke)))
				dir = 5;
		}
		else if (ke.type == EVT_KBRD)
		{
			if (ke.key == '*')
			{
				/* Set new target, use target if legal */
				if (target_set_interactive(TARGET_KILL, -1, -1))
					dir = 5;
			}
			else if (ke.key == '\'')
			{
				/* Set to closest target */
				if (target_set_closest(TARGET_KILL))
					dir = 5;
			}
			else if (ke.key == 't' || ke.key == '5' ||
					ke.key == '0' || ke.key == '.')
			{
				if (target_okay())
					dir = 5;
			}
			else
			{
				/* Possible direction */
				int keypresses_handled = 0;
				
				while (ke.key != 0)
				{
					int this_dir;
					
					/* XXX Ideally show and move the cursor here to indicate 
					   the currently "Pending" direction. XXX */
					this_dir = target_dir(ke.key);
					
					if (this_dir)
						dir = dir_transitions[dir][this_dir];
					else
						break;
					
					if (lazymove_delay == 0 || ++keypresses_handled > 1)
						break;
				
					/* See if there's a second keypress within the defined
					   period of time. */
					inkey_scan = lazymove_delay; 
					ke = inkey_ex();
				}
			}
		}

		/* Error */
		if (!dir) bell("Illegal aim direction!");
	}

	/* No direction */
	if (!dir) return (FALSE);

	/* Save direction */
	(*dp) = dir;

	/* Check for confusion */
	if (p_ptr->timed[TMD_CONFUSED])
	{
		/* Random direction */
		dir = ddd[randint0(8)];
	}

	/* Notice confusion */
	if ((*dp) != dir)
	{
		/* Warn the user */
		msg_print("You are confused.");
	}

	/* Save direction */
	(*dp) = dir;

	/* A "valid" direction was entered */
	return (TRUE);
}


/*
 * Request a "movement" direction (1,2,3,4,6,7,8,9) from the user.
 *
 * Return TRUE if a direction was chosen, otherwise return FALSE.
 *
 * This function should be used for all "repeatable" commands, such as
 * run, walk, open, close, bash, disarm, spike, tunnel, etc, as well
 * as all commands which must reference a grid adjacent to the player,
 * and which may not reference the grid under the player.
 *
 * Directions "5" and "0" are illegal and will not be accepted.
 *
 * This function tracks and uses the "global direction", and uses
 * that as the "desired direction", if it is set.
 */
bool get_rep_dir(int *dp)
{
	int dir = 0;

	ui_event_data ke;

	/* Initialize */
	(*dp) = 0;

	/* Get a direction */
	while (!dir)
	{
		/* Paranoia XXX XXX XXX */
		message_flush();

		/* Get first keypress - the first test is to avoid displaying the
		   prompt for direction if there's already a keypress queued up
		   and waiting - this just avoids a flickering prompt if there is
		   a "lazy" movement delay. */
		inkey_scan = SCAN_INSTANT;
		ke = inkey_ex();
		inkey_scan = SCAN_OFF;

		if (ke.type == EVT_KBRD && target_dir(ke.key) == 0)
		{
			prt("Direction or <click> (Escape to cancel)? ", 0, 0);
			ke = inkey_ex();
		}

		/* Check mouse coordinates */
		if (ke.type == EVT_MOUSE)
		{
			/*if (ke.button) */
			{
				int y = KEY_GRID_Y(ke);
				int x = KEY_GRID_X(ke);

				/* Calculate approximate angle */
				int angle = get_angle_to_target(p_ptr->py, p_ptr->px, y, x, 0);

				/* Convert angle to direction */
				if (angle < 15) dir = 6;
				else if (angle < 33) dir = 9;
				else if (angle < 59) dir = 8;
				else if (angle < 78) dir = 7;
				else if (angle < 104) dir = 4;
				else if (angle < 123) dir = 1;
				else if (angle < 149) dir = 2;
				else if (angle < 168) dir = 3;
				else dir = 6;
			}
		}

		/* Get other keypresses until a direction is chosen. */
		else
		{
			int keypresses_handled = 0;

			while (ke.key != 0)
			{
				int this_dir;

				if (ke.key == ESCAPE) 
				{
					/* Clear the prompt */
					prt("", 0, 0);

					return (FALSE);
				}

				/* XXX Ideally show and move the cursor here to indicate 
				   the currently "Pending" direction. XXX */
				this_dir = target_dir(ke.key);

				if (this_dir)
				{
					dir = dir_transitions[dir][this_dir];
				}

				if (lazymove_delay == 0 || ++keypresses_handled > 1)
					break;

				inkey_scan = lazymove_delay; 
				ke = inkey_ex();
			}

			/* 5 is equivalent to "escape" */
			if (dir == 5)
			{
				/* Clear the prompt */
				prt("", 0, 0);

				return (FALSE);
			}
		}

		/* Oops */
		if (!dir) bell("Illegal repeatable direction!");
	}

	/* Clear the prompt */
	prt("", 0, 0);

	/* Save direction */
	(*dp) = dir;

	/* Success */
	return (TRUE);
}


/*
 * Apply confusion, if needed, to a direction
 *
 * Display a message and return TRUE if direction changes.
 */
bool confuse_dir(int *dp)
{
	int dir;

	/* Default */
	dir = (*dp);

	/* Apply "confusion" */
	if (p_ptr->timed[TMD_CONFUSED])
	{
		/* Apply confusion XXX XXX XXX */
		if ((dir == 5) || (randint0(100) < 75))
		{
			/* Random direction */
			dir = ddd[randint0(8)];
		}
	}

	/* Notice confusion */
	if ((*dp) != dir)
	{
		/* Warn the user */
		msg_print("You are confused.");

		/* Save direction */
		(*dp) = dir;

		/* Confused */
		return (TRUE);
	}

	/* Not confused */
	return (FALSE);
}



