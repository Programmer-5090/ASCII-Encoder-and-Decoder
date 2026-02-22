#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <print>
#include <cstring>
#include "gif.h"
#include "codec.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using cv::Mat;
using ASCIIFrame = std::vector<std::pair<char, rgb>>;

namespace fs = std::filesystem;

const auto brightness = [](const Mat& tile) -> uint {
    if (tile.empty()) {
        return 0;
    }
    return static_cast<uint>(cv::mean(tile)[0]);
};

const auto color = [](const Mat& tile) -> rgb {
    if (tile.empty()) {
        return { 0, 0, 0 };
    }
    const cv::Scalar meanColor = cv::mean(tile);
    return {
        static_cast<unsigned int>(meanColor[2]),
        static_cast<unsigned int>(meanColor[1]),
        static_cast<unsigned int>(meanColor[0])
    };
};

Mat loadImage(const std::string& filePath) {
    return cv::imread(filePath);
}

std::vector<Mat> loadVideo(const std::string& filePath) {
    cv::VideoCapture videoCapture(filePath);
    if (!videoCapture.isOpened()) {
        return {};
    }
    std::vector<Mat> frames;
    Mat frame;
    while (videoCapture.read(frame)) {
        frames.push_back(frame.clone());
    }
    videoCapture.release();
    return frames;
}

ASCIIFrame convertToASCII(const Mat& media) {
    const std::string gradient = "@%#*+=-:. ";
    ASCIIFrame asciiOutput;

    Mat grayScale;
    cv::cvtColor(media, grayScale, cv::COLOR_BGR2GRAY);
    const int ImgW = grayScale.cols;
    const int ImgH = grayScale.rows;
    const int targetColumns = 200;
    const double glyphAspectRatio = 0.5;
    const int tileW = std::max(1, ImgW / targetColumns);
    const int tileH = std::max(1, static_cast<int>(tileW / glyphAspectRatio));

    for (int y = 0; y < ImgH; y += tileH) {
        for (int x = 0; x < ImgW; x += tileW) {
            const int w = std::min(tileW, ImgW - x);
            const int h = std::min(tileH, ImgH - y);
            const cv::Rect region(x, y, w, h);
            const Mat grayTile = grayScale(region);
            const Mat colorTile = media(region);
            const auto value = brightness(grayTile);
            const int gradientIndex = static_cast<int>(std::clamp(
                static_cast<double>(value) / 255.0 * (gradient.size() - 1),
                0.0,
                static_cast<double>(gradient.size() - 1)));
            asciiOutput.emplace_back(gradient[gradientIndex], color(colorTile));
        }
        asciiOutput.emplace_back('\n', rgb{ 0, 0, 0 });
    }
    asciiOutput.emplace_back('\n', rgb{ 0, 0, 0 });

    return asciiOutput;
}

std::unordered_map<int, ASCIIFrame> convertVideoToASCII(const std::vector<Mat>& media) {
    std::unordered_map<int, ASCIIFrame> asciiVideo;
    int count = 0;
    for (const auto& frame : media) {
        asciiVideo[count++] = convertToASCII(frame);
    }
    return asciiVideo;
}

SDL_Surface* renderASCIISurface(const std::vector<std::pair<char, rgb>>& media,
    const std::string& fontPath,
    const int pointSize)
{
    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), static_cast<float>(pointSize));
    if (font == nullptr) {
        std::cerr << "TTF_OpenFont failed: " << SDL_GetError() << '\n';
        return nullptr;
    }

    int glyphW = 0;
    int glyphH = 0;
    if (!TTF_GetStringSize(font, "@", 0, &glyphW, &glyphH)) {
        std::cerr << "TTF_GetStringSize failed: " << SDL_GetError() << '\n';
        TTF_CloseFont(font);
        return nullptr;
    }
    
    int maxColumns = 0;
    int columns = 0;
    int rows = 1;
    for (const auto& [glyph, _] : media) {
        if (glyph == '\n') {
            maxColumns = std::max(columns, maxColumns);
            columns = 0;
            ++rows;
            continue;
        }
        ++columns;
    }
    maxColumns = std::max(columns, maxColumns);

    SDL_Surface* surface = SDL_CreateSurface(maxColumns * glyphW, rows * glyphH, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
        TTF_CloseFont(font);
        return nullptr;
    }
    const SDL_PixelFormatDetails* formatDetails = SDL_GetPixelFormatDetails(surface->format);
    SDL_FillSurfaceRect(surface, nullptr, SDL_MapRGBA(formatDetails, NULL, 0, 0, 0, 255));

    int x = 0;
    int y = 0;
    for (const auto& [glyph, color] : media) {
        if (glyph == '\n') {
            x = 0;
            y += glyphH;
            continue;
        }

        const SDL_Color sdlColor{
            static_cast<Uint8>(color[0]),
            static_cast<Uint8>(color[1]),
            static_cast<Uint8>(color[2]),
            255
        };

        const char text[2]{ glyph, '\0' };
        SDL_Surface* glyphSurface = TTF_RenderText_Blended(font, text, 0, sdlColor);
        if (glyphSurface == nullptr) {
            std::cerr << "TTF_RenderText_Blended failed: " << SDL_GetError() << '\n';
            continue;
        }

        SDL_Rect dst{ x, y, glyphSurface->w, glyphSurface->h };
        SDL_BlitSurface(glyphSurface, nullptr, surface, &dst);
        SDL_DestroySurface(glyphSurface);
        x += glyphW;
    }

    TTF_CloseFont(font);
    return surface;
}

