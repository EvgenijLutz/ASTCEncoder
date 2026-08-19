# ASTCEncoder
ARM's [astc-encoder](https://github.com/ARM-software/astc-encoder) library prebuilt for all Apple platforms and Android with C++ helper interfaces and Swift support.

Current version: [5.6.0](https://github.com/ARM-software/astc-encoder/releases/tag/5.6.0).

![An example of compressing a 384x384 image (original on the left) using the biggest (12x12) block size with the highest quality - 432KB vs 16KB](Resources/Media/Example-1.png)

## Installing ASTCEncoder

Add the following dependency to your Package.swift:

```Swift
.package(url: "https://github.com/EvgenijLutz/ASTCEncoder.git", from: "5.6.0")
```


If you want to use original astcenc API in C/C++:
```Swift
// Add a dependency to your target
.product(name: "astcenc", package: "ASTCEncoder")
```
And then include the `astcenc.h` header:
```Cpp
#include <astcenc.h>
```


If you want to use original astcenc API with additional helpers in C++:
```Swift
.product(name: "ASTCEncoderC", package: "ASTCEncoder"),
```

And then include the `ASTCEncoderC.hpp` header:
```Cpp
#include <ASTCEncoderC/ASTCEncoderC.hpp>
```


If you want to use Swift API:
```Swift
.product(name: "ASTCEncoder", package: "ASTCEncoder"),
```

And then import the module in your target:
```Swift
import ASTCEncoder
```

Voilà, you're good to go!
