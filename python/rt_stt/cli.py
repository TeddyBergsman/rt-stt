#!/usr/bin/env python3
"""
RT-STT Command Line Interface
"""

import sys
import argparse
import json
import signal
import time
from datetime import datetime
from typing import Optional
import logging

from .client import RTSTTClient, ConnectionError, CommandError
from .models import TranscriptionResult, Status, Language


# ANSI color codes
class Colors:
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'


def setup_logging(verbose: bool = False):
    """Configure logging"""
    level = logging.DEBUG if verbose else logging.WARNING
    logging.basicConfig(
        level=level,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )


def print_colored(text: str, color: str = '', bold: bool = False):
    """Print colored text"""
    if bold:
        text = f"{Colors.BOLD}{text}{Colors.ENDC}"
    if color:
        text = f"{color}{text}{Colors.ENDC}"
    print(text)


def format_timestamp(timestamp: int) -> str:
    """Format Unix timestamp to readable time"""
    dt = datetime.fromtimestamp(timestamp / 1000)  # Convert from ms
    return dt.strftime('%H:%M:%S')


def stream_transcriptions(args):
    """Stream transcriptions from daemon"""
    client = RTSTTClient(socket_path=args.socket)
    
    # Handle Ctrl+C gracefully
    def signal_handler(sig, frame):
        print("\nStopping...")
        client.disconnect()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    
    try:
        client.connect()
        
        if not args.quiet:
            print_colored("Connected to RT-STT daemon", Colors.GREEN)
            print_colored("Streaming transcriptions... (Press Ctrl+C to stop)", Colors.BLUE)
            print()
        
        def on_transcription(result: TranscriptionResult):
            if args.json:
                # JSON output
                output = {
                    'text': result.text,
                    'confidence': result.confidence,
                    'timestamp': result.timestamp
                }
                print(json.dumps(output))
            else:
                # Human-readable output
                text = result.text
                if args.timestamps:
                    time_str = format_timestamp(result.timestamp)
                    text = f"[{time_str}] {text}"
                
                if args.confidence:
                    text = f"{text} (confidence: {result.confidence:.2f})"
                
                print(text)
                
                # Flush output for piping
                sys.stdout.flush()
        
        def on_error(error: str):
            print_colored(f"Error: {error}", Colors.RED, file=sys.stderr)
        
        def on_connection(connected: bool):
            if not args.quiet:
                if connected:
                    print_colored("Reconnected to daemon", Colors.GREEN)
                else:
                    print_colored("Disconnected from daemon", Colors.YELLOW)
        
        client.on_transcription(on_transcription)
        client.on_error(on_error)
        client.on_connection(on_connection)
        
        client.start_listening()
        
        # Keep running
        while True:
            time.sleep(1)
            
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        pass
    finally:
        client.disconnect()
    
    return 0


