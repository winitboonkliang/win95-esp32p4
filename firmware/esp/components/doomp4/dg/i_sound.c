//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:  none
//

#include <stdio.h>
#include <stdlib.h>

// ESP32-P4 port: no SDL — the sound backend is DG_sound_module in
// main/doom_sound.cpp (ES8311 codec).  SDL_mixer include removed.

#include "config.h"
#include "doomfeatures.h"
#include "doomtype.h"

#ifdef ORIGCODE
#include "gusconf.h"
#endif
#include "i_sound.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"

static int const snd_samplerate__b82i = 44100;  /* B82: initial value in flash */
int snd_samplerate;  /* B82: live copy, .bss -> PSRAM */

static int const snd_cachesize__b82i = 64 * 1024 * 1024;  /* B82: initial value in flash */
int snd_cachesize;  /* B82: live copy, .bss -> PSRAM */

static int const snd_maxslicetime_ms__b82i = 28;  /* B82: initial value in flash */
int snd_maxslicetime_ms;  /* B82: live copy, .bss -> PSRAM */

static char * const snd_musiccmd__b82i = "";  /* B82: initial value in flash */
char *snd_musiccmd;  /* B82: live copy, .bss -> PSRAM */

static sound_module_t * const sound_module__b82i = NULL;  /* B82: initial value in flash */
static sound_module_t *sound_module;  /* B82: live copy, .bss -> PSRAM */
static music_module_t * const music_module__b82i = NULL;  /* B82: initial value in flash */
static music_module_t *music_module;  /* B82: live copy, .bss -> PSRAM */

static int const snd_musicdevice__b82i = SNDDEVICE_SB;  /* B82: initial value in flash */
int snd_musicdevice;  /* B82: live copy, .bss -> PSRAM */
static int const snd_sfxdevice__b82i = SNDDEVICE_SB;  /* B82: initial value in flash */
int snd_sfxdevice;  /* B82: live copy, .bss -> PSRAM */

static int const snd_sbport__b82i = 0;  /* B82: initial value in flash */
static int snd_sbport;  /* B82: live copy, .bss -> PSRAM */
static int const snd_sbirq__b82i = 0;  /* B82: initial value in flash */
static int snd_sbirq;  /* B82: live copy, .bss -> PSRAM */
static int const snd_sbdma__b82i = 0;  /* B82: initial value in flash */
static int snd_sbdma;  /* B82: live copy, .bss -> PSRAM */
static int const snd_mport__b82i = 0;  /* B82: initial value in flash */
static int snd_mport;  /* B82: live copy, .bss -> PSRAM */

static sound_module_t * const sound_modules__b82i[] = {
    #ifdef FEATURE_SOUND
    &DG_sound_module,
    #endif
    NULL,
};  /* B82: initial value in flash */
static sound_module_t *sound_modules[sizeof sound_modules__b82i / sizeof sound_modules__b82i[0]];  /* B82: live copy, .bss -> PSRAM */

// Check if a sound device is in the given list of devices

static boolean SndDeviceInList(snddevice_t device, snddevice_t *list,
                               int len)
{
    int i;

    for (i=0; i<len; ++i)
    {
        if (device == list[i])
        {
            return true;
        }
    }

    return false;
}

// Find and initialize a sound_module_t appropriate for the setting
// in snd_sfxdevice.

static void InitSfxModule(boolean use_sfx_prefix)
{
    int i;

    sound_module = NULL;

    for (i=0; sound_modules[i] != NULL; ++i)
    {
        // Is the sfx device in the list of devices supported by
        // this module?

        if (SndDeviceInList(snd_sfxdevice, 
                            sound_modules[i]->sound_devices,
                            sound_modules[i]->num_sound_devices))
        {
            // Initialize the module

            if (sound_modules[i]->Init(use_sfx_prefix))
            {
                sound_module = sound_modules[i];
                return;
            }
        }
    }
}

// Initialize music according to snd_musicdevice.

