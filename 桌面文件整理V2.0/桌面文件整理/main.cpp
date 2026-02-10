#include <iostream>
#include <string>
#include <filesystem> 
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <queue>
#include <chrono>
// 引入必要的库，用于创建快捷方式
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace fs = std::filesystem;
using namespace std;
namespace fs = std::filesystem;

// 全局变量
wstring desktopdir;
struct files {
    wstring filename;
    wstring filepath;
    int filetype; // 1=文件夹, 0=普通文件
};
queue<files> f;

#pragma comment(lib, "advapi32.lib") 

// ==========================================
// 窗口控制与权限工具
// ==========================================

// 隐藏窗口（只隐藏，不释放资源，保证 cmd 命令能运行）
void HideConsole() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

// 判断是否以管理员运行
bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdminSID = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSID)) {
        if (!CheckTokenMembership(NULL, pAdminSID, &fIsRunAsAdmin)) {
            fIsRunAsAdmin = FALSE;
        }
        FreeSid(pAdminSID);
    }
    return fIsRunAsAdmin;
}

// 提权函数
void RelaunchAsAdmin(HWND hwnd) {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = hwnd;
        sei.nShow = SW_HIDE; // 提权后的窗口也尽量隐藏
        if (!ShellExecuteExW(&sei)) {
            // 用户取消了 UAC，直接退出
            exit(0);
        }
        // 提权成功后，当前旧进程退出
        exit(0);
    }
}

// ==========================================
// 计划任务管理 
// ==========================================

// 检查计划任务是否存在
bool IsTaskExist(const std::wstring& taskName) {
    // 构造命令：schtasks /query /tn "任务名"
    // 使用 >nul 2>nul 屏蔽输出
    std::wstring cmd = L"schtasks /query /tn \"" + taskName + L"\" >nul 2>nul";

    // 如果 FreeConsole 被调用，这里会失败。现在修正后应该正常了。
    int result = _wsystem(cmd.c_str());

    // 返回 0 代表执行成功（任务存在）
    return (result == 0);
}

// 设置开机自启（同步执行，确保创建成功）
void SetAutoStart() {
    wchar_t szPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szPath, MAX_PATH)) return;
    std::wstring currentPath(szPath);

    // 构造 schtasks 命令
    // /sc onlogon : 用户登录时触发
    // /rl highest : 最高权限 (绕过UAC)
    // /f : 强制覆盖
    // 注意：路径必须用 \" 包裹，防止路径中有空格导致命令失败
    std::wstring params = L"/create /tn \"DesktopHelper\" /tr \"\\\"" + currentPath + L"\\\"\" /sc onlogon /rl highest /f";

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = L"schtasks.exe"; // 直接调用 exe
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE; // 隐藏创建任务的过程
    sei.fMask = SEE_MASK_NOCLOSEPROCESS; // 为了等待执行结束

    if (ShellExecuteExW(&sei)) {
        // 等待 schtasks 执行完毕，确保任务创建成功后再继续
        WaitForSingleObject(sei.hProcess, 5000);
        CloseHandle(sei.hProcess);
    }
}

// ==========================================
// 文件处理
// ==========================================

wstring GetWindowsDesktopPath() {
    PWSTR path = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &path);
    if (SUCCEEDED(hr)) {
        wstring result(path);
        CoTaskMemFree(path);
        return result;
    }
    return L"";
}

fs::path _GetUniquePath(const fs::path& targetPath) {
    if (!fs::exists(targetPath)) return targetPath;
    fs::path parent = targetPath.parent_path();
    wstring stem = targetPath.stem().wstring();
    wstring ext = targetPath.extension().wstring();
    fs::path newPath;
    int i = 1;
    while (true) {
        wstring newName = stem + L"(" + to_wstring(i) + L")" + ext;
        newPath = parent / newName;
        if (!fs::exists(newPath)) break;
        i++;
    }
    return newPath;
}

bool MoveItemSafe(const wstring& source, const wstring& target, bool isFolder) {
    fs::path srcPath(source);
    fs::path tgtPath(target);
    if (!fs::exists(srcPath)) return false;
    fs::path finalPath = _GetUniquePath(tgtPath);
    try { fs::create_directories(finalPath.parent_path()); }
    catch (...) {}

    // 移动文件
    return MoveFileExW(srcPath.c_str(), finalPath.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
}

void traveldir(const wstring& pathStr) {
    fs::path path(pathStr);
    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                files tmp;
                tmp.filepath = entry.path().wstring();
                tmp.filename = entry.path().filename().wstring();
                if (entry.is_directory()) tmp.filetype = 1;
                else if (entry.is_regular_file()) tmp.filetype = 0;
                else continue;
                f.push(tmp);
            }
        }
    }
    catch (...) {}
}

