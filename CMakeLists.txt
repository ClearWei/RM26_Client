cmake_minimum_required(VERSION 3.16)

# 某些 IDE 自带的 CMake 仍是 x86_64；在 Apple Silicon 上按硬件架构构建，
# 同时保留通过 -DCMAKE_OSX_ARCHITECTURES 显式覆盖的能力。
if(CMAKE_HOST_APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    execute_process(
        COMMAND /usr/sbin/sysctl -n hw.optional.arm64
        OUTPUT_VARIABLE _rm26_macos_has_arm64
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_rm26_macos_has_arm64 STREQUAL "1")
        set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "macOS target architectures" FORCE)
    endif()
endif()

project(RM26CustomClient VERSION 1.0.0 LANGUAGES CXX C) # Protobuf 依赖部分 C 源码，这里显式启用 C

# 导出编译数据库，方便编辑器和静态分析工具解析 Qt 等依赖的头文件路径。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(RM26_ENABLE_DEVTOOLS "Enable optional RM26 development hooks" OFF)

# 加载本机可选路径；该文件不进入版本控制。
include(CMakeUserPaths.cmake OPTIONAL)

# 客户端必需的 Qt 组件
set(_qt_required_components Core Concurrent Widgets Gui Multimedia Network SerialPort Svg Quick QuickWidgets QuickControls2)
find_package(Qt6 REQUIRED COMPONENTS ${_qt_required_components})

set(RM_HAS_QT_HTTP_SERVER OFF)
if(RM26_ENABLE_DEVTOOLS)
    # HttpServer 只服务于开发观测接口，正式客户端不应自动引入这个依赖。
    find_package(Qt6 REQUIRED COMPONENTS HttpServer)
    set(RM_HAS_QT_HTTP_SERVER ON)
    message(STATUS "Qt6 HttpServer (QHttpServer) found; HTTP dev hooks available")
endif()

# ccache 编译缓存
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found. Install it for faster builds (brew install ccache)")
endif()

# 当前优先使用 ccache，未启用可能导致缓存失效的预编译头。

#
# Protobuf（跨平台）
# - macOS：常见是 Homebrew 安装，protobuf 的 CMake 包位于 /opt/homebrew/opt/protobuf
# - Windows：不要写死 macOS 路径；优先找系统/包管理器提供的 Protobuf，找不到则在配置期自动拉取并构建
#
if(APPLE)
    # Homebrew 的常见安装前缀，作为 find_package 的提示路径
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/protobuf")
endif()
find_package(Protobuf REQUIRED)
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
find_package(absl CONFIG REQUIRED)

# Config 模式不一定设置 protoc 路径，这里从导入目标补齐。
if(NOT Protobuf_PROTOC_EXECUTABLE)
    if(TARGET protobuf::protoc)
        get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION_RELEASE)
        if(NOT Protobuf_PROTOC_EXECUTABLE)
             get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION_DEBUG)
        endif()
        if(NOT Protobuf_PROTOC_EXECUTABLE)
             get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
        endif()
    endif()
endif()

# 导入目标未提供路径时，从 VCPKG_ROOT 或系统 PATH 查找 protoc。
if(NOT Protobuf_PROTOC_EXECUTABLE)
    if(DEFINED ENV{VCPKG_ROOT})
        file(GLOB _PROTOC_CANDIDATES
            "${ENV{VCPKG_ROOT}}/installed/*/tools/protobuf/protoc"
            "${ENV{VCPKG_ROOT}}/installed/*/tools/protobuf/protoc.exe"
        )
        list(LENGTH _PROTOC_CANDIDATES _PROTOC_CANDIDATES_COUNT)
        if(_PROTOC_CANDIDATES_COUNT GREATER 0)
            list(GET _PROTOC_CANDIDATES 0 _PROTOC_FIRST)
            set(Protobuf_PROTOC_EXECUTABLE "${_PROTOC_FIRST}")
        endif()
    endif()

    if(NOT Protobuf_PROTOC_EXECUTABLE)
        find_program(Protobuf_PROTOC_EXECUTABLE protoc
            HINTS
                "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/protobuf"
                "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/protobuf/bin"
        )
    endif()
