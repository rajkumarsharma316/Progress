#define UNICODE
#define _UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

// --- Data Models ---
struct TaskItem {
    std::wstring id;
    std::wstring title;
    bool isCompleted = false;
};

struct StreakData {
    int currentStreak = 0;
    std::wstring lastCompletedDate;
    int previousStreak = 0;
    std::wstring previousCompletedDate;
    std::wstring tasksDate;
    std::vector<TaskItem> tasks;
};

// --- Daily Quotes ---
const std::vector<std::wstring> g_quotes = {
    L"Small daily promises become a remarkable life.",
    L"Start where you are. Use what you have. Do what you can.",
    L"Consistency is a quiet kind of courage.",
    L"A little progress today is still progress.",
    L"Your future self is built by today’s choices.",
    L"Keep the promise you made to yourself."
};

// --- Helper Functions ---
std::wstring GetTodayString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::wstring(buf);
}

std::wstring GetYesterdayString() {
    auto now = std::chrono::system_clock::now() - std::chrono::hours(24);
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::wstring(buf);
}

int GetDayOfYear() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    return tm.tm_yday;
}

std::wstring GenerateId() {
    static long long counter = 0;
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return std::to_wstring(now) + L"_" + std::to_wstring(++counter);
}

std::wstring GetAppDataPath() {
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        std::wstring path = localAppData;
        path += L"\\StreakBar";
        CreateDirectoryW(path.c_str(), NULL);
        return path + L"\\streak.json";
    }
    return L"streak.json";
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

