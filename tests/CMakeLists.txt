
enable_testing()
message(STATUS "Processing tests directory...")

find_package(Qt6 COMPONENTS Test Qml Quick REQUIRED)

function(rm26_add_qt_test test_name target_name)
    add_test(NAME ${test_name} COMMAND ${target_name})
    if(WIN32)
        set(_rm26_test_env_mod
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:RoboMasterClient2025>"
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:RoboMasterClient2025>/platforms"
            ENVIRONMENT_MODIFICATION
                "${_rm26_test_env_mod}"
        )
    endif()
endfunction()

# 模拟器依赖提示测试
add_test(
    NAME TestLinuxSimDependencyHint
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_linux_sim_dependency_hint.sh
)

# 比赛状态测试
add_executable(test_game_data unit/test_game_data.cpp)
target_link_libraries(test_game_data PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestGameData test_game_data)

# 血条组件测试
add_executable(test_health_bar unit/test_health_bar.cpp)
target_link_libraries(test_health_bar PRIVATE Qt6::Core Qt6::Widgets Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestHealthBar test_health_bar)



# 比赛结果组件测试
add_executable(test_game_result_widget unit/test_game_result_widget.cpp)
target_link_libraries(test_game_result_widget PRIVATE Qt6::Core Qt6::Widgets Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestGameResultWidget test_game_result_widget)

# Protobuf 测试
add_executable(test_protobuf unit/test_protobuf.cpp)
target_link_libraries(test_protobuf PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib protobuf::libprotobuf)
rm26_add_qt_test(TestProtobuf test_protobuf)

# 协议测试
add_executable(test_protocol unit/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib protobuf::libprotobuf)
rm26_add_qt_test(TestProtocol test_protocol)

# 输入快捷键策略测试
add_executable(test_input_hotkey_policy unit/test_input_hotkey_policy.cpp)
target_link_libraries(test_input_hotkey_policy PRIVATE Qt6::Core Qt6::Gui Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestInputHotkeyPolicy test_input_hotkey_policy)

# 设置面板快捷键测试
add_executable(test_settings_panel_shortcuts unit/test_settings_panel_shortcuts.cpp)
target_link_libraries(test_settings_panel_shortcuts PRIVATE Qt6::Core Qt6::Qml Qt6::Quick Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestSettingsPanelShortcuts test_settings_panel_shortcuts)
set_property(TEST TestSettingsPanelShortcuts APPEND PROPERTY ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")

# 兑换指令策略测试
add_executable(test_exchange_command_policy unit/test_exchange_command_policy.cpp)
target_link_libraries(test_exchange_command_policy PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestExchangeCommandPolicy test_exchange_command_policy)

# 弹窗覆盖策略测试
add_executable(test_popup_overlay_policy unit/test_popup_overlay_policy.cpp)
target_link_libraries(test_popup_overlay_policy PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestPopupOverlayPolicy test_popup_overlay_policy)

# 战术分析器测试
add_executable(test_tactical_analyzer unit/test_tactical_analyzer.cpp)
target_link_libraries(test_tactical_analyzer PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestTacticalAnalyzer test_tactical_analyzer)

# 战术指挥页测试
add_executable(test_tactical_command_page unit/test_tactical_command_page.cpp)
target_link_libraries(test_tactical_command_page PRIVATE Qt6::Core Qt6::Qml Qt6::Quick Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestTacticalCommandPage test_tactical_command_page)
set_property(TEST TestTacticalCommandPage APPEND PROPERTY ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen")
foreach(_scale_case IN ITEMS "Scale125;1.25" "Scale150;1.5" "Scale200;2")
    list(GET _scale_case 0 _scale_name)
    list(GET _scale_case 1 _scale_factor)
    set(_test_name "TestTacticalCommandPage${_scale_name}")
    rm26_add_qt_test(${_test_name} test_tactical_command_page)
    set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_SCALE_FACTOR=${_scale_factor}")
endforeach()

# 主窗口快捷键路由与战术状态重置集成测试
add_executable(test_mainwindow_tactical_hotkey_integration
    unit/test_mainwindow_tactical_hotkey_integration.cpp)
target_link_libraries(test_mainwindow_tactical_hotkey_integration PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestMainWindowTacticalHotkeyIntegration
    test_mainwindow_tactical_hotkey_integration)
set_property(TEST TestMainWindowTacticalHotkeyIntegration APPEND PROPERTY ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen"
    "QT_QUICK_BACKEND=software"
    "RM_DISABLE_AUDIO=1"
    "RM_DEVTOOLS=0")

# 定时事件规则测试
add_executable(test_timed_event_rules unit/test_timed_event_rules.cpp)
target_link_libraries(test_timed_event_rules PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestTimedEventRules test_timed_event_rules)

# MQTT 管理器测试
add_executable(test_mqtt_manager unit/test_mqtt_manager.cpp)
target_link_libraries(test_mqtt_manager PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestMqttManager test_mqtt_manager)

# 主窗口状态策略测试
add_executable(test_mainwindow_state_policy unit/test_mainwindow_state_policy.cpp)
target_link_libraries(test_mainwindow_state_policy PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestMainWindowStatePolicy test_mainwindow_state_policy)

# 主窗口元对象测试
add_executable(test_mainwindow_metaobject unit/test_mainwindow_metaobject.cpp)
target_link_libraries(test_mainwindow_metaobject PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestMainWindowMetaObject test_mainwindow_metaobject)

# 日志文件命名测试
add_executable(test_log_file_naming unit/test_log_file_naming.cpp)
target_link_libraries(test_log_file_naming PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestLogFileNaming test_log_file_naming)

# UDP 图传数据报头日志测试
add_executable(test_video_datagram_logger unit/test_video_datagram_logger.cpp)
target_link_libraries(test_video_datagram_logger PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestVideoDatagramLogger test_video_datagram_logger)

# H.264 分片流卡顿恢复策略测试
add_executable(test_h264_stream_recovery_policy
    unit/test_h264_stream_recovery_policy.cpp)
target_link_libraries(test_h264_stream_recovery_policy PRIVATE
    Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestH264StreamRecoveryPolicy
    test_h264_stream_recovery_policy)

# Annex-B 访问单元解析测试（IDR 可能与 SPS/PPS 位于同一个数据包）
add_executable(test_h264_annex_b unit/test_h264_annex_b.cpp)
target_link_libraries(test_h264_annex_b PRIVATE
    Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestH264AnnexB test_h264_annex_b)

if(RM_HAS_FFMPEG)
    # 使用真实 FFmpeg 固定样例验证 SPS/PPS/SEI/IDR 与重置恢复。
    add_executable(test_h264_decoder unit/test_h264_decoder.cpp)
    target_link_libraries(test_h264_decoder PRIVATE
        Qt6::Core Qt6::Gui Qt6::Test RoboMasterClientLib)
    rm26_add_qt_test(TestH264Decoder test_h264_decoder)
endif()

# 视频显示配置默认值与环境变量覆盖测试
add_executable(test_config_manager_video unit/test_config_manager_video.cpp)
target_link_libraries(test_config_manager_video PRIVATE
    Qt6::Core Qt6::Test RoboMasterClientLib)
rm26_add_qt_test(TestConfigManagerVideo test_config_manager_video)

# 性能基准测试
add_executable(test_benchmark benchmark/test_benchmark.cpp)
target_link_libraries(test_benchmark PRIVATE Qt6::Core Qt6::Test RoboMasterClientLib protobuf::libprotobuf)
rm26_add_qt_test(TestBenchmark test_benchmark)
