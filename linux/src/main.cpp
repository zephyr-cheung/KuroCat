#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits.h>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <csignal>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <glib-unix.h>

#define KUROCAT_VERSION_STR "1.3.5"
static constexpr const char* APP_VERSION = KUROCAT_VERSION_STR;
// Other KuroCat builds scan installed binaries for this marker (see readBinaryVersion) to find the
// newest copy WITHOUT executing it. Always derived from KUROCAT_VERSION_STR.
__attribute__((used)) static const char kVersionMarker[] = "KUROCAT_VERSION=" KUROCAT_VERSION_STR;
static constexpr const char* UPDATE_PAGE_URL = "https://kurocat-download.app.workbuddy.host/?sharecode=JVAABIVZ7L";
#if defined(__aarch64__) || defined(_M_ARM64)
static constexpr const char* UPDATE_TAR_URL = "https://kurocat-download.app.workbuddy.host/downloads/KuroCat-Linux-arm64.tar.gz?sharecode=JVAABIVZ7L";
#else
static constexpr const char* UPDATE_TAR_URL = "https://kurocat-download.app.workbuddy.host/downloads/KuroCat-Linux-x86_64.tar.gz?sharecode=JVAABIVZ7L";
#endif

struct LinuxApp {
    GtkWidget* window = nullptr;
    GtkWidget* drawArea = nullptr;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkStatusIcon* trayIcon = nullptr;
    G_GNUC_END_IGNORE_DEPRECATIONS

    CatSettings settings;
    InputEngine input;
    CatPhysics physics;

    bool isVisible = true;
    bool isDragging = false;
    bool clickThroughTemporarilyUnlocked = false;
    double dragStartRootX = 0.0, dragStartRootY = 0.0;
    int dragStartWinX = 0, dragStartWinY = 0;

    bool hasLastHover = false;
    Pt lastHoverCanvas = { 0.0f, 0.0f };
    double lastTickTime = 0.0;

    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    std::string availableUpdateVersion;
    std::string availableUpdateUrl;
    bool isUpdating = false;

    // Cached window geometry (updated from configure-event). Querying it from the X server every
    // frame cost several synchronous round-trips per frame.
    int winX = 0, winY = 0, winW = 360, winH = 320;
    double lastGeometryRefresh = 0.0;
};

static LinuxApp* g_app = nullptr;
static int g_lockFd = -1;
static guint g_frameIntervalMs = 0;

// MARK: - Versions of installed copies / single instance

static int versionToNumber(const std::string& v) {
    int parts[3] = { 0, 0, 0 };
    int idx = 0;
    for (char c : v) {
        if (c >= '0' && c <= '9') parts[idx] = parts[idx] * 10 + (c - '0');
        else if (c == '.') { if (++idx >= 3) break; }
        else if (c != 'v' && c != 'V') break;
    }
    return parts[0] * 10000 + parts[1] * 100 + parts[2];
}

/// Reads the version baked into a kurocat binary without running it (0 = unknown / not kurocat).
/// New builds carry "KUROCAT_VERSION=x.y.z"; builds up to 1.3.2 only contain "KuroCat/x.y.z"
/// (their HTTP User-Agent).
static int readBinaryVersion(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 64 * 1024 * 1024) return 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto scan = [&](const std::string& needle) -> int {
        size_t pos = 0;
        while ((pos = data.find(needle, pos)) != std::string::npos) {
            size_t p = pos + needle.size();
            if (p < data.size() && std::isdigit((unsigned char)data[p])) {
                size_t e = p;
                while (e < data.size() && (std::isdigit((unsigned char)data[e]) || data[e] == '.')) e++;
                return versionToNumber(data.substr(p, e - p));
            }
            pos = p;
        }
        return 0;
    };
    int v = scan(std::string("KUROCAT_") + "VERSION=");
    if (v == 0) v = scan(std::string("KuroCat") + "/");
    return v;
}

static std::string homeDir() {
    const char* home = std::getenv("HOME");
    return std::string(home && home[0] ? home : "/tmp");
}

static std::string userLocalBinPath() {
    return homeDir() + "/.local/bin/kurocat";
}

static std::string resolvedPath(const std::string& p) {
    char buf[PATH_MAX] = {0};
    if (realpath(p.c_str(), buf)) return std::string(buf);
    return p;
}

/// Password-free updates install into ~/.local/bin while a .deb copy may still sit in /usr/bin
/// (and vice-versa after a later .deb upgrade). Whatever launcher was used — menu entry,
/// autostart, terminal — always run the NEWEST installed copy, so an old copy can't keep
/// popping up "new version available" after the user already updated.
static void redirectToNewestInstalledCopy(char** argv) {
    char selfBuf[PATH_MAX] = {0};
    ssize_t n = readlink("/proc/self/exe", selfBuf, sizeof(selfBuf) - 1);
    std::string self = n > 0 ? std::string(selfBuf, (size_t)n) : std::string();
    int best = versionToNumber(APP_VERSION);
    std::string bestPath;
    const std::string candidates[] = { userLocalBinPath(), "/usr/bin/kurocat", "/usr/local/bin/kurocat" };
    for (const auto& c : candidates) {
        if (access(c.c_str(), X_OK) != 0) continue;
        std::string real = resolvedPath(c);
        if (real == self) continue;
        int v = readBinaryVersion(real);
        if (v > best) { best = v; bestPath = real; }
    }
    if (bestPath.empty()) return;
    setenv("KUROCAT_NO_REDIRECT", "1", 1);
    execv(bestPath.c_str(), argv);
    // exec failed: keep running this copy
}

static std::string lockFilePath() {
    const char* rt = std::getenv("XDG_RUNTIME_DIR");
    std::string dir = (rt && rt[0] && access(rt, W_OK) == 0) ? std::string(rt) : std::string("/tmp");
    return dir + "/kurocat-" + std::to_string((long)getuid()) + ".lock";
}

