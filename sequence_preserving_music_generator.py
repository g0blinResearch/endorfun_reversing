#!/usr/bin/env python3
"""
Sequence Preserving ELV Music Generator
Generate 30-second backing track samples preserving original ELV sequence order
Uses sequence-preserving extraction data to create authentic level music as intended by designers
"""

import os
import sys
import json
import wave
import numpy as np
from typing import List, Dict, Optional

class SequencePreservingMusicGenerator:
    def __init__(self, extraction_data_file: str, rloops_dir: str = "rloops"):
        self.rloops_dir = rloops_dir
        self.extraction_data = self._load_extraction_data(extraction_data_file)
        self.target_sample_rate = 44100
        
    def _load_extraction_data(self, data_file: str) -> Dict:
        """Load the sequence-preserving extraction data from JSON file"""
        try:
            with open(data_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                
                # Verify this is sequence-preserving data
                metadata = data.get('extraction_metadata', {})
                if not metadata.get('sequence_preserved', False):
                    print("⚠️ Warning: Data may not be sequence-preserving!")
                else:
                    print("✅ Using sequence-preserving extraction data")
                    
                return data
        except Exception as e:
            print(f"❌ Error loading extraction data: {e}")
            return {}
    
    def list_available_levels(self) -> List[str]:
        """List all available levels with their audio counts (original sequence order)"""
        levels = []
        if 'files' in self.extraction_data:
            for file_info in self.extraction_data['files']:
                if file_info['parse_success'] and file_info['audio_count'] > 0:
                    levels.append((
                        file_info['filename'],
                        file_info['audio_count'],
                        file_info.get('level_title', 'No title')
                    ))
        
        # Sort by audio complexity (most complex first) for display
        levels.sort(key=lambda x: x[1], reverse=True)
        return levels
    
    def get_level_info(self, level_filename: str) -> Optional[Dict]:
        """Get detailed info for a specific level"""
        if 'files' not in self.extraction_data:
            return None
            
        for file_info in self.extraction_data['files']:
            if file_info['filename'].upper() == level_filename.upper():
                return file_info
        return None
    
    def load_audio_file(self, filename: str) -> Optional[np.ndarray]:
        """Load a single audio file and convert to standard format"""
        file_path = os.path.join(self.rloops_dir, filename)
        
        if not os.path.exists(file_path):
            print(f"⚠️ Audio file not found: {filename}")
            return None
        
        try:
            with wave.open(file_path, 'rb') as wav_file:
                channels = wav_file.getnchannels()
                sampwidth = wav_file.getsampwidth()
                framerate = wav_file.getframerate()
                nframes = wav_file.getnframes()
                frames = wav_file.readframes(nframes)
                
                # Convert to float32 mono
                if sampwidth == 1 and channels == 2:
                    # 8-bit stereo
                    audio_data = np.frombuffer(frames, dtype=np.uint8).reshape(-1, 2)
                    audio_mono = np.mean(audio_data, axis=1)
                    audio_float = (audio_mono.astype(np.float32) - 128) / 128.0
                elif sampwidth == 2 and channels == 2:
                    # 16-bit stereo
                    audio_data = np.frombuffer(frames, dtype=np.int16).reshape(-1, 2)
                    audio_mono = np.mean(audio_data, axis=1)
                    audio_float = audio_mono.astype(np.float32) / 32768.0
                elif sampwidth == 2 and channels == 1:
                    # 16-bit mono
                    audio_data = np.frombuffer(frames, dtype=np.int16)
                    audio_float = audio_data.astype(np.float32) / 32768.0
                elif sampwidth == 1 and channels == 1:
                    # 8-bit mono
                    audio_data = np.frombuffer(frames, dtype=np.uint8)
                    audio_float = (audio_data.astype(np.float32) - 128) / 128.0
                else:
                    print(f"⚠️ Unsupported format: {sampwidth}-bit, {channels} channels")
                    return None
                
                # Resample if needed
                if framerate != self.target_sample_rate:
                    # Simple resampling by repetition or decimation
                    if framerate < self.target_sample_rate:
                        repeat_factor = self.target_sample_rate // framerate
                        audio_float = np.repeat(audio_float, repeat_factor)
                    else:
                        decimate_factor = framerate // self.target_sample_rate
                        audio_float = audio_float[::decimate_factor]
                
                return audio_float
                
        except Exception as e:
            print(f"❌ Error loading {filename}: {e}")
            return None
    
    def generate_level_music(self, level_filename: str, duration_seconds: float = 30.0) -> Optional[np.ndarray]:
        """Generate backing track for a specific level using original sequence order"""
        print(f"\n🎵 Generating music for {level_filename}")
        
        # Get level info
        level_info = self.get_level_info(level_filename)
        if not level_info:
            print(f"❌ Level not found: {level_filename}")
            return None
        
        if level_info['audio_count'] == 0:
            print(f"❌ No audio files found for {level_filename}")
            return None
        
        print(f"📋 Level: {level_info.get('level_title', 'Unknown')}")
        print(f"🎶 Audio files: {level_info['audio_count']}")
        print(f"🎯 PRESERVING ORIGINAL ELV SEQUENCE ORDER (NO ALPHABETICAL SORTING)")
        
        # Load audio files in ORIGINAL SEQUENCE ORDER - this is the key fix!
        audio_clips = []
        total_duration = 0.0
        
        # Sort audio files by sequence_position to ensure original order
        audio_files_sorted = sorted(level_info['audio_files'], key=lambda x: x.get('sequence_position', 0))
        
        print(f"📊 Audio sequence (as appeared in ELV binary data):")
        for i, audio_info in enumerate(audio_files_sorted, 1):
            filename = audio_info['filename']
            seq_pos = audio_info.get('sequence_position', 'unknown')
            file_type = audio_info.get('file_type', 'unknown')
            print(f"  {i:2d}. [{seq_pos:2}] {filename} ({file_type})")
            
            # Skip silence files for backing track
            if audio_info.get('file_type') == 'silence':
                print(f"      ⏭️ Skipping silence file")
                continue
                
            audio_data = self.load_audio_file(filename)
            if audio_data is not None:
                clip_duration = len(audio_data) / self.target_sample_rate
                audio_clips.append((filename, audio_data, clip_duration, seq_pos))
                total_duration += clip_duration
                print(f"      ✅ Loaded ({clip_duration:.1f}s)")
            else:
                print(f"      ❌ Failed to load")
        
        if not audio_clips:
            print(f"❌ No audio clips loaded successfully")
            return None
        
        print(f"\n📊 Total source duration: {total_duration:.1f}s")
        print(f"🔄 Will cycle through {len(audio_clips)} audio clips in ORIGINAL ORDER")
        
        # Create target buffer
        target_samples = int(duration_seconds * self.target_sample_rate)
        music_buffer = np.zeros(target_samples, dtype=np.float32)
        
        # Fill buffer by cycling through clips IN ORIGINAL SEQUENCE ORDER
        current_pos = 0
        cycle_count = 0
        
        while current_pos < target_samples:
            cycle_count += 1
            print(f"\n🔄 Cycle {cycle_count} (preserving ELV sequence order)")
            
            for filename, audio_data, clip_duration, seq_pos in audio_clips:
                if current_pos >= target_samples:
                    break
                
                remaining_samples = target_samples - current_pos
                samples_to_copy = min(len(audio_data), remaining_samples)
                
                # Copy audio data
                music_buffer[current_pos:current_pos + samples_to_copy] = audio_data[:samples_to_copy]
                current_pos += samples_to_copy
                
                actual_duration = samples_to_copy / self.target_sample_rate
                print(f"  + [{seq_pos:2}] {filename}: {actual_duration:.1f}s")
                
                if samples_to_copy < len(audio_data):
                    break  # Reached target duration
        
        # Normalize audio
        max_amplitude = np.max(np.abs(music_buffer))
        if max_amplitude > 1.0:
            music_buffer = music_buffer / max_amplitude
            print(f"🔧 Normalized (max was {max_amplitude:.3f})")
        elif max_amplitude > 0:
            print(f"✅ Audio levels good (max: {max_amplitude:.3f})")
        
        return music_buffer
    
    def save_music(self, audio_data: np.ndarray, output_filename: str) -> bool:
        """Save generated music to WAV file"""
        try:
            # Convert to 16-bit
            audio_16bit = (audio_data * 32767).astype(np.int16)
            
            # Save WAV file
            with wave.open(output_filename, 'wb') as wav_file:
                wav_file.setnchannels(1)  # Mono
                wav_file.setsampwidth(2)  # 16-bit
                wav_file.setframerate(self.target_sample_rate)
                wav_file.writeframes(audio_16bit.tobytes())
            
            file_size = os.path.getsize(output_filename) / (1024 * 1024)
            duration = len(audio_data) / self.target_sample_rate
            print(f"\n💾 Saved: {output_filename}")
            print(f"📏 Duration: {duration:.1f}s, Size: {file_size:.1f} MB")
            
            return True
            
        except Exception as e:
            print(f"❌ Error saving: {e}")
            return False

def main():
    print("=" * 80)
    print("🎯 SEQUENCE PRESERVING ELV MUSIC GENERATOR")
    print("Generate backing tracks using ORIGINAL ELV sequence order (no alphabetical sorting)")
    print("=" * 80)
    
    # Check for sequence-preserving extraction files
    sequence_files = [f for f in os.listdir('.') if f.startswith('sequence_preserved_elv_') and f.endswith('.json')]
    
    if not sequence_files:
        print("❌ No sequence-preserving ELV extraction data found!")
        print("   Please run sequence_preserving_elv_extractor.py first to generate extraction data.")
        print("   This ensures audio files are in original ELV binary order, not alphabetical.")
        sys.exit(1)
    
    # Use the most recent sequence-preserving extraction file
    extraction_file = sorted(sequence_files)[-1]
    print(f"📁 Using sequence-preserving data: {extraction_file}")
    
    # Initialize generator
    generator = SequencePreservingMusicGenerator(extraction_file)
    
    # Check command line arguments
    if len(sys.argv) > 1:
        level_name = sys.argv[1]
        duration = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0
    else:
        # Show available levels
        print(f"\n📋 Available levels (showing complexity ranking):")
        print(f"{'Rank':<4} {'Level':<15} {'Audio':<5} {'Title':<40}")
        print("-" * 80)
        
        levels = generator.list_available_levels()
        for i, (filename, audio_count, title) in enumerate(levels, 1):
            title_short = title[:40] + "..." if len(title) > 40 else title
            print(f"{i:<4} {filename:<15} {audio_count:<5} {title_short}")
        
        print(f"\n🎯 USAGE:")
        print(f"  python sequence_preserving_music_generator.py <LEVEL_NAME> [duration_seconds]")
        print(f"\n📝 EXAMPLES:")
        print(f"  python sequence_preserving_music_generator.py ONESONG.ELV 30")
        print(f"  python sequence_preserving_music_generator.py BEGINNER.ELV 15") 
        print(f"  python sequence_preserving_music_generator.py MAGIC.ELV")
        print(f"\n🎵 NOTE: Audio will play in ORIGINAL ELV sequence order as intended by level designers")
        
        sys.exit(0)
    
    # Generate music
    print(f"\n🎵 Generating {duration}s backing track for {level_name}")
    print(f"🎯 Using ORIGINAL sequence order from ELV binary data")
    
    audio_data = generator.generate_level_music(level_name, duration)
    if audio_data is None:
        print(f"❌ Failed to generate music")
        sys.exit(1)
    
    # Save output with sequence-preserving indicator
    base_name = os.path.splitext(level_name)[0].lower()
    output_filename = f"{base_name}_sequence_preserved_{int(duration)}s.wav"
    
    if generator.save_music(audio_data, output_filename):
        print(f"\n🎯 SUCCESS!")
        print(f"✅ Generated sequence-preserving backing track: {output_filename}")
        print(f"🎧 Audio plays in ORIGINAL ELV order (no alphabetical sorting)")
        print(f"🎵 Ready to play!")
    else:
        print(f"❌ Failed to save music")
        sys.exit(1)

if __name__ == "__main__":
    main()