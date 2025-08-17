#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

constexpr int UNSET_WIDTH = -1;
constexpr int UNSET_HEIGHT = -1;
constexpr int UNSET_CHANNELS = -1;

/**
 * @brief Define the properties of an image
 */
struct ImageData
{
    std::string source_file_path;
    int width = UNSET_WIDTH;
    int height = UNSET_HEIGHT;
    int channels = UNSET_CHANNELS;
    bool is_original_size = true;
    int size_bytes = 0;
    std::unique_ptr<unsigned char[]> raw_data;

    int key()
    {
        std::size_t name_hash = std::hash<std::string>{}(source_file_path);
        if (!is_original_size && width != UNSET_WIDTH && height != UNSET_HEIGHT)
        {
            size_t size_hash = std::hash<long>{}(width*height);

            return name_hash ^ (size_hash + 0x9e3779b9 + (name_hash << 6) + (name_hash >> 2));
        }

        // default to only name hash for original image size
        return name_hash;
    }

    ImageData() = default;
    ImageData(const std::string source_file_path, int width, int height, bool is_original_size)
    : source_file_path(source_file_path),
      width(width),
      height(height),
      is_original_size(is_original_size)
    {}
};

static int get_image_key(
    const std::string source_file_path,
    int width = UNSET_WIDTH,
    int height = UNSET_HEIGHT,
    bool is_original_size = true)
{
    return ImageData(source_file_path, width, height, is_original_size).key();
}

class IImageDataReader
{
public:
    virtual std::unique_ptr<ImageData> read_image_data(std::vector<std::string>& read_errors) = 0;
};

class ImageDiskReader : public IImageDataReader
{
public:
    ImageDiskReader() = delete;
    ImageDiskReader(const std::string& source_file_path);
    std::unique_ptr<ImageData> read_image_data(std::vector<std::string>& read_errors);

private:
    std::string source_file_path;
};

class ImageDataTransferer : public IImageDataReader
{
public:
    ImageDataTransferer() = delete;
    ImageDataTransferer(std::unique_ptr<ImageData> data);
    std::unique_ptr<ImageData> read_image_data(std::vector<std::string>& read_errors);

private:
    std::unique_ptr<ImageData> data;
};

class ImageDataResizer
{
public:
    ImageDataResizer() = delete;
    ImageDataResizer(int resize_width, int resize_height);
    std::unique_ptr<ImageData> resize_image_data(const ImageData& in_image_data, std::vector<std::string> resize_errors);

private:
    int resize_width;
    int resize_height;
};