static bool waitForPidExit(pid_t pid, int timeoutMs) {
    for (int waited = 0; waited <= timeoutMs; waited += 100) {
        if (kill(pid, 0) != 0 && errno == ESRCH) return true;
        usleep(100 * 1000);
    }
    return false;
}

/// Builds <= 1.3.2 had no single-instance lock at all, so a freshly installed version would run
/// NEXT to the old one (two cats, and the old one still nagging about updates). Find them via
/// /proc (same user, process name "kurocat") and stop them.
static void terminateLegacyInstances() {
    pid_t self = getpid();
    uid_t uid = getuid();
    std::vector<pid_t> victims;
    if (DIR* d = opendir("/proc")) {
        while (struct dirent* ent = readdir(d)) {
            if (!std::isdigit((unsigned char)ent->d_name[0])) continue;
            pid_t pid = (pid_t)std::atol(ent->d_name);
            if (pid <= 1 || pid == self) continue;
            std::string base = std::string("/proc/") + ent->d_name;
            struct stat st{};
            if (stat(base.c_str(), &st) != 0 || st.st_uid != uid) continue;
            std::ifstream comm(base + "/comm");
            std::string name;
            std::getline(comm, name);
            if (name == "kurocat") victims.push_back(pid);
        }
        closedir(d);
    }
    for (pid_t v : victims) kill(v, SIGTERM);
    for (pid_t v : victims) {
        if (!waitForPidExit(v, 3000)) kill(v, SIGKILL);
    }
}

static bool claimLock(int fd) {
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) return false;
    std::string content = std::to_string((long)getpid()) + " " + std::to_string(versionToNumber(APP_VERSION)) + "\n";
    if (ftruncate(fd, 0) == 0) {
        ssize_t w = pwrite(fd, content.data(), content.size(), 0);
        (void)w;
    }
    g_lockFd = fd;
    return true;
}

/// Returns false when this process should exit because an equal/newer KuroCat is already running
/// (that one is woken up instead: shown again, click-through cancelled, menu opened).
static bool acquireSingleInstance() {
    int fd = open(lockFilePath().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true; // can't lock (odd /tmp setup): never block startup over it
    if (claimLock(fd)) {
        terminateLegacyInstances();
        return true;
    }
    char buf[64] = {0};
    ssize_t r = pread(fd, buf, sizeof(buf) - 1, 0);
    (void)r;
    long otherPid = 0;
    int otherVer = 0;
    std::sscanf(buf, "%ld %d", &otherPid, &otherVer);
    if (otherPid <= 0 || otherVer >= versionToNumber(APP_VERSION)) {
        if (otherPid > 0) kill((pid_t)otherPid, SIGUSR1);
        close(fd);
        return false;
    }
    kill((pid_t)otherPid, SIGTERM); // older KuroCat: replace it
    for (int i = 0; i < 50; ++i) {
        usleep(100 * 1000);
        if (claimLock(fd)) {
            terminateLegacyInstances();
            return true;
        }
    }
    kill((pid_t)otherPid, SIGUSR1);
    close(fd);
    return false;
}

static void releaseSingleInstance() {
    if (g_lockFd >= 0) {
        close(g_lockFd);
        g_lockFd = -1;
    }
}

// MARK: - Launch at Login (Freedesktop XDG Autostart: Ubuntu/Debian/Deepin/统信 UOS/KDE/GNOME)

static std::string getAutostartDir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') return std::string(xdg) + "/autostart";
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/autostart";
}

static std::string getAutostartFilePath() {
    return getAutostartDir() + "/kurocat.desktop";
}

static std::string getSelfExePath() {
    char buf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "kurocat";
}

static bool isAutoStartEnabled() {
    return access(getAutostartFilePath().c_str(), F_OK) == 0;
}

static bool setAutoStartEnabled(bool enable) {
    std::string path = getAutostartFilePath();
    if (!enable) {
        std::remove(path.c_str());
        return true;
    }
    std::string dir = getAutostartDir();
    g_mkdir_with_parents(dir.c_str(), 0755);
    std::string exe = getSelfExePath();
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "[Desktop Entry]\n"
      << "Type=Application\n"
      << "Name=KuroCat\n"
      << "Name[zh_CN]=KuroCat 黑猫桌宠\n"
      << "Comment=Native desktop keyboard & mouse cat pet\n"
      << "Exec=\"" << exe << "\"\n"
      << "Icon=kurocat\n"
      << "Terminal=false\n"
      << "X-GNOME-Autostart-enabled=true\n";
    return f.good();
}

static void updateInputShape(LinuxApp& app) {
    if (!app.window || !gtk_widget_get_realized(app.window)) return;
    if (app.settings.fullClickThrough && !app.clickThroughTemporarilyUnlocked) {
        cairo_region_t* empty = cairo_region_create();
        gtk_widget_input_shape_combine_region(app.window, empty);
        cairo_region_destroy(empty);
        return;
    }

    // Smart per-area input mask: transparent corners pass clicks straight through,
    // while the cat head/body & mini keyboard remain interactive for petting, dragging & right-click!
    float s = app.settings.scale;
    cairo_region_t* reg = cairo_region_create();
    cairo_rectangle_int_t catBox = {
        (int)(128 * s), (int)(72 * s), (int)(116 * s), (int)(172 * s)
    };
    cairo_rectangle_int_t kbBox = {
        (int)(56 * s), (int)(230 * s), (int)(242 * s), (int)(72 * s)
    };
    cairo_region_union_rectangle(reg, &catBox);
    cairo_region_union_rectangle(reg, &kbBox);
    gtk_widget_input_shape_combine_region(app.window, reg);
    cairo_region_destroy(reg);
}

static void getDefaultOrigin(int w, int h, int& outX, int& outY) {
    GdkDisplay* dpy = gdk_display_get_default();
    GdkMonitor* mon = dpy ? gdk_display_get_primary_monitor(dpy) : nullptr;
    if (!mon && dpy) mon = gdk_display_get_monitor(dpy, 0);
    GdkRectangle geo = { 0, 0, 1440, 900 };
    if (mon) gdk_monitor_get_workarea(mon, &geo);
    outX = geo.x + geo.width - w - 24;
    outY = geo.y + geo.height - h - 16;
}

