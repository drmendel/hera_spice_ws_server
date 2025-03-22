// CPP
#include <iostream>           // Used for console input/output (std::cout, std::cerr)

// EXTERNAL
#include <cspice/SpiceUsr.h>  // CSPICE library for space navigation computations (SpiceDouble)
#include <uWebSockets/App.h>  // uWebSockets for handling WebSockets and HTTP requests (uWS::App)
#include <minizip/unzip.h>    // minizip for handling ZIP file extraction (unzFile, unzOpen, unzReadCurrentFile)
#include <curl/curl.h>        // libcurl for network requests (curl_easy_init, curl_easy_cleanup)

// PROJECT
#include <data_manager.hpp>   // Manage remote version updates from https://spiftp.esac.esa.int/data/SPICE/HERA/
#include <spice_core.hpp>     // Spice request handler



// ###################### DATAMANAGER TEST ###################### 
/*
int main() {
    DataManager DataManager;

    std::cout << (DataManager.isNewVersionAvailable() ? "New Version Available!" : "No New Version") << std::endl;
    std::cout << (DataManager.isNewVersionAvailable() ? "New Version Available!" : "No New Version") << std::endl;
    DataManager.downloadZipFile();
    DataManager.unzipZipFile();
    DataManager.deleteZipFile();
    DataManager.editTempMetaKernelFiles();
    DataManager.moveFolder();

    return 0;
}*/


// ###################### SPICE_CORE TEST ######################
/*
int main() {
    initSpiceCore();
    SpiceDouble et;
    
    int32_t unixTimestamp = 1710840000 + 365*24*60*60; // 2025-03-19T12:00:00 UTC
    str2et_c("2025-03-19T12:00:00", &et);
    uint8_t mode = (uint8_t)MessageMode::ALL_INSTANTANEOUS;               // Example mode
    int32_t id = -91110;               // Example ID

    std::string binaryMessage = createMessage(unixTimestamp, mode, id);
    std::cout << "Binary message human readable:\n";
    const char* c = binaryMessage.c_str();
    uint32_t uTm;
    memcpy(&uTm, c, sizeof(uint32_t));
    uint8_t md = (uint8_t)c[4];
    int32_t i;
    memcpy(&i, c + sizeof(uint32_t) + sizeof(uint8_t), sizeof(int32_t));
    std::cout << uTm << ", " << (int)md << ", " << i << std::endl;
    printHex(binaryMessage);
    Request rq(binaryMessage);
    printResponse(rq.getMessage());
    return 0;
}*/













































/*
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <curl/curl.h>
#include <cspice/SpiceUsr.h>

#include <websocket_manager.hpp>
#include <data_manager.hpp>



namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

std::atomic<bool> keepRunning(true);

class WebSocketServer {
public:
    WebSocketServer(asio::io_context& ioc, short port, const std::string& message)
        : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)), socket_(ioc), messageToSend_(message) {
        acceptConnection();
    }

private:
    void acceptConnection() {
        if (!keepRunning.load()) return;

        acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket_), messageToSend_)->start();
            }
            acceptConnection();
        });
    }

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, const std::string& message)
            : ws_(std::move(socket)), message_(message) {}

        void start() {
            ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
                if (!ec) self->sendMessage();
            });
        }

    private:
        void sendMessage() {
            std::cout << "Sending message: " << message_ << std::endl; // Print when data is sent
            ws_.async_write(asio::buffer(message_), [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Delay before closing
                    self->closeWebSocket();
                }
            });
        }

        void closeWebSocket() {
            ws_.async_close(beast::websocket::close_code::normal, [self = shared_from_this()](boost::system::error_code ec) {
                if (ec) {
                    std::cerr << "Server WebSocket Close Error: " << ec.message() << std::endl;
                }
            });
        }

        beast::websocket::stream<tcp::socket> ws_;
        std::string message_;
    };

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::string messageToSend_;
};

void connectToServer(const std::string& host, short port) {
    try {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        auto results = resolver.resolve(host, std::to_string(port));

        beast::websocket::stream<tcp::socket> ws(ioc);
        asio::connect(ws.next_layer(), results.begin(), results.end());
        ws.handshake(host, "/");

        beast::flat_buffer buffer;
        ws.read(buffer);

        std::cout << "Received: " << beast::buffers_to_string(buffer.data()) << std::endl;

        // Safe WebSocket shutdown
        boost::system::error_code ec;
        ws.close(beast::websocket::close_code::normal, ec);
        if (ec && ec != beast::websocket::error::closed) { // Ignore normal closure
            std::cerr << "WebSocket Close Error: " << ec.message() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Client Error: " << e.what() << std::endl;
    }
}

void waitForKeyPress() {
    std::cout << "Press Enter to exit...\n";
    std::cin.get();
    keepRunning = false;
}



int main() {
    
    std::cout << "Hello World\n\n";
    websocketHeaderTest();
    SpiceInt number = 1;
    if(number) std::cout << "SpiceUsr header is available!\n\n";
    std::string message = "Send from port A to port B";
    short port = 8080;
    asio::io_context ioc;
    WebSocketServer server(ioc, port, message);
    std::thread serverThread([&ioc]() {
        ioc.run();
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Give server time to start
    connectToServer("localhost", port);
    std::cout << "Boost:Beast header is available!\n\n";
    waitForKeyPress();
    ioc.stop();
    serverThread.join(); 
    std::cout << "Program exited successfully.\n";
    
    return 0;
}*/

























