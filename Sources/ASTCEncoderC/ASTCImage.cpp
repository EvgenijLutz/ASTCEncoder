//
//  ASTCImage.cpp
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 10.11.25.
//

#define __STDC_LIB_EXT1__ 1
#include <astcenc.h>
#include <ASTCEncoderC/ASTCImage.hpp>
#include <stdio.h>
#include <thread>
#include <string.h>
#include <numeric>


struct ASTCCallbackContext {
    astcenc_context* fn_nullable context;
    void* fn_nullable userInfo = nullptr;
    ASTCEncoderProgressCallback fn_nullable callback = nullptr;
    
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


// MARK: - ASTCError

ASTCError::ASTCError() {
    setErrorMessage(nullptr);
}

ASTCError::ASTCError(const char* fn_nonnull message fn_noescape) {
    setErrorMessage(message);
}

ASTCError::ASTCError(const ASTCError& other) {
    std::memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
}

ASTCError::ASTCError(ASTCError&& other) {
    std::memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
}

ASTCError::~ASTCError() {
    // Done
}


ASTCError& ASTCError::operator = (const ASTCError& other) {
    if (this == &other) {
        return *this;
    }
    
    std::memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
    
    return *this;
}


ASTCError& ASTCError::operator = (ASTCError&& other) {
    if (this == &other) {
        return *this;
    }
    
    std::memcpy(_errorMessage, other._errorMessage, ASTC_ENCODER_ERROR_SIZE);
    
    return *this;
}

std::span<const char> ASTCError::getMessageSpan() const fn_lifetimebound {
    return std::span(_errorMessage, strnlen(_errorMessage, ASTC_ENCODER_ERROR_SIZE));
}

const char* fn_nullable ASTCError::getErrorMessage() const {
    return _errorMessage;
}

void ASTCError::setErrorMessage(const char* fn_nullable errorMessage fn_noescape) {
    if (errorMessage == nullptr) {
        _errorMessage[0] = 0;
        return;
    }
    
    std::strncpy(_errorMessage, errorMessage, ASTC_ENCODER_ERROR_SIZE);
}


// MARK: - ASTCBlockSize

const ASTCBlockSize ASTCBlockSize::_4x4 = ASTCBlockSize(4, 4, 1);
const ASTCBlockSize ASTCBlockSize::_5x4 = ASTCBlockSize(5, 4, 1);
const ASTCBlockSize ASTCBlockSize::_5x5 = ASTCBlockSize(5, 5, 1);
const ASTCBlockSize ASTCBlockSize::_6x5 = ASTCBlockSize(6, 5, 1);
const ASTCBlockSize ASTCBlockSize::_6x6 = ASTCBlockSize(6, 6, 1);
const ASTCBlockSize ASTCBlockSize::_8x5 = ASTCBlockSize(8, 5, 1);
const ASTCBlockSize ASTCBlockSize::_8x6 = ASTCBlockSize(8, 6, 1);
const ASTCBlockSize ASTCBlockSize::_8x8 = ASTCBlockSize(8, 8, 1);
const ASTCBlockSize ASTCBlockSize::_10x5 = ASTCBlockSize(10, 5, 1);
const ASTCBlockSize ASTCBlockSize::_10x6 = ASTCBlockSize(10, 6, 1);
const ASTCBlockSize ASTCBlockSize::_10x8 = ASTCBlockSize(10, 8, 1);
const ASTCBlockSize ASTCBlockSize::_10x10 = ASTCBlockSize(10, 10, 1);
const ASTCBlockSize ASTCBlockSize::_12x10 = ASTCBlockSize(12, 10, 1);
const ASTCBlockSize ASTCBlockSize::_12x12 = ASTCBlockSize(12, 12, 1);

const ASTCBlockSize ASTCBlockSize::_3x3x3 = ASTCBlockSize(3, 3, 3);
const ASTCBlockSize ASTCBlockSize::_4x3x3 = ASTCBlockSize(4, 3, 3);
const ASTCBlockSize ASTCBlockSize::_4x4x3 = ASTCBlockSize(4, 4, 3);
const ASTCBlockSize ASTCBlockSize::_4x4x4 = ASTCBlockSize(4, 4, 4);
const ASTCBlockSize ASTCBlockSize::_5x4x4 = ASTCBlockSize(5, 4, 4);
const ASTCBlockSize ASTCBlockSize::_5x5x4 = ASTCBlockSize(5, 5, 4);
const ASTCBlockSize ASTCBlockSize::_5x5x5 = ASTCBlockSize(5, 5, 5);
const ASTCBlockSize ASTCBlockSize::_6x5x5 = ASTCBlockSize(6, 5, 5);
const ASTCBlockSize ASTCBlockSize::_6x6x5 = ASTCBlockSize(6, 6, 5);
const ASTCBlockSize ASTCBlockSize::_6x6x6 = ASTCBlockSize(6, 6, 6);


bool ASTCBlockSize::operator == (const ASTCBlockSize& other) const {
    return width == other.width && height == other.height && depth == other.depth;
}


// MARK: - ASTCRawImage

ASTCRawImage::ASTCRawImage(char* fn_nonnull contents, long width, long height, long depth, long originalNumComponents, long componentSize, bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap):
_referenceCounter(1),
_contents(contents),
_width(width),
_height(height),
_depth(depth),
_originalNumComponents(originalNumComponents),
_componentSize(componentSize),
_linear(linear),
_hdr(hdr),
_containsAlpha(containsAlpha),
_ldrAlpha(ldrAlpha),
_normalMap(normalMap) {
    // Done
}

ASTCRawImage::~ASTCRawImage() {
    delete [] _contents;
}


template <typename SourceType, typename DestinationType>
struct PixelInfo {
    // Sanity check
    static_assert(sizeof(SourceType) == sizeof(DestinationType), "Source and destination sizes should match");
    