static void applyWindowScaleAndPos(LinuxApp& app, bool anchorBottomCenter) {
    int newW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int newH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale);

    int x = app.settings.posX;
    int y = app.settings.posY;
    if (anchorBottomCenter && app.window) {
        int oldX = 0, oldY = 0, oldW = 0, oldH = 0;
        gtk_window_get_position(GTK_WINDOW(app.window), &oldX, &oldY);
        gtk_window_get_size(GTK_WINDOW(app.window), &oldW, &oldH);
        x = oldX + oldW / 2 - newW / 2;
        y = oldY + oldH - newH;
    } else if (x == -99999 || y == -99999) {
        getDefaultOrigin(newW, newH, x, y);
    }

    app.settings.posX = x;
    app.settings.posY = y;
    gtk_window_resize(GTK_WINDOW(app.window), newW, newH);
    gtk_window_move(GTK_WINDOW(app.window), x, y);
    gtk_window_set_keep_above(GTK_WINDOW(app.window), app.settings.alwaysOnTop ? TRUE : FALSE);
    updateInputShape(app);
    app.settings.save();
}

static void openUrlInBrowser(const std::string& url) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
        _exit(127);
    }
}

struct LinuxUpdateResult {
    bool ok = false;
    bool manual = false;
    bool hasUpdate = false;
    std::string latestTag;
    std::string releaseUrl;
};

static std::string extractJsonField(const std::string& json, const char* fieldName) {
    std::string key = std::string("\"") + fieldName + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static bool isSemVerNewer(const std::string& lhs, const std::string& rhs) {
    auto parse = [](const std::string& v) {
        std::vector<int> parts;
        size_t i = 0;
        while (i < v.size() && (v[i] == 'v' || v[i] == 'V' || v[i] == ' ')) i++;
        int cur = 0; bool inNum = false;
        for (; i < v.size(); ++i) {
            if (v[i] >= '0' && v[i] <= '9') { cur = cur * 10 + (v[i] - '0'); inNum = true; }
            else if (v[i] == '.') { parts.push_back(inNum ? cur : 0); cur = 0; inNum = false; }
            else break;
        }
        if (inNum) parts.push_back(cur);
        return parts;
    };
    auto a = parse(lhs), b = parse(rhs);
    size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        int va = i < a.size() ? a[i] : 0;
        int vb = i < b.size() ? b[i] : 0;
        if (va != vb) return va > vb;
    }
    return false;
}

static std::string extractVersionFromHtml(const std::string& html) {
    auto parseVerAt = [&](size_t pos) -> std::string {
        if (pos >= html.size() || (html[pos] != 'v' && html[pos] != 'V')) return "";
        size_t i = pos + 1;
        int dots = 0;
        std::string ver = "v";
        while (i < html.size()) {
            char c = html[i];
            if (c >= '0' && c <= '9') { ver.push_back(c); }
            else if (c == '.') { dots++; ver.push_back(c); }
            else break;
            i++;
        }
        return (dots >= 2 && ver.size() > 5) ? ver : "";
    };

    size_t mPos = html.find("已发布");
    if (mPos != std::string::npos) {
        size_t searchStart = (mPos > 32) ? (mPos - 32) : 0;
        for (size_t i = mPos; i > searchStart; --i) {
            if (html[i - 1] == 'v' || html[i - 1] == 'V') {
                std::string v = parseVerAt(i - 1);
                if (!v.empty()) return v;
            }
        }
    }
    size_t vPos = html.find("版本 ");
    if (vPos != std::string::npos) {
        std::string v = parseVerAt(vPos + 7);
        if (!v.empty()) return v;
    }
    return "";
}

static std::string fetchUrlBody(const char* url) {
    std::string cmd = std::string("curl -fsSL --max-time 8 -A KuroCat/") + APP_VERSION + " '" + url +
        "' 2>/dev/null || wget -qO- --timeout=8 '" + url + "' 2>/dev/null";
    std::string body;
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        char buf[1024];
        while (size_t n = std::fread(buf, 1, sizeof(buf), fp)) body.append(buf, n);
        pclose(fp);
    }
    return body;
}

static void startInAppUpdate();

