#!/usr/bin/env python3
"""
Captured Portal - Test Captive Portal Server
Simulates various captive portal types for testing the device

Run this on your computer, connect to your computer's WiFi hotspot,
and the device will detect and analyze this test portal.
"""

import os
import sys
import argparse
import socket
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs, urlparse
from pathlib import Path
import json
import random

# Check for hakcer library
try:
    import hakcer
    HAKCER_AVAILABLE = True
except ImportError:
    HAKCER_AVAILABLE = False

# Path to the banner file
BANNER_FILE = Path(__file__).parent.parent / "banner"

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

# Portal templates
PORTAL_TEMPLATES = {
    "hotel": {
        "name": "Grand Hotel WiFi",
        "title": "Grand Hotel - Guest WiFi Access",
        "fields": ["room_number", "last_name"],
        "valid_rooms": ["101", "102", "103", "201", "202", "203", "301", "302", "303", "420"],
        "valid_names": ["smith", "johnson", "guest", "test", "demo"],
        "html": """
<!DOCTYPE html>
<html>
<head>
    <title>Grand Hotel - Guest WiFi Access</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background: #f5f5f5; margin: 0; padding: 20px; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #8B4513; margin-bottom: 10px; }
        h2 { color: #666; font-weight: normal; font-size: 16px; margin-bottom: 30px; }
        label { display: block; margin-bottom: 5px; color: #333; font-weight: bold; }
        input[type="text"], input[type="number"] { width: 100%; padding: 12px; margin-bottom: 20px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { width: 100%; padding: 14px; background: #8B4513; color: white; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }
        button:hover { background: #A0522D; }
        .logo { text-align: center; margin-bottom: 20px; }
        .error { background: #ffebee; color: #c62828; padding: 10px; border-radius: 4px; margin-bottom: 20px; display: none; }
        .footer { text-align: center; margin-top: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <h1>Grand Hotel</h1>
            <h2>Guest WiFi Access</h2>
        </div>
        <div class="error" id="error">Invalid room number or name. Please try again.</div>
        <form method="POST" action="/login">
            <label for="room">Room Number</label>
            <input type="text" id="room" name="room_number" placeholder="e.g., 101" required>

            <label for="name">Last Name</label>
            <input type="text" id="name" name="last_name" placeholder="Guest last name" required>

            <input type="checkbox" name="terms" required> I accept the terms of service

            <button type="submit">Connect to WiFi</button>
        </form>
        <div class="footer">
            Managed by HotelWiFi Systems<br>
            &copy; 2024 Grand Hotel. All rights reserved.
        </div>
    </div>
</body>
</html>
"""
    },

    "airport": {
        "name": "Airport Free WiFi",
        "title": "Airport Free WiFi - Connect",
        "fields": ["email"],
        "valid_emails": ["*"],  # Accept any email
        "html": """
<!DOCTYPE html>
<html>
<head>
    <title>Airport Free WiFi - Connect</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: linear-gradient(135deg, #1a5276, #2980b9); min-height: 100vh; margin: 0; display: flex; align-items: center; justify-content: center; }
        .container { background: white; padding: 40px; border-radius: 12px; max-width: 380px; text-align: center; }
        h1 { color: #1a5276; margin-bottom: 5px; }
        p { color: #666; margin-bottom: 30px; }
        input[type="email"] { width: 100%; padding: 14px; margin-bottom: 20px; border: 2px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
        button { width: 100%; padding: 14px; background: #2980b9; color: white; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; }
        .terms { font-size: 12px; color: #999; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Welcome!</h1>
        <p>Connect to Airport Free WiFi</p>
        <form method="POST" action="/login">
            <input type="email" name="email" placeholder="Enter your email" required>
            <button type="submit">Connect Now</button>
        </form>
        <p class="terms">By connecting, you agree to our terms of service and privacy policy. Free WiFi for 2 hours.</p>
    </div>
</body>
</html>
"""
    },

    "cafe": {
        "name": "Coffee House WiFi",
        "title": "Coffee House - Free WiFi",
        "fields": ["access_code"],
        "valid_codes": ["COFFEE2024", "LATTE", "WIFI", "1234", "guest"],
        "html": """
<!DOCTYPE html>
<html>
<head>
    <title>Coffee House - Free WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Georgia, serif; background: #3e2723; color: white; min-height: 100vh; margin: 0; display: flex; align-items: center; justify-content: center; }
        .container { background: rgba(255,255,255,0.1); padding: 40px; border-radius: 20px; max-width: 350px; text-align: center; backdrop-filter: blur(10px); }
        h1 { font-size: 28px; margin-bottom: 10px; }
        p { opacity: 0.8; margin-bottom: 30px; }
        input { width: 100%; padding: 14px; margin-bottom: 20px; border: none; border-radius: 25px; box-sizing: border-box; font-size: 16px; text-align: center; }
        button { width: 100%; padding: 14px; background: #8d6e63; color: white; border: none; border-radius: 25px; font-size: 16px; cursor: pointer; }
        .hint { font-size: 12px; opacity: 0.6; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Coffee House</h1>
        <p>Enter the access code from your receipt</p>
        <form method="POST" action="/login">
            <input type="text" name="access_code" placeholder="Access Code" required>
            <button type="submit">Connect</button>
        </form>
        <p class="hint">Ask your barista for the daily code!</p>
    </div>
</body>
</html>
"""
    },

    "conference": {
        "name": "TechConf 2024 WiFi",
        "title": "TechConf 2024 - Attendee WiFi",
        "fields": ["badge_id", "email"],
        "valid_badges": ["TC001", "TC002", "TC100", "TC200", "TC999", "SPEAKER", "VIP"],
        "html": """
<!DOCTYPE html>
<html>
<head>
    <title>TechConf 2024 - Attendee WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Helvetica Neue', sans-serif; background: #0d0d0d; color: white; min-height: 100vh; margin: 0; display: flex; align-items: center; justify-content: center; }
        .container { background: linear-gradient(145deg, #1a1a2e, #16213e); padding: 40px; border-radius: 16px; max-width: 400px; border: 1px solid #0f3460; }
        h1 { background: linear-gradient(90deg, #e94560, #0f3460); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 5px; }
        p { color: #888; margin-bottom: 30px; }
        label { display: block; margin-bottom: 5px; color: #e94560; font-size: 12px; text-transform: uppercase; }
        input { width: 100%; padding: 14px; margin-bottom: 20px; background: rgba(255,255,255,0.05); border: 1px solid #0f3460; border-radius: 8px; color: white; box-sizing: border-box; }
        button { width: 100%; padding: 14px; background: linear-gradient(90deg, #e94560, #0f3460); color: white; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; }
        .info { background: rgba(233, 69, 96, 0.1); padding: 15px; border-radius: 8px; margin-top: 20px; font-size: 12px; color: #e94560; }
    </style>
</head>
<body>
    <div class="container">
        <h1>TECHCONF 2024</h1>
        <p>Attendee WiFi Access</p>
        <form method="POST" action="/login">
            <label>Badge ID</label>
            <input type="text" name="badge_id" placeholder="TC001" required>

            <label>Email</label>
            <input type="email" name="email" placeholder="attendee@email.com" required>

            <button type="submit">Access Network</button>
        </form>
        <div class="info">
            Your badge ID is printed on your conference badge.<br>
            VIP and Speaker badges have priority access.
        </div>
    </div>
</body>
</html>
"""
    },

    "hospital": {
        "name": "Memorial Hospital Guest WiFi",
        "title": "Memorial Hospital - Guest WiFi",
        "fields": ["patient_room", "visitor_name"],
        "valid_rooms": ["ICU1", "ICU2", "101", "102", "201", "202", "301", "ER1", "ER2"],
        "html": """
<!DOCTYPE html>
<html>
<head>
    <title>Memorial Hospital - Guest WiFi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Open Sans', sans-serif; background: #e3f2fd; margin: 0; padding: 20px; min-height: 100vh; }
        .container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 20px rgba(0,0,0,0.1); }
        .header { background: #1565c0; color: white; padding: 20px; margin: -30px -30px 30px -30px; border-radius: 8px 8px 0 0; text-align: center; }
        h1 { margin: 0; font-size: 20px; }
        h2 { margin: 5px 0 0 0; font-weight: normal; font-size: 14px; opacity: 0.9; }
        label { display: block; margin-bottom: 5px; color: #333; font-weight: 600; }
        input { width: 100%; padding: 12px; margin-bottom: 20px; border: 2px solid #e0e0e0; border-radius: 6px; box-sizing: border-box; }
        button { width: 100%; padding: 14px; background: #1565c0; color: white; border: none; border-radius: 6px; font-size: 16px; cursor: pointer; }
        .notice { background: #fff3e0; padding: 15px; border-radius: 6px; margin-top: 20px; font-size: 13px; color: #e65100; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Memorial Hospital</h1>
            <h2>Guest WiFi Access</h2>
        </div>
        <form method="POST" action="/login">
            <label>Patient Room Number</label>
            <input type="text" name="patient_room" placeholder="e.g., 101, ICU1" required>

            <label>Visitor Name</label>
            <input type="text" name="visitor_name" placeholder="Your full name" required>

            <button type="submit">Connect</button>
        </form>
        <div class="notice">
            <strong>Notice:</strong> This network is for visitors only. Patient room number is required for security purposes. By connecting, you agree to our acceptable use policy.
        </div>
    </div>
</body>
</html>
"""
    }
}

