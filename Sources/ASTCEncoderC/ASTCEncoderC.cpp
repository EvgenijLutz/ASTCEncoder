//
//  ASTCEncoderC.cpp
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 26.08.25.
//

#define __STDC_LIB_EXT1__ 1
#include <astcenc.h>
#include <ASTCEncoderC.hpp>
#include <stdio.h>
#include <thread>

#include <string.h>


static void copyString(char* __nonnull dst, const char* __nullable src, long maxLen) {
    if (src == nullptr) {
        dst[0] = 0;
        return;
    }
    
    auto length = strnlen(src, maxLen);
    if (length >= maxLen) {
        length = maxLen - 1;
    }
    memcpy(dst, src, length);
    dst[length] = 0;
}


struct ASTCCallbackContext {
    astcenc_context* __nullable context;
    void* __nullable userInfo = nullptr;
    ASTCEncoderProgressCallback __nullable callback = nullptr;
    
    // Task was cancelled during compression
    bool cancelled;
    
    void reset() {
        context = nullptr;
        userInfo = nullptr;
        callback = nullptr;
        cancelled = false;
    }
};


thread_local ASTCCallbackContext callbackContext = ASTCCallbackContext();


// MARK: - ASTCErrorInfo

ASTCErrorInfo::ASTCErrorInfo() {
    copyString(_errorMessage, nullptr, ASTC_ENCODER_ERROR_SIZE);
}

ASTCErrorInfo::ASTCErrorInfo(const ASTCErrorInfo& other) {
    memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
}

ASTCErrorInfo::ASTCErrorInfo(ASTCErrorInfo&& other) {
    memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
}

ASTCErrorInfo::~ASTCErrorInfo() {
    // Done
}


ASTCErrorInfo& ASTCErrorInfo::operator = (const ASTCErrorInfo& other) {
    if (this == &other) {
        return *this;
    }
    
    memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
    
    return *this;
}


ASTCErrorInfo& ASTCErrorInfo::operator = (ASTCErrorInfo&& other) {
    if (this == &other) {
        return *this;
    }
    
    memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
    
    return *this;
}

const char* __nullable ASTCErrorInfo::getErrorMessage() const {
    return _errorMessage;
}

void ASTCErrorInfo::setErrorMessage(const char* __nullable errorMessage) {
    memcpy(_errorMessage, errorMessage, ASTC_ENCODER_ERROR_SIZE);
}


// MARK: - ASTCRawImage

ASTCRawImage::ASTCRawImage(char* __nonnull data, long width, long height, long originalNumComponents, long componentSize, bool linear, bool hdr):
referenceCounter(1),
_data(data),
_width(width),
_height(height),
_originalNumComponents(originalNumComponents),
_componentSize(componentSize),
_linear(linear),
_hdr(hdr) {
    // Done
}

ASTCRawImage::~ASTCRawImage() {
    delete [] _data;
}


ASTCRawImage* __nullable ASTCRawImageRetain(ASTCRawImage* __nullable image) SWIFT_RETURNS_UNRETAINED {
    if (image) {
        image->referenceCounter.fetch_add(1);
    }
    return image;
}

void ASTCRawImageRelease(ASTCRawImage* __nullable image) {
    if (image && image->referenceCounter.fetch_sub(1) <= 1) {
        delete image;
    }
}


ASTCRawImage* __nullable ASTCRawImage::create(char* __nonnull data, long width, long height, long numComponents, long componentSize, bool linear, bool hdr, ASTCErrorInfo& error) SWIFT_RETURNS_RETAINED {
    // Validate input data
    if (data == nullptr) {
        error.setErrorMessage("Image data not specified");
        return nullptr;
    }
    
    if (width < 1) {
        error.setErrorMessage("Invalid width");
        return nullptr;
    }
    
    if (height < 1) {
        error.setErrorMessage("Invalid height");
        return nullptr;
    }
    
    if (numComponents < 1 || numComponents > 4) {
        error.setErrorMessage("Unsupported number of components");
        return nullptr;
    }
    
    if (componentSize != 1 && componentSize != 2 && componentSize != 4) {
        error.setErrorMessage("Unsupported component size");
        return nullptr;
    }
    
    
    // Create image data
    auto imageDataSize = width * height * componentSize * 4;
    auto dataCopy = new char[imageDataSize];
    
    // Copy the whole image contents if the original number of component matches
    if (numComponents == 4) {
        memcpy(dataCopy, data, imageDataSize);
    }
    else {
        memset(dataCopy, 255, imageDataSize);
        auto pixelSize = numComponents * componentSize;
        for (auto j = 0; j < height; j++) {
            for (auto i = 0; i < width; i++) {
                memcpy(dataCopy + (i * 4 + j * width * 4),
                       data + (i * pixelSize + 0 + j * width * pixelSize),
                       pixelSize);
            }
        }
    }
    
    // TODO: Add swizzle support
    
    // Success
    return new ASTCRawImage(dataCopy, width, height, numComponents, componentSize,
                            linear, hdr);
}