endif()

message(STATUS "Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")

find_package(PkgConfig QUIET)

# FFmpeg 用于 H.264/HEVC 解码
option(RM_WITH_FFMPEG "Enable FFmpeg-based HEVC decoding" ON)
set(RM_HAS_FFMPEG OFF)
if(RM_WITH_FFMPEG)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG QUIET libavcodec libavformat libavutil libswscale)
        if(FFMPEG_FOUND)
            set(RM_HAS_FFMPEG ON)
            message(STATUS "FFmpeg enabled")
            message(STATUS "FFmpeg include dirs: ${FFMPEG_INCLUDE_DIRS}")
            message(STATUS "FFmpeg library dirs: ${FFMPEG_LIBRARY_DIRS}")
            message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
        else()
            message(WARNING "FFmpeg not found via pkg-config, trying manual library detection...")
        endif()
    else()
        message(WARNING "PkgConfig not found, trying manual library detection...")
    endif()

    # Windows、MinGW 或缺少 pkg-config 时，按 vcpkg 常见布局直接查找头文件和库。
    if(NOT RM_HAS_FFMPEG)
        find_path(FFMPEG_INCLUDE_DIR
            NAMES libavcodec/avcodec.h
        )
        find_library(FFMPEG_AVCODEC_LIB
            NAMES avcodec libavcodec
        )
        find_library(FFMPEG_AVFORMAT_LIB
            NAMES avformat libavformat
        )
        find_library(FFMPEG_AVUTIL_LIB
            NAMES avutil libavutil
        )
        find_library(FFMPEG_SWSCALE_LIB
            NAMES swscale libswscale
        )

        if(FFMPEG_INCLUDE_DIR
           AND FFMPEG_AVCODEC_LIB
           AND FFMPEG_AVFORMAT_LIB
           AND FFMPEG_AVUTIL_LIB
           AND FFMPEG_SWSCALE_LIB)
            set(RM_HAS_FFMPEG ON)
            set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
            set(FFMPEG_LIBRARIES
                ${FFMPEG_AVCODEC_LIB}
                ${FFMPEG_AVFORMAT_LIB}
                ${FFMPEG_AVUTIL_LIB}
                ${FFMPEG_SWSCALE_LIB}
            )
            get_filename_component(_ffmpeg_lib_dir "${FFMPEG_AVCODEC_LIB}" DIRECTORY)
            set(FFMPEG_LIBRARY_DIRS "${_ffmpeg_lib_dir}")
            message(STATUS "FFmpeg enabled (manual detection)")
            message(STATUS "FFmpeg include dirs: ${FFMPEG_INCLUDE_DIRS}")
            message(STATUS "FFmpeg library dirs: ${FFMPEG_LIBRARY_DIRS}")
            message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
        else()
            message(WARNING "FFmpeg not found, HEVC decoding disabled")
        endif()
    endif()
endif()

