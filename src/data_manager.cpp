// CPP
#include <filesystem>           // Used for handling file paths and directories (std::filesystem::create_directories, std::filesystem::current_path)
#include <iostream>             // Used for console input/output (std::cout, std::cerr)
#include <fstream>              // Used for file writing and editing

// SYSTEM
#include <sys/ioctl.h>          // Used to get terminal width (ioctl, winsize struct)
#include <unistd.h>             // Used for file and terminal operations (STDOUT_FILENO)
#include <limits.h>             // Used for defining PATH_MAX (maximum file path length)
#include <libgen.h>

// EXTERNAL
#include <cspice/SpiceUsr.h>    // CSPICE library for space navigation computations (SpiceDouble)
#include <uWebSockets/App.h>    // uWebSockets for handling WebSockets and HTTP requests (uWS::App)
#include <minizip/unzip.h>      // minizip for handling ZIP file extraction (unzFile, unzOpen, unzReadCurrentFile)
#include <curl/curl.h>          // libcurl for network requests (curl_easy_init, curl_easy_cleanup)

// PROJECT
#include <data_manager.hpp>     // function declerations

unsigned int getTerminalWidth() {
    unsigned int width = 80;
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
    exePath[len] = '\0';
    std::string dirPath = dirname(exePath);
    return dirname(const_cast<char*>(dirPath.c_str()));
}

std::filesystem::path getExecutablePath() {
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count != -1) {
        path[count] = '\0';
        return std::filesystem::path(path);
    }
    return {};
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
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);        // Timeout after 10s

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    return response;
}