ASTCImage* __nullable ASTCRawImage::compress(long blockWidth, long blockHeight, float quality, ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) {
    // Prepare ASTC encoder config
    astcenc_config config;
    auto profile = astcenc_profile::ASTCENC_PRF_LDR;
    //auto blockWidth = 4;
    //auto blockHeight = 4;
    long blockDepth = 1;
    auto result = astcenc_config_init(profile,
                                      static_cast<unsigned int>(blockWidth),
                                      static_cast<unsigned int>(blockHeight),
                                      static_cast<unsigned int>(blockDepth),
                                      quality,
                                      ASTCENC_FLG_USE_DECODE_UNORM8,
                                      &config);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not initialise config");
        return nullptr;
    }
    // Power user settings
    config.progress_callback = [](float progress) {
        if (callbackContext.callback == nullptr) {
            return;
        }
        
        // Don't process if cancelled
        if (callbackContext.cancelled) {
            return;
        }
        
        // Execute callback
        // TODO: We can also send back image data to see the live preview!
        auto shouldStop = callbackContext.callback(callbackContext.userInfo, progress);
        if (shouldStop) {
            callbackContext.cancelled = true;
            astcenc_compress_cancel(callbackContext.context);
        }
    };
    
    astcenc_context* context = nullptr;
    auto numThreads = 1; //std::thread::hardware_concurrency();
    result = astcenc_context_alloc(&config, numThreads, &context);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not create context");
        return nullptr;
    }
    
    
    // Set callback context
    callbackContext.context = context;
    callbackContext.userInfo = userInfo;
    callbackContext.callback = progressCallback;
    callbackContext.cancelled = false;
    
    
    // Prepare image data
    astcenc_image image;
    switch (_componentSize) {
        case 1: image.data_type = astcenc_type::ASTCENC_TYPE_U8; break;
        case 2: image.data_type = astcenc_type::ASTCENC_TYPE_F16; break;
        case 4: image.data_type = astcenc_type::ASTCENC_TYPE_F32; break;
        default:
            error.setErrorMessage("Unsupported component size");
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
    image.dim_x = static_cast<unsigned int>(_width);
    image.dim_y = static_cast<unsigned int>(_height);
    image.dim_z = static_cast<unsigned int>(1);
    // Data is always passed as 4 component image array
    char* content = _data;
    image.data = reinterpret_cast<void**>(&content);
    
    // Prepare swizzle info
    astcenc_swizzle swizzle;
#if 0
    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
    //swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
    switch (numComponents) {
        case 4: swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
        case 3: swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
        case 2: swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
        case 1: break;
            
        default:
            error.setErrorMessage("Unsupported number of components");
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
#else
    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
    swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
    swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
//    swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
    swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
#endif
    
    // Allocate memory for astc compressed output image
    auto astcXCount = static_cast<long>(ceilf(static_cast<float>(_width) / static_cast<float>(blockWidth)));
    auto astcYCount = static_cast<long>(ceilf(static_cast<float>(_height) / static_cast<float>(blockHeight)));
    size_t dataLength = astcXCount * astcYCount * blockDepth * 16;
    char* astcData = new char[dataLength];
    memset(astcData, 0, dataLength);
    
    // Compress image
    auto compressedData = reinterpret_cast<uint8_t*>(astcData);
    result = astcenc_compress_image(context, &image, &swizzle, compressedData, dataLength, 0);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not compress image");
        delete [] astcData;
        astcenc_context_free(context);
        callbackContext.reset();
        return nullptr;
    }
    
    // Check if task was cancelled
    if (callbackContext.cancelled) {
        error.setErrorMessage("Task was cancelled");
        delete [] astcData;
        astcenc_context_free(context);
        callbackContext.reset();
        return nullptr;
    }
    
    // Clean up
    astcenc_context_free(context);
    callbackContext.reset();
    
    return new ASTCImage(astcData, _width, _height, 1, _originalNumComponents, _componentSize, _linear, _hdr, astcXCount, astcYCount, 1, blockWidth, blockHeight, blockDepth);
}


