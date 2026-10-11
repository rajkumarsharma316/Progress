# 🔥 StreakBar

> A lightweight, zero-framework, distraction-free daily streak and task utility pinned to your screen. Cross-platform support for **Windows** (pure Win32/GDI+), **Android** (floating notch overlay), and **Linux** (pure X11/Cairo).

---

## 🌟 Features

- **🔥 Daily Streak Counter**: Pinned to the top of your screen as a sleek notch bar.
- **✓ Rock-Solid Streak Logic**:
  - Toggling on the current day switches cleanly between **Pending (`DAY X`)** and **Completed (`DAY X ✓`)**.
  - Count advances strictly upon midnight calendar rollover when yesterday was completed.
  - Automatically resets to Day 1 if a day is broken.
- **📋 Daily Task Checklist**:
  - Live progress mini-boxes and task counter (`X/Y ✓`).
  - Expandable drawer to add, complete, and delete tasks.
  - Daily rotating motivational quotes.
- **⚡ Zero Bloat, Ultra Lightweight**:
  - **No Electron, no WebViews, no heavy runtime**.
  - Windows: ~1 MB standalone native executable using pure Win32 & GDI+.
  - Linux: Native X11 & Cairo with true composite transparency.
  - Android: Native Kotlin floating window service with system notch integration.
- **📡 Local Wi-Fi Sync (Port 54321)**:
  - Synchronize streak counts between your PC and Android phone over your local network with zero external servers, clouds, or logins.

---

## 📁 Repository Structure

```
StreakBar/
├── windows/
│   └── StreakBar.Cpp/
│       ├── main.cpp          # Pure Win32 + GDI+ C++ application & embedded sync server
│       └── build.bat         # 1-click compilation script (MinGW g++)
├── android/
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/example/streakbar/
│   │   │   │   ├── MainActivity.kt      # Main settings & IP pairing activity
│   │   │   │   ├── OverlayService.kt    # Floating notch overlay service
│   │   │   │   ├── StreakRepository.kt  # Local data persistence & rollover logic
│   │   │   │   └── BootReceiver.kt      # Boot completion auto-start
│   │   │   ├── res/                     # Layouts, vector drawables & themes
│   │   │   └── AndroidManifest.xml
│   │   └── build.gradle.kts
│   └── build.gradle.kts
├── linux/
│   ├── main.cpp              # Pure X11 + Cairo C++ implementation
│   └── Makefile              # Linux build script
├── .gitignore
└── README.md
```

---

## 🚀 Building & Running

### 1. Windows (Pure C++ / Win32)

#### Prerequisites:
- **MinGW-w64** (`g++`) or Visual Studio C++ compiler.

#### Compile & Run:
Open PowerShell or Command Prompt inside `windows/StreakBar.Cpp/`:

```powershell
cd "windows\StreakBar.Cpp"
.\build.bat
```

Or compile directly with `g++`:
```cmd
g++ -O3 -s -mwindows -std=c++17 main.cpp -o StreakBar.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -lws2_32 -static -static-libgcc -static-libstdc++
.\StreakBar.exe
```

Data is stored locally at `%LOCALAPPDATA%\StreakBar\streak.json`.

---

### 2. Android (Floating Notch Overlay)

#### Prerequisites:
- Android Studio Ladybug (or newer)
- Android SDK 34+ (Android 8.0 - 15)

#### Steps:
1. Open the `android` folder in **Android Studio**.
2. Allow Gradle sync to complete.
3. Connect your Android device via USB (or wireless debugging) with **Developer Options & USB Debugging** enabled.
4. Click **Run (`Shift + F10`)**.
5. Grant the **Display over other apps** (Overlay) permission when prompted.
6. Enter your PC's IP address (shown in the Windows app drawer) and tap **Link & Sync Now** to sync streak count.

---

### 3. Linux (Pure X11 / Cairo)

#### Prerequisites:
```bash
sudo apt-get install build-essential libx11-dev libcairo2-dev
```

#### Compile & Run:
```bash
cd linux
make
./streakbar
```

---

## 📡 Wi-Fi Sync Guide

1. Ensure your PC and Android phone are connected to the **same Wi-Fi network**.
2. On Windows, click the expand arrow (`▼`) on the StreakBar to view your local IP (e.g., `192.168.1.5`).
3. Click **Copy IP** to copy it to clipboard.
4. Open the StreakBar app on Android, paste the IP into the **PC Wi-Fi IP** field, and tap **Link & Sync Now**.

---

## 🐙 How to Create a GitHub Repository and Push from Terminal

Follow these terminal steps right from your workspace root (`StreakBar`):

### Step 1: Initialize Git and Commit Your Files
```bash
# 1. Ensure you are in the project root
cd "c:\Users\Victus\Documents\New project\StreakBar"

# 2. Stage all project files (respecting .gitignore)
git add .

# 3. Create your initial commit
git commit -m "Initial commit: StreakBar cross-platform utility (Windows, Android, Linux)"

# 4. Set default branch to main
git branch -M main
```

---

### Step 2: Push to GitHub

#### Method A: Using GitHub CLI (`gh`) (Fastest & Automatic)
If you have GitHub CLI installed:
```bash
# Log in if you haven't already
gh auth login

# Create a public or private repository and push automatically
gh repo create StreakBar --public --source=. --remote=origin --push
```

#### Method B: Using Standard Git & Web Browser
1. Go to [github.com/new](https://github.com/new).
2. Enter repository name: `StreakBar`.
3. Choose **Public** or **Private**.
4. **Do NOT** check "Add a README file" or ".gitignore" (we already have them).
5. Click **Create repository**.
6. Copy your repository URL, then run in your terminal:

```bash
# Replace YOUR_USERNAME with your GitHub username
git remote add origin https://github.com/YOUR_USERNAME/StreakBar.git

# Push code to GitHub
git push -u origin main
```

---

## 📄 License
This project is open-source under the MIT License.
