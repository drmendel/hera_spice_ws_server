// C++ Standard Library
#include <filesystem>           // File path and directory handling (create_directories, current_path)
#include <algorithm>            // Utility algorithms (max)
#include <iostream>             // Console input/output (cout, cerr)
#include <fstream>              // File reading and writing
#include <cstring>              // String manipulation (strlen)
#include <vector>               // Dynamic array storage
#include <array>                // Fixed-size data storage

// System Libraries
#include <sys/ioctl.h>          // Terminal width retrieval (ioctl, winsize)
#include <unistd.h>             // File and terminal operations (STDOUT_FILENO)
#include <limits.h>             // Path length limits (PATH_MAX)
#include <libgen.h>             // Path manipulation (dirname)

// External Libraries
#include <minizip/unzip.h>      // ZIP extraction (unzFile, unzOpen, unzReadCurrentFile)
#include <curl/curl.h>          // Network requests (file downloads, update checks)

// Project Headers
#include <data_manager.hpp>     // Project-specific functions



unsigned int getTerminalWidth() {
    unsigned int width = 40;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        width = w.ws_col;
    }
    return width;
}
std::string getDefaultSaveDir() {
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) {
        perror("readlink");
        return "";
    }
    exePath[len] = '\0';  // Null-terminate
    return std::string(dirname(dirname(exePath)));  // Get the parent of the executable's directory
}

std::filesystem::path getExecutablePath() {
    std::array<char, PATH_MAX> path{};
    ssize_t count = readlink("/proc/self/exe", path.data(), path.size() - 1);
    return (count != -1) ? std::filesystem::path(path.data()) : std::filesystem::path{};
}


// DOWNLOAD MANAGEMENT

size_t writeStringCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string downloadUrlContent(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    std::string result;
    if (res == CURLE_OK) {
        result = std::move(response);  // Avoid unnecessary copy
    }

    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    return result;
}

size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    out->write(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

int progressCallback(void* ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    if (total <= 0) return 0;

    constexpr int padding = 20;
    int width = getTerminalWidth() / 2 - padding;
    width = std::max(5, width);
    int progress = static_cast<int>((now * width) / total);
    std::cout << "\r\033[2K" << (now >= total ? "Finished [" : "Progress [")
              << std::string(progress, '#') << std::string(width - progress, '.')
              << "] " << std::fixed << std::setw(6) << std::setprecision(2) << (now * 100.0 / total) << "% " << std::flush;

    return 0;
}

std::string getFilenameFromUrl(const std::string& url) {
    size_t pos = url.find_last_of("/");
    return (pos != std::string::npos) ? url.substr(pos + 1) : url;
}

bool downloadFile(const std::string& url, const std::filesystem::path& saveDirectory) {
    std::cout << "Downloading: " << url << "\n";
    
    // Ensure directory exists
    if (!std::filesystem::exists(saveDirectory) && !std::filesystem::create_directories(saveDirectory)) {
        std::cerr << "Failed to create directory: " << saveDirectory << std::endl;
        return false;
    }

    std::string savePath = (saveDirectory / getFilenameFromUrl(url)).string();

    // Use RAII for CURL and file handling
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL\n";
        return false;
    }

    std::ofstream file(savePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << savePath << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Handle redirects

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    file.close();

    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        std::filesystem::remove(savePath); // Remove incomplete file
        return false;
    }

    std::cout << "\n";  // Newline after progress bar
    return true;
}



// ZIP MANAGEMENT

void printZipProgressBar(int current, int total) {
    if (total <= 0) return; // Prevent division by zero
    static int barWidth = std::max(10, (int)getTerminalWidth() / 2 - 20); // Cache bar width

    float progress = static_cast<float>(current) / total;
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "\033[K\r" << (progress < 1.0f ? "Progress [" : "Finished [")
              << std::string(pos, '#') << std::string(barWidth - pos, '.') << "] "
              << std::setw(3) << std::setprecision(2) << std::fixed << (progress * 100) << "% " << std::flush;
}


int extractFile(unzFile zip, const std::string& outputPath, int current, int total) {
    unz_file_info fileInfo;
    char filename[PATH_MAX];

    if (unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) {
        std::cerr << "Failed to get file info." << std::endl;
        return -1;
    }

    std::filesystem::path fullPath = std::filesystem::path(outputPath) / filename;

    if (filename[strlen(filename) - 1] == '/') {
        std::filesystem::create_directories(fullPath);
        return 0;  // No file extraction needed for directories
    }

    std::filesystem::create_directories(fullPath.parent_path());

    if (unzOpenCurrentFile(zip) != UNZ_OK) {
        std::cerr << "Failed to open file inside zip: " << filename << std::endl;
        return -1;
    }

    std::ofstream outFile(fullPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create output file: " << fullPath << std::endl;
        unzCloseCurrentFile(zip);
        return -1;
    }

    std::vector<char> buffer(256 * 1024);
    int bytesRead;
    while ((bytesRead = unzReadCurrentFile(zip, buffer.data(), buffer.size())) > 0) {
        outFile.write(buffer.data(), bytesRead);
    }

    unzCloseCurrentFile(zip);
    // printZipProgressBar(current, total);

    return 0;
}