// MARK: - ASTCImage

ASTCImage::ASTCImage(char* __nonnull data, long width, long height, long depth, long originalNumComponents, long componentSize, bool linear, bool hdr, long numBlocksWidth, long numBlocksHeight, long numBlocksDepth, long blockWidth, long blockHeight, long blockDepth):
referenceCounter(1),
_data(data),
_width(width),
_height(height),
_depth(depth),
_originalNumComponents(originalNumComponents),
_componentSize(componentSize),
_linear(linear),
_hdr(hdr),
_numBlocksWidth(numBlocksWidth),
_numBlocksHeight(numBlocksHeight),
_numBlocksDepth(numBlocksDepth),
_blockWidth(blockWidth),
_blockHeight(blockHeight),
_blockDepth(blockDepth) {
    // Done
}

ASTCImage::~ASTCImage() {
    delete [] _data;
}


ASTCImage* __nullable ASTCImageRetain(ASTCImage* __nullable image) {
    if (image) {
        image->referenceCounter.fetch_add(1);
    }
    
    return image;
}

void ASTCImageRelease(ASTCImage* __nullable image) {
    if (image && image->referenceCounter.fetch_sub(1) <= 1) {
        delete image;
    }
}


ASTCRawImage* __nullable ASTCImage::decompress(ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) {
    // Prepare ASTC encoder config
    astcenc_config config;
    auto profile = astcenc_profile::ASTCENC_PRF_LDR;
    auto result = astcenc_config_init(profile,
                                      static_cast<unsigned int>(_blockWidth),
                                      static_cast<unsigned int>(_blockHeight),
                                      static_cast<unsigned int>(_blockDepth),
                                      ASTCENC_PRE_MEDIUM, // ASTCENC_PRE_EXHAUSTIVE,
                                      ASTCENC_FLG_USE_DECODE_UNORM8 | ASTCENC_FLG_DECOMPRESS_ONLY,
                                      &config);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not initialise config");
        return nullptr;
    }
    // Power user settings
    config.progress_callback = [](float progress) {
        if (callbackContext.callback == nullptr) {
            return;
        }
        
        // Execute callback
        callbackContext.callback(callbackContext.userInfo, progress);
    };
    
    astcenc_context* context = nullptr;
    auto numThreads = 1; //std::thread::hardware_concurrency();
    result = astcenc_context_alloc(&config, numThreads, &context);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not create context");
        return nullptr;
    }
    
    
    // Set callback context
    callbackContext.context = context;
    callbackContext.userInfo = userInfo;
    callbackContext.callback = progressCallback;
    callbackContext.cancelled = false;
    
    
    // Prepare image data
    astcenc_image image;
    switch (_componentSize) {
        case 1: image.data_type = astcenc_type::ASTCENC_TYPE_U8; break;
        case 2: image.data_type = astcenc_type::ASTCENC_TYPE_F16; break;
        case 4: image.data_type = astcenc_type::ASTCENC_TYPE_F32; break;
        default:
            error.setErrorMessage("Unsupported component size");
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
    image.dim_x = static_cast<unsigned int>(_width);
    image.dim_y = static_cast<unsigned int>(_height);
    image.dim_z = static_cast<unsigned int>(_depth);
    // Data is always passed as 4 component image array
    auto content = new char[_width * _height * _depth * 4 * _componentSize];
    image.data = reinterpret_cast<void**>(&content);
    
    // Prepare swizzle info
    astcenc_swizzle swizzle;
#if 0
    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
    //swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
    switch (numComponents) {
        case 4: swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
        case 3: swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
        case 2: swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
        case 1: break;
            
        default:
            error.setErrorMessage("Unsupported number of components");
            delete [] content;
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
#else
    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
    swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
    swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
//    swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
    swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
#endif
    
    
    auto dataLength = _numBlocksWidth * _numBlocksHeight * _numBlocksDepth * 16;
    
    
    // Decompress image
    auto compressedData = reinterpret_cast<uint8_t*>(_data);
    result = astcenc_decompress_image(context, compressedData, dataLength, &image, &swizzle, 0);
    if (result != astcenc_error::ASTCENC_SUCCESS) {
        error.setErrorMessage("Could not decompress image");
        delete [] content;
        astcenc_context_free(context);
        callbackContext.reset();
        return nullptr;
    }
    
    // Clean up
    astcenc_context_free(context);
    callbackContext.reset();
    
    return new ASTCRawImage(content, _width, _height, _originalNumComponents, _componentSize, _linear, _hdr);
}
