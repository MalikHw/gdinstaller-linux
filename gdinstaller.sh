#!/usr/bin/env bash

INSTALL_DIR="$HOME/Games/GeometryDash"
LOG_DIR="$HOME/.local/share/gdinstaller"
LOG_FILE="$LOG_DIR/install.log"
MARKER="$HOME/.local/share/gdash.exists"
STEAMCMD_DIR="$HOME/.local/share/steamcmd"
STEAMCMD="$STEAMCMD_DIR/steamcmd.sh"
MODE=""
LAUNCH_AT_FINISH=0

trap 'echo -e "\nYou stopped it." | tee -a "$LOG_FILE"; exit 1' INT

log() { echo "$1" | tee -a "$LOG_FILE"; }

die() {
    log ""
    log "[ERROR] $1"
    log "Check the dependencies section at: https://github.com/MalikHw/gdinstaller-linux"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --uninstall)        MODE="uninstall" ;;
        --reinstall)        MODE="reinstall" ;;
        --install-update)   MODE="update" ;;
        --launch-at-finish) LAUNCH_AT_FINISH=1 ;;
        --install-dir)      shift; INSTALL_DIR="$1" ;;
        --help|-h)
            echo "gdinstaller/gdash - Geometry Dash installer/launcher for Linux"
            echo ""
            echo "Usage:"
            echo "  gdinstaller/gdash                     Install GD (first run), or launch it (if already installed)"
            echo "  gdinstaller/gdash --reinstall         Wipe and reinstall everything from scratch"
            echo "  gdinstaller/gdash --uninstall         Remove GD, aliases, desktop entry and all installer files"
            echo "  gdinstaller/gdash --install-update    Check for a GD update and apply it if available"
            echo "  gdinstaller/gdash --install-dir PATH  Use a custom install directory instead of ~/Games/GeometryDash"
            echo "  gdinstaller/gdash --launch-at-finish  Launch GD immediately after install/update finishes"
            echo "  gdinstaller/gdash --help              Show this message"
            echo ""
            echo "Logs: ~/.local/share/gdinstaller/install.log"
            echo "More info: https://github.com/MalikHw/gdinstaller-linux"
            echo ""
            echo "Pls consider supporting me for making ts https://malikhw.github.io/donate"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

EXE="$INSTALL_DIR/GeometryDash.exe"
VERSION_FILE="$INSTALL_DIR/version.txt"
ICO_PATH="$HOME/.local/share/applications/geometrydash.ico"
PNG_PATH="$HOME/.local/share/applications/geometrydash.png"
DESKTOP_FILE="$HOME/.local/share/applications/geometrydash.desktop"
BIN_DIR="$HOME/.local/bin"

mkdir -p "$LOG_DIR"
echo "=== GD Installer - $(date) - mode: ${MODE:-auto} ===" >> "$LOG_FILE"

install_steamcmd() {
    if [ -f "$STEAMCMD" ]; then return; fi
    log "steamcmd not found, installing..."
    mkdir -p "$STEAMCMD_DIR"
    curl -sL "https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz" -o /tmp/steamcmd.tar.gz
    tar -xzf /tmp/steamcmd.tar.gz -C "$STEAMCMD_DIR"
    chmod +x "$STEAMCMD_DIR/steamcmd.sh" "$STEAMCMD_DIR/linux32/steamcmd"

    log "Installing steamcmd to /usr/local/bin (needs sudo)..."
    sudo ln -sf "$STEAMCMD" /usr/local/bin/steamcmd
    sudo ln -sf "$STEAMCMD_DIR/linux32/steamcmd" /usr/local/bin/steamcmd32
    sudo chmod +x /usr/local/bin/steamcmd
    log "steamcmd installed."
}

