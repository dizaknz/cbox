#include <torch/torch.h>

struct GanOptions
{
    torch::nn::ConvTranspose2dOptions convolutions[4];
    int batchSizes[3];
    int stride;
    int padding;
    bool bias;
};

struct GanGeneratorImpl : torch::nn::Module
{
    GanGeneratorImpl(GanOptions &options)
        : conv1(options.convolutions[0].bias(options.bias)),
          batchNorm1(options.batchSizes[0]),
          conv2(options.convolutions[1].stride(options.stride).padding(options.padding).bias(options.bias)),
          batchNorm2(options.batchSizes[1]),
          conv3(options.convolutions[2].stride(options.stride).padding(options.padding).bias(options.bias)),
          batchNorm3(options.batchSizes[2]),
          conv4(options.convolutions[3].stride(options.stride).padding(options.padding).bias(options.bias))
    {
        register_module("conv1", conv1);
        register_module("conv2", conv2);
        register_module("conv3", conv3);
        register_module("conv4", conv4);
        register_module("batchNorm1", batchNorm1);
        register_module("batchNorm2", batchNorm2);
        register_module("batchNorm3", batchNorm3);
    }

    torch::Tensor forward(torch::Tensor x)
    {
        x = torch::relu(batchNorm1(conv1(x)));
        x = torch::relu(batchNorm2(conv2(x)));
        x = torch::relu(batchNorm3(conv3(x)));
        x = torch::tanh(conv4(x));
        return x;
    }
    private:
        torch::nn::ConvTranspose2d conv1, conv2, conv3, conv4;
        torch::nn::BatchNorm2d batchNorm1, batchNorm2, batchNorm3;
};
TORCH_MODULE(GanGenerator);