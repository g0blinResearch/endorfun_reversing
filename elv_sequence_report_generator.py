#!/usr/bin/env python3
"""
ELV Sequence Report Generator
Generate comprehensive reports showing WAV samples and ordering for all ELV files
Uses sequence-preserving logic to show original intended audio progressions
"""

import os
import sys
import json
import struct
import re
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from datetime import datetime

@dataclass
class AudioSequenceInfo:
    """Information about an audio file in sequence"""
    sequence_position: int
    filename: str
    file_type: str
    exists_in_rloops: bool
    binary_offset: int
    duration_seconds: float = 0.0
    file_size_kb: int = 0
    alternative_found: Optional[str] = None

@dataclass
class ELVSequenceReport:
    """Complete sequence report for an ELV file"""
    filename: str
    level_title: str
    elvd_version: int
    total_audio_files: int
    available_audio_files: int
    missing_audio_files: int
    total_duration: float
    audio_sequence: List[AudioSequenceInfo]
    type_breakdown: Dict[str, int]
    complexity_score: int
    analysis_notes: List[str]

class ELVSequenceReportGenerator:
    def __init__(self, elvrl_directory: str = "ELVRL", rloops_directory: str = "rloops"):
        self.elvrl_dir = elvrl_directory
        self.rloops_dir = rloops_directory
        
        # Cache available audio files with metadata
        self.audio_file_info = {}
        self._cache_audio_file_info()
        
        # Results storage
        self.reports: List[ELVSequenceReport] = []
        
    def _cache_audio_file_info(self):
        """Cache audio file information for analysis"""
        if not os.path.exists(self.rloops_dir):
            print(f"⚠️ Audio directory not found: {self.rloops_dir}")
            return
            
        for filename in os.listdir(self.rloops_dir):
            if filename.upper().endswith('.WAV'):
                filepath = os.path.join(self.rloops_dir, filename)
                file_size = os.path.getsize(filepath) // 1024  # KB
                
                # Try to get duration from WAV header
                duration = self._get_wav_duration(filepath)
                
                self.audio_file_info[filename.upper()] = {
                    'duration': duration,
                    'size_kb': file_size
                }
        
        print(f"📁 Cached metadata for {len(self.audio_file_info)} audio files")
    
    def _get_wav_duration(self, filepath: str) -> float:
        """Get WAV file duration in seconds"""
        try:
            import wave
            with wave.open(filepath, 'rb') as wav_file:
                frames = wav_file.getnframes()
                sample_rate = wav_file.getframerate()
                return frames / sample_rate if sample_rate > 0 else 0.0
        except:
            return 0.0
    
    def _check_audio_file_exists(self, filename: str) -> Tuple[bool, Optional[str], Dict]:
        """Check if audio file exists and get metadata"""
        filename_upper = filename.upper()
        
        # Direct match
        if filename_upper in self.audio_file_info:
            return True, filename_upper, self.audio_file_info[filename_upper]
        
        # Try without first digit (e.g., 6970RHYTH.WAV → 970RHYTH.WAV)
        if len(filename) > 4 and filename[0].isdigit():
            alt_filename = filename[1:].upper()
            if alt_filename in self.audio_file_info:
                return True, alt_filename, self.audio_file_info[alt_filename]
        
        return False, None, {'duration': 0.0, 'size_kb': 0}
    
    def _extract_audio_sequence_from_data(self, data: bytes) -> List[Tuple[str, int]]:
        """Extract audio files preserving the original sequence order from binary data"""
        audio_sequence = []  # List of (filename, byte_offset) tuples
        
        try:
            # Convert to string for pattern matching
            text_data = data.decode('ascii', errors='ignore')
            
            # Comprehensive audio file patterns
            audio_patterns = [
                r'(\d+rhyth\.wav)',         # Standard rhythm files
                r'([a-z]rhyth\.wav)',       # Letter rhythm files  
                r'(!\d+rhyt\.wav)',         # System files
                r'(x\d+rhyt\.wav)',         # X-prefixed files
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
            
            # Remove duplicates while preserving order
            seen = set()
            for filename, position in all_matches:
                if filename not in seen and len(filename) > 4:
                    audio_sequence.append((filename, position))
                    seen.add(filename)
                    
        except Exception as e:
            print(f"⚠️ Error extracting audio sequence: {e}")
        
        return audio_sequence
    
    def _classify_audio_file(self, filename: str) -> str:
        """Classify audio file by type"""
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
    
    def _calculate_complexity_score(self, audio_sequence: List[AudioSequenceInfo]) -> int:
        """Calculate complexity score based on various factors"""
        score = 0
        
        # Base score from number of files
        score += len(audio_sequence) * 2
        
        # Bonus for variety of types
        types = set(audio.file_type for audio in audio_sequence)
        score += len(types) * 5
        
        # Bonus for longer durations
        total_duration = sum(audio.duration_seconds for audio in audio_sequence)
        score += int(total_duration / 10)  # 1 point per 10 seconds
        
        # Penalty for missing files
        missing_count = sum(1 for audio in audio_sequence if not audio.exists_in_rloops)
        score -= missing_count * 3
        
        return max(0, score)
    
    def _generate_analysis_notes(self, audio_sequence: List[AudioSequenceInfo]) -> List[str]:
        """Generate analysis notes about the audio sequence"""
        notes = []
        
        if not audio_sequence:
            notes.append("No audio files found in this level")
            return notes
        
        # Check sequence patterns
        rhythm_files = [a for a in audio_sequence if a.file_type == 'numbered_rhythm']
        if len(rhythm_files) > 1:
            rhythm_numbers = []
            for audio in rhythm_files:
                match = re.match(r'^(\d+)RHYTH\.WAV$', audio.filename)
                if match:
                    rhythm_numbers.append(int(match.group(1)))
            
            if rhythm_numbers:
                if rhythm_numbers == sorted(rhythm_numbers):
                    notes.append("Rhythm files are in ascending numerical order")
                elif rhythm_numbers == sorted(rhythm_numbers, reverse=True):
                    notes.append("Rhythm files are in descending numerical order")
                else:
                    notes.append("Rhythm files follow a custom sequence pattern")
        
        # Check for silence placement
        silence_positions = [i for i, audio in enumerate(audio_sequence) if audio.file_type == 'silence']
        if silence_positions:
            if 0 in silence_positions:
                notes.append("Level starts with silence")
            if len(audio_sequence) - 1 in silence_positions:
                notes.append("Level ends with silence")
            if len(silence_positions) > 1:
                notes.append(f"Multiple silence files at positions: {silence_positions}")
        
        # Check for special files
        system_files = [a for a in audio_sequence if a.file_type == 'system']
        if system_files:
            notes.append(f"Contains {len(system_files)} system audio file(s)")
        
        special_files = [a for a in audio_sequence if a.file_type == 'special']
        if special_files:
            notes.append(f"Contains {len(special_files)} special audio file(s)")
        
        # Check for missing files
        missing_files = [a for a in audio_sequence if not a.exists_in_rloops]
        if missing_files:
            notes.append(f"⚠️ {len(missing_files)} audio files are missing from rloops directory")
        
        return notes
    
    def generate_elv_report(self, filepath: str) -> ELVSequenceReport:
        """Generate detailed sequence report for a single ELV file"""
        filename = os.path.basename(filepath)
        print(f"\n📊 Analyzing: {filename}")
        
        # Initialize report with defaults
        report = ELVSequenceReport(
            filename=filename,
            level_title="Unknown",
            elvd_version=0,
            total_audio_files=0,
            available_audio_files=0,
            missing_audio_files=0,
            total_duration=0.0,
            audio_sequence=[],
            type_breakdown={},
            complexity_score=0,
            analysis_notes=[]
        )
        
        if not os.path.exists(filepath):
            report.analysis_notes.append(f"❌ File not found: {filepath}")
            return report
        
        try:
            with open(filepath, 'rb') as f:
                file_data = f.read()
                
                # Check ELVD header
                if len(file_data) < 8 or file_data[:4] != b'ELVD':
                    report.analysis_notes.append("❌ Invalid ELVD header")
                    return report
                
                # Parse version
                version_data = file_data[4:8]
                report.elvd_version = struct.unpack('<H', version_data[:2])[0]
                
                # Extract level title
                text_data = file_data[8:].decode('ascii', errors='ignore')
                lines = text_data.split('\x00')
                for line in lines:
                    clean_line = ''.join(c for c in line if c.isprintable()).strip()
                    if len(clean_line) > 10 and not clean_line.endswith('.lbm'):
                        report.level_title = clean_line[:50]
                        break
                
                # Extract audio sequence
                audio_sequence_raw = self._extract_audio_sequence_from_data(file_data)
                
                # Build detailed audio sequence info
                total_duration = 0.0
                available_count = 0
                type_counts = {}
                
                for seq_pos, (audio_filename, binary_offset) in enumerate(audio_sequence_raw):
                    exists, alternative, metadata = self._check_audio_file_exists(audio_filename)
                    file_type = self._classify_audio_file(audio_filename)
                    
                    if exists:
                        available_count += 1
                        total_duration += metadata['duration']
                    
                    type_counts[file_type] = type_counts.get(file_type, 0) + 1
                    
                    audio_info = AudioSequenceInfo(
                        sequence_position=seq_pos,
                        filename=audio_filename,
                        file_type=file_type,
                        exists_in_rloops=exists,
                        binary_offset=binary_offset,
                        duration_seconds=metadata['duration'],
                        file_size_kb=metadata['size_kb'],
                        alternative_found=alternative if alternative != audio_filename.upper() else None
                    )
                    report.audio_sequence.append(audio_info)
                
                # Update report statistics
                report.total_audio_files = len(audio_sequence_raw)
                report.available_audio_files = available_count
                report.missing_audio_files = report.total_audio_files - available_count
                report.total_duration = total_duration
                report.type_breakdown = type_counts
                report.complexity_score = self._calculate_complexity_score(report.audio_sequence)
                report.analysis_notes = self._generate_analysis_notes(report.audio_sequence)
                
                print(f"  ✓ Found {report.total_audio_files} audio files, {available_count} available")
                print(f"  ⏱️ Total duration: {total_duration:.1f}s")
                print(f"  🏆 Complexity score: {report.complexity_score}")
                
        except Exception as e:
            report.analysis_notes.append(f"❌ Parse error: {str(e)}")
            print(f"  ❌ Error: {e}")
        
        return report
    
    def generate_all_reports(self) -> bool:
        """Generate sequence reports for all ELV files"""
        print("=" * 80)
        print("📊 ELV SEQUENCE REPORT GENERATOR")
        print("Analyzing WAV samples and ordering for all ELV files")
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
        print(f"📁 Found {len(elv_files)} ELV files to analyze")
        
        if not elv_files:
            print("❌ No ELV files found")
            return False
        
        # Generate reports for each file
        for i, filepath in enumerate(elv_files, 1):
            print(f"\n[{i}/{len(elv_files)}]", end=" ")
            report = self.generate_elv_report(filepath)
            self.reports.append(report)
        
        print(f"\n\n✅ Analysis complete: {len(self.reports)} reports generated")
        return True
    
    def save_individual_reports(self, output_dir: str = "reports") -> bool:
        """Save individual text reports for each ELV file"""
        try:
            os.makedirs(output_dir, exist_ok=True)
            
            for report in self.reports:
                if not report.audio_sequence:
                    continue
                    
                base_name = os.path.splitext(report.filename)[0]
                report_file = os.path.join(output_dir, f"{base_name}_sequence_report.txt")
                
                with open(report_file, 'w', encoding='utf-8') as f:
                    f.write("=" * 80 + "\n")
                    f.write(f"ELV SEQUENCE REPORT: {report.filename}\n")
                    f.write("=" * 80 + "\n\n")
                    
                    f.write(f"Level Title: {report.level_title}\n")
                    f.write(f"ELVD Version: {report.elvd_version}\n")
                    f.write(f"Total Audio Files: {report.total_audio_files}\n")
                    f.write(f"Available Files: {report.available_audio_files}\n")
                    f.write(f"Missing Files: {report.missing_audio_files}\n")
                    f.write(f"Total Duration: {report.total_duration:.1f} seconds\n")
                    f.write(f"Complexity Score: {report.complexity_score}\n\n")
                    
                    f.write("AUDIO SEQUENCE (Original ELV Order):\n")
                    f.write("-" * 80 + "\n")
                    f.write(f"{'Pos':<3} {'Filename':<20} {'Type':<15} {'Exists':<6} {'Duration':<8} {'Size':<8}\n")
                    f.write("-" * 80 + "\n")
                    
                    for audio in report.audio_sequence:
                        exists_str = "✓" if audio.exists_in_rloops else "✗"
                        duration_str = f"{audio.duration_seconds:.1f}s" if audio.duration_seconds > 0 else "N/A"
                        size_str = f"{audio.file_size_kb}KB" if audio.file_size_kb > 0 else "N/A"
                        
                        f.write(f"{audio.sequence_position:<3} {audio.filename:<20} {audio.file_type:<15} "
                               f"{exists_str:<6} {duration_str:<8} {size_str:<8}\n")
                    
                    f.write("\nTYPE BREAKDOWN:\n")
                    f.write("-" * 30 + "\n")
                    for file_type, count in report.type_breakdown.items():
                        f.write(f"{file_type:<20}: {count}\n")
                    
                    if report.analysis_notes:
                        f.write("\nANALYSIS NOTES:\n")
                        f.write("-" * 30 + "\n")
                        for note in report.analysis_notes:
                            f.write(f"• {note}\n")
            
            print(f"📄 Individual reports saved to '{output_dir}' directory")
            return True
            
        except Exception as e:
            print(f"❌ Error saving individual reports: {e}")
            return False
    
    def save_summary_report(self, output_file: str) -> bool:
        """Save comprehensive summary report"""
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write("=" * 100 + "\n")
                f.write("COMPREHENSIVE ELV SEQUENCE ANALYSIS SUMMARY\n")
                f.write(f"Generated: {datetime.now().isoformat()}\n")
                f.write("=" * 100 + "\n\n")
                
                # Overall statistics
                total_files = len(self.reports)
                total_audio_refs = sum(r.total_audio_files for r in self.reports)
                total_available = sum(r.available_audio_files for r in self.reports)
                total_duration = sum(r.total_duration for r in self.reports)
                
                f.write("OVERALL STATISTICS:\n")
                f.write("-" * 50 + "\n")
                f.write(f"Total ELV Files Analyzed: {total_files}\n")
                f.write(f"Total Audio References: {total_audio_refs}\n")
                f.write(f"Available Audio Files: {total_available}\n")
                f.write(f"Missing Audio Files: {total_audio_refs - total_available}\n")
                f.write(f"Total Audio Duration: {total_duration:.1f} seconds ({total_duration/60:.1f} minutes)\n\n")
                
                # Sort reports by complexity score
                sorted_reports = sorted(self.reports, key=lambda r: r.complexity_score, reverse=True)
                
                f.write("LEVELS BY COMPLEXITY (Highest to Lowest):\n")
                f.write("-" * 100 + "\n")
                f.write(f"{'Rank':<4} {'Level':<15} {'Title':<25} {'Files':<5} {'Avail':<5} {'Duration':<8} {'Score':<5}\n")
                f.write("-" * 100 + "\n")
                
                for i, report in enumerate(sorted_reports, 1):
                    title_short = report.level_title[:25] + "..." if len(report.level_title) > 25 else report.level_title
                    duration_str = f"{report.total_duration:.1f}s"
                    f.write(f"{i:<4} {report.filename:<15} {title_short:<25} {report.total_audio_files:<5} "
                           f"{report.available_audio_files:<5} {duration_str:<8} {report.complexity_score:<5}\n")
                
                f.write("\nDETAILED SEQUENCE BREAKDOWN:\n")
                f.write("=" * 100 + "\n")
                
                for report in sorted_reports:
                    if not report.audio_sequence:
                        continue
                        
                    f.write(f"\n{report.filename} - {report.level_title}\n")
                    f.write("-" * 80 + "\n")
                    f.write("Audio Sequence: ")
                    
                    sequence_str = " → ".join([
                        f"{audio.filename}" + ("❌" if not audio.exists_in_rloops else "")
                        for audio in report.audio_sequence[:10]  # Show first 10
                    ])
                    
                    if len(report.audio_sequence) > 10:
                        sequence_str += f" → ... ({len(report.audio_sequence) - 10} more)"
                    
                    f.write(sequence_str + "\n")
                    
                    if report.analysis_notes:
                        f.write("Notes: " + "; ".join(report.analysis_notes[:3]) + "\n")
            
            print(f"📋 Summary report saved: {output_file}")
            return True
            
        except Exception as e:
            print(f"❌ Error saving summary report: {e}")
            return False

def main():
    print("=" * 80)
    print("📊 ELV SEQUENCE REPORT GENERATOR")
    print("Generate comprehensive reports showing WAV samples and ordering")
    print("=" * 80)
    
    # Parse command line arguments
    elvrl_dir = sys.argv[1] if len(sys.argv) > 1 else "ELVRL"
    rloops_dir = sys.argv[2] if len(sys.argv) > 2 else "rloops"
    
    # Initialize generator
    generator = ELVSequenceReportGenerator(elvrl_dir, rloops_dir)
    
    # Generate all reports
    if not generator.generate_all_reports():
        print("❌ Report generation failed")
        sys.exit(1)
    
    # Save outputs
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Individual reports
    generator.save_individual_reports("elv_sequence_reports")
    
    # Summary report
    summary_file = f"elv_sequence_summary_{timestamp}.txt"
    generator.save_summary_report(summary_file)
    
    print(f"\n🎯 REPORT GENERATION COMPLETE!")
    print(f"📁 Individual reports: elv_sequence_reports/ directory")
    print(f"📋 Summary report: {summary_file}")
    print(f"🎵 All audio sequences shown in original ELV binary order")

if __name__ == "__main__":
    main()