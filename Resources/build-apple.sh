# bash build-apple.sh

# astc-encoder

source common.sh


# Get some help
#./configure -help >> configure-help.txt


# Output library name. Determined by the build system. Try to change the name if possible in the future
libname=astcenc
# Source code folder name
source_name="astc-encoder-5.6.0"


last_directory=$(pwd)


# Remove logs if exist
# rm -f "build/log.txt"


build_library() {
  local platform=$1
  local arch=$2
  local min_os=$3

  # Reset variables
  export LT_SYS_LIBRARY_PATH=""
  export AR=""
  export CC=""
  export AS=""
  export CXX=""
  export LD=""
  export RANLIB=""
  export STRIP=""
  export CPPFLAGS=""
  export CFLAGS=""

  # Determine host based on platform and architecture
  # Apple
  if [[ "$platform" == "MacOSX" ]] || \
    [[ "$platform" == "iPhoneOS" ]] || [[ "$platform" == "iPhoneSimulator" ]] || \
    [[ "$platform" == "AppleTVOS" ]] || [[ "$platform" == "AppleTVSimulator" ]] || \
    [[ "$platform" == "WatchOS" ]] || [[ "$platform" == "WatchSimulator" ]] || \
    [[ "$platform" == "XROS" ]] || [[ "$platform" == "XRSimulator" ]]; then
    local os_family="Apple"

    if   [[ "$arch" == "arm64" ]];  then local host="arm-apple-darwin"
    elif [[ "$arch" == "x86_64" ]]; then local host="x86_64-apple-darwin"
    fi

    local sysroot="$ft_developer/Platforms/$platform.platform/Developer/SDKs/$platform.sdk"
    local arch_flags="-arch $arch"
    local target_os_flags="-mtargetos=$min_os"
    export CC="$ft_developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
    export CXX="$ft_developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
    #export LD="$ft_developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld"
    export LT_SYS_LIBRARY_PATH="-isysroot $sysroot/usr/include"
    export CPPFLAGS="-I$sysroot/usr/include"
    export CFLAGS="-isysroot $sysroot $arch_flags -std=c17 $target_os_flags -O2"
    export CXXFLAGS="-isysroot $sysroot $arch_flags -std=c++20 $target_os_flags -O2"

  # Android
  elif [[ "$platform" == "Android" ]]; then
    local os_family="Android"

    if   [[ "$arch" == "aarch64" ]];  then local host="aarch64-linux-android"
    elif [[ "$arch" == "arm" ]];      then local host="arm-linux-androideabi"
    elif [[ "$arch" == "i686" ]];     then local host="i686-linux-android"
    elif [[ "$arch" == "riscv64" ]];  then local host="riscv64-linux-android"
    elif [[ "$arch" == "x86_64" ]];   then local host="x86_64-linux-android"
    fi

    local sysroot="$ndk_path/toolchains/llvm/prebuilt/darwin-x86_64/sysroot"
    local arch_flags=""
    local target_os_flags="--target=$host$min_os"
    export LT_SYS_LIBRARY_PATH=""

    local toolchain="$ndk_path/toolchains/llvm/prebuilt/darwin-x86_64"
    export AR=$toolchain/bin/llvm-ar
    export CC="$toolchain/bin/clang"
    export AS=$CC
    export CXX="$toolchain/bin/clang++"
    export LD=$toolchain/bin/ld
    export RANLIB=$toolchain/bin/llvm-ranlib
    export STRIP=$toolchain/bin/llvm-strip

    export CPPFLAGS=""
    export CFLAGS="-std=c17 $target_os_flags -O2"
    export CXXFLAGS="-std=c++20 $target_os_flags -O2"

  else
    echo "Unknown platform $platform"
    exit 1
  fi

  # Determine ASTC compression feature flags (probably to improve performance) or noting if not supported
  if [[ "$os_family" == "Apple" ]]; then
    local make_program=""
    local android_settings=""

    if [[ "$arch" == "arm64" ]];  then
      local astc_features="-DASTCENC_ISA_NEON=ON"
      local output_name="libastcenc-neon-static"

    elif [[ "$arch" == "x86_64" ]]; then
      local astc_features="-DASTCENC_ISA_AVX2=ON -DASTCENC_X86_GATHERS=OFF"
      local output_name="libastcenc-avx2-static"
    fi

  elif [[ "$platform" == "Android" ]]; then
    local make_program="-DCMAKE_TOOLCHAIN_FILE=${ndk_path}/build/cmake/android.toolchain.cmake"
    local android_settings="-DANDROID_PLATFORM=android-$min_os -DANDROID_TOOLCHAIN=clang -DANDROID_STL=c++_static"
    #local android_settings="-DANDROID_PLATFORM=android-$min_os -DANDROID_TOOLCHAIN=clang -DANDROID_STL=c++_static -DARCH=aarch64"
    #BUILD_TYPE=RelWithDebInfo

    # Android ABIs
    # https://developer.android.com/ndk/guides/abis
    if [[ "$arch" == "aarch64" ]];  then
      local android_settings="-DANDROID_ABI=arm64-v8a"
      local astc_features="-DASTCENC_ISA_NEON=ON"
      local output_name="libastcenc-neon-static"
      #local astc_features="-DASTCENC_ISA_NONE=ON"
      #local output_name="libastcenc-none-static"

    elif [[ "$arch" == "arm" ]]; then
      local android_settings="-DANDROID_ABI=armeabi-v7a"
      # Doesn't compile for some reason, although NEON should be supported on this platform
      #local astc_features="-DASTCENC_ISA_NEON=ON"
      #local output_name="libastcenc-neon-static"

      local astc_features="-DASTCENC_ISA_NONE=ON"
      local output_name="libastcenc-none-static"

    elif [[ "$arch" == "i686" ]]; then
      local android_settings="-DANDROID_ABI=x86"

      # Fully supported in Android x86
      local astc_features="-DASTCENC_ISA_SSE2=ON"
      local output_name="libastcenc-sse2-static"

      #local astc_features="-DASTCENC_ISA_NONE=ON"
      #local output_name="libastcenc-none-static"

    elif [[ "$arch" == "riscv64" ]]; then
      # TODO: Check this platform if the name is correct
      local android_settings="-DANDROID_ABI=riscv64"
      local astc_features="-DASTCENC_ISA_NONE=ON"
      local output_name="libastcenc-none-static"

    elif [[ "$arch" == "x86_64" ]]; then
      local android_settings="-DANDROID_ABI=x86_64"

      # Generally not available in the standard Android ecosystem
      #local astc_features="-DASTCENC_ISA_AVX2=ON -DASTCENC_X86_GATHERS=OFF"
      #local output_name="libastcenc-avx2-static"

      # Fully supported in Android x86_64, but SSE4.1 is cooler
      #local astc_features="-DASTCENC_ISA_SSE2=ON -DASTCENC_X86_GATHERS=OFF"
      #local output_name="libastcenc-sse2-static"
      
      # Supported almost everywhere and often is required
      local astc_features="-DASTCENC_ISA_SSE41=ON -DASTCENC_X86_GATHERS=OFF"
      local output_name="libastcenc-sse4.1-static"

      #local astc_features="-DASTCENC_ISA_NONE=ON"
      #local output_name="libastcenc-none-static"
      
      # This also works, produces multiple binaries with different backends
      #local astc_features="-DASTCENC_ISA_NONE=ON -DASTCENC_ISA_SSE2=ON -DASTCENC_ISA_SSE41=ON -DASTCENC_X86_GATHERS=OFF"
      #local output_name="libastcenc-sse4.1-static"

    fi
  fi

  # Are we sure that we need to specify the architecture for the linker?
  export LDFLAGS="$arch_flags"

  # Welcome message
  echo "Build for ${bold}$platform $host${normal}"

  # Clean
  #make clean

  # Remove previously build foler for the specified platform and architecture if exists
  rm -rf "build/$platform/$arch"
  mkdir -p "build/$platform/$arch/tmp"

  # Configure for the specified platform and architecture
  cd build/$platform/$arch/tmp
  cmake ../../../../$source_name \
    -DASTCENC_CLI=OFF \
    -DASTCENC_SHAREDLIB=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DASTCENC_UNIVERSAL_BUILD=OFF \
    $astc_features \
    $make_program \
    $android_settings \
    -DCMAKE_INSTALL_PREFIX=../install
  exit_if_error

  # Build
  make -j$(sysctl -n hw.ncpu)
  exit_if_error

  # Go back
  cd ../../../..

  # Copy library
  mkdir -p "build/$platform/$arch/lib"
  cp build/$platform/$arch/tmp/Source/$output_name.a "build/$platform/$arch/lib/$libname.a"
  exit_if_error

  # Copy header
  mkdir -p "build/$platform/$arch/include/$libname"
  cp $source_name/Source/astcenc.h "build/$platform/$arch/include/$libname/astcenc.h"
  exit_if_error

  # Copy modulemap
  # About modules
  # https://clang.llvm.org/docs/Modules.html
  # Without module.modulemap astcenc is not exposed to Swift
  # Copy the module map into the directory with installed header files
  mkdir -p build/$platform/$arch/include/$libname/astcenc-Module
  cp Contents/module.modulemap build/$platform/$arch/include/$libname/astcenc-Module/module.modulemap
  exit_if_error

  # Remove temporary build data
  rm -rf "build/$platform/$arch/tmp"

}

