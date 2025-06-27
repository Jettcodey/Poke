#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

#ifndef PROJECT_AUTHOR
#define PROJECT_AUTHOR "Jettcodey"
#endif
#ifndef PROJECT_COPYRIGHT
#define PROJECT_COPYRIGHT "(C) Copyright 2025 Jettcodey. All rights reserved."
#endif
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

void PrintApiError(const std::string& context, const std::string& filename = "", DWORD errorCode = 0) {
    if (errorCode == 0) {
        errorCode = GetLastError();
    }

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::cerr << "Error: " << context;
    if (!filename.empty()) {
        std::cerr << " for file '" << filename << "'";
    }
    std::cerr << " (Code: " << errorCode << ").";

    if (messageBuffer && size > 0) {
        while (size > 0 && (messageBuffer[size - 1] == '\n' || messageBuffer[size - 1] == '\r')) {
            messageBuffer[--size] = '\0';
        }
        std::cerr << " " << messageBuffer;
        LocalFree(messageBuffer);
    } else {
        std::cerr << " Windows error message could not be retrieved.";
    }
    std::cerr << std::endl;
}

void PrintError(const std::string& message, const std::string& details = "") {
    std::cerr << "Error: " << message;
    if (!details.empty()) {
        std::cerr << ": " << details;
    }
    std::cerr << std::endl;
}

void PrintVersionInfo() {
    std::cout << "poke Version " << PROJECT_VERSION << "\n"
              << PROJECT_COPYRIGHT << "\n";
}

void PrintUsage() {
    std::cerr <<
        "Usage:\n"
        "   poke [OPTIONS] file [...]\n\n"
        "Options:\n"
        "   -h            Mark file as hidden\n"
        "   -v            Mark file as visible (remove hidden attribute)\n"
        "   -o            Mark file as read-only\n"
        "   -w            Mark file as writable (remove read-only attribute)\n"
        "   -a            Update access time only\n"
        "   -m            Update modification time only\n"
        "   -t STAMP      Use [[CC]YY]MMDDhhmm[.ss] timestamp (local time)\n"
        "   -d DATESTR    Use date string (local time) in one of the following formats:\n"
        "                 'DD-MM-YYYY hh:mm:ss', 'DD-MM-YYYY hh:mm', 'DD-MM-YYYY hh:mm:ss AM/PM', 'DD-MM-YYYY hh:mm AM/PM',\n"
        "                 'DD-MM-YY hh:mm:ss', 'DD-MM-YY hh:mm', 'DD-MM-YY hh:mm:ss AM/PM', 'DD-MM-YY hh:mm AM/PM',\n"
        "                 'DD-MM hh:mm:ss', 'DD-MM hh:mm', 'DD-MM hh:mm:ss AM/PM', 'DD-MM hh:mm' (current year assumed)\n"
        "   -r REF_FILE   Use timestamps from Reference file\n"
        "   -c            Do not create file if it does not exist\n"
        "   --version     Show version information and exit\n"
        "   --help        Show this usage information and exit\n\n"
        "Notes:\n"
        "   - If neither -a nor -m is given, both times are updated.\n"
        "   - If -t, -d, or -r is not given, the current time is used.\n"
        "   - Options -t, -d, and -r are mutually exclusive.\n";
}

