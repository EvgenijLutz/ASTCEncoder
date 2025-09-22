//
//  ASTCEncoderC.hpp
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 26.08.25.
//

#ifndef ASTCEncoderC_hpp
#define ASTCEncoderC_hpp

#if defined __cplusplus

#include <swift/bridging>
#include <atomic>
#include <string_view>


#define ASTC_ENCODER_ERROR_SIZE 128


class ASTCRawImage;
class ASTCImage;


/// Error description.
struct ASTCErrorInfo final {
private:
    char _errorMessage[ASTC_ENCODER_ERROR_SIZE];
    
public:
    ASTCErrorInfo();
    ASTCErrorInfo(const ASTCErrorInfo& other);
    ASTCErrorInfo(ASTCErrorInfo&& other);
    ~ASTCErrorInfo();
    
    ASTCErrorInfo& operator = (const ASTCErrorInfo& other);
    ASTCErrorInfo& operator = (ASTCErrorInfo&& other);
    
    const char* __nullable getErrorMessage() const SWIFT_COMPUTED_PROPERTY;
    void setErrorMessage(const char* __nullable errorMessage) SWIFT_COMPUTED_PROPERTY;
};


/// ASTC block dimensions.
struct ASTCBlockSize {
    long width;
    long height;
    long depth;
    
    
    const static ASTCBlockSize _4x4;
    const static ASTCBlockSize _5x4;
    const static ASTCBlockSize _5x5;
    const static ASTCBlockSize _6x5;
    const static ASTCBlockSize _6x6;
    const static ASTCBlockSize _8x5;
    const static ASTCBlockSize _8x6;
    const static ASTCBlockSize _8x8;
    const static ASTCBlockSize _10x5;
    const static ASTCBlockSize _10x6;
    const static ASTCBlockSize _10x8;
    const static ASTCBlockSize _10x10;
    const static ASTCBlockSize _12x10;
    const static ASTCBlockSize _12x12;
    
    const static ASTCBlockSize _3x3x3;
    const static ASTCBlockSize _4x3x3;
    const static ASTCBlockSize _4x4x3;
    const static ASTCBlockSize _4x4x4;
    const static ASTCBlockSize _5x4x4;
    const static ASTCBlockSize _5x5x4;
    const static ASTCBlockSize _5x5x5;
    const static ASTCBlockSize _6x5x5;
    const static ASTCBlockSize _6x6x5;
    const static ASTCBlockSize _6x6x6;
    
    
    ASTCBlockSize(long width, long height, long depth) {
        this->width = width;
        this->height = height;
        this->depth = depth;
    }
    
    bool operator == (const ASTCBlockSize& other) const;
};


/// ASTC encoder progress callback.
///
/// Specify a callback in the ``ASTCRawImage/compress`` method to track compression progress.
///
/// - Returns: `true` if encoder should stop encoding, otherwise `false`.
typedef bool (* ASTCEncoderProgressCallback)(void* __nullable userInfo, float progress);


/// Uncompressed image that is ready for ASTC compression.
///
/// At the moment it's a 2D image.
class ASTCRawImage {
private:
    std::atomic<size_t> referenceCounter;
    
    char* __nonnull _data;
    const long _width;
    const long _height;
    const long _originalNumComponents;
    const long _componentSize;
    const bool _linear;
    const bool _hdr;
    
    
    friend ASTCRawImage* __nullable ASTCRawImageRetain(ASTCRawImage* __nullable image) SWIFT_RETURNS_UNRETAINED;
    friend void ASTCRawImageRelease(ASTCRawImage* __nullable image);
    
    friend class ASTCImage;
    
    
    ASTCRawImage(char* __nonnull data, long width, long height, long originalNumComponents, long componentSize, bool linear, bool hdr);
    ~ASTCRawImage();
    
public:
    static ASTCRawImage* __nullable create(char* __nonnull data, long width, long height, long numComponents, long componentSize, bool linear, bool hdr, ASTCErrorInfo& error) SWIFT_NAME(__createUnsafe(_:width:height:numComponents:componentSize:linear:hdr:error:)) SWIFT_RETURNS_RETAINED;
    