# Success page
SUCCESS_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Connected!</title>
    <style>
        body { font-family: sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; background: #e8f5e9; }
        .container { text-align: center; padding: 40px; }
        h1 { color: #2e7d32; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Connected!</h1>
        <p>You now have internet access.</p>
        <p><a href="http://example.com">Continue browsing</a></p>
    </div>
</body>
</html>
"""

# Error page
ERROR_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Access Denied</title>
    <style>
        body { font-family: sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; background: #ffebee; }
        .container { text-align: center; padding: 40px; }
        h1 { color: #c62828; }
        p { color: #666; }
        a { color: #1976d2; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Access Denied</h1>
        <p>Invalid credentials. Please try again.</p>
        <p><a href="/">Back to login</a></p>
    </div>
</body>
</html>
"""


class CaptivePortalHandler(BaseHTTPRequestHandler):
    """HTTP handler for captive portal simulation"""

    portal_type = "hotel"  # Default

    def log_message(self, format, *args):
        """Custom logging"""
        print(f"{Colors.DIM}[{self.log_date_time_string()}]{Colors.RESET} {Colors.CYAN}{args[0]}{Colors.RESET}")

    def do_GET(self):
        """Handle GET requests"""
        path = urlparse(self.path).path

        # Captive portal detection endpoints
        if path in ['/generate_204', '/connecttest.txt', '/hotspot-detect.html', '/success.txt']:
            # Return redirect to portal
            self.send_response(302)
            self.send_header('Location', f'http://{self.headers["Host"]}/')
            self.end_headers()
            print(f"{Colors.GREEN}[DETECTED]{Colors.RESET} Captive portal check intercepted: {path}")
            return

        # Serve portal page
        if path == '/' or path == '/login':
            portal = PORTAL_TEMPLATES.get(self.portal_type, PORTAL_TEMPLATES["hotel"])
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(portal['html'].encode())
            print(f"{Colors.MAGENTA}[PORTAL]{Colors.RESET} Served {portal['name']} login page")
            return

        # 404 for everything else
        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        """Handle POST requests (login attempts)"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length).decode('utf-8')
        params = parse_qs(post_data)

        # Flatten params
        data = {k: v[0] for k, v in params.items()}

        print(f"\n{Colors.YELLOW}[LOGIN ATTEMPT]{Colors.RESET}")
        for key, value in data.items():
            print(f"  {Colors.CYAN}{key}:{Colors.RESET} {value}")

        # Validate credentials based on portal type
        portal = PORTAL_TEMPLATES.get(self.portal_type, PORTAL_TEMPLATES["hotel"])
        valid = self.validate_credentials(data, portal)

        if valid:
            print(f"{Colors.GREEN}[SUCCESS]{Colors.RESET} Valid credentials!")
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(SUCCESS_HTML.encode())
        else:
            print(f"{Colors.RED}[FAILED]{Colors.RESET} Invalid credentials")
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(ERROR_HTML.encode())

    def validate_credentials(self, data, portal):
        """Validate submitted credentials"""
        if self.portal_type == "hotel":
            room = data.get('room_number', '').strip()
            name = data.get('last_name', '').strip().lower()
            return room in portal.get('valid_rooms', []) and name in portal.get('valid_names', [])

        elif self.portal_type == "airport":
            # Accept any email format
            email = data.get('email', '').strip()
            return '@' in email and '.' in email

        elif self.portal_type == "cafe":
            code = data.get('access_code', '').strip().upper()
            valid = [c.upper() for c in portal.get('valid_codes', [])]
            return code in valid

        elif self.portal_type == "conference":
            badge = data.get('badge_id', '').strip().upper()
            valid = [b.upper() for b in portal.get('valid_badges', [])]
            return badge in valid

        elif self.portal_type == "hospital":
            room = data.get('patient_room', '').strip().upper()
            valid = [r.upper() for r in portal.get('valid_rooms', [])]
            return room in valid

        return False


def get_local_ip():
    """Get local IP address"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"


def show_banner():
    """Show startup banner"""
    os.system('cls' if os.name == 'nt' else 'clear')

    if HAKCER_AVAILABLE:
        # Use hakcer's show_banner with custom file for full effects
        hakcer.show_banner(
            custom_file=str(BANNER_FILE),
            effect_name="decrypt",
            theme="synthwave",
            speed_preference="fast",
            hold_time=1.0
        )
    else:
        # Fallback to plain banner
        if BANNER_FILE.exists():
            print(f"{Colors.GREEN}{BANNER_FILE.read_text()}{Colors.RESET}")
        else:
            print(f"\n{Colors.CYAN}╔═══════════════════════════════════════╗{Colors.RESET}")
            print(f"{Colors.CYAN}║     CAPTURED PORTAL - TEST SERVER     ║{Colors.RESET}")
            print(f"{Colors.CYAN}╚═══════════════════════════════════════╝{Colors.RESET}")

    print()


def main():
    parser = argparse.ArgumentParser(description='Test Captive Portal Server')
    parser.add_argument('-p', '--port', type=int, default=80, help='Port to run on (default: 80)')
    parser.add_argument('-t', '--type', choices=PORTAL_TEMPLATES.keys(), default='hotel',
                        help='Portal type to simulate')
    parser.add_argument('--list', action='store_true', help='List available portal types')
    args = parser.parse_args()

    if args.list:
        print(f"\n{Colors.CYAN}Available portal types:{Colors.RESET}\n")
        for key, portal in PORTAL_TEMPLATES.items():
            print(f"  {Colors.GREEN}{key:12}{Colors.RESET} - {portal['name']}")
            print(f"               Fields: {', '.join(portal['fields'])}")
            print()
        return

    show_banner()

    # Set portal type
    CaptivePortalHandler.portal_type = args.type
    portal = PORTAL_TEMPLATES[args.type]

    local_ip = get_local_ip()
    port = args.port

    print(f"{Colors.CYAN}Portal Type:{Colors.RESET} {portal['name']}")
    print(f"{Colors.CYAN}Fields:{Colors.RESET} {', '.join(portal['fields'])}")
    print()

    # Show valid credentials for testing
    print(f"{Colors.YELLOW}Valid test credentials:{Colors.RESET}")
    if 'valid_rooms' in portal:
        print(f"  Rooms: {', '.join(portal['valid_rooms'][:5])}...")
    if 'valid_names' in portal:
        print(f"  Names: {', '.join(portal['valid_names'])}")
    if 'valid_codes' in portal:
        print(f"  Codes: {', '.join(portal['valid_codes'])}")
    if 'valid_badges' in portal:
        print(f"  Badges: {', '.join(portal['valid_badges'][:5])}...")
    print()

    print(f"{Colors.MAGENTA}{'═' * 50}{Colors.RESET}")
    print(f"\n{Colors.GREEN}Starting server...{Colors.RESET}")
    print(f"\n{Colors.BOLD}To test:{Colors.RESET}")
    print(f"  1. Create a WiFi hotspot on this computer")
    print(f"  2. Connect your Captured Portal device to it")
    print(f"  3. The device should detect this captive portal")
    print()
    print(f"{Colors.CYAN}Server running at:{Colors.RESET}")
    print(f"  http://{local_ip}:{port}/")
    print(f"  http://localhost:{port}/")
    print()
    print(f"{Colors.DIM}Press Ctrl+C to stop{Colors.RESET}")
    print(f"{Colors.MAGENTA}{'═' * 50}{Colors.RESET}\n")

    try:
        server = HTTPServer(('0.0.0.0', port), CaptivePortalHandler)
        server.serve_forever()
    except PermissionError:
        print(f"{Colors.RED}[ERROR]{Colors.RESET} Port {port} requires root privileges.")
        print(f"Try: sudo python3 test_portal.py -p {port}")
        print(f"Or use a high port: python3 test_portal.py -p 8080")
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}[!] Server stopped{Colors.RESET}")


if __name__ == "__main__":
    main()
