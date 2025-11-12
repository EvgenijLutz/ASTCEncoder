// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
@_exported import ASTCEncoderC


public enum LibASTCError: Error {
    case other(_ message: String)
    case unknown
}


public extension ASTCErrorInfo {
    var error: LibASTCError {
        if let message = errorMessage {
            return .other(String(cString: message))
        }
        
        return .unknown
    }
}


extension ASTCBlockSize: @retroactive Hashable {
    public func hash(into hasher: inout Hasher) {
        hasher.combine(width)
        hasher.combine(height)
        hasher.combine(depth)
    }
}


public typealias ASTCCompressionQuality = Float
public extension ASTCCompressionQuality {
    static let fastest: ASTCCompressionQuality = 0
    static let fast: ASTCCompressionQuality = 10
    static let medium: ASTCCompressionQuality = 60
    static let thorough: ASTCCompressionQuality = 98
    static let veryThorough: ASTCCompressionQuality = 99
    static let exhaustive: ASTCCompressionQuality = 100
}


public extension ASTCRawImage {
    static func create(data: UnsafePointer<CChar>, width: Int, height: Int, depth: Int, numComponents: Int, componentSize: Int, integerComponents: Bool, littleEndian: Bool, linear: Bool, hdr: Bool, ldrAlpha: Bool) throws(LibASTCError) -> ASTCRawImage {
        var error = ASTCErrorInfo()
        let image = ASTCRawImage.__createUnsafe(data,
                                                width: width, height: height, depth: depth,
                                                numComponents: numComponents,
                                                componentSize: componentSize,
                                                integerComponents: integerComponents,
                                                littleEndian: littleEndian,
                                                linear: linear,
                                                hdr: hdr,
                                                ldrAlpha: ldrAlpha,
                                                error: &error)
        
        guard let image else {
            throw error.error
        }
        
        return image
    }
    
    
    func compress(blockSize: ASTCBlockSize, quality: ASTCCompressionQuality, _ progressCallback: @Sendable (_ progress: Float) -> Void = { _ in }) throws -> ASTCImage {
        return try withoutActuallyEscaping(progressCallback) { escapingClosure in
            struct CallbackContext: Sendable {
                var progressCallback: @Sendable (Float) -> Void
            }
            var callbackContext = CallbackContext(progressCallback: escapingClosure)
            
            return try withUnsafeMutablePointer(to: &callbackContext) { pointer in
                var error = ASTCErrorInfo()
                let image = __compressUnsafe(blockSize: blockSize,
                                             quality: quality,
                                             error: &error,
                                             userInfo: pointer) { userInfo, progress in
                    userInfo?.withMemoryRebound(to: CallbackContext.self, capacity: 1) { pointer in
                        pointer.pointee.progressCallback(progress)
                    }
                    
                    return Task.isCancelled
                }
                
                guard let image else {
                    throw error.error
                }
                
                return image
            }
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


public extension ASTCImage {
    func decompress() throws -> ASTCRawImage {
        var error = ASTCErrorInfo()
        let rawImage = __decompressUnsafe(error: &error, userInfo: nil, progressCallback: nil)
        
        guard let rawImage else {
            throw error.error
        }
        
        return rawImage
    }
}


#if canImport(CoreGraphics)

import CoreGraphics


public extension ASTCRawImage {
    func createCgImage(colorSpace: CGColorSpace? = nil, assumeSRGB: Bool = true) throws -> CGImage {
        guard let dataProvider = CGDataProvider(data: data as CFData) else {
            throw LibASTCError.other("No data provider :(")
        }
        
        
        let optionalName = assumeSRGB ? CGColorSpace.sRGB : CGColorSpace.genericRGBLinear
        guard let colorSpace = colorSpace ?? CGColorSpace(name: optionalName) else {
        //guard let colorSpace = colorSpace ?? CGColorSpace(name: CGColorSpace.sRGB) else {
            throw LibASTCError.other("Could not create color space")
        }
        
        
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
            throw LibASTCError.other("Unsupported component size: \(componentSize)")
        }
        
        
        let image = CGImage(
            width: width,
            height: height,
            bitsPerComponent: componentSize * 8,
            bitsPerPixel: componentSize * 8 * 4,
            bytesPerRow: width * componentSize * 4,
            space: colorSpace,
            bitmapInfo: .init(rawValue: CGImageAlphaInfo.last.rawValue | componentMask | orderMask),
            provider: dataProvider,
            decode: nil,
            shouldInterpolate: true,
            intent: .defaultIntent
        )
        guard let image else {
            throw LibASTCError.other("Could not create CGImage")
        }
        
        return image
    }
}

#endif
