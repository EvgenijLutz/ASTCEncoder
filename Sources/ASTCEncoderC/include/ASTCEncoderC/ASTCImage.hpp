//
//  ASTCImage.hpp
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 10.11.25.
//

#pragma once

#include <ASTCEncoderC/Common.hpp>
#include <string_view>


#define ASTC_ENCODER_ERROR_SIZE 128


struct ASTCImageEncoderContext;
class ASTCRawImage;
class ASTCImage;


/// Error description.
struct ASTCError final {
private:
    char _errorMessage[ASTC_ENCODER_ERROR_SIZE];
    
public:
    ASTCError();
    ASTCError(const char* fn_nonnull message fn_noescape);
    ASTCError(const ASTCError& other);
    ASTCError(ASTCError&& other);
    ~ASTCError();
    
    ASTCError& operator = (const ASTCError& other);
    ASTCError& operator = (ASTCError&& other);
    
    std::span<const char> getMessageSpan() const fn_lifetimebound SWIFT_COMPUTED_PROPERTY;
    const char* fn_nullable getErrorMessage() const fn_lifetimebound SWIFT_COMPUTED_PROPERTY;
    void setErrorMessage(const char* fn_nullable errorMessage fn_noescape) SWIFT_COMPUTED_PROPERTY;
};


/// ASTC block dimensions.
///
/// Access ``ASTCBlockSize``'s predefined static variables like ``ASTCBlockSize/_4x4-type.property`` to get valid block sizes.
struct ASTCBlockSize final {
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


struct ASTCImageEncoderContext final {
    void* fn_nullable owner;
    long astcXCount;
    long astcYCount;
    long astcZCount;
    ASTCBlockSize blockSize;
    size_t dataLength;
    char* fn_nullable astcData;
    
    ASTCImageEncoderContext() { }
    ~ASTCImageEncoderContext() { }
    
    [[nodiscard]] ASTCImage* fn_nonnull extractImage() const SWIFT_RETURNS_RETAINED;
}
SWIFT_UNCHECKED_SENDABLE;


/// ASTC encoder progress callback.
///
/// Specify a callback in the ``ASTCRawImage/compress-method`` method to track compression progress.
///
/// - Returns: `true` if encoder should stop encoding, otherwise `false`.
typedef bool (* ASTCEncoderProgressCallback)(void* fn_nullable userInfo, float progress, const ASTCImageEncoderContext& compressorInfo);


typedef bool (* ASTCDecoderProgressCallback)(void* fn_nullable userInfo, float progress);


// MARK: - ASTCRawImage

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
class ASTCRawImage final {
private:
    std::atomic<size_t> _referenceCounter;
    
