#include <string>
#include <iostream>
#include <spdlog/spdlog.h>

#include <torch/torch.h>
#include <torch/script.h> 

extern int load(std::string modelPath)
{
    using namespace torch;
    using namespace spdlog;

    set_level(level::debug);

    if (modelPath.length() == 0)
    {
        error("No model path provided");
        return 1;
    }

    Device device = kCPU;
    if (torch::cuda::is_available()) {
        info("CUDA is available, using GPU");
        device = kCUDA;
    }

    torch::jit::script::Module module;
    try
    {
        module = torch::jit::load(modelPath);
    }
    catch (const c10::Error &e)
    {
        error("Failed loading the model, reason: {}", e.what());
        return 1;
    }
    info("Model loaded");
    return 0;
}
