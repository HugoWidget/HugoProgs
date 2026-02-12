//Copyright 2025 HWorldY
//the Apache License, Version 2.0
//Author: HWorldY
#ifndef WPATH_H
#define WPATH_H
#include <windows.h>
#include<string>
using namespace std;
#define QString wstring
class WPath
{
public:
    WPath();
    static QString transPath(QString cur,QString des);
    static QString getModulePath();
    static QString getModuleFolder();
    static void ShellExe(QString lpFile,QString lpOperation=L"open",QString lpParameters=L"",QString lpDirectory=L"default");
    QString getModulePath(QString moduleName);
    QString getModuleFolder(QString moduleName);
    static QString splitPath(QString path);
private:
    QString moduleName=L"";
};

#endif // WPATH_H