// Parses "[[CC]YY]MMDDhhmm[.ss]" (local time) into a UTC SYSTEMTIME.
bool ParseTouchTimestamp(const std::string& ts, SYSTEMTIME& out) {
    std::string s = ts;
    int sec = 0;

    auto dot_pos = s.find('.');
    if (dot_pos != std::string::npos) {
        std::string sec_str = s.substr(dot_pos + 1);
        s = s.substr(0, dot_pos);
        try {
            if (sec_str.length() > 2) sec_str = sec_str.substr(0, 2);
            sec = std::stoi(sec_str);
            if (sec < 0 || sec > 59) throw std::out_of_range("Seconds");
        } catch (const std::exception&) {
            PrintError("Invalid seconds component in -t timestamp", sec_str);
            return false;
        }
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    try {
        if (s.length() == 12) { // CCYYMMDDhhmm
            year = std::stoi(s.substr(0, 4));
            month = std::stoi(s.substr(4, 2)); day = std::stoi(s.substr(6, 2));
            hour = std::stoi(s.substr(8, 2)); minute = std::stoi(s.substr(10, 2));
        } else if (s.length() == 10) { // YYMMDDhhmm
            int yy = std::stoi(s.substr(0, 2));
            year = (yy >= 69) ? (1900 + yy) : (2000 + yy); // POSIX 2-digit year convention
            month = std::stoi(s.substr(2, 2)); day = std::stoi(s.substr(4, 2));
            hour = std::stoi(s.substr(6, 2)); minute = std::stoi(s.substr(8, 2));
        } else if (s.length() == 8) { // MMDDhhmm
            SYSTEMTIME currentLocalTime;
            GetLocalTime(&currentLocalTime);
            year = currentLocalTime.wYear;
            month = std::stoi(s.substr(0, 2)); day = std::stoi(s.substr(2, 2));
            hour = std::stoi(s.substr(4, 2)); minute = std::stoi(s.substr(6, 2));
        } else {
            PrintError("Invalid length for -t timestamp string", s);
            return false;
        }
    } catch (const std::exception&) {
        PrintError("Non-numeric or out-of-range component in -t timestamp", s);
        return false;
    }

    std::tm tm = {};
    tm.tm_year = year - 1900; tm.tm_mon = month - 1; tm.tm_mday = day;
    tm.tm_hour = hour; tm.tm_min = minute; tm.tm_sec = sec;
    tm.tm_isdst = -1;

    time_t time_val = mktime(&tm);
    if (time_val == -1) {
        PrintError("Invalid date/time components in -t timestamp (e.g., Feb 30)", ts);
        return false;
    }

    struct tm utc_tm = {};
    if (gmtime_s(&utc_tm, &time_val) != 0) {
        PrintError("Failed to convert local time to UTC structure.", std::to_string(errno));
        return false;
    }

    out.wYear = utc_tm.tm_year + 1900;
    out.wMonth = utc_tm.tm_mon + 1;
    out.wDay = utc_tm.tm_mday;
    out.wHour = utc_tm.tm_hour;
    out.wMinute = utc_tm.tm_min;
    out.wSecond = utc_tm.tm_sec;
    out.wMilliseconds = 0;

    return true;
}

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

bool IsValidDay(int year, int month, int day) {
    if (month < 1 || month > 12 || day < 1) return false;
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (IsLeapYear(year)) {
        days_in_month[2] = 29;
    }
    return day <= days_in_month[month];
}

bool ParseDateTimeString(const std::string& dts, SYSTEMTIME& out) {
    std::tm tm_local = {};
    size_t date_time_separator_pos = dts.find_last_of(' ');
    if (date_time_separator_pos == std::string::npos) {
        PrintError("Invalid date string format: Missing space separator between date and time.", dts);
        return false;
    }

    std::string date_str = dts.substr(0, date_time_separator_pos);
    std::string time_str = dts.substr(date_time_separator_pos + 1);

    // Time Parsing
    bool time_parsed = false;
    std::tm temp_tm_time = {};

    std::transform(time_str.begin(), time_str.end(), time_str.begin(), ::toupper);
    
    const std::vector<std::string> time_formats = {
        "%H:%M:%S",
        "%H:%M",
        "%I:%M:%S %p",
        "%I:%M %p"
    };

    for (const auto& format : time_formats) {
        std::istringstream ss_time(time_str);
        temp_tm_time = {};
        ss_time >> std::get_time(&temp_tm_time, format.c_str());
        if (!ss_time.fail() && ss_time.eof()) {
            tm_local.tm_hour = temp_tm_time.tm_hour;
            tm_local.tm_min = temp_tm_time.tm_min;
            tm_local.tm_sec = temp_tm_time.tm_sec;
            time_parsed = true;
            break;
        }
    }

    if (!time_parsed) {
        PrintError("Invalid time format in -d date string", dts);
        return false;
    }

    // Date Parsing (DD-MM-YYYY or DD-MM-YY only)
    int p1 = 0, p2 = 0, p3 = 0;
    int year = 0, month = 0, day = 0;
    bool date_parsed = false;

    char delimiter = 0;
    if (date_str.find('-') != std::string::npos) delimiter = '-';
    else if (date_str.find('/') != std::string::npos) delimiter = '/';

    if (delimiter != 0) {
        std::string s1, s2, s3;
        std::istringstream ss_date(date_str);
        if (std::getline(ss_date, s1, delimiter) &&
            std::getline(ss_date, s2, delimiter) &&
            std::getline(ss_date, s3, delimiter)) {
            
            try {
                p1 = std::stoi(s1);
                p2 = std::stoi(s2);
                p3 = std::stoi(s3);
            } catch (const std::exception&) {
                PrintError("Non-numeric date component in -d date string", date_str);
                return false;
            }
            
            if (s3.length() == 4 && p3 >= 1 && p3 <= 9999 && IsValidDay(p3, p2, p1)) { // DD-MM-YYYY
                year = p3; month = p2; day = p1; date_parsed = true;
            } else if (s3.length() == 2 && p3 >= 0 && p3 <= 99) { // DD-MM-YY
                int temp_year_yy = (p3 >= 69) ? (1900 + p3) : (2000 + p3);
                if (IsValidDay(temp_year_yy, p2, p1)) {
                    year = temp_year_yy; month = p2; day = p1; date_parsed = true;
                }
            }
        } else { // DD-MM (current year assumed)
            ss_date.clear();
            ss_date.seekg(0);
            if (std::getline(ss_date, s1, delimiter) && std::getline(ss_date, s2, delimiter)) {
                try {
                    p1 = std::stoi(s1);
                    p2 = std::stoi(s2);
                } catch (const std::exception&) {
                    PrintError("Non-numeric date component in -d date string", date_str);
                    return false;
                }
                SYSTEMTIME currentLocalTime;
                GetLocalTime(&currentLocalTime);
                year = currentLocalTime.wYear;

                if (IsValidDay(year, p2, p1)) {
                    month = p2; day = p1; date_parsed = true;
                }
            }
        }
    } else { // Space separated or only DDMM
        std::istringstream ss_date_space(date_str);
        if (ss_date_space >> p1 >> p2 >> p3) { // DD MM YYYY/YY
            if (std::to_string(p3).length() == 4 && p3 >= 1 && p3 <= 9999 && IsValidDay(p3, p2, p1)) { // DD MM YYYY
                year = p3; month = p2; day = p1; date_parsed = true;
            } else if (std::to_string(p3).length() == 2 && p3 >= 0 && p3 <= 99) { // DD MM YY
                int temp_year_yy = (p3 >= 69) ? (1900 + p3) : (2000 + p3);
                if (IsValidDay(temp_year_yy, p2, p1)) {
                    year = temp_year_yy; month = p2; day = p1; date_parsed = true;
                }
            }
        } else { // DD MM (current year assumed)
            ss_date_space.clear();
            ss_date_space.seekg(0);
            if (ss_date_space >> p1 >> p2) {
                SYSTEMTIME currentLocalTime;
                GetLocalTime(&currentLocalTime);
                year = currentLocalTime.wYear;

                if (IsValidDay(year, p2, p1)) {
                    month = p2; day = p1; date_parsed = true;
                }
            }
        }
    }

    if (!date_parsed) {
        PrintError("Invalid or unsupported date format in -d date string. Only 'DD-MM-YYYY/YY' or 'DD-MM' formats (with '-' or '/' or ' ' as delimiter) are supported for date part.", dts);
        return false;
    }
    
    tm_local.tm_year = year - 1900;
    tm_local.tm_mon = month - 1;
    tm_local.tm_mday = day;
    tm_local.tm_isdst = -1;

    time_t time_val = mktime(&tm_local);
    if (time_val == -1) {
        PrintError("Invalid date/time components in -d string (mktime failed, check date validity like Feb 30)", dts);
        return false;
    }
    
    struct tm utc_tm = {};
    if (gmtime_s(&utc_tm, &time_val) != 0) {
        PrintError("Failed converting -d date to UTC structure", dts);
        return false;
    }

    out.wYear = utc_tm.tm_year + 1900;
    out.wMonth = utc_tm.tm_mon + 1;
    out.wDay = utc_tm.tm_mday;
    out.wHour = utc_tm.tm_hour;
    out.wMinute = utc_tm.tm_min;
    out.wSecond = utc_tm.tm_sec;
    out.wMilliseconds = 0;
    return true;
}

bool GetTimesFromReferenceFile(const std::string& path, FILETIME& outAccess, FILETIME& outModify) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        PrintApiError("Opening reference file", path);
        return false;
    }

    FILETIME ftCreate;
    bool ok = GetFileTime(hFile, &ftCreate, &outAccess, &outModify);
    if (!ok) {
        PrintApiError("Reading times from reference file", path);
    }

    CloseHandle(hFile);
    return ok;
}