static void InitMusicModule(void)
{
#ifdef FEATURE_SOUND
    music_module = &DG_music_module;
#endif /* FEATURE_SOUND */
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void I_InitSound(boolean use_sfx_prefix)
{  
    boolean nosound, nosfx, nomusic;

    //!
    // @vanilla
    //
    // Disable all sound output.
    //

    nosound = M_CheckParm("-nosound") > 0;
    if (snd_sfxdevice == SNDDEVICE_NONE) snd_sfxdevice = SNDDEVICE_SB;   /* B84: a default.cfg written by a soundless build must not mute us */
    if (snd_musicdevice == SNDDEVICE_NONE) snd_musicdevice = SNDDEVICE_SB;
    printf("I_InitSound: sfx device %d music device %d%s\n", snd_sfxdevice, snd_musicdevice, nosound ? " (-nosound)" : "");

    //!
    // @vanilla
    //
    // Disable sound effects. 
    //

    nosfx = M_CheckParm("-nosfx") > 0;

    //!
    // @vanilla
    //
    // Disable music.
    //

    nomusic = M_CheckParm("-nomusic") > 0;

    // Initialize the sound and music subsystems.

    if (!nosound && !screensaver_mode)
    {
        // This is kind of a hack. If native MIDI is enabled, set up
        // the TIMIDITY_CFG environment variable here before SDL_mixer
        // is opened.

        if (!nomusic
         && (snd_musicdevice == SNDDEVICE_GENMIDI
          || snd_musicdevice == SNDDEVICE_GUS))
        {
            //I_InitTimidityConfig();
        }

        if (!nosfx)
        {
            InitSfxModule(use_sfx_prefix);
        }

        if (!nomusic)
        {
            InitMusicModule();
        }
    }

}

void I_ShutdownSound(void)
{
    if (sound_module != NULL)
    {
        sound_module->Shutdown();
    }

    if (music_module != NULL)
    {
        music_module->Shutdown();
    }
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    if (sound_module != NULL) 
    {
        return sound_module->GetSfxLumpNum(sfxinfo);
    }
    else
    {
        return 0;
    }
}

void I_UpdateSound(void)
{
    if (sound_module != NULL)
    {
        sound_module->Update();
    }

    if (music_module != NULL && music_module->Poll != NULL)
    {
        music_module->Poll();
    }
}

static void CheckVolumeSeparation(int *vol, int *sep)
{
    if (*sep < 0)
    {
        *sep = 0;
    }
    else if (*sep > 254)
    {
        *sep = 254;
    }

    if (*vol < 0)
    {
        *vol = 0;
    }
    else if (*vol > 127)
    {
        *vol = 127;
    }
}

void I_UpdateSoundParams(int channel, int vol, int sep)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        sound_module->UpdateSoundParams(channel, vol, sep);
    }
}

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        return sound_module->StartSound(sfxinfo, channel, vol, sep);
    }
    else
    {
        return 0;
    }
}

void I_StopSound(int channel)
{
    if (sound_module != NULL)
    {
        sound_module->StopSound(channel);
    }
}

boolean I_SoundIsPlaying(int channel)
{
    if (sound_module != NULL)
    {
        return sound_module->SoundIsPlaying(channel);
    }
    else
    {
        return false;
    }
}

void I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    if (sound_module != NULL && sound_module->CacheSounds != NULL)
    {
	sound_module->CacheSounds(sounds, num_sounds);
    }
}

void I_InitMusic(void)
{
    if(music_module != NULL)
    {
        music_module->Init();
    }
}

void I_ShutdownMusic(void)
{

}

void I_SetMusicVolume(int volume)
{
    if (music_module != NULL)
    {
        music_module->SetMusicVolume(volume);
    }
}

void I_PauseSong(void)
{
    if (music_module != NULL)
    {
        music_module->PauseMusic();
    }
}

void I_ResumeSong(void)
{
    if (music_module != NULL)
    {
        music_module->ResumeMusic();
    }
}

