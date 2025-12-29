#pragma once
#include "paint_operations.h"
#include <map>
#include <vector>

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();
    
    void rasterize(const DisplayList& displayList, const Rect& viewport);
    
private:
    void executeDrawRect(const DrawRectOp& op);
    void executeDrawText(const DrawTextOp& op);
    void executeDrawImage(const DrawImageOp& op);
    void executeSetClip(const SetClipOp& op);
    void executeRestoreClip(const RestoreClipOp& op);
    
    void loadImage(const std::string& imagePath);
    void decodeJpeg(const std::string& filePath);
    void decodePng(const std::string& filePath);
    
    struct ImageData {
        uint32_t width, height;
        std::vector<uint8_t> pixels;
        uint32_t textureId;
    };
    
    std::map<std::string, ImageData> imageCache;
    Rect currentClip;
    bool hasClip;
};