bool unzipRecursive(const std::filesystem::path& zipFilePath, std::filesystem::path& saveDirectory) {
    std::cout << "Extracting " << zipFilePath << " to " << saveDirectory << std::endl;
    unzFile zip = unzOpen(zipFilePath.string().c_str());
    if (!zip) {
        std::cerr << "Failed to open zip file: " << zipFilePath << std::endl;
        return false;
    }

    int fileCount = 0;
    do {++fileCount;}
    while (unzGoToNextFile(zip) == UNZ_OK);

    if (unzGoToFirstFile(zip) != UNZ_OK) {
        std::cerr << "Failed to go to first file in zip." << std::endl;
        unzClose(zip);
        return false;
    }

    int extracted = 0;
    do {
        if (extractFile(zip, saveDirectory, ++extracted, fileCount) != 0) {
            std::cerr << "Error extracting a file." << std::endl;
            break;
        }
    } while (unzGoToNextFile(zip) == UNZ_OK);

    unzClose(zip);
    // std::cout << "\n";   // New line after progress bar
    return true;
}

// MOVE MANAGEMENT

std::string removeFilenameExtension(std::string& filename) {
    size_t dotPos = filename.rfind('.');
    return (dotPos == std::string::npos) ? filename : filename.erase(dotPos);
}

void replaceInFile(const std::filesystem::path& filePath, const std::string& target, const std::string& replacement) {
    std::ifstream inFile(filePath, std::ios::in);
    if (!inFile) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    std::filesystem::path tempFile = filePath.string() + ".tmp";
    std::ofstream outFile(tempFile, std::ios::out | std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to create temporary file: " << tempFile << std::endl;
        return;
    }

    std::string line;
    bool modified = false;

    while (std::getline(inFile, line)) {
        size_t pos = 0;
        while ((pos = line.find(target, pos)) != std::string::npos) {
            line.replace(pos, target.length(), replacement);
            pos += replacement.length();
            modified = true;
        }
        outFile << line << '\n';
    }

    inFile.close();
    outFile.close();

    if (modified) {
        std::filesystem::rename(tempFile, filePath);
    } else {
        std::filesystem::remove(tempFile);
    }
}


bool updateMetaKernelPaths(const std::filesystem::path& mkDir, const std::filesystem::path& replacementPath) {
    if (!std::filesystem::is_directory(mkDir)) {
        std::cerr << "Invalid directory: " << mkDir << std::endl;
        return false;
    }

    const auto& replacement = replacementPath.native();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(mkDir, std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            replaceInFile(entry.path(), "..", replacement);
        }
    }

    std::cout << "MetaKernel paths updated successfully." << std::endl;
    return true;
}


std::string ensureTrailingSlash(const std::string& path) {
    if (!path.empty() && path.back() != '/') return path + '/';
    else return path;
}

bool replaceDirectory(const std::filesystem::path& source, const std::filesystem::path& target) {
    try {
        // Verify that the source directory exists
        if (!std::filesystem::is_directory(source)) {
            std::cerr << "Error: Source directory does not exist or is not a directory.\n";
            return false;
        }

        // Remove the target directory if it exists
        std::error_code ec;
        std::filesystem::remove_all(target, ec);
        if (ec) {
            std::cerr << "Warning: Failed to remove target directory: " << ec.message() << "\n";
        }

        // Move the source directory to the target location
        std::filesystem::rename(source, target);
        std::cout << "Renamed: " << source << " to " << target << "\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}


// DATA_MANGER

std::string DataManager::loadLocalVersion() {
    std::ifstream versionFile(versionFilePath);
    if (!versionFile.is_open()) return "default";

    std::getline(versionFile, localVersion);
    versionFile.close();

    return localVersion;
}

std::string DataManager::loadRemoteVersion() {
    return downloadUrlContent(versionUrl);
}

DataManager::DataManager() {
    // Initialize URLs
    zipUrl = REMOTE_ARCHIVE_URL;
    versionUrl = REMOTE_VERSION_URL;

    // Set up directories
    exeDirectory = std::filesystem::path(getDefaultSaveDir());
    dataDirectory = exeDirectory / "data";
    heraDirectory = dataDirectory / "hera";
    heraTemporaryDirectory = dataDirectory / "HERA";
    kernelDirectory = heraDirectory / "kernels";
    tempMetaKernelDirectory = heraTemporaryDirectory / "kernels" / "mk";

    // Initialize zip file path
    zipFile = dataDirectory / std::filesystem::path(getFilenameFromUrl(zipUrl));

    // Set version file path
    versionFilePath = heraDirectory / "version";

    // Get versions
    localVersion = loadLocalVersion();
    remoteVersion = loadRemoteVersion();
}

std::string DataManager::getLocalVersion() const {
    return localVersion;
}

bool DataManager::isNewVersionAvailable() {
    if (localVersion == remoteVersion) {
        std::cout << "No new kernel version available.\n";
        return false;
    }
    std::cout << "New kernel version available.\n";
    return true;
}    

bool DataManager::downloadZipFile() {
    return downloadFile(zipUrl, dataDirectory);
}

bool DataManager::unzipZipFile() {
    return unzipRecursive(zipFile, dataDirectory);
}

bool DataManager::editTempMetaKernelFiles() {
    return updateMetaKernelPaths(tempMetaKernelDirectory, kernelDirectory);    
}

bool DataManager::editTempVersionFile() {
    std::ofstream versionFile(heraTemporaryDirectory / "version");
    if (!versionFile.is_open()) {
        std::cerr << "No version file found." << std::endl;
        return false;
    }
    versionFile << remoteVersion;
    versionFile.close();
    std::cout << "Updated temporary version file.\n";
    return true;
}


bool DataManager::moveFolder() {
    return replaceDirectory(heraTemporaryDirectory, heraDirectory);
}

bool DataManager::deleteZipFile() {
    std::error_code ec;
    if (!std::filesystem::remove(zipFile, ec)) {
        std::cerr << "Error deleting " << zipFile << ": " << ec.message() << std::endl;
        return false;
    }
    std::cout << "Deleted: " << zipFile << std::endl;
    return true;
}

void DataManager::updateLocalVersion() {
    localVersion = remoteVersion;
}