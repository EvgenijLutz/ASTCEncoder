//
//  ASTCRawImage.swift
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 17.08.26.
//

import Foundation
import ASTCEncoderC


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe public extension ASTCRawImage {
    static func create(data: UnsafePointer<CChar>, width: Int, height: Int, depth: Int, numComponents: Int, componentSize: Int, integerComponents: Bool, littleEndian: Bool, linear: Bool, hdr: Bool, containsAlpha: Bool, ldrAlpha: Bool, normalMap: Bool) throws(ASTCError) -> ASTCRawImage {
        var error = ASTCError()
        let image = unsafe ASTCRawImage.__createUnsafe(data,
                                                       width: width, height: height, depth: depth,
                                                       numComponents: numComponents,
                                                       componentSize: componentSize,
                                                       integerComponents: integerComponents,
                                                       littleEndian: littleEndian,
                                                       linear: linear,
                                                       hdr: hdr,
                                                       containsAlpha: containsAlpha,
                                                       ldrAlpha: ldrAlpha,
                                                       normalMap: normalMap,
                                                       error: &error)
        guard let image else {
            throw error
        }
        
        return image
    }
    
    
    /// Executes a closure with data without copying it.
    ///
    /// Don't let data escape the closure. Explicitly copy it if you need it outside the closure.
    @unsafe func withData<T>(_ action: (_ data: borrowing Data) throws -> T) rethrows -> T {
        let contents = unsafe _contents
        let data = unsafe Data(bytesNoCopy: contents, count: contentsSize, deallocator: .none)
        return try action(data)
    }
    
    
    /// Returns a copy of contents.
    var data: Data {
        unsafe Data(bytes: _contents, count: contentsSize)
    }
}


// MARK: Compression

public typealias ASTCEncoderCallback = (_ progress: Float, _ info: ASTCImageEncoderContext) -> Void


@safe fileprivate struct CallbackContext {
    var progressCallback: ASTCEncoderCallback
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe public func astcEncoderCCallback(_ userInfo: UnsafeMutableRawPointer?, _ progress: Float, _ info: ASTCImageEncoderContext) -> Bool {
    unsafe userInfo?.withMemoryRebound(to: CallbackContext.self, capacity: 1) { pointer in
        unsafe pointer.pointee.progressCallback(progress, info)
    }
    
    return Task.isCancelled
}


@safe public func withASTCEncoderCallback<T>(_ callback: ASTCEncoderCallback, action: (_ userInfo: UnsafeMutableRawPointer?) throws -> T) rethrows -> T {
    return unsafe try withoutActuallyEscaping(callback) { escapingClosure in
        var callbackContext = unsafe CallbackContext(progressCallback: escapingClosure)
        return unsafe try withUnsafeMutablePointer(to: &callbackContext) { pointer in
            unsafe try action(pointer)
        }
    }
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe public extension ASTCRawImage {
    /// Compresses image contents using specified block size and compression quality into an ``ASTCImage``.
    ///
    /// - Parameter blockSize: compression block size.
    /// - Parameter quality: compression quality in range from `0` (fastest, lowest quality) to `100` (slowest, highest quality). The ``ASTCCompressionQuality`` extension contains predefined options such as ``ASTCCompressionQuality.medium``.
    /// - Parameter progressCallback: progress callback, accepts current progress in range from `0` (start) to `1` (finish) and compression info that can be used to retreive current image.
    ///
    /// - Returns: an instance of ``ASTCImage`` on success. Otherwise an ``ASTCError`` error is thrown containing verbose information what went wrong.
    ///
    /// - Warning: This method is computationally intensive. Delegate its work to a concurrent thread or task to make sure that your working thread is not blocked. When delegating to a concurrent thread, don't forget to retain this object using the ``ASTCRawImageRetain`` function before passing to other context and ``ASTCRawImageRelease`` after finishing work on a concurrent thread to acheive proper retain/release cycles and make sure that the object does not get prematurely destroyed.
    func compress(blockSize: ASTCBlockSize, quality: ASTCCompressionQuality, _ progressCallback: ASTCEncoderCallback = { _, _ in }) throws -> ASTCImage {
        return try withASTCEncoderCallback(progressCallback) { userInfo in
            var error = ASTCError()
            let image = unsafe __compressUnsafe(blockSize: blockSize, quality: quality, error: &error, userInfo: userInfo) { userInfo, progress, info in
                astcEncoderCCallback(userInfo, progress, info)
            }
            
            guard let image else {
                throw error
            }
            
            return image
        }
    }
}


// MARK: CoreGraphics port

#if canImport(CoreGraphics)

import CoreGraphics


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe public extension ASTCRawImage {
    func createCgImage(colorSpace: CGColorSpace? = nil, assumeSRGB: Bool = true, hdr: Bool = false) throws -> CGImage {
        guard let dataProvider = CGDataProvider(data: data as CFData) else {
            throw ASTCError("No data provider :(")
        }
        
        
        let colorSpace: CGColorSpace = try {
            let optionalName = assumeSRGB ? CGColorSpace.sRGB : CGColorSpace.genericRGBLinear
            guard let colorSpace = colorSpace ?? CGColorSpace(name: optionalName) else {
                throw ASTCError("Could not create color space")
            }
            
            if hdr {
                guard let hdrColorSpace = CGColorSpaceCreateExtended(colorSpace) else {
                    throw ASTCError("Could not create HDR color space")
                }
                return hdrColorSpace
            }
            
            return colorSpace
        }()
        
        
        let componentMask: UInt32
        let orderMask: UInt32
        switch componentSize {
        case 1:
            componentMask = CGImageComponentInfo.integer.rawValue
            orderMask = CGImageByteOrderInfo.orderDefault.rawValue
            
        case 2:
            componentMask = CGImageComponentInfo.float.rawValue
            orderMask = CGImageByteOrderInfo.order16Little.rawValue
            
        case 4:
            componentMask = CGImageComponentInfo.float.rawValue
            orderMask = CGImageByteOrderInfo.order32Little.rawValue
            
        default:
            throw ASTCError("Unsupported component size: \(componentSize)")
        }
        
        let alphaMask: UInt32
        if _containsAlpha {
            alphaMask = CGImageAlphaInfo.last.rawValue
        }
        else {
            alphaMask = CGImageAlphaInfo.noneSkipLast.rawValue
        }
        
        
        func createImage(_ provider: CGDataProvider) -> CGImage? {
            if hdr, #available(macOS 15.0, iOS 18.0, *) {
                return unsafe CGImage(
                    headroom: 8,
                    width: width,
                    height: height,
                    bitsPerComponent: componentSize * 8,
                    bitsPerPixel: componentSize * 8 * 4,
                    bytesPerRow: width * componentSize * 4,
                    space: colorSpace,
                    bitmapInfo: .init(rawValue: alphaMask | componentMask | orderMask),
                    provider: provider,
                    decode: nil,
                    shouldInterpolate: true,
                    intent: .defaultIntent)
            } else {
                return unsafe CGImage(
                    width: width,
                    height: height,
                    bitsPerComponent: componentSize * 8,
                    bitsPerPixel: componentSize * 8 * 4,
                    bytesPerRow: width * componentSize * 4,
                    space: colorSpace,
                    bitmapInfo: .init(rawValue: alphaMask | componentMask | orderMask),
                    provider: provider,
                    decode: nil,
                    shouldInterpolate: true,
                    intent: .defaultIntent
                )
            }
        }
        
        let image = createImage(dataProvider)
        guard let image else {
            throw ASTCError("Could not create CGImage")
        }
        
        return image
    }
}

#endif
