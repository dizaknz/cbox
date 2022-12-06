#include <iostream>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "dcgan.hpp"

int main(int argc, char ** argv)
{
    CLI::App app{"Torch Training Demo"};

    std::string datasetPath;
    int loaders = 2;
    int batchSize = 512;
    int noiseSize = 100;
    int epochs = 10;
    app.add_option("-d", datasetPath, "Path to downloaded dataset");
    app.add_option("-l", loaders, "Number of workers to use for dataset loading");
    app.add_option("-b", batchSize, "Batch size to use for dataset loading");
    app.add_option("-n", noiseSize, "Input noise size");
    app.add_option("-e", epochs, "Number of epochs to use in training loop");

    CLI11_PARSE(app, argc, argv);

    using namespace torch;
    using namespace spdlog;

    set_level(level::debug);

    if (datasetPath.length() == 0)
    {
        error("No data set path provided");
        return 1;
    }

    Device device = kCPU;
    if (torch::cuda::is_available()) {
        info("CUDA is available, using GPU");
        device = kCUDA;
    }

    GanOptions opts = {
        { 
            nn::ConvTranspose2dOptions(noiseSize, 256, 4),
            nn::ConvTranspose2dOptions(256, 128, 3),
            nn::ConvTranspose2dOptions(128, 64, 4),
            nn::ConvTranspose2dOptions(64, 1, 4)
        },
        {
            256,
            128,
            64
        },
        2,
        1,
        false
    };

    info("Creating GAN generator");
    GanGenerator generator(opts);

    info("Creating GAN discriminator");
    nn::Sequential discriminator(
        nn::Conv2d(
            nn::Conv2dOptions(1, 64, 4).stride(2).padding(1).bias(false)),
        nn::LeakyReLU(nn::LeakyReLUOptions().negative_slope(0.2)),
        nn::Conv2d(
            nn::Conv2dOptions(64, 128, 4).stride(2).padding(1).bias(false)),
        nn::BatchNorm2d(128),
        nn::LeakyReLU(nn::LeakyReLUOptions().negative_slope(0.2)),
        nn::Conv2d(
            nn::Conv2dOptions(128, 256, 4).stride(2).padding(1).bias(false)),
        nn::BatchNorm2d(256),
        nn::LeakyReLU(nn::LeakyReLUOptions().negative_slope(0.2)),
        nn::Conv2d(
            nn::Conv2dOptions(256, 1, 3).stride(1).padding(0).bias(false)),
        nn::Sigmoid());

    auto dataset = data::datasets::MNIST(datasetPath)
        .map(data::transforms::Normalize<>(0.5, 0.5))
        .map(data::transforms::Stack<>());
    const int64_t batchesPerEpoch =
      std::ceil(dataset.size().value() / static_cast<double>(batchSize));

    info("Loading dataset using batch size of {}", batchSize);
    auto dataLoader = data::make_data_loader(
        std::move(dataset),
        data::DataLoaderOptions().batch_size(batchSize).workers(loaders));

    info("Optimizing");
    optim::Adam generatorOptimizer(
        generator->parameters(), optim::AdamOptions(2e-4)
        .betas(std::make_tuple (0.5, 0.5)));
    optim::Adam discriminatorOptimizer(
        discriminator->parameters(), optim::AdamOptions(5e-4)
        .betas(std::make_tuple (0.5, 0.5)));

    generator->to(device);
    discriminator->to(device);

    info("Training");
    for (int64_t epoch = 1; epoch <= epochs; ++epoch)
    {
        int64_t batchIdx = 0;
        for (data::Example<> &batch : *dataLoader)
        {
            // real images.
            discriminator->zero_grad();
            Tensor realImages = batch.data.to(device);
            Tensor realLabels = torch::empty(batch.data.size(0), device).uniform_(0.8, 1.0);
            Tensor realOutput = discriminator->forward(realImages);
            Tensor realLoss = binary_cross_entropy(realOutput, realLabels);
            realLoss.backward();

            // fake images.
            Tensor noise = torch::randn({batch.data.size(0), noiseSize, 1, 1}, device);
            Tensor fakeImages = generator->forward(noise);
            Tensor fakeLabels = torch::zeros(batch.data.size(0), device);
            Tensor fakeOutput = discriminator->forward(fakeImages.detach());
            Tensor fakeLoss = binary_cross_entropy(fakeOutput, fakeLabels);
            fakeLoss.backward();

            Tensor loss = realLoss + fakeLoss;
            discriminatorOptimizer.step();

            // train generator
            generator->zero_grad();
            fakeLabels.fill_(1);
            fakeOutput = discriminator->forward(fakeImages);
            Tensor generatedLoss = binary_cross_entropy(fakeOutput, fakeLabels);
            generatedLoss.backward();
            generatorOptimizer.step();

            std::printf(
                "\r[%2ld/%2ld][%3ld/%3ld] D_loss: %.4f | G_loss: %.4f",
                epoch,
                epochs,
                ++batchIdx,
                batchesPerEpoch,
                loss.item<float>(),
                generatedLoss.item<float>());
        }

        save(generator, "generator-checkpoint.pt");
        save(generatorOptimizer, "generator-optimizer-checkpoint.pt");
        save(discriminator, "discriminator-checkpoint.pt");
        save(discriminatorOptimizer, "discriminator-optimizer-checkpoint.pt");
        Tensor samples = generator->forward(torch::randn({9, noiseSize, 1, 1}, device));
        save((samples + 1.0) / 2.0, torch::str("sample-", epoch, ".pt"));
        info("Checkpoint done");
    }

    return 0;
}