std::string SerializeDataJson(const StreakData& data) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"CurrentStreak\": " << data.currentStreak << ",\n";
    ss << "  \"LastCompletedDate\": \"" << WideToUtf8(data.lastCompletedDate) << "\",\n";
    ss << "  \"PreviousStreak\": " << data.previousStreak << ",\n";
    ss << "  \"PreviousCompletedDate\": \"" << WideToUtf8(data.previousCompletedDate) << "\",\n";
    ss << "  \"TasksDate\": \"" << WideToUtf8(data.tasksDate) << "\",\n";
    ss << "  \"Tasks\": [\n";
    for (size_t i = 0; i < data.tasks.size(); ++i) {
        ss << "    {\n";
        ss << "      \"Id\": \"" << WideToUtf8(data.tasks[i].id) << "\",\n";
        ss << "      \"Title\": \"" << WideToUtf8(data.tasks[i].title) << "\",\n";
        ss << "      \"IsCompleted\": " << (data.tasks[i].isCompleted ? "true" : "false") << "\n";
        ss << "    }" << (i + 1 < data.tasks.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

StreakData DeserializeDataJson(const std::string& content) {
    StreakData data;

    auto findString = [&](const std::string& key) -> std::wstring {
        size_t keyPos = content.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return L"";
        size_t colonPos = content.find(":", keyPos + key.length() + 2);
        if (colonPos == std::string::npos) return L"";
        size_t valStart = content.find("\"", colonPos);
        if (valStart == std::string::npos) return L"";
        size_t valEnd = content.find("\"", valStart + 1);
        if (valEnd == std::string::npos) return L"";
        return Utf8ToWide(content.substr(valStart + 1, valEnd - valStart - 1));
    };

    auto findInt = [&](const std::string& key) -> int {
        size_t keyPos = content.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return 0;
        size_t colonPos = content.find(":", keyPos + key.length() + 2);
        if (colonPos == std::string::npos) return 0;
        size_t numStart = colonPos + 1;
        while (numStart < content.size() && (content[numStart] == ' ' || content[numStart] == '\t' || content[numStart] == '\r' || content[numStart] == '\n')) {
            numStart++;
        }
        return std::atoi(content.c_str() + numStart);
    };

    data.currentStreak = findInt("CurrentStreak");
    data.lastCompletedDate = findString("LastCompletedDate");
    data.previousStreak = findInt("PreviousStreak");
    data.previousCompletedDate = findString("PreviousCompletedDate");
    data.tasksDate = findString("TasksDate");

    size_t tasksPos = content.find("\"Tasks\"");
    if (tasksPos != std::string::npos) {
        size_t cur = tasksPos;
        while ((cur = content.find("\"Id\"", cur)) != std::string::npos) {
            size_t colonPos = content.find(":", cur + 4);
            if (colonPos == std::string::npos) break;
            size_t idStart = content.find("\"", colonPos);
            if (idStart == std::string::npos) break;
            size_t idEnd = content.find("\"", idStart + 1);
            if (idEnd == std::string::npos) break;
            std::wstring id = Utf8ToWide(content.substr(idStart + 1, idEnd - idStart - 1));

            size_t titleKey = content.find("\"Title\"", idEnd);
            if (titleKey == std::string::npos) break;
            size_t titleColon = content.find(":", titleKey + 7);
            if (titleColon == std::string::npos) break;
            size_t titleStart = content.find("\"", titleColon);
            if (titleStart == std::string::npos) break;
            size_t titleEnd = content.find("\"", titleStart + 1);
            if (titleEnd == std::string::npos) break;
            std::wstring title = Utf8ToWide(content.substr(titleStart + 1, titleEnd - titleStart - 1));

            size_t compKey = content.find("\"IsCompleted\"", titleEnd);
            if (compKey == std::string::npos) break;
            size_t compColon = content.find(":", compKey + 13);
            if (compColon == std::string::npos) break;
            size_t truePos = content.find("true", compColon);
            size_t bracePos = content.find("}", compColon);
            bool isComp = (truePos != std::string::npos && (bracePos == std::string::npos || truePos < bracePos));

            data.tasks.push_back({ id, title, isComp });
            cur = compColon;
        }
    }

    return data;
}

void SaveData(const StreakData& data) {
    std::wstring path = GetAppDataPath();
    std::ofstream file(path.c_str());
    if (!file.is_open()) return;
    file << SerializeDataJson(data);
}

StreakData LoadData() {
    std::wstring path = GetAppDataPath();
    std::ifstream file(path.c_str());
    if (!file.is_open()) return StreakData();

    std::stringstream buffer;
    buffer << file.rdbuf();
    return DeserializeDataJson(buffer.str());
}

// --- Application Layout Constants ---
const int WIN_WIDTH = 420;
const int WIN_HEIGHT_COLLAPSED = 40;
const int WIN_HEIGHT_EXPANDED = 390;

// --- Application State ---
StreakData g_data;
std::mutex g_dataMutex;
bool g_isExpanded = false;
HWND g_hwnd = NULL;
std::wstring g_inputBuffer = L"";
bool g_inputFocused = false;
bool g_cursorVisible = true;
std::wstring g_localIp = L"127.0.0.1";
bool g_copiedFeedback = false;
int g_copiedFeedbackTicks = 0;

void DetectLocalIp() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {0}, *info = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
            for (struct addrinfo* p = info; p != nullptr; p = p->ai_next) {
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
                std::string s(ipStr);
                if (s.rfind("192.168.", 0) == 0 || s.rfind("10.", 0) == 0 || s.rfind("172.", 0) == 0) {
                    g_localIp = Utf8ToWide(s);
                    break;
                }
                if (g_localIp == L"127.0.0.1" && s != "127.0.0.1") {
                    g_localIp = Utf8ToWide(s);
                }
            }
            freeaddrinfo(info);
        }
    }
}

