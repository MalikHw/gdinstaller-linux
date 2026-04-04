#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <termios.h>
#include <functional>

static std::string homeDir;

void onCtrlC(int) {
    std::cout << "\nYou stopped it.\n";
    exit(1);
}

std::string getHome() {
    if (!homeDir.empty()) return homeDir;
    const char *h = getenv("HOME");
    if (h) { homeDir = h; return homeDir; }
    struct passwd *pw = getpwuid(getuid());
    homeDir = pw->pw_dir;
    return homeDir;
}

void mkdirp(const std::string &path) {
    std::string tmp = path;
    for (size_t i = 1; i < tmp.size(); i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            mkdir(tmp.c_str(), 0755);
            tmp[i] = '/';
        }
    }
    mkdir(tmp.c_str(), 0755);
}

bool cmdExists(const std::string &cmd) {
    std::string c = "command -v " + cmd + " > /dev/null 2>&1";
    return system(c.c_str()) == 0;
}

void die(const std::string &msg) {
    std::cerr << "\n[ERROR] " << msg << "\n";
    std::cerr << "Check the dependencies section at: https://github.com/MalikHw/gdinstaller-linux\n";
    exit(1);
}

std::string readPass() {
    struct termios old, neo;
    tcgetattr(STDIN_FILENO, &old);
    neo = old;
    neo.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &neo);
    std::string pass;
    std::getline(std::cin, pass);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    std::cout << "\n";
    return pass;
}

int steamLogin(const std::string &user, const std::string &pass, const std::string &installDir) {
    std::string cmd = "steamcmd +login " + user + " " + pass +
        " +force_install_dir \"" + installDir + "\"" +
        " +app_update 322170 validate +quit";
    return system(cmd.c_str());
}

void writeAliases(const std::string &wineprefix, const std::string &exePath) {
    std::string aliasLine = "alias gdash='WINEPREFIX=" + wineprefix + " wine \"" + exePath + "\"'\n";
    aliasLine += "alias geometrydash='WINEPREFIX=" + wineprefix + " wine \"" + exePath + "\"'\n";

    auto appendIfMissing = [&](const std::string &file, const std::string &content) {
        std::ifstream check(file);
        std::stringstream ss;
        ss << check.rdbuf();
        std::string existing = ss.str();
        if (existing.find("alias gdash=") == std::string::npos) {
            std::ofstream out(file, std::ios::app);
            out << "\n" << content;
        }
    };

    std::string bash = getHome() + "/.bashrc";
    std::string zsh  = getHome() + "/.zshrc";
    appendIfMissing(bash, aliasLine);
    appendIfMissing(zsh, aliasLine);

    std::string fishDir = getHome() + "/.config/fish/functions";
    mkdirp(fishDir);

    auto writeFishAlias = [&](const std::string &name) {
        std::string fishFile = fishDir + "/" + name + ".fish";
        std::ofstream f(fishFile);
        f << "function " << name << "\n";
        f << "    set -x WINEPREFIX " << wineprefix << "\n";
        f << "    wine \"" << exePath << "\"\n";
        f << "end\n";
    };
    writeFishAlias("gdash");
    writeFishAlias("geometrydash");
}

void makeDesktopEntry(const std::string &wineprefix, const std::string &exePath, const std::string &iconPath) {
    std::string appDir = getHome() + "/.local/share/applications";
    mkdirp(appDir);

    std::string desktopFile = appDir + "/geometrydash.desktop";
    std::ofstream f(desktopFile);
    f << "[Desktop Entry]\n";
    f << "Name=Geometry Dash\n";
    f << "Exec=env WINEPREFIX=" << wineprefix << " wine \"" << exePath << "\"\n";
    f << "Icon=" << iconPath << "\n";
    f << "Type=Application\n";
    f << "Categories=Game;\n";
    f << "StartupNotify=true\n";
}

int main() {
    signal(SIGINT, onCtrlC);

    std::cout << "=== GD Installer ===\n\n";

    if (!cmdExists("wine"))        die("wine not found.");
    if (!cmdExists("magick"))      die("imagemagick not found.");
    if (!cmdExists("steamcmd"))    die("steamcmd not found.");
    if (!cmdExists("winetricks"))  die("winetricks not found.");

    std::string installDir = getHome() + "/Games/GeometryDash";
    std::string wineprefix = getHome() + "/.wine-gd";
    std::string exePath    = installDir + "/GeometryDash.exe";

    mkdirp(installDir);

    std::cout << "Steam username: ";
    std::string user;
    std::getline(std::cin, user);

    std::cout << "Steam password: ";
    std::string pass = readPass();

    std::cout << "\nStarting steamcmd...\n";
    int r = steamLogin(user, pass, installDir);
    if (r != 0) {
        std::cerr << "[ERROR] steamcmd exited with code " << r << "\n";
        return 1;
    }

    std::cout << "\nSetting up wine prefix...\n";
    std::string wpCmd = "WINEPREFIX=" + wineprefix + " wineboot --init 2>&1";
    system(wpCmd.c_str());

    std::cout << "Installing vcrun2015...\n";
    std::string wt1 = "WINEPREFIX=" + wineprefix + " winetricks -q vcrun2015 2>&1";
    system(wt1.c_str());

    std::cout << "Installing d3dcompiler_47...\n";
    std::string wt2 = "WINEPREFIX=" + wineprefix + " winetricks -q d3dcompiler_47 2>&1";
    system(wt2.c_str());

    std::cout << "Downloading icon...\n";
    std::string icoPath = getHome() + "/.local/share/applications/geometrydash.ico";
    std::string pngPath = getHome() + "/.local/share/applications/geometrydash.png";
    std::string dlCmd = "curl -sL \"https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/322170/630be2daec290610d9ec3c7ba9bbacc786996953.ico\" -o \"" + icoPath + "\"";
    system(dlCmd.c_str());
    // what the fuck imagemagick why is this the syntax
    std::string convCmd = "magick \"" + icoPath + "\" -thumbnail 256x256 \"" + pngPath + "\"";
    system(convCmd.c_str());

    writeAliases(wineprefix, exePath);
    makeDesktopEntry(wineprefix, exePath, pngPath);

    std::cout << "\nDone! Run 'gdash' or 'geometrydash' (restart shell first), or find it in your app menu.\n";
    return 0;
}
