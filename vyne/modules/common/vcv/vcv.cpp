#include "vcv.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib> // system()
#include <cstdint> // intptr_t

#include "../../../../vendor/stb/stb_image.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#ifndef SW_SHOWNORMAL
    #define SW_SHOWNORMAL 1
#endif
#endif

namespace VCVNative {
    Value show(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::string path = args[0].asString();

        std::cout << "[VCV] Opening image: " << path << "\n";

#ifdef _WIN32
        HINSTANCE res = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)res <= 32) {
            std::cerr << "[VCV] Error: Could not open image file at " << path << "\n";
            return Value(false);
        }
#elif defined(__linux__)
        std::string cmd = "xdg-open " + path + " > /dev/null 2>&1";
        [[maybe_unused]] int ret = std::system(cmd.c_str());
#endif
        return Value(true);
    }

    Value info(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        std::string path = args[0].asString();

        int width, height, channels;
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (data) {
            std::cout << "[VCV] Info: " << width << "x" << height << " (" << channels << " channels)" << std::endl;
            stbi_image_free(data);

            std::vector<Value> channelData;
            channelData.emplace_back(Value((double)width));
            channelData.emplace_back(Value((double)height));
            channelData.emplace_back(Value((double)channels));

            return Value(channelData);
        }

        std::cerr << "[VCV] Error: Could not load image info for " << path << std::endl;
        return Value(false);
    }
}

void setupVCV(SymbolContainer& env, StringPool& pool) {
    auto& vcv = env["vcv"];
    vcv[pool.intern("show")] = Value(VCVNative::show);
    vcv[pool.intern("info")] = Value(VCVNative::info);
}