void StartSyncServer() {
    std::thread([]() {
        SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSock == INVALID_SOCKET) return;

        BOOL opt = TRUE;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(54321);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(serverSock);
            return;
        }

        if (listen(serverSock, 5) == SOCKET_ERROR) {
            closesocket(serverSock);
            return;
        }

        while (true) {
            SOCKET clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock == INVALID_SOCKET) break;

            char buf[8192];
            int bytes = recv(clientSock, buf, sizeof(buf) - 1, 0);
            if (bytes > 0) {
                buf[bytes] = '\0';
                std::string req(buf);

                if (req.rfind("OPTIONS", 0) == 0) {
                    std::string resp = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
                    send(clientSock, resp.c_str(), (int)resp.length(), 0);
                } else if (req.find("GET /api/sync") != std::string::npos) {
                    std::string json;
                    {
                        std::lock_guard<std::mutex> lock(g_dataMutex);
                        json = SerializeDataJson(g_data);
                    }
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " + std::to_string(json.length()) + "\r\nConnection: close\r\n\r\n" + json;
                    send(clientSock, resp.c_str(), (int)resp.length(), 0);
                } else if (req.find("POST /api/sync") != std::string::npos) {
                    size_t bodyPos = req.find("\r\n\r\n");
                    if (bodyPos != std::string::npos) {
                        std::string body = req.substr(bodyPos + 4);
                        StreakData incoming = DeserializeDataJson(body);

                        {
                            std::lock_guard<std::mutex> lock(g_dataMutex);
                            if (incoming.currentStreak > 0) {
                                g_data.currentStreak = incoming.currentStreak;
                            }
                            SaveData(g_data);
                        }

                        if (g_hwnd) {
                            PostMessage(g_hwnd, WM_USER + 10, 0, 0);
                        }
                    }

                    std::string respJson = "{\"status\":\"ok\"}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " + std::to_string(respJson.length()) + "\r\nConnection: close\r\n\r\n" + respJson;
                    send(clientSock, resp.c_str(), (int)resp.length(), 0);
                } else if (req.find("GET /api/ping") != std::string::npos) {
                    std::string respJson = "{\"app\":\"StreakBar\",\"port\":54321}";
                    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " + std::to_string(respJson.length()) + "\r\nConnection: close\r\n\r\n" + respJson;
                    send(clientSock, resp.c_str(), (int)resp.length(), 0);
                } else {
                    std::string notFound = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                    send(clientSock, notFound.c_str(), (int)notFound.length(), 0);
                }
            }
            closesocket(clientSock);
        }
    }).detach();
}

void CopyIpToClipboard(HWND hwnd) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        std::wstring text = g_localIp + L":54321";
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
        CloseClipboard();
        g_copiedFeedback = true;
        g_copiedFeedbackTicks = 4; // ~2 seconds
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

void CheckRollover() {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    std::wstring today = GetTodayString();
    if (g_data.tasksDate != today) {
        std::wstring yesterday = GetYesterdayString();
        if (g_data.lastCompletedDate == yesterday) {
            g_data.currentStreak = g_data.currentStreak + 1;
        } else if (g_data.lastCompletedDate != today) {
            g_data.currentStreak = 1;
        }
        g_data.tasksDate = today;
        for (auto& t : g_data.tasks) {
            t.isCompleted = false;
        }
        SaveData(g_data);
    }
}

void CompleteToday() {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    std::wstring today = GetTodayString();
    if (g_data.lastCompletedDate == today) return;

    if (g_data.currentStreak <= 0) {
        g_data.currentStreak = 1;
    }
    // Confirms today as completed without runaway increment
    g_data.lastCompletedDate = today;
    SaveData(g_data);
}

void UncompleteToday() {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    std::wstring today = GetTodayString();
    if (g_data.lastCompletedDate != today) return;

    g_data.lastCompletedDate = L"";
    SaveData(g_data);
}

void ToggleToday() {
    std::wstring today = GetTodayString();
    bool isDone = false;
    {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        isDone = (g_data.lastCompletedDate == today);
    }
    if (isDone) UncompleteToday(); else CompleteToday();
}

void UpdateAppRegion(int width, int height) {
    HRGN rgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, 16, 16);
    SetWindowRgn(g_hwnd, rgn, TRUE);
}