void *I_RegisterSong(void *data, int len)
{
    if (music_module != NULL)
    {
        return music_module->RegisterSong(data, len);
    }
    else
    {
        return NULL;
    }
}

void I_UnRegisterSong(void *handle)
{
    if (music_module != NULL)
    {
        music_module->UnRegisterSong(handle);
    }
}

void I_PlaySong(void *handle, boolean looping)
{
    if (music_module != NULL)
    {
        music_module->PlaySong(handle, looping);
    }
}

void I_StopSong(void)
{
    if (music_module != NULL)
    {
        music_module->StopSong();
    }
}

boolean I_MusicIsPlaying(void)
{
    if (music_module != NULL)
    {
        return music_module->MusicIsPlaying();
    }
    else
    {
        return false;
    }
    
}

void I_BindSoundVariables(void)
{
    extern int use_libsamplerate;
    extern float libsamplerate_scale;

    M_BindVariable("snd_musicdevice",   &snd_musicdevice);
    M_BindVariable("snd_sfxdevice",     &snd_sfxdevice);
    M_BindVariable("snd_sbport",        &snd_sbport);
    M_BindVariable("snd_sbirq",         &snd_sbirq);
    M_BindVariable("snd_sbdma",         &snd_sbdma);
    M_BindVariable("snd_mport",         &snd_mport);
    M_BindVariable("snd_maxslicetime_ms", &snd_maxslicetime_ms);
    M_BindVariable("snd_musiccmd",      &snd_musiccmd);
    M_BindVariable("snd_samplerate",    &snd_samplerate);
    M_BindVariable("snd_cachesize",     &snd_cachesize);

#ifdef FEATURE_SOUND
    M_BindVariable("use_libsamplerate",   &use_libsamplerate);
    M_BindVariable("libsamplerate_scale", &libsamplerate_scale);
#endif

    // Before SDL_mixer version 1.2.11, MIDI music caused the game
    // to crash when it looped.  If this is an old SDL_mixer version,
    // disable MIDI.
}


/* B82_DATA2PSRAM: generated by lab:LEAD/B82/tools/b82_data2psram.py */
#include <string.h>
void b82i_i_sound(void)
{
    memcpy((void *)&snd_samplerate, (const void *)&snd_samplerate__b82i, sizeof snd_samplerate__b82i);
    memcpy((void *)&snd_cachesize, (const void *)&snd_cachesize__b82i, sizeof snd_cachesize__b82i);
    memcpy((void *)&snd_maxslicetime_ms, (const void *)&snd_maxslicetime_ms__b82i, sizeof snd_maxslicetime_ms__b82i);
    memcpy((void *)&snd_musiccmd, (const void *)&snd_musiccmd__b82i, sizeof snd_musiccmd__b82i);
    memcpy((void *)&sound_module, (const void *)&sound_module__b82i, sizeof sound_module__b82i);
    memcpy((void *)&music_module, (const void *)&music_module__b82i, sizeof music_module__b82i);
    memcpy((void *)&snd_musicdevice, (const void *)&snd_musicdevice__b82i, sizeof snd_musicdevice__b82i);
    memcpy((void *)&snd_sfxdevice, (const void *)&snd_sfxdevice__b82i, sizeof snd_sfxdevice__b82i);
    memcpy((void *)&snd_sbport, (const void *)&snd_sbport__b82i, sizeof snd_sbport__b82i);
    memcpy((void *)&snd_sbirq, (const void *)&snd_sbirq__b82i, sizeof snd_sbirq__b82i);
    memcpy((void *)&snd_sbdma, (const void *)&snd_sbdma__b82i, sizeof snd_sbdma__b82i);
    memcpy((void *)&snd_mport, (const void *)&snd_mport__b82i, sizeof snd_mport__b82i);
    memcpy((void *)&sound_modules, (const void *)&sound_modules__b82i, sizeof sound_modules__b82i);
}