static void startAsyncUpdateCheck(bool manual, int delayMs = 0) {
    std::thread([manual, delayMs]() {
        if (delayMs > 0) usleep((useconds_t)delayMs * 1000);
        auto* res = new LinuxUpdateResult();
        res->manual = manual;

        std::string body = fetchUrlBody("https://kurocat-download.app.workbuddy.host/version.json?sharecode=JVAABIVZ7L");
        std::string tag = extractJsonField(body, "tag_name");
        std::string url = extractJsonField(body, "html_url");

        if (tag.empty()) {
            std::string html = fetchUrlBody(UPDATE_PAGE_URL);
            tag = extractVersionFromHtml(html);
        }

        if (!tag.empty()) {
            res->ok = true;
            res->latestTag = tag;
            res->releaseUrl = url.empty() ? UPDATE_PAGE_URL : url;
            res->hasUpdate = isSemVerNewer(tag, APP_VERSION);
        }

        g_idle_add(+[](gpointer data) -> gboolean {
            auto* r = static_cast<LinuxUpdateResult*>(data);
            if (g_app && r->ok && r->hasUpdate) {
                g_app->availableUpdateVersion = r->latestTag;
                g_app->availableUpdateUrl = r->releaseUrl;
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
                    "发现 KuroCat 新版本 %s", r->latestTag.c_str());
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "当前版本：v%s\n最新版本：%s\n\n是否立即在软件内自动下载并更新重启？",
                    APP_VERSION, r->latestTag.c_str());
                int resp = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (resp == GTK_RESPONSE_YES) {
                    startInAppUpdate();
                }
            } else if (r->manual) {
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL,
                    r->ok ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                    "%s", r->ok ? "已是最新版本" : "检查更新失败");
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "%s", r->ok ? "当前版本已是最新版本。" : "无法连接到更新服务器，请稍后重试或检查网络连接。");
                gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
            }
            delete r;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

static std::string shQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

/// Writes a per-user launcher (~/.local/share/applications overrides /usr/share/applications for
/// the same file name) so the app menu starts the per-user copy after a password-free update.
static void writeUserDesktopEntry(const std::string& exePath) {
    std::string dir = homeDir() + "/.local/share/applications";
    g_mkdir_with_parents(dir.c_str(), 0755);
    std::ofstream f(dir + "/kurocat.desktop");
    if (!f.is_open()) return;
    f << "[Desktop Entry]\n"
      << "Name=KuroCat\n"
      << "Name[zh_CN]=KuroCat 黑猫键盘桌宠\n"
      << "Comment=Full-state keyboard & mouse desktop cat pet\n"
      << "Comment[zh_CN]=打字/游戏全状态监听键盘小黑猫桌宠\n"
      << "Exec=\"" << exePath << "\"\n"
      << "Icon=kurocat\n"
      << "Terminal=false\n"
      << "Type=Application\n"
      << "Categories=Utility;Amusement;\n"
      << "StartupNotify=false\n";
}

static void startInAppUpdate() {
    if (!g_app || g_app->isUpdating) return;
    g_app->isUpdating = true;

    std::thread([]() {
        bool ok = false;
        char exeBuf[PATH_MAX] = {};
        ssize_t exeLen = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
        std::string exePath = (exeLen > 0) ? std::string(exeBuf, exeLen) : "";
        const std::string deletedSuffix = " (deleted)"; // binary replaced on disk while running (e.g. .deb upgrade)
        if (exePath.size() > deletedSuffix.size() &&
            exePath.compare(exePath.size() - deletedSuffix.size(), deletedSuffix.size(), deletedSuffix) == 0) {
            exePath.erase(exePath.size() - deletedSuffix.size());
        }
        std::string targetPath = exePath;

        std::string workDir = "/tmp/kurocat_upd_" + std::to_string((long)getpid());
        std::string tarPath = workDir + "/update.tar.gz";
        std::string newBin = workDir + "/kurocat";

        if (!exePath.empty()) {
            std::string dlCmd = "mkdir -p " + shQuote(workDir) + " && (curl -fsSL --max-time 60 -o " + shQuote(tarPath) +
                " " + shQuote(UPDATE_TAR_URL) + " || wget -qO " + shQuote(tarPath) + " --timeout=60 " + shQuote(UPDATE_TAR_URL) +
                ") && tar -xzf " + shQuote(tarPath) + " -C " + shQuote(workDir);
            if (std::system(dlCmd.c_str()) == 0) {
                struct stat st{};
                if (stat(newBin.c_str(), &st) == 0 && st.st_size > 10000) {
                    size_t slash = exePath.rfind('/');
                    std::string exeDir = (slash != std::string::npos) ? exePath.substr(0, slash) : ".";
                    bool perUser = (access(exeDir.c_str(), W_OK) != 0);
                    if (perUser) {
                        // System-wide install (.deb in /usr/bin): previously this asked for the login
                        // password via pkexec. Install for this user instead — no password needed —
                        // and redirectToNewestInstalledCopy() makes every launcher pick it up.
                        targetPath = userLocalBinPath();
                    }
                    std::string targetDir = targetPath.substr(0, targetPath.rfind('/'));
                    std::string tmpTarget = targetPath + ".new";
                    std::string installCmd = "mkdir -p " + shQuote(targetDir) +
                        " && cp -f " + shQuote(newBin) + " " + shQuote(tmpTarget) +
                        " && chmod 755 " + shQuote(tmpTarget) +
                        " && mv -f " + shQuote(tmpTarget) + " " + shQuote(targetPath);
                    if (std::system(installCmd.c_str()) == 0) {
                        ok = true;
                        if (perUser) {
                            writeUserDesktopEntry(targetPath);
                            std::string iconDir = homeDir() + "/.local/share/icons/hicolor/scalable/apps";
                            std::string iconCmd = "[ -f " + shQuote(workDir + "/kurocat.svg") + " ] && mkdir -p " + shQuote(iconDir) +
                                " && cp -f " + shQuote(workDir + "/kurocat.svg") + " " + shQuote(iconDir + "/kurocat.svg");
                            (void)std::system(iconCmd.c_str());
                        }
                    }
                }
            }
        }
        std::string cleanupCmd = "rm -rf " + shQuote(workDir);
        (void)std::system(cleanupCmd.c_str());

        auto* res = new std::pair<bool, std::string>(ok, targetPath);
        g_idle_add(+[](gpointer data) -> gboolean {
            auto* p = static_cast<std::pair<bool, std::string>*>(data);
            if (g_app) g_app->isUpdating = false;
            if (p->first && !p->second.empty()) {
                // Hand the single-instance lock to the new binary before it starts.
                releaseSingleInstance();
                pid_t pid = fork();
                if (pid == 0) {
                    execl(p->second.c_str(), p->second.c_str(), nullptr);
                    _exit(127);
                }
                gtk_main_quit();
            } else {
                GtkWidget* dlg = gtk_message_dialog_new(
                    nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                    "自动更新失败");
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "自动下载或替换安装包失败，是否打开下载页手动下载？");
                int resp = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (resp == GTK_RESPONSE_YES) {
                    std::string url = (g_app && !g_app->availableUpdateUrl.empty())
                        ? g_app->availableUpdateUrl : UPDATE_PAGE_URL;
                    openUrlInBrowser(url);
                }
            }
            delete p;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

static GdkPixbuf* createCatTrayPixbuf() {
    constexpr int sz = 32;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
    cairo_t* cr = cairo_create(surf);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Matte black cat head silhouette
    cairo_set_source_rgba(cr, 0.085, 0.085, 0.10, 1.0);
    cairo_move_to(cr, 4, 15); cairo_line_to(cr, 6, 3); cairo_line_to(cr, 14, 11); cairo_close_path(cr); cairo_fill(cr);
    cairo_move_to(cr, 28, 15); cairo_line_to(cr, 26, 3); cairo_line_to(cr, 18, 11); cairo_close_path(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 16, 18.5); cairo_scale(cr, 12.0, 10.5); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    // Chartreuse glowing eyes
    cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 1.0);
    cairo_save(cr); cairo_translate(cr, 11.5, 17.2); cairo_scale(cr, 3.0, 2.6); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 20.5, 17.2); cairo_scale(cr, 3.0, 2.6); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.08, 0.08, 0.10, 1.0);
    cairo_save(cr); cairo_translate(cr, 11.5, 17.2); cairo_scale(cr, 0.9, 1.9); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);
    cairo_save(cr); cairo_translate(cr, 20.5, 17.2); cairo_scale(cr, 0.9, 1.9); cairo_arc(cr, 0, 0, 1.0, 0, 6.2832); cairo_restore(cr); cairo_fill(cr);

    GdkPixbuf* pb = gdk_pixbuf_get_from_surface(surf, 0, 0, sz, sz);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return pb;
}

static void showPopupMenu(LinuxApp& app, GdkEventButton* evt) {
    GtkWidget* menu = gtk_menu_new();

    auto addDisabled = [&](const std::string& label) {
        GtkWidget* it = gtk_menu_item_new_with_label(label.c_str());
        gtk_widget_set_sensitive(it, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    };
    auto addSep = [&]() {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    };
    auto addCheck = [&](const char* label, bool checked, GCallback cb, gpointer data = nullptr) {
        GtkWidget* it = gtk_check_menu_item_new_with_label(label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), checked ? TRUE : FALSE);
        g_signal_connect(it, "toggled", cb, data);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    };
    auto addAction = [&](const char* label, GCallback cb, gpointer data = nullptr) {
        GtkWidget* it = gtk_menu_item_new_with_label(label);
        g_signal_connect(it, "activate", cb, data);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    };

    addDisabled(app.input.statusText());
    addDisabled("今日击键 " + std::to_string(app.input.totalKeystrokesToday)
        + "  ·  KPM " + std::to_string(app.input.currentKPM())
        + "  ·  摸猫 " + std::to_string(app.physics.totalPetSessions) + " 次");
    addSep();

    addAction(app.isVisible ? "隐藏小猫" : "显示小猫", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        g_app->isVisible = !g_app->isVisible;
        if (g_app->isVisible) gtk_widget_show(g_app->window);
        else gtk_widget_hide(g_app->window);
    }));

    addCheck("完全鼠标穿透 (按住 Alt 可临时右键/拖动)", app.settings.fullClickThrough, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.fullClickThrough = gtk_check_menu_item_get_active(item);
        updateInputShape(*g_app);
        g_app->settings.save();
    }));

    addCheck("窗口置顶显示", app.settings.alwaysOnTop, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.alwaysOnTop = gtk_check_menu_item_get_active(item);
        applyWindowScaleAndPos(*g_app, false);
    }));

    addCheck("开机自动启动", isAutoStartEnabled(), G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        setAutoStartEnabled(gtk_check_menu_item_get_active(item));
    }));

    addCheck("高帧率 60fps（电脑卡顿时请关闭）", app.settings.highFrameRate, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.highFrameRate = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));

    addSep();

    addCheck("显示按键气泡 ([S] [W] [⇧])", app.settings.showKeyBadges, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.showKeyBadges = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));
    addCheck("显示鼠标点击气泡 ([左键] [右键])", app.settings.showMouseBadges, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.showMouseBadges = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));
    addCheck("显示键盘字符", app.settings.showKeyLegends, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.showKeyLegends = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));
    addCheck("3键同时按「用脚按」彩蛋 🐾", app.settings.footEasterEgg, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.footEasterEgg = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));
    addCheck("摸头/摸背闭眼互动", app.settings.petInteraction, G_CALLBACK(+[](GtkCheckMenuItem* item, gpointer) {
        g_app->settings.petInteraction = gtk_check_menu_item_get_active(item);
        g_app->settings.save();
    }));

    addSep();

    // Coat Skin submenu
    {
        GtkWidget* sub = gtk_menu_new();
        auto addCoat = [&](const char* lbl, CatCoatTheme c) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(lbl);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), app.settings.coatTheme == c);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                auto coat = (CatCoatTheme)(intptr_t)p;
                g_app->settings.coatTheme = coat;
                g_app->settings.eyeTheme = coat == CatCoatTheme::Siamese ? EyeColorTheme::SiameseBlue :
                    ((coat == CatCoatTheme::ClassicBlack || coat == CatCoatTheme::Tuxedo) ? EyeColorTheme::NeonChartreuse : EyeColorTheme::SoftAmber);
                g_app->settings.save();
            }), (gpointer)(intptr_t)c);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        };
        addCoat("经典小黑猫 (原版)", CatCoatTheme::ClassicBlack);
        addCoat("挖煤暹罗猫", CatCoatTheme::Siamese);
        addCoat("美短（银虎斑加白）", CatCoatTheme::Shorthair);
        addCoat("橘白猫", CatCoatTheme::OrangeWhite);
        addCoat("奶牛猫（燕尾服）", CatCoatTheme::Tuxedo);
        addCoat("三花猫", CatCoatTheme::Calico);
        addCoat("橘猫虎斑", CatCoatTheme::GingerTabby);
        addCoat("英短蓝猫", CatCoatTheme::BritishBlue);
        GtkWidget* parent = gtk_menu_item_new_with_label("猫咪皮肤 🐱");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), parent);
    }

    // Keyboard Theme submenu
    {
        GtkWidget* sub = gtk_menu_new();
        auto addKb = [&](const char* lbl, KeyboardTheme t) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(lbl);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), app.settings.keyboardTheme == t);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                g_app->settings.keyboardTheme = (KeyboardTheme)(intptr_t)p;
                g_app->settings.save();
            }), (gpointer)(intptr_t)t);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        };
        addKb("经典奶白暖黄 (原版)", KeyboardTheme::ClassicMilk);
        addKb("暗夜极客黑", KeyboardTheme::MidnightHacker);
        addKb("复古奶油灰", KeyboardTheme::RetroCream);
        GtkWidget* parent = gtk_menu_item_new_with_label("键盘配色");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), parent);
    }

    // Eye Color submenu
    {
        GtkWidget* sub = gtk_menu_new();
        auto addEye = [&](const char* lbl, EyeColorTheme e) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(lbl);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), app.settings.eyeTheme == e);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                g_app->settings.eyeTheme = (EyeColorTheme)(intptr_t)p;
                g_app->settings.save();
            }), (gpointer)(intptr_t)e);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        };
        addEye("经典荧光黄绿 (原版)", EyeColorTheme::NeonChartreuse);
        addEye("暹罗宝石蓝", EyeColorTheme::SiameseBlue);
        addEye("琥珀金瞳", EyeColorTheme::WarmAmber);
        addEye("冰晶浅蓝", EyeColorTheme::IceCyan);
        addEye("柔和琥珀", EyeColorTheme::SoftAmber);
        addEye("淡金青绿", EyeColorTheme::ReferenceHazel);
        GtkWidget* parent = gtk_menu_item_new_with_label("瞳色切换");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), parent);
    }

    // Scale submenu
    {
        GtkWidget* sub = gtk_menu_new();
        const std::pair<const char*, int> scales[] = {
            {"迷你 (0.65x)", 65}, {"偏小 (0.85x)", 85}, {"标准 (1.0x)", 100},
            {"偏大 (1.25x)", 125}, {"超大 (1.5x)", 150}
        };
        for (auto [lbl, pct] : scales) {
            GtkWidget* it = gtk_check_menu_item_new_with_label(lbl);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), std::fabs(app.settings.scale * 100.0f - pct) < 5.0f);
            g_signal_connect(it, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer p) {
                g_app->settings.scale = (float)(intptr_t)p / 100.0f;
                applyWindowScaleAndPos(*g_app, true);
            }), (gpointer)(intptr_t)pct);
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
        }
        GtkWidget* parent = gtk_menu_item_new_with_label("尺寸大小");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), parent);
    }

    addSep();
    addAction("演示打字动画", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        g_app->demoActive = true;
        g_app->demoStartTime = nowSeconds();
        g_app->demoStepIdx = 0;
    }));
    addAction("重置位置到右下角", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        g_app->settings.posX = -99999;
        g_app->settings.posY = -99999;
        applyWindowScaleAndPos(*g_app, false);
    }));
    if (!app.availableUpdateVersion.empty()) {
        std::string lbl = app.isUpdating
            ? ("⏳ 正在下载更新 " + app.availableUpdateVersion + "...")
            : ("🆕 发现新版本 " + app.availableUpdateVersion + " (点击自动更新)");
        if (app.isUpdating) {
            GtkWidget* item = gtk_menu_item_new_with_label(lbl.c_str());
            gtk_widget_set_sensitive(item, FALSE);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        } else {
            addAction(lbl.c_str(), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
                startInAppUpdate();
            }));
        }
    } else {
        std::string lbl = std::string("检查更新 (当前 v") + APP_VERSION + ")";
        addAction(lbl.c_str(), G_CALLBACK(+[](GtkMenuItem*, gpointer) {
            startAsyncUpdateCheck(true, 0);
        }));
    }
    addSep();
    addAction("退出 KuroCat", G_CALLBACK(+[](GtkMenuItem*, gpointer) {
        releaseSingleInstance();
        gtk_main_quit();
    }));

    gtk_widget_show_all(menu);
    if (evt) {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)evt);
    } else {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    }
}

