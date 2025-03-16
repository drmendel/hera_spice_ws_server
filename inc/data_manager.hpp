#ifndef DATA_MANAGER_HPP
#define DATA_MANAGER_HPP

// CPP
#include <iostream>
#include <filesystem>

// SYSTEM
#include <curl/curl.h>
#include <minizip/unzip.h>

#define REMOTE_ARCHIVE_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/HERA.zip"
#define REMOTE_VERSION_URL "https://spiftp.esac.esa.int/data/SPICE/HERA/misc/skd/version.txt"

// SYSTEM FUNCTIONS
unsigned int getTerminalWidth();
std::filesystem::path getExecutablePath();

// DOWNLOAD MANAGEMENT
size_t writeStringCallback(void* contents, size_t size, size_t nmemb, std::string* output);
std::string downloadUrlContent(const std::string& url);
size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream);
int progressCallback(void* ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t);
std::string getDefaultSaveDir();
std::string getFilenameFromUrl(const std::string& url);
bool downloadFile(const std::string& url, const std::filesystem::path& saveDirectory);

// UNZIP MANAGEMENT
void printZipProgressBar(int current, int total);
int extractFile(unzFile zip, const std::string& outputPath, int current, int total);

bool unzipRecursive(const std::filesystem::path& zipFilePath, std::filesystem::path& saveDirecotry);    

// MOVE MANAGEMENT
std::string removeFilenameExtension(const std::string& filename);
void replaceInFile(const std::filesystem::path& filePath, const std::string& target, const std::string& replacement);
bool updateMetaKernelPaths(const std::filesystem::path& mkDir, const std::filesystem::path& replacementPath);
std::string ensureTrailingSlash(const std::string& path);
bool replaceDirectory(const std::filesystem::path& source, const std::filesystem::path& target);

class DataManager {

    std::string zipUrl;
    std::string versionUrl;

    std::string localVersion;

    std::filesystem::path exeDirectory;
    std::filesystem::path dataDirectory;
    std::filesystem::path heraDirectory;
    std::filesystem::path heraTemporaryDirectory;

    std::filesystem::path kernelDirectory;              // actual used kernel directory (kernel path in metakernel files)
    std::filesystem::path tempMetaKernelDirectory;      // temporary directory for editing meta kernel paths

    std::filesystem::path zipFile;

    std::string getLocalVersion();
    std::string getRemoteVersion();
public:
    DataManager();
    bool isNewVersionAvailable();
    bool downloadZipFile();
    bool unzipZipFile();
    bool editTempMetaKernelFiles();
    bool moveFolder();
    bool deleteZipFile();
};

#endif // DATA_MANAGER_HPP