size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = static_cast<std::ofstream*>(stream);
    out->write(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

int progressCallback(void* ptr, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    if (total > 0) {
        int width = getTerminalWidth()/2 - 20;  // Use half width of terminal for progress bar
        int progress = static_cast<int>((now * width) / total);
        std::cout << ((now/total == 1) ? "\r\033[2KFinished [" : "\r\033[2KProgress [") << std::string(progress, '#') << std::string(width - progress, '.')
                  << "] " << std::fixed << std::setw(5) << std::setprecision(2) << (now * 100.0 / total) << "% ";
        std::cout.flush();
    }
    return 0;
}

std::string getFilenameFromUrl(const std::string& url) {
    size_t pos = url.find_last_of("/");
    return (pos != std::string::npos) ? url.substr(pos + 1) : url;
}

bool downloadFile(const std::string& url, const std::filesystem::path& saveDirectory) {
    std::cout << "Downloading: " << url << "\n";
    std::filesystem::create_directories(saveDirectory);
    std::string savePath = saveDirectory.string() + "/" + getFilenameFromUrl(url);
    
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::ofstream file(savePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << savePath << std::endl;
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    
    std::cout << "\n";  // Ensure a newline after progress bar
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    return true;
}


// ZIP MANAGEMENT

void printZipProgressBar(int current, int total) {
    if (total == 0) return;
    int barWidth = getTerminalWidth() / 2 - 20;
    float progress = (float)current / total;
    int pos = barWidth * progress;

    std::cout << "\033[K\r";
    std::cout << (progress != 1 ? "Progress [" : "Finished [");
    for (int i = 0; i < barWidth; ++i) {
        std::cout << (i <= pos ? "#" : ".");
    }
    std::cout << "] " << std::setw(3) << int(progress * 100.0) << "%" << std::flush;
}

int extractFile(unzFile zip, const std::string& outputPath, int current, int total) {
    char buffer[256 * 1024];
    unz_file_info fileInfo;
    if (unzGetCurrentFileInfo(zip, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
        std::cerr << "Failed to get file info." << std::endl;
        return -1;
    }

    char filename[PATH_MAX];
    if (unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) {
        std::cerr << "Failed to get filename." << std::endl;
        return -1;
    }

    std::string fullPath = (std::filesystem::path(outputPath) / filename).string();
    if (filename[strlen(filename) - 1] == '/') {
        std::filesystem::create_directories(fullPath);
    } else {
        std::filesystem::create_directories(std::filesystem::path(fullPath).parent_path());
        if (unzOpenCurrentFile(zip) != UNZ_OK) {
            std::cerr << "Failed to open file inside zip: " << filename << std::endl;
            return -1;
        }
        FILE* outFile = fopen(fullPath.c_str(), "wb");
        if (!outFile) {
            std::cerr << "Failed to create output file: " << fullPath << std::endl;
            unzCloseCurrentFile(zip);
            return -1;
        }
        int bytesRead;
        while ((bytesRead = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0) {
            fwrite(buffer, 1, bytesRead, outFile);
        }
        fclose(outFile);
        unzCloseCurrentFile(zip);
    }
    printZipProgressBar(current, total);
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
    do {
        ++fileCount;
    } while (unzGoToNextFile(zip) == UNZ_OK);
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
    std::cout << "\n";
    return true;
}

// MOVE MANAGEMENT

std::string removeFilenameExtension(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) {
        return filename;  // No extension found
    }
    return filename.substr(0, dotPos);
}


void replaceInFile(const std::filesystem::path& filePath, const std::string& target, const std::string& replacement) {
    std::ifstream inFile(filePath);
    if (!inFile) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    size_t pos = 0;
    while ((pos = content.find(target, pos)) != std::string::npos) {
        content.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }

    std::ofstream outFile(filePath);
    if (!outFile) {
        std::cerr << "Failed to write to file: " << filePath << std::endl;
        return;
    }

    outFile << content;
    outFile.close();
}

bool updateMetaKernelPaths(const std::filesystem::path& mkDir, const std::filesystem::path& replacementPath) {
    if (!std::filesystem::exists(mkDir) || !std::filesystem::is_directory(mkDir)) {
        std::cerr << "Invalid directory: " << mkDir << std::endl;
        return false;
    }

    std::string replacement = replacementPath.string();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(mkDir)) {
        if (std::filesystem::is_regular_file(entry.path())) {
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
        // Ensure source exists
        if (!std::filesystem::exists(source) || !std::filesystem::is_directory(source)) {
            std::cerr << "Error: Source directory does not exist or is not a directory." << std::endl;
            return false;
        }

        // Remove target if it exists
        if (std::filesystem::exists(target)) {
            std::filesystem::remove_all(target);
        }

        // Move source to target location
        std::filesystem::rename(source, target);
        std::cout << "Replaced " << target << " with " << source << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

// DATA_MANGER

std::string DataManager::getLocalVersion() {
    return localVersion;
}

std::string DataManager::getRemoteVersion() {
    return downloadUrlContent(versionUrl);
}

DataManager::DataManager() {
    zipUrl = REMOTE_ARCHIVE_URL;
    versionUrl = REMOTE_VERSION_URL;

    localVersion = "";
    
    exeDirectory = std::filesystem::path(getDefaultSaveDir());
    dataDirectory = exeDirectory / "data";
    heraDirectory = dataDirectory / "hera";
    heraTemporaryDirectory = dataDirectory / "HERA";

    kernelDirectory = heraDirectory / "kernels";
    tempMetaKernelDirectory = heraTemporaryDirectory / "kernels" / "mk";

    zipFile = dataDirectory / std::filesystem::path(getFilenameFromUrl(zipUrl));

}

bool DataManager::isNewVersionAvailable() {
    std::string remoteVersion = getRemoteVersion();
    if (localVersion != remoteVersion) {
        localVersion = remoteVersion;
        return true;
    }
    return false;
}

bool DataManager::downloadZipFile() {
    return downloadFile(zipUrl, dataDirectory);
}

bool DataManager::unzipZipFile() {
    return unzipRecursive(zipFile, dataDirectory);
}

bool DataManager::editTempMetaKernelFiles() {
    return false;
    return updateMetaKernelPaths(tempMetaKernelDirectory, kernelDirectory);    
}

bool DataManager::moveFolder() {
    return replaceDirectory(heraTemporaryDirectory, heraDirectory);
}

bool DataManager::deleteZipFile() {
    if (!std::filesystem::exists(zipFile) || !std::filesystem::is_regular_file(zipFile)) {
        std::cerr << "File not found: " << zipFile << std::endl;
        return false;
    }
    if (std::filesystem::remove(zipFile)) {
        std::cout << "Deleted: " << zipFile << std::endl;
        return true;
    }
    else {
        std::cerr << "Failed to delete: " << zipFile << std::endl;
        return false;
    }
}