static gboolean onDraw(GtkWidget* widget, cairo_t* cr, gpointer) {
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_restore(cr);

    cairo_save(cr);
    cairo_scale(cr, (double)w / CatPhysics::CanvasWidth, (double)h / CatPhysics::CanvasHeight);
    CatRenderer::draw(cr, g_app->physics, g_app->settings);
    cairo_restore(cr);
    return FALSE;
}

static gboolean onButtonPress(GtkWidget*, GdkEventButton* evt, gpointer) {
    if (evt->button == 3) {
        showPopupMenu(*g_app, evt);
        return TRUE;
    }
    if (evt->button == 1 && (!g_app->settings.fullClickThrough || g_app->clickThroughTemporarilyUnlocked)) {
        g_app->isDragging = true;
        g_app->dragStartRootX = evt->x_root;
        g_app->dragStartRootY = evt->y_root;
        gtk_window_get_position(GTK_WINDOW(g_app->window), &g_app->dragStartWinX, &g_app->dragStartWinY);
        return TRUE;
    }
    return FALSE;
}

static gboolean onMotionNotify(GtkWidget*, GdkEventMotion* evt, gpointer) {
    if (g_app->isDragging) {
        int nx = g_app->dragStartWinX + (int)std::round(evt->x_root - g_app->dragStartRootX);
        int ny = g_app->dragStartWinY + (int)std::round(evt->y_root - g_app->dragStartRootY);
        gtk_window_move(GTK_WINDOW(g_app->window), nx, ny);
        return TRUE;
    }
    return FALSE;
}

