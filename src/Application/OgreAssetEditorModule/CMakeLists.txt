# Define target name and output directory
init_target (OgreAssetEditorModule OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB XML_FILES *.xml)
file (GLOB RESOURCE_FILES resource/*.qrc)
file (GLOB MOC_FILES OgreAssetEditorModule.h EditorManager.h OgreScriptEditor.h PropertyTableWidget.h
                     OgreMaterialProperties.h MaterialWizard.h TexturePreviewEditor.h AudioPreviewEditor.h AudioSignalLabel.h
                     MeshPreviewEditor.h OgreScriptHighlighter.h)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

add_definitions (-DASSET_EDITOR_MODULE_EXPORTS)

# Qt4 Moc files to subgroup "CMake Moc"
MocFolder ()

# Qt4 Wrap
QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI(UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

use_core_modules (Framework AssetModule OgreRenderingModule Input Audio)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS} ${RESOURCE_SRCS})

link_modules (Framework OgreRenderingModule Input Audio)

link_ogre()

SetupCompileFlagsWithPCH()


final_target ()
