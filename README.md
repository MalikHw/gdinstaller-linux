(i need testers: a linux user (NO ATOMIC/NIX😭) who owns gd, please test and open an issue with results)
# gdinstaller-linux

Installs Geometry Dash on Linux without the Steam client, with Geode pre-installed and ready to go.

---

## What it does

Downloads GD via steamcmd using your Steam credentials, sets up Wine with the required dependencies, installs the latest version of Geode, and creates aliases + a desktop shortcut so you can launch the game normally.

---

## Dependencies (mostly handled by the script)

You need to own Geometry Dash on Steam. This tool downloads the game using your account — it does not pirate anything.

Install the rest via your distro's package manager:

**Arch / Manjaro**
```bash
sudo pacman -S wine winetricks imagemagick unzip curl
yay -S steamcmd  # or however you install AUR packages
```

**Ubuntu**
```bash
sudo add-apt-repository multiverse && sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install wine winetricks imagemagick unzip curl steamcmd
```

**Debian**
```
sudo apt update; sudo apt install software-properties-common; sudo apt-add-repository non-free; sudo dpkg --add-architecture i386; sudo apt update
sudo apt install wine winetricks imagemagick unzip curl steamcmd
```

**Fedora**
```bash
sudo dnf install wine winetricks ImageMagick unzip curl
```
AND you need steamcmd, look up how to install (updating this guide soon)

You also need one of `jq`, `python3`, or `python` for version parsing:
```bash
# pick one
sudo pacman -S jq
sudo apt install jq
sudo dnf install jq
```

---

## Usage

RUn this!
```
curl -o- 'https://github.com/MalikHw/gdinstaller-linux/raw/refs/heads/main/gdinstaller.sh' | bash
```
or clone the repo and run the script manually


It'll ask for your Steam username and password and some questions, then handle everything else automatically. Once it's done, restart your shell and run:

```bash
gdash
# or
geometrydash
```

Or find it in your application menu.

---

## About the Steam stub

After installing the game, the installer replaces `steam_api64.dll` with a stub from [MalikHw/stub-for-gdinstaller](https://github.com/MalikHw/stub-for-gdinstaller). This is required because the legit Steam dll hangs indefinitely when Steam client isn't running, which makes the game unlaunchable.

The stub is **not** a crack and does not bypass ownership — you still need to own the game and log in with your real Steam account to download it. The stub only replaces the runtime Steam connection that would otherwise block the game from starting.


btw pwease [donate](https://malikhw.github.io/donate) i made this in houws 🥺 :trollface:
