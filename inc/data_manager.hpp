#ifndef DATA_MANAGER_HPP
#define DATA_MANAGER_HPP

// Standard C++ Libraries
#include <mutex>
#include <atomic>
#include <string>
#include <filesystem>
#include <condition_variable>

// External Libraries
#include <curl/curl.h>
#include <minizip/unzip.h>

// Remote URLs
#define REMOTE_ARCHIVE_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/HERA.zip"
#define REMOTE_VERSION_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/version.txt"

// ─────────────────────────────────────────────
// System Utility Functions
// ─────────────────────────────────────────────
unsigned int getTerminalWidth();
std::string getDefaultSaveDir();
std::filesystem::path getExecutablePath();

// ─────────────────────────────────────────────
// Download Management
// ─────────────────────────────────────────────
size_t writeStringCallback(void* contents, size_t size, size_t nmemb, std::string* output);
std::string downloadUrlContent(const std::string& url);
size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream);
int progressCallback(void* ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t);
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
    std::filesystem::path exeDirectory;                 // Directory where the executable is located (build)
    std::filesystem::path dataDirectory;                // Data directory (where the zip file - HERA.zip - is downloaded
    std::filesystem::path heraDirectory;                // Main Hera directory (data is used from here)
    std::filesystem::path heraTemporaryDirectory;       // Temporary directory for unzipping (HERA)
    std::filesystem::path kernelDirectory;              // Active kernel directory (referenced in meta-kernel files)
    std::filesystem::path tempMetaKernelDirectory;      // Temporary directory for modifying meta-kernel paths

    // File Paths
    std::filesystem::path zipFile;                      // Path to the downloaded zip file (HERA.zip)

    // Version Management
    std::string loadLocalVersion();                     // Load the local version from the version file
    std::string loadRemoteVersion();                    // Load the remote version from the URL

public:
    DataManager();

    // Core Operations
    std::string getLocalVersion() const;                // Get the local version from memory
    bool isNewVersionAvailable();                       // Check if a new version is available
    bool downloadZipFile();                             // Download the zip file from the URL  
    bool unzipZipFile();                                // Unzip the downloaded zip file
    bool editTempMetaKernelFiles();                     // Edit the temporary meta-kernel files ('..' -> 'actual/kernel/path') 
    bool editTempVersionFile();                         // Edit the temporary version file (update the version file in the new directory)
    bool moveFolder();                                  // Move the unzipped folder to the data directory   
    bool deleteZipFile();                               // Delete the downloaded zip file
    void updateLocalVersion();                          // Update the local version in the memory to the new version
};

// ─────────────────────────────────────────────
// DataManager Shutdown Control
// ─────────────────────────────────────────────
extern std::mutex versionMutex;
extern std::condition_variable versionCondition;
extern std::atomic<bool> shouldDataManagerRun;
void stopDataManagerWorker();

#endif // DATA_MANAGER_HPP
