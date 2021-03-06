# Define target name and output directory
init_target (ECEditorModule OUTPUT plugins)

GetEntityComponents()

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB XML_FILES *.xml)
file (GLOB MOC_FILES ECEditorModule.h ECEditorWindow.h ECAttributeEditor.h ECBrowser.h EcXmlEditorWidget.h
    MultiEditPropertyFactory.h MultiEditPropertyManager.h EntityPlacer.h ECComponentEditor.h LineEditPropertyFactory.h
    AddComponentDialog.h EntityActionDialog.h FunctionDialog.h TreeWidgetItemExpandMemory.h SceneStructureModule.h
    SceneStructureWindow.h SceneTreeWidget.h AddContentWindow.h AssetsWindow.h AssetTreeWidget.h RequestNewAssetDialog.h
    CloneAssetDialog.h EditorButtonFactory.h TransformEditor.h)

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

# Qt4 Moc files to subgroup "CMake Moc"
MocFolder ()

# Qt4 Wrap
QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI(UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

add_definitions (-DECEDITOR_MODULE_EXPORTS)

use_package (QT_PROPERTY_BROWSER)
use_core_modules (Framework Scene Input Console Ui OgreRenderingModule TundraProtocolModule AssetModule)

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS})

link_ogre()
link_package (QT_PROPERTY_BROWSER)
link_modules (Framework Scene Input Console Ui OgreRenderingModule TundraProtocolModule AssetModule)
link_entity_components (EC_TransformGizmo EC_Highlight)

SetupCompileFlagsWithPCH()

final_target ()

# Install windows deps
setup_install_windows_deps ("QtSolutions_PropertyBrowser-2.5.dll" Release|RelWithDebInfo "QtSolutions_PropertyBrowser-2.5d.dll" Debug)