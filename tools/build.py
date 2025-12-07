#!/usr/bin/env python3
"""
Captured Portal - Build & Test Tool
Configures hardware, builds firmware, and runs test portals.
"""

import os
import sys
import time
import subprocess
import glob
import random
import base64
from pathlib import Path

# Try to import hakcer for cool effects
HAKCER_AVAILABLE = False
HAKCER_ERROR = None
try:
    import hakcer
    # Test if hakcer can actually work (check for terminal support)
    if hasattr(hakcer, 'show_banner') and hasattr(hakcer, 'print_text'):
        HAKCER_AVAILABLE = True
except ImportError as e:
    HAKCER_ERROR = f"Not installed: {e}"
except Exception as e:
    HAKCER_ERROR = f"Error: {e}"

# Available hakcer themes and effects
HAKCER_THEMES = ["matrix", "synthwave", "cyberpunk", "dracula", "nord", "gruvbox", "monokai"]
HAKCER_EFFECTS = ["decrypt", "typewriter", "glitch", "matrix_rain", "scan"]

def random_theme():
    """Get a random hakcer theme"""
    return random.choice(HAKCER_THEMES)

def random_effect():
    """Get a random hakcer effect"""
    return random.choice(HAKCER_EFFECTS)

def random_ap_suffix():
    """Generate a random base64 suffix from 'hakc' or '1337'"""
    words = ["hakc", "1337", "h4x0r", "c0de", "pwnd", "r00t"]
    word = random.choice(words)
    # Encode to base64 and take first 4-6 chars (remove padding)
    b64 = base64.b64encode(word.encode()).decode().rstrip('=')
    return b64[:random.randint(4, 6)]

# Paths
SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent
BANNER_FILE = PROJECT_DIR / "banner"
CONFIG_FILE = PROJECT_DIR / "include" / "config.h"
TEST_PORTAL_SCRIPT = SCRIPT_DIR / "test_portal.py"