bool UpdateFileAttributes(const std::string& path, const std::unordered_set<std::string>& flags) {
    DWORD currentAttrs = GetFileAttributesA(path.c_str());
    if (currentAttrs == INVALID_FILE_ATTRIBUTES) {
        PrintApiError("Getting file attributes", path);
        return false;
    }

    DWORD newAttrs = currentAttrs;
    bool changed = false;

    if (flags.count("-h")) { newAttrs |= FILE_ATTRIBUTE_HIDDEN; }
    if (flags.count("-v")) { newAttrs &= ~FILE_ATTRIBUTE_HIDDEN; }
    if (flags.count("-o")) { newAttrs |= FILE_ATTRIBUTE_READONLY; }
    if (flags.count("-w")) { newAttrs &= ~FILE_ATTRIBUTE_READONLY; }

    changed = (newAttrs != currentAttrs);

    if (changed) {
        if (!SetFileAttributesA(path.c_str(), newAttrs)) {
            PrintApiError("Setting file attributes", path);
            return false;
        }
    }
    return true;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::unordered_set<std::string> flags;
    std::string refFile, tStamp, dStamp;
    std::vector<std::string> targetFiles;
    bool show_help = false;
    bool show_version = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "/?") { show_help = true; }
        else if (arg == "--version") { show_version = true; }
        else if (arg == "-r") {
            if (!refFile.empty()) { PrintError("Cannot specify -r multiple times."); return 1; }
            if (flags.count("-t") || flags.count("-d")) { PrintError("Cannot use -r with -t or -d."); return 1; }
            if (++i < argc) { refFile = argv[i]; flags.insert("-r"); } 
            else { PrintError("Option -r requires a reference file argument."); return 1; }
        } else if (arg == "-t") {
            if (!tStamp.empty()) { PrintError("Cannot specify -t multiple times."); return 1; }
            if (flags.count("-r") || flags.count("-d")) { PrintError("Cannot use -t with -r or -d."); return 1; }
            if (++i < argc) { tStamp = argv[i]; flags.insert("-t"); } 
            else { PrintError("Option -t requires a timestamp argument."); return 1; }
        } else if (arg == "-d") {
            if (!dStamp.empty()) { PrintError("Cannot specify -d multiple times."); return 1; }
            if (flags.count("-r") || flags.count("-t")) { PrintError("Cannot use -d with -r or -t."); return 1; }
            if (++i < argc) { dStamp = argv[i]; flags.insert("-d"); } 
            else { PrintError("Option -d requires a date string argument."); return 1; }
        } else if (arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                std::string flag = std::string("-") + arg[j];
                if (flag == "-a" || flag == "-m" || flag == "-c" || flag == "-h" || flag == "-v" || flag == "-o" || flag == "-w") {
                    flags.insert(flag);
                } else {
                    PrintError("Unknown option specified", flag);
                    return 1;
                }
            }
        } else {
            targetFiles.push_back(arg);
        }
    }

    if (show_help) { PrintUsage(); return 0; }
    if (show_version) { PrintVersionInfo(); return 0; }

    if (targetFiles.empty()) {
        PrintError("No target file specified.");
        PrintUsage();
        return 1;
    }

    FILETIME ftAccess{}, ftModify{};
    bool useSpecificTime = !refFile.empty() || !tStamp.empty() || !dStamp.empty();

    if (!refFile.empty()) {
        if (!GetTimesFromReferenceFile(refFile, ftAccess, ftModify)) return 1;
    } else if (!tStamp.empty()) {
        SYSTEMTIME st;
        if (!ParseTouchTimestamp(tStamp, st) || !SystemTimeToFileTime(&st, &ftAccess)) return 1;
        ftModify = ftAccess;
    } else if (!dStamp.empty()) {
        SYSTEMTIME st;
        if (!ParseDateTimeString(dStamp, st) || !SystemTimeToFileTime(&st, &ftAccess)) return 1;
        ftModify = ftAccess;
    }
    
    bool updateAccess = !flags.count("-m") || flags.count("-a");
    bool updateModify = !flags.count("-a") || flags.count("-m");

    int overallResult = 0;
    for (const auto& currentTargetFile : targetFiles) {
        if (!useSpecificTime) {
            SYSTEMTIME stNow;
            GetSystemTime(&stNow);
            SystemTimeToFileTime(&stNow, &ftAccess);
            ftModify = ftAccess;
        }

        HANDLE hFile = CreateFileA(currentTargetFile.c_str(), FILE_WRITE_ATTRIBUTES,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                                       FILE_FLAG_BACKUP_SEMANTICS, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                if (flags.count("-c")) {
                    continue;
                }
                hFile = CreateFileA(currentTargetFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE) {
                    PrintApiError("Creating file", currentTargetFile);
                    overallResult = 1;
                    continue;
                }
            } else {
                PrintApiError("Opening file", currentTargetFile);
                overallResult = 1;
                continue;
            }
        }

        if (!SetFileTime(hFile, nullptr, updateAccess ? &ftAccess : nullptr, updateModify ? &ftModify : nullptr)) {
            PrintApiError("Setting file times", currentTargetFile);
            overallResult = 1;
        }
        CloseHandle(hFile);

        if (!UpdateFileAttributes(currentTargetFile, flags)) {
            overallResult = 1;
        }
    }

    return overallResult;
}