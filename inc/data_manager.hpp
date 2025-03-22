#ifndef DATA_MANAGER_HPP
#define DATA_MANAGER_HPP

// C++ Standard Library
#include <iostream>
#include <filesystem>

// System Libraries
#include <curl/curl.h>
#include <minizip/unzip.h>

// Remote URLs
#define REMOTE_ARCHIVE_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/HERA.zip"
#define REMOTE_VERSION_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/version.txt"

// ─────────────────────────────────────────────
// System Utility Functions
// ─────────────────────────────────────────────
unsigned int getTerminalWidth();
std::filesystem::path getExecutablePath();

// ─────────────────────────────────────────────
// Download Management
// ─────────────────────────────────────────────
size_t writeStringCallback(void* contents, size_t size, size_t nmemb, std::string* output);
std::string downloadUrlContent(const std::string& url);
size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream);
int progressCallback(void* ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t);
std::string getDefaultSaveDir();
std::string getFilenameFromUrl(const std::string& url);
bool downloadFile(const std::string& url, const std::filesystem::path& saveDirectory);

// ─────────────────────────────────────────────
// Unzip Management
// ─────────────────────────────────────────────
void printZipProgressBar(int current, int total);
int extractFile(unzFile zip, const std::string& outputPath, int current, int total);
bool unzipRecursive(const std::filesystem::path& zipFilePath, std::filesystem::path& saveDirectory);

// ─────────────────────────────────────────────
// File & Directory Management
// ─────────────────────────────────────────────
std::string removeFilenameExtension(std::string& filename);
void replaceInFile(const std::filesystem::path& filePath, const std::string& target, const std::string& replacement);
bool updateMetaKernelPaths(const std::filesystem::path& mkDir, const std::filesystem::path& replacementPath);
std::string ensureTrailingSlash(const std::string& path);
bool replaceDirectory(const std::filesystem::path& source, const std::filesystem::path& target);

// ─────────────────────────────────────────────
// DataManager Class
// ─────────────────────────────────────────────
class DataManager {
private:
    // URLs
    std::string zipUrl;
    std::string versionUrl;

    // Version Tracking
    std::string localVersion;
    std::string remoteVersion;
    std::filesystem::path versionFilePath;

    // Directory Paths
    std::filesystem::path exeDirectory;
    std::filesystem::path dataDirectory;
    std::filesystem::path heraDirectory;
    std::filesystem::path heraTemporaryDirectory;
    std::filesystem::path kernelDirectory;              // Active kernel directory (referenced in meta-kernel files)
    std::filesystem::path tempMetaKernelDirectory;      // Temporary directory for modifying meta-kernel paths

    // File Paths
    std::filesystem::path zipFile;

    // Version Management
    std::string loadLocalVersion();
    std::string loadRemoteVersion();

public:
    DataManager();

    // Core Operations
    std::string getLocalVersion() const;
    bool isNewVersionAvailable();
    bool downloadZipFile();
    bool unzipZipFile();
    bool editTempMetaKernelFiles();
    bool editTempVersionFile();               // the version file doesnt have the data for some reason
    bool moveFolder();
    bool deleteZipFile();
    void updateLocalVersion();
};

#endif // DATA_MANAGER_HPP
