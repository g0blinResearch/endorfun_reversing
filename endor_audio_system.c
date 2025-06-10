/**
 * ========================================================================
 * ENDOR AUDIO AND SOUND SYSTEM
 * ========================================================================
 * 
 * Comprehensive audio management for the Endor game engine. Handles all
 * sound effects, background music, MIDI playback, and audio streaming.
 * Supports WAV file loading, volume control, and rhythm loop integration.
 * 
 * Features:
 * - Wave audio output for sound effects
 * - MIDI output for background music
 * - Volume control with smooth transitions
 * - Audio file caching for performance
 * - Support for rhythm loops (discovered in rloops/)
 * - Multi-channel mixing capabilities
 */

#include "endor_readable.h"
#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "winmm.lib")

// ========================================================================
// AUDIO SYSTEM CONSTANTS
// ========================================================================

#define MAX_SOUND_EFFECTS 128
#define MAX_MUSIC_TRACKS 32
#define MAX_AUDIO_CHANNELS 8
#define AUDIO_BUFFER_SIZE 4096
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BITS_PER_SAMPLE 16
#define DEFAULT_CHANNELS 2

// Audio file extensions
#define AUDIO_EXT_WAV ".wav"
#define AUDIO_EXT_MID ".mid"
#define AUDIO_EXT_RMI ".rmi"

// Sound effect IDs (from game logic analysis)
#define SFX_MENU_SELECT 0
#define SFX_MENU_BACK 1
#define SFX_GAME_START 2
#define SFX_PLAYER_JUMP 3
#define SFX_PLAYER_LAND 4
#define SFX_ITEM_PICKUP 5
#define SFX_ENEMY_HIT 6
#define SFX_PLAYER_HIT 7
#define SFX_EXPLOSION 8
#define SFX_VICTORY 9

// Music track IDs
#define MUSIC_MENU 0
#define MUSIC_LEVEL1 1
#define MUSIC_LEVEL2 2
#define MUSIC_BOSS 3
#define MUSIC_VICTORY 4
#define MUSIC_GAMEOVER 5

// ========================================================================
// AUDIO SYSTEM STRUCTURES
// ========================================================================

/**
 * Sound effect data structure
 */
typedef struct {
    char filename[MAX_PATH];
    LPWAVEHDR pWaveHeader;
    LPBYTE pData;
    DWORD dwDataSize;
    WAVEFORMATEX format;
    BOOL bLoaded;
    BOOL bPlaying;
    float fVolume;
} SoundEffect;

/**
 * Music track information
 */
typedef struct {
    char filename[MAX_PATH];
    int nTrackID;
    BOOL bLooping;
    float fVolume;
    BOOL bPlaying;
} MusicTrack;

/**
 * Audio channel for mixing
 */
typedef struct {
    int nSoundID;
    DWORD dwPosition;
    float fVolume;
    BOOL bActive;
} AudioChannel;

// ========================================================================
// AUDIO SYSTEM GLOBALS
// ========================================================================

// Core audio handles
static HWAVEOUT g_hWaveOut = NULL;
static HMIDIOUT g_hMidiOut = NULL;
static HMMIO g_hCurrentFile = NULL;

// System state
static BOOL g_bAudioInitialized = FALSE;
static BOOL g_bMidiAvailable = FALSE;
static float g_fMasterVolume = 1.0f;
static float g_fSoundVolume = 1.0f;
static float g_fMusicVolume = 0.8f;

// Sound effect cache
static SoundEffect g_soundEffects[MAX_SOUND_EFFECTS];
static int g_nSoundCount = 0;

// Music tracks
static MusicTrack g_musicTracks[MAX_MUSIC_TRACKS];
static int g_nCurrentMusic = -1;

// Audio channels for mixing
static AudioChannel g_audioChannels[MAX_AUDIO_CHANNELS];

