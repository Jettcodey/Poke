#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <system_error>

#ifndef PROJECT_AUTHOR
#define PROJECT_AUTHOR "Jettcodey"
#endif
#ifndef PROJECT_COPYRIGHT
#define PROJECT_COPYRIGHT "(C) Copyright 2025 Jettcodey. All rights reserved."
#endif
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.1.0"
#endif

// Function to print Windows API errors
void PrintApiError(const std::string& context, DWORD errorCode, const std::string& filename = "") {
    if (errorCode == 0) errorCode = GetLastError(); // Get error code

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
        // Trim trailing newline characters often included by 'FormatMessage'
        while (size > 0 && (messageBuffer[size - 1] == '\n' || messageBuffer[size - 1] == '\r')) {
            messageBuffer[--size] = '\0';
        }
        std::cerr << " " << messageBuffer;
        LocalFree(messageBuffer); // Free the buffer allocated by 'FormatMessage'
    }
     else {
        std::cerr << " Windows error message could not be retrieved.";
     }
    std::cerr << std::endl;
}

// Function to print generic errors
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
    "  poke [OPTIONS] file [...]\n\n"
    "Options:\n"
    "  -h           Mark file as hidden\n"
    "  -v           Mark file as visible (remove hidden attribute)\n"
    "  -o           Mark file as read-only\n"
    "  -w           Mark file as writable (remove read-only attribute)\n"
    "  -a           Update access time only\n"
    "  -m           Update modification time only\n"
    "  -t STAMP     Use [[CC]YY]MMDDhhmm[.ss] timestamp (local time)\n"
    "  -d DATESTR   Use 'YYYY-MM-DD hh:mm:ss' date string (local time)\n"
    "  -r REF_FILE  Use timestamps from REF_FILE\n"
    "  -c           Do not create file if it does not exist\n"
    "  --version    Show version information and exit\n"
    "  --help       Show this usage information and exit\n\n"
    "Notes:\n"
    "  - If neither -a nor -m is given, both times are updated.\n"
    "  - If -t, -d, or -r is not given, the current time is used.\n"
    "  - Options -t, -d, and -r are mutually exclusive.\n";
}