static bool SaveGif(const std::vector<SDL_Surface*>& frames, const std::string& path, int delayMs)
{
    // CAUTION: The GIFS generated are very large
    if (frames.empty()) {
        return false;
    }

    const int w = frames.front()->w;
    const int h = frames.front()->h;
    const int delayCs = std::max(1, delayMs / 10); // gif delay in 1/100s

    GifWriter writer{};
    if (!GifBegin(&writer, path.c_str(), w, h, delayCs)) {
        std::cerr << "GifBegin failed\n";
        return false;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

    for (SDL_Surface* surf : frames) {
        if (!surf || surf->w != w || surf->h != h) {
            std::cerr << "Surface size mismatch\n";
            GifEnd(&writer);
            return false;
        }

        SDL_Surface* converted = surf;
        if (surf->format != SDL_PIXELFORMAT_RGBA32) {
            converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
            if (!converted) {
                std::cerr << "Surface conversion failed\n";
                GifEnd(&writer);
                return false;
            }
        }

        std::memcpy(rgba.data(), converted->pixels, rgba.size());

        if (converted != surf) {
            SDL_DestroySurface(converted);
        }

        if (!GifWriteFrame(&writer, rgba.data(), w, h, delayCs)) {
            std::cerr << "GifWriteFrame failed\n";
            GifEnd(&writer);
            return false;
        }
    }

    if (!GifEnd(&writer)) {
        std::cerr << "GifEnd failed\n";
        return false;
    }
    
    return true;
}

static bool SavePNG(SDL_Surface* surface, const std::string& path)
{
    if (!surface) return false;

    SDL_Surface* converted = surface;
    if (surface->format != SDL_PIXELFORMAT_RGBA32) {
        converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (!converted) {
            std::cerr << "Surface conversion failed\n";
            return false;
        }
    }

    int result = stbi_write_png(path.c_str(),
                                converted->w,
                                converted->h,
                                4,  // RGBA = 4 channels
                                converted->pixels,
                                converted->pitch);

    if (converted != surface) {
        SDL_DestroySurface(converted);
    }

    return result != 0;
}

static bool SaveJPG(SDL_Surface* surface, const std::string& path, int quality = 75)
{
    if (!surface) return false;

    SDL_Surface* converted = surface;
    if (surface->format != SDL_PIXELFORMAT_RGB24) {
        converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
        if (!converted) {
            std::cerr << "Surface conversion failed\n";
            return false;
        }
    }

    int result = stbi_write_jpg(path.c_str(),
                                converted->w,
                                converted->h,
                                3,  // RGB = 3 channels
                                converted->pixels,
                                quality);  // 0-100

    if (converted != surface) {
        SDL_DestroySurface(converted);
    }

    return result != 0;
}

static bool SaveMP4(const std::vector<SDL_Surface*>& frames, const std::string& path, int fps)
{
    if (frames.empty()) {
        std::cerr << "No frames to save\n";
        return false;
    }

    const int w = frames.front()->w;
    const int h = frames.front()->h;

    cv::VideoWriter writer(path,
                          cv::VideoWriter::fourcc('m', 'p', '4', 'v'),  // MPEG-4 codec
                          fps,
                          cv::Size(w, h));

    if (!writer.isOpened()) {
        std::cerr << "Failed to open video writer for " << path << "\n";
        return false;
    }

    for (SDL_Surface* surf : frames) {
        if (!surf || surf->w != w || surf->h != h) {
            std::cerr << "Surface size mismatch\n";
            writer.release();
            return false;
        }

        SDL_Surface* converted = surf;
        if (surf->format != SDL_PIXELFORMAT_RGB24) {
            converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGB24);
            if (!converted) {
                std::cerr << "Surface conversion failed\n";
                writer.release();
                return false;
            }
        }

        // OpenCV uses BGR, convert RGB to BGR
        Mat frame(h, w, CV_8UC3);
        const uint8_t* pixels = static_cast<const uint8_t*>(converted->pixels);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int srcIdx = (y * converted->pitch) + (x * 3);
                int dstIdx = (y * w + x) * 3;
                frame.data[dstIdx + 0] = pixels[srcIdx + 2]; // B
                frame.data[dstIdx + 1] = pixels[srcIdx + 1]; // G
                frame.data[dstIdx + 2] = pixels[srcIdx + 0]; // R
            }
        }

        writer.write(frame);

        if (converted != surf) {
            SDL_DestroySurface(converted);
        }
    }

    writer.release();
    std::cerr << "MP4 saved successfully!\n";
    return true;
}