static gboolean onButtonRelease(GtkWidget*, GdkEventButton* evt, gpointer) {
    if (evt->button == 1 && g_app->isDragging) {
        g_app->isDragging = false;
        gtk_window_get_position(GTK_WINDOW(g_app->window), &g_app->settings.posX, &g_app->settings.posY);
        g_app->settings.save();
        return TRUE;
    }
    return FALSE;
}

static void refreshWindowGeometry(LinuxApp& app) {
    if (!app.window) return;
    gtk_window_get_position(GTK_WINDOW(app.window), &app.winX, &app.winY);
    gtk_window_get_size(GTK_WINDOW(app.window), &app.winW, &app.winH);
    app.lastGeometryRefresh = nowSeconds();
}

/// Adaptive frame pacing. The old fixed 60fps redraw of a translucent always-on-top window made
/// software-composited desktops (统信 UOS / 麒麟 on weak or 国产 GPUs, VMs) so slow that only
/// typing still worked. Now: 30fps while the user is typing/moving/petting, 20fps when idle,
/// 10fps when the cat is asleep, 4fps when hidden. "高帧率 60fps" in the menu restores 60fps.
static guint desiredFrameIntervalMs(const LinuxApp& app) {
    if (!app.isVisible) return 250;
    if (app.settings.highFrameRate) return 16;
    double now = nowSeconds();
    bool busy = app.isDragging || app.demoActive || !app.input.activeKeys.empty()
        || app.input.leftMouseDown || app.input.rightMouseDown
        || (now - app.input.lastAnyInputTime()) < 2.5
        || !app.physics.floatingKeycaps.empty()
        || app.physics.petHeadIntensity > 0.02f || app.physics.petBodyIntensity > 0.02f;
    if (busy) return 33;
    if (app.physics.sleepiness > 0.6f) return 100;
    return 50;
}