# ANSI colors
class Colors:
    GREEN = '\033[92m'
    CYAN = '\033[96m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    MAGENTA = '\033[95m'
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'


def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')


def load_banner():
    """Load the ASCII banner from file"""
    if BANNER_FILE.exists():
        return BANNER_FILE.read_text()
    return """
    ╔═══════════════════════════════════════╗
    ║         CAPTURED PORTAL               ║
    ║     Captive Portal Hunter/Analyzer    ║
    ╚═══════════════════════════════════════╝
"""


def show_banner(theme=None, effect=None, randomize=True):
    """Display the banner with hakcer effects if available"""
    clear_screen()

    # Change to project dir for relative banner path
    original_dir = os.getcwd()
    os.chdir(PROJECT_DIR)

    if HAKCER_AVAILABLE:
        try:
            # Use unstable effect for the haKC aesthetic
            hakcer.show_banner(
                custom_file="banner",
                effect_name="unstable",
                theme="synthwave",
                hold_time=1.0
            )
        except Exception:
            # Fallback if hakcer has issues
            print(f"{Colors.GREEN}{load_banner()}{Colors.RESET}")
    else:
        print(f"{Colors.GREEN}{load_banner()}{Colors.RESET}")

    os.chdir(original_dir)


def hakcer_print(text, theme=None, effect="typewriter", speed="fast"):
    """Print text with hakcer effects if available"""
    if HAKCER_AVAILABLE:
        try:
            use_theme = theme if theme else random_theme()
            hakcer.print_text(
                text,
                effect_name=effect,
                theme=use_theme,
                speed_preference=speed
            )
            return
        except Exception:
            pass
    # Fallback
    print(text)


def info(message, use_hakcer=False):
    if use_hakcer and HAKCER_AVAILABLE:
        try:
            hakcer.print_text(f"[+] {message}", effect_name="typewriter", theme="matrix", speed_preference="fast")
            return
        except Exception:
            pass
    print(f"{Colors.GREEN}[+]{Colors.RESET} {message}")


def warn(message, use_hakcer=False):
    if use_hakcer and HAKCER_AVAILABLE:
        try:
            hakcer.print_text(f"[!] {message}", effect_name="glitch", theme="cyberpunk", speed_preference="fast")
            return
        except Exception:
            pass
    print(f"{Colors.YELLOW}[!]{Colors.RESET} {message}")


def error(message, use_hakcer=False):
    if use_hakcer and HAKCER_AVAILABLE:
        try:
            hakcer.print_text(f"[X] {message}", effect_name="glitch", theme="dracula", speed_preference="fast")
            return
        except Exception:
            pass
    print(f"{Colors.RED}[X]{Colors.RESET} {message}")


def step(message):
    print(f"{Colors.CYAN}[*]{Colors.RESET} {message}")


def section(title, use_hakcer=False):
    """Print a section header with optional hakcer effects"""
    if use_hakcer and HAKCER_AVAILABLE:
        try:
            hakcer.print_text(
                f"\n{'─' * 60}\n  {title}\n{'─' * 60}\n",
                effect_name="decrypt",
                theme=random_theme(),
                speed_preference="fast"
            )
            return
        except Exception:
            pass
    print(f"\n{Colors.MAGENTA}{'─' * 60}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.MAGENTA}  {title}{Colors.RESET}")
    print(f"{Colors.MAGENTA}{'─' * 60}{Colors.RESET}\n")


def prompt(message, default=None):
    """Styled prompt"""
    if default:
        result = input(f"{Colors.CYAN}[?]{Colors.RESET} {message} [{Colors.DIM}{default}{Colors.RESET}]: ").strip()
        return result if result else default
    return input(f"{Colors.CYAN}[?]{Colors.RESET} {message}: ").strip()


def prompt_yes_no(message, default=True):
    """Yes/no prompt"""
    default_str = "Y/n" if default else "y/N"
    result = input(f"{Colors.CYAN}[?]{Colors.RESET} {message} [{default_str}]: ").strip().lower()
    if not result:
        return default
    return result in ['y', 'yes']


# Hardware options
HARDWARE_OPTIONS = {
    "1": {
        "name": "LILYGO T-Display S3",
        "env": "lilygo-t-display-s3",
        "description": "1.9\" LCD, 2 buttons, battery support",
        "price": "~$20"
    },
    "2": {
        "name": "LILYGO T-Deck",
        "env": "lilygo-t-deck",
        "description": "2.8\" LCD, keyboard, trackball, LoRa option",
        "price": "~$50"
    },
    "3": {
        "name": "Waveshare ESP32-S3 LCD 1.47",
        "env": "waveshare-esp32s3-lcd147",
        "description": "USB dongle, 1.47\" LCD, SD card",
        "price": "~$13"
    },
    "4": {
        "name": "ESP32-S3 Round 1.28\"",
        "env": "esp32s3-round-128",
        "description": "Round display, compact, watch-like",
        "price": "~$15"
    },
    "5": {
        "name": "Waveshare Touch 1.69\"",
        "env": "waveshare-esp32s3-touch-169",
        "description": "Touch screen, IMU, RTC",
        "price": "~$25"
    }
}

# Color schemes
COLOR_SCHEMES = {
    "1": {"name": "Matrix", "value": "COLOR_MATRIX", "desc": "Classic green on black"},
    "2": {"name": "Synthwave", "value": "COLOR_SYNTHWAVE", "desc": "Cyan/magenta/purple"},
    "3": {"name": "Cyberpunk", "value": "COLOR_CYBERPUNK", "desc": "Hot pink/yellow"},
    "4": {"name": "Dracula", "value": "COLOR_DRACULA", "desc": "Purple theme"}
}

# Test portal types
PORTAL_TYPES = {
    "1": {"name": "hotel", "desc": "Hotel WiFi (room + last name)"},
    "2": {"name": "airport", "desc": "Airport WiFi (email)"},
    "3": {"name": "cafe", "desc": "Coffee shop (access code)"},
    "4": {"name": "conference", "desc": "Conference (badge ID + email)"},
    "5": {"name": "hospital", "desc": "Hospital (patient room + name)"}
}


def select_hardware():
    """Hardware selection menu"""
    section("SELECT HARDWARE")

    print(f"{Colors.DIM}Choose your ESP32 development board:{Colors.RESET}\n")

    for key, hw in HARDWARE_OPTIONS.items():
        print(f"  {Colors.CYAN}[{key}]{Colors.RESET} {Colors.BOLD}{hw['name']}{Colors.RESET}")
        print(f"      {Colors.DIM}{hw['description']}{Colors.RESET}")
        print(f"      {Colors.GREEN}{hw['price']}{Colors.RESET}\n")

    while True:
        choice = prompt("Select hardware (1-5)", "1")
        if choice in HARDWARE_OPTIONS:
            hw = HARDWARE_OPTIONS[choice]
            info(f"Selected: {hw['name']}")
            return hw
        error("Invalid selection. Please enter 1-5.")


def select_color_scheme():
    """Color scheme selection"""
    section("COLOR SCHEME")

    for key, scheme in COLOR_SCHEMES.items():
        print(f"  {Colors.CYAN}[{key}]{Colors.RESET} {scheme['name']} - {Colors.DIM}{scheme['desc']}{Colors.RESET}")

    while True:
        choice = prompt("Select color scheme (1-4)", "1")
        if choice in COLOR_SCHEMES:
            scheme = COLOR_SCHEMES[choice]
            info(f"Selected: {scheme['name']}")
            return scheme
        error("Invalid selection. Please enter 1-4.")


def configure_network():
    """Configure WiFi AP settings"""
    section("NETWORK CONFIGURATION")

    config = {}

    # AP SSID prefix
    config['ssid_prefix'] = prompt("AP SSID prefix", "CapturedPortal_")

    # Hidden SSID
    print(f"\n{Colors.DIM}Hidden SSID won't broadcast the network name (more covert){Colors.RESET}")
    config['hidden_ssid'] = prompt_yes_no("Hide SSID?", False)

    # AP Password
    print(f"\n{Colors.DIM}Leave password empty for open network (easier testing){Colors.RESET}")
    config['password'] = prompt("AP password (8+ chars or empty)", "")
    if config['password'] and len(config['password']) < 8:
        warn("Password must be 8+ characters. Setting to open network.")
        config['password'] = ""

    # Web server on battery
    config['web_on_battery'] = prompt_yes_no("Enable web server on battery? (uses more power)", False)

    return config


def configure_advanced():
    """Advanced configuration options"""
    section("ADVANCED OPTIONS")

    config = {}

    # Scan intervals
    config['battery_scan_interval'] = int(prompt("Battery mode scan interval (ms)", "10000"))
    config['usb_scan_interval'] = int(prompt("USB mode scan interval (ms)", "3000"))

    # Sleep timeouts
    config['idle_sleep'] = int(prompt("Idle sleep timeout (ms)", "60000"))
    config['deep_sleep'] = int(prompt("Deep sleep timeout (ms)", "300000"))

    # LLM
    config['llm_enabled'] = prompt_yes_no("Enable LLM analysis?", True)
    if config['llm_enabled']:
        print(f"\n{Colors.DIM}LLM on battery drains power faster but enables offline analysis{Colors.RESET}")
        config['llm_on_battery'] = prompt_yes_no("Enable LLM on battery? (uses more power)", False)
    else:
        config['llm_on_battery'] = False

    return config


def generate_config(hardware, color_scheme, network, advanced):
    """Generate the config.h file"""
    import re
    section("GENERATING CONFIGURATION")

    if not CONFIG_FILE.exists():
        error(f"Config file not found: {CONFIG_FILE}")
        return False

    # Read template
    config = CONFIG_FILE.read_text()

    # Use regex for robust replacements (matches any current value)
    regex_replacements = [
        (r'#define COLOR_SCHEME \w+', f'#define COLOR_SCHEME {color_scheme["value"]}'),
        (r'#define AP_SSID_PREFIX "[^"]*"', f'#define AP_SSID_PREFIX "{network["ssid_prefix"]}"'),
        (r'#define AP_HIDDEN (true|false)', f'#define AP_HIDDEN {"true" if network.get("hidden_ssid", False) else "false"}'),
        (r'#define AP_PASSWORD "[^"]*"', f'#define AP_PASSWORD "{network["password"]}"'),
        (r'#define WEB_SERVER_ON_BATTERY (true|false)', f'#define WEB_SERVER_ON_BATTERY {"true" if network["web_on_battery"] else "false"}'),
        (r'#define BATTERY_SCAN_INTERVAL \d+', f'#define BATTERY_SCAN_INTERVAL {advanced["battery_scan_interval"]}'),
        (r'#define USB_SCAN_INTERVAL \d+', f'#define USB_SCAN_INTERVAL {advanced["usb_scan_interval"]}'),
        (r'#define IDLE_SLEEP_TIMEOUT \d+', f'#define IDLE_SLEEP_TIMEOUT {advanced["idle_sleep"]}'),
        (r'#define DEEP_SLEEP_TIMEOUT \d+', f'#define DEEP_SLEEP_TIMEOUT {advanced["deep_sleep"]}'),
        (r'#define LLM_ENABLED (true|false)', f'#define LLM_ENABLED {"true" if advanced["llm_enabled"] else "false"}'),
        (r'#define LLM_ON_BATTERY (true|false)', f'#define LLM_ON_BATTERY {"true" if advanced.get("llm_on_battery", False) else "false"}'),
    ]

    for pattern, replacement in regex_replacements:
        config = re.sub(pattern, replacement, config)

    # Write config
    CONFIG_FILE.write_text(config)

    info(f"Configuration saved to {CONFIG_FILE}")
    info(f"  Color scheme: {color_scheme['name']}")
    info(f"  AP SSID: {network['ssid_prefix']}...")
    info(f"  Password: {'(open)' if not network['password'] else '***'}")
    return True


def success_animation(message="SUCCESS"):
    """Show a success animation with hakcer effects"""
    if HAKCER_AVAILABLE:
        try:
            hakcer.print_text(
                f"[+] {message}",
                effect_name=random.choice(["decrypt", "typewriter", "glitch"]),
                theme=random.choice(["matrix", "synthwave", "cyberpunk"]),
                speed_preference="fast"
            )
            return
        except Exception:
            pass
    print(f"{Colors.GREEN}[+] {message}{Colors.RESET}")


def error_animation(message="ERROR"):
    """Show an error animation with hakcer effects"""
    if HAKCER_AVAILABLE:
        try:
            hakcer.print_text(
                f"[X] {message}",
                effect_name="glitch",
                theme="dracula",
                speed_preference="fast"
            )
            return
        except Exception:
            pass
    print(f"{Colors.RED}[X] {message}{Colors.RESET}")


def check_platformio():
    """Check if PlatformIO is available"""
    try:
        result = subprocess.run(['pio', '--version'], capture_output=True, text=True)
        info(f"PlatformIO: {result.stdout.strip()}")
        return True
    except FileNotFoundError:
        error("PlatformIO not found. Install with: pip install platformio")
        return False


def build_firmware(hardware):
    """Build the firmware"""
    section("BUILDING FIRMWARE")

    env = hardware['env']

    info(f"Building for {hardware['name']} (env:{env})...")
    print()

    try:
        result = subprocess.run(
            ['pio', 'run', '-e', env],
            cwd=PROJECT_DIR,
            check=True
        )
        success_animation("Build successful!")
        return True
    except subprocess.CalledProcessError:
        error_animation("Build failed. Check the output above for errors.")
        return False


def list_serial_ports():
    """List available serial ports (filters out Bluetooth devices)"""
    ports = []

    # macOS - USB serial ports only
    ports.extend(glob.glob('/dev/cu.usbmodem*'))
    ports.extend(glob.glob('/dev/cu.usbserial*'))
    ports.extend(glob.glob('/dev/cu.wchusbserial*'))  # CH340 chips
    ports.extend(glob.glob('/dev/cu.SLAB*'))  # CP210x chips
    ports.extend(glob.glob('/dev/tty.usbmodem*'))
    ports.extend(glob.glob('/dev/tty.usbserial*'))

    # Linux
    ports.extend(glob.glob('/dev/ttyUSB*'))
    ports.extend(glob.glob('/dev/ttyACM*'))

    # Sort and dedupe
    ports = sorted(set(ports))
    return ports


def select_serial_port():
    """Let user select a serial port"""
    while True:
        ports = list_serial_ports()

        if not ports:
            warn("No serial ports detected!")
            print(f"{Colors.DIM}Make sure your device is connected via USB.{Colors.RESET}")
            print(f"\n{Colors.CYAN}Options:{Colors.RESET}")
            print(f"  {Colors.GREEN}[R]{Colors.RESET} Rescan for ports")
            print(f"  {Colors.GREEN}[A]{Colors.RESET} Try auto-detect anyway")
            print(f"  {Colors.GREEN}[Q]{Colors.RESET} Quit")

            while True:
                choice = input(f"\n{Colors.YELLOW}Choose [R/A/Q]:{Colors.RESET} ").strip().upper()
                if choice == 'R' or choice == '':
                    print(f"{Colors.CYAN}Rescanning...{Colors.RESET}")
                    break  # Rescan loop
                elif choice == 'A':
                    return None  # Let PlatformIO auto-detect
                elif choice == 'Q':
                    return False
                else:
                    print(f"{Colors.RED}Invalid choice.{Colors.RESET}")
            continue  # Go back to rescan

        print(f"\n{Colors.CYAN}Available serial ports:{Colors.RESET}")
        for i, port in enumerate(ports, 1):
            print(f"  {Colors.GREEN}[{i}]{Colors.RESET} {port}")
        print(f"  {Colors.GREEN}[R]{Colors.RESET} Rescan")
        print(f"  {Colors.GREEN}[A]{Colors.RESET} Auto-detect")

        while True:
            choice = input(f"\n{Colors.YELLOW}Select port [1-{len(ports)}/R/A]:{Colors.RESET} ").strip().upper()

            if choice == 'R':
                print(f"{Colors.CYAN}Rescanning...{Colors.RESET}")
                break  # Rescan loop
            elif choice == 'A' or choice == '':
                return None  # Auto-detect

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    return ports[idx]
            except ValueError:
                pass

            print(f"{Colors.RED}Invalid choice. Try again.{Colors.RESET}")


def wait_for_port_reappear(original_port, timeout=10):
    """Wait for device to reappear after reset, return the new port path"""
    import os

    step("Waiting for device to reappear...")

    # First, wait for the original port to disappear (confirms reset worked)
    disappear_timeout = 3
    start = time.time()
    while time.time() - start < disappear_timeout:
        if not os.path.exists(original_port):
            info("Device disconnected (reset confirmed)")
            break
        time.sleep(0.1)

    # Now wait for a port to appear
    start = time.time()
    last_ports = set()

    while time.time() - start < timeout:
        current_ports = set(list_serial_ports())

        # Check if original port came back
        if original_port in current_ports:
            info(f"Device reconnected: {original_port}")
            time.sleep(0.3)  # Brief settle time
            return original_port

        # Check for any new port (device might enumerate differently)
        new_ports = current_ports - last_ports
        if new_ports and last_ports:  # Only after first scan
            new_port = sorted(new_ports)[0]
            info(f"New device detected: {new_port}")
            time.sleep(0.3)  # Brief settle time
            return new_port

        last_ports = current_ports

        # Show progress dots
        elapsed = int(time.time() - start)
        if elapsed > 0 and elapsed % 2 == 0:
            print(f"{Colors.DIM}.{Colors.RESET}", end="", flush=True)

        time.sleep(0.2)

    print()  # Newline after dots
    warn(f"Timeout waiting for device (tried {timeout}s)")
    return None


def reset_to_bootloader(port):
    """Try to reset ESP32 into bootloader mode via software.
    Returns the port to use for upload (may be different after reset)."""
    if not port:
        return None

    info("Attempting software reset to bootloader mode...")
    reset_sent = False

    # Try using esptool to reset the device
    try:
        # Use esptool's chip_id command with reset - this triggers the bootloader
        result = subprocess.run(
            ['python3', '-m', 'esptool', '--port', port, '--before', 'default_reset', '--after', 'no_reset', 'chip_id'],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode == 0:
            info("Device reset via esptool!")
            reset_sent = True
    except subprocess.TimeoutExpired:
        warn("Reset timed out - device may need manual reset")
    except FileNotFoundError:
        # esptool not found as module, try direct command
        try:
            result = subprocess.run(
                ['esptool.py', '--port', port, '--before', 'default_reset', '--after', 'no_reset', 'chip_id'],
                capture_output=True,
                text=True,
                timeout=10
            )
            if result.returncode == 0:
                info("Device reset via esptool!")
                reset_sent = True
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
    except Exception:
        pass

    # Try pyserial DTR/RTS toggle as fallback if esptool didn't work
    if not reset_sent:
        try:
            import serial
            with serial.Serial(port, 115200, timeout=1) as ser:
                # Toggle DTR and RTS to trigger reset into bootloader
                # This mimics what esptool does
                ser.dtr = False
                ser.rts = True
                time.sleep(0.1)
                ser.dtr = True
                ser.rts = False
                time.sleep(0.05)
                ser.dtr = False
            info("Reset signal sent via serial!")
            reset_sent = True
        except ImportError:
            # pyserial not available
            warn("pyserial not available for reset")
        except Exception as e:
            warn(f"Serial reset failed: {e}")

    if not reset_sent:
        warn("Could not reset device - manual reset may be required")
        return port  # Return original port, let upload try anyway

    # Wait for device to reappear after reset
    new_port = wait_for_port_reappear(port, timeout=10)

    if new_port:
        return new_port
    else:
        # Fallback: return original port, maybe it's still valid
        warn("Using original port - device may need manual reset")
        return port


def erase_flash(port):
    """Erase ESP32 flash completely using esptool"""
    if not port:
        error("No port specified for erase")
        return False

    section("ERASING FLASH")
    warn("This will completely wipe the device!")
    print(f"{Colors.DIM}Hold BOOT button while pressing RESET if device doesn't respond{Colors.RESET}")

    if not prompt_yes_no("Proceed with flash erase?", True):
        return False

    info(f"Erasing flash on {port}...")
    print(f"{Colors.DIM}This may take 10-30 seconds...{Colors.RESET}")
    print()

    # Try esptool as Python module first
    try:
        result = subprocess.run(
            ['python3', '-m', 'esptool', '--port', port, '--chip', 'esp32s3', 'erase_flash'],
            cwd=PROJECT_DIR,
            timeout=60
        )
        if result.returncode == 0:
            success_animation("Flash erased successfully!")
            info("Device is now blank - ready for fresh upload")
            # Wait for device to reboot after erase
            time.sleep(2)
            return True
    except subprocess.TimeoutExpired:
        error("Erase timed out")
    except FileNotFoundError:
        pass
    except Exception as e:
        warn(f"esptool module failed: {e}")

    # Try esptool.py directly
    try:
        result = subprocess.run(
            ['esptool.py', '--port', port, '--chip', 'esp32s3', 'erase_flash'],
            cwd=PROJECT_DIR,
            timeout=60
        )
        if result.returncode == 0:
            success_animation("Flash erased successfully!")
            info("Device is now blank - ready for fresh upload")
            time.sleep(2)
            return True
    except subprocess.TimeoutExpired:
        error("Erase timed out")
    except FileNotFoundError:
        error("esptool not found - install with: pip install esptool")
    except Exception as e:
        error(f"Erase failed: {e}")

    return False


def upload_firmware(hardware, port=None):
    """Upload firmware to device"""
    env = hardware['env']

    while True:
        section("UPLOADING FIRMWARE")

        warn("Make sure your device is connected via USB!")
        print(f"{Colors.DIM}Tip: If upload fails, hold BOOT button and press RESET{Colors.RESET}")

        # Let user select port
        selected_port = select_serial_port()
        if selected_port is False:
            return False

        if not prompt_yes_no("Ready to upload?", True):
            return False

        info(f"Uploading to {hardware['name']}...")
        upload_port = selected_port
        if selected_port:
            info(f"Selected port: {selected_port}")
            # Try to reset device into bootloader mode before upload
            # This returns the port to use (may change after reset)
            upload_port = reset_to_bootloader(selected_port)
            if upload_port:
                info(f"Upload port: {upload_port}")
            else:
                warn("Could not detect port after reset - trying original")
                upload_port = selected_port
        else:
            info("Using auto-detect...")
        print()

        try:
            cmd = ['pio', 'run', '-e', env, '-t', 'upload']
            if upload_port:
                cmd.extend(['--upload-port', upload_port])

            subprocess.run(cmd, cwd=PROJECT_DIR, check=True)
            success_animation("Upload successful!")
            return True
        except subprocess.CalledProcessError:
            error_animation("Upload failed.")
            print()
            print(f"{Colors.CYAN}Options:{Colors.RESET}")
            print(f"  {Colors.GREEN}[R]{Colors.RESET} Retry with port re-detection")
            print(f"  {Colors.GREEN}[E]{Colors.RESET} Erase flash first (fixes stuck devices)")
            print(f"  {Colors.GREEN}[S]{Colors.RESET} Skip upload")
            print(f"  {Colors.GREEN}[Q]{Colors.RESET} Quit")

            while True:
                choice = input(f"\n{Colors.YELLOW}Choose [R/E/S/Q]:{Colors.RESET} ").strip().upper()
                if choice == 'R' or choice == '':
                    break  # Retry loop
                elif choice == 'E':
                    # Erase flash then retry
                    erase_port = select_serial_port()
                    if erase_port and erase_port is not False:
                        if erase_flash(erase_port):
                            info("Flash erased - retrying upload...")
                            break  # Retry upload
                    continue
                elif choice == 'S':
                    warn("Skipping firmware upload.")
                    return False
                elif choice == 'Q':
                    info("Exiting.")
                    sys.exit(0)
                else:
                    print(f"{Colors.RED}Invalid choice.{Colors.RESET}")


def upload_filesystem(hardware, selected_port=None):
    """Upload SPIFFS filesystem"""
    env = hardware['env']

    while True:
        section("UPLOADING WEB FILES")

        # Ask user to select port
        selected_port = select_serial_port()
        if selected_port is False:
            return False

        info("Uploading web interface to device filesystem...")
        upload_port = selected_port
        if selected_port:
            info(f"Selected port: {selected_port}")
            # Try to reset device into bootloader mode before upload
            # This returns the port to use (may change after reset)
            upload_port = reset_to_bootloader(selected_port)
            if upload_port:
                info(f"Upload port: {upload_port}")
            else:
                warn("Could not detect port after reset - trying original")
                upload_port = selected_port
        else:
            info("Using auto-detect...")
        print()

        try:
            cmd = ['pio', 'run', '-e', env, '-t', 'uploadfs']
            if upload_port:
                cmd.extend(['--upload-port', upload_port])

            subprocess.run(cmd, cwd=PROJECT_DIR, check=True)
            success_animation("Filesystem upload successful!")
            return True
        except subprocess.CalledProcessError:
            error_animation("Filesystem upload failed.")
            print()
            print(f"{Colors.CYAN}Options:{Colors.RESET}")
            print(f"  {Colors.GREEN}[R]{Colors.RESET} Retry with port re-detection")
            print(f"  {Colors.GREEN}[S]{Colors.RESET} Skip filesystem upload")
            print(f"  {Colors.GREEN}[Q]{Colors.RESET} Quit")

            while True:
                choice = input(f"\n{Colors.YELLOW}Choose [R/S/Q]:{Colors.RESET} ").strip().upper()
                if choice == 'R' or choice == '':
                    break  # Retry loop
                elif choice == 'S':
                    warn("Skipping filesystem upload.")
                    return False
                elif choice == 'Q':
                    info("Exiting.")
                    sys.exit(0)
                else:
                    print(f"{Colors.RED}Invalid choice.{Colors.RESET}")


def show_build_summary(hardware, network):
    """Show final build summary"""
    section("BUILD COMPLETE")

    if HAKCER_AVAILABLE:
        try:
            hakcer.show_banner(
                custom_text="READY",
                effect_name=random_effect(),
                theme=random_theme(),
                speed_preference="fast",
                hold_time=0.8
            )
        except Exception:
            pass

    # Generate a random AP suffix for display
    ap_suffix = random_ap_suffix()

    print(f"""
{Colors.GREEN}Your Captured Portal device is ready!{Colors.RESET}

{Colors.CYAN}Hardware:{Colors.RESET} {hardware['name']}
{Colors.CYAN}WiFi AP:{Colors.RESET} {network['ssid_prefix']}{ap_suffix}
{Colors.CYAN}Password:{Colors.RESET} {'(open)' if not network['password'] else network['password']}
{Colors.CYAN}Dashboard:{Colors.RESET} http://192.168.4.1

{Colors.YELLOW}To connect:{Colors.RESET}
  1. Power on the device
  2. Connect to the WiFi network: {network['ssid_prefix']}{ap_suffix}
  3. Open http://192.168.4.1 in your browser

{Colors.YELLOW}Physical controls:{Colors.RESET}
  - Short press: Navigate / Select
  - Long press: Start scan / Connect

{Colors.MAGENTA}Happy hunting!{Colors.RESET}
""")


def menu_build():
    """Hardware setup and build menu"""
    show_banner()  # Random theme/effect

    if HAKCER_AVAILABLE:
        try:
            hakcer.print_text(
                "  Build & Flash Firmware",
                effect_name="decrypt",
                theme=random_theme(),
                speed_preference="fast"
            )
        except Exception:
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
            print(f"{Colors.GREEN}  Build & Flash Firmware{Colors.RESET}")
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")
    else:
        print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
        print(f"{Colors.GREEN}  Build & Flash Firmware{Colors.RESET}")
        print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")

    if not check_platformio():
        return

    # Hardware selection
    hardware = select_hardware()

    # Color scheme
    color_scheme = select_color_scheme()

    # Network configuration
    network = configure_network()

    # Advanced options
    if prompt_yes_no("Configure advanced options?", False):
        advanced = configure_advanced()
    else:
        advanced = {
            'battery_scan_interval': 10000,
            'usb_scan_interval': 3000,
            'idle_sleep': 60000,
            'deep_sleep': 300000,
            'llm_enabled': True,
            'llm_on_battery': False
        }

    # Generate config
    generate_config(hardware, color_scheme, network, advanced)

    # Build
    if not build_firmware(hardware):
        return

    # Upload
    if prompt_yes_no("Upload firmware to device now?", True):
        if upload_firmware(hardware):
            # Upload filesystem
            if prompt_yes_no("Upload web interface files?", True):
                upload_filesystem(hardware)

    # Summary
    show_build_summary(hardware, network)


def menu_test_portal():
    """Test portal server menu"""
    show_banner()  # Random theme/effect

    if HAKCER_AVAILABLE:
        try:
            hakcer.print_text(
                "  Test Captive Portal Server",
                effect_name="typewriter",
                theme=random_theme(),
                speed_preference="fast"
            )
        except Exception:
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
            print(f"{Colors.GREEN}  Test Captive Portal Server{Colors.RESET}")
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")
    else:
        print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
        print(f"{Colors.GREEN}  Test Captive Portal Server{Colors.RESET}")
        print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")

    section("SELECT PORTAL TYPE")

    for key, portal in PORTAL_TYPES.items():
        print(f"  {Colors.CYAN}[{key}]{Colors.RESET} {portal['name']:12} - {Colors.DIM}{portal['desc']}{Colors.RESET}")

    print()

    while True:
        choice = prompt("Select portal type (1-5)", "1")
        if choice in PORTAL_TYPES:
            portal = PORTAL_TYPES[choice]
            info(f"Selected: {portal['name']}")
            break
        error("Invalid selection. Please enter 1-5.")

    # Port selection
    port = prompt("Port to run on (80 requires sudo)", "8080")

    print()
    info(f"Starting {portal['name']} portal on port {port}...")
    print()

    # Run test portal
    try:
        subprocess.run(
            [sys.executable, str(TEST_PORTAL_SCRIPT), '-t', portal['name'], '-p', port],
            cwd=PROJECT_DIR
        )
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}[!] Server stopped{Colors.RESET}")


def menu_serial_monitor():
    """Serial monitor for debugging"""
    show_banner()

    section("SERIAL MONITOR")

    selected_port = select_serial_port()
    if selected_port is False or selected_port is None:
        return

    baud = prompt("Baud rate", "115200")

    info(f"Opening serial monitor on {selected_port} at {baud} baud...")
    print(f"{Colors.DIM}Press Ctrl+C to exit{Colors.RESET}\n")
    print(f"{Colors.CYAN}{'─' * 60}{Colors.RESET}")

    try:
        # Use PlatformIO's device monitor
        subprocess.run(
            ['pio', 'device', 'monitor', '-p', selected_port, '-b', baud],
            cwd=PROJECT_DIR
        )
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}[!] Monitor closed{Colors.RESET}")
    except FileNotFoundError:
        # Fallback to screen or minicom
        warn("PlatformIO monitor not available, trying screen...")
        try:
            subprocess.run(['screen', selected_port, baud])
        except FileNotFoundError:
            error("No serial monitor available. Install PlatformIO or screen.")


