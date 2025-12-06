#!/bin/bash
#
# Captured Portal - Installer
# Creates Python 3.13 venv, installs dependencies, runs build tool
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color
BOLD='\033[1m'
DIM='\033[2m'

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="${SCRIPT_DIR}/.venv"
REQUIREMENTS="${SCRIPT_DIR}/tools/requirements.txt"
BUILD_SCRIPT="${SCRIPT_DIR}/tools/build.py"
BANNER_FILE="${SCRIPT_DIR}/banner"

# Print banner
print_banner() {
    clear
    if [ -f "$BANNER_FILE" ]; then
        echo -e "${GREEN}"
        cat "$BANNER_FILE"
        echo -e "${NC}"
    else
        echo -e "${CYAN}"
        echo "╔═══════════════════════════════════════╗"
        echo "║         CAPTURED PORTAL               ║"
        echo "║     Captive Portal Hunter/Analyzer    ║"
        echo "╚═══════════════════════════════════════╝"
        echo -e "${NC}"
    fi
    echo
}

# Print status messages
info() {
    echo -e "${GREEN}[+]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[!]${NC} $1"
}

error() {
    echo -e "${RED}[X]${NC} $1"
}

step() {
    echo -e "${CYAN}[*]${NC} $1"
}

# Check for Python 3.13
find_python() {
    # Try python3.13 first
    if command -v python3.13 &> /dev/null; then
        PYTHON_CMD="python3.13"
        return 0
    fi

    # Try python3 and check version
    if command -v python3 &> /dev/null; then
        PY_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
        if [ "$PY_VERSION" = "3.13" ]; then
            PYTHON_CMD="python3"
            return 0
        fi
    fi

    # Try pyenv
    if command -v pyenv &> /dev/null; then
        if pyenv versions | grep -q "3.13"; then
            warn "Python 3.13 found in pyenv but not active"
            echo -e "${DIM}Run: pyenv install 3.13 && pyenv local 3.13${NC}"
        fi
    fi

    return 1
}

# Check for Python (any 3.x as fallback)
find_python_fallback() {
    if command -v python3 &> /dev/null; then
        PY_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
        PY_MAJOR=$(python3 -c 'import sys; print(sys.version_info.major)')
        PY_MINOR=$(python3 -c 'import sys; print(sys.version_info.minor)')

        if [ "$PY_MAJOR" -eq 3 ] && [ "$PY_MINOR" -ge 9 ]; then
            PYTHON_CMD="python3"
            return 0
        fi
    fi
    return 1
}

# Create virtual environment
create_venv() {
    step "Creating virtual environment..."

    if [ -d "$VENV_DIR" ]; then
        warn "Virtual environment already exists at ${VENV_DIR}"
        read -p "Recreate it? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf "$VENV_DIR"
        else
            info "Using existing virtual environment"
            return 0
        fi
    fi

    $PYTHON_CMD -m venv "$VENV_DIR"
    info "Virtual environment created at ${VENV_DIR}"
}

# Activate venv
activate_venv() {
    if [ -f "${VENV_DIR}/bin/activate" ]; then
        source "${VENV_DIR}/bin/activate"
    elif [ -f "${VENV_DIR}/Scripts/activate" ]; then
        source "${VENV_DIR}/Scripts/activate"
    else
        error "Could not find venv activation script"
        exit 1
    fi
}

# Install requirements
install_requirements() {
    step "Installing dependencies..."

    activate_venv

    # Upgrade pip
    pip install --upgrade pip --quiet

    # Install from requirements.txt
    if [ -f "$REQUIREMENTS" ]; then
        pip install -r "$REQUIREMENTS"
        info "Dependencies installed"
    else
        error "requirements.txt not found at ${REQUIREMENTS}"
        exit 1
    fi
}

# Run build script
run_build() {
    step "Launching build tool..."
    echo

    activate_venv

    if [ -f "$BUILD_SCRIPT" ]; then
        python "$BUILD_SCRIPT" "$@"
    else
        error "build.py not found at ${BUILD_SCRIPT}"
        exit 1
    fi
}

# Main
main() {
    print_banner

    echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Captured Portal - Installation Script${NC}"
    echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
    echo

    # Find Python
    step "Checking for Python 3.13..."

    if find_python; then
        info "Found Python 3.13: $($PYTHON_CMD --version)"
    else
        warn "Python 3.13 not found, checking for fallback..."
        if find_python_fallback; then
            warn "Using Python $PY_VERSION (3.13 recommended)"
            PYTHON_CMD="python3"
        else
            error "Python 3.9+ required. Please install Python 3.13:"
            echo
            echo -e "${DIM}  macOS:   brew install python@3.13${NC}"
            echo -e "${DIM}  Ubuntu:  sudo apt install python3.13${NC}"
            echo -e "${DIM}  Windows: Download from python.org${NC}"
            echo
            exit 1
        fi
    fi

    # Create venv
    create_venv

    # Install requirements
    install_requirements

    echo
    info "Setup complete!"
    echo

    # Run build tool
    run_build "$@"
}

# Run main with all arguments
main "$@"