static gboolean onFrameTick(gpointer);

static void scheduleFrames(guint intervalMs) {
    g_frameIntervalMs = intervalMs;
    g_timeout_add(intervalMs, onFrameTick, nullptr);
}

static gboolean onFrameTick(gpointer) {
    if (!g_app) return G_SOURCE_REMOVE;
    if (!g_app->isVisible) {
        guint want = desiredFrameIntervalMs(*g_app);
        if (want != g_frameIntervalMs) { scheduleFrames(want); return G_SOURCE_REMOVE; }
        return G_SOURCE_CONTINUE;
    }

    double now = nowSeconds();
    float dt = (float)(now - g_app->lastTickTime);
    g_app->lastTickTime = now;
    if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 30.0f;

    // Fallback GDK cursor query + Alt check — only needed without XInput2 (Wayland / no XI2).
    // With XI2 the input thread already tracks the cursor, so skip these two X round-trips per frame.
    float fbX = 0.0f, fbY = 0.0f;
    bool hasFb = false;
    bool gdkAltHeld = false;
    if (!g_app->input.isXI2Active) {
        if (GdkDisplay* dpy = gdk_display_get_default()) {
            if (GdkSeat* seat = gdk_display_get_default_seat(dpy)) {
                if (GdkDevice* ptr = gdk_seat_get_pointer(seat)) {
                    gint gx = 0, gy = 0;
                    gdk_device_get_position(ptr, nullptr, &gx, &gy);
                    fbX = (float)gx; fbY = (float)gy; hasFb = true;
                    if (GdkWindow* gw = gtk_widget_get_window(g_app->window)) {
                        GdkModifierType mask = (GdkModifierType)0;
                        gdk_window_get_device_position(gw, ptr, nullptr, nullptr, &mask);
                        if (mask & GDK_MOD1_MASK) gdkAltHeld = true;
                    }
                }
            }
        }
    }

    g_app->input.pollAndDrain(dt, fbX, fbY, hasFb);

    // Hold Alt to temporarily unlock Full Click-Through so the user can always right-click/drag!
    bool altHeld = gdkAltHeld
        || g_app->input.activeKeys.count(PhysicalKey::LeftOption) > 0
        || g_app->input.activeKeys.count(PhysicalKey::RightOption) > 0;
    if (altHeld != g_app->clickThroughTemporarilyUnlocked) {
        g_app->clickThroughTemporarilyUnlocked = altHeld;
        if (g_app->settings.fullClickThrough) {
            updateInputShape(*g_app);
        }
    }

    if (g_app->demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - g_app->demoStartTime) / 0.13);
        while (g_app->demoStepIdx <= targetIdx && g_app->demoStepIdx < 10) {
            g_app->physics.triggerKeyDown(demoKeys[g_app->demoStepIdx], g_app->settings);
            g_app->demoStepIdx++;
        }
        if (g_app->demoStepIdx >= 10) g_app->demoActive = false;
    }

    // Geometry comes from configure-event; re-sync occasionally as a safety net.
    if (now - g_app->lastGeometryRefresh > 3.0) refreshWindowGeometry(*g_app);
    int winX = g_app->winX, winY = g_app->winY, winW = g_app->winW, winH = g_app->winH;

    // Petting detection using global cursor position (works in Smart AND Full Click-Through modes!)
    if (!g_app->isDragging && winW > 0 && winH > 0) {
        Pt cur = g_app->input.lastCursorPos;
        if (cur.x >= winX && cur.x <= winX + winW && cur.y >= winY && cur.y <= winY + winH) {
            Pt cp = {
                (cur.x - (float)winX) / (float)winW * CatPhysics::CanvasWidth,
                (cur.y - (float)winY) / (float)winH * CatPhysics::CanvasHeight
            };
            CatHitZone zone = CatRenderer::hitTest(cp, g_app->physics, g_app->settings.coatTheme);
            if (zone == CatHitZone::Head) {
                if (g_app->hasLastHover) {
                    float d = std::hypot(cp.x - g_app->lastHoverCanvas.x, cp.y - g_app->lastHoverCanvas.y);
                    if (d > 1.2f && d < 80.0f) {
                        g_app->physics.registerPetStroke(cp, d, true, g_app->settings);
                    }
                }
                g_app->lastHoverCanvas = cp;
                g_app->hasLastHover = true;
            } else {
                g_app->hasLastHover = false;
            }
        } else {
            g_app->hasLastHover = false;
        }
    }

    // Physics clamps dt to 1/25s; sub-step slower idle frames so animations keep real-time speed.
    int substeps = std::max(1, std::min(4, (int)std::ceil(dt / 0.034f)));
    for (int i = 0; i < substeps; ++i) {
        g_app->physics.step(dt / (float)substeps, (float)winX, (float)winY, (float)winW, g_app->input, g_app->settings);
    }
    gtk_widget_queue_draw(g_app->drawArea);

    guint want = desiredFrameIntervalMs(*g_app);
    if (want != g_frameIntervalMs) {
        scheduleFrames(want);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static gboolean onQuitSignal(gpointer) {
    releaseSingleInstance();
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

/// SIGUSR1 = "KuroCat was launched again": show the cat, cancel full click-through, open the menu.
static gboolean onWakeSignal(gpointer) {
    if (!g_app || !g_app->window) return G_SOURCE_CONTINUE;
    if (g_app->settings.fullClickThrough) {
        g_app->settings.fullClickThrough = false;
        g_app->settings.save();
        updateInputShape(*g_app);
    }
    if (!g_app->isVisible) {
        g_app->isVisible = true;
        gtk_widget_show(g_app->window);
    }
    showPopupMenu(*g_app, nullptr);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char** argv) {
    if (argc > 1 && (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0)) {
        std::printf("KuroCat %s\n", APP_VERSION);
        return 0;
    }
    if (!std::getenv("KUROCAT_NO_REDIRECT")) redirectToNewestInstalledCopy(argv);
    unsetenv("KUROCAT_NO_REDIRECT");

    // The XInput2 reader runs on its own thread with its own Display. libX11 < 1.8 (统信 UOS 20,
    // 麒麟 V10, Debian 10/11) is not thread-safe unless this is called before any other Xlib call.
    XInitThreads();
    // Positioning, keep-above and click-through shapes need X11 (XWayland on Wayland sessions).
    gdk_set_allowed_backends("x11,*");
    gtk_init(&argc, &argv);

    LinuxApp app;
    g_app = &app;

    // Install signal handlers BEFORE publishing our PID in the lock file (SIGUSR1 would otherwise
    // kill us by default if a second launch pokes us during startup).
    g_unix_signal_add(SIGTERM, onQuitSignal, nullptr);
    g_unix_signal_add(SIGINT, onQuitSignal, nullptr);
    g_unix_signal_add(SIGHUP, onQuitSignal, nullptr);
    g_unix_signal_add(SIGUSR1, onWakeSignal, nullptr);
    if (!acquireSingleInstance()) {
        g_app = nullptr;
        return 0;
    }

    app.settings.load();
    if (isAutoStartEnabled()) {
        // Keep Exec path updated if user moved portable kurocat binary
        setAutoStartEnabled(true);
    }

    app.input.onKeyDown = [&](PhysicalKey k) { app.physics.triggerKeyDown(k, app.settings); };
    app.input.onMouseClick = [&](int btn, bool down) { app.physics.triggerMouseClick(btn, down, app.settings); };
    app.input.start();

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "KuroCat");
    gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app.window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(app.window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(app.window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(app.window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_stick(GTK_WINDOW(app.window));
    gtk_widget_set_app_paintable(app.window, TRUE);

    GdkScreen* screen = gtk_widget_get_screen(app.window);
    if (GdkVisual* visual = gdk_screen_get_rgba_visual(screen)) {
        gtk_widget_set_visual(app.window, visual);
    }

    app.drawArea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(app.window), app.drawArea);

    gtk_widget_add_events(app.window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_STRUCTURE_MASK);
    g_signal_connect(app.drawArea, "draw", G_CALLBACK(onDraw), nullptr);
    g_signal_connect(app.window, "button-press-event", G_CALLBACK(onButtonPress), nullptr);
    g_signal_connect(app.window, "motion-notify-event", G_CALLBACK(onMotionNotify), nullptr);
    g_signal_connect(app.window, "button-release-event", G_CALLBACK(onButtonRelease), nullptr);
    g_signal_connect(app.window, "configure-event", G_CALLBACK(+[](GtkWidget*, GdkEventConfigure*, gpointer) -> gboolean {
        if (g_app) refreshWindowGeometry(*g_app);
        return FALSE;
    }), nullptr);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    g_signal_connect(app.window, "realize", G_CALLBACK(+[](GtkWidget* w, gpointer) {
        updateInputShape(*g_app);
        if (GdkWindow* gw = gtk_widget_get_window(w)) {
            GdkCursor* hand = gdk_cursor_new_from_name(gdk_window_get_display(gw), "grab");
            if (hand) {
                gdk_window_set_cursor(gw, hand);
                g_object_unref(hand);
            }
        }
    }), nullptr);

    // System tray icon (works cleanly on 统信 UOS, Deepin, KDE, XFCE, MATE!)
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (GdkPixbuf* pb = createCatTrayPixbuf()) {
        app.trayIcon = gtk_status_icon_new_from_pixbuf(pb);
        gtk_status_icon_set_tooltip_text(app.trayIcon, "KuroCat 黑猫键盘桌宠 (右键设置)");
        g_signal_connect(app.trayIcon, "popup-menu", G_CALLBACK(+[](GtkStatusIcon*, guint, guint, gpointer) {
            showPopupMenu(*g_app, nullptr);
        }), nullptr);
        g_signal_connect(app.trayIcon, "activate", G_CALLBACK(+[](GtkStatusIcon*, gpointer) {
            showPopupMenu(*g_app, nullptr);
        }), nullptr);
        g_object_unref(pb);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS

    applyWindowScaleAndPos(app, false);
    gtk_widget_show_all(app.window);
    refreshWindowGeometry(app);

    app.lastTickTime = nowSeconds();
    scheduleFrames(desiredFrameIntervalMs(app));
    startAsyncUpdateCheck(false, 2500);

    gtk_main();
    app.input.stop();
    releaseSingleInstance();
    g_app = nullptr;
    return 0;
}
