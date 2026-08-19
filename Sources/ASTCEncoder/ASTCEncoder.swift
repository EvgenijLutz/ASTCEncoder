// The Swift Programming Language
// https://docs.swift.org/swift-book

import Foundation
@_exported import ASTCEncoderC


// MARK: ASTCError

@available(macOS 13.3, iOS 16.4, tvOS 16.4, watchOS 9.4, visionOS 1.0, *)
@safe extension ASTCError: @retroactive Error, @retroactive CustomStringConvertible {
    init(_ message: String) {
        self.init()
        unsafe message.withCString { pointer in
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
public extension ASTCCompressionQuality {
    /// The fastest, lowest quality, search preset.
    static let fastest: ASTCCompressionQuality = ASTCENC_PRE_FASTEST
    
    /// The fast search preset.
    static let fast: ASTCCompressionQuality = ASTCENC_PRE_FAST
    
    /// The medium quality search preset.
    static let medium: ASTCCompressionQuality = ASTCENC_PRE_MEDIUM
    
    /// The thorough quality search preset.
    static let thorough: ASTCCompressionQuality = ASTCENC_PRE_THOROUGH
    
    /// The thorough quality search preset.
    static let veryThorough: ASTCCompressionQuality = ASTCENC_PRE_VERYTHOROUGH
    
    /// The exhaustive, highest quality, search preset.
    static let exhaustive: ASTCCompressionQuality = ASTCENC_PRE_EXHAUSTIVE
}
