#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

// --- Models ---
struct TaskItem {
    std::string id;
    std::string title;
    bool isCompleted = false;
};

struct StreakData {
    int currentStreak = 0;
    std::string lastCompletedDate;
    int previousStreak = 0;
    std::string previousCompletedDate;
    std::string tasksDate;
    std::vector<TaskItem> tasks;
};

const std::vector<std::string> g_quotes = {
    "Small daily promises become a remarkable life.",
    "Start where you are. Use what you have. Do what you can.",
    "Consistency is a quiet kind of courage.",
    "A little progress today is still progress.",
    "Your future self is built by today’s choices.",
    "Keep the promise you made to yourself."
};

std::string GetTodayString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::string(buf);
}

std::string GetYesterdayString() {
    auto now = std::chrono::system_clock::now() - std::chrono::hours(24);
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::string(buf);
}

int GetDayOfYear() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm.tm_yday;
}

std::string GetDataPath() {
    const char* xdg = getenv("XDG_DATA_HOME");
    std::string dir;
    if (xdg) {
        dir = std::string(xdg) + "/streakbar";
    } else {
        const char* home = getenv("HOME");
        if (!home) home = getpwuid(getuid())->pw_dir;
        dir = std::string(home) + "/.local/share/streakbar";
    }
    mkdir(dir.c_str(), 0755);
    return dir + "/streak.json";
}

