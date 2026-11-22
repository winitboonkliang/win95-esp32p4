//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>

#include "doomtype.h"
#include "doomkeys.h"

#include "m_config.h"
#include "m_misc.h"

static int const key_right__b82i = KEY_RIGHTARROW;  /* B82: initial value in flash */
int key_right;  /* B82: live copy, .bss -> PSRAM */
static int const key_left__b82i = KEY_LEFTARROW;  /* B82: initial value in flash */
int key_left;  /* B82: live copy, .bss -> PSRAM */
static int const key_up__b82i = KEY_UPARROW;  /* B82: initial value in flash */
int key_up;  /* B82: live copy, .bss -> PSRAM */
static int const key_down__b82i = KEY_DOWNARROW;  /* B82: initial value in flash */
int key_down;  /* B82: live copy, .bss -> PSRAM */ 
static int const key_strafeleft__b82i = KEY_STRAFE_L;  /* B82: initial value in flash */
int key_strafeleft;  /* B82: live copy, .bss -> PSRAM */
static int const key_straferight__b82i = KEY_STRAFE_R;  /* B82: initial value in flash */
int key_straferight;  /* B82: live copy, .bss -> PSRAM */
static int const key_fire__b82i = KEY_FIRE;  /* B82: initial value in flash */
int key_fire;  /* B82: live copy, .bss -> PSRAM */
static int const key_use__b82i = KEY_USE;  /* B82: initial value in flash */
int key_use;  /* B82: live copy, .bss -> PSRAM */
static int const key_strafe__b82i = KEY_RALT;  /* B82: initial value in flash */
int key_strafe;  /* B82: live copy, .bss -> PSRAM */
static int const key_speed__b82i = KEY_RSHIFT;  /* B82: initial value in flash */
int key_speed;  /* B82: live copy, .bss -> PSRAM */ 

static int const key_flyup__b82i = KEY_PGUP;  /* B82: initial value in flash */
int key_flyup;  /* B82: live copy, .bss -> PSRAM */
static int const key_flydown__b82i = KEY_INS;  /* B82: initial value in flash */
int key_flydown;  /* B82: live copy, .bss -> PSRAM */
static int const key_flycenter__b82i = KEY_HOME;  /* B82: initial value in flash */
int key_flycenter;  /* B82: live copy, .bss -> PSRAM */

static int const key_lookup__b82i = KEY_PGDN;  /* B82: initial value in flash */
int key_lookup;  /* B82: live copy, .bss -> PSRAM */
static int const key_lookdown__b82i = KEY_DEL;  /* B82: initial value in flash */
int key_lookdown;  /* B82: live copy, .bss -> PSRAM */
static int const key_lookcenter__b82i = KEY_END;  /* B82: initial value in flash */
int key_lookcenter;  /* B82: live copy, .bss -> PSRAM */

static int const key_invleft__b82i = '[';  /* B82: initial value in flash */
int key_invleft;  /* B82: live copy, .bss -> PSRAM */
static int const key_invright__b82i = ']';  /* B82: initial value in flash */
int key_invright;  /* B82: live copy, .bss -> PSRAM */
static int const key_useartifact__b82i = KEY_ENTER;  /* B82: initial value in flash */
int key_useartifact;  /* B82: live copy, .bss -> PSRAM */

static int const key_jump__b82i = '/';  /* B82: initial value in flash */
int key_jump;  /* B82: live copy, .bss -> PSRAM */

static int const key_arti_all__b82i = KEY_BACKSPACE;  /* B82: initial value in flash */
int key_arti_all;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_health__b82i = '\\';  /* B82: initial value in flash */
int key_arti_health;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_poisonbag__b82i = '0';  /* B82: initial value in flash */
int key_arti_poisonbag;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_blastradius__b82i = '9';  /* B82: initial value in flash */
int key_arti_blastradius;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_teleport__b82i = '8';  /* B82: initial value in flash */
int key_arti_teleport;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_teleportother__b82i = '7';  /* B82: initial value in flash */
int key_arti_teleportother;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_egg__b82i = '6';  /* B82: initial value in flash */
int key_arti_egg;  /* B82: live copy, .bss -> PSRAM */
static int const key_arti_invulnerability__b82i = '5';  /* B82: initial value in flash */
int key_arti_invulnerability;  /* B82: live copy, .bss -> PSRAM */