# Build for Apple systems
build_library MacOSX           arm64  macos11
build_library MacOSX           x86_64 macos10.13
build_library iPhoneOS         arm64  ios12
build_library iPhoneSimulator  arm64  ios14-simulator
build_library iPhoneSimulator  x86_64 ios12-simulator
build_library AppleTVOS        arm64  tvos12
build_library AppleTVSimulator arm64  tvos12-simulator
build_library AppleTVSimulator x86_64 tvos12-simulator
build_library WatchOS          arm64  watchos8
build_library WatchSimulator   arm64  watchos8-simulator
build_library WatchSimulator   x86_64 watchos8-simulator
build_library XROS             arm64  xros1
build_library XRSimulator      arm64  xros1-simulator
build_library XRSimulator      x86_64 xros1-simulator

# Build for Android
build_library Android aarch64 21
build_library Android arm     21
build_library Android i686    21
build_library Android riscv64 35
build_library Android x86_64  21


create_framework() {
  # Remove previously created framework if exists
  rm -rf build/$libname.xcframework
  exit_if_error

  # Merge macOS arm and x86 binaries
  mkdir -p build/MacOSX
  exit_if_error
  lipo -create -output build/MacOSX/$libname.a \
    build/MacOSX/arm64/lib/$libname.a \
    build/MacOSX/x86_64/lib/$libname.a
  exit_if_error

  # Merge iOS simulator arm and x86 binaries
  mkdir -p build/iPhoneSimulator
  exit_if_error
  lipo -create -output build/iPhoneSimulator/$libname.a \
    build/iPhoneSimulator/arm64/lib/$libname.a \
    build/iPhoneSimulator/x86_64/lib/$libname.a
  exit_if_error

  # Merge tvOS simulator arm and x86 binaries
  mkdir -p build/AppleTVSimulator
  exit_if_error
  lipo -create -output build/AppleTVSimulator/$libname.a \
    build/AppleTVSimulator/arm64/lib/$libname.a \
    build/AppleTVSimulator/x86_64/lib/$libname.a
  exit_if_error

  # Merge watchOS simulator arm and x86 binaries
  mkdir -p build/WatchSimulator
  exit_if_error
  lipo -create -output build/WatchSimulator/$libname.a \
    build/WatchSimulator/arm64/lib/$libname.a \
    build/WatchSimulator/x86_64/lib/$libname.a
  exit_if_error

  # Merge visionOS simulator arm and x86 binaries
  mkdir -p build/XRSimulator
  exit_if_error
  lipo -create -output build/XRSimulator/$libname.a \
    build/XRSimulator/arm64/lib/$libname.a \
    build/XRSimulator/x86_64/lib/$libname.a
  exit_if_error

  # Create the framework with multiple platforms
  xcodebuild -create-xcframework \
    -library build/MacOSX/$libname.a              -headers build/MacOSX/arm64/include/$libname \
    -library build/iPhoneOS/arm64/lib/$libname.a  -headers build/iPhoneOS/arm64/include/$libname \
    -library build/iPhoneSimulator/$libname.a     -headers build/iPhoneSimulator/arm64/include/$libname \
    -library build/AppleTVOS/arm64/lib/$libname.a -headers build/AppleTVOS/arm64/include/$libname \
    -library build/AppleTVSimulator/$libname.a    -headers build/AppleTVSimulator/arm64/include/$libname \
    -library build/WatchOS/arm64/lib/$libname.a   -headers build/WatchOS/arm64/include/$libname \
    -library build/WatchSimulator/$libname.a      -headers build/WatchSimulator/arm64/include/$libname \
    -library build/XROS/arm64/lib/$libname.a      -headers build/XROS/arm64/include/$libname \
    -library build/XRSimulator/$libname.a         -headers build/XRSimulator/arm64/include/$libname \
    -output build/$libname.xcframework
  exit_if_error

  # And sign the framework
  codesign --timestamp -s $identity build/$libname.xcframework
  exit_if_error
}
create_framework