message(STATUS "FFmpeg include dirs: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS "FFmpeg library dirs: ${FFMPEG_LIBRARY_DIRS}")
message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")

# 补充 FFmpeg 链接目录
if(FFMPEG_LIBRARY_DIRS)
    link_directories(${FFMPEG_LIBRARY_DIRS})
endif()

# Paho MQTT C 用于赛事 MQTT 通信
if(APPLE)
    set(PAHO_MQTT_ROOT "/opt/homebrew/opt/libpaho-mqtt")
endif()

if(DEFINED PAHO_MQTT_ROOT AND EXISTS "${PAHO_MQTT_ROOT}")
    find_path(PAHO_MQTT_INCLUDE_DIRS
        NAMES MQTTClient.h
        PATHS "${PAHO_MQTT_ROOT}/include"
        NO_DEFAULT_PATH
    )
    find_library(PAHO_MQTT_LIBRARIES
        NAMES paho-mqtt3c libpaho-mqtt3c
        PATHS "${PAHO_MQTT_ROOT}/lib" "${PAHO_MQTT_ROOT}/lib64"
        NO_DEFAULT_PATH
    )
    if(PAHO_MQTT_INCLUDE_DIRS AND PAHO_MQTT_LIBRARIES)
        message(STATUS "Paho MQTT found: ${PAHO_MQTT_ROOT}")
        set(PAHO_MQTT_FOUND TRUE)
    else()
        message(WARNING "PAHO_MQTT_ROOT is set but Paho MQTT headers or library were not found")
    endif()
else()
    # 先尝试 pkg-config；没有 pkg-config 时继续走 CMake package 查找。
    if(PkgConfig_FOUND)
        pkg_check_modules(PAHO_MQTT QUIET paho-mqtt3c)
    endif()
    if(PAHO_MQTT_FOUND)
        message(STATUS "Paho MQTT found via pkg-config")
    else()
        # 继续尝试 vcpkg 或 Config 包导出的目标名。
        find_package(eclipse-paho-mqtt-c CONFIG QUIET)
        if(eclipse-paho-mqtt-c_FOUND)
            set(PAHO_MQTT_FOUND TRUE)
            # 导入目标会携带头文件路径，不需要再次手工设置。
            set(PAHO_MQTT_LIBRARIES eclipse-paho-mqtt-c::paho-mqtt3c)
            message(STATUS "Paho MQTT found via CMake config (eclipse-paho-mqtt-c)")
        else()
            message(WARNING "Paho MQTT not found. MQTT communication will be disabled.")
        endif()
    endif()
endif()

# ONNX Runtime 用于可选目标检测
if(APPLE)
    set(ONNXRUNTIME_ROOT "/opt/homebrew/opt/onnxruntime")
elseif(WIN32)
    # Windows 通过标准搜索路径或用户提供的 ONNXRUNTIME_ROOT 定位运行库。
    if(NOT DEFINED ONNXRUNTIME_ROOT)
        message(STATUS "Windows: Set ONNXRUNTIME_ROOT if AR features are needed")
    endif()
endif()

if(DEFINED ONNXRUNTIME_ROOT AND EXISTS "${ONNXRUNTIME_ROOT}")
    set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_ROOT}/include")
    # Windows 构建阶段链接 .lib，运行阶段再部署对应 DLL。
    if(WIN32)
        set(ONNXRUNTIME_LIBRARIES "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
    else()
        set(ONNXRUNTIME_LIBRARIES "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.dylib")
    endif()

    message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_ROOT}")
    message(STATUS "ONNX Runtime include: ${ONNXRUNTIME_INCLUDE_DIRS}")
    set(ONNXRUNTIME_FOUND TRUE)
else()
    message(WARNING "ONNX Runtime not found. AR overlay will be disabled.")
    set(ONNXRUNTIME_FOUND FALSE)
endif()

# Protobuf 代码生成
file(GLOB PROTO_FILES "src/network/proto/*.proto")
# 手工声明生成步骤，规避 Visual Studio 下 protobuf::protoc 触发的 MSB8066。
set(PROTO_SRCS)
set(PROTO_HDRS)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/network/proto)
# 清理历史生成位置，避免 include 命中旧的 build/robomaster.pb.h
file(REMOVE
    ${CMAKE_CURRENT_BINARY_DIR}/robomaster.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/robomaster.pb.cc
)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(FIL_WE ${PROTO_FILE} NAME_WE)

    set(PROTO_HDR ${CMAKE_CURRENT_BINARY_DIR}/src/network/proto/${FIL_WE}.pb.h)
    set(PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/src/network/proto/${FIL_WE}.pb.cc)

    list(APPEND PROTO_SRCS ${PROTO_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR})

    add_custom_command(
        OUTPUT ${PROTO_HDR} ${PROTO_SRC}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/src/network/proto
             -I${CMAKE_CURRENT_SOURCE_DIR}/src/network/proto
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating ${FIL_WE}.pb.cc/h"
        VERBATIM
    )
endforeach()