def get_status(args):
    """Get daemon status"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        status = client.get_status()
        
        if args.json:
            # JSON output
            output = {
                'listening': status.listening,
                'model': status.model,
                'language': status.language,
                'vad_enabled': status.vad_enabled,
                'clients': status.clients,
                'uptime': status.uptime
            }
            print(json.dumps(output, indent=2))
        else:
            # Human-readable output
            print_colored("RT-STT Daemon Status", Colors.BOLD)
            print_colored("=" * 30, Colors.BOLD)
            
            status_color = Colors.GREEN if status.listening else Colors.RED
            print(f"Listening: {Colors.BOLD}{status_color}{('Yes' if status.listening else 'No')}{Colors.ENDC}")
            print(f"Model: {status.model}")
            print(f"Language: {status.language}")
            print(f"VAD Enabled: {'Yes' if status.vad_enabled else 'No'}")
            
            if status.clients > 0:
                print(f"Connected Clients: {status.clients}")
            
            if status.uptime > 0:
                if status.uptime < 3600:
                    uptime_str = f"{status.uptime_minutes:.1f} minutes"
                else:
                    uptime_str = f"{status.uptime_hours:.1f} hours"
                print(f"Uptime: {uptime_str}")
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def send_command(args):
    """Send command to daemon"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        
        if args.command == 'pause':
            client.pause()
            print_colored("Listening paused", Colors.GREEN)
            
        elif args.command == 'resume':
            client.resume()
            print_colored("Listening resumed", Colors.GREEN)
            
        elif args.command == 'set-language':
            if not args.value:
                print_colored("Error: Language code required", Colors.RED, file=sys.stderr)
                return 1
            client.set_language(args.value[0])
            print_colored(f"Language set to: {args.value[0]}", Colors.GREEN)
            
        elif args.command == 'set-model':
            if not args.value:
                print_colored("Error: Model name required", Colors.RED, file=sys.stderr)
                return 1
            client.set_model(args.value[0])
            print_colored(f"Model set to: {args.value[0]}", Colors.GREEN)
            
        elif args.command == 'set-vad-sensitivity':
            if not args.value:
                print_colored("Error: Sensitivity value required", Colors.RED, file=sys.stderr)
                return 1
            try:
                sensitivity = float(args.value[0])
                client.set_vad_sensitivity(sensitivity)
                print_colored(f"VAD sensitivity set to: {sensitivity}", Colors.GREEN)
            except ValueError:
                print_colored("Error: Invalid sensitivity value", Colors.RED, file=sys.stderr)
                return 1
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def monitor(args):
    """Monitor daemon with live updates"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        
        print_colored("RT-STT Monitor", Colors.BOLD)
        print_colored("Press Ctrl+C to exit", Colors.BLUE)
        print()
        
        # Track statistics
        transcription_count = 0
        total_confidence = 0.0
        
        def on_transcription(result: TranscriptionResult):
            nonlocal transcription_count, total_confidence
            transcription_count += 1
            total_confidence += result.confidence
            
            # Clear line and print stats
            avg_confidence = total_confidence / transcription_count if transcription_count > 0 else 0
            
            print(f"\r{Colors.GREEN}Transcriptions: {transcription_count} | "
                  f"Avg Confidence: {avg_confidence:.2f} | "
                  f"Latest: {result.text[:50]}...{Colors.ENDC}", end='', flush=True)
        
        client.on_transcription(on_transcription)
        client.start_listening()
        
        # Keep running
        while True:
            time.sleep(0.1)
            
    except ConnectionError as e:
        print_colored(f"\nConnection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\n")
    finally:
        client.disconnect()
    
    return 0


def get_configuration(args):
    """Get current daemon configuration"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        config = client.get_config()
        
        if args.json:
            print(json.dumps(config, indent=2))
        else:
            print_colored("RT-STT Configuration", Colors.BOLD)
            print_colored("=" * 30, Colors.BOLD)
            print(json.dumps(config, indent=2))
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def set_configuration(args):
    """Update daemon configuration"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        # Parse config JSON
        try:
            config = json.loads(args.config_json)
        except json.JSONDecodeError as e:
            print_colored(f"Invalid JSON: {e}", Colors.RED, file=sys.stderr)
            return 1
        
        client.connect()
        result = client.set_config(config, save=not args.no_save)
        
        print_colored("Configuration updated successfully", Colors.GREEN)
        if hasattr(args, 'no_save') and args.no_save:
            print_colored("(Changes not saved to file)", Colors.YELLOW)
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def set_vad_config(args):
    """Update VAD configuration"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        
        # Build VAD config from arguments
        vad_config = {}
        if args.energy_threshold is not None:
            vad_config['energy_threshold'] = args.energy_threshold
        if args.speech_start_ms is not None:
            vad_config['speech_start_ms'] = args.speech_start_ms
        if args.speech_end_ms is not None:
            vad_config['speech_end_ms'] = args.speech_end_ms
        if args.min_speech_ms is not None:
            vad_config['min_speech_ms'] = args.min_speech_ms
        if args.start_threshold is not None:
            vad_config['speech_start_threshold'] = args.start_threshold
        if args.end_threshold is not None:
            vad_config['speech_end_threshold'] = args.end_threshold
        
        if not vad_config:
            print_colored("No VAD settings specified", Colors.RED, file=sys.stderr)
            return 1
        
        client.set_vad_config(**vad_config)
        print_colored("VAD configuration updated", Colors.GREEN)
        
        # Show what was updated
        for key, value in vad_config.items():
            print(f"  {key}: {value}")
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def get_metrics(args):
    """Get performance metrics"""
    client = RTSTTClient(socket_path=args.socket)
    
    try:
        client.connect()
        metrics = client.get_metrics()
        
        if args.json:
            print(json.dumps(metrics, indent=2))
        else:
            print_colored("RT-STT Performance Metrics", Colors.BOLD)
            print_colored("=" * 30, Colors.BOLD)
            print(f"Average Latency: {metrics.get('avg_latency_ms', 0):.1f} ms")
            print(f"Average RTF: {metrics.get('avg_rtf', 0):.2f}")
            print(f"CPU Usage: {metrics.get('cpu_usage', 0):.1f}%")
            print(f"Memory Usage: {metrics.get('memory_usage_mb', 0)} MB")
            print(f"Transcriptions: {metrics.get('transcriptions_count', 0)}")
        
        client.disconnect()
        return 0
        
    except ConnectionError as e:
        print_colored(f"Connection error: {e}", Colors.RED, file=sys.stderr)
        return 1
    except CommandError as e:
        print_colored(f"Command error: {e}", Colors.RED, file=sys.stderr)
        return 1


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='RT-STT Command Line Interface',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  rt-stt-cli stream                    # Stream transcriptions
  rt-stt-cli stream -j                 # Output as JSON
  rt-stt-cli stream -t                 # Include timestamps
  rt-stt-cli status                    # Check daemon status
  rt-stt-cli pause                     # Pause listening
  rt-stt-cli resume                    # Resume listening
  rt-stt-cli set-language es           # Set Spanish
  rt-stt-cli monitor                   # Live monitoring
        """
    )
    
    parser.add_argument('-s', '--socket', 
                       default='/tmp/rt-stt.sock',
                       help='Socket path (default: /tmp/rt-stt.sock)')
    
    parser.add_argument('-v', '--verbose',
                       action='store_true',
                       help='Enable verbose logging')
    
    subparsers = parser.add_subparsers(dest='action', help='Action to perform')
    
    # Stream command
    stream_parser = subparsers.add_parser('stream', help='Stream transcriptions')
    stream_parser.add_argument('-j', '--json', action='store_true',
                              help='Output as JSON')
    stream_parser.add_argument('-t', '--timestamps', action='store_true',
                              help='Include timestamps')
    stream_parser.add_argument('-c', '--confidence', action='store_true',
                              help='Include confidence scores')
    stream_parser.add_argument('-q', '--quiet', action='store_true',
                              help='Suppress status messages')
    
    # Status command
    status_parser = subparsers.add_parser('status', help='Get daemon status')
    status_parser.add_argument('-j', '--json', action='store_true',
                              help='Output as JSON')
    
    # Control commands
    subparsers.add_parser('pause', help='Pause listening')
    subparsers.add_parser('resume', help='Resume listening')
    
    # Configuration commands
    lang_parser = subparsers.add_parser('set-language', help='Set language')
    lang_parser.add_argument('value', nargs=1, help='Language code (e.g., en, es)')
    
    model_parser = subparsers.add_parser('set-model', help='Set model')
    model_parser.add_argument('value', nargs=1, help='Model name')
    
    vad_parser = subparsers.add_parser('set-vad-sensitivity', help='Set VAD sensitivity')
    vad_parser.add_argument('value', nargs=1, help='Sensitivity value (e.g., 1.08)')
    
    # Monitor command
    subparsers.add_parser('monitor', help='Monitor daemon with live stats')
    
    # Configuration commands
    get_config_parser = subparsers.add_parser('get-config', help='Get current configuration')
    get_config_parser.add_argument('-j', '--json', action='store_true', help='Output as JSON')
    
    config_parser = subparsers.add_parser('set-config', help='Update configuration')
    config_parser.add_argument('config_json', help='Configuration JSON')
    config_parser.add_argument('--no-save', action='store_true', help='Don\'t save to file')
    
    vad_parser = subparsers.add_parser('set-vad', help='Update VAD settings')
    vad_parser.add_argument('--energy-threshold', type=float, help='Energy threshold')
    vad_parser.add_argument('--speech-start-ms', type=int, help='Speech start time')
    vad_parser.add_argument('--speech-end-ms', type=int, help='Speech end time')
    vad_parser.add_argument('--min-speech-ms', type=int, help='Minimum speech duration')
    vad_parser.add_argument('--start-threshold', type=float, help='Start threshold multiplier')
    vad_parser.add_argument('--end-threshold', type=float, help='End threshold multiplier')
    
    get_metrics_parser = subparsers.add_parser('get-metrics', help='Get performance metrics')
    get_metrics_parser.add_argument('-j', '--json', action='store_true', help='Output as JSON')
    
    # Parse arguments
    args = parser.parse_args()
    
    # Setup logging
    setup_logging(args.verbose)
    
    # Default to stream if no action specified
    if not args.action:
        args.action = 'stream'
        args.json = False
        args.timestamps = False
        args.confidence = False
        args.quiet = False
    
    # Route to appropriate handler
    if args.action == 'stream':
        return stream_transcriptions(args)
    elif args.action == 'status':
        return get_status(args)
    elif args.action == 'monitor':
        return monitor(args)
    elif args.action == 'get-config':
        return get_configuration(args)
    elif args.action == 'set-config':
        return set_configuration(args)
    elif args.action == 'set-vad':
        return set_vad_config(args)
    elif args.action == 'get-metrics':
        return get_metrics(args)
    elif args.action in ['pause', 'resume', 'set-language', 'set-model', 'set-vad-sensitivity']:
        args.command = args.action
        return send_command(args)
    else:
        parser.print_help()
        return 1


if __name__ == '__main__':
    sys.exit(main())
