
TOOL_BUILD=release
BUILD=debug
PIC=
TOOL=gcc
ARC=ar
DBG_OPTS=--di\ --opt_level=0
DBG_GCC=

if [ -z "${TOOL_PATH}" ]; then
	ROOT=..
	BUILD_PATH=$ROOT/TypeScriptCompiler/__build
	BIN_PATH=$BUILD_PATH/tsc/linux-ninja-$TOOL-$TOOL_BUILD/bin
else
	BUILD_PATH=$TOOL_PATH
	BIN_PATH=$TOOL_PATH
fi

# if [ -z "${GC_LIB_PATH}" ]; then
# 	export GC_LIB_PATH=$BUILD_PATH/gc/ninja/$BUILD
# fi

# if [ -z "${LLVM_LIB_PATH}" ]; then
# 	export LLVM_LIB_PATH=$BUILD_PATH/llvm/ninja/$BUILD/lib
# fi

# if [ -z "${TSC_LIB_PATH}" ]; then
# 	export TSC_LIB_PATH=$BUILD_PATH/tsc/linux-ninja-$TOOL-$BUILD/lib
# fi
mkdir -p __build/$TOOL_BUILD/defaultlib/tests
$BIN_PATH/tsc --emit=exe --default-lib-path=__build/$TOOL_BUILD tests/$1.ts