// Parses "[[CC]YY]MMDDhhmm[.ss]" (local time) into a UTC SYSTEMTIME.
bool ParsePokeTimestamp(const std::string &ts, SYSTEMTIME &out) {
    std::string s = ts;
    std::string frac_str;
    int sec = 0;

    // Separate seconds if present
    auto dot = s.find('.');
    if (dot != std::string::npos) {
        frac_str = s.substr(dot + 1);
        s = s.substr(0, dot);
        if (frac_str.length() > 2) frac_str = frac_str.substr(0, 2); // Max 2 digits for seconds
        try {
            sec = std::stoi(frac_str);
            if (sec < 0 || sec > 59) throw std::out_of_range("Seconds out of range");
        } catch (const std::invalid_argument&) {
            PrintError("Invalid seconds component in -t timestamp", frac_str);
            return false;
        } catch (const std::out_of_range&) {
            PrintError("Seconds component in -t timestamp out of range (0-59)", frac_str);
            return false;
        }
    }

    // Determine format and extract parts
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    std::string year_s, month_s, day_s, hour_s, minute_s;

    try {
        if (s.length() == 8) { // MMDDhhmm (No year specified - use current year)
             SYSTEMTIME currentLocalTime;
             GetLocalTime(&currentLocalTime);
             year = currentLocalTime.wYear;

             month_s = s.substr(0, 2); day_s = s.substr(2, 2);
             hour_s = s.substr(4, 2); minute_s = s.substr(6, 2);
        } else if (s.length() == 10) { // YYMMDDhhmm
            int yy = std::stoi(s.substr(0, 2));
            // POSIX Poke convention for 2-digit year (69-99 -> 19xx, 00-68 -> 20xx)
            year = (yy < 69) ? (2000 + yy) : (1900 + yy);

            month_s = s.substr(2, 2); day_s = s.substr(4, 2);
            hour_s = s.substr(6, 2); minute_s = s.substr(8, 2);
        } else if (s.length() == 12) { // CCYYMMDDhhmm
            year_s = s.substr(0, 4); // Keep as string for now
            year = std::stoi(year_s);

            month_s = s.substr(4, 2); day_s = s.substr(6, 2);
            hour_s = s.substr(8, 2); minute_s = s.substr(10, 2);
        } else {
            PrintError("Invalid length for -t timestamp string", s);
            return false;
        }

        // Convert parts to integers
        month = std::stoi(month_s); day = std::stoi(day_s);
        hour = std::stoi(hour_s); minute = std::stoi(minute_s);

    } catch (const std::invalid_argument&) {
        PrintError("Non-numeric component found in -t timestamp", s);
        return false;
    } catch (const std::out_of_range&) {
        PrintError("Numeric component out of range in -t timestamp", s);
        return false;
    }

    // Validate components
    if (month < 1 || month > 12) { PrintError("Month out of range (1-12) in -t timestamp", month_s); return false; }
    if (day < 1 || day > 31)     { PrintError("Day out of range (1-31) in -t timestamp", day_s); return false; } // Basic check
    if (hour < 0 || hour > 23)   { PrintError("Hour out of range (0-23) in -t timestamp", hour_s); return false; }
    if (minute < 0 || minute > 59) { PrintError("Minute out of range (0-59) in -t timestamp", minute_s); return false; }
    // Note: We don't do complex day-of-month validation here, rely on mktime/gmtime_s

    // Populate tm struct (local time)
    std::tm tm = {};
    tm.tm_year = year - 1900; // tm_year is years since 1900
    tm.tm_mon = month - 1;    // tm_mon is 0-11
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = sec;
    tm.tm_isdst = -1; // Let mktime determine DST

    // Convert local tm struct to time_t
    time_t time_val = mktime(&tm);
    if (time_val == -1) {
        PrintError("Failed to convert parsed -t timestamp components to a valid calendar time", ts);
        // This often happens if the date components are invalid (e.g., Feb 30th)
        return false;
    }

    // Convert time_t to UTC tm struct
    struct tm utc_tm = {};
    errno_t gmtime_err = gmtime_s(&utc_tm, &time_val);
    if (gmtime_err != 0) {
        // Use strerror_s or std::system_error for more specific C runtime errors if needed
         PrintError("Failed to convert calendar time to UTC structure", std::to_string(gmtime_err));
        return false;
    }

    // Manually convert UTC tm struct to UTC SYSTEMTIME struct
    out.wYear         = utc_tm.tm_year + 1900;
    out.wMonth        = utc_tm.tm_mon + 1;
    out.wDayOfWeek    = utc_tm.tm_wday;
    out.wDay          = utc_tm.tm_mday;
    out.wHour         = utc_tm.tm_hour;
    out.wMinute       = utc_tm.tm_min;
    out.wSecond       = utc_tm.tm_sec;

    return true;
}

// Open file for reading its times
bool GetFileTimes(const std::string &path, FILETIME &c, FILETIME &a, FILETIME &m) {
    HANDLE h = CreateFileA(path.c_str(),
                           GENERIC_READ,
                           FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        PrintApiError("Opening file to read times", 0, path);
        return false;
    }

    bool ok = GetFileTime(h, &c, &a, &m);
    if (!ok) {
        PrintApiError("Reading file times", 0, path);
    }

    CloseHandle(h);
    return ok;
}

bool SetFileTimes(const std::string &path,
                  const FILETIME* c, const FILETIME* a, const FILETIME* m) {
    HANDLE h = CreateFileA(path.c_str(),
                           FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           NULL);

    if (h == INVALID_HANDLE_VALUE) {
        PrintApiError("Opening file to set times", 0, path);
        return false;
    }

    if (!c && !a && !m) {
        std::cerr << "No times provided, nothing to do" << std::endl;
        CloseHandle(h);
        return true;
    }

    SetLastError(0); // Clear last error before calling SetFileTime

    // Use const_cast as SetFileTime doesn't accept const
    bool ok = SetFileTime(h,
                          const_cast<FILETIME*>(c),
                          const_cast<FILETIME*>(a),
                          const_cast<FILETIME*>(m));

    if (!ok) {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_INVALID_PARAMETER && c != nullptr) {
            std::cerr << "Warning: Could not set creation time for file '" << path
                      << "'. This is often restricted by the OS or filesystem." << std::endl;

            // Try again without creation time
            ok = SetFileTime(h, nullptr,
                             const_cast<FILETIME*>(a),
                             const_cast<FILETIME*>(m));
            if (!ok) {
                PrintApiError("Setting access/modification time (after failing creation time)", 0, path);
            }
        } else {
            PrintApiError("Setting file times", lastError, path);
        }
    }

    CloseHandle(h);
    return ok;
}


