#include <gtest/gtest.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include "../server/tcp_server.hpp" // Adjust path as needed

class RedisIntegrationTest : public ::testing::Test {
protected:
    const std::string IP = "127.0.0.1";
    const int PORT = 6379;
    std::unique_ptr<Redis::TCPServer> server;
    std::thread server_thread;

    void SetUp() override {
        server = std::make_unique<Redis::TCPServer>(IP, PORT);
        // We start the server in a separate thread.
        // Note: You might need to modify TCPServer::start() to not block on std::cin 
        // during automated tests.
        server_thread = std::thread([this]() {
            server->start();
        });
        
        // Give the server a moment to bind to the port
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server->stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    // Helper to send a raw string and get the response
    std::string send_command(const std::string& command) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, IP.c_str(), &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            return "CONNECTION_FAILED";
        }

        send(sock, command.c_str(), command.length(), 0);

        char buffer[1024] = {0};
        read(sock, buffer, 1024);
        close(sock);
        return std::string(buffer);
    }
};

// 1. Test RPUSH on a new key (Returns :1)
TEST_F(RedisIntegrationTest, RPushSingleElement) {
    // RESP for: RPUSH mylist "hello"
    std::string cmd = "*3\r\n$5\r\nRPUSH\r\n$6\r\nmylist\r\n$5\r\nhello\r\n";
    std::string response = send_command(cmd);
    
    // Expecting an integer response :1
    EXPECT_EQ(response, ":1\r\n");
}

// 2. Test RPUSH with multiple elements (Returns :3)
TEST_F(RedisIntegrationTest, RPushMultipleElements) {
    // RPUSH mylist "a" "b" "c"
    std::string cmd = "*5\r\n$5\r\nRPUSH\r\n$6\r\nmylist\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n";
    std::string response = send_command(cmd);
    
    EXPECT_EQ(response, ":3\r\n");
}

// 3. Test Type Mismatch (RPUSH to a key that is a String)
TEST_F(RedisIntegrationTest, RPushWrongType) {
    // First, SET the key to a string
    send_command("*3\r\n$3\r\nSET\r\n$4\r\ntest\r\n$3\r\nval\r\n");
    
    // Now try to RPUSH to it
    std::string cmd = "*3\r\n$5\r\nRPUSH\r\n$4\r\ntest\r\n$1\r\na\r\n";
    std::string response = send_command(cmd);
    
    // Expecting the error message defined in your handle_rpush
    EXPECT_TRUE(response.find("KEY HOLDING WRONG TYPE VALUE") != std::string::npos);
}

// 4. Test missing arguments
TEST_F(RedisIntegrationTest, RPushMissingArgs) {
    // RPUSH with only one argument (missing value)
    std::string cmd = "*2\r\n$5\r\nRPUSH\r\n$6\r\nmylist\r\n";
    std::string response = send_command(cmd);
    
    EXPECT_TRUE(response.find("Wrong number of arguments") != std::string::npos);
}