static bool ensureOutputDir(fs::path& outPath, std::string_view defaultExt)
{
    if (!outPath.has_extension()) {
        outPath.replace_extension(defaultExt);
    }
    const fs::path parentDir = outPath.parent_path();
    try {
        if (!parentDir.empty() && !fs::exists(parentDir)) {
            fs::create_directories(parentDir);
            std::println("Directories created: {}", parentDir.string());
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << '\n';
        return false;
    }
    return true;
}

static std::vector<SDL_Surface*> renderASCIIVideoSurfaces(const ASCIIVideo& asciiVideo,
    const std::string& fontPath,
    int pointSize)
{
    std::vector<SDL_Surface*> surfaceFrames;
    surfaceFrames.reserve(asciiVideo.size());

    for (int i = 0; i < static_cast<int>(asciiVideo.size()); ++i) {
        if (asciiVideo.find(i) == asciiVideo.end()) {
            std::cerr << "Frame " << i << " not found in video\n";
            continue;
        }
        SDL_Surface* surface = renderASCIISurface(asciiVideo.at(i), fontPath, pointSize);
        if (surface) {
            surfaceFrames.emplace_back(surface);
            if ((i + 1) % 10 == 0) {
                std::cerr << "Processed " << (i + 1) << " frames\n";
            }
        } else {
            std::cerr << "Failed to render frame " << i << '\n';
        }
    }

    std::cerr << "Rendered " << surfaceFrames.size() << " surfaces\n";
    return surfaceFrames;
}

void saveASCIIImage(const ASCIIFrame& asciiFrame,
    const std::string& fontPath,
    int pointSize,
    const std::string& basePath, const std::string& ext = ".png")
{
    SDL_Surface* surface = renderASCIISurface(asciiFrame, fontPath, pointSize);
    if (!surface) {
        std::cerr << "Failed to render surface\n";
        return;
    }

    fs::path base(basePath);
    fs::path parentDir = base.parent_path();

    try {
        if (!parentDir.empty() && !fs::exists(parentDir)) {
            fs::create_directories(parentDir);
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        SDL_DestroySurface(surface);
        return;
    }

    // Save as PNG (lossless)
    if (ext == ".png") {
        std::string pngPath = base.string() + ext;
        if (SavePNG(surface, pngPath)) {
            std::cerr << "Saved PNG: " << pngPath << '\n';
        }
        else {
            std::cerr << "Failed to save PNG\n";
        }
    }
    else {
        // Save as JPEG (smaller file size)
        std::string jpgPath = base.string() + ".jpg";
        if (SaveJPG(surface, jpgPath, 95)) {
            std::cerr << "Saved JPG: " << jpgPath << '\n';
        }
        else {
            std::cerr << "Failed to save JPG\n";
        }
    }

    SDL_DestroySurface(surface);
}

void saveASCII_GIF(const std::vector<Mat>& frames,
    const std::string& fontPath,
    int pointSize,
    const std::string& outputPath,
    int delayMs)
{
    fs::path outPath(outputPath);
    fs::path parentDir = outPath.parent_path();

    if (!outPath.has_extension()) {
        outPath.replace_extension(".gif");
    }

    try {
        if (!fs::exists(parentDir)) {
            fs::create_directories(parentDir);
            std::println("Directories created: {}", parentDir.string());
        }
        else {
            std::println("Directories already exist: {}", parentDir.string());
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        return;
    }

    std::cerr << "Processing " << frames.size() << " frames...\n";

    std::vector<SDL_Surface*> surfaceFrames;
    surfaceFrames.reserve(frames.size());

    int frameCount = 0;
    for (const auto& frame : frames) {
        const auto ascii = convertToASCII(frame);
        SDL_Surface* surface = renderASCIISurface(ascii, fontPath, pointSize);
        if (surface) {
            surfaceFrames.emplace_back(surface);
            frameCount++;
            if (frameCount % 10 == 0) {
                std::cerr << "Processed " << frameCount << " frames\n";
            }
        }
        else {
            std::cerr << "Failed to render frame " << frameCount << '\n';
        }
    }

    std::cerr << "Rendered " << surfaceFrames.size() << " surfaces\n";

    if (!surfaceFrames.empty()) {
        std::cerr << "Saving GIF to " << outPath.string() << '\n';
        if (SaveGif(surfaceFrames, outPath.string(), delayMs)) {
            std::cerr << "GIF saved successfully!\n";
        }
        else {
            std::cerr << "Failed to save GIF: " << outPath.string() << '\n';
        }
    }

    for (SDL_Surface* s : surfaceFrames) {
        SDL_DestroySurface(s);
    }
}

void saveASCIIVideo(const ASCIIVideo& asciiVideo,
    const std::string& fontPath,
    int pointSize,
    const std::string& outputPath,
    int delayMs)
{
    fs::path outPath(outputPath);
    const std::string ext = outPath.has_extension() ? outPath.extension().string() : ".mp4";

    if (!ensureOutputDir(outPath, ext)) {
        return;
    }

    std::cerr << "Processing " << asciiVideo.size() << " frames...\n";

    std::vector<SDL_Surface*> surfaceFrames = renderASCIIVideoSurfaces(asciiVideo, fontPath, pointSize);

    if (surfaceFrames.empty()) {
        std::cerr << "No frames rendered, skipping video creation\n";
        return;
    }

    if (outPath.extension() == ".gif") {
        std::cerr << "Saving GIF to " << outPath.string() << '\n';
        if (SaveGif(surfaceFrames, outPath.string(), delayMs)) {
            std::cerr << "GIF saved successfully!\n";
        } else {
            std::cerr << "Failed to save GIF: " << outPath.string() << '\n';
        }
    } else {
        std::cerr << "Saving MP4 to " << outPath.string() << '\n';
        if (SaveMP4(surfaceFrames, outPath.string(), 1000 / delayMs)) {
            std::cerr << "MP4 saved to " << outPath.string() << "!\n";
        }
    }

    for (SDL_Surface* s : surfaceFrames) {
        SDL_DestroySurface(s);
    }
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (!TTF_Init()) {
        std::cerr << "SDL_ttf could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    const std::string videoAssetPath = "assets/bocchi.mp4";
    const std::string imageAssetPath = "assets/Sakura_Nene_CPP.jpg";
    const std::string fontPath = "assets/Boogaloo-Regular.ttf";
    const std::string outputPath = "out/ascii";

    // Convert and save a single image
    const Mat image = loadImage(imageAssetPath);
    if (!image.empty()) {
        std::cerr << "Converting image to ASCII...\n";
        const ASCIIFrame asciiImage = convertToASCII(image);
        saveASCIIImage(asciiImage, fontPath, 10, "out/ascii_image");
    } else {
        std::cerr << "No image found at " << imageAssetPath << ", skipping image conversion\n";
    }

    // Convert and save video
    const std::vector<Mat> video = loadVideo(videoAssetPath);
    if (video.empty()) {
        std::cerr << "Failed to load video from " << videoAssetPath << '\n';
        return 1;
    }

    // Testing Compression and Decompression
    std::cerr << "Compressing video...\n";
    compressASCIIVideo(convertVideoToASCII(video), "ascii_video.bin");

    std::cerr << "Loading compressed video from ascii_video.bin...\n";
    auto asciiVid = decompressASCIIVideo("ascii_video.bin");
    
    if (asciiVid.empty()) {
        std::cerr << "Failed to decompress video!\n";
        return 1;
    }
    std::cerr << "Successfully decompressed " << asciiVid.size() << " frames\n";

    // Convert and save video
    saveASCIIVideo(asciiVid, fontPath, 10, outputPath + "_video.mp4", 17);

    TTF_Quit();
    SDL_Quit();
    return 0;
}