void transfile() {
    if (desktopdir.empty()) return;
    fs::path baseDir(desktopdir);
    fs::path helperDir = baseDir / L"桌面文件整理";
    fs::path folderDir = baseDir / L"文件夹";
    try {
        fs::create_directories(helperDir);
        fs::create_directories(folderDir);
    }
    catch (...) {}
    bool createlnk = 1;
    while (!f.empty()) {
        files tmp = f.front();
        f.pop();
        if (tmp.filename == L"桌面文件整理.lnk") createlnk = 0;
        if (tmp.filename == L"桌面文件整理" || tmp.filename == L"文件夹" || tmp.filename == L"desktop.ini") continue;

        fs::path srcPath(tmp.filepath);
        if (tmp.filetype == 1) {
            fs::path target = folderDir / tmp.filename;
            MoveItemSafe(tmp.filepath, target.wstring(), true);
        } else {
            wstring ext = srcPath.extension().wstring();
            if (ext.empty()) ext = L"无后缀文件";
            if (ext == L".lnk" || ext == L".url") continue;
            fs::path targetFolder = helperDir / ext;
            fs::path targetFile = targetFolder / tmp.filename;
            MoveItemSafe(tmp.filepath, targetFile.wstring(), false);
        }
    }
    if (createlnk) {

    }
}

// 创建快捷方式的函数
bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& workDir) {
    HRESULT hres;
    IShellLink* psl;

    // 初始化 COM 库
    CoInitialize(NULL);

    // 获取 IShellLink 接口
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // 设置目标路径
        psl->SetPath(targetPath.c_str());
        // 设置工作目录（通常设为程序所在目录）
        psl->SetWorkingDirectory(workDir.c_str());
        // 设置描述
        psl->SetDescription(L"桌面文件整理工具");

        // 获取 IPersistFile 接口用于保存
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres)) {
            // 保存快捷方式 (.lnk)
            hres = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize();
    return SUCCEEDED(hres);
}

void AutoInstall() {
    // 1. 获取当前程序完整路径
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    fs::path currentPath(buffer);

    // 2. 获取桌面路径
    if (SHGetSpecialFolderPathW(NULL, buffer, CSIDL_DESKTOPDIRECTORY, FALSE)) {
        fs::path desktopPath(buffer);

        // 3. 定义目标目录和目标EXE路径
        // 目标目录: 桌面\桌面文件整理\.exe
        fs::path targetDir = desktopPath / L"桌面文件整理" / L".exe";
        // 目标EXE: 桌面\桌面文件整理\.exe\卓面文件整理.exe
        fs::path targetExe = targetDir / L"桌面文件整理.exe";
        // 快捷方式: 桌面\卓面文件整理.lnk
        fs::path shortcutPath = desktopPath / L"桌面文件整理.lnk";

        // 4. 判断当前是否已经在正确的位置
        // fs::equivalent 比较两个路径是否指向同一个文件
        bool isAtTarget = false;
        try {
            if (fs::exists(targetExe) && fs::equivalent(currentPath, targetExe)) {
                isAtTarget = true;
            }
        }
        catch (...) {
            // 忽略比较错误
        }

        // --- 情况 A: 程序不在目标位置 ---
        if (!isAtTarget) {
            try {
                // 创建目录 (如果不存在)
                fs::create_directories(targetDir);

                // 复制自身到目标位置 (覆盖已存在的)
                fs::copy_file(currentPath, targetExe, fs::copy_options::overwrite_existing);

                // 创建快捷方式
                CreateShortcut(targetExe.wstring(), shortcutPath.wstring(), targetDir.wstring());

                // 启动目标位置的新程序
                ShellExecuteW(NULL, L"open", targetExe.c_str(), NULL, targetDir.c_str(), SW_SHOW);

                // 退出当前程序 (旧的程序)
                exit(0);
            }
            catch (const std::exception& e) {
                MessageBoxA(NULL, e.what(), "安装失败", MB_ICONERROR);
            }
        }
        // --- 情况 B: 程序已经在目标位置 ---
        else {
            // 检查桌面上有没有快捷方式，没有就补一个
            if (!fs::exists(shortcutPath)) {
                CreateShortcut(targetExe.wstring(), shortcutPath.wstring(), targetDir.wstring());
            }
        }
    }
}

// ==========================================
// 主函数
// ==========================================
int main() {


    HideConsole();
    //如果不在规定的位置那么就放到规定位置 创建快捷方式
    AutoInstall();

    const std::wstring TaskName = L"DesktopHelper";

    //  权限判断
    if (IsRunAsAdmin()) {
        // 管理员模式
        // 场景：1. 第一次提权运行  2. 开机自动运行 最高权限
        // 无论哪种情况，都强制更新一下计划任务，防止路径变动或任务被删
        SetAutoStart();
        // 执行清理
        desktopdir = GetWindowsDesktopPath();
        if (!desktopdir.empty()) {
            traveldir(desktopdir);
            transfile();
        }
    } else {
        // 普通用户模式
        // 场景：用户双击图标
        if (IsTaskExist(TaskName)) {
            // 任务已存在 -> 说明以前授权过 -> 静默运行
            // 普通权限足够移动用户桌面的文件
            desktopdir = GetWindowsDesktopPath();
            if (!desktopdir.empty()) {
                traveldir(desktopdir);
                transfile();
            }
        } else {
            // 任务不存在 -> 说明是第一次运行 -> 申请提权
            // 只有这里会弹 UAC
            RelaunchAsAdmin(NULL);
            return 0;
        }
    }
    return 0;
}