# Define target name and output directory
init_target(Asset)

MocFolder ()

# Use this if building Asset as a SHARED lib.
#init_target(Asset OUTPUT ./)

# Define source files
file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
file(GLOB MOC_FILES AssetAPI.h IAsset.h IAssetTransfer.h IAssetUploadTransfer.h IAssetStorage.h AssetRefListener.h BinaryAsset.h AssetCache.h)

set(SOURCE_FILES ${CPP_FILES} ${H_FILES})

QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})

use_core_modules(Framework Ui)

build_library(${TARGET_NAME} STATIC ${SOURCE_FILES} ${MOC_SRCS})

add_definitions(-DASSET_API_EXPORTS)

link_package(BOOST)
link_package(QT4)

link_modules(Framework)

SetupCompileFlags()

final_target()
