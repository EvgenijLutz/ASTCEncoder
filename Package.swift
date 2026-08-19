// swift-tools-version: 6.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription


let package = Package(
    name: "ASTCEncoder",
    platforms: [
        .macOS(.v10_13),
        .iOS(.v12),
        .tvOS(.v12),
        .watchOS(.v8),
        .visionOS(.v1),
        .custom("Android", versionString: "5.0")
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
        {
#if os(macOS) || os(iOS) || os(tvOS) || os(watchOS) || os(visionOS)
            .binaryTarget(name: "astcenc", path: "Binaries/astcenc.xcframework")
#else
            .binaryTarget(name: "astcenc", path: "Binaries/astcenc.artifactbundle")
#endif
        }(),
        .target(
            name: "ASTCEncoderC",
            dependencies: [
                .target(name: "astcenc")
            ],
            cxxSettings: [
                .enableWarning("all")
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        ),
        .target(
            name: "ASTCEncoder",
            dependencies: [
                .target(name: "astcenc"),
                .target(name: "ASTCEncoderC")
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx),
                .strictMemorySafety()
            ]
        ),
    ],
    // The astcenc library was compiled using c17, so set it also here
    cLanguageStandard: .c17,
    // Also use c++20, we don't live in the stone age, but still not ready to accept c++23
    cxxLanguageStandard: .cxx20
)