static int const key_usehealth__b82i = 'h';  /* B82: initial value in flash */
int key_usehealth;  /* B82: live copy, .bss -> PSRAM */
static int const key_invquery__b82i = 'q';  /* B82: initial value in flash */
int key_invquery;  /* B82: live copy, .bss -> PSRAM */
static int const key_mission__b82i = 'w';  /* B82: initial value in flash */
int key_mission;  /* B82: live copy, .bss -> PSRAM */
static int const key_invpop__b82i = 'z';  /* B82: initial value in flash */
int key_invpop;  /* B82: live copy, .bss -> PSRAM */
static int const key_invkey__b82i = 'k';  /* B82: initial value in flash */
int key_invkey;  /* B82: live copy, .bss -> PSRAM */
static int const key_invhome__b82i = KEY_HOME;  /* B82: initial value in flash */
int key_invhome;  /* B82: live copy, .bss -> PSRAM */
static int const key_invend__b82i = KEY_END;  /* B82: initial value in flash */
int key_invend;  /* B82: live copy, .bss -> PSRAM */
static int const key_invuse__b82i = KEY_ENTER;  /* B82: initial value in flash */
int key_invuse;  /* B82: live copy, .bss -> PSRAM */
static int const key_invdrop__b82i = KEY_BACKSPACE;  /* B82: initial value in flash */
int key_invdrop;  /* B82: live copy, .bss -> PSRAM */


static int const mousebfire__b82i = 0;  /* B82: initial value in flash */
int mousebfire;  /* B82: live copy, .bss -> PSRAM */
static int const mousebstrafe__b82i = 1;  /* B82: initial value in flash */
int mousebstrafe;  /* B82: live copy, .bss -> PSRAM */
static int const mousebforward__b82i = 2;  /* B82: initial value in flash */
int mousebforward;  /* B82: live copy, .bss -> PSRAM */

static int const mousebjump__b82i = -1;  /* B82: initial value in flash */
int mousebjump;  /* B82: live copy, .bss -> PSRAM */

static int const mousebstrafeleft__b82i = -1;  /* B82: initial value in flash */
int mousebstrafeleft;  /* B82: live copy, .bss -> PSRAM */
static int const mousebstraferight__b82i = -1;  /* B82: initial value in flash */
int mousebstraferight;  /* B82: live copy, .bss -> PSRAM */
static int const mousebbackward__b82i = -1;  /* B82: initial value in flash */
int mousebbackward;  /* B82: live copy, .bss -> PSRAM */
static int const mousebuse__b82i = -1;  /* B82: initial value in flash */
int mousebuse;  /* B82: live copy, .bss -> PSRAM */

static int const mousebprevweapon__b82i = -1;  /* B82: initial value in flash */
int mousebprevweapon;  /* B82: live copy, .bss -> PSRAM */
static int const mousebnextweapon__b82i = -1;  /* B82: initial value in flash */
int mousebnextweapon;  /* B82: live copy, .bss -> PSRAM */


static int const key_message_refresh__b82i = KEY_ENTER;  /* B82: initial value in flash */
int key_message_refresh;  /* B82: live copy, .bss -> PSRAM */
static int const key_pause__b82i = KEY_PAUSE;  /* B82: initial value in flash */
int key_pause;  /* B82: live copy, .bss -> PSRAM */
static int const key_demo_quit__b82i = 'q';  /* B82: initial value in flash */
int key_demo_quit;  /* B82: live copy, .bss -> PSRAM */
static int const key_spy__b82i = KEY_F12;  /* B82: initial value in flash */
int key_spy;  /* B82: live copy, .bss -> PSRAM */

static int const key_multi_msg__b82i = 't';  /* B82: initial value in flash */
int key_multi_msg;  /* B82: live copy, .bss -> PSRAM */
int key_multi_msgplayer[8];

static int const key_weapon1__b82i = '1';  /* B82: initial value in flash */
int key_weapon1;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon2__b82i = '2';  /* B82: initial value in flash */
int key_weapon2;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon3__b82i = '3';  /* B82: initial value in flash */
int key_weapon3;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon4__b82i = '4';  /* B82: initial value in flash */
int key_weapon4;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon5__b82i = '5';  /* B82: initial value in flash */
int key_weapon5;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon6__b82i = '6';  /* B82: initial value in flash */
int key_weapon6;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon7__b82i = '7';  /* B82: initial value in flash */
int key_weapon7;  /* B82: live copy, .bss -> PSRAM */
static int const key_weapon8__b82i = '8';  /* B82: initial value in flash */
int key_weapon8;  /* B82: live copy, .bss -> PSRAM */
static int const key_prevweapon__b82i = 0;  /* B82: initial value in flash */
int key_prevweapon;  /* B82: live copy, .bss -> PSRAM */
static int const key_nextweapon__b82i = 0;  /* B82: initial value in flash */
int key_nextweapon;  /* B82: live copy, .bss -> PSRAM */

