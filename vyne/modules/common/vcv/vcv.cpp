#include "vcv.h"
#ifdef HAS_OPENCV
#include <opencv2/opencv.hpp>

namespace VCVNative {
    Value show(std::vector<Value>& args) {
        std::cout << "[DEBUG] vcv.show funksiyasına girildi!" << std::endl;
        if (args.empty()) return Value(false);

        std::string path = args[0].asString();
        std::cout << "[DEBUG] Oxunacaq fayl: " << path << std::endl;

        cv::Mat image = cv::imread(path);
        if (image.empty()) {
            std::cout << "[DEBUG] Şəkil tapılmadı və ya oxunmadı!" << std::endl;
            return Value(false);
        }

        cv::imshow("Vyne Vision", image);
        std::cout << "[DEBUG] imshow çağırıldı, waitKey gözləyir..." << std::endl;
        cv::waitKey(0); 
        return Value(true);
    }
}
#endif

void setupVCV(SymbolContainer& env, StringPool& pool) {
    auto& vcv = env["vcv"];

#ifdef HAS_OPENCV
    std::cout << "[DEBUG] VCV Modulu HAS_OPENCV ilə yüklənir..." << std::endl;
    vcv[pool.intern("show")] = Value(VCVNative::show);
#else
    std::cout << "[DEBUG] VCV Modulu OpenCV OLMADAN yüklənir!" << std::endl;
    vcv[pool.intern("show")] = Value([](std::vector<Value>& args) -> Value {
        throw std::runtime_error("Vyne Error: vcv.show() is disabled. OpenCV not found during build.");
    });
#endif
}