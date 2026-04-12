#include "vcv.h"
#ifdef HAS_OPENCV
#include <opencv2/opencv.hpp>

namespace VCVNative {
    Value show(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("vcv.show() requires an image path");

        std::string path = args[0].asString();
        
        cv::Mat image = cv::imread(path);

        if (image.empty()) {
            throw std::runtime_error("Runtime Error: Could not open or find the image at " + path);
        }

        cv::imshow("Vyne Vision - " + path, image);
        
        cv::waitKey(0); 
        
        cv::destroyAllWindows();

        return Value(true);
    }
}
#endif

void setupVCV(SymbolContainer& env, StringPool& pool) {
    auto& vcv = env["vcv"];
#ifdef HAS_OPENCV
    vcv[pool.intern("show")] = Value(VCVNative::show);
#else
    throw std::runtime_error("Library Error : OPEN_CV is not installed.");
#endif
}