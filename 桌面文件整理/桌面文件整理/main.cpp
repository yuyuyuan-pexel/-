// 桌面文件助手.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <filesystem>
#include<string>
#include <fstream>
#include <ctime>   
#include<atlstr.h>
#include <sys/types.h>    
#include <sys/stat.h> 
#include <io.h>
#include <direct.h>
#include <string>
#include<Windows.h>
using namespace std;
bool ismulu(const char* path) {//判断文件是否为目录，是=1
    struct stat s;
    if (stat(path, &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            return 1;
        }
        else if (s.st_mode & S_IFREG) {
            return 0;
        }
    }
}
void createroot(const std::filesystem::path& root) {
    string gen = root.string() + "\\桌面文件整理";
    if (_access(gen.c_str(), 0) == -1)	//如果文件夹不存在
        _mkdir(gen.c_str());				//则创建
}
bool move_file(std::string old_path_and_file_name, std::string new_path)
{
    std::filesystem::path file_old_path(old_path_and_file_name);
    string file_name = file_old_path.filename().string();

    std::string resule_path = new_path + file_name;

    std::error_code ec;
    std::filesystem::rename(old_path_and_file_name, resule_path, ec);

    if (ec)
    {
        return false;
    }
    else
    {
        return true;
    }

}
string& trim(string& s)//去除string首尾空格
{
    if (s.empty())
    {
        return s;
    }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}
std::filesystem::path debugg(std::filesystem::path root) {
    return root;
}
void traverseDir(const std::filesystem::path& root, int depth = 0) {
    try {
        if (std::filesystem::is_directory(root) && std::filesystem::exists(debugg(root))) {
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                string a = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         ";
                a = root.string() + "\\" + entry.path().filename().string();

                if (!ismulu(a.c_str())) {//是文件获取后缀名
                    a = entry.path().filename().string();
                    if (a == "窗口置顶.exe" || a == "桌面文件助手.exe") {
                        continue;
                    }
                    int wei = a.length();
                    int i = a.length() - 1;
                    for (i; i >= 0; i--) {
                        if (a[i] == '.') break;
                    }
                    string houzhui = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   ";
                    for (int j = i; j < wei; j++) {
                        houzhui[j - i] = a[j];
                    }
                    trim(houzhui);
                    if (houzhui == ".lnk" || houzhui == ".url") continue;
                    createroot(root);
                    a = root.string() + "\\" + entry.path().filename().string();
                    ifstream source(a.c_str(), std::ios::binary);
                    string mubiao = root.string() + "\\桌面文件整理\\" + houzhui + "\\";
                    if (_access(mubiao.c_str(), 0) == -1)	//如果文件夹不存在
                        _mkdir(mubiao.c_str());				//则创建
                    ofstream dest(mubiao.c_str(), std::ios::binary);
                    dest << source.rdbuf();
                    source.close();
                    dest.close();
                    wstring temp = wstring(a.begin(), a.end());
                    LPCWSTR a1 = temp.c_str();
                    temp = wstring(mubiao.begin(), mubiao.end());
                    LPCWSTR mubiao1 = temp.c_str();
                    cout << move_file(a, mubiao);
                    cout << a << '\n';
                }
            }
        }
        else {
            std::cout << "root is not directory or exists " << std::endl;
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "exception:" << ex.what() << std::endl;
    }
}
void test_filesystem() {
    std::filesystem::path root("./");
    traverseDir(root);
}
std::string ConvertLPWSTRToUTF8(LPWSTR lpwstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, lpwstr, -1, NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, lpwstr, -1, &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
int main() {
    // ShowWindow(GetConsoleWindow(), SW_HIDE);
    // wchar_t myString[] = L"                                                                                                                                                        "; // 定义一个宽字符数组
    // LPWSTR UserName = myString; // 将宽字符数组赋值给LPWSTR变量
   //  unsigned long size = 255;
    // GetUserName(UserName, &size);//GetUserName()函数同上
    // string t = ConvertLPWSTRToUTF8(UserName);
    // trim(t);
    // string ccc = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               ";
   //  ccc = "C:\\Users\\" + t + "\\Desktop";
    traverseDir("D:\\Users\\36138\\Desktop");
    // traverseDir("C:\\Users\\Public\\Desktop");
}