    ASTCImage* __nullable compress(ASTCBlockSize blockDimensions, float quality, ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) SWIFT_NAME(__compressUnsafe(blockSize:quality:error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    // TODO: Migrate to @lifebound Spans by returning std::span when this Swift feature will be available
    /// Returns image contents.
    const char* __nonnull getContents() SWIFT_RETURNS_INDEPENDENT_VALUE SWIFT_COMPUTED_PROPERTY { return _data; }
    /// Returns image contents size.
    long getContentsSize() SWIFT_COMPUTED_PROPERTY { return _width * _height * 4 * _componentSize; }
    
    /// Width of the image.
    long getWidth() SWIFT_COMPUTED_PROPERTY { return _width; }
    
    /// Height of the image.
    long getHeight() SWIFT_COMPUTED_PROPERTY { return _height; }
    
    /// Original number of components, `0` if unknown.
    ///
    /// Expected values:
    /// - `1` - greyscale;
    /// - `2` - greyscale with alpha channel;
    /// - `3` - RGB;
    /// - `4` - RGBA.
    long getOriginalNumComponents() SWIFT_COMPUTED_PROPERTY { return _originalNumComponents; }
    
    /// Component size in bytes.
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
}
SWIFT_PRIVATE_FILEID("ASTCEncoder/ASTCEncoder.swift")
SWIFT_SHARED_REFERENCE(ASTCRawImageRetain, ASTCRawImageRelease);


/// ASTC compressed image.
///
/// - Seealso: [Wikipedia](https://en.wikipedia.org/wiki/Adaptive_scalable_texture_compression), ["Adaptive Scalable Texture Compression" (PDF)](https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/nystad12_astc.pdf).
class ASTCImage {
private:
    std::atomic<size_t> referenceCounter;
    
    /*const*/ char* __nonnull _data;
    const long _width;
    const long _height;
    const long _depth;
    const long _originalNumComponents;
    const long _componentSize;
    const bool _linear;
    const bool _hdr;
    
    const long _numBlocksWidth;
    const long _numBlocksHeight;
    const long _numBlocksDepth;
    
    const long _blockWidth;
    const long _blockHeight;
    const long _blockDepth;
    
    
    friend ASTCImage* __nullable ASTCImageRetain(ASTCImage* __nullable image) SWIFT_RETURNS_UNRETAINED;
    friend void ASTCImageRelease(ASTCImage* __nullable image);
    
    friend class ASTCRawImage;
    
    
    ASTCImage(char* __nonnull data,
              long width, long height, long depth,
              long originalNumComponents, long componentSize,
              bool linear, bool hdr,
              long numBlocksWidth, long numBlocksHeight, long numBlocksDepth,
              long blockWidth, long blockHeight, long blockDepth);
    ~ASTCImage();
    
public:
    ASTCRawImage* __nullable decompress(ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) SWIFT_NAME(__decompressUnsafe(error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    /// Number of components of decompressed image.
    ///
    /// Expected values:
    /// - `1` - greyscale;
    /// - `2` - greyscale with alpha channel;
    /// - `3` - RGB;
    /// - `4` - RGBA.
    long getNumberOfComponents() SWIFT_COMPUTED_PROPERTY { return _originalNumComponents; }
    
    /// Size of each component in bytes of decompressed image.
    ///
    /// Expected values are `1` (8 bit), `2` (16 bit) and `4` (32 bit).
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
    
    //long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
    
    const char* __nonnull getData() SWIFT_RETURNS_INDEPENDENT_VALUE SWIFT_COMPUTED_PROPERTY { return _data; }
}
SWIFT_SHARED_REFERENCE(ASTCImageRetain, ASTCImageRelease);


#endif

#endif
