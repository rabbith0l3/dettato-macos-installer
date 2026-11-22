#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <unistd.h>

int main()
{
    if (const char* logPath = std::getenv("FAKE_ENGINE_PID_LOG"))
    {
        std::ofstream log(logPath, std::ios::app);
        log << ::getpid() << "\n";
    }
    const char* delayText = std::getenv("FAKE_ENGINE_DELAY_SECONDS");
    const int delay = delayText == nullptr ? 0 : std::atoi(delayText);
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::seconds(delay));

        std::smatch idMatch;
        std::smatch outputMatch;
        const std::regex idPattern("\\\"id\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
        const std::regex outputPattern("\\\"output\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
        if (!std::regex_search(line, idMatch, idPattern) || !std::regex_search(line, outputMatch, outputPattern))
        {
            std::cout << "{\"id\":null,\"status\":\"failed\",\"error\":\"bad fake request\"}" << std::endl;
            continue;
        }
        const std::string id = idMatch[1];
        const std::string output = outputMatch[1];
        if (std::getenv("FAKE_ENGINE_MALFORMED") != nullptr)
        {
            std::cout << "this is not JSON" << std::endl;
            continue;
        }
        const unsigned char midi[] = {
            0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xe0,
            0x4d, 0x54, 0x72, 0x6b, 0, 0, 0, 13,
            0, 0x90, 0x3c, 0x64, 0x83, 0x60, 0x80, 0x3c, 0x40, 0, 0xff, 0x2f, 0
        };
        std::ofstream stream(output, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(midi), sizeof(midi));
        stream.close();
        std::cout << "{\"id\":\"" << id << "\",\"status\":\"succeeded\",\"midi\":\"" << output
                  << "\",\"pid\":" << ::getpid() << ",\"summary\":{\"tracks\":[]}}" << std::endl;
    }
}
