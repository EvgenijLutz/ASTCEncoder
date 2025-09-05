// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
import ASTCEncoderC

#if canImport(CoreGraphics)
import CoreGraphics
#endif


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


public extension ASTCRawImage {
    static func create(data: UnsafeMutablePointer<CChar>, width: Int, height: Int, numComponents: Int, componentSize: Int, linear: Bool, hdr: Bool) throws(LibASTCError) -> ASTCRawImage {
        var error = ASTCErrorInfo()
        let image = ASTCRawImage.__createUnsafe(data, width: width, height: height,
                                                numComponents: numComponents,
                                                componentSize: componentSize,
                                                linear: linear, hdr: hdr,
                                                error: &error)
        
        guard let image else {
            throw error.error
        }
        
        return image
    }
    
    
    func compress(blockWidth: Int, blockHeight: Int, quality: Float, _ progressCallback: @Sendable (_ progress: Float) -> Void = { _ in }) throws -> ASTCImage {
        return try withoutActuallyEscaping(progressCallback) { escapingClosure in
            struct CallbackContext: Sendable {
                var progressCallback: @Sendable (Float) -> Void
            }
            var callbackContext = CallbackContext(progressCallback: escapingClosure)
            
            return try withUnsafeMutablePointer(to: &callbackContext) { pointer in
                var error = ASTCErrorInfo()
                let image = __compressUnsafe(blockWidth: blockWidth,
                                             blockHeight: blockHeight,
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

public extension ASTCRawImage {
    var cgImage: CGImage  {
        get throws {
            let contents = Data(bytes: data, count: dataSize)
            guard let dataProvider = CGDataProvider(data: contents as CFData) else {
                throw LibASTCError.other("No data provider :(")
            }
            
            //guard let colorSpace = CGColorSpace(name: CGColorSpace.extendedDisplayP3) else {
            //guard let colorSpace = CGColorSpace(name: CGColorSpace.displayP3) else {
            guard let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) else {
                throw LibASTCError.other("No color space :(")
            }
            
            let image = CGImage(
                width: width,
                height: height,
                bitsPerComponent: componentSize * 8,
                bitsPerPixel: componentSize * 8 * 4,
                bytesPerRow: width * componentSize * 4,
                space: colorSpace,
                bitmapInfo: .init(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrderDefault.rawValue),
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
}

#endif