create_artifactbundle() {
  # Remove previously created artifact if exists
  rm -rf build/$libname.artifactbundle
  exit_if_error

  # Create the artifact bundle folder
  mkdir -p build/$libname.artifactbundle
  exit_if_error

  # info.json
  cp Contents/info.json build/$libname.artifactbundle/info.json
  exit_if_error

  # Headers
  cp -r build/Android/aarch64/include build/$libname.artifactbundle/include
  exit_if_error

  # aarch64-linux-android
  mkdir -p build/$libname.artifactbundle/aarch64-linux-android
  exit_if_error
  cp build/Android/aarch64/lib/$libname.a build/$libname.artifactbundle/aarch64-linux-android/$libname.a
  exit_if_error

  # arm-linux-androideabi
  mkdir -p build/$libname.artifactbundle/arm-linux-androideabi
  exit_if_error
  cp build/Android/arm/lib/$libname.a build/$libname.artifactbundle/arm-linux-androideabi/$libname.a
  exit_if_error

  # i686-linux-android
  mkdir -p build/$libname.artifactbundle/i686-linux-android
  exit_if_error
  cp build/Android/i686/lib/$libname.a build/$libname.artifactbundle/i686-linux-android/$libname.a
  exit_if_error

  # riscv64-linux-android
  mkdir -p build/$libname.artifactbundle/riscv64-linux-android
  exit_if_error
  cp build/Android/riscv64/lib/$libname.a build/$libname.artifactbundle/riscv64-linux-android/$libname.a
  exit_if_error

  # x86_64-linux-android
  mkdir -p build/$libname.artifactbundle/x86_64-linux-android
  exit_if_error
  cp build/Android/x86_64/lib/$libname.a build/$libname.artifactbundle/x86_64-linux-android/$libname.a
  exit_if_error
}
create_artifactbundle





# Done!