    /*const*/ char* fn_nonnull _contents;
    const long _width;
    const long _height;
    const long _depth;
    const long _originalNumComponents;
    const long _componentSize;
    /// Linear or gamma-compressed. If the value is set to `false`, then the `_hdr` value is ignored and assumed to be `false`.
    const bool _linear;
    /// High dynamic range support. The `_linear` property has to be true.
    const bool _hdr;
    /// Image contains alpha channel. Allows more accurate enconding in opaque areas. Only valid if number of components is either `2` or `4`, otherwise ignored.
    const bool _containsAlpha;
    /// Only valid if ``_containsAlpha`` is `true`, otherwise ignored.
    const bool _ldrAlpha;
    /// Normal map.
    const bool _normalMap;
    
    
    friend struct ASTCImageEncoderContext;
    friend class ASTCImage;
    FN_FRIEND_SWIFT_INTERFACE(ASTCRawImage)
    
    
    ASTCRawImage(char* fn_nonnull contents, long width, long height, long depth, long originalNumComponents, long componentSize, bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap);
    ~ASTCRawImage();
    
public:
    /// Creates an instance of ``ASTCRawImage`` by copying image contents.
    ///
    /// The `contents` component values are expected to be either unsigned integers for standard dynamic range (`hdr = false`) or floating poins for extended dynamic range (`hdr = true` and `componentSize` is either `2` or `4`), depending on the `hdr` setting.
    ///
    /// - Parameter contents: image contens to copy in `R`, `RG`, `RGB` or `RGBA` pixel format.
    /// - Parameter width: width, surprise.
    /// - Parameter height: height.
    /// - Parameter depth: depth.
    /// - Parameter numComponents: number of components. The valid values are `1` (`R` - grey), `2` (`RG` - grey with alpha channel), `3` (`RGB` - red, green and blue channels) or `4` (`RGBA` - red, green, blue and alpha channels).
    /// - Parameter componentSize: pixel component size in **bytes**. The valid values are `1`, `2` or `4`.
    /// - Parameter integerComponents: wheter the input components are integers. If `true`, then it's unsigned integer that maps between `0` and `1` (max value depending on the `componentSize`). For floating point components, the `componentSize` should be at least two bytes, otherwise the parameter is ignored and assumed to be `true`.
    /// - Parameter littleEndian: whether the components are in little endian or big endian order. For instance, `png`'s contents are usually represented in big endian order.
    /// - Parameter linear: whether the colour components have `linear` or `sRGB` colour transfer function.
    /// - Parameter hdr: whether the components represent extended dynamic range. I that case, components in the `contents` input are expected to be floating point values (i.e. the `integerComponents` parameter should be `false`), `componentSize` should be either `2` (16-bit float) or `4` (32-bit float) and the `linear` parameter should be `true`, otherwise this parameter is ignored and assumed to be `false`.
    /// - Parameter containsAlpha: Image contains alpha channel. Allows more accurate enconding in opaque areas. Only valid if number of components is either `2` or `4`, otherwise this parameter is ignored.
    /// - Parameter ldrAlpha: whether the last channel of hdr texture is used as LDR alpha channel. This oprion is valid when `numComponents` is set to `4`, `hdr` setting is `true` and `containsAlpha` is `true`, otherwise this setting is ignored. This option tells ASTC encoder to prioritize quality for opaque regions.
    /// - Parameter normalMap: Indicates that it's a normal map. This property is only valid if `numComponents` is greater than or equals to `2` (`r` and `g` components will be used for encoding), `linear`=`true` and `hdr`=`false` and `containsAlpha`=`false`. The compresseor generates an RRRG ASTC image, the shader reads `x` and `y` components, the `z` component is reconstructed in the shader.
    /// - Parameter error: error description container is something goes wrong.
    ///
    /// - Returns: an instance of ``ASTCRawImage`` if all input data is valid. Otherwise `null` is returned and `error` will contain verbose information what went wrong.
    [[nodiscard]] static ASTCRawImage* fn_nullable create(const char* fn_nonnull contents fn_noescape, long width, long height, long depth, long numComponents, long componentSize, bool integerComponents, bool littleEndian, bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap, ASTCError& error) SWIFT_NAME(__createUnsafe(_:width:height:depth:numComponents:componentSize:integerComponents:littleEndian:linear:hdr:containsAlpha:ldrAlpha:normalMap:error:)) SWIFT_RETURNS_RETAINED;
    
    
    /// Compresses image contents using specified block size and compression quality into an ``ASTCImage``.
    ///
    /// - Parameter blockSize: compression block size.
    /// - Parameter quality: compression quality in range from `0` (fastest, lowest quality) to `100` (slowest, highest quality).
    /// - Parameter error: error information in case something goes wrong and no ``ASTCImage`` is returned.
    /// - Parameter userInfo: compression block size.
    /// - Parameter progressCallback: progress callback, accepts earlier passed `userInfo` and current progress in range from `0` (start) to `1` (finish).
    ///
    /// - Returns: an instance of ``ASTCImage`` on success. Otherwise `null` is returned and `error` will contain verbose information what went wrong.
    ///
    /// - Warning: This method is computationally intensive. Delegate its work to a concurrent thread or task to make sure that your working thread is not blocked. When delegating to a concurrent thread, don't forget to retain this object using the ``ASTCRawImageRetain`` function before passing to other context and ``ASTCRawImageRelease`` after finishing work on a concurrent thread to acheive proper retain/release cycles and make sure that the object does not get prematurely destroyed.
    [[nodiscard]] ASTCImage* fn_nullable compress(ASTCBlockSize blockSize, float quality, ASTCError& error, void* fn_nullable userInfo fn_noescape, ASTCEncoderProgressCallback fn_nullable progressCallback fn_noescape) SWIFT_NAME(__compressUnsafe(blockSize:quality:error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    
    // TODO: Migrate to @lifebound Spans by returning std::span when this Swift feature will be available
    /// Returns image contents.
    const char* fn_nonnull getContents() fn_lifetimebound SWIFT_COMPUTED_PROPERTY { return _contents; }
    /// Returns image contents size.
    long getContentsSize() SWIFT_COMPUTED_PROPERTY { return _width * _height * _depth * 4 * _componentSize; }
    
    
    /// Width of the image.
    long getWidth() SWIFT_COMPUTED_PROPERTY { return _width; }
    
    
    /// Height of the image.
    long getHeight() SWIFT_COMPUTED_PROPERTY { return _height; }
    
    
    /// Depth of the image.
    long getDepth() SWIFT_COMPUTED_PROPERTY { return _depth; }
    
    
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
    
    bool getLinear() SWIFT_COMPUTED_PROPERTY { return _linear; }
    bool getHDR() SWIFT_COMPUTED_PROPERTY { return _hdr; }
}
SWIFT_PRIVATE_FILEID("ASTCEncoder/ASTCRawImage.swift")
FN_SWIFT_INTERFACE(ASTCRawImage)
SWIFT_UNCHECKED_SENDABLE;


// MARK: - ASTCImage

/// ASTC compressed image.
///
/// - Note: This object is immutable. Thus, it's safe to use it from concurrent threads.
///
/// - Seealso: [Wikipedia](https://en.wikipedia.org/wiki/Adaptive_scalable_texture_compression), ["Adaptive Scalable Texture Compression" (PDF)](https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/nystad12_astc.pdf).
class ASTCImage final {
private:
    std::atomic<size_t> _referenceCounter;
    
    char* fn_nonnull _contents;
    const long _width;
    const long _height;
    const long _depth;
    const long _originalNumComponents;
    const long _componentSize;
    const bool _linear;
    const bool _hdr;
    const bool _containsAlpha;
    const bool _ldrAlpha;
    const bool _normalMap;
    
    const long _numBlocksWidth;
    const long _numBlocksHeight;
    const long _numBlocksDepth;
    const ASTCBlockSize _blockSize;
    
    
    friend struct ASTCImageEncoderContext;
    friend class ASTCRawImage;
    FN_FRIEND_SWIFT_INTERFACE(ASTCImage)
    
    
    ASTCImage(char* fn_nonnull contents,
              long width, long height, long depth,
              long originalNumComponents, long componentSize,
              bool linear, bool hdr, bool containsAlpha, bool ldrAlpha, bool normalMap,
              long numBlocksWidth, long numBlocksHeight, long numBlocksDepth,
              ASTCBlockSize blockSize);
    ~ASTCImage();
    
public:
    ASTCRawImage* fn_nullable decompress(ASTCError& error, void* fn_nullable userInfo fn_noescape, ASTCDecoderProgressCallback fn_nullable progressCallback fn_noescape) SWIFT_NAME(__decompressUnsafe(error:userInfo:progressCallback:)) SWIFT_RETURNS_RETAINED;
    
    long getWidth() SWIFT_COMPUTED_PROPERTY { return _width; }
    long getHeight() SWIFT_COMPUTED_PROPERTY { return _height; }
    long getDepth() SWIFT_COMPUTED_PROPERTY { return _depth; }
    
    /// Number of components of decompressed image.
    ///
    /// Expected values:
    /// - `0` - unknown;
    /// - `1` - greyscale;
    /// - `2` - greyscale with alpha channel;
    /// - `3` - RGB;
    /// - `4` - RGBA.
    long getNumberOfComponents() SWIFT_COMPUTED_PROPERTY { return _originalNumComponents; }
    
    /// Size of each component in bytes of decompressed image.
    ///
    /// Expected values are `1` (8 bit uint), `2` (16 bit float) and `4` (32 bit float).
    long getComponentSize() SWIFT_COMPUTED_PROPERTY { return _componentSize; }
    
    bool getLinear() SWIFT_COMPUTED_PROPERTY { return _linear; }
    bool getHdr() SWIFT_COMPUTED_PROPERTY { return _hdr; }
    bool getContainsAlpha() SWIFT_COMPUTED_PROPERTY { return _containsAlpha; }
    bool getHdrAlpha() SWIFT_COMPUTED_PROPERTY { return _ldrAlpha; }
    bool getNormalMap() SWIFT_COMPUTED_PROPERTY { return _normalMap; }
    
    long getNumBlocksWidth() SWIFT_COMPUTED_PROPERTY { return _numBlocksWidth; }
    long getNumBlocksHeight() SWIFT_COMPUTED_PROPERTY { return _numBlocksHeight; }
    long getNumBlocksDepth() SWIFT_COMPUTED_PROPERTY { return _numBlocksDepth; }
    ASTCBlockSize getBlockSize() SWIFT_COMPUTED_PROPERTY { return _blockSize; }
    
    const char* fn_nonnull getContents() fn_lifetimebound SWIFT_COMPUTED_PROPERTY { return _contents; }
    long getContentsSize() SWIFT_COMPUTED_PROPERTY { return _numBlocksWidth * _numBlocksHeight * _numBlocksDepth * 16; }
}
FN_SWIFT_INTERFACE(ASTCImage)
SWIFT_UNCHECKED_SENDABLE;


FN_DEFINE_SWIFT_INTERFACE(ASTCRawImage)
FN_DEFINE_SWIFT_INTERFACE(ASTCImage)