static int const key_map_north__b82i = KEY_UPARROW;  /* B82: initial value in flash */
int key_map_north;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_south__b82i = KEY_DOWNARROW;  /* B82: initial value in flash */
int key_map_south;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_east__b82i = KEY_RIGHTARROW;  /* B82: initial value in flash */
int key_map_east;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_west__b82i = KEY_LEFTARROW;  /* B82: initial value in flash */
int key_map_west;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_zoomin__b82i = '=';  /* B82: initial value in flash */
int key_map_zoomin;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_zoomout__b82i = '-';  /* B82: initial value in flash */
int key_map_zoomout;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_toggle__b82i = KEY_TAB;  /* B82: initial value in flash */
int key_map_toggle;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_maxzoom__b82i = '0';  /* B82: initial value in flash */
int key_map_maxzoom;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_follow__b82i = 'f';  /* B82: initial value in flash */
int key_map_follow;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_grid__b82i = 'g';  /* B82: initial value in flash */
int key_map_grid;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_mark__b82i = 'm';  /* B82: initial value in flash */
int key_map_mark;  /* B82: live copy, .bss -> PSRAM */
static int const key_map_clearmark__b82i = 'c';  /* B82: initial value in flash */
int key_map_clearmark;  /* B82: live copy, .bss -> PSRAM */

static int const key_menu_activate__b82i = KEY_ESCAPE;  /* B82: initial value in flash */
int key_menu_activate;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_up__b82i = KEY_UPARROW;  /* B82: initial value in flash */
int key_menu_up;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_down__b82i = KEY_DOWNARROW;  /* B82: initial value in flash */
int key_menu_down;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_left__b82i = KEY_LEFTARROW;  /* B82: initial value in flash */
int key_menu_left;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_right__b82i = KEY_RIGHTARROW;  /* B82: initial value in flash */
int key_menu_right;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_back__b82i = KEY_BACKSPACE;  /* B82: initial value in flash */
int key_menu_back;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_forward__b82i = KEY_ENTER;  /* B82: initial value in flash */
int key_menu_forward;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_confirm__b82i = 'y';  /* B82: initial value in flash */
int key_menu_confirm;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_abort__b82i = 'n';  /* B82: initial value in flash */
int key_menu_abort;  /* B82: live copy, .bss -> PSRAM */

static int const key_menu_help__b82i = KEY_F1;  /* B82: initial value in flash */
int key_menu_help;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_save__b82i = KEY_F2;  /* B82: initial value in flash */
int key_menu_save;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_load__b82i = KEY_F3;  /* B82: initial value in flash */
int key_menu_load;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_volume__b82i = KEY_F4;  /* B82: initial value in flash */
int key_menu_volume;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_detail__b82i = KEY_F5;  /* B82: initial value in flash */
int key_menu_detail;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_qsave__b82i = KEY_F6;  /* B82: initial value in flash */
int key_menu_qsave;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_endgame__b82i = KEY_F7;  /* B82: initial value in flash */
int key_menu_endgame;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_messages__b82i = KEY_F8;  /* B82: initial value in flash */
int key_menu_messages;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_qload__b82i = KEY_F9;  /* B82: initial value in flash */
int key_menu_qload;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_quit__b82i = KEY_F10;  /* B82: initial value in flash */
int key_menu_quit;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_gamma__b82i = KEY_F11;  /* B82: initial value in flash */
int key_menu_gamma;  /* B82: live copy, .bss -> PSRAM */

static int const key_menu_incscreen__b82i = KEY_EQUALS;  /* B82: initial value in flash */
int key_menu_incscreen;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_decscreen__b82i = KEY_MINUS;  /* B82: initial value in flash */
int key_menu_decscreen;  /* B82: live copy, .bss -> PSRAM */
static int const key_menu_screenshot__b82i = 0;  /* B82: initial value in flash */
int key_menu_screenshot;  /* B82: live copy, .bss -> PSRAM */

static int const joybfire__b82i = 0;  /* B82: initial value in flash */
int joybfire;  /* B82: live copy, .bss -> PSRAM */
static int const joybstrafe__b82i = 1;  /* B82: initial value in flash */
int joybstrafe;  /* B82: live copy, .bss -> PSRAM */
static int const joybuse__b82i = 3;  /* B82: initial value in flash */
int joybuse;  /* B82: live copy, .bss -> PSRAM */
static int const joybspeed__b82i = 2;  /* B82: initial value in flash */
int joybspeed;  /* B82: live copy, .bss -> PSRAM */

static int const joybstrafeleft__b82i = -1;  /* B82: initial value in flash */
int joybstrafeleft;  /* B82: live copy, .bss -> PSRAM */
static int const joybstraferight__b82i = -1;  /* B82: initial value in flash */
int joybstraferight;  /* B82: live copy, .bss -> PSRAM */

static int const joybjump__b82i = -1;  /* B82: initial value in flash */
int joybjump;  /* B82: live copy, .bss -> PSRAM */

