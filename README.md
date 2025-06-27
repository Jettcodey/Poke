# Poke

**Poke** is a simple Windows command-line utility that mimics the basic functionality of the UNIX `touch` command. It lets you update access and modification timestamps of files and optionally create files if they don't exist. Additionally, you can set file attributes such as hidden and read-only.

## Features

- Update file access and/or modification times
- Create new files if they don’t exist
- Apply custom timestamps or copy from another file
- Set or remove file attributes like hidden and read-only
- Supports multiple files and combined short options

## Usage

```bash
poke [OPTIONS] file [...]
```

## Requirements & Installation

- **Supported OS:**
    - Windows XP SP3 or later (32-bit)
    - Windows XP SP2 or later (64-bit)
- **Dependencies:**
    - **[Visual C++ Redistributable for Visual Studio 2015](https://www.microsoft.com/en-us/download/details.aspx?id=48145)** (*required to run `poke`*)

- **Download:**  
   Get the latest version from the [Releases page](https://github.com/Jettcodey/Poke/releases/latest).

## Options

* `-h`: Mark the file as hidden.
* `-v`: Mark the file as visible (remove the hidden attribute).
* `-o`: Mark the file as read-only.
* `-w`: Mark the file as writable (remove the read-only attribute).
* `-a`: Update the access time only.
* `-m`: Update the modification time only.
* `-t STAMP`: Use a specific timestamp for the file. The format is `[[CC]YY]MMDDhhmm[.ss]` (local time).
    * `MMDDhhmm`: Month, Day, Hour, Minute (uses the current year).
    * `YYMMDDhhmm`: Two-digit Year, Month, Day, Hour, Minute (interprets years 69-99 as 19xx, 00-68 as 20xx).
    * `CCYYMMDDhhmm`: Four-digit Century and Year, Month, Day, Hour, Minute.
    * `.ss` (optional): Seconds (up to two digits).
* `-d DATESTR`: Use a specific date and time string (local time). The date part must be in `DD-MM-YY` or `DD-MM-YYYY` format, and the time can be 24-hour or 12-hour with AM/PM. Seconds are optional.
    * Supported formats include (but are not limited to):
        * `DD-MM-YYYY hh:mm:ss` or `DD-MM-YYYY hh:mm`
        * `DD-MM-YYYY hh:mm:ss AM/PM` or `DD-MM-YYYY hh:mm AM/PM`
        * `DD-MM-YY hh:mm:ss` or `DD-MM-YY hh:mm`
        * `DD-MM-YY hh:mm:ss AM/PM` or `DD-MM-YY hh:mm AM/PM`
        * `DD-MM hh:mm:ss`, `DD-MM hh:mm`, `DD-MM hh:mm:ss AM/PM`, or `DD-MM hh:mm` (current year assumed if not provided)
* `-r REF_FILE`: Use the timestamps from the specified `Reference file`.
* `--version`: Show version information and exit.
* `--help`: Show this usage information and exit.

>`-t`, `-d`, and `-r` are mutually exclusive. If none are used, the current system time is applied.

## Notes

* If neither `-a` nor `-m` is specified, both times are updated.
* Supports combined short options, e.g., `-ho`.
* You can specify multiple files at once.

## Examples

* Update the timestamp of `myfile.txt` to the current time:
    ```
    poke myfile.txt
    ```

* Create `newfile.txt`:
    ```
    poke newfile.txt
    ```

* Update only the access time of `report.docx`:
    ```
    poke -a report.docx
    ```

* Set the timestamp of `archive.zip` to October 27, 2023 at 10:30 AM:
    ```
    poke -t 2310271030 archive.zip
    ```

* Set the timestamp of `image.png` using a specific date and time (DD-MM-YYYY format):
    ```
    poke -d "15-05-2024 14:00:00" image.png
    ```

* Set the timestamp of `document.txt` to May 15, 2024 at 2:00 PM (DD-MM-YYYY format, 24-hour time):
    ```
    poke -d "15-05-2024 14:00" document.txt
    ```

* Set the timestamp of `report.pdf` to May 15, 2024 at 2:00 PM (DD-MM-YY format, 12-hour time with AM/PM):
    ```
    poke -d "15-05-24 2:00PM" report.pdf
    ```

* Apply the timestamps from `template.txt` to `document.pdf`:
    ```
    poke -r template.txt document.pdf
    ```

* Mark `secret.txt` as hidden:
    ```
    poke -h secret.txt
    ```

* Mark `readonly.doc` as read-only:
    ```
    poke -o readonly.doc
    ```

* Do not create `missing.log` if it doesn't exist:
    ```
    poke -c missing.log
    ```

## Build from Source

0. **Prerequisites:**
    - **Visual Studio 2017** (Community Edition is free) with:
        - **Windows 10 SDK 10.0.17763.0**
        - **VC++ 2017 version 15.9 v14.16 latest v141 tools**
        - **Windows XP support for C++**

    - **CMake** version **3.10+**

1. **Clone the Repository:**
    ```bash
    git clone https://github.com/Jettcodey/Poke.git
    cd poke
    ```
2. **Generate Build Files:**

    **For x86:**
    ```bash
    mkdir build-x86
    cd build-x86
    cmake -G "Visual Studio 15 2017" -T v141_xp ..
    ```
    **For x64:**
    ```bash
    mkdir build-x64
    cd build-x64
    cmake -G "Visual Studio 15 2017" -A x64 -T v141_xp ..
    ```

3. **Build**

    **Debug:**
    ```bash
    cmake --build . --config Debug
    ```
    **Release:**
    ```bash
    cmake --build . --config Release
    ```