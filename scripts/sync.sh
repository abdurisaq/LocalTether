#!/usr/bin/env bash

# =======================
# Configuration defaults
# =======================
DEPTH=0
CUSTOM_WINDOWS_NAME=""
CUSTOM_WINDOWS_USER=""

# =======================
# Parse arguments
# =======================
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --depth)
            DEPTH="$2"
            shift
            ;;
        --win-name)
            CUSTOM_WINDOWS_NAME="$2"
            shift
            ;;
        --win-user)
            CUSTOM_WINDOWS_USER="$2"
            shift
            ;;
        *)
            echo "Unknown parameter: $1"
            exit 1
            ;;
    esac
    shift
done

# =======================
# Path setup
# =======================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Move up DEPTH + 1 levels
PROJECT_DIR="$SCRIPT_DIR"
for ((i=0; i<=DEPTH; i++)); do
    PROJECT_DIR="$(dirname "$PROJECT_DIR")"
done

# Determine project name
if [[ -n "$CUSTOM_WINDOWS_NAME" ]]; then
    PROJECT_NAME="$CUSTOM_WINDOWS_NAME"
else
    PROJECT_NAME="$(basename "$PROJECT_DIR")"
fi

# =======================
# Determine Windows user
# =======================
if [[ -n "$CUSTOM_WINDOWS_USER" ]]; then
    WINDOWS_USER="$CUSTOM_WINDOWS_USER"
else
    # Try to guess user by finding the first directory inside /mnt/c/Users
    WINDOWS_USER=$(ls /mnt/c/Users | grep -v 'Public' | head -n 1)
    if [[ -z "$WINDOWS_USER" ]]; then
        echo "Failed to detect Windows user. Please use --win-user."
        exit 1
    fi
fi

# =======================
# Paths
# =======================
WATCHED_DIR="$PROJECT_DIR"
WATCHED_DIR_LENGTH=${#WATCHED_DIR}
WINDOWS_DIR="/mnt/c/Users/${WINDOWS_USER}/Documents/${PROJECT_NAME}"
LOG_FILE="/mnt/c/Users/${WINDOWS_USER}/Documents/sync_log.txt"
TEMP_PATH=""

# =======================
# Dependency logic
# =======================
detect_package_manager() {
    if command -v pacman >/dev/null 2>&1; then echo "pacman"
    elif command -v apt-get >/dev/null 2>&1; then echo "apt"
    elif command -v dnf >/dev/null 2>&1; then echo "dnf"
    else echo "unknown"; fi
}

install_package() {
    local PACKAGE=$1
    local MANAGER
    MANAGER=$(detect_package_manager)

    case $MANAGER in
        pacman) sudo pacman -Sy --noconfirm "$PACKAGE" ;;
        apt)    sudo apt update && sudo apt install -y "$PACKAGE" ;;
        dnf)    sudo dnf install -y "$PACKAGE" ;;
        *)      echo "Unknown package manager. Please install $PACKAGE manually."; exit 1 ;;
    esac
}

# =======================
# Check inotifywait
# =======================
if ! command -v inotifywait >/dev/null; then
    echo "Installing inotify-tools..."
    install_package inotify-tools
else
    echo "inotifywait already installed."
fi

# =======================
# File sync logic
# =======================
process_event() {
    local event_type="$1"
    local file_path1="$2"

    echo "-${event_type}-"
    case "$event_type" in
        *"ISDIR"*) mkdir -p "${WINDOWS_DIR}${file_path1}" ;;
        "CREATE")  touch "${WINDOWS_DIR}${file_path1}" ;;
        "DELETE")  rm -rf "${WINDOWS_DIR}${file_path1}" ;;
        "MODIFY")  cp -r "${WATCHED_DIR}${file_path1}" "${WINDOWS_DIR}${file_path1}" ;;
        "MOVED_FROM") TEMP_PATH="${file_path1}" ;;
        "MOVED_TO")   mv "${WINDOWS_DIR}${TEMP_PATH}" "${WINDOWS_DIR}${file_path1}" ;;
        *) ;;
    esac
}

# Initial sync
rsync -a "$WATCHED_DIR/" "$WINDOWS_DIR/"

# =======================
# Watcher loop
# =======================
inotifywait -m -r -e modify,create,delete,move,moved_to,moved_from "$WATCHED_DIR" |
    while read -r path action file; do
        echo "$(date): $action on $path$file" >> "$LOG_FILE"
        FULLPATH="${path}${file}"
        RELEVANTPATH=${FULLPATH:${WATCHED_DIR_LENGTH}}
        echo "$RELEVANTPATH"
        process_event "$action" "$RELEVANTPATH"
    done

