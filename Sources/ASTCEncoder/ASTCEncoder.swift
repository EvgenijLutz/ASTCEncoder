// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
@_exported import ASTCEncoderC


// MARK: ASTCError

@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe extension ASTCError: @retroactive Error, @retroactive CustomStringConvertible {
    init(_ message: String) {
        self.init()
        message.withCString { pointer in
            unsafe setErrorMessage(pointer)
        }
    }
    
    var message: String {
        guard let errorMessage = unsafe errorMessage else {
            return "<no message>"
        }
        
        return unsafe String(cString: errorMessage)
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
@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
public extension ASTCCompressionQuality {
    /// The fastest, lowest quality, search preset.
    static let fastest: ASTCCompressionQuality = ASTCImage.compressionQualityFastest
    
    /// The fast search preset.
    static let fast: ASTCCompressionQuality = ASTCImage.compressionQualityFast
    
    /// The medium quality search preset.
    static let medium: ASTCCompressionQuality = ASTCImage.compressionQualityMedium
    
    /// The thorough quality search preset.
    static let thorough: ASTCCompressionQuality = ASTCImage.compressionQualityThorough
    
    /// The thorough quality search preset.
    static let veryThorough: ASTCCompressionQuality = ASTCImage.compressionQualityVeryThorough
    
    /// The exhaustive, highest quality, search preset.
    static let exhaustive: ASTCCompressionQuality = ASTCImage.compressionQualityExhaustive
}