void SaveData(const StreakData& data) {
    std::ofstream f(GetDataPath());
    if (!f.is_open()) return;
    f << "{\n";
    f << "  \"CurrentStreak\": " << data.currentStreak << ",\n";
    f << "  \"LastCompletedDate\": \"" << data.lastCompletedDate << "\",\n";
    f << "  \"PreviousStreak\": " << data.previousStreak << ",\n";
    f << "  \"PreviousCompletedDate\": \"" << data.previousCompletedDate << "\",\n";
    f << "  \"TasksDate\": \"" << data.tasksDate << "\",\n";
    f << "  \"Tasks\": [\n";
    for (size_t i = 0; i < data.tasks.size(); ++i) {
        f << "    {\n";
        f << "      \"Id\": \"" << data.tasks[i].id << "\",\n";
        f << "      \"Title\": \"" << data.tasks[i].title << "\",\n";
        f << "      \"IsCompleted\": " << (data.tasks[i].isCompleted ? "true" : "false") << "\n";
        f << "    }" << (i + 1 < data.tasks.size() ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

StreakData LoadData() {
    StreakData data;
    std::ifstream f(GetDataPath());
    if (!f.is_open()) return data;

    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();

    auto findInt = [&](const std::string& key) -> int {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        pos = content.find(":", pos);
        if (pos == std::string::npos) return 0;
        return std::atoi(content.c_str() + pos + 1);
    };

    auto findString = [&](const std::string& key) -> std::string {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = content.find("\"", pos + key.length() + 2);
        if (pos == std::string::npos) return "";
        size_t end = content.find("\"", pos + 1);
        if (end == std::string::npos) return "";
        return content.substr(pos + 1, end - pos - 1);
    };

    data.currentStreak = findInt("CurrentStreak");
    data.lastCompletedDate = findString("LastCompletedDate");
    data.previousStreak = findInt("PreviousStreak");
    data.previousCompletedDate = findString("PreviousCompletedDate");
    data.tasksDate = findString("TasksDate");

    size_t tasksPos = content.find("\"Tasks\"");
    if (tasksPos != std::string::npos) {
        size_t cur = tasksPos;
        while ((cur = content.find("{\"Id\"", cur)) != std::string::npos || (cur = content.find("\"Id\"", cur)) != std::string::npos) {
            size_t idStart = content.find("\"", cur + 5);
            if (idStart == std::string::npos) break;
            size_t idEnd = content.find("\"", idStart + 1);
            if (idEnd == std::string::npos) break;
            std::string id = content.substr(idStart + 1, idEnd - idStart - 1);

            size_t titleKey = content.find("\"Title\"", idEnd);
            if (titleKey == std::string::npos) break;
            size_t titleStart = content.find("\"", titleKey + 7);
            if (titleStart == std::string::npos) break;
            size_t titleEnd = content.find("\"", titleStart + 1);
            if (titleEnd == std::string::npos) break;
            std::string title = content.substr(titleStart + 1, titleEnd - titleStart - 1);

            size_t compKey = content.find("\"IsCompleted\"", titleEnd);
            if (compKey == std::string::npos) break;
            bool isComp = (content.find("true", compKey) < content.find("}", compKey));

            data.tasks.push_back({ id, title, isComp });
            cur = titleEnd;
        }
    }

    return data;
}

// Layout Constants
const int WIN_WIDTH = 420;
const int WIN_HEIGHT_COLLAPSED = 40;
const int WIN_HEIGHT_EXPANDED = 370;

StreakData g_data;
bool g_isExpanded = false;
std::string g_inputBuffer = "";
bool g_inputFocused = false;

void CheckRollover() {
    std::string today = GetTodayString();
    if (g_data.tasksDate != today) {
        if (!g_data.tasksDate.empty()) {
            std::string yesterday = GetYesterdayString();
            if (g_data.lastCompletedDate == yesterday) {
                g_data.currentStreak = g_data.currentStreak + 1;
            } else if (g_data.lastCompletedDate != today) {
                g_data.currentStreak = 1;
            }
        }
        g_data.tasksDate = today;
        for (auto& t : g_data.tasks) t.isCompleted = false;
        SaveData(g_data);
    }
}

void ToggleToday() {
    std::string today = GetTodayString();
    if (g_data.lastCompletedDate == today) {
        g_data.lastCompletedDate = "";
    } else {
        if (g_data.currentStreak <= 0) {
            g_data.currentStreak = 1;
        }
        g_data.lastCompletedDate = today;
    }
    SaveData(g_data);
}

void DrawRoundedRect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void Render(cairo_t* cr, int width, int height) {
    // Background #0D080A
    DrawRoundedRect(cr, 0.5, 0.5, width - 1, height - 1, 8.0);
    cairo_set_source_rgb(cr, 13.0 / 255.0, 8.0 / 255.0, 10.0 / 255.0);
    cairo_fill_preserve(cr);
    // Border #4A1218
    cairo_set_source_rgb(cr, 74.0 / 255.0, 18.0 / 255.0, 24.0 / 255.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    std::string today = GetTodayString();
    bool completedToday = (g_data.lastCompletedDate == today);

    // --- Header ---
    // Flame
    cairo_select_font_face(cr, "Noto Color Emoji", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.0);
    cairo_move_to(cr, 16.0, 25.0);
    cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
    cairo_show_text(cr, "🔥");

    // Streak Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cairo_move_to(cr, 42.0, 25.0);
    if (completedToday) {
        cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
    } else {
        cairo_set_source_rgb(cr, 248.0 / 255.0, 250.0 / 255.0, 252.0 / 255.0);
    }
    std::string st = "DAY " + std::to_string(g_data.currentStreak) + (completedToday ? " ✓" : "");
    cairo_show_text(cr, st.c_str());

    // Task indicators
    int total = g_data.tasks.size();
    int done = 0;
    for (const auto& t : g_data.tasks) if (t.isCompleted) done++;

    if (total > 0) {
        double bx = 145.0;
        int maxB = std::min(total, 6);
        for (int i = 0; i < maxB; ++i) {
            DrawRoundedRect(cr, bx + i * 14.0, 15.0, 10.0, 10.0, 2.5);
            if (g_data.tasks[i].isCompleted) {
                cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
                cairo_fill(cr);
            } else {
                cairo_set_source_rgb(cr, 26.0 / 255.0, 10.0 / 255.0, 14.0 / 255.0);
                cairo_fill_preserve(cr);
                cairo_set_source_rgb(cr, 85.0 / 255.0, 25.0 / 255.0, 34.0 / 255.0);
                cairo_set_line_width(cr, 1.0);
                cairo_stroke(cr);
            }
        }
        std::string badge = std::to_string(done) + "/" + std::to_string(total) + (done == total ? " ✓" : "");
        cairo_move_to(cr, bx + maxB * 14.0 + 8.0, 24.0);
        cairo_set_font_size(cr, 11.0);
        if (done == total) cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
        else cairo_set_source_rgb(cr, 113.0 / 255.0, 113.0 / 255.0, 122.0 / 255.0);
        cairo_show_text(cr, badge.c_str());
    }

    // Chevron and Close
    cairo_set_source_rgb(cr, 113.0 / 255.0, 113.0 / 255.0, 122.0 / 255.0);
    cairo_set_font_size(cr, 12.0);
    cairo_move_to(cr, width - 52.0, 25.0);
    cairo_show_text(cr, g_isExpanded ? "▲" : "▼");
    cairo_move_to(cr, width - 28.0, 25.0);
    cairo_show_text(cr, "✕");

    if (g_isExpanded) {
        // Divider
        cairo_set_source_rgb(cr, 45.0 / 255.0, 14.0 / 255.0, 20.0 / 255.0);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 16.0, 40.0);
        cairo_line_to(cr, width - 16.0, 40.0);
        cairo_stroke(cr);

        // Status Card
        DrawRoundedRect(cr, 16.0, 48.0, width - 32.0, 36.0, 6.0);
        cairo_set_source_rgb(cr, 21.0 / 255.0, 9.0 / 255.0, 13.0 / 255.0);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 74.0 / 255.0, 21.0 / 255.0, 28.0 / 255.0);
        cairo_stroke(cr);

        // Indicator dot
        cairo_arc(cr, 28.0, 66.0, 4.0, 0, 2 * M_PI);
        if (completedToday) cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
        else cairo_set_source_rgb(cr, 245.0 / 255.0, 158.0 / 255.0, 11.0 / 255.0);
        cairo_fill(cr);

        cairo_move_to(cr, 38.0, 70.0);
        cairo_set_font_size(cr, 12.0);
        if (completedToday) {
            cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
            cairo_show_text(cr, "Completed Today");
        } else {
            cairo_set_source_rgb(cr, 248.0 / 255.0, 250.0 / 255.0, 252.0 / 255.0);
            cairo_show_text(cr, "Today: In Progress");
        }

        // Toggle button
        DrawRoundedRect(cr, width - 16.0 - 114.0 - 5.0, 53.0, 114.0, 26.0, 5.0);
        if (completedToday) {
            cairo_set_source_rgb(cr, 43.0 / 255.0, 14.0 / 255.0, 19.0 / 255.0);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 92.0 / 255.0, 29.0 / 255.0, 36.0 / 255.0);
            cairo_stroke(cr);
            cairo_move_to(cr, width - 16.0 - 114.0 + 18.0, 70.0);
            cairo_set_source_rgb(cr, 161.0 / 255.0, 161.0 / 255.0, 170.0 / 255.0);
            cairo_show_text(cr, "↩ Undo Day");
        } else {
            cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
            cairo_fill(cr);
            cairo_move_to(cr, width - 16.0 - 114.0 + 16.0, 70.0);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_show_text(cr, "✓ Mark Done");
        }

        // Add Task Input Row
        DrawRoundedRect(cr, 16.0, 92.0, width - 32.0 - 74.0, 32.0, 6.0);
        cairo_set_source_rgb(cr, 21.0 / 255.0, 9.0 / 255.0, 13.0 / 255.0);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, g_inputFocused ? 239.0 / 255.0 : 74.0 / 255.0, g_inputFocused ? 68.0 / 255.0 : 21.0 / 255.0, g_inputFocused ? 68.0 / 255.0 : 28.0 / 255.0);
        cairo_stroke(cr);

        cairo_move_to(cr, 26.0, 112.0);
        cairo_set_font_size(cr, 12.0);
        if (g_inputBuffer.empty() && !g_inputFocused) {
            cairo_set_source_rgb(cr, 113.0 / 255.0, 113.0 / 255.0, 122.0 / 255.0);
            cairo_show_text(cr, "Type task and press Enter...");
        } else {
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            std::string disp = g_inputBuffer + (g_inputFocused ? "|" : "");
            cairo_show_text(cr, disp.c_str());
        }

        // Add Button
        DrawRoundedRect(cr, width - 16.0 - 66.0, 92.0, 66.0, 32.0, 6.0);
        cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
        cairo_fill(cr);
        cairo_move_to(cr, width - 16.0 - 52.0, 112.0);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_show_text(cr, "+ Add");

        // Task Items or Empty state
        if (g_data.tasks.empty()) {
            cairo_move_to(cr, width / 2.0 - 70.0, 195.0);
            cairo_set_source_rgb(cr, 113.0 / 255.0, 113.0 / 255.0, 122.0 / 255.0);
            cairo_show_text(cr, "No tasks tracked today");
            cairo_move_to(cr, width / 2.0 - 110.0, 218.0);
            cairo_set_source_rgb(cr, 82.0 / 255.0, 82.0 / 255.0, 91.0 / 255.0);
            cairo_show_text(cr, "Add one above to stay consistent!");
        } else {
            double ty = 134.0;
            for (size_t i = 0; i < g_data.tasks.size() && ty < 315.0; ++i) {
                const auto& item = g_data.tasks[i];
                DrawRoundedRect(cr, 20.0, ty + 3.0, 16.0, 16.0, 3.5);
                if (item.isCompleted) {
                    cairo_set_source_rgb(cr, 239.0 / 255.0, 68.0 / 255.0, 68.0 / 255.0);
                    cairo_fill(cr);
                    cairo_move_to(cr, 23.0, ty + 15.0);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    cairo_set_font_size(cr, 11.0);
                    cairo_show_text(cr, "✓");
                } else {
                    cairo_set_source_rgb(cr, 26.0 / 255.0, 10.0 / 255.0, 14.0 / 255.0);
                    cairo_fill_preserve(cr);
                    cairo_set_source_rgb(cr, 85.0 / 255.0, 25.0 / 255.0, 34.0 / 255.0);
                    cairo_stroke(cr);
                }

                cairo_move_to(cr, 46.0, ty + 16.0);
                cairo_set_font_size(cr, 12.0);
                if (item.isCompleted) {
                    cairo_set_source_rgb(cr, 82.0 / 255.0, 82.0 / 255.0, 91.0 / 255.0);
                    cairo_show_text(cr, item.title.c_str());
                    cairo_text_extents_t ext;
                    cairo_text_extents(cr, item.title.c_str(), &ext);
                    cairo_set_source_rgb(cr, 95.0 / 255.0, 95.0 / 255.0, 105.0 / 255.0);
                    cairo_set_line_width(cr, 1.0);
                    cairo_move_to(cr, 46.0, ty + 12.0);
                    cairo_line_to(cr, 46.0 + ext.width, ty + 12.0);
                    cairo_stroke(cr);
                } else {
                    cairo_set_source_rgb(cr, 248.0 / 255.0, 250.0 / 255.0, 252.0 / 255.0);
                    cairo_show_text(cr, item.title.c_str());
                }

                // Delete ✕
                cairo_move_to(cr, width - 34.0, ty + 16.0);
                cairo_set_source_rgb(cr, 153.0 / 255.0, 27.0 / 255.0, 27.0 / 255.0);
                cairo_show_text(cr, "✕");

                ty += 26.0;
            }
        }

        // Quote
        cairo_set_source_rgb(cr, 40.0 / 255.0, 12.0 / 255.0, 17.0 / 255.0);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 20.0, 332.0);
        cairo_line_to(cr, width - 20.0, 332.0);
        cairo_stroke(cr);

        int qIdx = GetDayOfYear() % g_quotes.size();
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.5);
        cairo_set_source_rgb(cr, 113.0 / 255.0, 113.0 / 255.0, 122.0 / 255.0);
        cairo_move_to(cr, 30.0, 350.0);
        cairo_show_text(cr, g_quotes[qIdx].c_str());
    }
}