# 源文件
set(SOURCES
    resources.qrc
    src/qml/qml.qrc
    src/ui/ExchangeCommandPolicy.cpp
    src/ui/InputHotkeyPolicy.cpp
    src/ui/PopupOverlayPolicy.cpp
    src/ui/MainWindow.cpp
    src/widgets/HealthBarWidget.cpp
    src/widgets/RobotStatusPanel.cpp
    src/widgets/CrosshairWidget.cpp
    src/widgets/HeatRingWidget.cpp
    src/widgets/RobotStatusWidget.cpp
    src/widgets/VideoBackgroundWidget.cpp
    src/widgets/HeroVideoWidget.cpp
    src/widgets/BattleMessageWidget.cpp
    src/widgets/GameResultWidget.cpp
    src/widgets/MiniMapWidget.cpp
    src/widgets/HelpOverlayWidget.cpp
    src/widgets/InfoPanel.cpp
    src/widgets/OperationsPanel.cpp
    src/simulator/ProtocolSimulator.cpp
    src/network/Protocol.cpp
    src/network/NetworkManager.cpp
    src/network/VideoReceiver.cpp
    src/network/VideoDatagramLogger.cpp
    src/network/VideoReceiver.h
    src/network/VideoDatagramLogger.h
    src/network/HevcDecoder.cpp
    src/network/HevcDecoder.h
    src/network/H264Decoder.cpp
    src/network/H264Decoder.h
    src/network/H264AnnexB.cpp
    src/network/H264AnnexB.h
    src/network/H264StreamRecoveryPolicy.cpp
    src/network/H264StreamRecoveryPolicy.h
    src/network/CRC.cpp
    src/network/MqttManager.cpp
    src/network/MqttLogger.cpp
    src/network/S1ProtocolManager.cpp
    ${PROTO_SRCS}
    src/core/GameData.cpp
    src/core/LogFileNaming.cpp
    src/core/TimedEventRules.cpp
    src/core/PopupStateMachine.cpp
    src/config/ConfigManager.cpp
    src/config/ConfigLoader.cpp
    src/config/ConfigValidator.cpp

    # 战术分析管线
    src/core/DataFreshnessGuard.cpp
    src/core/MapCoordinateMapper.cpp
    src/core/ThreatRanker.cpp
    src/core/TacticalAnalyzer.cpp
    src/core/ExecutionFusion.cpp
)

# 头文件
set(HEADERS
    src/ui/LayoutConstants.h
    src/ui/ExchangeCommandPolicy.h
    src/ui/InputHotkeyPolicy.h
    src/ui/MainWindowStatePolicy.h
    src/ui/PopupOverlayPolicy.h
    src/ui/MainWindow.h
    src/widgets/HealthBarWidget.h
    src/widgets/RobotStatusPanel.h
    src/widgets/CrosshairWidget.h
    src/widgets/HeatRingWidget.h
    src/widgets/RobotStatusWidget.h
    src/widgets/VideoBackgroundWidget.h
    src/widgets/HeroVideoWidget.h
    src/widgets/BattleMessageWidget.h
    src/widgets/GameResultWidget.h
    src/widgets/MiniMapWidget.h
    src/widgets/HelpOverlayWidget.h
    src/widgets/InfoPanel.h
    src/widgets/OperationsPanel.h
    src/widgets/DebugLogWidget.h
    src/simulator/ProtocolSimulator.h
    src/network/Protocol.h
    src/network/NetworkManager.h
    src/network/CRC.h
    src/network/MqttManager.h
    src/network/MqttLogger.h
    src/network/H264AnnexB.h
    src/network/H264StreamRecoveryPolicy.h
    src/network/S1ProtocolManager.h
    src/core/GameData.h
    src/core/LogFileNaming.h
    src/core/TimedEventRules.h
    src/core/PopupTypes.h
    src/core/PopupStateMachine.h
    src/config/ConfigManager.h
    src/config/ConfigLoader.h
    src/config/ConfigValidator.h
    ${PROTO_HDRS}

    # 战术分析管线头文件
    src/core/TacticalConfig.h
    src/core/TacticalSnapshot.h
    src/core/DataFreshnessGuard.h
    src/core/MapCoordinateMapper.h
    src/core/ThreatRanker.h
    src/core/TacticalAnalyzer.h
    src/core/ExecutionFusion.h
)

