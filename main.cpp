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

int steamInstall(const std::string &user, const std::string &pass, const std::string &installDir) {
    std::string cmd = "steamcmd"
        " +force_install_dir \"" + installDir + "\""
        " +login " + user + " " + pass +
        " +app_update 322170 validate"
        " +quit";
    return system(cmd.c_str());
}

std::string fetchGeodeTag() {
    std::string tmp = "/tmp/gd_geode_ver.json";
    std::string dlCmd = "curl -s 'https://api.geode-sdk.org/v1/loader/versions/latest?platform=win' -o " + tmp;
    system(dlCmd.c_str());

    std::string parseCmd;
    if (system("command -v jq > /dev/null 2>&1") == 0) {
        parseCmd = "jq -r .payload.tag " + tmp;
    } else if (system("command -v python3 > /dev/null 2>&1") == 0) {
        parseCmd = "python3 -c 'import json,sys;print(json.load(open(\"" + tmp + "\"))[\"payload\"][\"tag\"])'";
    } else {
        parseCmd = "python -c 'import json,sys;print(json.load(open(\"" + tmp + "\"))[\"payload\"][\"tag\"])'";
    }

    FILE *f = popen(parseCmd.c_str(), "r");
    if (!f) return "";
    char buf[128] = {};
    fgets(buf, sizeof(buf), f);
    pclose(f);

    std::string tag = buf;
    while (!tag.empty() && (tag.back() == '\n' || tag.back() == '\r' || tag.back() == ' '))
        tag.pop_back();
    return tag;
}

void installGeode(const std::string &gdPath) {
    std::cout << "Fetching latest Geode version...\n";
    std::string tag = fetchGeodeTag();
    if (tag.empty()) {
        std::cerr << "[ERROR] Could not fetch Geode version, skipping.\n";
        return;
    }
    std::cout << "Downloading Geode " << tag << "...\n";

    std::string zipPath = "/tmp/geode.zip";
    std::string extractPath = "/tmp/geode_extracted";
    std::string dlCmd = "curl -L -o " + zipPath +
        " \"https://github.com/geode-sdk/geode/releases/download/" + tag +
        "/geode-" + tag + "-win.zip\"";

    if (system(dlCmd.c_str()) != 0) {
        std::cerr << "[ERROR] Failed to download Geode.\n";
        return;
    }

    std::string mkdirCmd = "mkdir -p " + extractPath;
    system(mkdirCmd.c_str());

    std::string unzipCmd = "unzip -qq \"" + zipPath + "\" -d \"" + extractPath + "\"";
    if (system(unzipCmd.c_str()) != 0) {
        std::cerr << "[ERROR] Failed to unzip Geode.\n";
        return;
    }

    std::string mvCmd = "mv \"" + extractPath + "\"/* \"" + gdPath + "/\"";
    system(mvCmd.c_str());
    std::cout << "Geode installed.\n";
}

void writeAliases(const std::string &wineprefix, const std::string &exePath) {
    std::string aliasLine =
        "alias gdash='WINEPREFIX=" + wineprefix + " WINEDLLOVERRIDES=\"xinput1_4=n,b\" wine \"" + exePath + "\"'\n"
        "alias geometrydash='WINEPREFIX=" + wineprefix + " WINEDLLOVERRIDES=\"xinput1_4=n,b\" wine \"" + exePath + "\"'\n";

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

    appendIfMissing(getHome() + "/.bashrc", aliasLine);
    appendIfMissing(getHome() + "/.zshrc", aliasLine);

    std::string fishDir = getHome() + "/.config/fish/functions";
    mkdirp(fishDir);

    auto writeFishAlias = [&](const std::string &name) {
        std::ofstream f(fishDir + "/" + name + ".fish");
        f << "function " << name << "\n";
        f << "    set -x WINEPREFIX " << wineprefix << "\n";
        f << "    set -x WINEDLLOVERRIDES \"xinput1_4=n,b\"\n";
        f << "    wine \"" << exePath << "\"\n";
        f << "end\n";
    };
    writeFishAlias("gdash");
    writeFishAlias("geometrydash");
}

void makeDesktopEntry(const std::string &wineprefix, const std::string &exePath, const std::string &iconPath) {
    std::string appDir = getHome() + "/.local/share/applications";
    mkdirp(appDir);

    std::ofstream f(appDir + "/geometrydash.desktop");
    f << "[Desktop Entry]\n";
    f << "Name=Geometry Dash\n";
    f << "Exec=env WINEPREFIX=" << wineprefix << " WINEDLLOVERRIDES=\"xinput1_4=n,b\" wine \"" << exePath << "\"\n";
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
    if (!cmdExists("unzip"))       die("unzip not found.");
    if (!cmdExists("curl"))        die("curl not found.");

    if (!cmdExists("jq") && !cmdExists("python3") && !cmdExists("python"))
        die("jq or python is required for Geode version parsing.");

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
    steamInstall(user, pass, installDir);

    if (access(exePath.c_str(), F_OK) != 0) {
        std::cerr << "[ERROR] GeometryDash.exe not found after steamcmd ran, something actually went wrong.\n";
        return 1;
    }
    std::cout << "Game files OK.\n";

    std::cout << "\nSetting up wine prefix...\n";
    system(("WINEPREFIX=" + wineprefix + " wineboot --init 2>&1").c_str());

    std::cout << "Installing vcrun2015...\n";
    system(("WINEPREFIX=" + wineprefix + " winetricks -q vcrun2015 2>&1").c_str());

    std::cout << "Installing d3dcompiler_47...\n";
    system(("WINEPREFIX=" + wineprefix + " winetricks -q d3dcompiler_47 2>&1").c_str());

    installGeode(installDir);

    std::cout << "Downloading icon...\n";
    std::string icoPath = getHome() + "/.local/share/applications/geometrydash.ico";
    std::string pngPath = getHome() + "/.local/share/applications/geometrydash.png";
    system(("curl -sL \"https://cdn.cloudflare.steamstatic.com/steamcommunity/public/images/apps/322170/630be2daec290610d9ec3c7ba9bbacc786996953.ico\" -o \"" + icoPath + "\"").c_str());
    // what the fuck imagemagick why is this the syntax
    system(("magick \"" + icoPath + "\" -thumbnail 256x256 \"" + pngPath + "\"").c_str());

    writeAliases(wineprefix, exePath);
    makeDesktopEntry(wineprefix, exePath, pngPath);

    std::cout << "\nDone! Run 'gdash' or 'geometrydash' (restart shell first), or find it in your app menu.\n";
    return 0;
}
