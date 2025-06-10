#include "endor_readable.h"
#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>

// =============================================
// ENDOR AUDIO SYSTEM IMPLEMENTATION
// =============================================

// Audio subsystem globals
static HWAVEOUT hWaveOut = NULL;
static HMIDIOUT hMidiOut = NULL; 
static HMMIO hmmioFile = NULL;
static int audio_initialized = 0;
static int current_music_track = -1;
static float master_volume = 1.0f;

// =============================================
// CORE AUDIO SYSTEM FUNCTIONS
// =============================================

int initialize_audio_system() {
    if (audio_initialized) return 1;
    
    // Initialize wave audio
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 0;
    }
    
    // Initialize MIDI
    if (midiOutOpen(&hMidiOut, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        waveOutClose(hWaveOut);
        return 0;
    }
    
    audio_initialized = 1;
    return 1;
}

void shutdown_audio_system() {
    if (!audio_initialized) return;
    
    if (hmmioFile) {
        mmioClose(hmmioFile, 0);
        hmmioFile = NULL;
    }
    
    if (hMidiOut) {
        midiOutClose(hMidiOut);
        hMidiOut = NULL;
    }
    
    if (hWaveOut) {
        waveOutClose(hWaveOut);
        hWaveOut = NULL;
    }
    
    audio_initialized = 0;
}

int play_sound_effect(int sound_id) {
    if (!audio_initialized) return 0;
    
    // Placeholder for sound effect playback
    // In real implementation, would load and play WAV file based on sound_id
    return 1;
}

int start_background_music(int track_id) {
    if (!audio_initialized) return 0;
    
    current_music_track = track_id;
    // Placeholder for background music playback
    // In real implementation, would start MIDI or streaming audio
    return 1;
}

void stop_background_music() {
    if (!audio_initialized) return;
    
    if (hMidiOut) {
        midiOutReset(hMidiOut);
    }
    current_music_track = -1;
}

void set_master_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    master_volume = volume;
    
    if (audio_initialized && hWaveOut) {
        DWORD dwVolume = (DWORD)(volume * 0xFFFF);
        dwVolume |= (dwVolume << 16);
        waveOutSetVolume(hWaveOut, dwVolume);
    }
}

float get_master_volume() {
    return master_volume;
}

// =============================================
// AUDIO-RELATED EXTRACTED FUNCTIONS
// =============================================

int j_sub_40eb30(char* filename) {
    // Real implementation extracted from sub_40eb30 - multimedia file loader
    if (!filename) return 0;
    
    // Open multimedia file using mmio APIs
    HMMIO hmmio = mmioOpen(filename, NULL, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio) return 0;
    
    // Read file info and setup for playback
    MMCKINFO mmckinfoParent;
    MMCKINFO mmckinfoSubchunk;
    
    mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return 0;
    }
    
    mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return 0;
    }
    
    // Store the file handle globally for later use
    if (hmmioFile) mmioClose(hmmioFile, 0);
    hmmioFile = hmmio;
    
    return 1;
}

// =============================================
// ADVANCED AUDIO FEATURES
// =============================================

int load_wav_file(const char* filename) {
    if (!audio_initialized) return 0;
    
    // Implementation for loading WAV files
    // This would integrate with the rhythm loop system discovered in rloops/
    return j_sub_40eb30((char*)filename);
}

int play_rhythm_loop(int loop_id) {
    if (!audio_initialized) return 0;
    
    // Implementation for playing rhythm loops
    // This would map to the *RHYTH.WAV files in rloops/
    char filename[256];
    sprintf(filename, "rloops/%dRHYTH.WAV", loop_id);
    
    return load_wav_file(filename);
}

int get_current_music_track() {
    return current_music_track;
}

int is_audio_initialized() {
    return audio_initialized;
}

/* End of endor_audio_system.c */