# 编译业务源码前先生成 Protobuf 头文件。
set_source_files_properties(${SOURCES} PROPERTIES OBJECT_DEPENDS "${PROTO_HDRS}")

# AR 叠加系统源文件 (可选，依赖 ONNX Runtime)
if(ONNXRUNTIME_FOUND)
    set(AR_SOURCES
        src/core/YoloDetector.cpp
        src/core/TargetTracker.cpp
        src/widgets/AROverlayManager.cpp
        src/widgets/AROverlayWidget.cpp
    )
    set(AR_HEADERS
        src/core/YoloDetector.h
        src/core/TargetTracker.h
        src/widgets/AROverlayManager.h
        src/widgets/AROverlayWidget.h
    )
    list(APPEND SOURCES ${AR_SOURCES})
    list(APPEND HEADERS ${AR_HEADERS})
    message(STATUS "AR Overlay system enabled")
endif()

# 本地配置优先；干净克隆没有 config.json 时使用公开示例生成运行配置。
set(RM26_CONFIG_EXAMPLE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config.example.json")
set(RM26_CONFIG_LOCAL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
if(EXISTS "${RM26_CONFIG_LOCAL_FILE}")
    set(RM26_RUNTIME_CONFIG_SOURCE "${RM26_CONFIG_LOCAL_FILE}")
    message(STATUS "Using local runtime config: config.json")
elseif(EXISTS "${RM26_CONFIG_EXAMPLE_FILE}")
    set(RM26_RUNTIME_CONFIG_SOURCE "${RM26_CONFIG_EXAMPLE_FILE}")
    message(STATUS "config.json not found; using config.example.json")
else()
    message(FATAL_ERROR "Missing config.example.json; cannot prepare runtime configuration")
endif()
configure_file(
    "${RM26_RUNTIME_CONFIG_SOURCE}"
    "${CMAKE_CURRENT_BINARY_DIR}/config.json"
    COPYONLY
)

option(RM_VALIDATE_ASSETS "Validate image resources and portable paths before build" ON)
if(RM_VALIDATE_ASSETS)
    add_custom_target(validate_assets
        COMMAND ${CMAKE_COMMAND}
            -DRM_PROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateAssets.cmake
        COMMENT "Validating resources and cross-platform path portability"
    )
endif()


# 开发观测接口保持独立，只有显式开启时才进入生产目标的源码列表。
if(RM26_ENABLE_DEVTOOLS)
    list(APPEND SOURCES
        src/devhooks/ClientDevHooks.cpp
    )
    list(APPEND HEADERS
        src/devhooks/ClientDevHooks.h
    )
endif()

# 创建静态库
add_library(RoboMasterClientLib STATIC ${SOURCES} ${HEADERS})
if(TARGET validate_assets)
    add_dependencies(RoboMasterClientLib validate_assets)
endif()

target_include_directories(RoboMasterClientLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/devhooks
    ${CMAKE_CURRENT_BINARY_DIR}/src/network/proto # 生成后的嵌套 Protobuf 头文件
    ${CMAKE_CURRENT_BINARY_DIR} # 其余生成头文件
    ${FFMPEG_INCLUDE_DIRS} # FFmpeg 头文件
)

# Paho MQTT 头文件与能力标记
if(PAHO_MQTT_FOUND)
    target_include_directories(RoboMasterClientLib PUBLIC ${PAHO_MQTT_INCLUDE_DIRS})
    target_compile_definitions(RoboMasterClientLib PUBLIC RM_HAS_MQTT)
endif()

# ONNX Runtime 头文件与能力标记
if(ONNXRUNTIME_FOUND)
    target_include_directories(RoboMasterClientLib PUBLIC ${ONNXRUNTIME_INCLUDE_DIRS})
    target_compile_definitions(RoboMasterClientLib PUBLIC RM_HAS_AR_OVERLAY)
endif()

if(RM26_ENABLE_DEVTOOLS)
    target_compile_definitions(RoboMasterClientLib PUBLIC RM26_ENABLE_DEVTOOLS)
endif()

target_link_libraries(RoboMasterClientLib PUBLIC
    Qt6::Core
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
    Qt6::SerialPort
    Qt6::Svg
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    protobuf::libprotobuf
)

# HttpServer 仅随显式开启的开发观测构建链接。
if(RM26_ENABLE_DEVTOOLS)
    target_link_libraries(RoboMasterClientLib PUBLIC Qt6::HttpServer)
    target_compile_definitions(RoboMasterClientLib PUBLIC RM_HAS_QT_HTTP_SERVER)
endif()

if(RM_HAS_FFMPEG)
    target_compile_definitions(RoboMasterClientLib PUBLIC RM_ENABLE_FFMPEG)
    target_include_directories(RoboMasterClientLib PUBLIC ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(RoboMasterClientLib PUBLIC ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(RoboMasterClientLib PUBLIC ${FFMPEG_LIBRARIES})
endif()

# ONNX Runtime 链接库
if(ONNXRUNTIME_FOUND)
    target_link_libraries(RoboMasterClientLib PUBLIC ${ONNXRUNTIME_LIBRARIES})
endif()

# Paho MQTT 链接库
if(PAHO_MQTT_FOUND)
    target_link_libraries(RoboMasterClientLib PUBLIC ${PAHO_MQTT_LIBRARIES})
endif()

set(RM_ABSL_LINK_TARGETS
    absl::base
    absl::strings
)
foreach(_absl_target IN ITEMS absl::log absl::log_internal_check_op absl::log_internal_message)
    if(TARGET ${_absl_target})
        list(APPEND RM_ABSL_LINK_TARGETS ${_absl_target})
    endif()
endforeach()

target_link_libraries(RoboMasterClientLib PUBLIC ${RM_ABSL_LINK_TARGETS})

# 链接Qt库到静态库
if(Qt6WebEngineWidgets_FOUND)
    target_link_libraries(RoboMasterClientLib PUBLIC Qt6::WebEngineWidgets)
    target_compile_definitions(RoboMasterClientLib PUBLIC RM_HAS_WEBENGINE)
    message(STATUS "Qt6 WebEngineWidgets found. Bilibili playback enabled.")
else()
    message(WARNING "Qt6 WebEngineWidgets NOT found. Bilibili playback disabled.")
endif()

# 创建可执行文件
if(WIN32)
    add_executable(RoboMasterClient2025 src/main.cpp resources/app_icon.rc)
elseif(APPLE)
    add_executable(RoboMasterClient2025 src/main.cpp)
    set_source_files_properties(resources/images/app_icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(RoboMasterClient2025 PRIVATE resources/images/app_icon.icns)
else()
    add_executable(RoboMasterClient2025 src/main.cpp)
endif()

# 链接静态库
target_link_libraries(RoboMasterClient2025 PRIVATE RoboMasterClientLib)

# 设置目标属性
set_target_properties(RoboMasterClient2025 PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# 构建后将声音资源和运行配置复制到可执行文件目录。
add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds
    $<TARGET_FILE_DIR:RoboMasterClient2025>/resources/sounds
    COMMENT "Copying sound resources to output directory"
)
add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${RM26_RUNTIME_CONFIG_SOURCE}"
    $<TARGET_FILE_DIR:RoboMasterClient2025>/config.json
    COMMENT "Copying config.json to output directory"
)

# Windows 构建后按实际依赖目标复制运行库，避免写死安装路径。
if(WIN32)
    # 优先从 Protobuf 导入目标复制运行库。
    if(TARGET protobuf::libprotobuf)
        add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:protobuf::libprotobuf>
            $<TARGET_FILE_DIR:RoboMasterClient2025>/
            COMMENT "Copying libprotobuf (from protobuf target) to output directory"
        )
    else()
        # 没有导入目标时，再从 VCPKG_ROOT 查找，不写死绝对路径。
        if(DEFINED ENV{VCPKG_ROOT})
            set(_VCPKG_ROOT $ENV{VCPKG_ROOT})
        elseif(DEFINED VCPKG_ROOT)
            set(_VCPKG_ROOT ${VCPKG_ROOT})
        endif()

        if(DEFINED _VCPKG_ROOT)
            # 优先使用已配置 triplet，否则尝试常见 Windows triplet。
            if(NOT DEFINED VCPKG_TARGET_TRIPLET)
                set(_TRIPLETS "x64-windows;x64-mingw-dynamic;x64-mingw-static;x64-windows-static")
            else()
                set(_TRIPLETS ${VCPKG_TARGET_TRIPLET})
            endif()
            foreach(_t IN LISTS _TRIPLETS)
                set(_candidate "${_VCPKG_ROOT}/installed/${_t}/bin/libprotobuf.dll")
                if(EXISTS ${_candidate})
                    add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_candidate}"
                        $<TARGET_FILE_DIR:RoboMasterClient2025>/
                        COMMENT "Copying libprotobuf.dll (from VCPKG ${_t})"
                    )
                    break()
                endif()
            endforeach()
        endif()
    endif()

    # Abseil 同样优先使用导入目标提供的位置。
    set(_ABSL_TARGETS absl::base absl::strings absl::log absl::log_internal_check_op absl::log_internal_message)
    foreach(_t IN LISTS _ABSL_TARGETS)
        if(TARGET ${_t})
            # 部分 Abseil 目标只有接口定义，因此先读取显式导入位置。
            get_target_property(_imported_loc ${_t} IMPORTED_LOCATION)
            if(NOT _imported_loc)
                get_target_property(_imported_loc ${_t} IMPORTED_LOCATION_RELEASE)
            endif()
            if(NOT _imported_loc)
                get_target_property(_imported_loc ${_t} IMPORTED_LOCATION_DEBUG)
            endif()

            if(_imported_loc)
                add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_imported_loc}"
                    $<TARGET_FILE_DIR:RoboMasterClient2025>/
                    COMMENT "Copying ${_t} DLL (imported location) to output directory"
                )
            else()
                # 非接口目标可以通过生成器表达式取得实际文件。
                get_target_property(_t_type ${_t} TYPE)
                if(NOT _t_type STREQUAL "INTERFACE_LIBRARY")
                    add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:${_t}>
                        $<TARGET_FILE_DIR:RoboMasterClient2025>/
                        COMMENT "Copying ${_t} DLL to output directory"
                    )
                endif()
            endif()
        endif()
    endforeach()

    # 没有可用导入目标时，从 vcpkg 的 bin 目录补充 Abseil DLL。
    if(DEFINED _VCPKG_ROOT)
        file(GLOB ABSL_DLLS "${_VCPKG_ROOT}/installed/*/bin/libabsl_*.dll")
        if(ABSL_DLLS)
            foreach(DLL ${ABSL_DLLS})
                add_custom_command(TARGET RoboMasterClient2025 POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL}
                    $<TARGET_FILE_DIR:RoboMasterClient2025>/
                    COMMENT "Copying Abseil DLLs (from VCPKG)..."
                )
            endforeach()
        endif()
    endif()
endif()

# 创建声音资源目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/sounds)

# 每次构建只同步 sounds，避免反复复制整个 resources 目录
add_custom_target(create_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds ${CMAKE_CURRENT_BINARY_DIR}/resources/sounds
    COMMENT "Copying sound resources directory"
)
add_dependencies(RoboMasterClient2025 create_resources)

# 安装树保留与 QRC 一致的目录，便于排查资源包并兼容现有文件系统兜底。
install(
    DIRECTORY resources/images/resultpanel
    DESTINATION resources/images
    FILES_MATCHING PATTERN "*.png"
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