/*
#include <uWebSockets/App.h>
#include <iostream>
#include <atomic>
#include <csignal>

// Flag to control shutdown
std::atomic<bool> staySilent = false;
struct us_listen_socket_t *listenSocket = nullptr;

// Struct for WebSocket per-connection data
struct PerSocketData {};

// WebSocket event handlers
void onOpen(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws) {
    std::cout << "Client connected!" << std::endl;
}

void onMessage(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws, std::string_view message, uWS::OpCode opCode) {
    if(!staySilent) ws->send(message, opCode);  // Echo back the message
}

void onClose(uWS::WebSocket<false, uWS::SERVER, PerSocketData> *ws, int code, std::string_view message) {
    std::cout << "Client disconnected! Code: " << code << ", Message: " << message << std::endl;
}

// Function to start the WebSocket server
void startServer() {
    uWS::App app;

    app.ws<PerSocketData>("/", {
        .open = onOpen,
        .message = onMessage,
        .close = onClose
    }).listen(9001, [](auto *token) {
        if (token) {
            listenSocket = token;
            std::cout << "Server listening on port 9001" << std::endl;
        } else {
            std::cerr << "Failed to listen on port 9001" << std::endl;
            exit(1);
        }
    });

    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    app.run();
}

int main() {
    staySilent = false;
    std::thread wsThread(startServer);
    wsThread.detach();
    std::cout << "Server running!\n\nPress ENTER to make the server silent.\n";
    std::cin.get();
    staySilent = true;
    std::cout << "Server silent.\n";
    std::cout << "Press ENTER to make the server silent.\n";
    std::cin.get();
    staySilent = false;
    std::cout << "Server silent.\n";
    std::cout << "Press ENTER to stop the server.\n";
    std::cin.get();
    std::cout << "Stopping on return.\n";
    return 0;
}
*/






/*
#include <uWebSockets/App.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
struct PerSocketData;
std::atomic<bool> running(true);
struct us_listen_socket_t *listenSocket = nullptr;
std::vector<uWS::WebSocket<false, true, PerSocketData>*> clients;
std::mutex clientsMutex;

struct PerSocketData {};  // Empty data for WebSocket connections

int main() {
    std::thread inputThread([&]() {
        std::cin.get();  // Wait for Enter key
        running = false;

        // Close all WebSocket connections
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto* ws : clients) {
                ws->close();
            }
            clients.clear();
        }

        if (listenSocket) {
            us_listen_socket_close(0, listenSocket);
        }

        uWS::Loop::get()->defer([]() {
            std::exit(0);
        });

        std::exit(0);  // Ensure exit
    });

    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto* ws) {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(ws);
        },
        .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode);  // Echo message back
        },
        .close = [](auto* ws, int, std::string_view) {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.erase(std::remove(clients.begin(), clients.end(), ws), clients.end());
        }
    }).listen(3000, [&](auto *token) {
        if (token) {
            listenSocket = token;
            std::cout << "WebSocket server running on ws://localhost:3000. Press Enter to stop.\n";
        } else {
            std::cout << "Failed to start server.\n";
        }
    }).run();  // Start event loop

    inputThread.join();
    return 0;
}
*/


















/*
#include <uWebSockets/App.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

std::atomic<bool> running(true);
struct us_listen_socket_t *listenSocket = nullptr;

struct PerSocketData {};  // Empty data for WebSocket connections

int main() {
    std::thread inputThread([&]() {
        std::cin.get();  // Wait for Enter key
        running = false;

        if (listenSocket) {
            us_listen_socket_close(0, listenSocket);
        }

        uWS::Loop::get()->defer([]() {
            std::exit(0);
        });
    });

    uWS::App().ws<PerSocketData>("/*", {
        .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received: " << message << "\n";
            ws->send(message, opCode);  // Echo back the message
        }
    }).listen(3000, [&](auto *token) {
        if (token) {
            listenSocket = token;
            std::cout << "WebSocket server running on ws://localhost:3000. Press Enter to stop.\n";
        } else {
            std::cout << "Failed to start server.\n";
        }
    }).run();  // Start the event loop

    inputThread.join();
    std::cout << "Server stopped.\n";
    return 0;
}
*/