int main() {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | StructureNotifyMask;

    int screenWidth = DisplayWidth(dpy, screen);
    int winX = (screenWidth - WIN_WIDTH) / 2;
    int winY = 0;

    Window win = XCreateWindow(dpy, root, winX, winY, WIN_WIDTH, WIN_HEIGHT_COLLAPSED, 0,
                               DefaultDepth(dpy, screen), InputOutput,
                               DefaultVisual(dpy, screen), CWOverrideRedirect | CWEventMask, &attrs);

    XMapWindow(dpy, win);

    cairo_surface_t* surface = cairo_xlib_surface_create(dpy, win, DefaultVisual(dpy, screen), WIN_WIDTH, WIN_HEIGHT_EXPANDED);
    cairo_t* cr = cairo_create(surface);

    g_data = LoadData();
    CheckRollover();

    XEvent ev;
    bool running = true;

    while (running) {
        XNextEvent(dpy, &ev);
        if (ev.type == Expose) {
            int h = g_isExpanded ? WIN_HEIGHT_EXPANDED : WIN_HEIGHT_COLLAPSED;
            Render(cr, WIN_WIDTH, h);
        } else if (ev.type == ButtonPress) {
            int x = ev.xbutton.x;
            int y = ev.xbutton.y;

            if (y <= 40) {
                if (x >= WIN_WIDTH - 36) {
                    running = false;
                } else if (x >= WIN_WIDTH - 64) {
                    g_isExpanded = !g_isExpanded;
                    int newH = g_isExpanded ? WIN_HEIGHT_EXPANDED : WIN_HEIGHT_COLLAPSED;
                    XResizeWindow(dpy, win, WIN_WIDTH, newH);
                } else if (x <= 135) {
                    ToggleToday();
                }
            } else if (g_isExpanded) {
                if (x >= WIN_WIDTH - 145 && x <= WIN_WIDTH - 21 && y >= 53 && y <= 79) {
                    ToggleToday();
                } else if (x >= 16 && x <= WIN_WIDTH - 88 && y >= 92 && y <= 124) {
                    g_inputFocused = true;
                } else {
                    g_inputFocused = false;
                }

                if (x >= WIN_WIDTH - 80 && x <= WIN_WIDTH - 16 && y >= 92 && y <= 124) {
                    if (!g_inputBuffer.empty()) {
                        g_data.tasks.push_back({ std::to_string(time(NULL)), g_inputBuffer, false });
                        g_inputBuffer.clear();
                        SaveData(g_data);
                    }
                }

                // Tasks clicks
                double ty = 134.0;
                for (size_t i = 0; i < g_data.tasks.size() && ty < 315.0; ++i) {
                    if (x >= 16 && x <= 40 && y >= (int)ty && y <= (int)ty + 24) {
                        g_data.tasks[i].isCompleted = !g_data.tasks[i].isCompleted;
                        SaveData(g_data);
                        break;
                    }
                    if (x >= WIN_WIDTH - 42 && x <= WIN_WIDTH - 14 && y >= (int)ty && y <= (int)ty + 24) {
                        g_data.tasks.erase(g_data.tasks.begin() + i);
                        SaveData(g_data);
                        break;
                    }
                    ty += 26.0;
                }
            }
            int h = g_isExpanded ? WIN_HEIGHT_EXPANDED : WIN_HEIGHT_COLLAPSED;
            Render(cr, WIN_WIDTH, h);
        } else if (ev.type == KeyPress && g_isExpanded && g_inputFocused) {
            char buf[32];
            KeySym ks;
            int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
            if (ks == XK_BackSpace) {
                if (!g_inputBuffer.empty()) g_inputBuffer.pop_back();
            } else if (ks == XK_Return) {
                if (!g_inputBuffer.empty()) {
                    g_data.tasks.push_back({ std::to_string(time(NULL)), g_inputBuffer, false });
                    g_inputBuffer.clear();
                    SaveData(g_data);
                }
            } else if (len > 0 && buf[0] >= 32) {
                g_inputBuffer += buf[0];
            }
            int h = g_isExpanded ? WIN_HEIGHT_EXPANDED : WIN_HEIGHT_COLLAPSED;
            Render(cr, WIN_WIDTH, h);
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