static int const joybprevweapon__b82i = -1;  /* B82: initial value in flash */
int joybprevweapon;  /* B82: live copy, .bss -> PSRAM */
static int const joybnextweapon__b82i = -1;  /* B82: initial value in flash */
int joybnextweapon;  /* B82: live copy, .bss -> PSRAM */

static int const joybmenu__b82i = -1;  /* B82: initial value in flash */
int joybmenu;  /* B82: live copy, .bss -> PSRAM */

static int const dclick_use__b82i = 1;  /* B82: initial value in flash */
int dclick_use;  /* B82: live copy, .bss -> PSRAM */
 
// 
// Bind all of the common controls used by Doom and all other games.
//

void M_BindBaseControls(void)
{
    M_BindVariable("key_right",          &key_right);
    M_BindVariable("key_left",           &key_left);
    M_BindVariable("key_up",             &key_up);
    M_BindVariable("key_down",           &key_down);
    M_BindVariable("key_strafeleft",     &key_strafeleft);
    M_BindVariable("key_straferight",    &key_straferight);
    M_BindVariable("key_fire",           &key_fire);
    M_BindVariable("key_use",            &key_use);
    M_BindVariable("key_strafe",         &key_strafe);
    M_BindVariable("key_speed",          &key_speed);

    M_BindVariable("mouseb_fire",        &mousebfire);
    M_BindVariable("mouseb_strafe",      &mousebstrafe);
    M_BindVariable("mouseb_forward",     &mousebforward);

    M_BindVariable("joyb_fire",          &joybfire);
    M_BindVariable("joyb_strafe",        &joybstrafe);
    M_BindVariable("joyb_use",           &joybuse);
    M_BindVariable("joyb_speed",         &joybspeed);

    M_BindVariable("joyb_menu_activate", &joybmenu);

    // Extra controls that are not in the Vanilla versions:

    M_BindVariable("joyb_strafeleft",    &joybstrafeleft);
    M_BindVariable("joyb_straferight",   &joybstraferight);
    M_BindVariable("mouseb_strafeleft",  &mousebstrafeleft);
    M_BindVariable("mouseb_straferight", &mousebstraferight);
    M_BindVariable("mouseb_use",         &mousebuse);
    M_BindVariable("mouseb_backward",    &mousebbackward);
    M_BindVariable("dclick_use",         &dclick_use);
    M_BindVariable("key_pause",          &key_pause);
    M_BindVariable("key_message_refresh", &key_message_refresh);
}

void M_BindHereticControls(void)
{
    M_BindVariable("key_flyup",          &key_flyup);
    M_BindVariable("key_flydown",        &key_flydown);
    M_BindVariable("key_flycenter",      &key_flycenter);

    M_BindVariable("key_lookup",         &key_lookup);
    M_BindVariable("key_lookdown",       &key_lookdown);
    M_BindVariable("key_lookcenter",     &key_lookcenter);

    M_BindVariable("key_invleft",        &key_invleft);
    M_BindVariable("key_invright",       &key_invright);
    M_BindVariable("key_useartifact",    &key_useartifact);
}

void M_BindHexenControls(void)
{
    M_BindVariable("key_jump",           &key_jump);
    M_BindVariable("mouseb_jump",        &mousebjump);
    M_BindVariable("joyb_jump",          &joybjump);

    M_BindVariable("key_arti_all",             &key_arti_all);
    M_BindVariable("key_arti_health",          &key_arti_health);
    M_BindVariable("key_arti_poisonbag",       &key_arti_poisonbag);
    M_BindVariable("key_arti_blastradius",     &key_arti_blastradius);
    M_BindVariable("key_arti_teleport",        &key_arti_teleport);
    M_BindVariable("key_arti_teleportother",   &key_arti_teleportother);
    M_BindVariable("key_arti_egg",             &key_arti_egg);
    M_BindVariable("key_arti_invulnerability", &key_arti_invulnerability);
}

void M_BindStrifeControls(void)
{
    // These are shared with all games, but have different defaults:
    key_message_refresh = '/';

    // These keys are shared with Heretic/Hexen but have different defaults:
    key_jump     = 'a';
    key_lookup   = KEY_PGUP;
    key_lookdown = KEY_PGDN;
    key_invleft  = KEY_INS;
    key_invright = KEY_DEL;

    M_BindVariable("key_jump",           &key_jump);
    M_BindVariable("key_lookUp",         &key_lookup);
    M_BindVariable("key_lookDown",       &key_lookdown);
    M_BindVariable("key_invLeft",        &key_invleft);
    M_BindVariable("key_invRight",       &key_invright);

    // Custom Strife-only Keys:
    M_BindVariable("key_useHealth",      &key_usehealth);
    M_BindVariable("key_invquery",       &key_invquery);
    M_BindVariable("key_mission",        &key_mission);
    M_BindVariable("key_invPop",         &key_invpop);
    M_BindVariable("key_invKey",         &key_invkey);
    M_BindVariable("key_invHome",        &key_invhome);
    M_BindVariable("key_invEnd",         &key_invend);
    M_BindVariable("key_invUse",         &key_invuse);
    M_BindVariable("key_invDrop",        &key_invdrop);

    // Strife also supports jump on mouse and joystick, and in the exact same
    // manner as Hexen!
    M_BindVariable("mouseb_jump",        &mousebjump);
    M_BindVariable("joyb_jump",          &joybjump);
}