void SetWindowExpansion(bool expand) {
    g_isExpanded = expand;
    int height = expand ? WIN_HEIGHT_EXPANDED : WIN_HEIGHT_COLLAPSED;

    RECT curRc;
    GetWindowRect(g_hwnd, &curRc);

    SetWindowPos(g_hwnd, HWND_TOPMOST, curRc.left, curRc.top, WIN_WIDTH, height, SWP_NOACTIVATE | SWP_FRAMECHANGED);
    UpdateAppRegion(WIN_WIDTH, height);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

// --- Drawing Helpers ---
void FillRoundedRectangle(Graphics& g, Brush* brush, float x, float y, float w, float h, float radius) {
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90);
    path.AddArc(x + w - radius * 2, y, radius * 2, radius * 2, 270, 90);
    path.AddArc(x + w - radius * 2, y + h - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(x, y + h - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();
    g.FillPath(brush, &path);
}

void DrawRoundedRectangle(Graphics& g, Pen* pen, float x, float y, float w, float h, float radius) {
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90);
    path.AddArc(x + w - radius * 2, y, radius * 2, radius * 2, 270, 90);
    path.AddArc(x + w - radius * 2, y + h - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(x, y + h - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();
    g.DrawPath(pen, &path);
}

// --- Paint Function ---
void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    Color bgObsidian(255, 13, 8, 10);
    Color borderRed(255, 74, 18, 24);
    Color primaryRed(255, 239, 68, 68);
    Color darkRed(255, 153, 27, 27);
    Color cardBg(255, 21, 9, 13);
    Color cardBorder(255, 74, 21, 28);
    Color textWhite(255, 248, 250, 252);
    Color textMuted(255, 113, 113, 122);
    Color textDim(255, 82, 82, 91);
    Color wineButton(255, 43, 14, 19);
    Color wineBorder(255, 92, 29, 36);

    SolidBrush bgBrush(bgObsidian);
    Pen borderPen(borderRed, 1.5f);

    FillRoundedRectangle(g, &bgBrush, 0, 0, (float)width - 1, (float)height - 1, 8.0f);
    DrawRoundedRectangle(g, &borderPen, 0.5f, 0.5f, (float)width - 2, (float)height - 2, 8.0f);

    FontFamily ff(L"Segoe UI");
    Font fontHeader(&ff, 10.5f, FontStyleBold, UnitPoint);
    Font fontNormal(&ff, 9.0f, FontStyleRegular, UnitPoint);
    Font fontNormalBold(&ff, 9.0f, FontStyleBold, UnitPoint);
    Font fontSmall(&ff, 8.0f, FontStyleRegular, UnitPoint);
    Font fontSmallBold(&ff, 8.0f, FontStyleBold, UnitPoint);
    Font fontQuote(&ff, 8.0f, FontStyleItalic, UnitPoint);
    Font fontEmoji(L"Segoe UI Emoji", 11.0f, FontStyleRegular, UnitPoint);

    SolidBrush redBrush(primaryRed);
    SolidBrush darkRedBrush(darkRed);
    SolidBrush whiteBrush(textWhite);
    SolidBrush mutedBrush(textMuted);
    SolidBrush dimBrush(textDim);
    SolidBrush darkBoxBg(Color(255, 26, 10, 14));
    Pen darkBoxPen(Color(255, 85, 25, 34), 1.0f);

    StringFormat centerFormat;
    centerFormat.SetAlignment(StringAlignmentCenter);
    centerFormat.SetLineAlignment(StringAlignmentCenter);

    StringFormat leftVcenter;
    leftVcenter.SetAlignment(StringAlignmentNear);
    leftVcenter.SetLineAlignment(StringAlignmentCenter);

    std::wstring today = GetTodayString();
    bool completedToday = false;
    int currentStreak = 0;
    int totalTasks = 0;
    int completedTasks = 0;

    {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        completedToday = (g_data.lastCompletedDate == today);
        currentStreak = g_data.currentStreak;
        totalTasks = (int)g_data.tasks.size();
        for (const auto& t : g_data.tasks) {
            if (t.isCompleted) completedTasks++;
        }
    }

    // --- Header ---
    RectF fireRect(14.0f, 0.0f, 22.0f, 40.0f);
    g.DrawString(L"🔥", -1, &fontEmoji, fireRect, &centerFormat, &redBrush);

    std::wstring streakText = L"DAY " + std::to_wstring(currentStreak);
    if (completedToday) streakText += L" ✓";
    RectF streakRect(40.0f, 0.0f, 90.0f, 40.0f);
    g.DrawString(streakText.c_str(), -1, &fontHeader, streakRect, &leftVcenter, completedToday ? &redBrush : &whiteBrush);

    if (totalTasks > 0) {
        float startX = 145.0f;
        int maxBoxes = std::min(totalTasks, 6);
        std::lock_guard<std::mutex> lock(g_dataMutex);
        for (int i = 0; i < maxBoxes; ++i) {
            float bx = startX + i * 14.0f;
            float by = 15.0f;
            if (g_data.tasks[i].isCompleted) {
                FillRoundedRectangle(g, &redBrush, bx, by, 10.0f, 10.0f, 2.5f);
            } else {
                FillRoundedRectangle(g, &darkBoxBg, bx, by, 10.0f, 10.0f, 2.5f);
                DrawRoundedRectangle(g, &darkBoxPen, bx, by, 10.0f, 10.0f, 2.5f);
            }
        }
        std::wstring badge = std::to_wstring(completedTasks) + L"/" + std::to_wstring(totalTasks);
        if (completedTasks == totalTasks) badge += L" ✓";
        RectF badgeRect(startX + maxBoxes * 14.0f + 6.0f, 0.0f, 50.0f, 40.0f);
        g.DrawString(badge.c_str(), -1, &fontSmallBold, badgeRect, &leftVcenter, (completedTasks == totalTasks) ? &redBrush : &mutedBrush);
    } else {
        RectF hintRect(145.0f, 0.0f, 120.0f, 40.0f);
        g.DrawString(L"0 tasks", -1, &fontNormal, hintRect, &leftVcenter, &dimBrush);
    }

    RectF chevronRect((float)width - 58.0f, 8.0f, 24.0f, 24.0f);
    g.DrawString(g_isExpanded ? L"▲" : L"▼", -1, &fontNormal, chevronRect, &centerFormat, &mutedBrush);

    RectF closeRect((float)width - 32.0f, 8.0f, 24.0f, 24.0f);
    g.DrawString(L"✕", -1, &fontNormal, closeRect, &centerFormat, &mutedBrush);

    // --- Expanded Section ---
    if (g_isExpanded) {
        Pen divPen(Color(255, 45, 14, 20), 1.0f);
        g.DrawLine(&divPen, 16, 40, width - 16, 40);

        // Status Banner
        float bannerX = 16.0f;
        float bannerY = 48.0f;
        float bannerW = (float)width - 32.0f;
        float bannerH = 36.0f;

        SolidBrush bannerBg(cardBg);
        Pen bannerBorder(cardBorder, 1.0f);
        FillRoundedRectangle(g, &bannerBg, bannerX, bannerY, bannerW, bannerH, 6.0f);
        DrawRoundedRectangle(g, &bannerBorder, bannerX, bannerY, bannerW, bannerH, 6.0f);

        if (completedToday) {
            SolidBrush dotBrush(primaryRed);
            g.FillEllipse(&dotBrush, bannerX + 12.0f, bannerY + 14.0f, 8.0f, 8.0f);

            RectF statusTextRect(bannerX + 26.0f, bannerY, 150.0f, bannerH);
            g.DrawString(L"Completed Today", -1, &fontNormalBold, statusTextRect, &leftVcenter, &redBrush);
        } else {
            SolidBrush dotBrush(Color(255, 245, 158, 11));
            g.FillEllipse(&dotBrush, bannerX + 12.0f, bannerY + 14.0f, 8.0f, 8.0f);

            RectF statusTextRect(bannerX + 26.0f, bannerY, 150.0f, bannerH);
            g.DrawString(L"Today: In Progress", -1, &fontNormal, statusTextRect, &leftVcenter, &whiteBrush);
        }

        float btnW = 114.0f;
        float btnH = 26.0f;
        float btnX = bannerX + bannerW - btnW - 5.0f;
        float btnY = bannerY + 5.0f;

        if (completedToday) {
            SolidBrush undoBg(wineButton);
            Pen undoPen(wineBorder, 1.0f);
            FillRoundedRectangle(g, &undoBg, btnX, btnY, btnW, btnH, 5.0f);
            DrawRoundedRectangle(g, &undoPen, btnX, btnY, btnW, btnH, 5.0f);

            RectF btnTextRect(btnX, btnY, btnW, btnH);
            g.DrawString(L"↩ Undo Day", -1, &fontNormalBold, btnTextRect, &centerFormat, &mutedBrush);
        } else {
            SolidBrush doneBg(primaryRed);
            FillRoundedRectangle(g, &doneBg, btnX, btnY, btnW, btnH, 5.0f);

            RectF btnTextRect(btnX, btnY, btnW, btnH);
            g.DrawString(L"✓ Mark Done", -1, &fontNormalBold, btnTextRect, &centerFormat, &whiteBrush);
        }

        // Add Task Input Row
        float inputRowY = 92.0f;
        float addBtnW = 66.0f;
        float inputW = (float)width - 32.0f - addBtnW - 8.0f;

        SolidBrush inputBg(cardBg);
        Pen inputPen(g_inputFocused ? primaryRed : cardBorder, 1.0f);
        FillRoundedRectangle(g, &inputBg, 16.0f, inputRowY, inputW, 32.0f, 6.0f);
        DrawRoundedRectangle(g, &inputPen, 16.0f, inputRowY, inputW, 32.0f, 6.0f);

        RectF inputRect(26.0f, inputRowY, inputW - 20.0f, 32.0f);
        if (g_inputBuffer.empty() && !g_inputFocused) {
            g.DrawString(L"Type task and press Enter...", -1, &fontNormal, inputRect, &leftVcenter, &dimBrush);
        } else {
            std::wstring displayStr = g_inputBuffer;
            if (g_inputFocused && g_cursorVisible) {
                displayStr += L"|";
            }
            g.DrawString(displayStr.c_str(), -1, &fontNormal, inputRect, &leftVcenter, &whiteBrush);
        }

        float addBtnX = 16.0f + inputW + 8.0f;
        SolidBrush addBtnBg(primaryRed);
        FillRoundedRectangle(g, &addBtnBg, addBtnX, inputRowY, addBtnW, 32.0f, 6.0f);

        RectF addBtnRect(addBtnX, inputRowY, addBtnW, 32.0f);
        g.DrawString(L"+ Add", -1, &fontNormalBold, addBtnRect, &centerFormat, &whiteBrush);

        // Task Items Viewport
        float listY = 134.0f;
        if (totalTasks == 0) {
            RectF emptyRect(20.0f, 175.0f, (float)width - 40.0f, 22.0f);
            g.DrawString(L"No tasks tracked today", -1, &fontNormal, emptyRect, &centerFormat, &mutedBrush);

            RectF emptySubRect(20.0f, 197.0f, (float)width - 40.0f, 18.0f);
            g.DrawString(L"Add one above to stay consistent!", -1, &fontQuote, emptySubRect, &centerFormat, &dimBrush);
        } else {
            float taskY = listY;
            std::lock_guard<std::mutex> lock(g_dataMutex);
            for (size_t i = 0; i < g_data.tasks.size() && taskY < 305.0f; ++i) {
                const auto& item = g_data.tasks[i];

                float boxX = 20.0f;
                float boxY = taskY + 3.0f;
                if (item.isCompleted) {
                    FillRoundedRectangle(g, &redBrush, boxX, boxY, 16.0f, 16.0f, 3.5f);
                    RectF checkRect(boxX, boxY - 1.0f, 16.0f, 16.0f);
                    g.DrawString(L"✓", -1, &fontSmallBold, checkRect, &centerFormat, &whiteBrush);
                } else {
                    FillRoundedRectangle(g, &darkBoxBg, boxX, boxY, 16.0f, 16.0f, 3.5f);
                    DrawRoundedRectangle(g, &darkBoxPen, boxX, boxY, 16.0f, 16.0f, 3.5f);
                }

                RectF titleRect(46.0f, taskY, (float)width - 85.0f, 24.0f);
                g.DrawString(item.title.c_str(), -1, &fontNormal, titleRect, &leftVcenter, item.isCompleted ? &dimBrush : &whiteBrush);

                if (item.isCompleted) {
                    RectF bounds;
                    g.MeasureString(item.title.c_str(), -1, &fontNormal, titleRect, &bounds);
                    Pen strikePen(Color(255, 95, 95, 105), 1.2f);
                    g.DrawLine(&strikePen, 46.0f, taskY + 12.0f, std::min(46.0f + bounds.Width, (float)width - 85.0f), taskY + 12.0f);
                }

                float delX = (float)width - 38.0f;
                RectF delRect(delX, taskY, 22.0f, 24.0f);
                g.DrawString(L"✕", -1, &fontNormal, delRect, &centerFormat, &darkRedBrush);

                taskY += 26.0f;
            }
        }

        // Quote Section (Y: 322 to 346)
        Pen quoteLinePen(Color(255, 40, 12, 17), 1.0f);
        g.DrawLine(&quoteLinePen, 20, 322, width - 20, 322);

        int quoteIdx = GetDayOfYear() % g_quotes.size();
        RectF quoteRect(20.0f, 326.0f, (float)width - 40.0f, 20.0f);
        g.DrawString(g_quotes[quoteIdx].c_str(), -1, &fontQuote, quoteRect, &centerFormat, &dimBrush);

        // Wi-Fi Sync Status Footer (Y: 350 to 380)
        Pen syncLinePen(Color(255, 30, 10, 14), 1.0f);
        g.DrawLine(&syncLinePen, 20, 348, width - 20, 348);

        // Green Sync indicator dot
        SolidBrush greenDot(Color(255, 34, 197, 94));
        g.FillEllipse(&greenDot, 20.0f, 362.0f, 6.0f, 6.0f);

        std::wstring syncLabel = L"Wi-Fi Sync: " + g_localIp + L":54321";
        RectF syncLabelRect(30.0f, 355.0f, 250.0f, 20.0f);
        g.DrawString(syncLabel.c_str(), -1, &fontSmall, syncLabelRect, &leftVcenter, &mutedBrush);

        // Copy IP Badge / Button (X: width - 86, Y: 354, W: 70, H: 22)
        float copyW = 70.0f;
        float copyX = (float)width - 16.0f - copyW;
        float copyY = 354.0f;
        float copyH = 22.0f;

        SolidBrush copyBg(Color(255, 26, 10, 15));
        Pen copyBorder(Color(255, 60, 18, 24), 1.0f);
        FillRoundedRectangle(g, &copyBg, copyX, copyY, copyW, copyH, 4.0f);
        DrawRoundedRectangle(g, &copyBorder, copyX, copyY, copyW, copyH, 4.0f);

        RectF copyTextRect(copyX, copyY, copyW, copyH);
        std::wstring copyText = g_copiedFeedback ? L"✓ Copied!" : L"Copy Link";
        SolidBrush copyTextBrush(g_copiedFeedback ? Color(255, 34, 197, 94) : Color(255, 161, 161, 170));
        g.DrawString(copyText.c_str(), -1, &fontSmallBold, copyTextRect, &centerFormat, &copyTextBrush);
    }

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        OnPaint(hwnd);
        return 0;

    case WM_USER + 10: // Remote Sync Update from Phone
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            CheckRollover();
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (wParam == 2) {
            if (g_inputFocused) {
                g_cursorVisible = !g_cursorVisible;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (g_copiedFeedback) {
                if (--g_copiedFeedbackTicks <= 0) {
                    g_copiedFeedback = false;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
        }
        return 0;

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;

        bool isHand = false;
        bool isIbeam = false;

        if (pt.y <= 40) {
            if (pt.x >= width - 60 && pt.x <= width - 8) isHand = true;
            if (pt.x >= 14 && pt.x <= 135) isHand = true;
        } else if (g_isExpanded) {
            if (pt.x >= width - 145 && pt.x <= width - 21 && pt.y >= 53 && pt.y <= 79) isHand = true;
            if (pt.x >= 16 && pt.x <= width - 88 && pt.y >= 92 && pt.y <= 124) isIbeam = true;
            if (pt.x >= width - 80 && pt.x <= width - 16 && pt.y >= 92 && pt.y <= 124) isHand = true;

            // Copy Wi-Fi IP button
            if (pt.x >= width - 86 && pt.x <= width - 16 && pt.y >= 354 && pt.y <= 376) isHand = true;

            float taskY = 134.0f;
            std::lock_guard<std::mutex> lock(g_dataMutex);
            for (size_t i = 0; i < g_data.tasks.size() && taskY < 305.0f; ++i) {
                if (pt.x >= 16 && pt.x <= 40 && pt.y >= (int)taskY && pt.y <= (int)taskY + 24) isHand = true;
                if (pt.x >= width - 42 && pt.x <= width - 14 && pt.y >= (int)taskY && pt.y <= (int)taskY + 24) isHand = true;
                taskY += 26.0f;
            }
        }

        if (isHand) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        } else if (isIbeam) {
            SetCursor(LoadCursor(NULL, IDC_IBEAM));
            return TRUE;
        }

        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;

        if (y <= 40) {
            if (x >= width - 36 && x <= width - 8 && y >= 6 && y <= 34) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (x >= width - 64 && x <= width - 38 && y >= 6 && y <= 34) {
                SetWindowExpansion(!g_isExpanded);
                return 0;
            }
            if (x >= 14 && x <= 135 && y >= 6 && y <= 34) {
                ToggleToday();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }

        if (g_isExpanded) {
            // Day Status Button Click
            if (x >= width - 145 && x <= width - 21 && y >= 53 && y <= 79) {
                ToggleToday();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Input Box Focus
            if (x >= 16 && x <= width - 88 && y >= 92 && y <= 124) {
                g_inputFocused = true;
                g_cursorVisible = true;
                SetFocus(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            } else {
                g_inputFocused = false;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // "+ Add" Button Click
            if (x >= width - 80 && x <= width - 16 && y >= 92 && y <= 124) {
                if (!g_inputBuffer.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(g_dataMutex);
                        g_data.tasks.push_back({ GenerateId(), g_inputBuffer, false });
                        SaveData(g_data);
                    }
                    g_inputBuffer.clear();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            // Copy IP Link Click
            if (x >= width - 86 && x <= width - 16 && y >= 354 && y <= 376) {
                CopyIpToClipboard(hwnd);
                return 0;
            }

            // Task items list clicks
            float taskY = 134.0f;
            std::lock_guard<std::mutex> lock(g_dataMutex);
            for (size_t i = 0; i < g_data.tasks.size() && taskY < 305.0f; ++i) {
                if (x >= 16 && x <= 40 && y >= (int)taskY && y <= (int)taskY + 24) {
                    g_data.tasks[i].isCompleted = !g_data.tasks[i].isCompleted;

                    bool allDone = !g_data.tasks.empty();
                    for (const auto& t : g_data.tasks) {
                        if (!t.isCompleted) { allDone = false; break; }
                    }
                    if (allDone) {
                        std::wstring today = GetTodayString();
                        if (g_data.lastCompletedDate != today) {
                            if (g_data.currentStreak <= 0) {
                                g_data.currentStreak = 1;
                            }
                            g_data.lastCompletedDate = today;
                        }
                    }

                    SaveData(g_data);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                if (x >= width - 42 && x <= width - 14 && y >= (int)taskY && y <= (int)taskY + 24) {
                    g_data.tasks.erase(g_data.tasks.begin() + i);
                    SaveData(g_data);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }

                taskY += 26.0f;
            }
        }
        return 0;
    }

    case WM_CHAR: {
        if (!g_isExpanded || !g_inputFocused) return 0;

        wchar_t ch = (wchar_t)wParam;
        if (ch == VK_BACK) {
            if (!g_inputBuffer.empty()) {
                g_inputBuffer.pop_back();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (ch == VK_RETURN) {
            if (!g_inputBuffer.empty()) {
                {
                    std::lock_guard<std::mutex> lock(g_dataMutex);
                    g_data.tasks.push_back({ GenerateId(), g_inputBuffer, false });
                    SaveData(g_data);
                }
                g_inputBuffer.clear();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (ch >= 32) {
            g_inputBuffer += ch;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Entry Point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();

    // Winsock Init
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Detect LAN IP
    DetectLocalIp();

    // Start background Wi-Fi sync server
    StartSyncServer();

    // GDI+ Init
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    g_data = LoadData();
    CheckRollover();

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"StreakBarNative";

    RegisterClassEx(&wc);

    int width = WIN_WIDTH;
    int height = WIN_HEIGHT_COLLAPSED;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.left + (workArea.right - workArea.left - width) / 2;
    int y = workArea.top;

    g_hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"StreakBarNative",
        L"StreakBar",
        WS_POPUP | WS_VISIBLE,
        x, y, width, height,
        NULL, NULL, hInstance, NULL);

    UpdateAppRegion(width, height);

    SetTimer(g_hwnd, 1, 30000, NULL);
    SetTimer(g_hwnd, 2, 500, NULL);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    WSACleanup();
    return (int)msg.wParam;
}
