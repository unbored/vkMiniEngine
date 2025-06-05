#include "Utility.h"
#include <locale>

std::string Utility::ToLower(const std::string &str)
{
    std::string lower_case = str;
    std::locale loc;
    for (char &s : lower_case)
        s = std::tolower(s, loc);
    return lower_case;
}

std::string Utility::GetBasePath(const std::string &filePath)
{
    size_t lastSlash;
    if ((lastSlash = filePath.rfind('/')) != std::string::npos)
        return filePath.substr(0, lastSlash + 1);
    else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
        return filePath.substr(0, lastSlash + 1);
    else
        return "";
}

std::string Utility::RemoveBasePath(const std::string &filePath)
{
    size_t lastSlash;
    if ((lastSlash = filePath.rfind('/')) != std::string::npos)
        return filePath.substr(lastSlash + 1, std::string::npos);
    else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
        return filePath.substr(lastSlash + 1, std::string::npos);
    else
        return filePath;
}

std::string Utility::GetFileExtension(const std::string &filePath)
{
    std::string fileName = RemoveBasePath(filePath);
    size_t extOffset = fileName.rfind('.');
    if (extOffset == std::wstring::npos)
        return "";

    return fileName.substr(extOffset + 1);
}

std::string Utility::RemoveExtension(const std::string &filePath) { return filePath.substr(0, filePath.rfind(".")); }