def main_menu():
    """Main menu"""
    while True:
        show_banner()  # Random theme/effect each time

        # Title with hakcer effect
        if HAKCER_AVAILABLE:
            try:
                hakcer.print_text(
                    "  Captured Portal - Build Tool",
                    effect_name=random_effect(),
                    theme=random_theme(),
                    speed_preference="fast"
                )
                print()
            except Exception:
                print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
                print(f"{Colors.GREEN}  Captured Portal - Build Tool{Colors.RESET}")
                print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")
        else:
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}")
            print(f"{Colors.GREEN}  Captured Portal - Build Tool{Colors.RESET}")
            print(f"{Colors.CYAN}{'═' * 60}{Colors.RESET}\n")

        print(f"  {Colors.CYAN}[1]{Colors.RESET} {Colors.BOLD}Build & Flash Firmware{Colors.RESET}")
        print(f"      {Colors.DIM}Configure hardware, build, and upload to device{Colors.RESET}\n")

        print(f"  {Colors.CYAN}[2]{Colors.RESET} {Colors.BOLD}Start Test Portal{Colors.RESET}")
        print(f"      {Colors.DIM}Run a fake captive portal for testing{Colors.RESET}\n")

        print(f"  {Colors.CYAN}[3]{Colors.RESET} {Colors.BOLD}Serial Monitor{Colors.RESET}")
        print(f"      {Colors.DIM}View device debug output (troubleshooting){Colors.RESET}\n")

        print(f"  {Colors.CYAN}[q]{Colors.RESET} {Colors.BOLD}Quit{Colors.RESET}\n")

        # Show hakcer status
        if HAKCER_AVAILABLE:
            print(f"  {Colors.DIM}hakcer: {Colors.GREEN}enabled{Colors.RESET}")
        else:
            print(f"  {Colors.DIM}hakcer: {Colors.YELLOW}disabled{Colors.RESET} {Colors.DIM}({HAKCER_ERROR or 'unknown'}){Colors.RESET}")
        print()

        choice = prompt("Select option", "1")

        if choice == "1":
            menu_build()
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        elif choice == "2":
            menu_test_portal()
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        elif choice == "3":
            menu_serial_monitor()
            input(f"\n{Colors.DIM}Press Enter to continue...{Colors.RESET}")
        elif choice.lower() == "q":
            print(f"\n{Colors.GREEN}Goodbye!{Colors.RESET}\n")
            break
        else:
            error("Invalid selection")
            time.sleep(1)


if __name__ == "__main__":
    try:
        main_menu()
    except KeyboardInterrupt:
        print(f"\n\n{Colors.YELLOW}[!] Cancelled{Colors.RESET}")
        sys.exit(0)
