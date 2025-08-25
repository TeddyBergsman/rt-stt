"""
RT-STT Python Client Library
Real-time Speech-to-Text client for macOS
"""

from .client import RTSTTClient, RTSTTError, ConnectionError, CommandError
from .models import TranscriptionResult, Status, Language

__version__ = "1.0.0"
__all__ = [
    "RTSTTClient",
    "RTSTTError",
    "ConnectionError", 
    "CommandError",
    "TranscriptionResult",
    "Status",
    "Language"
]
