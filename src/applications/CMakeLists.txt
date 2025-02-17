PROJECT(OSGEARTH_APPLICATIONS)

SET(OSGCORE_BUNDLED TRUE)

IF(NOT OSGCORE_BUNDLED)
	FIND_PACKAGE(OSGCORE)
	IF(OSGCORE_FOUND)
		INCLUDE(${OSGCORE_USE_FILE})
	ELSE(OSGCORE_FOUND)
		MESSAGE(ERROR "OSGCORE needed but NOT FOUND")
	ENDIF(OSGCORE_FOUND)
	SET(CMAKE_MODULE_PATH  ${PROJECT_SOURCE_DIR}/../../Macros)
ENDIF(NOT OSGCORE_BUNDLED)

SET(OPENSCENEGRAPH_APPLICATION_DIR ${PROJECT_SOURCE_DIR})


#OpenThreads, osg, osgDB and osgUtil are included elsewhere.
SET(TARGET_COMMON_LIBRARIES
    osgEarth
)

SET(TARGET_DEFAULT_PREFIX "application_")

IF(NOT OSGEARTH_BUILD_PLATFORM_IPHONE)

    # Tools:
    IF(OSGEARTH_BUILD_TOOLS)
        SET(TARGET_DEFAULT_LABEL_PREFIX "Tool")
        SET(TARGET_DEFAULT_APPLICATION_FOLDER "Tools")
        ADD_SUBDIRECTORY(osgearth_viewer)
        ADD_SUBDIRECTORY(osgearth_imgui)
        ADD_SUBDIRECTORY(osgearth_tfs)
        ADD_SUBDIRECTORY(osgearth_boundarygen)
        ADD_SUBDIRECTORY(osgearth_version)
        ADD_SUBDIRECTORY(osgearth_atlas)
        ADD_SUBDIRECTORY(osgearth_bakefeaturetiles)
        ADD_SUBDIRECTORY(osgearth_conv)
        ADD_SUBDIRECTORY(osgearth_3pv)
        ADD_SUBDIRECTORY(osgearth_clamp)
        ADD_SUBDIRECTORY(osgearth_createtile)
        
        if(OSGEARTH_BUILD_PROCEDURAL_NODEKIT)
            add_subdirectory(osgearth_exportvegetation)
            add_subdirectory(osgearth_biome)
            add_subdirectory(osgearth_imposterbaker)
        endif()
        
        if(OSGEARTH_BUILD_LEGACY_SPLAT_NODEKIT)
            ADD_SUBDIRECTORY(osgearth_exportgroundcover_splat)
        endif()

        IF (Protobuf_FOUND AND SQLITE3_FOUND)
            ADD_SUBDIRECTORY(osgearth_mvtindex)
        ENDIF()        
    ENDIF()

    # Examples:
    IF(OSGEARTH_BUILD_EXAMPLES)
        SET(TARGET_DEFAULT_LABEL_PREFIX "Example")
        SET(TARGET_DEFAULT_APPLICATION_FOLDER "Examples")
        ADD_SUBDIRECTORY(osgearth_simple)
        ADD_SUBDIRECTORY(osgearth_manip)
        ADD_SUBDIRECTORY(osgearth_cluster)
        ADD_SUBDIRECTORY(osgearth_features)
        ADD_SUBDIRECTORY(osgearth_featurefilter)
        ADD_SUBDIRECTORY(osgearth_los)
        ADD_SUBDIRECTORY(osgearth_terrainprofile)
        ADD_SUBDIRECTORY(osgearth_map)
        ADD_SUBDIRECTORY(osgearth_annotation)
        ADD_SUBDIRECTORY(osgearth_tracks)
        ADD_SUBDIRECTORY(osgearth_transform)
        ADD_SUBDIRECTORY(osgearth_graticule)
        ADD_SUBDIRECTORY(osgearth_occlusionculling)
        ADD_SUBDIRECTORY(osgearth_minimap)
        ADD_SUBDIRECTORY(osgearth_mrt)
        ADD_SUBDIRECTORY(osgearth_pick)
        ADD_SUBDIRECTORY(osgearth_ephemeris)
        ADD_SUBDIRECTORY(osgearth_skyview)
        ADD_SUBDIRECTORY(osgearth_lights)
        ADD_SUBDIRECTORY(osgearth_infinitescroll)
        ADD_SUBDIRECTORY(osgearth_video)
        ADD_SUBDIRECTORY(osgearth_magnify)
        ADD_SUBDIRECTORY(osgearth_eci)
        ADD_SUBDIRECTORY(osgearth_heatmap)
        ADD_SUBDIRECTORY(osgearth_collecttriangles)

    ENDIF(OSGEARTH_BUILD_EXAMPLES)
    
    IF(OSGEARTH_BUILD_TESTS)
        SET(TARGET_DEFAULT_LABEL_PREFIX "Test")
        SET(TARGET_DEFAULT_APPLICATION_FOLDER "Tests")
        add_subdirectory(osgearth_bindless)
        ADD_SUBDIRECTORY(osgearth_drawables)
        ADD_SUBDIRECTORY(osgearth_horizon)
        ADD_SUBDIRECTORY(osgearth_overlayviewer)
        ADD_SUBDIRECTORY(osgearth_shadercomp)
        ADD_SUBDIRECTORY(osgearth_windows)

        IF(SILVERLINING_FOUND)
            ADD_SUBDIRECTORY(osgearth_silverlining)
        ENDIF(SILVERLINING_FOUND)

        IF(TRITON_FOUND)
            ADD_SUBDIRECTORY(osgearth_triton)
        ENDIF(TRITON_FOUND)
    ENDIF(OSGEARTH_BUILD_TESTS)

ELSE()

    # ios
    IF(OSGEARTH_BUILD_EXAMPLES)
        SET(TARGET_DEFAULT_LABEL_PREFIX "Example")
        SET(TARGET_DEFAULT_APPLICATION_FOLDER "Examples")
        ADD_SUBDIRECTORY(osgearth_viewerIOS)
    ENDIF()

ENDIF()
