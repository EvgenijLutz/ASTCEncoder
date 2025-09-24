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
///
/// Access ``ASTCBlockSize``'s predefined static variables like ``ASTCBlockSize/_4x4-type.property`` to get valid block sizes.
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
    
    
    ASTCBlockSize() {
        this->width = 0;
        this->height = 0;
        this->depth = 0;
    }
    
    ASTCBlockSize(long width, long height, long depth) {
        this->width = width;
        this->height = height;
        this->depth = depth;
    }
    
    bool operator == (const ASTCBlockSize& other) const;
} SWIFT_CONFORMS_TO_PROTOCOL(Swift.Equatable);


/// ASTC encoder progress callback.
///
/// Specify a callback in the ``ASTCRawImage/compress-method`` method to track compression progress.
///
/// - Returns: `true` if encoder should stop encoding, otherwise `false`.
typedef bool (* ASTCEncoderProgressCallback)(void* __nullable userInfo, float progress);


/// Uncompressed image that is ready for ASTC compression.
///
/// The internal representation has always 4 channels in RGBA order. Every component is stored in the **little endian** system. You acces each component depending of the number of original components:
///
/// ```plain
/// | Input data   | Encoding swizzle | Sampling swizzle |
/// | ------------ | ---------------- | ---------------- |
/// | 1 component  | RRR1             | .[rgb]           |
/// | 2 components | RRRG             | .[rgb]a          |
/// | 3 components | RGB1             | .rgb             |
/// | 4 components | RGBA             | .rgba            |
/// ```
///
/// - Note: This object is immutable. Thus, it's safe to use it from concurrent threads. At the moment it's a 2D image, **depth support coming soon**.
class ASTCRawImage {
private:
    std::atomic<size_t> _referenceCounter;
    
    /*const*/ char* __nonnull _contents;
    const long _width;
    const long _height;
    const long _originalNumComponents;
    const long _componentSize;
    /// Linear or sRGB
    const bool _linear;
    const bool _hdr;
    
    
    friend ASTCRawImage* __nullable ASTCRawImageRetain(ASTCRawImage* __nullable image) SWIFT_RETURNS_UNRETAINED;
    friend void ASTCRawImageRelease(ASTCRawImage* __nullable image);
    
    friend class ASTCImage;
    
    
    ASTCRawImage(char* __nonnull contents, long width, long height, long originalNumComponents, long componentSize, bool linear, bool hdr);
    ~ASTCRawImage();
    
public:
    /// Creates an instance of ``ASTCRawImage`` by copying image contents.
    ///
    /// The `contents` component values are expected to be either unsigned integers for standard dynamic range (`hdr = false`) or floating poins for extended dynamic range (`hdr = true` and `componentSize` is either `2` or `4`), depending on the `hdr` setting.
    ///
    /// - Parameter contents: image contens to copy in `R`, `RG`, `RGB` or `RGBA` pixel format.
    /// - Parameter width: width, surprise.
    /// - Parameter height: height.
    /// - Parameter numComponents: number of components. The valid values are `1` (`R` - grey), `2` (`RG` - grey with alpha channel), `3` (`RGB` - red, green and blue channels) or `4` (`RGBA` - red, green, blue and alpha channels).
    /// - Parameter componentSize: pixel component size in **bytes**. The valid values are `1`, `2` or `4`.
    /// - Parameter littleEndian: whether the components are in little endian or big endian order. For instance, `png`'s contents are usually represented in big endian order.
    /// - Parameter linear: whether the components are in `linear` or `sRGB` (gamma-compressed) colour space. If the `hdr` setting is `true`, then this value is ignored and assumed to be `true`.
    /// - Parameter hdr: whether the components represent extended dynamic range. I that case, components in the `contents` input are expected to be floating point values and `componentSize` should be either `2` (16-bit float) or `4` (32-bit float).
    /// - Parameter error: error description container is something goes wrong.
    ///
    /// - Returns: an instance of ``ASTCRawImage`` if all input data is valid. Otherwise `null` is returned and `error` will contain verbose information what went wrong.
    static ASTCRawImage* __nullable create(char* __nonnull contents, long width, long height, long numComponents, long componentSize, /*componentType = integer, float */ bool littleEndian, bool linear, bool hdr, ASTCErrorInfo& error) SWIFT_NAME(__createUnsafe(_:width:height:numComponents:componentSize:littleEndian:linear:hdr:error:)) SWIFT_RETURNS_RETAINED;
    
    
    /// Compresses image contents using specified block size and compression quality into an ``ASTCImage``.
    ///
    /// - Returns: an instance of ``ASTCImage`` if all input data is valid. Otherwise `null` is returned and `error` will contain verbose information what went wrong.
    ///
    /// - Warning: This method is computationally intensive. Delegate its work to a concurrent thread or task to make sure that your working thread is not blocked. When delegating to a concurrent thread, don't forget to retain this object using the ``ASTCRawImageRetain`` function before passing to other context and ``ASTCRawImageRelease`` after finishing work on a concurrent thread to acheive proper retain/release cycles and make sure that the object does not get prematurely destroyed.
    ASTCImage* __nullable compress(ASTCBlockSize blockSize, float quality, ASTCErrorInfo& error, void* __nullable userInfo, ASTCEncoderProgressCallback __nullable progressCallback) SWIFT_NAME(__compressUnsafe(blockSize:quality:error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    
    // TODO: Migrate to @lifebound Spans by returning std::span when this Swift feature will be available
    /// Returns image contents.
    const char* __nonnull getContents() SWIFT_RETURNS_INDEPENDENT_VALUE SWIFT_COMPUTED_PROPERTY { return _contents; }
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
    ///
    /// Expected values are `1` (8 bit uint), `2` (16 bit float) and `4` (32 bit float).
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
}
SWIFT_UNCHECKED_SENDABLE
SWIFT_PRIVATE_FILEID("ASTCEncoder/ASTCEncoder.swift")
SWIFT_SHARED_REFERENCE(ASTCRawImageRetain, ASTCRawImageRelease);


/// ASTC compressed image.
///
/// - Note: This object is immutable. Thus, it's safe to use it from concurrent threads.
///
/// - Seealso: [Wikipedia](https://en.wikipedia.org/wiki/Adaptive_scalable_texture_compression), ["Adaptive Scalable Texture Compression" (PDF)](https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/nystad12_astc.pdf).
class ASTCImage {
private:
    std::atomic<size_t> _referenceCounter;
    
    /*const*/ char* __nonnull _contents;
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
    
    
    ASTCImage(char* __nonnull contents,
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
    /// Expected values are `1` (8 bit uint), `2` (16 bit float) and `4` (32 bit float).
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
    
    const char* __nonnull getContents() SWIFT_RETURNS_INDEPENDENT_VALUE SWIFT_COMPUTED_PROPERTY { return _contents; }
}
SWIFT_UNCHECKED_SENDABLE
SWIFT_SHARED_REFERENCE(ASTCImageRetain, ASTCImageRelease);


#endif

#endif