detect_and_install_deps() {
    local missing=()
    for dep in wine winetricks magick unzip curl; do
        command -v "$dep" > /dev/null 2>&1 || missing+=("$dep")
    done

    if ! command -v jq > /dev/null 2>&1 && ! command -v python3 > /dev/null 2>&1 && ! command -v python > /dev/null 2>&1; then
        missing+=("jq")
    fi

    if [ ${#missing[@]} -eq 0 ]; then return; fi

    log "Missing dependencies: ${missing[*]}"

    local pkg_install=""
    if command -v apt > /dev/null 2>&1; then
        local apt_names=()
        for dep in "${missing[@]}"; do
            case "$dep" in
                magick) apt_names+=("imagemagick") ;;
                *)       apt_names+=("$dep") ;;
            esac
        done
        pkg_install="sudo apt install -y ${apt_names[*]}"
    elif command -v pacman > /dev/null 2>&1; then
        local pac_names=()
        for dep in "${missing[@]}"; do
            case "$dep" in
                magick) pac_names+=("imagemagick") ;;
                *)       pac_names+=("$dep") ;;
            esac
        done
        pkg_install="sudo pacman -S --noconfirm ${pac_names[*]}"
    elif command -v dnf > /dev/null 2>&1; then
        local dnf_names=()
        for dep in "${missing[@]}"; do
            case "$dep" in
                magick) dnf_names+=("ImageMagick") ;;
                *)       dnf_names+=("$dep") ;;
            esac
        done
        pkg_install="sudo dnf install -y ${dnf_names[*]}"
    else
        log "[ERROR] Unsupported distro. Please install these manually: ${missing[*]}"
        log "Check the dependencies section at: https://github.com/MalikHw/gdinstaller-linux"
        exit 1
    fi

    log "Installing missing deps..."
    eval "$pkg_install" 2>&1 | tee -a "$LOG_FILE"
}

ask_credentials() {
    read -p "Steam username: " STEAM_USER
    read -s -p "Steam password: " STEAM_PASS
    echo ""
}

get_remote_version() {
    "$STEAMCMD" +login anonymous +app_info_update 1 +app_info_print 322170 +quit 2>/dev/null \
        | grep -A2 '"branches"' | grep -A1 '"public"' | grep '"buildid"' \
        | awk -F'"' '{print $4}'
}

get_geode_tag() {
    local json
    json="$(curl -s 'https://api.geode-sdk.org/v1/loader/versions/latest?platform=win')"
    if command -v jq > /dev/null 2>&1; then
        echo "$json" | jq -r .payload.tag
    elif command -v python3 > /dev/null 2>&1; then
        echo "$json" | python3 -c 'import json,sys;print(json.load(sys.stdin)["payload"]["tag"])'
    else
        echo "$json" | python -c 'import json,sys;print(json.load(sys.stdin)["payload"]["tag"])'
    fi
}

run_steamcmd() {
    "$STEAMCMD" +force_install_dir "$INSTALL_DIR" +login "$STEAM_USER" "$STEAM_PASS" +app_update 322170 validate +quit 2>&1 | tee -a "$LOG_FILE"
}