// Set File Attributes
bool UpdateFileAttributes(const std::string& path, const std::unordered_set<std::string>& flags) {
     DWORD currentAttrs = GetFileAttributesA(path.c_str());
     if (currentAttrs == INVALID_FILE_ATTRIBUTES) {
          PrintApiError("Getting file attributes", 0, path);
          return false;
     }

     DWORD newAttrs = currentAttrs;
     bool changed = false;

     // Apply flags - note precedence if conflicting flags are given (e.g., -h and -v)
     if (flags.count("-h")) {
          if (!(newAttrs & FILE_ATTRIBUTE_HIDDEN)) { newAttrs |= FILE_ATTRIBUTE_HIDDEN; changed = true; }
     }
     if (flags.count("-v")) { // -v overrides -h
          if (newAttrs & FILE_ATTRIBUTE_HIDDEN) { newAttrs &= ~FILE_ATTRIBUTE_HIDDEN; changed = true; }
     }
     if (flags.count("-o")) {
          if (!(newAttrs & FILE_ATTRIBUTE_READONLY)) { newAttrs |= FILE_ATTRIBUTE_READONLY; changed = true; }
     }
     if (flags.count("-w")) { // -w overrides -o
          if (newAttrs & FILE_ATTRIBUTE_READONLY) { newAttrs &= ~FILE_ATTRIBUTE_READONLY; changed = true; }
     }

     if (changed) {
          if (!SetFileAttributesA(path.c_str(), newAttrs)) {
               PrintApiError("Setting file attributes", 0, path);
               return false;
          }
     }
     return true;
}

// Main Function

