#!/usr/bin/env python3
"""
Sequence Preserving ELV Extractor
Extract audio information while preserving the original sequence order from ELV binary data
Fixed to match the C code's sequential playback logic
"""

import os
import sys
import struct
import re
import json
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from datetime import datetime

@dataclass
class ELVAudioInfo:
    """Audio file reference information with sequence position"""
    filename: str
    file_type: str
    exists_in_rloops: bool
    sequence_position: int  # Order of appearance in binary data
    binary_offset: int      # Byte offset where found
    alternative_found: Optional[str] = None

@dataclass
class ELVFileInfo:
    """Complete information about an ELV file with preserved sequence"""
    filename: str
    filepath: str
    file_size: int
    elvd_version: int
    level_title: Optional[str]
    audio_files: List[ELVAudioInfo]
    audio_count: int
    parse_success: bool
    error_message: Optional[str]
    analysis_timestamp: str

class SequencePreservingELVExtractor:
    def __init__(self, elvrl_directory: str = "ELVRL", rloops_directory: str = "rloops"):
        self.elvrl_dir = elvrl_directory
        self.rloops_dir = rloops_directory
        
        # Cache available audio files
        self.available_audio_files = set()
        self._cache_available_audio_files()
        
        # Results storage
        self.extracted_files: List[ELVFileInfo] = []
        
    def _cache_available_audio_files(self):
        """Cache list of available audio files for faster lookup"""
        if os.path.exists(self.rloops_dir):
            for filename in os.listdir(self.rloops_dir):
                if filename.upper().endswith('.WAV'):
                    self.available_audio_files.add(filename.upper())
        print(f"📁 Found {len(self.available_audio_files)} audio files in {self.rloops_dir}")
    
    def _check_audio_file_exists(self, filename: str) -> Tuple[bool, Optional[str]]:
        """Check if audio file exists, with fallback checking"""
        filename_upper = filename.upper()
        
        # Direct match
        if filename_upper in self.available_audio_files:
            return True, filename_upper
        
        # Try without first digit (e.g., 6970RHYTH.WAV → 970RHYTH.WAV)
        if len(filename) > 4 and filename[0].isdigit():
            alt_filename = filename[1:].upper()
            if alt_filename in self.available_audio_files:
                return True, alt_filename
        
        return False, None
    
    def _extract_audio_sequence_from_data(self, data: bytes) -> List[Tuple[str, int]]:
        """Extract audio files preserving the original sequence order from binary data"""
        audio_sequence = []  # List of (filename, byte_offset) tuples
        
        try:
            # Convert to string for pattern matching
            text_data = data.decode('ascii', errors='ignore')
            
            # Comprehensive audio file patterns with case variations
            audio_patterns = [
                r'(\d+rhyth\.wav)',         # Standard rhythm files: 123rhyth.wav
                r'([a-z]rhyth\.wav)',       # Letter rhythm files: arhyth.wav  
                r'(!\d+rhyt\.wav)',         # System files: !93rhyt.wav
                r'(x\d+rhyt\.wav)',         # X-prefixed files: x93rhyt.wav
                r'(nosound\.wav)',          # Silence file
                r'(\d+RHYTH\.WAV)',         # Uppercase variants
                r'([A-Z]RHYTH\.WAV)',       # Uppercase letter variants
                r'(!\d+RHYT\.WAV)',         # Uppercase system files
                r'(X\d+RHYT\.WAV)',         # Uppercase X-prefixed files
                r'(NOSOUND\.WAV)',          # Uppercase silence
            ]
            
            # Find all matches with their positions
            all_matches = []
            for pattern in audio_patterns:
                for match in re.finditer(pattern, text_data, re.IGNORECASE):
                    filename = match.group(1).upper()
                    position = match.start()
                    all_matches.append((filename, position))
            
            # Sort by position to preserve original sequence order
            all_matches.sort(key=lambda x: x[1])
            
            # Remove duplicates while preserving order (keep first occurrence)
            seen = set()
            for filename, position in all_matches:
                if filename not in seen and len(filename) > 4:
                    audio_sequence.append((filename, position))
                    seen.add(filename)
                    
        except Exception as e:
            print(f"⚠️ Error extracting audio sequence: {e}")
        
        return audio_sequence
    
    def _classify_audio_file(self, filename: str) -> str:
        """Classify audio file by type based on filename patterns"""
        filename_upper = filename.upper()
        
        if filename_upper == 'NOSOUND.WAV':
            return 'silence'
        elif filename_upper.startswith('!'):
            return 'system'
        elif filename_upper.startswith('X'):
            return 'special'
        elif re.match(r'^[A-Z]RHYTH\.WAV$', filename_upper):
            return 'letter_rhythm'
        elif re.match(r'^\d+RHYTH\.WAV$', filename_upper):
            return 'numbered_rhythm'
        else:
            return 'unknown'
    
    def parse_elv_file(self, filepath: str) -> ELVFileInfo:
        """Parse a single ELV file preserving original audio sequence"""
        filename = os.path.basename(filepath)
        print(f"\n🔍 Parsing: {filename}")
        
        file_info = ELVFileInfo(
            filename=filename,
            filepath=filepath,
            file_size=0,
            elvd_version=0,
            level_title=None,
            audio_files=[],
            audio_count=0,
            parse_success=False,
            error_message=None,
            analysis_timestamp=datetime.now().isoformat()
        )
        
        if not os.path.exists(filepath):
            file_info.error_message = f"File not found: {filepath}"
            return file_info
        
        try:
            file_info.file_size = os.path.getsize(filepath)
            
            with open(filepath, 'rb') as f:
                # Read entire file for comprehensive analysis
                file_data = f.read()
                
                # Check for ELVD header
                if len(file_data) < 8 or file_data[:4] != b'ELVD':
                    file_info.error_message = f"Invalid ELVD header"
                    return file_info
                
                # Parse version (using 16-bit as per analysis doc)
                version_data = file_data[4:8]
                file_info.elvd_version = struct.unpack('<H', version_data[:2])[0]
                print(f"  ✓ ELVD version {file_info.elvd_version}")
                
                # Extract level title from raw data
                text_data = file_data[8:].decode('ascii', errors='ignore')
                lines = text_data.split('\x00')
                for line in lines:
                    clean_line = ''.join(c for c in line if c.isprintable()).strip()
                    if len(clean_line) > 10 and not clean_line.endswith('.lbm'):
                        file_info.level_title = clean_line[:50]
                        break
                
                # Extract audio sequence preserving original order
                audio_sequence = self._extract_audio_sequence_from_data(file_data)
                
                # Process audio files in sequence order
                for sequence_pos, (audio_filename, binary_offset) in enumerate(audio_sequence):
                    exists, alternative = self._check_audio_file_exists(audio_filename)
                    file_type = self._classify_audio_file(audio_filename)
                    
                    audio_info = ELVAudioInfo(
                        filename=audio_filename,
                        file_type=file_type,
                        exists_in_rloops=exists,
                        sequence_position=sequence_pos,
                        binary_offset=binary_offset,
                        alternative_found=alternative if alternative != audio_filename.upper() else None
                    )
                    file_info.audio_files.append(audio_info)
                
                file_info.audio_count = len(file_info.audio_files)
                file_info.parse_success = True
                
                print(f"  ✓ Found {file_info.audio_count} audio files in sequence order")
                
                if file_info.audio_count > 0:
                    # Show sequence preview
                    print(f"  🎵 Audio sequence:")
                    for i, audio in enumerate(file_info.audio_files[:5]):  # Show first 5
                        print(f"    {i+1:2d}. {audio.filename}")
                    if len(file_info.audio_files) > 5:
                        print(f"    ... and {len(file_info.audio_files) - 5} more")
                    
                    # Show type breakdown
                    type_counts = {}
                    existing_count = 0
                    for audio in file_info.audio_files:
                        type_counts[audio.file_type] = type_counts.get(audio.file_type, 0) + 1
                        if audio.exists_in_rloops:
                            existing_count += 1
                    
                    type_summary = ", ".join([f"{count} {type_}" for type_, count in type_counts.items()])
                    print(f"    📊 Types: {type_summary}")
                    print(f"    ✅ Available: {existing_count}, ❌ Missing: {file_info.audio_count - existing_count}")
                
        except Exception as e:
            file_info.error_message = f"Parse error: {str(e)}"
            print(f"  ❌ Error: {e}")
        
        return file_info
    
    def extract_all_files(self) -> bool:
        """Extract information from all ELV files preserving sequence order"""
        print("=" * 80)
        print("🎯 SEQUENCE PRESERVING ELV EXTRACTOR")
        print("Extracting audio sequences in original binary order (no alphabetical sorting)")
        print("=" * 80)
        
        if not os.path.exists(self.elvrl_dir):
            print(f"❌ ELVRL directory not found: {self.elvrl_dir}")
            return False
        
        # Find all ELV files
        elv_files = []
        for filename in os.listdir(self.elvrl_dir):
            if filename.upper().endswith('.ELV'):
                elv_files.append(os.path.join(self.elvrl_dir, filename))
        
        elv_files.sort()
        print(f"📁 Found {len(elv_files)} ELV files to process")
        
        if not elv_files:
            print("❌ No ELV files found")
            return False
        
        # Process each file
        success_count = 0
        for i, filepath in enumerate(elv_files, 1):
            print(f"\n[{i}/{len(elv_files)}]", end=" ")
            file_info = self.parse_elv_file(filepath)
            self.extracted_files.append(file_info)
            
            if file_info.parse_success:
                success_count += 1
        
        print(f"\n\n✅ Processing complete: {success_count}/{len(elv_files)} files parsed successfully")
        return True
    
    def save_sequence_data(self, output_file: str) -> bool:
        """Save sequence-preserving extraction results as JSON"""
        try:
            json_data = {
                "extraction_metadata": {
                    "timestamp": datetime.now().isoformat(),
                    "total_files": len(self.extracted_files),
                    "successful_files": len([f for f in self.extracted_files if f.parse_success]),
                    "elvrl_directory": self.elvrl_dir,
                    "rloops_directory": self.rloops_dir,
                    "sequence_preserved": True,
                    "note": "Audio files are in original ELV binary order, not alphabetical"
                },
                "files": [asdict(file_info) for file_info in self.extracted_files]
            }
            
            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(json_data, f, indent=2, ensure_ascii=False)
            
            print(f"💾 Sequence data saved: {output_file}")
            return True
            
        except Exception as e:
            print(f"❌ Error saving sequence data: {e}")
            return False

def main():
    print("=" * 80)
    print("🎯 SEQUENCE PRESERVING ELV EXTRACTOR")
    print("Extract audio sequences preserving original binary order from ELV files")
    print("=" * 80)
    
    # Parse command line arguments
    elvrl_dir = sys.argv[1] if len(sys.argv) > 1 else "ELVRL"
    rloops_dir = sys.argv[2] if len(sys.argv) > 2 else "rloops"
    
    # Initialize extractor
    extractor = SequencePreservingELVExtractor(elvrl_dir, rloops_dir)
    
    # Extract all information
    if not extractor.extract_all_files():
        print("❌ Extraction failed")
        sys.exit(1)
    
    # Generate outputs
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Sequence-preserving data
    sequence_file = f"sequence_preserved_elv_{timestamp}.json"
    extractor.save_sequence_data(sequence_file)
    
    print(f"\n🎯 SEQUENCE EXTRACTION COMPLETE!")
    print(f"📁 Generated: {sequence_file}")
    print(f"🎵 Audio sequences preserved in original ELV binary order")
    print(f"❌ No alphabetical sorting applied")

if __name__ == "__main__":
    main()