install_geode() {
    log "Fetching latest Geode version..."
    local tag
    tag="$(get_geode_tag)"
    if [ -z "$tag" ]; then
        log "[ERROR] Could not fetch Geode version, skipping."
        return
    fi
    log "Downloading Geode $tag..."
    curl -L -o /tmp/geode.zip "https://github.com/geode-sdk/geode/releases/download/$tag/geode-$tag-win.zip" 2>&1 | tee -a "$LOG_FILE"
    mkdir -p /tmp/geode_extracted
    unzip -qq /tmp/geode.zip -d /tmp/geode_extracted
    mv /tmp/geode_extracted/* "$INSTALL_DIR/"
    log "Geode installed."
}

install_stub() {
    log "Downloading steam_api64 stub..."
    curl -L -o "$INSTALL_DIR/steam_api64.dll" \
        "https://github.com/MalikHw/stub-for-gdinstaller/raw/main/steam_api64.dll" 2>&1 | tee -a "$LOG_FILE"
    echo "322170" > "$INSTALL_DIR/steam_appid.txt"
    log "Steam stub installed."
}

install_icon() {
    log "Downloading icon..."
    mkdir -p "$HOME/.local/share/applications"
    curl -sL "https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/322170/630be2daec290610d9ec3c7ba9bbacc786996953.ico" -o "$ICO_PATH"
    # what the fuck imagemagick why is ts the syntax
    magick "$ICO_PATH[0]" "$PNG_PATH"
}

ask_bin_dir() {
    read -p "Where to install gdash/geometrydash scripts? [default: $BIN_DIR]: " input < /dev/tty
    if [ -n "$input" ]; then BIN_DIR="$input"; fi
    mkdir -p "$BIN_DIR"
}

write_scripts() {
    local script_path
    script_path="$(realpath "$0")"
    mkdir -p "$BIN_DIR"

    for NAME in gdash geometrydash; do
        cp "$script_path" "$BIN_DIR/$NAME"
        chmod +x "$BIN_DIR/$NAME"
    done
    log "Installed gdash and geometrydash to $BIN_DIR"
}

write_desktop() {
    cat > "$DESKTOP_FILE" << DESKEOF
[Desktop Entry]
Name=Geometry Dash
Exec=env WINEDLLOVERRIDES="xinput1_4=n,b" wine "$EXE"
Icon=$PNG_PATH
Type=Application
Categories=Game;
StartupNotify=true
DESKEOF
}

remove_scripts() {
    rm -f "$BIN_DIR/gdash" "$BIN_DIR/geometrydash"
    rm -f "$HOME/.config/fish/functions/gdash.fish"
    rm -f "$HOME/.config/fish/functions/geometrydash.fish"
}

do_install() {
    log "=== GD Installer ==="
    log ""

    detect_and_install_deps
    install_steamcmd

    ask_credentials
    ask_bin_dir

    mkdir -p "$INSTALL_DIR"
    log ""
    log "Starting steamcmd..."
    run_steamcmd

    local size
    size=$(stat -c%s "$EXE" 2>/dev/null || echo 0)
    if [ ! -f "$EXE" ] || [ "$size" -lt 1024 ]; then
        die "You don't seem to own the game."
    fi
    log "Game files OK."

    log ""
    log "Installing vcrun2015..."
    winetricks -q vcrun2015 2>&1 | tee -a "$LOG_FILE"
    log "Installing d3dcompiler_47..."
    winetricks -q d3dcompiler_47 2>&1 | tee -a "$LOG_FILE"

    install_geode
    install_stub

    local ver
    ver="$(get_remote_version)"
    [ -n "$ver" ] && echo "$ver" > "$VERSION_FILE" && log "Saved version: $ver"

    install_icon
    write_scripts
    write_desktop

    touch "$MARKER"

    log ""
    log "Done! Run 'gdash' or 'geometrydash' (restart shell first), or find it in your app menu."
}

do_reinstall() {
    log "Reinstalling..."
    rm -f "$MARKER"
    rm -rf "$INSTALL_DIR"
    remove_scripts
    rm -f "$DESKTOP_FILE" "$ICO_PATH" "$PNG_PATH"
    do_install
}

do_uninstall() {
    log "Uninstalling Geometry Dash..."
    rm -rf "$INSTALL_DIR"
    rm -f "$DESKTOP_FILE" "$ICO_PATH" "$PNG_PATH" "$MARKER"
    remove_scripts
    rm -rf "$LOG_DIR"
    log "Uninstalled."
}

do_update() {
    detect_and_install_deps
    install_steamcmd

    if [ ! -f "$VERSION_FILE" ]; then
        log "[ERROR] version.txt not found. You need to reinstall."
        exit 1
    fi

    local current remote
    current="$(cat "$VERSION_FILE")"
    log "Checking for updates..."
    remote="$(get_remote_version)"

    if [ -z "$remote" ]; then die "Could not fetch remote version."; fi

    log "Installed: $current | Latest: $remote"

    if [ "$current" = "$remote" ]; then
        log "Already up to date."
        exit 0
    fi

    log "Update found! Updating..."
    ask_credentials
    run_steamcmd

    local size
    size=$(stat -c%s "$EXE" 2>/dev/null || echo 0)
    if [ ! -f "$EXE" ] || [ "$size" -lt 1024 ]; then
        die "GeometryDash.exe not found or too small after update."
    fi

    install_stub
    echo "$remote" > "$VERSION_FILE"
    log "Update complete."
}

launch_gd() {
    log "Launching Geometry Dash..."
    WINEDLLOVERRIDES="xinput1_4=n,b" wine "$EXE"
}

if [ -n "$MODE" ]; then
    case "$MODE" in
        uninstall) do_uninstall ;;
        reinstall) do_reinstall ;;
        update)    do_update ;;
    esac
elif [ -f "$MARKER" ]; then
    launch_gd
else
    do_install
fi

if [ "$LAUNCH_AT_FINISH" = "1" ] && [ -f "$EXE" ] && [ "$MODE" != "uninstall" ]; then
    launch_gd
fi