/*
#include <uWebSockets/App.h>
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> running = true;
struct PerSocketData
{};

int main() {
    struct us_listen_socket_t* listenSocket = nullptr;

    std::thread inputThread([&]() {
        std::cin.get();
        running = false;
        if (listenSocket) {
            us_listen_socket_close(0, listenSocket);
        }
    });

    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            std::cout << "Client connected\n";
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << "\n";
            ws->send(message, opCode); // Echo back the message
        },
        .close = [](auto *ws, int code, std::string_view message) {
            std::cout << "Client disconnected\n";
        }
    }).listen(3000, [&](auto *token) {
        if (token) {
            listenSocket = token;
            std::cout << "WebSocket server running on port 3000. Press Enter to stop.\n";
        } else {
            std::cout << "Failed to start server.\n";
        }
    }).run();

    inputThread.join();
    std::cout << "Server stopped.\n";
    return 0;
}
*/


/*
#include <uWebSockets/App.h>
#include <iostream>
#include <atomic>

std::atomic<bool> serverRun = true;
struct PerSocketData {};  // Required for WebSocket behavior


int main() {
    int messageCount = 0;
    uWS::App().ws<PerSocketData>("/", {
        .message = [&messageCount](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received: " << message << std::endl;
            messageCount++;
            
            if (messageCount >= 2) {
                std::cout << "Received second message, shutting down!" << std::endl;
                uWS::Loop::get()->defer([]() { std::exit(0); });  // Use Loop::get() instead of ws->getLoop()
            }
        }
    }).listen(9001, [](auto *listenSocket) {
        if (listenSocket) {
            std::cout << "Server running on port 9001" << std::endl;
        }
    }).run();
    
    return 0;
}
*/





/*
#include <uWebSockets/App.h>
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> running{true};

struct UserData
{
};


void websocketServer(int port) {
    uWS::App().ws<UserData>("/*", {
        .open = [](auto *ws) {
            std::cout << "Client connected\n";
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            ws->send(message, opCode); // Echo back
        },
        .close = [](auto *ws, int code, std::string_view message) {
            std::cout << "Client disconnected\n";
        }
    }).listen(port, [port](auto *token) {
        if (token) {
            std::cout << "WebSocket server listening on port " << port << "\n";
        }
    }).run();
}

int main() {
    int port = 9001;
    std::thread wsThread([&]() {
        websocketServer(port);
    });

    std::cout << "Press Enter to stop server...\n";
    std::cin.get(); // Wait for user input

    running = false; // Stop flag

    // Wait for the server thread to finish
    wsThread.~thread();

    std::cout << "Server stopped.\n";
    return 0;
}*/



// dataManager thread test
/*
#include <atomic>

#include <server_threads.hpp>

int main(void) {
    shouldDataManagerRun.store(true);
    std::thread dataManagerThread(dataManagerWorker);
    std::cin.get();
    shouldDataManagerRun.store(false);
    versionCondition.notify_all();
    dataManagerThread.join();
    return 0;
}
*/


// websocketManager thread test
/*
#include <spice_core.hpp>
#include <websocket_manager.hpp>
#include <server_threads.hpp>

int main(void) {
    initSpiceCore();
    
    int port = 9001;
    signalSpiceDataUnavailable();
    std::thread webSocketThread(webSocketManagerWorker, port);
    webSocketThread.detach();
    
    std::string tmp;
    std::cout << "Type something to signalSpiceAvailable ...\n";
    std::cin >> tmp;
    signalSpiceDataAvailable();
    
    std::cout << "Type something to make server echo (spice not available)...\n";
    std::cin >> tmp;
    signalSpiceDataUnavailable();
    
    std::cout << "Press Enter to make server work again ...\n";
    std::cin >> tmp;
    signalSpiceDataAvailable();
    
    std::cout << "Press Enter to stop server...\n";
    std::cin >> tmp;
    deinitSpiceCore();
    return 0;
}*/







#include <server_threads.hpp>

#include <iostream>
#include <string>
#include <atomic>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <interval>\n";
        std::cerr << "<port>    - The port number to run the server on.\n";
        std::cerr << "<interval> - The interval (in hours) between kernel version checks.\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    int hoursToWait;
    {
        int tmp = std::stoi(argv[2]);
        hoursToWait = tmp > 0 ? tmp : 1;
    }
    
    shouldDataManagerRun.store(true);
    std::thread dataManagerThread(dataManagerWorker, hoursToWait);
    std::thread webSocketManagerThread(webSocketManagerWorker, port);
    webSocketManagerThread.detach();

    std::string command = "";
    while (command != "exit") {
        std::cout << "Type in \"exit\" to stop the program!" << std::endl;
        std::cin >> command;
    }

    shouldDataManagerRun.store(false);
    versionCondition.notify_all();
    dataManagerThread.join();

    return 0;
}