void M_BindWeaponControls(void)
{
    M_BindVariable("key_weapon1",        &key_weapon1);
    M_BindVariable("key_weapon2",        &key_weapon2);
    M_BindVariable("key_weapon3",        &key_weapon3);
    M_BindVariable("key_weapon4",        &key_weapon4);
    M_BindVariable("key_weapon5",        &key_weapon5);
    M_BindVariable("key_weapon6",        &key_weapon6);
    M_BindVariable("key_weapon7",        &key_weapon7);
    M_BindVariable("key_weapon8",        &key_weapon8);

    M_BindVariable("key_prevweapon",     &key_prevweapon);
    M_BindVariable("key_nextweapon",     &key_nextweapon);

    M_BindVariable("joyb_prevweapon",    &joybprevweapon);
    M_BindVariable("joyb_nextweapon",    &joybnextweapon);

    M_BindVariable("mouseb_prevweapon",  &mousebprevweapon);
    M_BindVariable("mouseb_nextweapon",  &mousebnextweapon);
}

void M_BindMapControls(void)
{
    M_BindVariable("key_map_north",      &key_map_north);
    M_BindVariable("key_map_south",      &key_map_south);
    M_BindVariable("key_map_east",       &key_map_east);
    M_BindVariable("key_map_west",       &key_map_west);
    M_BindVariable("key_map_zoomin",     &key_map_zoomin);
    M_BindVariable("key_map_zoomout",    &key_map_zoomout);
    M_BindVariable("key_map_toggle",     &key_map_toggle);
    M_BindVariable("key_map_maxzoom",    &key_map_maxzoom);
    M_BindVariable("key_map_follow",     &key_map_follow);
    M_BindVariable("key_map_grid",       &key_map_grid);
    M_BindVariable("key_map_mark",       &key_map_mark);
    M_BindVariable("key_map_clearmark",  &key_map_clearmark);
}

void M_BindMenuControls(void)
{
    M_BindVariable("key_menu_activate",  &key_menu_activate);
    M_BindVariable("key_menu_up",        &key_menu_up);
    M_BindVariable("key_menu_down",      &key_menu_down);
    M_BindVariable("key_menu_left",      &key_menu_left);
    M_BindVariable("key_menu_right",     &key_menu_right);
    M_BindVariable("key_menu_back",      &key_menu_back);
    M_BindVariable("key_menu_forward",   &key_menu_forward);
    M_BindVariable("key_menu_confirm",   &key_menu_confirm);
    M_BindVariable("key_menu_abort",     &key_menu_abort);

    M_BindVariable("key_menu_help",      &key_menu_help);
    M_BindVariable("key_menu_save",      &key_menu_save);
    M_BindVariable("key_menu_load",      &key_menu_load);
    M_BindVariable("key_menu_volume",    &key_menu_volume);
    M_BindVariable("key_menu_detail",    &key_menu_detail);
    M_BindVariable("key_menu_qsave",     &key_menu_qsave);
    M_BindVariable("key_menu_endgame",   &key_menu_endgame);
    M_BindVariable("key_menu_messages",  &key_menu_messages);
    M_BindVariable("key_menu_qload",     &key_menu_qload);
    M_BindVariable("key_menu_quit",      &key_menu_quit);
    M_BindVariable("key_menu_gamma",     &key_menu_gamma);

    M_BindVariable("key_menu_incscreen", &key_menu_incscreen);
    M_BindVariable("key_menu_decscreen", &key_menu_decscreen);
    M_BindVariable("key_menu_screenshot",&key_menu_screenshot);
    M_BindVariable("key_demo_quit",      &key_demo_quit);
    M_BindVariable("key_spy",            &key_spy);
}

void M_BindChatControls(unsigned int num_players)
{
    char name[32];  // haleyjd: 20 not large enough - Thank you, come again!
    unsigned int i; // haleyjd: signedness conflict

    M_BindVariable("key_multi_msg",     &key_multi_msg);

    for (i=0; i<num_players; ++i)
    {
        M_snprintf(name, sizeof(name), "key_multi_msgplayer%i", i + 1);
        M_BindVariable(name, &key_multi_msgplayer[i]);
    }
}

