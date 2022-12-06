#include <torch/torch.h>

struct NetImpl : torch::nn::Module {
    NetImpl(int64_t N, int64_t M)
        : linear(register_module("linear", torch::nn::Linear(N, M))) {
            bias = register_parameter("b", torch::randn(M));
        }
    torch::Tensor forward(torch::Tensor input) {
        return linear(input) + bias;
    }
    private:
        torch::nn::Linear linear;
        torch::Tensor bias;
};
TORCH_MODULE(Net);
