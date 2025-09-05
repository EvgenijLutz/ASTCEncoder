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


typedef bool (* ASTCEncoderProgressCallback)(void* __nullable userInfo, float progress);


/// Uncompressed image that is ready for ASTC compression.
///
/// At the moment it's a 2D image.
class ASTCRawImage {
private:
    std::atomic<size_t> referenceCounter;
    
    /*const*/ char* __nonnull _data;
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
    // TODO: Mark as initializer after Swift 6.2 release
    static ASTCRawImage* __nullable create(char* __nonnull data, long width, long height, long numComponents, long componentSize, bool linear, bool hdr, ASTCErrorInfo& error) SWIFT_NAME(__createUnsafe(_:width:height:numComponents:componentSize:linear:hdr:error:)) SWIFT_RETURNS_RETAINED;
    
    ASTCImage* __nullable compress(long blockWidth, long blockHeight, float quality, ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) SWIFT_NAME(__compressUnsafe(blockWidth:blockHeight:quality:error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    /*const*/ char* __nonnull getData() SWIFT_RETURNS_INDEPENDENT_VALUE SWIFT_COMPUTED_PROPERTY { return _data; }
    
    long getDataSize() SWIFT_COMPUTED_PROPERTY { return _width * _height * 4 * _componentSize; }
    
    long getWidth() SWIFT_COMPUTED_PROPERTY { return _width; }
    
    long getHeight() SWIFT_COMPUTED_PROPERTY { return _height; }
    
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
}
SWIFT_SHARED_REFERENCE(ASTCRawImageRetain, ASTCRawImageRelease)
SWIFT_UNCHECKED_SENDABLE;


/// ASTC compressed image
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
    
    
    ASTCImage(char* __nonnull data, long width, long height, long depth, long originalNumComponents, long componentSize, bool linear, bool hdr, long numBlocksWidth, long numBlocksHeight, long numBlocksDepth, long blockWidth, long blockHeight, long blockDepth);
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
SWIFT_SHARED_REFERENCE(ASTCImageRetain, ASTCImageRelease)
SWIFT_UNCHECKED_SENDABLE;


#endif // __cplusplus

#endif // ASTCEncoderC_hpp