//
// Apply custom patches to the default values depending on the
// platform we are running on.
//

void M_ApplyPlatformDefaults(void)
{
    // no-op. Add your platform-specific patches here.
}


/* B82_DATA2PSRAM: generated by lab:LEAD/B82/tools/b82_data2psram.py */
#include <string.h>
void b82i_m_controls(void)
{
    memcpy((void *)&key_right, (const void *)&key_right__b82i, sizeof key_right__b82i);
    memcpy((void *)&key_left, (const void *)&key_left__b82i, sizeof key_left__b82i);
    memcpy((void *)&key_up, (const void *)&key_up__b82i, sizeof key_up__b82i);
    memcpy((void *)&key_down, (const void *)&key_down__b82i, sizeof key_down__b82i);
    memcpy((void *)&key_strafeleft, (const void *)&key_strafeleft__b82i, sizeof key_strafeleft__b82i);
    memcpy((void *)&key_straferight, (const void *)&key_straferight__b82i, sizeof key_straferight__b82i);
    memcpy((void *)&key_fire, (const void *)&key_fire__b82i, sizeof key_fire__b82i);
    memcpy((void *)&key_use, (const void *)&key_use__b82i, sizeof key_use__b82i);
    memcpy((void *)&key_strafe, (const void *)&key_strafe__b82i, sizeof key_strafe__b82i);
    memcpy((void *)&key_speed, (const void *)&key_speed__b82i, sizeof key_speed__b82i);
    memcpy((void *)&key_flyup, (const void *)&key_flyup__b82i, sizeof key_flyup__b82i);
    memcpy((void *)&key_flydown, (const void *)&key_flydown__b82i, sizeof key_flydown__b82i);
    memcpy((void *)&key_flycenter, (const void *)&key_flycenter__b82i, sizeof key_flycenter__b82i);
    memcpy((void *)&key_lookup, (const void *)&key_lookup__b82i, sizeof key_lookup__b82i);
    memcpy((void *)&key_lookdown, (const void *)&key_lookdown__b82i, sizeof key_lookdown__b82i);
    memcpy((void *)&key_lookcenter, (const void *)&key_lookcenter__b82i, sizeof key_lookcenter__b82i);
    memcpy((void *)&key_invleft, (const void *)&key_invleft__b82i, sizeof key_invleft__b82i);
    memcpy((void *)&key_invright, (const void *)&key_invright__b82i, sizeof key_invright__b82i);
    memcpy((void *)&key_useartifact, (const void *)&key_useartifact__b82i, sizeof key_useartifact__b82i);
    memcpy((void *)&key_jump, (const void *)&key_jump__b82i, sizeof key_jump__b82i);
    memcpy((void *)&key_arti_all, (const void *)&key_arti_all__b82i, sizeof key_arti_all__b82i);
    memcpy((void *)&key_arti_health, (const void *)&key_arti_health__b82i, sizeof key_arti_health__b82i);
    memcpy((void *)&key_arti_poisonbag, (const void *)&key_arti_poisonbag__b82i, sizeof key_arti_poisonbag__b82i);
    memcpy((void *)&key_arti_blastradius, (const void *)&key_arti_blastradius__b82i, sizeof key_arti_blastradius__b82i);
    memcpy((void *)&key_arti_teleport, (const void *)&key_arti_teleport__b82i, sizeof key_arti_teleport__b82i);
    memcpy((void *)&key_arti_teleportother, (const void *)&key_arti_teleportother__b82i, sizeof key_arti_teleportother__b82i);
    memcpy((void *)&key_arti_egg, (const void *)&key_arti_egg__b82i, sizeof key_arti_egg__b82i);
    memcpy((void *)&key_arti_invulnerability, (const void *)&key_arti_invulnerability__b82i, sizeof key_arti_invulnerability__b82i);
    memcpy((void *)&key_usehealth, (const void *)&key_usehealth__b82i, sizeof key_usehealth__b82i);
    memcpy((void *)&key_invquery, (const void *)&key_invquery__b82i, sizeof key_invquery__b82i);
    memcpy((void *)&key_mission, (const void *)&key_mission__b82i, sizeof key_mission__b82i);
    memcpy((void *)&key_invpop, (const void *)&key_invpop__b82i, sizeof key_invpop__b82i);
    memcpy((void *)&key_invkey, (const void *)&key_invkey__b82i, sizeof key_invkey__b82i);
    memcpy((void *)&key_invhome, (const void *)&key_invhome__b82i, sizeof key_invhome__b82i);
    memcpy((void *)&key_invend, (const void *)&key_invend__b82i, sizeof key_invend__b82i);
    memcpy((void *)&key_invuse, (const void *)&key_invuse__b82i, sizeof key_invuse__b82i);
    memcpy((void *)&key_invdrop, (const void *)&key_invdrop__b82i, sizeof key_invdrop__b82i);
    memcpy((void *)&mousebfire, (const void *)&mousebfire__b82i, sizeof mousebfire__b82i);
    memcpy((void *)&mousebstrafe, (const void *)&mousebstrafe__b82i, sizeof mousebstrafe__b82i);
    memcpy((void *)&mousebforward, (const void *)&mousebforward__b82i, sizeof mousebforward__b82i);
    memcpy((void *)&mousebjump, (const void *)&mousebjump__b82i, sizeof mousebjump__b82i);
    memcpy((void *)&mousebstrafeleft, (const void *)&mousebstrafeleft__b82i, sizeof mousebstrafeleft__b82i);
    memcpy((void *)&mousebstraferight, (const void *)&mousebstraferight__b82i, sizeof mousebstraferight__b82i);
    memcpy((void *)&mousebbackward, (const void *)&mousebbackward__b82i, sizeof mousebbackward__b82i);
    memcpy((void *)&mousebuse, (const void *)&mousebuse__b82i, sizeof mousebuse__b82i);
    memcpy((void *)&mousebprevweapon, (const void *)&mousebprevweapon__b82i, sizeof mousebprevweapon__b82i);
    memcpy((void *)&mousebnextweapon, (const void *)&mousebnextweapon__b82i, sizeof mousebnextweapon__b82i);
    memcpy((void *)&key_message_refresh, (const void *)&key_message_refresh__b82i, sizeof key_message_refresh__b82i);
    memcpy((void *)&key_pause, (const void *)&key_pause__b82i, sizeof key_pause__b82i);
    memcpy((void *)&key_demo_quit, (const void *)&key_demo_quit__b82i, sizeof key_demo_quit__b82i);
    memcpy((void *)&key_spy, (const void *)&key_spy__b82i, sizeof key_spy__b82i);
    memcpy((void *)&key_multi_msg, (const void *)&key_multi_msg__b82i, sizeof key_multi_msg__b82i);
    memcpy((void *)&key_weapon1, (const void *)&key_weapon1__b82i, sizeof key_weapon1__b82i);
    memcpy((void *)&key_weapon2, (const void *)&key_weapon2__b82i, sizeof key_weapon2__b82i);
    memcpy((void *)&key_weapon3, (const void *)&key_weapon3__b82i, sizeof key_weapon3__b82i);
    memcpy((void *)&key_weapon4, (const void *)&key_weapon4__b82i, sizeof key_weapon4__b82i);
    memcpy((void *)&key_weapon5, (const void *)&key_weapon5__b82i, sizeof key_weapon5__b82i);
    memcpy((void *)&key_weapon6, (const void *)&key_weapon6__b82i, sizeof key_weapon6__b82i);
    memcpy((void *)&key_weapon7, (const void *)&key_weapon7__b82i, sizeof key_weapon7__b82i);
    memcpy((void *)&key_weapon8, (const void *)&key_weapon8__b82i, sizeof key_weapon8__b82i);
    memcpy((void *)&key_prevweapon, (const void *)&key_prevweapon__b82i, sizeof key_prevweapon__b82i);
    memcpy((void *)&key_nextweapon, (const void *)&key_nextweapon__b82i, sizeof key_nextweapon__b82i);
    memcpy((void *)&key_map_north, (const void *)&key_map_north__b82i, sizeof key_map_north__b82i);
    memcpy((void *)&key_map_south, (const void *)&key_map_south__b82i, sizeof key_map_south__b82i);
    memcpy((void *)&key_map_east, (const void *)&key_map_east__b82i, sizeof key_map_east__b82i);
    memcpy((void *)&key_map_west, (const void *)&key_map_west__b82i, sizeof key_map_west__b82i);
    memcpy((void *)&key_map_zoomin, (const void *)&key_map_zoomin__b82i, sizeof key_map_zoomin__b82i);
    memcpy((void *)&key_map_zoomout, (const void *)&key_map_zoomout__b82i, sizeof key_map_zoomout__b82i);
    memcpy((void *)&key_map_toggle, (const void *)&key_map_toggle__b82i, sizeof key_map_toggle__b82i);
    memcpy((void *)&key_map_maxzoom, (const void *)&key_map_maxzoom__b82i, sizeof key_map_maxzoom__b82i);
    memcpy((void *)&key_map_follow, (const void *)&key_map_follow__b82i, sizeof key_map_follow__b82i);
    memcpy((void *)&key_map_grid, (const void *)&key_map_grid__b82i, sizeof key_map_grid__b82i);
    memcpy((void *)&key_map_mark, (const void *)&key_map_mark__b82i, sizeof key_map_mark__b82i);
    memcpy((void *)&key_map_clearmark, (const void *)&key_map_clearmark__b82i, sizeof key_map_clearmark__b82i);
    memcpy((void *)&key_menu_activate, (const void *)&key_menu_activate__b82i, sizeof key_menu_activate__b82i);
    memcpy((void *)&key_menu_up, (const void *)&key_menu_up__b82i, sizeof key_menu_up__b82i);
    memcpy((void *)&key_menu_down, (const void *)&key_menu_down__b82i, sizeof key_menu_down__b82i);
    memcpy((void *)&key_menu_left, (const void *)&key_menu_left__b82i, sizeof key_menu_left__b82i);
    memcpy((void *)&key_menu_right, (const void *)&key_menu_right__b82i, sizeof key_menu_right__b82i);
    memcpy((void *)&key_menu_back, (const void *)&key_menu_back__b82i, sizeof key_menu_back__b82i);
    memcpy((void *)&key_menu_forward, (const void *)&key_menu_forward__b82i, sizeof key_menu_forward__b82i);
    memcpy((void *)&key_menu_confirm, (const void *)&key_menu_confirm__b82i, sizeof key_menu_confirm__b82i);
    memcpy((void *)&key_menu_abort, (const void *)&key_menu_abort__b82i, sizeof key_menu_abort__b82i);
    memcpy((void *)&key_menu_help, (const void *)&key_menu_help__b82i, sizeof key_menu_help__b82i);
    memcpy((void *)&key_menu_save, (const void *)&key_menu_save__b82i, sizeof key_menu_save__b82i);
    memcpy((void *)&key_menu_load, (const void *)&key_menu_load__b82i, sizeof key_menu_load__b82i);
    memcpy((void *)&key_menu_volume, (const void *)&key_menu_volume__b82i, sizeof key_menu_volume__b82i);
    memcpy((void *)&key_menu_detail, (const void *)&key_menu_detail__b82i, sizeof key_menu_detail__b82i);
    memcpy((void *)&key_menu_qsave, (const void *)&key_menu_qsave__b82i, sizeof key_menu_qsave__b82i);
    memcpy((void *)&key_menu_endgame, (const void *)&key_menu_endgame__b82i, sizeof key_menu_endgame__b82i);
    memcpy((void *)&key_menu_messages, (const void *)&key_menu_messages__b82i, sizeof key_menu_messages__b82i);
    memcpy((void *)&key_menu_qload, (const void *)&key_menu_qload__b82i, sizeof key_menu_qload__b82i);
    memcpy((void *)&key_menu_quit, (const void *)&key_menu_quit__b82i, sizeof key_menu_quit__b82i);
    memcpy((void *)&key_menu_gamma, (const void *)&key_menu_gamma__b82i, sizeof key_menu_gamma__b82i);
    memcpy((void *)&key_menu_incscreen, (const void *)&key_menu_incscreen__b82i, sizeof key_menu_incscreen__b82i);
    memcpy((void *)&key_menu_decscreen, (const void *)&key_menu_decscreen__b82i, sizeof key_menu_decscreen__b82i);
    memcpy((void *)&key_menu_screenshot, (const void *)&key_menu_screenshot__b82i, sizeof key_menu_screenshot__b82i);
    memcpy((void *)&joybfire, (const void *)&joybfire__b82i, sizeof joybfire__b82i);
    memcpy((void *)&joybstrafe, (const void *)&joybstrafe__b82i, sizeof joybstrafe__b82i);
    memcpy((void *)&joybuse, (const void *)&joybuse__b82i, sizeof joybuse__b82i);
    memcpy((void *)&joybspeed, (const void *)&joybspeed__b82i, sizeof joybspeed__b82i);
    memcpy((void *)&joybstrafeleft, (const void *)&joybstrafeleft__b82i, sizeof joybstrafeleft__b82i);
    memcpy((void *)&joybstraferight, (const void *)&joybstraferight__b82i, sizeof joybstraferight__b82i);
    memcpy((void *)&joybjump, (const void *)&joybjump__b82i, sizeof joybjump__b82i);
    memcpy((void *)&joybprevweapon, (const void *)&joybprevweapon__b82i, sizeof joybprevweapon__b82i);
    memcpy((void *)&joybnextweapon, (const void *)&joybnextweapon__b82i, sizeof joybnextweapon__b82i);
    memcpy((void *)&joybmenu, (const void *)&joybmenu__b82i, sizeof joybmenu__b82i);
    memcpy((void *)&dclick_use, (const void *)&dclick_use__b82i, sizeof dclick_use__b82i);
}
