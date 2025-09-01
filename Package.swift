// swift-tools-version: 6.1
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "ASTCEncoder",
    // See the "Minimum Deployment Version for Reference Types Imported from C++":
    // https://www.swift.org/documentation/cxx-interop/status/
    platforms: [
        .macOS(.v14),
        .iOS(.v17),
        .tvOS(.v17),
        .watchOS(.v10),
        .visionOS(.v1)
    ],
    products: [
        .library(
            name: "astcenc",
            targets: ["astcenc"]
        ),
        .library(
            name: "ASTCEncoderC",
            targets: ["ASTCEncoderC"]
        ),
        .library(
            name: "ASTCEncoder",
            targets: ["ASTCEncoder"]
        ),
    ],
    targets: [
        .binaryTarget(
            name: "astcenc",
            path: "Binaries/astcenc.xcframework"
        ),
        .target(
            name: "ASTCEncoderC",
            dependencies: [
                .target(name: "astcenc")
            ]
        ),
        .target(
            name: "ASTCEncoder",
            dependencies: [
                .target(name: "ASTCEncoderC")
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        ),
    ],
    // The lcms2 library was compiled using c17, so set it also here
    cLanguageStandard: .c17,
    // Also use c++20, we don't live in the stone age, but still not ready to accept c++23
    cxxLanguageStandard: .cxx20
)
