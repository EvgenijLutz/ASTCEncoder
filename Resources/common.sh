# Common helpers

echo "hello handsome"

test() {
	echo "test"
}


# Define some global variables
ft_developer="/Applications/Xcode.app/Contents/Developer"
# Your signing identity to sign the xcframework. Execute "security find-identity -v -p codesigning" and select one from the list
identity=070BA25D98F2A17A61E3E27E31BE64C06F901016

# Android NDK path
ndk_path="/Users/evgenij/Library/Android/sdk/ndk/29.0.14206865"


# Console output formatting
# https://stackoverflow.com/a/2924755
bold=$(tput bold)
normal=$(tput sgr0)


# Checks if the path exists
assert_path() {
  local path=$1
  if [ ! -d "$path" ]; then
    echo "$path does not exist. Check if the path correct and try again."
    exit 1
  fi
}


# Checks if an error happened recently and terminates if it's true
exit_if_error() {
  local result=$?
  if [ $result -ne 0 ] ; then
     echo "Received an exit code $result, aborting"
     exit 1
  fi
}