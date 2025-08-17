#include "image_data.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

ImageDiskReader::ImageDiskReader(const std::string& source_file_path)
: source_file_path(source_file_path)
{}

std::unique_ptr<ImageData> ImageDiskReader::read_image_data(std::vector<std::string>& read_errors)
{
    std::unique_ptr<ImageData> data = std::make_unique<ImageData>();

    const char* file_path = source_file_path.c_str();
    stbi_info(file_path, &data->width, &data->height, &data->channels);
    if (stbi_failure_reason())
    {
        std::string check_err = stbi_failure_reason();
        read_errors.push_back(check_err);
        return data;
    }
    data->source_file_path = source_file_path;
    // do not filter any channels
    int channel_filter = 0;
    unsigned char* raw_data = stbi_load(file_path, &data->width, &data->height, &data->channels, channel_filter);
    if (raw_data == nullptr && stbi_failure_reason())
    {
        read_errors.push_back(stbi_failure_reason());
        return data;
    }
    data->size_bytes = data->width * data->height * data->channels;
    // own the raw memory of image
    data->raw_data = std::unique_ptr<unsigned char[]>(raw_data);

    return data;
}

ImageDataTransferer::ImageDataTransferer(std::unique_ptr<ImageData> data)
{
    this->data = std::move(data);
}

std::unique_ptr<ImageData> ImageDataTransferer::read_image_data(std::vector<std::string>& read_errors)
{
    std::unique_ptr<ImageData> passing_data = std::move(data);
    return passing_data;
}

ImageDataResizer::ImageDataResizer(int resize_width, int resize_height)
: resize_width(resize_width), resize_height(resize_height)
{}

std::unique_ptr<ImageData> ImageDataResizer::resize_image_data(const ImageData& in_image_data, std::vector<std::string> resize_errors)
{
    std::unique_ptr<ImageData> resized_data = std::make_unique<ImageData>();

    if (resize_width < 1 || resize_width >= in_image_data.width)
    {
        std::string err = "Failed resizing image: " + in_image_data.source_file_path
            + ", reason: resize width [" + std::to_string(resize_width)
            + "] is invalid, original width [" + std::to_string(in_image_data.width) +
            + "]";
        resize_errors.push_back(err);
    }
    if (resize_height < 1 || resize_height >= in_image_data.height)
    {
        std::string err = "Failed resizing image: " + in_image_data.source_file_path
            + ", reason: resize height [" + std::to_string(resize_height)
            + "] is invalid, original height [" + std::to_string(in_image_data.height)
            + "]";
        resize_errors.push_back(err);
    }
    if (resize_errors.size() > 0)
    {
        return resized_data;
    }
    unsigned char* resized_image_data = (unsigned char*)malloc(resize_width * resize_height * in_image_data.channels);
    if (!resized_image_data) {
        std::string err = "Failed resizing image: " + in_image_data.source_file_path + ", reason: failed memory allocation";
        resize_errors.push_back(err);
        return resized_data;
    }
    int calc_stride = 0;
    int val = stbir_resize_uint8(
        in_image_data.raw_data.get(), 
        in_image_data.width, 
        in_image_data.height,
        calc_stride,
        resized_image_data,
        resize_width,
        resize_height,
        calc_stride,
        in_image_data.channels);
    if (val != 1)
    {
        resize_errors.push_back(stbi_failure_reason());
        return resized_data;
    }
    resized_data->source_file_path = in_image_data.source_file_path;
    resized_data->width = resize_width;
    resized_data->height = resize_height;
    resized_data->channels = in_image_data.channels;
    resized_data->size_bytes = resized_data->width * resized_data->height * resized_data->channels;
    resized_data->raw_data = std::unique_ptr<unsigned char[]>(resized_image_data);
 
    return resized_data;
}
