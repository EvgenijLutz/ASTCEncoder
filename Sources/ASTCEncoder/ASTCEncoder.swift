// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
//import astcenc
@_exported import ASTCEncoderC


public typealias ASTCEncoderCallback = (_ progress: Float) -> Void

fileprivate struct CallbackContext {
    var progressCallback: ASTCEncoderCallback
}

@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
//@c
@_cdecl("astcEncoderCCallback")
fileprivate func astcEncoderCCallback(_ userInfo: UnsafeMutableRawPointer?, _ progress: Float) -> Bool {
    userInfo?.withMemoryRebound(to: CallbackContext.self, capacity: 1) { pointer in
        pointer.pointee.progressCallback(progress)
    }
    
    return Task.isCancelled
}

fileprivate func withASTCEncoderCallback<T>(_ callback: ASTCEncoderCallback, action: (_ userInfo: UnsafeMutableRawPointer?) throws -> T) rethrows -> T {
    return try withoutActuallyEscaping(callback) { escapingClosure in
        var callbackContext = CallbackContext(progressCallback: escapingClosure)
        return try withUnsafeMutablePointer(to: &callbackContext) { pointer in
            try action(pointer)
        }
    }
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
extension ASTCError: @retroactive Error, @retroactive CustomStringConvertible {
    init(_ message: String) {
        self.init()
        message.withCString { pointer in
            setErrorMessage(pointer)
        }
    }
    
    var message: String {
        guard let errorMessage else {
            return "<no message>"
        }
        
        return String(cString: errorMessage)
    }
    
    public var description: String {
        message
    }
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
extension ASTCBlockSize: @retroactive Hashable {
    public func hash(into hasher: inout Hasher) {
        hasher.combine(width)
        hasher.combine(height)
        hasher.combine(depth)
    }
}


public typealias ASTCCompressionQuality = Float
public extension ASTCCompressionQuality {
    static let fastest: ASTCCompressionQuality = ASTCENC_PRE_FASTEST
    static let fast: ASTCCompressionQuality = ASTCENC_PRE_FAST
    static let medium: ASTCCompressionQuality = ASTCENC_PRE_MEDIUM
    static let thorough: ASTCCompressionQuality = ASTCENC_PRE_THOROUGH
    static let veryThorough: ASTCCompressionQuality = ASTCENC_PRE_VERYTHOROUGH
    static let exhaustive: ASTCCompressionQuality = ASTCENC_PRE_EXHAUSTIVE
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
public extension ASTCRawImage {
    static func create(data: UnsafePointer<CChar>, width: Int, height: Int, depth: Int, numComponents: Int, componentSize: Int, integerComponents: Bool, littleEndian: Bool, linear: Bool, hdr: Bool, containsAlpha: Bool, ldrAlpha: Bool, normalMap: Bool) throws(ASTCError) -> ASTCRawImage {
        var error = ASTCError()
        let image = ASTCRawImage.__createUnsafe(data,
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
    
    
    func compress(blockSize: ASTCBlockSize, quality: ASTCCompressionQuality, _ progressCallback: @Sendable (_ progress: Float) -> Void = { _ in }) throws -> ASTCImage {
        return try withASTCEncoderCallback(progressCallback) { userInfo in
            var error = ASTCError()
            let image = __compressUnsafe(blockSize: blockSize,
                                         quality: quality,
                                         error: &error,
                                         userInfo: userInfo,
                                         progressCallback: astcEncoderCCallback)
            
            guard let image else {
                throw error
            }
            
            return image
        }
    }
    
    
    func compress(blockSize: ASTCBlockSize, quality: ASTCCompressionQuality, _ progressCallback: @escaping @Sendable (_ progress: Float) -> Void = { _ in }) async throws -> ASTCImage {
        try await Task {
            try compress(blockSize: blockSize, quality: quality, progressCallback)
        }.value
    }
    
    
    func withData(_ action: (_ data: borrowing Data) throws -> Void) rethrows {
        let contents = _contents
        let data = Data(bytesNoCopy: contents, count: contentsSize, deallocator: .none)
        try action(data)
    }
    
    var data: Data {
        Data(bytes: _contents, count: contentsSize)
    }
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
public extension ASTCImage {
    func decompress(_ progressCallback: @Sendable (_ progress: Float) -> Void = { _ in }) throws -> ASTCRawImage {
        return try withASTCEncoderCallback(progressCallback) { userInfo in
            var error = ASTCError()
            let rawImage = __decompressUnsafe(error: &error,
                                              userInfo: userInfo,
                                              progressCallback: astcEncoderCCallback)
            
            guard let rawImage else {
                throw error
            }
            
            return rawImage
        }
    }
}


#if canImport(CoreGraphics)

import CoreGraphics


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
public extension ASTCRawImage {
    func createCgImage(colorSpace: CGColorSpace? = nil, assumeSRGB: Bool = true, hdr: Bool = false) throws -> CGImage {
        guard let dataProvider = CGDataProvider(data: data as CFData) else {
            throw ASTCError("No data provider :(")
        }
        
        
        let colorSpace: CGColorSpace = try {
            let optionalName = assumeSRGB ? CGColorSpace.sRGB : CGColorSpace.genericRGBLinear
            guard let colorSpace = colorSpace ?? CGColorSpace(name: optionalName) else {
                //guard let colorSpace = colorSpace ?? CGColorSpace(name: CGColorSpace.sRGB) else {
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
        
        
        let image = CGImage(
            width: width,
            height: height,
            bitsPerComponent: componentSize * 8,
            bitsPerPixel: componentSize * 8 * 4,
            bytesPerRow: width * componentSize * 4,
            space: colorSpace,
            bitmapInfo: .init(rawValue: alphaMask | componentMask | orderMask),
            provider: dataProvider,
            decode: nil,
            shouldInterpolate: true,
            intent: .defaultIntent
        )
        guard let image else {
            throw ASTCError("Could not create CGImage")
        }
        
        return image
    }
}

#endif
