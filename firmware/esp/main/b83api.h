/* B83: the engine data and calls B85 (b85.c) uses, reached through the engine of this launch (C engine = plain names, Rust engine =
 * rs_* symbols of libdoomrs_p.a; the Doom app holds both, the Windows image none). Include AFTER the engine headers: the names below
 * then become table accesses, so b85.c keeps its own code. The tables live in b82doom.c (b83_api = the engine's one). */
#pragma once
#include "doomstat.h"
#include "d_event.h"
#include "g_game.h"
#include "w_wad.h"
#include "z_zone.h"

typedef struct {
	boolean *menuactive; int *messageToPrint; player_t *players; int *consoleplayer; gamestate_t *gamestate; char **savegamedir;
	int *joybfire, *joybuse, *joybspeed, *joybstrafe, *joybprevweapon, *joybnextweapon, *joybmenu;
	int *mousebfire, *mousebuse, *mousebstrafe, *mousebforward;
	int *gameepisode, *gamemap, *leveltime, *gametic; boolean *timingdemo, *demoplayback;
	void (*D_PostEvent)(event_t *); void (*M_SaveDefaults)(void); void (*G_SaveGame)(int, char *); void (*G_LoadGame)(char *);
	void (*G_DeferedInitNew)(skill_t, int, int); char *(*P_SaveGameFile)(int);
	int (*W_CheckNumForName)(char *); int (*W_GetNumForName)(char *); void *(*W_CacheLumpNum)(int, int); int (*W_LumpLength)(unsigned int);
	void *(*Z_Malloc)(int, int, void *);
} B83Api;
extern const B83Api *b83_api;

#ifndef B83_API_IMPL
#define menuactive       (*b83_api->menuactive)
#define messageToPrint   (*b83_api->messageToPrint)
#define players          (b83_api->players)
#define consoleplayer    (*b83_api->consoleplayer)
#define gamestate        (*b83_api->gamestate)
#define savegamedir      (*b83_api->savegamedir)
#define joybfire         (*b83_api->joybfire)
#define joybuse          (*b83_api->joybuse)
#define joybspeed        (*b83_api->joybspeed)
#define joybstrafe       (*b83_api->joybstrafe)
#define joybprevweapon   (*b83_api->joybprevweapon)
#define joybnextweapon   (*b83_api->joybnextweapon)
#define joybmenu         (*b83_api->joybmenu)
#define mousebfire       (*b83_api->mousebfire)
#define mousebuse        (*b83_api->mousebuse)
#define mousebstrafe     (*b83_api->mousebstrafe)
#define mousebforward    (*b83_api->mousebforward)
#define gameepisode      (*b83_api->gameepisode)
#define gamemap          (*b83_api->gamemap)
#define leveltime        (*b83_api->leveltime)
#define gametic          (*b83_api->gametic)
#define timingdemo       (*b83_api->timingdemo)
#define demoplayback     (*b83_api->demoplayback)
#define D_PostEvent      (b83_api->D_PostEvent)
#define M_SaveDefaults   (b83_api->M_SaveDefaults)
#define G_SaveGame       (b83_api->G_SaveGame)
#define G_LoadGame       (b83_api->G_LoadGame)
#define G_DeferedInitNew (b83_api->G_DeferedInitNew)
#define P_SaveGameFile   (b83_api->P_SaveGameFile)
#define W_CheckNumForName (b83_api->W_CheckNumForName)
#define W_GetNumForName  (b83_api->W_GetNumForName)
#define W_CacheLumpNum   (b83_api->W_CacheLumpNum)
#define W_LumpLength     (b83_api->W_LumpLength)
#define Z_Malloc         (b83_api->Z_Malloc)
#endif