int main(int argc, char* argv[]) {
    // Initial Checks & Help/Version
    if (argc < 2) {
        PrintUsage();
        return 1;
    }
    // Use standard --help convention
    if (argc > 1 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "/version")) {
        PrintVersionInfo();
        return 0;
    }
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "/help" || std::string(argv[1]) == "/?")) {
         PrintUsage();
         return 0;
    }


    // Argument Parsing
    std::unordered_set<std::string> flags;
    std::string refFile, targetFile, tStamp, dStamp;
    std::vector<std::string> targetFiles; // Store potential target files

    // Current limitations: Assumes flags come before files, simple -r handling.
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "-r") {
            if (!refFile.empty()) { PrintError("Cannot specify -r multiple times."); return 1; }
            if (flags.count("-t") || flags.count("-d")) { PrintError("Cannot use -r with -t or -d."); return 1; }
            if (i + 1 < argc) {
                refFile = argv[++i];
                flags.insert("-r"); // Mark -r as seen
            } else {
                PrintError("Option -r requires a reference file argument.");
                return 1;
            }
        } else if (a == "-t") {
            if (!tStamp.empty()) { PrintError("Cannot specify -t multiple times."); return 1; }
             if (flags.count("-r") || flags.count("-d")) { PrintError("Cannot use -t with -r or -d."); return 1; }
            if (i + 1 < argc) {
                tStamp = argv[++i];
                flags.insert("-t");
            } else {
                PrintError("Option -t requires a timestamp argument.");
                return 1;
            }
        } else if (a == "-d") {
             if (!dStamp.empty()) { PrintError("Cannot specify -d multiple times."); return 1; }
             if (flags.count("-r") || flags.count("-t")) { PrintError("Cannot use -d with -r or -t."); return 1; }
            if (i + 1 < argc) {
                dStamp = argv[++i];
                flags.insert("-d");
            } else {
                PrintError("Option -d requires a date string argument.");
                return 1;
            }
        } else if (a[0] == '-' && a.length() > 1) {
            // Handle combined flags like -am or individual like -h
            for (size_t j = 1; j < a.size(); ++j) {
                std::string flag = "-" + std::string(1, a[j]);

                if (flag == "-h" || flag == "-v" || flag == "-o" || flag == "-w" ||
                    flag == "-a" || flag == "-m" || flag == "-c") {
                    flags.insert(flag);
                } else {
                    PrintError("Unknown option specified", flag);
                    PrintUsage();
                    return 1;
                }
            }
        } else {
            // Assume it's a target file
            targetFiles.push_back(a);
        }
    }

    if (targetFiles.empty()) {
        PrintError("No target file specified.");
        PrintUsage();
        return 1;
    }
    // End Argument Parsing


    // Get Current Time (UTC) once if needed
    SYSTEMTIME now_st{};
    FILETIME now_ft{};
    bool now_time_valid = false;
    // Only get time if not using specific timestamp/ref file OR if using -a/-m implicitly
    if (refFile.empty() && tStamp.empty() && dStamp.empty() || flags.count("-a") || flags.count("-m")) {
        GetSystemTime(&now_st); // Get current UTC time
        if (!SystemTimeToFileTime(&now_st, &now_ft)) {
            PrintApiError("Getting current system time", 0);
            return 1;
        }
        now_time_valid = true;
    }


    // Process Each Target File
    int overallResult = 0; // Track if any file failed
    for (const auto& currentTargetFile : targetFiles) {
        bool fileProcessedSuccessfully = true; // Assume success for this file initially

        // Check File Existence
        DWORD attrs = GetFileAttributesA(currentTargetFile.c_str());
        bool exists = attrs != INVALID_FILE_ATTRIBUTES;

        // Handle -c (No Create)
        if (!exists && flags.count("-c")) {
            // File doesn't exist, and -c was specified. Do nothing for this file.
            continue; // Skip to the next target file
        }

        // Create File If Necessary
        if (!exists) {
            DWORD createAttrs = FILE_ATTRIBUTE_NORMAL;
            // Apply relevant creation attributes immediately if requested
            if (flags.count("-h")) createAttrs |= FILE_ATTRIBUTE_HIDDEN;

            HANDLE h = CreateFileA(currentTargetFile.c_str(),
                                   GENERIC_WRITE,
                                   0, NULL, CREATE_NEW, createAttrs, NULL);

            if (h == INVALID_HANDLE_VALUE) {
                PrintApiError("Creating file", 0, currentTargetFile);
                overallResult = 1; // Mark failure
                fileProcessedSuccessfully = false;
                continue; // Skip to next file
            }
            CloseHandle(h);
            exists = true;
        }
        // End File Creation


        // Determine Desired Timestamps
        FILETIME ftAccess{}, ftModify{};
        bool updateAccess = false;
        bool updateModify = false;

        if (!refFile.empty()) {
             // Use Reference File Timestamps
            FILETIME refCreate, refAccess, refModify;
            if (!GetFileTimes(refFile, refCreate, refAccess, refModify)) {
                // Error printed by GetFileTimes
                overallResult = 1; fileProcessedSuccessfully = false; continue;
            }
            ftAccess = refAccess; updateAccess = true;
            ftModify = refModify; updateModify = true;

        } else if (!tStamp.empty()) {
            // Use -t Timestamp
            SYSTEMTIME st = {};
            if (!ParsePokeTimestamp(tStamp, st)) {
                // Error printed by ParsePokeTimestamp
                 overallResult = 1; fileProcessedSuccessfully = false; continue;
            }
            if (!SystemTimeToFileTime(&st, &ftAccess)) { // Use ftAccess as temporary storage
                 PrintApiError("Converting -t timestamp (SystemTimeToFileTime)", 0, currentTargetFile);
                 overallResult = 1; fileProcessedSuccessfully = false; continue;
            }
            ftModify = ftAccess; // -t sets both unless overridden
            updateAccess = true;
            updateModify = true;

        } else if (!dStamp.empty()) {
             // Use -d Date String
             std::tm tm_local = {};
             std::istringstream ss(dStamp);
             ss >> std::get_time(&tm_local, "%Y-%m-%d %H:%M:%S");

             if (ss.fail() || !ss.eof()) { // Check for parse failure OR extra characters
                  PrintError("Invalid format or extra characters in -d date string", dStamp);
                  overallResult = 1; fileProcessedSuccessfully = false; continue;
             }
             tm_local.tm_isdst = -1;

             time_t time_val = mktime(&tm_local);
             if (time_val == -1) {
                  PrintError("Invalid date/time components in -d string (mktime failed)", dStamp);
                  overallResult = 1; fileProcessedSuccessfully = false; continue;
             }

             struct tm utc_tm = {};
             if (gmtime_s(&utc_tm, &time_val) != 0) {
                  PrintError("Failed converting -d date to UTC structure (gmtime_s)", dStamp);
                  overallResult = 1; fileProcessedSuccessfully = false; continue;
             }

             SYSTEMTIME st = {};
             st.wYear = utc_tm.tm_year + 1900; st.wMonth = utc_tm.tm_mon + 1;
             st.wDayOfWeek = utc_tm.tm_wday;   st.wDay = utc_tm.tm_mday;
             st.wHour = utc_tm.tm_hour;       st.wMinute = utc_tm.tm_min;
             st.wSecond = utc_tm.tm_sec;      st.wMilliseconds = 0;

             if (!SystemTimeToFileTime(&st, &ftAccess)) { // Use ftAccess as temporary storage
                  PrintApiError("Converting -d timestamp (SystemTimeToFileTime)", 0, currentTargetFile);
                  overallResult = 1; fileProcessedSuccessfully = false; continue;
             }
             ftModify = ftAccess; // -d sets both unless overridden
             updateAccess = true;
             updateModify = true;
        } else {
             // Use Current Time (Default)
              if (!now_time_valid) {
                   // Should not happen...
                    PrintError("Could not get current time.");
                    overallResult = 1; fileProcessedSuccessfully = false; continue;
              }
             ftAccess = now_ft; updateAccess = true;
             ftModify = now_ft; updateModify = true;
        }

        // Apply -a / -m overrides
        bool onlyAccess = flags.count("-a") && !flags.count("-m");
        bool onlyModify = flags.count("-m") && !flags.count("-a");
        // If both -a and -m are specified we update both times

        if (onlyAccess) {
             if (!updateAccess) { // If -a given but no time source (-t/-d/-r), use 'now'
                  if (!now_time_valid) { PrintError("Could not get current time for -a."); overallResult = 1; fileProcessedSuccessfully = false; continue; }
                   ftAccess = now_ft;
             }
             updateAccess = true; // Ensure it's marked for update
             updateModify = false; // Explicitly don't update modify time
        } else if (onlyModify) {
              if (!updateModify) { // If -m given but no time source (-t/-d/-r), use 'now'
                   if (!now_time_valid) { PrintError("Could not get current time for -m."); overallResult = 1; fileProcessedSuccessfully = false; continue; }
                    ftModify = now_ft;
              }
             updateModify = true; // Ensure it's marked for update
             updateAccess = false; // Explicitly don't update access time
        }
        // End Timestamp Determination


        // Set File Times
        if (updateAccess || updateModify) {
            if (!SetFileTimes(currentTargetFile,
                              nullptr,
                              updateAccess ? &ftAccess : nullptr,
                              updateModify ? &ftModify : nullptr))
            {
                 // Error printed by SetFileTimes
                overallResult = 1; fileProcessedSuccessfully = false; continue; // Skip attributes for this file
            }
        }
        // End Setting File Times


        // Set File Attributes
        if (!UpdateFileAttributes(currentTargetFile, flags)) {
             // Error printed by UpdateFileAttributes
             overallResult = 1; fileProcessedSuccessfully = false; // Mark failure & continue loop
        }
        // End Setting File Attributes

    } // End Loop Through Target Files

    return overallResult; // 0 on success for all files, 1 if any file had an error
}