    union {
        SourceType sourceTypeValue;
        unsigned char bytes[sizeof(SourceType)];
        DestinationType destinationTypeValue;
    };
    
    void convertToDestinationType(bool littleEndian) {
        // Swap bytes instead of manually bit shift to make the value little endian
        if (littleEndian == false) {
            constexpr auto numBytes = sizeof(SourceType);
            constexpr auto halfSize = numBytes / 2;
            constexpr auto lastByteIndex = numBytes - 1;
            for (auto byteIndex = 0; byteIndex < halfSize; byteIndex++) {
                auto byte0 = bytes[byteIndex];
                auto byte1 = bytes[lastByteIndex - byteIndex];
                bytes[byteIndex] = byte1;
                bytes[lastByteIndex - byteIndex] = byte0;
            }
        }
        
        // Cast to double - takes more space, produces less precision errors
        auto value = static_cast<double>(sourceTypeValue);
        
        // Cast to the destination type
        static auto sourceMax = std::numeric_limits<SourceType>::max();
        destinationTypeValue = static_cast<DestinationType>(value / static_cast<double>(sourceMax));
    }
};


ASTCRawImage* fn_nullable ASTCRawImage::create(const char* fn_nonnull data fn_noescape, long width, long height, long depth, long numComponents, long componentSize, bool integerComponents, bool littleEndian, bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap, ASTCError& error) SWIFT_RETURNS_RETAINED {
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
    
    if (depth < 1) {
        error.setErrorMessage("Invalid depth");
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
    
    // Components are allowed to be floats only if componentSize takes at least two bytes.
    if (componentSize == 1) {
        if (integerComponents == false) {
            printf("Floating point colour components should take at least 2 bytes. The integerComponents setting is ignored and assumed to be true.\n");
            integerComponents = true;
        }
    }
    
    // HDR image can be only in linear color space
    if (hdr) {
        if (componentSize < 2) {
            printf("HDR image colour components should take at least 2 bytes. The hdr setting is ignored.\n");
            hdr = false;
        }
        else if (integerComponents) {
            printf("HDR image can only contain floating point components. The hdr setting is ignored.\n");
            hdr = false;
        }
        else if (linear == false) {
            printf("HDR image can be only in linear colour space. The hdr setting is ignored.\n");
            hdr = false;
        }
    }
    
    // Check if there is really alpha channel, correct otherwise
    if (containsAlpha) {
        if (numComponents != 2 && numComponents != 4) {
            printf("Only textures with 2 or 4 pixel component may contain alpha channel. The containsAlpha setting is ignored.\n");
            containsAlpha = false;
        }
    }
    
    // Check ldr alpha
    if (ldrAlpha) {
        /*if (numComponents != 4) {
            printf("Only hdr textures with 4 colour components may contain LDR alpha. The ldrAlpha setting is ignored.\n");
            ldrAlpha = false;
        }
        else*/ if (hdr == false) {
            printf("The ldrAlpha setting is ignored because the texture is not HDR.\n");
            ldrAlpha = false;
        }
    }
    
    // Check normal map
    if (normalMap == true) {
        if (numComponents < 2) {
            printf("Normal map should have at least two components. The normalMap setting is ignored and the image is treated as a greyscale texture.\n");
            normalMap = false;
        }
        else if (hdr == true) {
            printf("Normal map cannot have extended colour space. The normalMap setting is ignored and the image is treated as a regular hdr texture.\n");
            normalMap = false;
        }
        else if (containsAlpha && numComponents < 4) {
            printf("Normal map cannot contain alpha channel. The normalMap setting is ignored and the image is treated as a regular texture.\n");
            normalMap = false;
        }
    }
    
    
    // Create image data
    auto imageDataSize = width * height * componentSize * 4;
    auto dataCopy = new char[imageDataSize];
    
    // Copy the whole image contents if the original number of component matches
    if (numComponents == 4) {
        std::memcpy(dataCopy, data, imageDataSize);
    }
    else {
        std::memset(dataCopy, 255, imageDataSize);
        auto pixelSize = numComponents * componentSize;
        auto targetPixelSize = 4 * componentSize;
        for (auto j = 0; j < height; j++) {
            for (auto i = 0; i < width; i++) {
                std::memcpy(dataCopy + (i * targetPixelSize + j * width * targetPixelSize),
                            data + (i * pixelSize + 0 + j * width * pixelSize),
                            pixelSize);
            }
        }
    }
    
    // Convert endianness and integer pixels to floats if component size is more than one byte
    if (integerComponents && componentSize > 1) {
        // Also convert to the little endian system
        // PNG's integers are in network byte order (big endian)
        auto numPixels = width * height * 4;
        if (componentSize == 2) {
            auto pixels = reinterpret_cast<PixelInfo<unsigned short, __fp16>*>(dataCopy);
            for (auto pixelIndex = 0; pixelIndex < numPixels; pixelIndex++) {
                pixels[pixelIndex].convertToDestinationType(littleEndian);
            }
        }
        else if (componentSize == 4) {
            auto pixels = reinterpret_cast<PixelInfo<unsigned int, float>*>(dataCopy);
            for (auto pixelIndex = 0; pixelIndex < numPixels; pixelIndex++) {
                pixels[pixelIndex].convertToDestinationType(littleEndian);
            }
        }
    }
    
    // Success
    return new ASTCRawImage(dataCopy,
                            width, height, depth,
                            normalMap ? 2 : numComponents, componentSize,
                            linear,
                            hdr, containsAlpha, ldrAlpha, normalMap);
}


static astcenc_profile makeASTCEncoderProfile(bool linear, bool hdr, bool ldrAlpha) {
    if (hdr) {
        if (ldrAlpha) {
            return astcenc_profile::ASTCENC_PRF_HDR_RGB_LDR_A;
        }
        
        return astcenc_profile::ASTCENC_PRF_HDR;
    }
    
    if (linear) {
        return astcenc_profile::ASTCENC_PRF_LDR;
    }
    
    return astcenc_profile::ASTCENC_PRF_LDR_SRGB;
}


ASTCImage* fn_nullable ASTCRawImage::compress(ASTCBlockSize blockSize, float quality, ASTCError& error, void* fn_nullable userInfo fn_noescape, ASTCEncoderProgressCallback fn_nullable progressCallback fn_noescape) {
    // Prepare ASTC encoder config
    astcenc_config config;
    long blockWidth = blockSize.width;
    long blockHeight = blockSize.height;
    long blockDepth = blockSize.depth;
    
    unsigned int flags = 0;
    // Normal map
    if (_normalMap) {
        flags |= ASTCENC_FLG_MAP_NORMAL;
    }
    else {
        // Alpha weight for more accurate enconding in opaque areas
        if (_containsAlpha && (_originalNumComponents == 2 || _originalNumComponents == 4)) {
            flags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;
        }
    }
    //ASTCENC_FLG_USE_ALPHA_WEIGHT
    //ASTCENC_FLG_USE_DECODE_UNORM8
    
    auto result = astcenc_config_init(makeASTCEncoderProfile(_linear, _hdr, _ldrAlpha),
                                      static_cast<unsigned int>(blockWidth),
                                      static_cast<unsigned int>(blockHeight),
                                      static_cast<unsigned int>(blockDepth),
                                      quality,
                                      flags,
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
        auto shouldStop = callbackContext.callback(callbackContext.userInfo, progress / 100);
        if (shouldStop) {
            callbackContext.cancelled = true;
            astcenc_compress_cancel(callbackContext.context);
        }
    };
    
    astcenc_context* context = nullptr;
    auto numThreads = 1; //std::thread::hardware_concurrency();
    result = astcenc_context_alloc(&config, numThreads, &context, nullptr);
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
    void* content = static_cast<void*>(_contents);
    image.data = &content;
    
    // Prepare swizzle info
    // From the documentation:
    // | Input data   | Encoding swizzle | Sampling swizzle |
    // | ------------ | ---------------- | ---------------- |
    // | 1 component  | RRR1             | .[rgb]           |
    // | 2 components | RRRG             | .[rgb]a          |
    // | 3 components | RGB1             | .rgb             |
    // | 4 components | RGBA             | .rgba            |
    astcenc_swizzle swizzle;
    switch (_originalNumComponents) {
        case 1:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
            break;
            
        case 2:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_G;
            break;
            
        case 3:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
            break;
            
        case 4:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
            break;
            
        default:
            error.setErrorMessage("Unsupported number of components");
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
    
    // Allocate memory for astc compressed output image
    auto astcXCount = static_cast<long>(ceilf(static_cast<float>(_width) / static_cast<float>(blockWidth)));
    auto astcYCount = static_cast<long>(ceilf(static_cast<float>(_height) / static_cast<float>(blockHeight)));
    auto astcZCount = static_cast<long>(ceilf(static_cast<float>(_depth) / static_cast<float>(blockDepth)));
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
    
    return new ASTCImage(astcData, _width, _height, _depth, _originalNumComponents, _componentSize, _linear, _hdr, _containsAlpha, _ldrAlpha, _normalMap, astcXCount, astcYCount, astcZCount, blockSize);
}


// MARK: - ASTCImage

ASTCImage::ASTCImage(char* fn_nonnull contents, long width, long height, long depth, long originalNumComponents, long componentSize, bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap, long numBlocksWidth, long numBlocksHeight, long numBlocksDepth, ASTCBlockSize blockSize):
_referenceCounter(1),
_contents(contents),
_width(width),
_height(height),
_depth(depth),
_originalNumComponents(originalNumComponents),
_componentSize(componentSize),
_linear(linear),
_hdr(hdr),
_containsAlpha(containsAlpha),
_ldrAlpha(ldrAlpha),
_normalMap(normalMap),
_numBlocksWidth(numBlocksWidth),
_numBlocksHeight(numBlocksHeight),
_numBlocksDepth(numBlocksDepth),
_blockSize(blockSize) {
    // Done
}

ASTCImage::~ASTCImage() {
    delete [] _contents;
}


ASTCRawImage* fn_nullable ASTCImage::decompress(ASTCError& error, void* fn_nullable userInfo fn_noescape, ASTCEncoderProgressCallback fn_nullable progressCallback fn_noescape) {
    // Prepare ASTC encoder config
    astcenc_config config;
    auto result = astcenc_config_init(makeASTCEncoderProfile(_linear, _hdr, _ldrAlpha),
                                      static_cast<unsigned int>(_blockSize.width),
                                      static_cast<unsigned int>(_blockSize.height),
                                      static_cast<unsigned int>(_blockSize.depth),
                                      ASTCENC_PRE_EXHAUSTIVE,
                                      ASTCENC_FLG_DECOMPRESS_ONLY,
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
        callbackContext.callback(callbackContext.userInfo, progress / 100);
    };
    
    astcenc_context* context = nullptr;
    auto numThreads = 1; //std::thread::hardware_concurrency();
    result = astcenc_context_alloc(&config, numThreads, &context, nullptr);
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
    switch (_originalNumComponents) {
        case 1:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
            break;
            
        case 2:
            if (_normalMap) {
                swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
                swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
                swizzle.b = astcenc_swz::ASTCENC_SWZ_Z;
                swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
            }
            else {
                if (_containsAlpha) {
                    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
                    swizzle.g = astcenc_swz::ASTCENC_SWZ_R;
                    swizzle.b = astcenc_swz::ASTCENC_SWZ_R;
                    swizzle.a = astcenc_swz::ASTCENC_SWZ_G;
                }
                else {
                    swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
                    swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
                    swizzle.b = astcenc_swz::ASTCENC_SWZ_0;
                    swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
                }
            }
            break;
            
        case 3:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_1;
            break;
            
        case 4:
            swizzle.r = astcenc_swz::ASTCENC_SWZ_R;
            swizzle.g = astcenc_swz::ASTCENC_SWZ_G;
            swizzle.b = astcenc_swz::ASTCENC_SWZ_B;
            swizzle.a = astcenc_swz::ASTCENC_SWZ_A;
            break;
            
        default:
            error.setErrorMessage("Unsupported number of components");
            astcenc_context_free(context);
            callbackContext.reset();
            return nullptr;
    }
    
    
    auto dataLength = _numBlocksWidth * _numBlocksHeight * _numBlocksDepth * 16;
    
    
    // Decompress image
    auto compressedData = reinterpret_cast<uint8_t*>(_contents);
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
    
    // Convert half float pixels to integers
//    if (_componentSize == 2) {
//        auto shorts = reinterpret_cast<unsigned short*>(content);
//        auto halfs = reinterpret_cast<__fp16*>(content);
//        for (auto j = 0; j < _height; j++) {
//            for (auto i = 0; i < _width; i++) {
//                auto index = j * _width * 4 + i * 4;
//                auto max = std::numeric_limits<unsigned short>::max();
//                for (auto i = 0; i < 4; i++) {
//#if 1
//                    auto value = static_cast<double>(halfs[index + i]);
//                    auto original = shorts[index + i];
//                    auto converted = static_cast<unsigned short>(round(value * static_cast<double>(max)));
//                    converted = ((converted << 8) & 0xFF00) | ((converted >> 8) & 0x00FF);
//                    shorts[index + i] = converted;
//#else
//
//                    auto value = shorts[index + i];
//                    value = ((value << 8) & 0xFF00) | ((value >> 8) & 0x00FF);
//                    shorts[index + i] = value;
//#endif
//                }
//            }
//        }
//    }
    
    return new ASTCRawImage(content, _width, _height, _depth, _originalNumComponents, _componentSize, _linear, _hdr, _containsAlpha, _ldrAlpha, _normalMap);
}


FN_IMPLEMENT_SWIFT_INTERFACE1(ASTCRawImage)
FN_IMPLEMENT_SWIFT_INTERFACE1(ASTCImage)
