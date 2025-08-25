"""
Data models for RT-STT
"""

from dataclasses import dataclass
from typing import Optional, List
from enum import Enum


class Language(str, Enum):
    """Supported languages"""
    ENGLISH = "en"
    SPANISH = "es"
    FRENCH = "fr"
    GERMAN = "de"
    ITALIAN = "it"
    PORTUGUESE = "pt"
    RUSSIAN = "ru"
    CHINESE = "zh"
    JAPANESE = "ja"
    KOREAN = "ko"
    AUTO = "auto"  # Auto-detect
    
    @classmethod
    def from_string(cls, lang: str) -> 'Language':
        """Convert string to Language enum"""
        lang = lang.lower()
        for member in cls:
            if member.value == lang:
                return member
        # Default to English if not found
        return cls.ENGLISH


@dataclass
class TranscriptionResult:
    """Result from speech-to-text transcription"""
    text: str
    confidence: float
    timestamp: int
    language: Optional[str] = None
    
    @property
    def is_final(self) -> bool:
        """Check if this is a final transcription"""
        return self.confidence > 0.0
    
    def __str__(self) -> str:
        return self.text


@dataclass
class Status:
    """Daemon status information"""
    listening: bool
    model: str
    language: str
    vad_enabled: bool
    clients: int = 0
    uptime: int = 0
    
    @property
    def is_active(self) -> bool:
        """Check if daemon is actively listening"""
        return self.listening
    
    @property
    def uptime_minutes(self) -> float:
        """Get uptime in minutes"""
        return self.uptime / 60.0
    
    @property
    def uptime_hours(self) -> float:
        """Get uptime in hours"""
        return self.uptime / 3600.0


@dataclass
class AudioConfig:
    """Audio configuration"""
    device_name: str = "MOTU M2"
    sample_rate: int = 16000
    channels: int = 1
    buffer_size_ms: int = 30
    input_channel_index: int = 1
    force_single_channel: bool = True


@dataclass
class VADConfig:
    """Voice Activity Detection configuration"""
    enabled: bool = True
    energy_threshold: float = 0.001
    speech_start_ms: int = 150
    speech_end_ms: int = 1000
    min_speech_ms: int = 500
    speech_start_threshold: float = 1.08
    speech_end_threshold: float = 0.85
    pre_speech_buffer_ms: int = 500
    noise_floor_adaptation_rate: float = 0.01
    use_adaptive_threshold: bool = True


@dataclass
class ModelConfig:
    """Whisper model configuration"""
    path: str
    language: str = "en"
    use_gpu: bool = True
    n_threads: int = 4
    beam_size: int = 5
    temperature: float = 0.0
    
    @property
    def model_name(self) -> str:
        """Extract model name from path"""
        import os
        return os.path.basename(self.path).replace('ggml-', '').replace('.bin', '')


@dataclass 
class DaemonConfig:
    """Complete daemon configuration"""
    audio: AudioConfig
    vad: VADConfig
    model: ModelConfig
    socket_path: str = "/tmp/rt-stt.sock"
    log_level: str = "info"
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            "stt": {
                "audio": {
                    "device_name": self.audio.device_name,
                    "sample_rate": self.audio.sample_rate,
                    "channels": self.audio.channels,
                    "buffer_size_ms": self.audio.buffer_size_ms,
                    "input_channel_index": self.audio.input_channel_index,
                    "force_single_channel": self.audio.force_single_channel
                },
                "vad": {
                    "enabled": self.vad.enabled,
                    "energy_threshold": self.vad.energy_threshold,
                    "speech_start_ms": self.vad.speech_start_ms,
                    "speech_end_ms": self.vad.speech_end_ms,
                    "min_speech_ms": self.vad.min_speech_ms,
                    "speech_start_threshold": self.vad.speech_start_threshold,
                    "speech_end_threshold": self.vad.speech_end_threshold,
                    "pre_speech_buffer_ms": self.vad.pre_speech_buffer_ms,
                    "noise_floor_adaptation_rate": self.vad.noise_floor_adaptation_rate,
                    "use_adaptive_threshold": self.vad.use_adaptive_threshold
                },
                "model": {
                    "path": self.model.path,
                    "language": self.model.language,
                    "use_gpu": self.model.use_gpu,
                    "n_threads": self.model.n_threads,
                    "beam_size": self.model.beam_size,
                    "temperature": self.model.temperature
                }
            },
            "ipc": {
                "socket_path": self.socket_path
            },
            "logging": {
                "level": self.log_level
            }
        }