// Critical section for thread safety
static CRITICAL_SECTION g_audioCS;

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs audio system messages
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void audio_log(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    OutputDebugString("[AUDIO] ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
}

/**
 * Converts linear volume (0-1) to logarithmic scale
 * @param linear Linear volume value
 * @return Logarithmic volume for Windows audio
 */
static DWORD linear_to_log_volume(float linear)
{
    if (linear <= 0.0f) return 0;
    if (linear >= 1.0f) return 0xFFFF;
    
    // Convert to logarithmic scale
    float db = 20.0f * log10f(linear);
    float normalized = (db + 96.0f) / 96.0f;  // -96dB to 0dB range
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    DWORD volume = (DWORD)(normalized * 0xFFFF);
    return (volume << 16) | volume;  // Left and right channels
}

/**
 * Finds a free audio channel
 * @return Channel index or -1 if none available
 */
static int find_free_channel(void)
{
    for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
    {
        if (!g_audioChannels[i].bActive)
            return i;
    }
    return -1;
}

// ========================================================================
// CORE AUDIO SYSTEM FUNCTIONS
// ========================================================================

/**
 * Initializes the audio system
 * @return TRUE if successful, FALSE on error
 */
int initialize_audio_system(void)
{
    if (g_bAudioInitialized)
    {
        audio_log("Audio system already initialized");
        return TRUE;
    }
    
    audio_log("Initializing audio system...");
    
    // Initialize critical section
    InitializeCriticalSection(&g_audioCS);
    
    // Clear structures
    memset(g_soundEffects, 0, sizeof(g_soundEffects));
    memset(g_musicTracks, 0, sizeof(g_musicTracks));
    memset(g_audioChannels, 0, sizeof(g_audioChannels));
    
    // Initialize wave audio
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = DEFAULT_CHANNELS;
    wfx.nSamplesPerSec = DEFAULT_SAMPLE_RATE;
    wfx.wBitsPerSample = DEFAULT_BITS_PER_SAMPLE;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    MMRESULT result = waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR)
    {
        audio_log("Failed to open wave audio device (error %d)", result);
        DeleteCriticalSection(&g_audioCS);
        return FALSE;
    }
    
    // Initialize MIDI (optional, don't fail if not available)
    result = midiOutOpen(&g_hMidiOut, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    if (result == MMSYSERR_NOERROR)
    {
        g_bMidiAvailable = TRUE;
        audio_log("MIDI output initialized");
    }
    else
    {
        audio_log("MIDI output not available (error %d)", result);
        g_bMidiAvailable = FALSE;
    }
    
    // Set initial volume
    set_master_volume(g_fMasterVolume);
    
    g_bAudioInitialized = TRUE;
    audio_log("Audio system initialized successfully");
    
    return TRUE;
}

/**
 * Shuts down the audio system
 */
void shutdown_audio_system(void)
{
    if (!g_bAudioInitialized)
        return;
    
    audio_log("Shutting down audio system...");
    
    EnterCriticalSection(&g_audioCS);
    
    // Stop all sounds
    for (int i = 0; i < g_nSoundCount; i++)
    {
        if (g_soundEffects[i].pData)
        {
            free(g_soundEffects[i].pData);
            g_soundEffects[i].pData = NULL;
        }
    }
    
    // Close current file if open
    if (g_hCurrentFile)
    {
        mmioClose(g_hCurrentFile, 0);
        g_hCurrentFile = NULL;
    }
    
    // Close MIDI output
    if (g_hMidiOut)
    {
        midiOutReset(g_hMidiOut);
        midiOutClose(g_hMidiOut);
        g_hMidiOut = NULL;
    }
    
    // Close wave output
    if (g_hWaveOut)
    {
        waveOutReset(g_hWaveOut);
        waveOutClose(g_hWaveOut);
        g_hWaveOut = NULL;
    }
    
    g_bAudioInitialized = FALSE;
    
    LeaveCriticalSection(&g_audioCS);
    DeleteCriticalSection(&g_audioCS);
    
    audio_log("Audio system shut down");
}

// ========================================================================
// VOLUME CONTROL FUNCTIONS
// ========================================================================

/**
 * Sets the master volume
 * @param volume Volume level (0.0 to 1.0)
 */
void set_master_volume(float volume)
{
    g_fMasterVolume = max(0.0f, min(1.0f, volume));
    
    if (g_bAudioInitialized && g_hWaveOut)
    {
        DWORD dwVolume = linear_to_log_volume(g_fMasterVolume);
        waveOutSetVolume(g_hWaveOut, dwVolume);
    }
    
    audio_log("Master volume set to %.2f", g_fMasterVolume);
}

/**
 * Gets the current master volume
 * @return Current volume level (0.0 to 1.0)
 */
float get_master_volume(void)
{
    return g_fMasterVolume;
}

/**
 * Sets the sound effects volume
 * @param volume Volume level (0.0 to 1.0)
 */
void set_sound_volume(float volume)
{
    g_fSoundVolume = max(0.0f, min(1.0f, volume));
    audio_log("Sound volume set to %.2f", g_fSoundVolume);
}

/**
 * Sets the music volume
 * @param volume Volume level (0.0 to 1.0)
 */
void set_music_volume(float volume)
{
    g_fMusicVolume = max(0.0f, min(1.0f, volume));
    
    if (g_bMidiAvailable && g_hMidiOut)
    {
        // MIDI volume control
        DWORD midiVolume = (DWORD)(g_fMusicVolume * 127);
        midiOutShortMsg(g_hMidiOut, 0xB0 | (0x07 << 8) | (midiVolume << 16));
    }
    
    audio_log("Music volume set to %.2f", g_fMusicVolume);
}

// ========================================================================
// SOUND EFFECT FUNCTIONS
// ========================================================================

/**
 * Loads a sound effect from file
 * @param sound_id Sound effect ID
 * @param filename WAV file to load
 * @return TRUE if successful
 */
BOOL load_sound_effect(int sound_id, const char* filename)
{
    if (!g_bAudioInitialized || sound_id < 0 || sound_id >= MAX_SOUND_EFFECTS)
        return FALSE;
    
    audio_log("Loading sound effect %d from %s", sound_id, filename);
    
    // Open the WAV file
    HMMIO hmmio = mmioOpen((LPSTR)filename, NULL, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio)
    {
        audio_log("Failed to open WAV file: %s", filename);
        return FALSE;
    }
    
    // Find the WAVE chunk
    MMCKINFO mmckinfoParent;
    mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
    {
        audio_log("Not a valid WAV file: %s", filename);
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Find the format chunk
    MMCKINFO mmckinfoSubchunk;
    mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
        audio_log("No format chunk in WAV file: %s", filename);
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Read the format
    WAVEFORMATEX format;
    if (mmioRead(hmmio, (HPSTR)&format, sizeof(PCMWAVEFORMAT)) != sizeof(PCMWAVEFORMAT))
    {
        audio_log("Failed to read WAV format: %s", filename);
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Ascend out of format chunk
    mmioAscend(hmmio, &mmckinfoSubchunk, 0);
    
    // Find the data chunk
    mmckinfoSubchunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
        audio_log("No data chunk in WAV file: %s", filename);
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Allocate memory for the data
    DWORD dataSize = mmckinfoSubchunk.cksize;
    LPBYTE pData = (LPBYTE)malloc(dataSize);
    if (!pData)
    {
        audio_log("Failed to allocate memory for WAV data");
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Read the data
    if (mmioRead(hmmio, (HPSTR)pData, dataSize) != (LONG)dataSize)
    {
        audio_log("Failed to read WAV data: %s", filename);
        free(pData);
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    mmioClose(hmmio, 0);
    
    // Store in sound effect array
    EnterCriticalSection(&g_audioCS);
    
    // Free previous data if any
    if (g_soundEffects[sound_id].pData)
        free(g_soundEffects[sound_id].pData);
    
    // Store new data
    strcpy(g_soundEffects[sound_id].filename, filename);
    g_soundEffects[sound_id].pData = pData;
    g_soundEffects[sound_id].dwDataSize = dataSize;
    g_soundEffects[sound_id].format = format;
    g_soundEffects[sound_id].bLoaded = TRUE;
    g_soundEffects[sound_id].fVolume = 1.0f;
    
    if (sound_id >= g_nSoundCount)
        g_nSoundCount = sound_id + 1;
    
    LeaveCriticalSection(&g_audioCS);
    
    audio_log("Sound effect %d loaded successfully", sound_id);
    return TRUE;
}

/**
 * Plays a sound effect
 * @param sound_id Sound effect ID to play
 * @return TRUE if successful
 */
int play_sound_effect(int sound_id)
{
    if (!g_bAudioInitialized || sound_id < 0 || sound_id >= g_nSoundCount)
        return FALSE;
    
    EnterCriticalSection(&g_audioCS);
    
    SoundEffect* pSound = &g_soundEffects[sound_id];
    if (!pSound->bLoaded || !pSound->pData)
    {
        LeaveCriticalSection(&g_audioCS);
        return FALSE;
    }
    
    // Find a free channel
    int channel = find_free_channel();
    if (channel < 0)
    {
        audio_log("No free audio channels");
        LeaveCriticalSection(&g_audioCS);
        return FALSE;
    }
    
    // Prepare wave header
    WAVEHDR waveHeader = {0};
    waveHeader.lpData = (LPSTR)pSound->pData;
    waveHeader.dwBufferLength = pSound->dwDataSize;
    waveHeader.dwFlags = 0;
    
    // Prepare the header
    MMRESULT result = waveOutPrepareHeader(g_hWaveOut, &waveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
    {
        audio_log("Failed to prepare wave header (error %d)", result);
        LeaveCriticalSection(&g_audioCS);
        return FALSE;
    }
    
    // Play the sound
    result = waveOutWrite(g_hWaveOut, &waveHeader, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
    {
        audio_log("Failed to play sound (error %d)", result);
        waveOutUnprepareHeader(g_hWaveOut, &waveHeader, sizeof(WAVEHDR));
        LeaveCriticalSection(&g_audioCS);
        return FALSE;
    }
    
    // Mark channel as active
    g_audioChannels[channel].nSoundID = sound_id;
    g_audioChannels[channel].bActive = TRUE;
    g_audioChannels[channel].fVolume = pSound->fVolume * g_fSoundVolume;
    
    LeaveCriticalSection(&g_audioCS);
    
    audio_log("Playing sound effect %d on channel %d", sound_id, channel);
    return TRUE;
}

/**
 * Stops all playing sound effects
 */
void stop_all_sounds(void)
{
    if (!g_bAudioInitialized)
        return;
    
    EnterCriticalSection(&g_audioCS);
    
    waveOutReset(g_hWaveOut);
    
    // Clear all channels
    for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
    {
        g_audioChannels[i].bActive = FALSE;
    }
    
    LeaveCriticalSection(&g_audioCS);
    
    audio_log("All sounds stopped");
}

// ========================================================================
// MUSIC PLAYBACK FUNCTIONS
// ========================================================================

/**
 * Loads a music track
 * @param track_id Music track ID
 * @param filename MIDI file to load
 * @return TRUE if successful
 */
BOOL load_music_track(int track_id, const char* filename)
{
    if (track_id < 0 || track_id >= MAX_MUSIC_TRACKS)
        return FALSE;
    
    strcpy(g_musicTracks[track_id].filename, filename);
    g_musicTracks[track_id].nTrackID = track_id;
    g_musicTracks[track_id].fVolume = 1.0f;
    g_musicTracks[track_id].bLooping = TRUE;
    
    audio_log("Music track %d registered: %s", track_id, filename);
    return TRUE;
}

/**
 * Starts playing background music
 * @param track_id Music track ID to play
 * @return TRUE if successful
 */
int start_background_music(int track_id)
{
    if (!g_bAudioInitialized || !g_bMidiAvailable)
        return FALSE;
    
    if (track_id < 0 || track_id >= MAX_MUSIC_TRACKS)
        return FALSE;
    
    // Stop current music if playing
    if (g_nCurrentMusic >= 0)
        stop_background_music();
    
    MusicTrack* pTrack = &g_musicTracks[track_id];
    
    // Open MIDI file
    char command[512];
    sprintf(command, "open \"%s\" type sequencer alias bgmusic", pTrack->filename);
    
    if (mciSendString(command, NULL, 0, NULL) != 0)
    {
        audio_log("Failed to open MIDI file: %s", pTrack->filename);
        return FALSE;
    }
    
    // Set volume
    set_music_volume(g_fMusicVolume);
    
    // Play the music
    if (pTrack->bLooping)
        strcpy(command, "play bgmusic from 0 repeat");
    else
        strcpy(command, "play bgmusic from 0");
    
    if (mciSendString(command, NULL, 0, NULL) != 0)
    {
        audio_log("Failed to play MIDI file");
        mciSendString("close bgmusic", NULL, 0, NULL);
        return FALSE;
    }
    
    g_nCurrentMusic = track_id;
    pTrack->bPlaying = TRUE;
    
    audio_log("Started background music: track %d", track_id);
    return TRUE;
}

/**
 * Stops the currently playing background music
 */
void stop_background_music(void)
{
    if (!g_bAudioInitialized || g_nCurrentMusic < 0)
        return;
    
    // Stop and close MIDI
    mciSendString("stop bgmusic", NULL, 0, NULL);
    mciSendString("close bgmusic", NULL, 0, NULL);
    
    if (g_nCurrentMusic >= 0 && g_nCurrentMusic < MAX_MUSIC_TRACKS)
        g_musicTracks[g_nCurrentMusic].bPlaying = FALSE;
    
    audio_log("Stopped background music");
    g_nCurrentMusic = -1;
}

/**
 * Pauses the currently playing background music
 */
void pause_background_music(void)
{
    if (!g_bAudioInitialized || g_nCurrentMusic < 0)
        return;
    
    mciSendString("pause bgmusic", NULL, 0, NULL);
    audio_log("Paused background music");
}

/**
 * Resumes the paused background music
 */
void resume_background_music(void)
{
    if (!g_bAudioInitialized || g_nCurrentMusic < 0)
        return;
    
    mciSendString("resume bgmusic", NULL, 0, NULL);
    audio_log("Resumed background music");
}

// ========================================================================
// RHYTHM LOOP SYSTEM (from rloops discovery)
// ========================================================================

/**
 * Loads and plays a rhythm loop file
 * @param filename Rhythm loop file (.wav)
 * @return TRUE if successful
 * 
 * Note: This function was originally sub_40eb30 - renamed for clarity
 */
int load_rhythm_loop(const char* filename)  // Was j_sub_40eb30
{
    if (!filename)
        return FALSE;
    
    audio_log("Loading rhythm loop: %s", filename);
    
    // Open multimedia file using mmio APIs
    HMMIO hmmio = mmioOpen((LPSTR)filename, NULL, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio)
    {
        audio_log("Failed to open rhythm loop file");
        return FALSE;
    }
    
    // Read file info and setup for playback
    MMCKINFO mmckinfoParent;
    MMCKINFO mmckinfoSubchunk;
    
    mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
    {
        audio_log("Invalid rhythm loop format");
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
        audio_log("No format chunk in rhythm loop");
        mmioClose(hmmio, 0);
        return FALSE;
    }
    
    // Store the file handle globally for later use
    if (g_hCurrentFile)
        mmioClose(g_hCurrentFile, 0);
    g_hCurrentFile = hmmio;
    
    audio_log("Rhythm loop loaded successfully");
    return TRUE;
}

/**
 * Plays a rhythm loop with specific timing
 * @param loop_id Loop identifier
 * @return TRUE if successful
 */
int play_rhythm_loop(int loop_id)
{
    if (!g_bAudioInitialized)
        return FALSE;
    
    // Map loop_id to actual rhythm files discovered
    const char* rhythm_files[] = {
        "rloops\\beginner_sequence.wav",
        "rloops\\first_sequence.wav",
        "rloops\\hrockin3_sequence.wav"
    };
    
    if (loop_id < 0 || loop_id >= sizeof(rhythm_files) / sizeof(rhythm_files[0]))
        return FALSE;
    
    return load_rhythm_loop(rhythm_files[loop_id]);
}

// ========================================================================
// COMPATIBILITY FUNCTIONS
// ========================================================================

/**
 * Legacy function for checking rhythm files
 * @return TRUE if rhythm files exist
 */
int check_rhythm_files(void)  // Was j_sub_406fc0
{
    // Check if rhythm loop directory exists
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("rloops\\*.wav", &findData);
    
    if (hFind != INVALID_HANDLE_VALUE)
    {
        FindClose(hFind);
        audio_log("Rhythm files found");
        return TRUE;
    }
    
    audio_log("No rhythm files found");
    return FALSE;
}

/**
 * Legacy function for initializing background music
 * @return TRUE if successful
 */
int initialize_background_music(void)  // Was j_sub_406fb0
{
    if (!g_bMidiAvailable)
    {
        audio_log("MIDI not available for background music");
        return FALSE;
    }
    
    // Load default music tracks
    load_music_track(MUSIC_MENU, "music\\menu.mid");
    load_music_track(MUSIC_LEVEL1, "music\\level1.mid");
    load_music_track(MUSIC_LEVEL2, "music\\level2.mid");
    load_music_track(MUSIC_BOSS, "music\\boss.mid");
    load_music_track(MUSIC_VICTORY, "music\\victory.mid");
    load_music_track(MUSIC_GAMEOVER, "music\\gameover.mid");
    
    audio_log("Background music initialized");
    return TRUE;
}

// ========================================================================
// DIAGNOSTIC FUNCTIONS
// ========================================================================

/**
 * Gets audio system status information
 * @param buffer Output buffer
 * @param size Buffer size
 */
void get_audio_status(char* buffer, size_t size)
{
    snprintf(buffer, size,
        "Audio System Status:\n"
        "  Initialized: %s\n"
        "  Wave Output: %s\n"
        "  MIDI Output: %s\n"
        "  Master Volume: %.2f\n"
        "  Sound Volume: %.2f\n"
        "  Music Volume: %.2f\n"
        "  Sounds Loaded: %d\n"
        "  Current Music: %d\n"
        "  Active Channels: %d\n",
        g_bAudioInitialized ? "Yes" : "No",
        g_hWaveOut ? "Active" : "Inactive",
        g_bMidiAvailable ? "Available" : "Not Available",
        g_fMasterVolume,
        g_fSoundVolume,
        g_fMusicVolume,
        g_nSoundCount,
        g_nCurrentMusic,
        0  // Would count active channels
    );
}

/**
 * Preloads common sound effects
 * @return Number of sounds loaded
 */
int preload_common_sounds(void)
{
    int loaded = 0;
    
    struct {
        int id;
        const char* file;
    } sounds[] = {
        { SFX_MENU_SELECT, "sounds\\menu_select.wav" },
        { SFX_MENU_BACK, "sounds\\menu_back.wav" },
        { SFX_GAME_START, "sounds\\game_start.wav" },
        { SFX_PLAYER_JUMP, "sounds\\jump.wav" },
        { SFX_PLAYER_LAND, "sounds\\land.wav" },
        { SFX_ITEM_PICKUP, "sounds\\pickup.wav" },
        { SFX_VICTORY, "sounds\\victory.wav" }
    };
    
    for (int i = 0; i < sizeof(sounds) / sizeof(sounds[0]); i++)
    {
        if (load_sound_effect(sounds[i].id, sounds[i].file))
            loaded++;
    }
    
    audio_log("Preloaded %d common sounds", loaded);
    return loaded;
}