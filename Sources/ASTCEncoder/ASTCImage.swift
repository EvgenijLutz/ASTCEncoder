//
//  ASTCImage.swift
//  ASTCEncoder
//
//  Created by Evgenij Lutz on 17.08.26.
//

import Foundation
import ASTCEncoderC


// MARK: Decompression

public typealias ASTCDecompressorCallback = (_ progress: Float) -> Void


fileprivate struct ASTCDecompressorContext {
    var progressCallback: ASTCDecompressorCallback
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe @c fileprivate func astcEncoderDecompressCCallback(_ userInfo: UnsafeMutableRawPointer?, _ progress: Float) -> Bool {
    unsafe userInfo?.withMemoryRebound(to: ASTCDecompressorContext.self, capacity: 1) { pointer in
        unsafe pointer.pointee.progressCallback(progress)
    }
    
    return Task.isCancelled
}


@safe fileprivate func withASTCDecoderCallback<T>(_ callback: ASTCDecompressorCallback, action: (_ userInfo: UnsafeMutableRawPointer?) throws -> T) rethrows -> T {
    return try withoutActuallyEscaping(callback) { escapingClosure in
        var callbackContext = ASTCDecompressorContext(progressCallback: escapingClosure)
        return unsafe try withUnsafeMutablePointer(to: &callbackContext) { pointer in
            unsafe try action(pointer)
        }
    }
}


@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe public extension ASTCImage {
    func decompress(_ progressCallback: ASTCDecompressorCallback = { _ in }) throws -> ASTCRawImage {
        return try withASTCDecoderCallback(progressCallback) { userInfo in
            var error = ASTCError()
            let rawImage = unsafe __decompressUnsafe(error: &error, userInfo: userInfo, progressCallback: astcEncoderDecompressCCallback)
            
            guard let rawImage else {
                throw error
            }
            
            return rawImage
        }
    }
}
