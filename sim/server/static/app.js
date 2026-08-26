const hasSocketIo = typeof window.io === "function";
const hasVue = typeof window.Vue === "function";

if (!hasSocketIo || !hasVue) {
    window.addEventListener("DOMContentLoaded", () => {
        const appRoot = document.getElementById("app");
        if (appRoot) {
            appRoot.innerHTML = "<section class='panel'><h3>前端依赖加载失败</h3><p>请检查 <code>/vendor/socket.io.min.js</code> 和 <code>/vendor/vue.min.js</code> 是否可访问。</p></section>";
        }
    });
    console.error("Missing frontend dependencies: Vue or Socket.IO", {
        hasSocketIo,
        hasVue
    });
} else {
    const socket = window.io();
    const FORTRESS_CAPTURE_DURATION_SEC = 20;
    const mapGeometry = window.MapGeometry || {
        clientPointToCanvas(clientX, clientY, rect, canvasWidth, canvasHeight) {
            const cssWidth = Number(rect && rect.width) || 0;
            const cssHeight = Number(rect && rect.height) || 0;
            if (cssWidth <= 0 || cssHeight <= 0) {
                return { x: 0, y: 0 };
            }
            return {
                x: (clientX - rect.left) * canvasWidth / cssWidth,
                y: (clientY - rect.top) * canvasHeight / cssHeight
            };
        }
    };

    new window.Vue({
    el: '#app',
    data: {
        connected: false,
        currentRobotId: 1, // 当前登录机器人 ID
        baseTeam: 'red',
        outpostTeam: 'red',
        runeTeam: 'red',
        runeType: 1,
        dartGateTeam: 'red',
        gameStatus: {
            current_round: 1,
            total_rounds: 3,
            current_stage: 0,
            stage_countdown_sec: 0,
            red_score: 0,
            blue_score: 0,
            is_paused: false
        },
        matchForm: {
            current_round: 1,
            total_rounds: 3
        },
        isEditingMatchForm: false,
        isEditingGlobalLogisticsStatus: false,
        globalLogisticsStatus: {
            red_economy: 550,
            blue_economy: 550,
            red_total_economy_obtained: 550,
            blue_total_economy_obtained: 550,
            red_total_damage: 0,
            blue_total_damage: 0,
            red_tech_level: 1,
            blue_tech_level: 1,
            red_encryption_level: 1,
            blue_encryption_level: 1
        },
        globalUnitStatus: {
            robot_health: [600, 600, 600, 600, 600, 600, 600, 600],
            robot_max_hp: [600, 600, 600, 600, 600, 600, 600, 600],
            robot_bullets: [0, 0, 0, 0, 0, 0, 0, 0],
            robot_fire_rate: [0, 0, 0, 0, 0, 0, 0, 0],
            robot_level: [1, 1, 1, 1, 1, 1, 1, 1],
            robot_heat: [0, 0, 0, 0, 0, 0, 0, 0],
            robot_power: [0, 0, 0, 0, 0, 0, 0, 0],
            base_health: 5000,
            outpost_health: 1500,
            base_status: 0,
            base_shield: 1000
        },
        refereeInfo: {
            rune_status: 1,
            activated_arms: 0,
            average_rings: 0,
            last_rune_activation: ''
        },
        refereeInfoByTeam: {
            red: {
                rune_status: 1,
                activated_arms: 0,
                average_rings: 0,
                last_rune_activation: ''
            },
            blue: {
                rune_status: 1,
                activated_arms: 0,
                average_rings: 0,
                last_rune_activation: ''
            }
        },
        airSupport: {
            red_status: 0,
            blue_status: 0,
            red_left_time: 30,
            blue_left_time: 30,
            red_default_time: 30,
            blue_default_time: 30,
            red_cost_coins: 0,
            blue_cost_coins: 0,
            red_is_being_targeted: 0,
            blue_is_being_targeted: 0,
            red_shooter_status: 1,
            blue_shooter_status: 1,
            active_team: ''
        },
        airSupportDraft: {
            red_status: 0,
            blue_status: 0,
            red_left_time: 30,
            blue_left_time: 30,
            red_default_time: 30,
            blue_default_time: 30,
            red_cost_coins: 0,
            blue_cost_coins: 0,
            red_is_being_targeted: 0,
            blue_is_being_targeted: 0,
            red_shooter_status: 1,
            blue_shooter_status: 1
        },
        runeInputs: {
            activated_arms: 0,
            average_rings: 0
        },
        isEditingRuneMetrics: false,
        controlValues: {
            red_score: 0,
            blue_score: 0,
            red_economy: 550,
            blue_economy: 550
        },
        isEditingControlValues: false,
        editingRobotDetailIndices: Array(8).fill(false),
        editingRobotPoseIndices: Array(8).fill(false),
        techCoreMotionState: {
            maximum_difficulty_level: 1,
            basic_state: 1,
            status: 1,
            putin_state: 0,
            move_state: 0,
            rotate_state: 0,
            enemy_core_status: 0,
            remain_time_all: 0,
            remain_time_step: 0
        },
        techCoreMotionStateDraft: {
            maximum_difficulty_level: 1,
            basic_state: 1,
            status: 1,
            putin_state: 0,
            move_state: 0,
            rotate_state: 0,
            enemy_core_status: 0,
            remain_time_all: 0,
            remain_time_step: 0
        },
        techCoreMotionStateDirty: false,
        deployModeStatus: 0,
        // 每队的英雄部署状态（0 未部署 / 1 已部署）
        deployStatusByTeam: { red: 0, blue: 0 },
        robotIds: [1, 3, 6, 7, 101, 103, 106, 107],
        globalSpecialMechanism: {
            mechanism_id: [1, 2],
            mechanism_time_sec: [0, 0]
        },
        specialMechanismForm: {
            ally_fortress_sec: 0,
            enemy_fortress_sec: 0
        },
        isEditingSpecialMechanismForm: false,
        robotInjuryStats: {},
        robotInjuryForm: {
            robot_id: 1,
            total_damage: 0,
            collision_damage: 0,
            small_projectile_damage: 0,
            large_projectile_damage: 0,
            dart_splash_damage: 0,
            module_offline_damage: 0,
            offline_damage: 0,
            penalty_damage: 0,
            server_kill_damage: 0,
            killer_id: 0
        },
        isEditingRobotInjuryForm: false,
        robotPerformanceSelectionSync: {
            shooter: 1,
            chassis: 1,
            sentry_control: 0
        },
        robotPerformanceForm: {
            shooter: 1,
            chassis: 1,
            sentry_control: 0
        },
        isEditingRobotPerformanceForm: false,
        robotStaticStatus: {
            connection_state: 1,
            field_state: 0,
            alive_state: 1,
            robot_id: 1,
            robot_type: 1,
            performance_system_shooter: 1,
            performance_system_chassis: 1,
            level: 1,
            max_health: 600,
            max_heat: 240,
            heat_cooldown_rate: 20,
            max_power: 120,
            max_buffer_energy: 60,
            max_chassis_energy: 120
        },
        robotStaticStatusForm: {
            connection_state: 1,
            field_state: 0,
            alive_state: 1,
            robot_id: 1,
            robot_type: 1,
            performance_system_shooter: 1,
            performance_system_chassis: 1,
            level: 1,
            max_health: 600,
            max_heat: 240,
            heat_cooldown_rate: 20,
            max_power: 120,
            max_buffer_energy: 60,
            max_chassis_energy: 120
        },
        robotStaticStatusDirty: false,
        isSyncingRobotStaticStatusForm: false,
        robotDynamicStatus: {
            current_health: 600,
            current_heat: 0,
            last_projectile_fire_rate: 0,
            current_chassis_energy: 0,
            current_buffer_energy: 0,
            current_experience: 100,
            experience_for_upgrade: 1000,
            total_projectiles_fired: 0,
            remaining_ammo: 0,
            is_out_of_combat: false,
            out_of_combat_countdown: 0,
            can_remote_heal: true,
            can_remote_ammo: true
        },
        robotDynamicStatusForm: {
            current_health: 600,
            current_heat: 0,
            last_projectile_fire_rate: 0,
            current_chassis_energy: 0,
            current_buffer_energy: 0,
            current_experience: 100,
            experience_for_upgrade: 1000,
            total_projectiles_fired: 0,
            remaining_ammo: 0,
            is_out_of_combat: 0,
            out_of_combat_countdown: 0,
            can_remote_heal: 1,
            can_remote_ammo: 1
        },
        robotDynamicStatusDirty: false,
        isSyncingRobotDynamicStatusForm: false,
        robotModuleStatus: {
            power_manager: 1,
            rfid: 1,
            light_strip: 1,
            small_shooter: 1,
            big_shooter: 1,
            uwb: 1,
            armor: 1,
            video_transmission: 1,
            capacitor: 1,
            main_controller: 1,
            laser_detection_module: 1
        },
        robotModuleStatusForm: {
            power_manager: 1,
            rfid: 1,
            light_strip: 1,
            small_shooter: 1,
            big_shooter: 1,
            uwb: 1,
            armor: 1,
            video_transmission: 1,
            capacitor: 1,
            main_controller: 1,
            laser_detection_module: 1
        },
        robotModuleStatusDirty: false,
        isSyncingRobotModuleStatusForm: false,
        sentryStatusSync: {
            posture_id: 0,
            is_weakened: false,
            is_powered: false
        },
        sentryStatusForm: {
            posture_id: 0,
            is_weakened: 0,
            is_powered: 0
        },
        isEditingSentryStatusForm: false,
        lastSentryCtrlResult: {
            command_id: 0,
            result_code: 0
        },
        sentryCtrlResultForm: {
            command_id: 0,
            result_code: 0
        },
        isEditingSentryCtrlResultForm: false,
        robotPathPlanInfo: {
            intention: 1,
            start_pos_x: 0,
            start_pos_y: 0,
            offset_x: [],
            offset_y: [],
            sender_id: 7
        },
        robotPathPlanForm: {
            intention: 1,
            start_pos_x: 0,
            start_pos_y: 0,
            offset_x: [48, 92, 136],
            offset_y: [0, 24, 58],
            sender_id: 7
        },
        isEditingRobotPathPlanForm: false,
        robotPathPlanDirty: false,
        isSyncingRobotPathPlanForm: false,
        dartStatus: {
            target_id: 2,
            open: 0
        },
        dartStatusByTeam: {
            red: {
                target_id: 2,
                open: 0
            },
            blue: {
                target_id: 2,
                open: 0
            }
        },
        customByteBlockHex: '',
        customByteBlockForm: {
            encoding: 'hex',
            hex_data: '01 02 03 04',
            text_data: 'SIM'
        },
        isEditingCustomByteBlockForm: false,
        lastGameResult: {
            winner: 0,
            winner_label: '未结算',
            reason: '',
            red_score: 0,
            blue_score: 0
        },
        specialMechanismForm: {
            ally_fortress_sec: 0,
            enemy_fortress_sec: 0
        },
        robotInjuryForm: {
            robot_id: 1,
            total_damage: 0,
            collision_damage: 0,
            small_projectile_damage: 0,
            large_projectile_damage: 0,
            dart_splash_damage: 0,
            module_offline_damage: 0,
            offline_damage: 0,
            penalty_damage: 0,
            server_kill_damage: 0,
            killer_id: 0
        },
        robotPathPlanForm: {
            intention: 1,
            start_pos_x: 0,
            start_pos_y: 0,
            offset_x: [48, 92, 136],
            offset_y: [0, 24, 58],
            sender_id: 7
        },
        robotPerformanceForm: {
            shooter: 1,
            chassis: 1,
            sentry_control: 0
        },
        robotPerformanceShooterOptions: [
            { value: 0, label: '0: 初始设置' },
            { value: 1, label: '1: 冷却优先' },
            { value: 2, label: '2: 爆发优先' },
            { value: 3, label: '3: 英雄近战优先' },
            { value: 4, label: '4: 英雄远程优先' }
        ],
        robotPerformanceChassisOptions: [
            { value: 0, label: '0: 初始设置' },
            { value: 1, label: '1: 血量优先' },
            { value: 2, label: '2: 功率优先' },
            { value: 3, label: '3: 英雄近战优先' },
            { value: 4, label: '4: 英雄远程优先' }
        ],
        robotPerformanceSentryControlOptions: [
            { value: 0, label: '0: 哨兵自动控制' },
            { value: 1, label: '1: 哨兵半自动控制' }
        ],
        sentryCtrlResultForm: {
            command_id: 0,
            result_code: 0
        },
        isNormalizingRobotInjuryForm: false,
        customByteBlockForm: {
            encoding: 'hex',
            hex_data: '01 02 03 04',
            text_data: 'SIM'
        },
        eventCommand: {
            event_id: 14,
            param: ''
        },
        isEditingEventCommand: false,
        outpostStatusForm: {
            status: 1
        },
        isEditingOutpostStatusForm: false,
        baseShieldForm: {
            shield: 0
        },
        isEditingBaseShieldForm: false,
        buffCommand: {
            robot_id: 0,
            buff_type: 1,
            buff_level: 1,
            buff_max_time: 30,
            buff_left_time: 30
        },
        buffPresetOptions: [
            { buff_type: 1, label: '攻击增益' },
            { buff_type: 2, label: '防御/易伤增益' },
            { buff_type: 3, label: '射击热量冷却增益' },
            { buff_type: 4, label: '底盘功率增益' },
            { buff_type: 5, label: '回血增益' },
            { buff_type: 6, label: '可兑换允许发弹量' },
            { buff_type: 7, label: '地形跨越增益' }
        ],
        sentryPostureOptions: [
            { value: 1, label: '1: 进攻姿态' },
            { value: 2, label: '2: 防御姿态' },
            { value: 3, label: '3: 移动姿态' }
        ],
        sentryCommandOptions: [
            { value: 1, label: '1: 补血点补弹' },
            { value: 3, label: '3: 远程补弹' },
            { value: 4, label: '4: 远程回血' },
            { value: 5, label: '5: 确认复活' },
            { value: 6, label: '6: 确认花费金币复活' },
            { value: 7, label: '7: 切换为进攻姿态' },
            { value: 8, label: '8: 切换为防御姿态' },
            { value: 9, label: '9: 切换为移动姿态' },
            { value: 10, label: '10: 切换为强化进攻姿态' },
            { value: 11, label: '11: 切换为强化防御姿态' },
            { value: 12, label: '12: 切换为强化移动姿态' }
        ],
        sentryResultCodeOptions: [
            { value: 0, label: '0: 成功' },
            { value: 1, label: '1: 失败' }
        ],
        isEditingBuffCommand: false,
        positions: [
            { x: 2.0, y: 2.0, angle: 0.0 },
            { x: 5.0, y: 2.0, angle: 0.0 },
            { x: 8.0, y: 2.0, angle: 0.0 },
            { x: 11.0, y: 2.0, angle: 0.0 },
            { x: 17.0, y: 10.0, angle: 180.0 },
            { x: 20.0, y: 10.0, angle: 180.0 },
            { x: 23.0, y: 10.0, angle: 180.0 },
            { x: 26.0, y: 10.0, angle: 180.0 }
        ],
        simulationConfig: {
            mode: 'quick',
            speed: 1,
            features: {
                positions: true,
                hp: true,
                events: true
            }
        },
        simulationStatus: {
            state: 'idle',
            mode: 'quick',
            speed: 1,
            elapsed: 0,
            remaining: 90,
            duration: 90,
            message: '选择演示模式后即可一键启动',
            recentEvents: [],
            lastFrameAt: ''
        },
        simulationUiMessage: '',
        simulationManualOverride: false,
        isMapFullscreen: false,
        fullscreenChangeHandler: null,
        windowResizeHandler: null,
        logs: [],
        draggingRobot: -1,
        canvas: null,
        ctx: null,
        robotPathPlanPoints: [
            { x: 56, y: 88 },
            { x: 104, y: 88 },
            { x: 148, y: 112 },
            { x: 192, y: 146 }
        ],
        pathPlanCanvas: null,
        pathPlanCtx: null,
        draggingPathPlanPoint: -1,
        pathPlanWidth: 280,
        pathPlanHeight: 150,
        pathPlanPadding: 10,
        mapWidth: 28.0,
        mapHeight: 15.0,
        videos: [],
        selectedVideo: '',
        customVideoUrl: '',
        videoStatus: {
            is_streaming: false,
            paused: false,
            current_video: '',
            frame_id: 0
        },
        selectedIndustrialVideo: '',
        industrialCameraStatus: {
            is_streaming: false,
            paused: false,
            current_video: '',
            frame_id: 0
        },
        logFilters: {
            info: true,
            command: true,
            error: true,
            video: true
        },
        minimapLegend: [
            { team: 'red', role: 'Hero', robotId: 1, icon: 'H' },
            { team: 'red', role: 'Infantry', robotId: 3, icon: 'I' },
            { team: 'red', role: 'Scout', robotId: 6, icon: 'S' },
            { team: 'red', role: 'Sentry', robotId: 7, icon: 'T' },
            { team: 'blue', role: 'Hero', robotId: 101, icon: 'H' },
            { team: 'blue', role: 'Infantry', robotId: 103, icon: 'I' },
            { team: 'blue', role: 'Scout', robotId: 106, icon: 'S' },
            { team: 'blue', role: 'Sentry', robotId: 107, icon: 'T' }
        ]
    },
    mounted() {
        // 从 localStorage 读取当前机器人 ID
        const savedRobotId = localStorage.getItem('currentRobotId');
        if (savedRobotId !== null) {
            this.currentRobotId = parseInt(savedRobotId) || 1;
        }
        this.syncRobotPathPlanEditorFromInfo(this.robotPathPlanInfo);
        this.$nextTick(() => {
            this.initCanvas();
            this.initPathPlanCanvas();
        });

        this.windowResizeHandler = () => this.resizeMinimapCanvas();
        this.fullscreenChangeHandler = () => {
            const stage = document.getElementById('simulationStage');
            const fullscreenElement = document.fullscreenElement || document.webkitFullscreenElement;
            this.isMapFullscreen = fullscreenElement === stage;
            this.$nextTick(() => window.requestAnimationFrame(() => this.resizeMinimapCanvas()));
        };
        window.addEventListener('resize', this.windowResizeHandler);
        document.addEventListener('fullscreenchange', this.fullscreenChangeHandler);
        document.addEventListener('webkitfullscreenchange', this.fullscreenChangeHandler);

        socket.on('connect', () => {
            this.connected = true;
            this.simulationUiMessage = '';
            this.initCanvas();
            this.initPathPlanCanvas();
            // 连接建立时，发送当前机器人 ID 给服务器
            this.sendCommand('set_current_robot', { robot_id: this.currentRobotId });
        });

        socket.on('disconnect', () => {
            this.connected = false;
            this.simulationUiMessage = '与仿真服务的连接已断开，赛事控制暂不可用';
        });

        socket.on('update', (data) => {
            // 如果后端有 currentRobotId，同步过来
            if (data.currentRobotId !== undefined && data.currentRobotId !== null) {
                this.currentRobotId = parseInt(data.currentRobotId);
            }

            const nextGameStatus = Object.assign({}, this.gameStatus, data.gameStatus || {});
            this.gameStatus = nextGameStatus;
            if (!this.isEditingMatchForm) {
                this.matchForm.current_round = Number(this.gameStatus.current_round ?? this.matchForm.current_round);
                this.matchForm.total_rounds = Number(this.gameStatus.total_rounds ?? this.matchForm.total_rounds);
            }

            const logisticsFallback = {
                red_economy: nextGameStatus.red_economy,
                blue_economy: nextGameStatus.blue_economy
            };
            if (!this.isEditingGlobalLogisticsStatus) {
                this.globalLogisticsStatus = Object.assign(
                    {},
                    this.globalLogisticsStatus,
                    logisticsFallback,
                    data.globalLogisticsStatus || {}
                );
            }

            this.globalUnitStatus = this.mergeGlobalUnitStatusPreservingEdits(data.globalUnitStatus || {});
            this.refereeInfo = data.refereeInfo || this.refereeInfo;
            this.refereeInfoByTeam = data.refereeInfoByTeam || this.refereeInfoByTeam;
            this.airSupport = Object.assign({}, this.airSupport, data.airSupport || {});
            if (!this.isEditingRuneMetrics) {
                const selectedRuneInfo = this.getRuneInfo(this.runeTeam);
                this.runeInputs.activated_arms = selectedRuneInfo.activated_arms ?? 0;
                this.runeInputs.average_rings = selectedRuneInfo.average_rings ?? 0;
            }
            if (!this.isEditingControlValues) {
                this.controlValues.red_score = this.gameStatus.red_score ?? this.controlValues.red_score;
                this.controlValues.blue_score = this.gameStatus.blue_score ?? this.controlValues.blue_score;
                this.controlValues.red_economy = this.globalLogisticsStatus.red_economy ?? this.controlValues.red_economy;
                this.controlValues.blue_economy = this.globalLogisticsStatus.blue_economy ?? this.controlValues.blue_economy;
            }
            const nextTechCoreMotionState = Object.assign(
                {},
                this.techCoreMotionState,
                data.techCoreMotionState || {}
            );
            if (nextTechCoreMotionState.basic_state === undefined) {
                nextTechCoreMotionState.basic_state = nextTechCoreMotionState.status ?? 1;
            }
            nextTechCoreMotionState.status = nextTechCoreMotionState.basic_state;
            this.techCoreMotionState = nextTechCoreMotionState;
            if (!this.techCoreMotionStateDirty) {
                this.techCoreMotionStateDraft = Object.assign({}, nextTechCoreMotionState);
            }
            // 兼容：优先读取按队部署状态；回退到单一 deployModeStatus
            if (data.deployModeStatusByTeam) {
                this.deployStatusByTeam = Object.assign({}, this.deployStatusByTeam, data.deployModeStatusByTeam);
            } else {
                const legacy = Number(data.deployModeStatus ?? this.deployModeStatus);
                this.deployModeStatus = legacy;
                // 同步到按队状态（保持兼容）
                this.deployStatusByTeam = { red: legacy, blue: legacy };
            }
            this.robotIds = data.robotIds || this.robotIds;
            this.globalSpecialMechanism = data.globalSpecialMechanism || this.globalSpecialMechanism;
            this.robotInjuryStats = data.robotInjuryStats || this.robotInjuryStats;
            if (!this.isEditingRobotInjuryForm) {
                this.syncRobotInjuryFormFromState();
            }
            this.robotPerformanceSelectionSync = data.robotPerformanceSelectionSync || this.robotPerformanceSelectionSync;
            if (!this.isEditingRobotPerformanceForm) {
                this.robotPerformanceForm.shooter = Number(this.robotPerformanceSelectionSync.shooter ?? this.robotPerformanceForm.shooter);
                this.robotPerformanceForm.chassis = Number(this.robotPerformanceSelectionSync.chassis ?? this.robotPerformanceForm.chassis);
                this.robotPerformanceForm.sentry_control = Number(this.robotPerformanceSelectionSync.sentry_control ?? this.robotPerformanceForm.sentry_control);
            }
            this.robotStaticStatus = data.robotStaticStatus || this.robotStaticStatus;
            if (this.robotStaticStatusDirty) {
                if (this.isRobotStaticStatusFormEqual(this.robotStaticStatus)) {
                    this.robotStaticStatusDirty = false;
                }
            } else {
                this.syncRobotStaticStatusFormFromState(this.robotStaticStatus);
            }
            this.robotDynamicStatus = data.robotDynamicStatus || this.robotDynamicStatus;
            if (this.robotDynamicStatusDirty) {
                if (this.isRobotDynamicStatusFormEqual(this.robotDynamicStatus)) {
                    this.robotDynamicStatusDirty = false;
                }
            } else {
                this.syncRobotDynamicStatusFormFromState(this.robotDynamicStatus);
            }
            this.robotModuleStatus = data.robotModuleStatus || this.robotModuleStatus;
            if (this.robotModuleStatusDirty) {
                if (this.isRobotModuleStatusFormEqual(this.robotModuleStatus)) {
                    this.robotModuleStatusDirty = false;
                }
            } else {
                this.syncRobotModuleStatusFormFromState(this.robotModuleStatus);
            }
            this.sentryStatusSync = data.sentryStatusSync || this.sentryStatusSync;
            if (!this.isEditingSentryStatusForm) {
                this.sentryStatusForm.posture_id = Number(this.sentryStatusSync.posture_id ?? this.sentryStatusForm.posture_id);
                const sentryMode = this.sentryStatusSync.is_powered ? 2 : (this.sentryStatusSync.is_weakened ? 1 : 0);
                this.sentryStatusForm.is_weakened = Number(sentryMode);
                this.sentryStatusForm.is_powered = sentryMode === 2 ? 1 : 0;
            }
            this.lastSentryCtrlResult = data.lastSentryCtrlResult || this.lastSentryCtrlResult;
            if (!this.isEditingSentryCtrlResultForm) {
                this.sentryCtrlResultForm.command_id = Number(this.lastSentryCtrlResult.command_id ?? this.sentryCtrlResultForm.command_id);
                this.sentryCtrlResultForm.result_code = Number(this.lastSentryCtrlResult.result_code ?? this.sentryCtrlResultForm.result_code);
            }
            this.robotPathPlanInfo = data.robotPathPlanInfo || this.robotPathPlanInfo;
            if (this.robotPathPlanDirty) {
                if (this.isRobotPathPlanStateEqual(this.robotPathPlanInfo)) {
                    this.robotPathPlanDirty = false;
                    if (!this.isEditingRobotPathPlanForm) {
                        this.syncRobotPathPlanEditorFromInfo(this.robotPathPlanInfo);
                    }
                }
            } else if (!this.isEditingRobotPathPlanForm) {
                this.syncRobotPathPlanEditorFromInfo(this.robotPathPlanInfo);
            }
            this.dartStatus = data.dartStatus || this.dartStatus;
            this.dartStatusByTeam = data.dartStatusByTeam || this.dartStatusByTeam;
            if (!this.isEditingEventCommand && data.lastEventCommand) {
                this.eventCommand.event_id = Number(data.lastEventCommand.event_id ?? this.eventCommand.event_id);
                this.eventCommand.param = String(data.lastEventCommand.param ?? this.eventCommand.param);
            }
            if (!this.isEditingCustomByteBlockForm && data.customByteBlockForm) {
                this.customByteBlockForm = Object.assign(
                    {},
                    this.customByteBlockForm,
                    data.customByteBlockForm
                );
            }
            if (!this.isEditingBuffCommand && data.lastBuffStatus) {
                this.buffCommand = Object.assign(
                    {},
                    this.buffCommand,
                    data.lastBuffStatus
                );
            }
            this.customByteBlockHex = data.customByteBlockHex ?? this.customByteBlockHex;
            this.lastGameResult = data.lastGameResult || this.lastGameResult;
            this.positions = this.mergePositionsPreservingEdits(data.positions);
            if (data.videos) {
                this.videos = data.videos;
                if (!this.selectedVideo && this.videos.length > 0) {
                    this.selectedVideo = this.videos[0];
                }
                if (!this.selectedIndustrialVideo && this.videos.length > 0) {
                    this.selectedIndustrialVideo = this.videos[0];
                }
            }
            if (data.videoStatus) {
                this.videoStatus = data.videoStatus;
            }
            if (data.industrialCameraStatus) {
                this.industrialCameraStatus = data.industrialCameraStatus;
            }
            this.drawMinimap();
            if (Array.isArray(data.logs)) {
                this.logs = [...data.logs].reverse();
            }
            this.$nextTick(() => {
                this.drawMinimap();
                this.drawRobotPathPlanCanvas();
            });
        });

        socket.on('simulation_status', (data) => {
            this.applySimulationPayload(data, false);
        });

        socket.on('simulation_frame', (data) => {
            this.applySimulationPayload(data, true);
        });
    },
    beforeDestroy() {
        if (this.windowResizeHandler) {
            window.removeEventListener('resize', this.windowResizeHandler);
        }
        if (this.fullscreenChangeHandler) {
            document.removeEventListener('fullscreenchange', this.fullscreenChangeHandler);
            document.removeEventListener('webkitfullscreenchange', this.fullscreenChangeHandler);
        }
    },
    methods: {
        // 当前机器人改变时触发
        onCurrentRobotChanged() {
            // 保存到 localStorage
            localStorage.setItem('currentRobotId', this.currentRobotId.toString());
            // 发送给服务器
            this.sendCommand('set_current_robot', { robot_id: this.currentRobotId });
        },
        sendCommand(cmd, data = {}) {
            socket.emit('referee_command', { command: cmd, ...data });
        },
        sendSimulationControl(action) {
            if (!this.connected) {
                this.simulationUiMessage = '仿真服务未连接，无法发送赛事控制指令';
                return;
            }

            const normalizedSpeed = Number(this.simulationConfig.speed) === 2 ? 2 : 1;
            const payload = {
                action,
                mode: this.simulationConfig.mode === 'full' ? 'full' : 'quick',
                speed: normalizedSpeed,
                features: {
                    positions: Boolean(this.simulationConfig.features.positions),
                    hp: Boolean(this.simulationConfig.features.hp),
                    events: Boolean(this.simulationConfig.features.events)
                }
            };

            if (action === 'start') {
                const duration = payload.mode === 'full' ? 420 : 90;
                this.simulationManualOverride = false;
                this.simulationUiMessage = '正在启动赛事演示…';
                this.simulationStatus = Object.assign({}, this.simulationStatus, {
                    state: 'starting',
                    mode: payload.mode,
                    speed: payload.speed,
                    elapsed: 0,
                    remaining: duration,
                    duration,
                    recentEvents: []
                });
            } else if (action === 'pause') {
                this.simulationUiMessage = '正在暂停赛事演示…';
                this.simulationStatus = Object.assign({}, this.simulationStatus, { state: 'pausing' });
            } else if (action === 'resume') {
                this.simulationManualOverride = false;
                this.simulationUiMessage = '正在继续赛事演示…';
                this.simulationStatus = Object.assign({}, this.simulationStatus, { state: 'resuming' });
            } else if (action === 'stop') {
                this.simulationManualOverride = false;
                this.simulationUiMessage = '正在停止并复位赛事演示…';
                this.simulationStatus = Object.assign({}, this.simulationStatus, { state: 'stopping' });
            } else if (action === 'configure') {
                this.simulationUiMessage = '赛事演示配置已更新';
            }

            socket.emit('simulation_control', payload);
        },
        onSimulationConfigChanged() {
            this.simulationConfig.speed = Number(this.simulationConfig.speed) === 2 ? 2 : 1;
            if (this.simulationIsActive) {
                this.sendSimulationControl('configure');
                return;
            }

            const duration = this.simulationConfig.mode === 'full' ? 420 : 90;
            this.simulationStatus = Object.assign({}, this.simulationStatus, {
                mode: this.simulationConfig.mode,
                speed: this.simulationConfig.speed,
                elapsed: 0,
                remaining: duration,
                duration
            });
        },
        normalizeSimulationState(value, source = {}) {
            if (source.paused === true || source.is_paused === true) {
                return 'paused';
            }
            if ((source.running === true || source.is_running === true) && source.paused !== true) {
                return 'running';
            }

            const numericStates = {
                0: 'idle',
                1: 'running',
                2: 'paused',
                3: 'completed'
            };
            if (typeof value === 'number' && numericStates[value]) {
                return numericStates[value];
            }

            const normalized = String(value ?? '').trim().toLowerCase();
            const aliases = {
                idle: 'idle',
                stopped: 'idle',
                reset: 'idle',
                ready: 'idle',
                start: 'running',
                started: 'running',
                active: 'running',
                playing: 'running',
                run: 'running',
                running: 'running',
                pause: 'paused',
                paused: 'paused',
                complete: 'completed',
                completed: 'completed',
                finish: 'completed',
                finished: 'completed',
                ended: 'completed',
                starting: 'starting',
                pausing: 'pausing',
                resuming: 'resuming',
                stopping: 'stopping'
            };
            return aliases[normalized] || this.simulationStatus.state || 'idle';
        },
        applySimulationPayload(payload, isFrame) {
            if (!payload || typeof payload !== 'object') {
                return;
            }

            const nestedStatus = payload.status && typeof payload.status === 'object'
                ? payload.status
                : {};
            const source = Object.assign({}, payload, nestedStatus);
            const pick = (...values) => values.find(value => value !== undefined && value !== null);
            const fallbackDuration = (pick(source.mode, this.simulationConfig.mode) === 'full') ? 420 : 90;
            const duration = Number(pick(
                source.duration,
                source.duration_sec,
                source.total,
                source.total_sec,
                this.simulationStatus.duration,
                fallbackDuration
            ));
            const elapsed = Number(pick(
                source.elapsed,
                source.elapsed_sec,
                source.elapsed_seconds,
                this.simulationStatus.elapsed,
                0
            ));
            const remaining = Number(pick(
                source.remaining,
                source.remaining_sec,
                source.remaining_seconds,
                source.time_left,
                Number.isFinite(duration) && Number.isFinite(elapsed) ? duration - elapsed : undefined,
                this.simulationStatus.remaining,
                fallbackDuration
            ));
            const stateValue = typeof payload.status === 'string'
                ? payload.status
                : pick(source.state, source.phase, source.engine_state, this.simulationStatus.state);
            const state = this.normalizeSimulationState(stateValue, source);
            const mode = pick(source.mode, source.simulation_mode, this.simulationStatus.mode, this.simulationConfig.mode) === 'full'
                ? 'full'
                : 'quick';
            const speed = Number(pick(source.speed, source.playback_speed, this.simulationStatus.speed, 1)) === 2 ? 2 : 1;
            const message = String(pick(source.message, source.detail, source.reason, source.error, '') || '');
            const pauseReason = String(pick(source.pause_reason, source.pauseReason, '') || '');
            const rawEvents = pick(source.recent_events, source.recentEvents, source.events);
            const recentEvents = Array.isArray(rawEvents)
                ? rawEvents.slice(-10).map((event, index) => this.normalizeSimulationEvent(event, index))
                : this.simulationStatus.recentEvents;

            this.simulationStatus = Object.assign({}, this.simulationStatus, {
                state,
                mode,
                speed,
                elapsed: Number.isFinite(elapsed) ? Math.max(0, elapsed) : 0,
                remaining: Number.isFinite(remaining) ? Math.max(0, remaining) : 0,
                duration: Number.isFinite(duration) && duration > 0 ? duration : fallbackDuration,
                message,
                recentEvents,
                lastFrameAt: isFrame ? new Date().toISOString() : this.simulationStatus.lastFrameAt
            });

            if (source.features && typeof source.features === 'object') {
                this.simulationConfig.features = Object.assign({}, this.simulationConfig.features, source.features);
            }
            if (source.mode !== undefined || source.simulation_mode !== undefined) {
                this.simulationConfig.mode = mode;
            }
            if (source.speed !== undefined || source.playback_speed !== undefined) {
                this.simulationConfig.speed = speed;
            }

            this.applySimulationRobotFrame(source);
            if (state === 'running' || state === 'paused' || state === 'completed' || state === 'idle') {
                this.simulationUiMessage = '';
            }
            if (state === 'running') {
                this.simulationManualOverride = false;
            }
            if (state === 'paused' && ['manual_position', 'manual_drag', 'position_override'].includes(pauseReason)) {
                this.simulationManualOverride = true;
            }
            if (state === 'idle' && Number(this.simulationStatus.elapsed) === 0) {
                this.simulationManualOverride = false;
            }
            this.$nextTick(() => this.drawMinimap());
        },
        normalizeSimulationEvent(event, index) {
            if (typeof event === 'string' || typeof event === 'number') {
                return {
                    key: `event-${index}-${String(event)}`,
                    message: String(event),
                    timeLabel: ''
                };
            }

            const value = event && typeof event === 'object' ? event : {};
            const message = value.message
                ?? value.text
                ?? value.title
                ?? value.label
                ?? value.description
                ?? value.name
                ?? value.event
                ?? (value.event_id !== undefined ? `Event ${value.event_id}` : '赛事状态更新');
            const seconds = Number(value.elapsed ?? value.elapsed_sec ?? value.at_sec ?? value.time_sec);
            const timestamp = value.timestamp ?? value.time;
            let timeLabel = '';
            if (Number.isFinite(seconds)) {
                timeLabel = `${this.formatSimulationTime(seconds)} `;
            } else if (timestamp) {
                const date = new Date(timestamp);
                if (!Number.isNaN(date.getTime())) {
                    timeLabel = `${date.toLocaleTimeString()} `;
                }
            }
            return {
                key: String(value.key ?? value.id ?? value.event_id ?? `event-${index}-${message}`),
                message: String(message),
                timeLabel
            };
        },
        applySimulationRobotFrame(source) {
            if (Array.isArray(source.positions)) {
                this.positions = this.mergePositionsPreservingEdits(source.positions);
            }

            let robots = source.robots ?? source.robot_states ?? source.robotStates;
            if (!robots || (typeof robots !== 'object' && !Array.isArray(robots))) {
                return;
            }

            let entries;
            if (Array.isArray(robots)) {
                entries = robots.map((robot, index) => ({ key: index, robot }));
            } else {
                entries = Object.entries(robots).map(([key, robot]) => ({ key, robot }));
            }

            const nextPositions = this.positions.map(position => Object.assign({}, position));
            const nextHealth = Array.isArray(this.globalUnitStatus.robot_health)
                ? this.globalUnitStatus.robot_health.slice()
                : Array(this.robotIds.length).fill(0);
            const nextMaximum = Array.isArray(this.globalUnitStatus.robot_max_hp)
                ? this.globalUnitStatus.robot_max_hp.slice()
                : Array(this.robotIds.length).fill(600);
            let changedPosition = false;
            let changedHealth = false;

            entries.forEach(({ key, robot }, fallbackIndex) => {
                if (!robot || typeof robot !== 'object') {
                    return;
                }

                const rawRobotId = robot.robot_id ?? robot.robotId ?? robot.id ?? key;
                const robotId = Number(rawRobotId);
                let index = this.robotIds.findIndex(id => Number(id) === robotId);
                if (index < 0 && Number.isInteger(Number(robot.index))) {
                    index = Number(robot.index);
                }
                if (index < 0 && Array.isArray(robots)) {
                    index = fallbackIndex;
                }
                if (index < 0 || index >= this.robotIds.length) {
                    return;
                }

                const pose = robot.position && typeof robot.position === 'object'
                    ? robot.position
                    : (robot.pose && typeof robot.pose === 'object' ? robot.pose : robot);
                const x = Number(pose.x ?? pose.position_x ?? pose.pos_x);
                const y = Number(pose.y ?? pose.position_y ?? pose.pos_y);
                const angle = Number(pose.angle ?? pose.yaw ?? pose.heading);
                if (Number.isFinite(x) && Number.isFinite(y)) {
                    nextPositions[index] = Object.assign({}, nextPositions[index], {
                        x,
                        y,
                        angle: Number.isFinite(angle) ? angle : Number(nextPositions[index].angle || 0)
                    });
                    changedPosition = true;
                }

                const hp = Number(robot.hp ?? robot.health ?? robot.current_hp ?? robot.current_health);
                const maxHp = Number(robot.max_hp ?? robot.maximum_hp ?? robot.max_health);
                if (Number.isFinite(hp)) {
                    nextHealth[index] = Math.max(0, hp);
                    changedHealth = true;
                }
                if (Number.isFinite(maxHp) && maxHp > 0) {
                    nextMaximum[index] = maxHp;
                    changedHealth = true;
                }
            });

            if (changedPosition) {
                this.positions = nextPositions;
            }
            if (changedHealth) {
                this.globalUnitStatus = Object.assign({}, this.globalUnitStatus, {
                    robot_health: nextHealth,
                    robot_max_hp: nextMaximum
                });
            }
        },
        formatSimulationTime(value) {
            const seconds = Math.max(0, Math.round(Number(value) || 0));
            const minutes = Math.floor(seconds / 60);
            return `${String(minutes).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
        },
        formatSimulationTimestamp(value) {
            const date = new Date(value);
            return Number.isNaN(date.getTime()) ? '' : date.toLocaleTimeString();
        },
        buildTeamHpSummary(team) {
            const robots = this.getTeamRobotIds(team).map((id) => {
                const index = this.robotIds.findIndex(robotId => Number(robotId) === Number(id));
                const current = index >= 0 ? Number(this.globalUnitStatus.robot_health[index] ?? 0) : 0;
                const maximum = index >= 0 ? Math.max(1, Number(this.globalUnitStatus.robot_max_hp[index] ?? 600)) : 600;
                return {
                    id,
                    current: Math.max(0, Math.round(current)),
                    maximum: Math.round(maximum),
                    percent: Math.max(0, Math.min(100, current / maximum * 100))
                };
            });
            const current = robots.reduce((sum, robot) => sum + robot.current, 0);
            const maximum = robots.reduce((sum, robot) => sum + robot.maximum, 0);
            return {
                robots,
                current,
                maximum,
                percent: maximum > 0 ? Math.max(0, Math.min(100, current / maximum * 100)) : 0
            };
        },
        async toggleMapFullscreen() {
            const stage = document.getElementById('simulationStage');
            if (!stage) {
                return;
            }

            try {
                const fullscreenElement = document.fullscreenElement || document.webkitFullscreenElement;
                if (fullscreenElement === stage) {
                    if (document.exitFullscreen) {
                        await document.exitFullscreen();
                    } else if (document.webkitExitFullscreen) {
                        document.webkitExitFullscreen();
                    }
                } else if (stage.requestFullscreen) {
                    await stage.requestFullscreen();
                } else if (stage.webkitRequestFullscreen) {
                    stage.webkitRequestFullscreen();
                } else {
                    this.simulationUiMessage = '当前浏览器不支持地图全屏功能';
                }
            } catch (error) {
                this.simulationUiMessage = `无法切换全屏：${error && error.message ? error.message : '浏览器拒绝了请求'}`;
            }
        },
        startEditRobotDetail(index) {
            this.$set(this.editingRobotDetailIndices, index, true);
        },
        finishEditRobotDetail(index) {
            this.$set(this.editingRobotDetailIndices, index, false);
        },
        startEditRobotPose(index) {
            this.$set(this.editingRobotPoseIndices, index, true);
        },
        finishEditRobotPose(index) {
            this.$set(this.editingRobotPoseIndices, index, false);
        },
        mergeGlobalUnitStatusPreservingEdits(incomingStatus = {}) {
            const merged = Object.assign({}, this.globalUnitStatus, incomingStatus);
            const detailKeys = ['robot_bullets', 'robot_fire_rate', 'robot_level', 'robot_heat', 'robot_power'];

            detailKeys.forEach((key) => {
                if (!Array.isArray(merged[key])) {
                    return;
                }
                merged[key] = merged[key].slice();
                this.editingRobotDetailIndices.forEach((isEditing, index) => {
                    if (isEditing && Array.isArray(this.globalUnitStatus[key])) {
                        merged[key][index] = this.globalUnitStatus[key][index];
                    }
                });
            });

            return merged;
        },
        mergePositionsPreservingEdits(incomingPositions) {
            if (!Array.isArray(incomingPositions)) {
                return this.positions;
            }

            return incomingPositions.map((incomingPose, index) => {
                const currentPose = this.positions[index] || { x: 0, y: 0, angle: 0 };
                if (this.editingRobotPoseIndices[index]) {
                    return Object.assign({}, incomingPose, {
                        angle: currentPose.angle
                    });
                }
                return incomingPose;
            });
        },
        parseNumberList(text) {
            return String(text || '')
                .split(',')
                .map(value => Number(String(value).trim()))
                .filter(value => Number.isFinite(value))
                .map(value => Math.trunc(value));
        },
        formatNumberList(values) {
            return Array.isArray(values) ? values.join(', ') : '';
        },
        getRobotLabel(robotId) {
            const normalized = Number(robotId);
            if (normalized >= 100) {
                return `蓝${normalized - 100}`;
            }
            return `红${normalized}`;
        },
        getTeamRobotIds(team) {
            return team === 'blue'
                ? [101, 103, 106, 107]
                : [1, 3, 6, 7];
        },
        getTeamRobotCards(team) {
            return this.getTeamRobotIds(team)
                .map((robotId) => ({
                    robotId,
                    index: this.robotIds.indexOf(robotId)
                }))
                .filter((card) => card.index >= 0);
        },
        getRobotMaxHp(index) {
            const values = this.globalUnitStatus.robot_max_hp || [];
            return Number(values[index] ?? 600);
        },
        getBaseHp(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const key = `${teamName}_base_health`;
            return this.globalUnitStatus[key] ?? this.globalUnitStatus.base_health ?? 0;
        },
        getBaseStatus(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const key = `${teamName}_base_status`;
            return Number(this.globalUnitStatus[key] ?? this.globalUnitStatus.base_status ?? 0);
        },
        getBaseShield(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const key = `${teamName}_base_shield`;
            return Number(this.globalUnitStatus[key] ?? this.globalUnitStatus.base_shield ?? 0);
        },
        getOutpostHp(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const key = `${teamName}_outpost_health`;
            return this.globalUnitStatus[key] ?? this.globalUnitStatus.outpost_health ?? 0;
        },
        getOutpostStatus(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const key = `${teamName}_outpost_status`;
            return Number(this.globalUnitStatus[key] ?? this.globalUnitStatus.outpost_status ?? 0);
        },
        getRuneInfo(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            return this.refereeInfoByTeam[teamName] || this.refereeInfo;
        },
        getDartStatus(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            return this.dartStatusByTeam[teamName] || this.dartStatus;
        },
        getDartTargetName(targetId) {
            const labels = {
                1: '目标 1',
                2: '目标 2',
                3: '目标 3',
                4: '目标 4',
                5: '目标 5'
            };
            return labels[Number(targetId)] || `目标 ${Number(targetId) || 0}`;
        },
        getBuffTypeName(buffType) {
            const option = this.buffPresetOptions.find((item) => Number(item.buff_type) === Number(buffType));
            return option ? option.label : `类型 ${Number(buffType) || 0}`;
        },
        getSentryPostureName(postureId) {
            const option = this.sentryPostureOptions.find((item) => Number(item.value) === Number(postureId));
            return option ? option.label.replace(/^\d+:\s*/, '') : `姿态 ${Number(postureId) || 0}`;
        },
        getSentryCommandName(commandId) {
            const option = this.sentryCommandOptions.find((item) => Number(item.value) === Number(commandId));
            return option ? option.label : `指令 ${Number(commandId) || 0}`;
        },
        getRobotPerformanceShooterName(value) {
            const option = this.robotPerformanceShooterOptions.find((item) => Number(item.value) === Number(value));
            return option ? option.label.replace(/^\d+:\s*/, '') : `射手 ${Number(value) || 0}`;
        },
        getRobotPerformanceChassisName(value) {
            const option = this.robotPerformanceChassisOptions.find((item) => Number(item.value) === Number(value));
            return option ? option.label.replace(/^\d+:\s*/, '') : `底盘 ${Number(value) || 0}`;
        },
        getRobotPerformanceSentryControlName(value) {
            const option = this.robotPerformanceSentryControlOptions.find((item) => Number(item.value) === Number(value));
            return option ? option.label.replace(/^\d+:\s*/, '') : `控制 ${Number(value) || 0}`;
        },
        syncRuneMetricsDraft() {
            const selectedRuneInfo = this.getRuneInfo(this.runeTeam);
            this.isEditingRuneMetrics = false;
            this.runeInputs.activated_arms = selectedRuneInfo.activated_arms ?? 0;
            this.runeInputs.average_rings = selectedRuneInfo.average_rings ?? 0;
        },
        getRobotInjuryStat(robotId) {
            const key = String(robotId);
            return this.robotInjuryStats[key] || this.robotInjuryStats[robotId] || {};
        },
        toNonNegativeInt(value) {
            const normalized = Number(value);
            if (!Number.isFinite(normalized)) {
                return 0;
            }
            return Math.max(0, Math.trunc(normalized));
        },
        toStatusFloat(value, fallback = 0) {
            const normalized = Number(value);
            if (!Number.isFinite(normalized)) {
                return Number(fallback) || 0;
            }
            return normalized;
        },
        toStatusBool(value) {
            return Number(value) === 1;
        },
        syncRobotStaticStatusFormFromState(state = {}) {
            this.isSyncingRobotStaticStatusForm = true;
            this.robotStaticStatusForm = Object.assign({}, this.robotStaticStatusForm, {
                connection_state: Number(state.connection_state ?? 1),
                field_state: Number(state.field_state ?? 0),
                alive_state: Number(state.alive_state ?? 1),
                robot_id: Number(state.robot_id ?? this.currentRobotId ?? 1),
                robot_type: Number(state.robot_type ?? ((this.currentRobotId || 1) % 100)),
                performance_system_shooter: Number(state.performance_system_shooter ?? 1),
                performance_system_chassis: Number(state.performance_system_chassis ?? 1),
                level: Number(state.level ?? 1),
                max_health: Number(state.max_health ?? 600),
                max_heat: Number(state.max_heat ?? 240),
                heat_cooldown_rate: this.toStatusFloat(state.heat_cooldown_rate ?? 20, 20),
                max_power: Number(state.max_power ?? 120),
                max_buffer_energy: Number(state.max_buffer_energy ?? 60),
                max_chassis_energy: Number(state.max_chassis_energy ?? 120)
            });
            this.isSyncingRobotStaticStatusForm = false;
        },
        syncRobotDynamicStatusFormFromState(state = {}) {
            this.isSyncingRobotDynamicStatusForm = true;
            this.robotDynamicStatusForm = Object.assign({}, this.robotDynamicStatusForm, {
                current_health: Number(state.current_health ?? 600),
                current_heat: this.toStatusFloat(state.current_heat ?? 0, 0),
                last_projectile_fire_rate: this.toStatusFloat(state.last_projectile_fire_rate ?? 0, 0),
                current_chassis_energy: Number(state.current_chassis_energy ?? 0),
                current_buffer_energy: Number(state.current_buffer_energy ?? 0),
                current_experience: Number(state.current_experience ?? 100),
                experience_for_upgrade: Number(state.experience_for_upgrade ?? 1000),
                total_projectiles_fired: Number(state.total_projectiles_fired ?? 0),
                remaining_ammo: Number(state.remaining_ammo ?? 0),
                is_out_of_combat: state.is_out_of_combat ? 1 : 0,
                out_of_combat_countdown: Number(state.out_of_combat_countdown ?? 0),
                can_remote_heal: state.can_remote_heal ? 1 : 0,
                can_remote_ammo: state.can_remote_ammo ? 1 : 0
            });
            this.isSyncingRobotDynamicStatusForm = false;
        },
        syncRobotModuleStatusFormFromState(state = {}) {
            this.isSyncingRobotModuleStatusForm = true;
            this.robotModuleStatusForm = Object.assign({}, this.robotModuleStatusForm, {
                power_manager: Number(state.power_manager ?? 1),
                rfid: Number(state.rfid ?? 1),
                light_strip: Number(state.light_strip ?? 1),
                small_shooter: Number(state.small_shooter ?? 1),
                big_shooter: Number(state.big_shooter ?? 1),
                uwb: Number(state.uwb ?? 1),
                armor: Number(state.armor ?? 1),
                video_transmission: Number(state.video_transmission ?? 1),
                capacitor: Number(state.capacitor ?? 1),
                main_controller: Number(state.main_controller ?? 1),
                laser_detection_module: Number(state.laser_detection_module ?? 1)
            });
            this.isSyncingRobotModuleStatusForm = false;
        },
        isRobotStaticStatusFormEqual(state = {}) {
            return Number(state.connection_state ?? 1) === Number(this.robotStaticStatusForm.connection_state)
                && Number(state.field_state ?? 0) === Number(this.robotStaticStatusForm.field_state)
                && Number(state.alive_state ?? 1) === Number(this.robotStaticStatusForm.alive_state)
                && Number(state.robot_id ?? this.currentRobotId ?? 1) === Number(this.robotStaticStatusForm.robot_id)
                && Number(state.robot_type ?? ((this.currentRobotId || 1) % 100)) === Number(this.robotStaticStatusForm.robot_type)
                && Number(state.performance_system_shooter ?? 1) === Number(this.robotStaticStatusForm.performance_system_shooter)
                && Number(state.performance_system_chassis ?? 1) === Number(this.robotStaticStatusForm.performance_system_chassis)
                && Number(state.level ?? 1) === Number(this.robotStaticStatusForm.level)
                && Number(state.max_health ?? 600) === Number(this.robotStaticStatusForm.max_health)
                && Number(state.max_heat ?? 240) === Number(this.robotStaticStatusForm.max_heat)
                && this.toStatusFloat(state.heat_cooldown_rate ?? 20, 20) === this.toStatusFloat(this.robotStaticStatusForm.heat_cooldown_rate, 20)
                && Number(state.max_power ?? 120) === Number(this.robotStaticStatusForm.max_power)
                && Number(state.max_buffer_energy ?? 60) === Number(this.robotStaticStatusForm.max_buffer_energy)
                && Number(state.max_chassis_energy ?? 120) === Number(this.robotStaticStatusForm.max_chassis_energy);
        },
        isRobotDynamicStatusFormEqual(state = {}) {
            return Number(state.current_health ?? 600) === Number(this.robotDynamicStatusForm.current_health)
                && this.toStatusFloat(state.current_heat ?? 0, 0) === this.toStatusFloat(this.robotDynamicStatusForm.current_heat, 0)
                && this.toStatusFloat(state.last_projectile_fire_rate ?? 0, 0) === this.toStatusFloat(this.robotDynamicStatusForm.last_projectile_fire_rate, 0)
                && Number(state.current_chassis_energy ?? 0) === Number(this.robotDynamicStatusForm.current_chassis_energy)
                && Number(state.current_buffer_energy ?? 0) === Number(this.robotDynamicStatusForm.current_buffer_energy)
                && Number(state.current_experience ?? 100) === Number(this.robotDynamicStatusForm.current_experience)
                && Number(state.experience_for_upgrade ?? 1000) === Number(this.robotDynamicStatusForm.experience_for_upgrade)
                && Number(state.total_projectiles_fired ?? 0) === Number(this.robotDynamicStatusForm.total_projectiles_fired)
                && Number(state.remaining_ammo ?? 0) === Number(this.robotDynamicStatusForm.remaining_ammo)
                && Number(state.is_out_of_combat ? 1 : 0) === Number(this.robotDynamicStatusForm.is_out_of_combat)
                && Number(state.out_of_combat_countdown ?? 0) === Number(this.robotDynamicStatusForm.out_of_combat_countdown)
                && Number(state.can_remote_heal ? 1 : 0) === Number(this.robotDynamicStatusForm.can_remote_heal)
                && Number(state.can_remote_ammo ? 1 : 0) === Number(this.robotDynamicStatusForm.can_remote_ammo);
        },
        isRobotModuleStatusFormEqual(state = {}) {
            return Number(state.power_manager ?? 1) === Number(this.robotModuleStatusForm.power_manager)
                && Number(state.rfid ?? 1) === Number(this.robotModuleStatusForm.rfid)
                && Number(state.light_strip ?? 1) === Number(this.robotModuleStatusForm.light_strip)
                && Number(state.small_shooter ?? 1) === Number(this.robotModuleStatusForm.small_shooter)
                && Number(state.big_shooter ?? 1) === Number(this.robotModuleStatusForm.big_shooter)
                && Number(state.uwb ?? 1) === Number(this.robotModuleStatusForm.uwb)
                && Number(state.armor ?? 1) === Number(this.robotModuleStatusForm.armor)
                && Number(state.video_transmission ?? 1) === Number(this.robotModuleStatusForm.video_transmission)
                && Number(state.capacitor ?? 1) === Number(this.robotModuleStatusForm.capacitor)
                && Number(state.main_controller ?? 1) === Number(this.robotModuleStatusForm.main_controller)
                && Number(state.laser_detection_module ?? 1) === Number(this.robotModuleStatusForm.laser_detection_module);
        },
        normalizeRobotInjuryForm() {
            if (this.isNormalizingRobotInjuryForm) {
                return;
            }
            this.isNormalizingRobotInjuryForm = true;
            const nextForm = Object.assign({}, this.robotInjuryForm);
            const numericKeys = [
                'robot_id',
                'total_damage',
                'collision_damage',
                'small_projectile_damage',
                'large_projectile_damage',
                'dart_splash_damage',
                'module_offline_damage',
                'offline_damage',
                'penalty_damage',
                'server_kill_damage',
                'killer_id'
            ];
            numericKeys.forEach((key) => {
                nextForm[key] = this.toNonNegativeInt(nextForm[key]);
            });
            const nonSmallKeys = [
                'collision_damage',
                'large_projectile_damage',
                'dart_splash_damage',
                'module_offline_damage',
                'offline_damage',
                'penalty_damage',
                'server_kill_damage'
            ];
            const nonSmallSum = nonSmallKeys.reduce((sum, key) => sum + Number(nextForm[key] || 0), 0);
            let totalDamage = Number(nextForm.total_damage || 0);
            let smallProjectileDamage = totalDamage - nonSmallSum;
            if (smallProjectileDamage < 0) {
                totalDamage = nonSmallSum;
                smallProjectileDamage = 0;
            }
            nextForm.total_damage = totalDamage;
            nextForm.small_projectile_damage = smallProjectileDamage;
            this.robotInjuryForm = Object.assign({}, this.robotInjuryForm, nextForm);
            this.isNormalizingRobotInjuryForm = false;
        },
        syncRobotInjuryFormFromState() {
            const stats = this.getRobotInjuryStat(this.robotInjuryForm.robot_id);
            this.isNormalizingRobotInjuryForm = true;
            this.robotInjuryForm = Object.assign({}, this.robotInjuryForm, {
                total_damage: Number(stats.total_damage ?? 0),
                collision_damage: Number(stats.collision_damage ?? 0),
                small_projectile_damage: Number(stats.small_projectile_damage ?? 0),
                large_projectile_damage: Number(stats.large_projectile_damage ?? 0),
                dart_splash_damage: Number(stats.dart_splash_damage ?? 0),
                module_offline_damage: Number(stats.module_offline_damage ?? 0),
                offline_damage: Number(stats.offline_damage ?? 0),
                penalty_damage: Number(stats.penalty_damage ?? 0),
                server_kill_damage: Number(stats.server_kill_damage ?? 0),
                killer_id: Number(stats.killer_id ?? 0)
            });
            this.isNormalizingRobotInjuryForm = false;
            this.normalizeRobotInjuryForm();
        },
        activateRune() {
            this.sendCommand('activate_rune', {
                team: this.runeTeam,
                rune_type: Number(this.runeType)
            });
        },
        finishactivateRune() {
            this.sendCommand('finish_activate_rune', {
                team: this.runeTeam,
                rune_type: Number(this.runeType)
            });
        },
        resetactivateRune() {
            this.sendCommand('reset_activate_rune', {
                team: this.runeTeam,
                rune_type: Number(this.runeType)
            });
        },
        startAirSupport(team, mode = 'free') {
            const teamName = team === 'blue' ? 'blue' : 'red';
            this.sendCommand('start_air_support', {
                team,
                mode,
                duration_sec: Number(this.airSupportDraft[`${teamName}_default_time`] ?? 30)
            });
        },
        stopAirSupport(team) {
            this.sendCommand('stop_air_support', { team });
        },
        resetAirSupportCost(team) {
            this.sendCommand('reset_air_support_cost', { team });
        },
        setAirSupportStatusSync(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            this.sendCommand('set_air_support_status_sync', {
                team: teamName,
                airsupport_status: Number(this.airSupportDraft[`${teamName}_status`] ?? 0),
                left_time: Number(this.airSupportDraft[`${teamName}_left_time`] ?? 0),
                cost_coins: Number(this.airSupportDraft[`${teamName}_cost_coins`] ?? 0),
                is_being_targeted: Number(this.airSupportDraft[`${teamName}_is_being_targeted`] ?? 0),
                shooter_status: Number(this.airSupportDraft[`${teamName}_shooter_status`] ?? 1)
            });
        },
        syncAirSupportDraft(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            this.airSupportDraft[`${teamName}_status`] = Number(this.airSupport[`${teamName}_status`] ?? 0);
            this.airSupportDraft[`${teamName}_left_time`] = Number(this.airSupport[`${teamName}_left_time`] ?? 0);
            this.airSupportDraft[`${teamName}_default_time`] = Number(this.airSupport[`${teamName}_default_time`] ?? 30);
            this.airSupportDraft[`${teamName}_cost_coins`] = Number(this.airSupport[`${teamName}_cost_coins`] ?? 0);
            this.airSupportDraft[`${teamName}_is_being_targeted`] = Number(this.airSupport[`${teamName}_is_being_targeted`] ?? 0);
            this.airSupportDraft[`${teamName}_shooter_status`] = Number(this.airSupport[`${teamName}_shooter_status`] ?? 1);
        },
        triggerAirSupportCountered(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            const isCountered = Number(this.airSupport[`${teamName}_shooter_status`] ?? 1) === 2;
            this.sendCommand('trigger_air_support_countered', { team });
            this.airSupport[`${teamName}_shooter_status`] = isCountered ? 1 : 2;
            this.airSupport[`${teamName}_is_being_targeted`] = isCountered ? 0 : 1;
        },
        setDartGateStatus(openState = 2, targetId = 2) {
            // 先在本地保存对应队伍的状态，避免 UI 与 state 不一致
            const teamName = this.dartGateTeam === 'blue' ? 'blue' : 'red';
            const next = Object.assign({}, this.dartStatusByTeam[teamName] || this.dartStatus);
            next.target_id = Number(targetId);
            next.open = Number(openState);
            this.dartStatusByTeam = Object.assign({}, this.dartStatusByTeam, { [teamName]: next });
            // 始终发送到后端以保证 state_manager 能接收到并更新状态
            this.sendCommand('set_dart_gate_status', {
                team: teamName,
                target_id: targetId,
                open_state: openState
            });
        },
        setDartTarget(targetId) {
            // 先在本地保存对应队伍的目标选择
            const teamName = this.dartGateTeam === 'blue' ? 'blue' : 'red';
            const next = Object.assign({}, this.dartStatusByTeam[teamName] || this.dartStatus);
            next.target_id = Number(targetId);
            this.dartStatusByTeam = Object.assign({}, this.dartStatusByTeam, { [teamName]: next });
            // 始终向后端发送选择的目标以保持 state 的单一来源
            this.sendCommand('set_dart_target', {
                team: teamName,
                target_id: Number(targetId)
            });
        },
        setGlobalSpecialMechanism() {
            this.sendCommand('set_global_special_mechanism', {
                ally_fortress_sec: this.specialMechanismForm.ally_fortress_sec,
                enemy_fortress_sec: this.specialMechanismForm.enemy_fortress_sec
            });
        },
        startFortressCapture(target) {
            const captureTarget = target === 'enemy' ? 'enemy' : 'ally';
            this.specialMechanismForm.ally_fortress_sec = captureTarget === 'ally' ? FORTRESS_CAPTURE_DURATION_SEC : 0;
            this.specialMechanismForm.enemy_fortress_sec = captureTarget === 'enemy' ? FORTRESS_CAPTURE_DURATION_SEC : 0;
            this.setGlobalSpecialMechanism();
        },
        resetFortressCapture() {
            this.specialMechanismForm.ally_fortress_sec = 0;
            this.specialMechanismForm.enemy_fortress_sec = 0;
            this.setGlobalSpecialMechanism();
        },
        sendRobotInjuryStat() {
            this.normalizeRobotInjuryForm();
            this.sendCommand('set_robot_injury_stat', Object.assign({}, this.robotInjuryForm, {
                robot_id: Number(this.robotInjuryForm.robot_id)
            }));
        },
        clampPathPlanIntention(value) {
            const normalized = Math.trunc(Number(value) || 1);
            return Math.max(1, Math.min(3, normalized));
        },
        clampPathPlanSenderId(value) {
            const normalized = Math.trunc(Number(value) || 7);
            return normalized > 0 ? normalized : 7;
        },
        clampPathPlanOffset(value) {
            const normalized = Math.trunc(Number(value) || 0);
            return Math.max(-128, Math.min(127, normalized));
        },
        getPathPlanBoundsDm() {
            return {
                width: Math.max(1, Math.round(Number(this.mapWidth || 28) * 10)),
                height: Math.max(1, Math.round(Number(this.mapHeight || 15) * 10))
            };
        },
        normalizeCanvasCoordinate(value, maxValue) {
            const normalized = Math.trunc(Number(value) || 0);
            return Math.max(this.pathPlanPadding, Math.min(maxValue - this.pathPlanPadding, normalized));
        },
        clampPathPlanDmX(value) {
            const bounds = this.getPathPlanBoundsDm();
            const normalized = Math.trunc(Number(value) || 0);
            return Math.max(0, Math.min(bounds.width, normalized));
        },
        clampPathPlanDmY(value) {
            const bounds = this.getPathPlanBoundsDm();
            const normalized = Math.trunc(Number(value) || 0);
            return Math.max(0, Math.min(bounds.height, normalized));
        },
        pathPlanDmToCanvas(dmX, dmY) {
            const bounds = this.getPathPlanBoundsDm();
            const drawableWidth = this.pathPlanWidth - this.pathPlanPadding * 2;
            const drawableHeight = this.pathPlanHeight - this.pathPlanPadding * 2;
            return {
                x: this.pathPlanPadding + (this.clampPathPlanDmX(dmX) / bounds.width) * drawableWidth,
                y: this.pathPlanPadding + (1 - this.clampPathPlanDmY(dmY) / bounds.height) * drawableHeight
            };
        },
        pathPlanCanvasToDm(canvasX, canvasY) {
            const bounds = this.getPathPlanBoundsDm();
            const drawableWidth = this.pathPlanWidth - this.pathPlanPadding * 2;
            const drawableHeight = this.pathPlanHeight - this.pathPlanPadding * 2;
            const normalizedX = (this.normalizeCanvasCoordinate(canvasX, this.pathPlanWidth) - this.pathPlanPadding) / drawableWidth;
            const normalizedY = (this.normalizeCanvasCoordinate(canvasY, this.pathPlanHeight) - this.pathPlanPadding) / drawableHeight;
            return {
                x: this.clampPathPlanDmX(Math.round(normalizedX * bounds.width)),
                y: this.clampPathPlanDmY(Math.round((1 - normalizedY) * bounds.height))
            };
        },
        hasRobotPathPlanGeometry(info = {}) {
            const startX = Number(info.start_pos_x || 0);
            const startY = Number(info.start_pos_y || 0);
            const offsetX = Array.isArray(info.offset_x) ? info.offset_x : [];
            const offsetY = Array.isArray(info.offset_y) ? info.offset_y : [];
            return startX !== 0
                || startY !== 0
                || offsetX.some(value => Number(value || 0) !== 0)
                || offsetY.some(value => Number(value || 0) !== 0);
        },
        getDefaultRobotPathPlanPoints() {
            const defaultPointsDm = [
                { x: 20, y: 20 },
                { x: 60, y: 20 },
                { x: 110, y: 45 },
                { x: 160, y: 90 }
            ];
            return defaultPointsDm.map(point => this.pathPlanDmToCanvas(point.x, point.y));
        },
        buildRobotPathPlanPoints(info = {}) {
            if (!this.hasRobotPathPlanGeometry(info)) {
                return this.getDefaultRobotPathPlanPoints().map(point => Object.assign({}, point));
            }

            const startX = this.clampPathPlanDmX(info.start_pos_x);
            const startY = this.clampPathPlanDmY(info.start_pos_y);
            const offsetX = Array.isArray(info.offset_x) ? info.offset_x : [];
            const offsetY = Array.isArray(info.offset_y) ? info.offset_y : [];
            const points = [this.pathPlanDmToCanvas(startX, startY)];

            for (let i = 0; i < 3; i += 1) {
                const dx = this.clampPathPlanOffset(offsetX[i] ?? 0);
                const dy = this.clampPathPlanOffset(offsetY[i] ?? 0);
                points.push(this.pathPlanDmToCanvas(startX + dx, startY + dy));
            }

            return points;
        },
        syncRobotPathPlanFormFromPoints() {
            const points = (Array.isArray(this.robotPathPlanPoints) ? this.robotPathPlanPoints : [])
                .slice(0, 4)
                .map(point => ({
                    x: this.normalizeCanvasCoordinate(point.x, this.pathPlanWidth),
                    y: this.normalizeCanvasCoordinate(point.y, this.pathPlanHeight)
                }));
            while (points.length < 4) {
                const fallback = this.getDefaultRobotPathPlanPoints()[points.length];
                points.push(Object.assign({}, fallback));
            }

            this.robotPathPlanPoints = points;
            const startPoint = points[0];
            const startDm = this.pathPlanCanvasToDm(startPoint.x, startPoint.y);
            const offsetX = [];
            const offsetY = [];
            for (let i = 1; i < points.length; i += 1) {
                const pointDm = this.pathPlanCanvasToDm(points[i].x, points[i].y);
                offsetX.push(this.clampPathPlanOffset(pointDm.x - startDm.x));
                offsetY.push(this.clampPathPlanOffset(pointDm.y - startDm.y));
            }

            this.robotPathPlanForm = Object.assign({}, this.robotPathPlanForm, {
                intention: this.clampPathPlanIntention(this.robotPathPlanForm.intention),
                sender_id: this.clampPathPlanSenderId(this.robotPathPlanForm.sender_id),
                start_pos_x: startDm.x,
                start_pos_y: startDm.y,
                offset_x: offsetX,
                offset_y: offsetY
            });
        },
        syncRobotPathPlanEditorFromInfo(info = {}) {
            this.isSyncingRobotPathPlanForm = true;
            this.robotPathPlanForm = Object.assign({}, this.robotPathPlanForm, {
                intention: this.clampPathPlanIntention(info.intention ?? 1),
                sender_id: this.clampPathPlanSenderId(info.sender_id ?? 7)
            });
            this.robotPathPlanPoints = this.buildRobotPathPlanPoints(info);
            this.syncRobotPathPlanFormFromPoints();
            this.isSyncingRobotPathPlanForm = false;
            this.$nextTick(() => this.drawRobotPathPlanCanvas());
        },
        isRobotPathPlanStateEqual(info = {}) {
            const incomingOffsetX = Array.isArray(info.offset_x) ? info.offset_x : [];
            const incomingOffsetY = Array.isArray(info.offset_y) ? info.offset_y : [];
            const localOffsetX = Array.isArray(this.robotPathPlanForm.offset_x) ? this.robotPathPlanForm.offset_x : [];
            const localOffsetY = Array.isArray(this.robotPathPlanForm.offset_y) ? this.robotPathPlanForm.offset_y : [];

            return this.clampPathPlanIntention(info.intention ?? 1) === this.clampPathPlanIntention(this.robotPathPlanForm.intention)
                && this.clampPathPlanSenderId(info.sender_id ?? 7) === this.clampPathPlanSenderId(this.robotPathPlanForm.sender_id)
                && this.clampPathPlanDmX(info.start_pos_x ?? 0) === this.clampPathPlanDmX(this.robotPathPlanForm.start_pos_x)
                && this.clampPathPlanDmY(info.start_pos_y ?? 0) === this.clampPathPlanDmY(this.robotPathPlanForm.start_pos_y)
                && incomingOffsetX.slice(0, 3).map(value => this.clampPathPlanOffset(value)).join(',') === localOffsetX.slice(0, 3).map(value => this.clampPathPlanOffset(value)).join(',')
                && incomingOffsetY.slice(0, 3).map(value => this.clampPathPlanOffset(value)).join(',') === localOffsetY.slice(0, 3).map(value => this.clampPathPlanOffset(value)).join(',');
        },
        markRobotPathPlanDirty() {
            if (!this.isSyncingRobotPathPlanForm) {
                this.robotPathPlanDirty = true;
            }
        },
        sendRobotPathPlan() {
            this.syncRobotPathPlanFormFromPoints();
            this.robotPathPlanDirty = true;
            this.sendCommand('set_robot_path_plan', {
                intention: this.clampPathPlanIntention(this.robotPathPlanForm.intention),
                start_pos_x: Number(this.robotPathPlanForm.start_pos_x),
                start_pos_y: Number(this.robotPathPlanForm.start_pos_y),
                offset_x: [...this.robotPathPlanForm.offset_x],
                offset_y: [...this.robotPathPlanForm.offset_y],
                sender_id: this.clampPathPlanSenderId(this.robotPathPlanForm.sender_id)
            });
        },
        sendRobotStaticStatus() {
            this.robotStaticStatusDirty = true;
            this.sendCommand('send_robot_static_status', {
                connection_state: this.toNonNegativeInt(this.robotStaticStatusForm.connection_state),
                field_state: this.toNonNegativeInt(this.robotStaticStatusForm.field_state),
                alive_state: this.toNonNegativeInt(this.robotStaticStatusForm.alive_state),
                robot_id: this.toNonNegativeInt(this.robotStaticStatusForm.robot_id),
                robot_type: this.toNonNegativeInt(this.robotStaticStatusForm.robot_type),
                performance_system_shooter: this.toNonNegativeInt(this.robotStaticStatusForm.performance_system_shooter),
                performance_system_chassis: this.toNonNegativeInt(this.robotStaticStatusForm.performance_system_chassis),
                level: this.toNonNegativeInt(this.robotStaticStatusForm.level),
                max_health: this.toNonNegativeInt(this.robotStaticStatusForm.max_health),
                max_heat: this.toNonNegativeInt(this.robotStaticStatusForm.max_heat),
                heat_cooldown_rate: this.toStatusFloat(this.robotStaticStatusForm.heat_cooldown_rate, 20),
                max_power: this.toNonNegativeInt(this.robotStaticStatusForm.max_power),
                max_buffer_energy: this.toNonNegativeInt(this.robotStaticStatusForm.max_buffer_energy),
                max_chassis_energy: this.toNonNegativeInt(this.robotStaticStatusForm.max_chassis_energy)
            });
        },
        sendRobotDynamicStatus() {
            this.robotDynamicStatusDirty = true;
            this.sendCommand('send_robot_dynamic_status', {
                current_health: this.toNonNegativeInt(this.robotDynamicStatusForm.current_health),
                current_heat: this.toStatusFloat(this.robotDynamicStatusForm.current_heat, 0),
                last_projectile_fire_rate: this.toStatusFloat(this.robotDynamicStatusForm.last_projectile_fire_rate, 0),
                current_chassis_energy: this.toNonNegativeInt(this.robotDynamicStatusForm.current_chassis_energy),
                current_buffer_energy: this.toNonNegativeInt(this.robotDynamicStatusForm.current_buffer_energy),
                current_experience: this.toNonNegativeInt(this.robotDynamicStatusForm.current_experience),
                experience_for_upgrade: this.toNonNegativeInt(this.robotDynamicStatusForm.experience_for_upgrade),
                total_projectiles_fired: this.toNonNegativeInt(this.robotDynamicStatusForm.total_projectiles_fired),
                remaining_ammo: this.toNonNegativeInt(this.robotDynamicStatusForm.remaining_ammo),
                is_out_of_combat: this.toStatusBool(this.robotDynamicStatusForm.is_out_of_combat),
                out_of_combat_countdown: this.toNonNegativeInt(this.robotDynamicStatusForm.out_of_combat_countdown),
                can_remote_heal: this.toStatusBool(this.robotDynamicStatusForm.can_remote_heal),
                can_remote_ammo: this.toStatusBool(this.robotDynamicStatusForm.can_remote_ammo)
            });
        },
        sendRobotModuleStatus() {
            this.robotModuleStatusDirty = true;
            this.sendCommand('send_robot_module_status', {
                power_manager: this.toNonNegativeInt(this.robotModuleStatusForm.power_manager),
                rfid: this.toNonNegativeInt(this.robotModuleStatusForm.rfid),
                light_strip: this.toNonNegativeInt(this.robotModuleStatusForm.light_strip),
                small_shooter: this.toNonNegativeInt(this.robotModuleStatusForm.small_shooter),
                big_shooter: this.toNonNegativeInt(this.robotModuleStatusForm.big_shooter),
                uwb: this.toNonNegativeInt(this.robotModuleStatusForm.uwb),
                armor: this.toNonNegativeInt(this.robotModuleStatusForm.armor),
                video_transmission: this.toNonNegativeInt(this.robotModuleStatusForm.video_transmission),
                capacitor: this.toNonNegativeInt(this.robotModuleStatusForm.capacitor),
                main_controller: this.toNonNegativeInt(this.robotModuleStatusForm.main_controller),
                laser_detection_module: this.toNonNegativeInt(this.robotModuleStatusForm.laser_detection_module)
            });
        },
        sendRobotPerformanceSelection() {
            this.sendCommand('set_robot_performance_selection_sync', Object.assign({}, this.robotPerformanceForm));
        },
        sendSentryStatusSync() {
            const sentryMode = Number(this.sentryStatusForm.is_weakened);
            this.sendCommand('set_sentry_status_sync', {
                posture_id: Number(this.sentryStatusForm.posture_id),
                is_weakened: sentryMode === 1,
                is_powered: sentryMode === 2
            });
        },
        sendSentryCtrlResult() {
            this.sendCommand('send_sentry_ctrl_result', {
                command_id: Number(this.sentryCtrlResultForm.command_id),
                result_code: Number(this.sentryCtrlResultForm.result_code)
            });
        },
        sendCustomByteBlock() {
            this.sendCommand('send_custom_byte_block', Object.assign({}, this.customByteBlockForm));
        },
        startEditRuneMetrics() {
            this.isEditingRuneMetrics = true;
        },
        finishEditRuneMetrics() {
            this.isEditingRuneMetrics = false;
            this.updateRuneMetrics();
        },
        updateRuneMetrics() {
            this.sendCommand('set_rune_metrics', {
                team: this.runeTeam,
                rune_type: Number(this.runeType),
                activated_arms: this.runeInputs.activated_arms,
                average_rings: this.runeInputs.average_rings
            });
        },
        getRuneStatusName(status) {
            const statusMap = {
                1: '未激活',
                2: '正在激活',
                3: '已激活'
            };
            return statusMap[status] || '未知状态';
        },
        getRunePhaseName() {
            if (Number(this.gameStatus.current_stage) !== 4) {
                return '非战斗阶段';
            }
            const remaining = Number(this.gameStatus.stage_countdown_sec ?? 0);
            return remaining > 240 ? '小能量机关阶段' : '大能量机关阶段';
        },
        getAirSupportStatusName(status) {
            const statusMap = {
                0: '未启动',
                1: '支援中',
                2: '被锁定'
            };
            return statusMap[Number(status)] || '未知';
        },
        getDeployModeStatusName(status) {
            return Number(status) === 1 ? '已部署' : '未部署';
        },
        getWinnerSummary() {
            const redScore = Number(this.gameStatus.red_score ?? 0);
            const blueScore = Number(this.gameStatus.blue_score ?? 0);
            if (redScore > blueScore) return '红方领先';
            if (blueScore > redScore) return '蓝方领先';
            const winner = Number(this.lastGameResult.winner ?? 0);
            if (winner === 1) return '最近结算红方胜';
            if (winner === 2) return '最近结算蓝方胜';
            return '当前平局';
        },
        applyRoundConfig() {
            this.sendCommand('set_round_config', {
                current_round: Number(this.matchForm.current_round),
                total_rounds: Number(this.matchForm.total_rounds)
            });
        },
        setMatchStage(stage, countdownSec = null) {
            const payload = { stage: Number(stage) };
            if (countdownSec !== null) {
                payload.countdown_sec = Number(countdownSec);
            }
            this.sendCommand('set_match_stage', payload);
        },
        setHp(index, hp) {
            this.sendCommand('set_hp', { robot_id: index, hp: hp });
        },
        setBaseHp(team, hp) {
            this.sendCommand('set_base_hp', { team: team, hp: hp });
        },
        changeBaseHp(team, delta) {
            this.sendCommand('adjust_base_hp', { team: team, delta: delta });
        },
        setBaseProtocolState(team, overrides = {}) {
            this.sendCommand('set_base_protocol_state', Object.assign({
                team: team,
                status: this.getBaseStatus(team),
                shield: this.getBaseShield(team)
            }, overrides));
        },
        setOutpostHp(hp) {
            this.sendCommand('set_outpost_hp', { hp: hp, team: this.outpostTeam });
        },
        adjustOutpostHp(delta) {
            this.sendCommand('adjust_outpost_hp', { team: this.outpostTeam, delta: delta });
        },
        setOutpostStatus(status) {
            this.sendCommand('set_outpost_status', { status: status, team: this.outpostTeam });
        },
        applyScore(team) {
            const key = team === 'blue' ? 'blue_score' : 'red_score';
            this.sendCommand('set_score', { team: team, score: this.controlValues[key] });
        },
        adjustScore(team, delta) {
            this.sendCommand('adjust_score', { team: team, delta: delta });
        },
        applyEconomy(team) {
            const key = team === 'blue' ? 'blue_economy' : 'red_economy';
            const totalKey = team === 'blue' ? 'blue_total_economy_obtained' : 'red_total_economy_obtained';
            const economy = this.controlValues[key];
            const total = this.globalLogisticsStatus[totalKey];

            // 超过累计经济时，自动限制到累计经济
            let finalEconomy = economy;
            if (finalEconomy > total) {
                finalEconomy = total;
            }

            this.sendCommand('set_economy', { team: team, economy: finalEconomy });
        },
        adjustEconomy(team, delta) {
            const key = team === 'blue' ? 'blue_economy' : 'red_economy';
            const totalKey = team === 'blue' ? 'blue_total_economy_obtained' : 'red_total_economy_obtained';
            const currentEconomy = this.controlValues[key];
            const total = this.globalLogisticsStatus[totalKey];
            let newEconomy = currentEconomy + delta;

            // 自动限制范围
            if (newEconomy > total) {
                newEconomy = total;
            }
            if (newEconomy < 0) {
                newEconomy = 0;
            }

            this.sendCommand('adjust_economy', { team: team, delta: newEconomy - currentEconomy });
        },
        setLogisticsProtocolState(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            this.sendCommand('set_logistics_protocol_state', {
                team: teamName,
                total_economy_obtained: Number(this.globalLogisticsStatus[`${teamName}_total_economy_obtained`] ?? 0),
                total_damage: Number(this.globalLogisticsStatus[`${teamName}_total_damage`] ?? 0),
                tech_level: Number(this.globalLogisticsStatus[`${teamName}_tech_level`] ?? 0),
                encryption_level: Number(this.globalLogisticsStatus[`${teamName}_encryption_level`] ?? 0)
            });
        },
        setAirSupportDuration(team) {
            const teamName = team === 'blue' ? 'blue' : 'red';
            this.sendCommand('set_air_support_duration', {
                team: teamName,
                duration_sec: Number(this.airSupportDraft[`${teamName}_default_time`] ?? 30)
            });
        },
        sendTechCoreMotionState() {
            const payload = Object.assign({}, this.techCoreMotionStateDraft);
            payload.basic_state = payload.basic_state ?? payload.status ?? 1;
            payload.status = payload.basic_state;
            this.techCoreMotionState = Object.assign({}, payload);
            this.techCoreMotionStateDraft = Object.assign({}, payload);
            this.techCoreMotionStateDirty = false;
            this.sendCommand('set_tech_core_motion_state', {
                maximum_difficulty_level: payload.maximum_difficulty_level,
                basic_state: payload.basic_state ?? payload.status,
                status: payload.status,
                putin_state: payload.putin_state,
                move_state: payload.move_state,
                rotate_state: payload.rotate_state,
                enemy_core_status: payload.enemy_core_status,
                remain_time_all: payload.remain_time_all,
                remain_time_step: payload.remain_time_step
            });
        },
        applyTechCorePreset(presetName) {
            const presets = {
                idle: { basic_state: 1 },
                moving: { basic_state: 2 },
                arrived: { basic_state: 3 },
                reset: {
                    maximum_difficulty_level: 1,
                    basic_state: 1,
                    putin_state: 0,
                    move_state: 0,
                    rotate_state: 0,
                    enemy_core_status: 0,
                    remain_time_all: 0,
                    remain_time_step: 0
                }
            };
            const patch = presets[presetName];
            if (!patch) return;
            this.techCoreMotionStateDraft = Object.assign({}, this.techCoreMotionStateDraft, patch);
            this.techCoreMotionStateDirty = true;
            this.sendTechCoreMotionState();
        },
        toggleTechCoreState(stateKey) {
            // 切换状态：0 <-> 1
            const currentValue = this.techCoreMotionStateDraft[stateKey];
            const newValue = currentValue === 1 ? 0 : 1;
            this.techCoreMotionStateDraft[stateKey] = newValue;
            this.techCoreMotionStateDirty = true;
            this.sendTechCoreMotionState();
        },
        toggleTechCoreBasicState(targetState) {
            // 切换基础状态：当前为targetState时恢复为1，否则设为targetState
            const currentValue = this.techCoreMotionStateDraft.basic_state;
            const newValue = currentValue === targetState ? 1 : targetState;
            this.techCoreMotionStateDraft.basic_state = newValue;
            this.techCoreMotionStateDirty = true;
            this.sendTechCoreMotionState();
        },
        sendDeployModeStatus(status) {
            const normalizedStatus = Number(status) === 1 ? 1 : 0;
            this.deployModeStatus = normalizedStatus;
            const team = Number(this.currentRobotId) >= 100 ? 'blue' : 'red';
            this.sendCommand('set_deploy_mode_status', {
                status: normalizedStatus,
                team
            });
        },
        // 切换指定队伍的英雄部署（仅当当前用户属于该队时才发送代表己方的 MQTT）
        toggleHeroDeploy(team, status) {
            const normalized = Number(status) === 1 ? 1 : 0;
            // 更新客户端显示状态
            this.deployStatusByTeam = Object.assign({}, this.deployStatusByTeam, { [team]: normalized });
            // 始终发送到后端以保证 state_manager 能接收到并更新状态
            this.sendCommand('set_deploy_mode_status', { team, status: normalized });
        },
        sendEvent(eventId = null, param = null) {
            const resolvedEventId = eventId === null ? this.eventCommand.event_id : eventId;
            const resolvedParam = param === null ? this.eventCommand.param : param;
            this.eventCommand.event_id = Number(resolvedEventId);
            this.eventCommand.param = String(resolvedParam ?? '');
            this.sendCommand('send_event', {
                event_id: this.eventCommand.event_id,
                param: this.eventCommand.param
            });
        },
        sendBuff(overrides = {}) {
            const payload = Object.assign({}, this.buffCommand, overrides);
            payload.robot_id = Math.max(0, Number(payload.robot_id ?? 1));
            payload.buff_type = Math.max(0, Number(payload.buff_type ?? 0));
            payload.buff_level = Number(payload.buff_level ?? 1);
            payload.buff_max_time = Math.max(0, Number(payload.buff_max_time ?? 0));
            payload.buff_left_time = Math.max(0, Number(payload.buff_left_time ?? 0));
            this.buffCommand = Object.assign({}, payload);
            this.sendCommand('send_buff', payload);
        },
        sendBuffPreset(buffType) {
            this.sendBuff({
                buff_type: Number(buffType),
                buff_level: 1,
                buff_max_time: 30,
                buff_left_time: 30
            });
        },
        setVideo() {
            if (this.selectedVideo) {
                this.sendCommand('set_video', { filename: this.selectedVideo });
            }
        },
        setCustomVideo() {
            if (this.customVideoUrl) {
                this.sendCommand('set_custom_url', { url: this.customVideoUrl });
            }
        },
        // === 推流控制方法 ===
        startStreaming() {
            this.sendCommand('video_control', { action: 'start' });
        },
        stopStreaming() {
            this.sendCommand('video_control', { action: 'stop' });
        },
        pauseStreaming() {
            this.sendCommand('video_control', { action: 'pause' });
        },
        resumeStreaming() {
            this.sendCommand('video_control', { action: 'resume' });
        },
        // === 工业相机推流控制方法 (H.264 over MQTT) ===
        setIndustrialVideo() {
            if (this.selectedIndustrialVideo) {
                this.sendCommand('set_industrial_video', { filename: this.selectedIndustrialVideo });
            }
        },
        startIndustrialCamera() {
            this.sendCommand('industrial_camera_control', { action: 'start' });
        },
        stopIndustrialCamera() {
            this.sendCommand('industrial_camera_control', { action: 'stop' });
        },
        pauseIndustrialCamera() {
            this.sendCommand('industrial_camera_control', { action: 'pause' });
        },
        resumeIndustrialCamera() {
            this.sendCommand('industrial_camera_control', { action: 'resume' });
        },
        async uploadVideo() {
            const file = this.$refs.fileInput.files[0];
            if (!file) return;

            const formData = new FormData();
            formData.append('file', file);

            try {
                const response = await fetch('/upload', {
                    method: 'POST',
                    body: formData
                });
                const result = await response.json();
                if (result.error) {
                    alert('Upload failed: ' + result.error);
                } else {
                    alert('Upload successful!');
                    this.$refs.fileInput.value = ''; // 清空文件选择
                }
            } catch (error) {
                console.error('Error:', error);
                alert('Upload error');
            }
        },
        toProtocolRobotId(index) {
            const idx = Number(index);
            return Number(this.robotIds[idx] ?? 0);
        },
        applyPenalty(index, damage) {
            this.sendCommand('penalty', { robot_id: index, damage: damage });
        },
        adjustRobotHp(index, delta) {
            this.sendCommand('adjust_robot_hp', { robot_id: index, delta: delta });
        },
        issueWarning(index, level) {
            const robotId = this.toProtocolRobotId(index);
            if (!robotId) {
                return;
            }
            this.sendCommand('referee_warning', { robot_id: robotId, level: level });
        },
        issueDoubleYellow() {
            this.sendCommand('referee_warning', { level: 2, robot_id: 0 });
        },
        triggerOverheat(index) {
            this.sendCommand('set_robot_detail', {
                robot_id: index,
                level: Number(this.globalUnitStatus.robot_level[index] ?? 1),
                heat: 300,
                power: Number(this.globalUnitStatus.robot_power[index] ?? 0),
                fire_rate: Number(this.globalUnitStatus.robot_fire_rate[index] ?? 0),
                bullets: Number(this.globalUnitStatus.robot_bullets[index] ?? 0)
            });
        },
        triggerOverfire(index) {
            this.sendCommand('set_robot_detail', {
                robot_id: index,
                level: Number(this.globalUnitStatus.robot_level[index] ?? 1),
                heat: Number(this.globalUnitStatus.robot_heat[index] ?? 0),
                power: Number(this.globalUnitStatus.robot_power[index] ?? 0),
                fire_rate: 40,
                bullets: Number(this.globalUnitStatus.robot_bullets[index] ?? 0)
            });
        },
        triggerOverpower(index) {
            this.sendCommand('set_robot_detail', {
                robot_id: index,
                level: Number(this.globalUnitStatus.robot_level[index] ?? 1),
                heat: Number(this.globalUnitStatus.robot_heat[index] ?? 0),
                power: 150,
                fire_rate: Number(this.globalUnitStatus.robot_fire_rate[index] ?? 0),
                bullets: Number(this.globalUnitStatus.robot_bullets[index] ?? 0)
            });
        },
        usePathPlanSender(robotId) {
            this.markRobotPathPlanDirty();
            this.robotPathPlanForm.sender_id = this.clampPathPlanSenderId(robotId);
        },
        initPathPlanCanvas() {
            this.pathPlanCanvas = document.getElementById('pathPlanCanvas');
            if (!this.pathPlanCanvas) {
                return;
            }
            this.pathPlanCtx = this.pathPlanCanvas.getContext('2d');
            this.drawRobotPathPlanCanvas();
        },
        getPathPlanMousePos(evt) {
            const rect = this.pathPlanCanvas.getBoundingClientRect();
            const scaleX = this.pathPlanCanvas.width / rect.width;
            const scaleY = this.pathPlanCanvas.height / rect.height;
            return {
                x: (evt.clientX - rect.left) * scaleX,
                y: (evt.clientY - rect.top) * scaleY
            };
        },
        startPathPlanDrag(evt) {
            if (!this.pathPlanCanvas) {
                return;
            }
            const mouse = this.getPathPlanMousePos(evt);
            let hitIndex = -1;
            let bestDistance = Number.POSITIVE_INFINITY;
            this.robotPathPlanPoints.forEach((point, index) => {
                const dx = mouse.x - point.x;
                const dy = mouse.y - point.y;
                const distance = Math.sqrt(dx * dx + dy * dy);
                if (distance < 14 && distance < bestDistance) {
                    bestDistance = distance;
                    hitIndex = index;
                }
            });
            this.draggingPathPlanPoint = hitIndex;
            if (hitIndex !== -1) {
                this.markRobotPathPlanDirty();
                this.isEditingRobotPathPlanForm = true;
            }
        },
        onPathPlanDrag(evt) {
            if (this.draggingPathPlanPoint === -1 || !this.pathPlanCanvas) {
                return;
            }
            const mouse = this.getPathPlanMousePos(evt);
            const nextPoint = {
                x: this.normalizeCanvasCoordinate(mouse.x, this.pathPlanWidth),
                y: this.normalizeCanvasCoordinate(mouse.y, this.pathPlanHeight)
            };
            this.$set(this.robotPathPlanPoints, this.draggingPathPlanPoint, nextPoint);
            this.syncRobotPathPlanFormFromPoints();
            this.drawRobotPathPlanCanvas();
        },
        stopPathPlanDrag() {
            if (this.draggingPathPlanPoint !== -1) {
                this.syncRobotPathPlanFormFromPoints();
                this.drawRobotPathPlanCanvas();
            }
            this.draggingPathPlanPoint = -1;
            this.isEditingRobotPathPlanForm = false;
        },
        drawRobotPathPlanCanvas() {
            if (!this.pathPlanCtx || !this.pathPlanCanvas) {
                return;
            }

            const ctx = this.pathPlanCtx;
            const w = this.pathPlanCanvas.width;
            const h = this.pathPlanCanvas.height;
            const points = this.robotPathPlanPoints;
            const drawRoundedRect = (left, top, width, height, radius) => {
                const corner = Math.min(radius, width / 2, height / 2);
                ctx.beginPath();
                ctx.moveTo(left + corner, top);
                ctx.lineTo(left + width - corner, top);
                ctx.quadraticCurveTo(left + width, top, left + width, top + corner);
                ctx.lineTo(left + width, top + height - corner);
                ctx.quadraticCurveTo(left + width, top + height, left + width - corner, top + height);
                ctx.lineTo(left + corner, top + height);
                ctx.quadraticCurveTo(left, top + height, left, top + height - corner);
                ctx.lineTo(left, top + corner);
                ctx.quadraticCurveTo(left, top, left + corner, top);
                ctx.closePath();
            };

            ctx.clearRect(0, 0, w, h);
            ctx.save();
            ctx.fillStyle = 'rgba(5, 10, 18, 0.18)';
            ctx.fillRect(0, 0, w, h);
            ctx.strokeStyle = 'rgba(162, 184, 223, 0.12)';
            ctx.lineWidth = 1;
            for (let x = this.pathPlanPadding; x <= w - this.pathPlanPadding; x += 32) {
                ctx.beginPath();
                ctx.moveTo(x, this.pathPlanPadding);
                ctx.lineTo(x, h - this.pathPlanPadding);
                ctx.stroke();
            }
            for (let y = this.pathPlanPadding; y <= h - this.pathPlanPadding; y += 32) {
                ctx.beginPath();
                ctx.moveTo(this.pathPlanPadding, y);
                ctx.lineTo(w - this.pathPlanPadding, y);
                ctx.stroke();
            }

            ctx.strokeStyle = '#67b3ff';
            ctx.lineWidth = 3;
            ctx.beginPath();
            points.forEach((point, index) => {
                if (index === 0) {
                    ctx.moveTo(point.x, point.y);
                } else {
                    ctx.lineTo(point.x, point.y);
                }
            });
            ctx.stroke();

            points.forEach((point, index) => {
                const isStart = index === 0;
                const isActive = index === this.draggingPathPlanPoint;
                ctx.save();
                ctx.fillStyle = isStart ? '#ffb347' : '#2f7bff';
                ctx.strokeStyle = isActive ? '#ffe082' : '#ffffff';
                ctx.lineWidth = isActive ? 3 : 2;
                ctx.beginPath();
                ctx.arc(point.x, point.y, isStart ? 7 : 6, 0, Math.PI * 2);
                ctx.fill();
                ctx.stroke();

                const label = isStart ? '起点' : `P${index}`;
                const pillWidth = isStart ? 34 : 24;
                const pillX = point.x - pillWidth / 2;
                const pillY = point.y - 24;
                ctx.fillStyle = 'rgba(8, 13, 24, 0.92)';
                ctx.strokeStyle = isStart ? 'rgba(255, 179, 71, 0.9)' : 'rgba(103, 179, 255, 0.9)';
                ctx.lineWidth = 1;
                drawRoundedRect(pillX, pillY, pillWidth, 16, 8);
                ctx.fill();
                ctx.stroke();

                ctx.fillStyle = '#ffffff';
                ctx.font = 'bold 10px "Segoe UI"';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(label, point.x, pillY + 8);
                ctx.restore();
            });
            ctx.restore();
        },
        setDetail(index, options = {}) {
            this.sendCommand('set_robot_detail', {
                robot_id: index,
                level: this.globalUnitStatus.robot_level[index],
                heat: this.globalUnitStatus.robot_heat[index],
                hold_heat: Boolean(options.holdHeat),
                power: this.globalUnitStatus.robot_power[index],
                fire_rate: this.globalUnitStatus.robot_fire_rate[index],
                bullets: this.globalUnitStatus.robot_bullets[index]
            });
        },
        syncRobotPose(index) {
            const pose = this.positions[index];
            if (!pose) return;

            this.sendCommand('set_position', {
                robot_id: index,
                x: Number(pose.x),
                y: Number(pose.y),
                angle: Number(pose.angle || 0)
            });
        },
        getMinimapRobotVisual(index) {
            const robotId = Number(this.robotIds[index] ?? 0);
            const isBlue = robotId >= 100;
            const isActive = index === this.draggingRobot;
            const roleMap = {
                1: { icon: 'H', label: 'Hero' },
                3: { icon: 'I', label: 'Infantry' },
                6: { icon: 'S', label: 'Scout' },
                7: { icon: 'T', label: 'Sentry' }
            };
            const role = roleMap[robotId % 100] || { icon: 'R', label: 'Robot' };
            return {
                fill: isBlue ? '#2f7bff' : '#ff4d4d',
                ring: isBlue ? 'rgba(115, 172, 255, 0.9)' : 'rgba(255, 166, 166, 0.9)',
                shadow: isBlue ? 'rgba(46, 123, 255, 0.38)' : 'rgba(255, 77, 77, 0.38)',
                labelBg: isBlue ? 'rgba(18, 49, 110, 0.92)' : 'rgba(96, 18, 18, 0.92)',
                activeStroke: isActive ? '#ffe082' : 'rgba(255,255,255,0.75)',
                icon: role.icon,
                label: role.label
            };
        },
        drawMinimapRobot(x, y, angle, visual, robotId) {
            const ctx = this.ctx;
            if (!ctx) return;

            const scale = Math.max(0.75, Math.min(this.canvas.width / 800, this.canvas.height / 450));
            const radius = 11 * scale;
            const drawRoundedRect = (left, top, width, height, cornerRadius) => {
                const r = Math.min(cornerRadius, width / 2, height / 2);
                ctx.beginPath();
                ctx.moveTo(left + r, top);
                ctx.lineTo(left + width - r, top);
                ctx.quadraticCurveTo(left + width, top, left + width, top + r);
                ctx.lineTo(left + width, top + height - r);
                ctx.quadraticCurveTo(left + width, top + height, left + width - r, top + height);
                ctx.lineTo(left + r, top + height);
                ctx.quadraticCurveTo(left, top + height, left, top + height - r);
                ctx.lineTo(left, top + r);
                ctx.quadraticCurveTo(left, top, left + r, top);
                ctx.closePath();
            };
            ctx.save();
            ctx.shadowColor = visual.shadow;
            ctx.shadowBlur = 14 * scale;
            ctx.fillStyle = visual.fill;
            ctx.beginPath();
            ctx.arc(x, y, radius, 0, Math.PI * 2);
            ctx.fill();

            ctx.shadowBlur = 0;
            ctx.lineWidth = 2.5 * scale;
            ctx.strokeStyle = visual.ring;
            ctx.beginPath();
            ctx.arc(x, y, radius + 3 * scale, 0, Math.PI * 2);
            ctx.stroke();

            const radians = (Number(angle || 0) - 90) * Math.PI / 180;
            const noseX = x + Math.cos(radians) * 18 * scale;
            const noseY = y + Math.sin(radians) * 18 * scale;
            ctx.strokeStyle = visual.activeStroke;
            ctx.lineWidth = 3 * scale;
            ctx.beginPath();
            ctx.moveTo(x, y);
            ctx.lineTo(noseX, noseY);
            ctx.stroke();

            ctx.fillStyle = '#ffffff';
            ctx.font = `bold ${10 * scale}px "Segoe UI"`;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(visual.icon, x, y + 0.5);

            const idText = String(robotId || 0);
            const pillWidth = Math.max(26, idText.length * 8 + 10) * scale;
            const pillX = x - pillWidth / 2;
            const pillY = y - 29 * scale;
            ctx.fillStyle = visual.labelBg;
            drawRoundedRect(pillX, pillY, pillWidth, 16 * scale, 8 * scale);
            ctx.fill();
            ctx.strokeStyle = visual.activeStroke;
            ctx.lineWidth = 1 * scale;
            ctx.stroke();

            ctx.fillStyle = '#ffffff';
            ctx.font = `bold ${11 * scale}px "Segoe UI"`;
            ctx.fillText(idText, x, pillY + 8 * scale);
            ctx.restore();
        },

        // 小地图逻辑
        initCanvas() {
            this.canvas = document.getElementById('minimapCanvas');
            if (!this.canvas) {
                return;
            }
            this.ctx = this.canvas.getContext('2d');
            this.resizeMinimapCanvas();
        },
        resizeMinimapCanvas() {
            if (!this.canvas || !this.ctx) {
                return;
            }

            const rect = this.canvas.getBoundingClientRect();
            if (rect.width <= 0 || rect.height <= 0) {
                return;
            }
            const pixelRatio = Math.max(1, Math.min(2, Number(window.devicePixelRatio) || 1));
            const targetWidth = Math.max(800, Math.round(rect.width * pixelRatio));
            const targetHeight = Math.max(450, Math.round(rect.height * pixelRatio));
            if (this.canvas.width !== targetWidth || this.canvas.height !== targetHeight) {
                this.canvas.width = targetWidth;
                this.canvas.height = targetHeight;
            }
            this.drawMinimap();
        },
        drawMinimap() {
            if (!this.ctx) return;
            const w = this.canvas.width;
            const h = this.canvas.height;
            const ctx = this.ctx;

            // 背景图由 CSS canvas background-image 提供，这里只清空前景绘制层
            ctx.clearRect(0, 0, w, h);

            ctx.save();
            ctx.fillStyle = 'rgba(5, 10, 18, 0.18)';
            ctx.fillRect(0, 0, w, h);
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
            const drawingScale = Math.max(0.75, Math.min(w / 800, h / 450));
            ctx.lineWidth = drawingScale;
            for (let i = 0; i <= 7; i++) {
                const gridX = (i / 7) * w;
                ctx.beginPath();
                ctx.moveTo(gridX, 0);
                ctx.lineTo(gridX, h);
                ctx.stroke();
            }
            for (let i = 0; i <= 5; i++) {
                const gridY = (i / 5) * h;
                ctx.beginPath();
                ctx.moveTo(0, gridY);
                ctx.lineTo(w, gridY);
                ctx.stroke();
            }
            ctx.restore();

            this.positions.forEach((pos, index) => {
                const x = (pos.x / this.mapWidth) * w;
                // 地图数据以左下角为原点，渲染到 canvas 时需要翻转 Y。
                const y = (1 - pos.y / this.mapHeight) * h;
                const robotId = Number(this.robotIds[index] ?? index + 1);
                const visual = this.getMinimapRobotVisual(index);
                this.drawMinimapRobot(x, y, Number(pos.angle || 0), visual, robotId);
            });
        },
        getMousePos(evt) {
            const rect = this.canvas.getBoundingClientRect();
            return mapGeometry.clientPointToCanvas(
                evt.clientX,
                evt.clientY,
                rect,
                this.canvas.width,
                this.canvas.height
            );
        },
        startDrag(evt) {
            const pos = this.getMousePos(evt);
            const w = this.canvas.width;
            const h = this.canvas.height;
            const drawingScale = Math.max(0.75, Math.min(w / 800, h / 450));

            // 查找点击位置命中的机器人
            this.draggingRobot = -1;
            this.positions.forEach((p, index) => {
                let px = (p.x / this.mapWidth) * w;
                let py = (1 - p.y / this.mapHeight) * h;
                let dist = Math.sqrt((pos.x - px) ** 2 + (pos.y - py) ** 2);
                if (dist < 18 * drawingScale) {
                    this.draggingRobot = index;
                }
            });
            if (this.draggingRobot !== -1 && this.simulationState === 'running') {
                this.simulationManualOverride = true;
            }
        },
        onDrag(evt) {
            if (this.draggingRobot !== -1) {
                const pos = this.getMousePos(evt);
                const w = this.canvas.width;
                const h = this.canvas.height;

                let mapX = (pos.x / w) * this.mapWidth;
                let mapY = (1 - pos.y / h) * this.mapHeight;

                // 限制到地图有效范围
                mapX = Math.max(0, Math.min(this.mapWidth, mapX));
                mapY = Math.max(0, Math.min(this.mapHeight, mapY));

                // 先更新本地位置，保证拖拽流畅
                this.positions[this.draggingRobot].x = mapX;
                this.positions[this.draggingRobot].y = mapY;
                this.drawMinimap();

                // 同步到服务端
                this.sendCommand('set_position', {
                    robot_id: this.draggingRobot,
                    x: mapX,
                    y: mapY,
                    angle: Number(this.positions[this.draggingRobot].angle || 0)
                });
            }
        },
        stopDrag() {
            this.draggingRobot = -1;
        },
        getStageName(stage) {
            const stages = {
                0: '未开始',
                1: '准备阶段',
                2: '自检阶段',
                3: '倒计时',
                4: '比赛中',
                5: '比赛结束'
            };
            return stages[stage] || '未知阶段';
        },
        formatTime(isoStr) {
            const date = new Date(isoStr);
            return date.toLocaleTimeString();
        }
    },
    watch: {
        // 监听baseTeam变化，同步更新baseShieldForm
        baseTeam(newTeam) {
            if (!this.isEditingBaseShieldForm) {
                this.baseShieldForm.shield = this.getBaseShield(newTeam);
            }
        },
        // 监听globalUnitStatus变化，同步更新baseShieldForm（编辑时除外）
        'globalUnitStatus': {
            handler() {
                if (!this.isEditingBaseShieldForm) {
                    this.baseShieldForm.shield = this.getBaseShield(this.baseTeam);
                }
            },
            deep: true
        },
        'robotInjuryForm.robot_id': function() {
            if (!this.isEditingRobotInjuryForm) {
                this.syncRobotInjuryFormFromState();
            }
        },
        'robotInjuryForm.total_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.collision_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.small_projectile_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.large_projectile_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.dart_splash_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.module_offline_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.offline_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.penalty_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotInjuryForm.server_kill_damage': function() {
            this.normalizeRobotInjuryForm();
        },
        'robotPathPlanForm.intention': function() {
            this.markRobotPathPlanDirty();
        },
        'robotPathPlanForm.sender_id': function() {
            this.markRobotPathPlanDirty();
        },
        robotStaticStatusForm: {
            handler() {
                if (!this.isSyncingRobotStaticStatusForm) {
                    this.robotStaticStatusDirty = true;
                }
            },
            deep: true
        },
        robotDynamicStatusForm: {
            handler() {
                if (!this.isSyncingRobotDynamicStatusForm) {
                    this.robotDynamicStatusDirty = true;
                }
            },
            deep: true
        },
        robotModuleStatusForm: {
            handler() {
                if (!this.isSyncingRobotModuleStatusForm) {
                    this.robotModuleStatusDirty = true;
                }
            },
            deep: true
        }
    },
    computed: {
        simulationState() {
            return this.normalizeSimulationState(this.simulationStatus.state, this.simulationStatus);
        },
        simulationIsActive() {
            return ['starting', 'running', 'pausing', 'paused', 'resuming', 'stopping'].includes(this.simulationState);
        },
        simulationCanStop() {
            // 比赛演示完成后仍允许显式停止或重置，使操作者无需先启动下一局
            // 就能恢复初始位置和血量。
            return !['idle', 'stopping'].includes(this.simulationState);
        },
        simulationStateLabel() {
            if (!this.connected) {
                return '服务未连接';
            }
            const labels = {
                idle: '等待开始',
                starting: '正在启动',
                running: '赛事演示进行中',
                pausing: '正在暂停',
                paused: this.simulationManualOverride ? '手动接管 · 已暂停' : '赛事演示已暂停',
                resuming: '正在继续',
                stopping: '正在停止并复位',
                completed: '赛事演示已完成'
            };
            return labels[this.simulationState] || '赛事引擎状态更新中';
        },
        simulationDuration() {
            const value = Number(this.simulationStatus.duration);
            if (Number.isFinite(value) && value > 0) {
                return value;
            }
            return this.simulationConfig.mode === 'full' ? 420 : 90;
        },
        simulationRemaining() {
            const value = Number(this.simulationStatus.remaining);
            return Number.isFinite(value)
                ? Math.max(0, Math.min(this.simulationDuration, value))
                : Math.max(0, this.simulationDuration - Number(this.simulationStatus.elapsed || 0));
        },
        simulationProgressPercent() {
            const elapsed = Math.max(
                Number(this.simulationStatus.elapsed) || 0,
                this.simulationDuration - this.simulationRemaining
            );
            return Math.max(0, Math.min(100, elapsed / this.simulationDuration * 100));
        },
        simulationRecentEvents() {
            return Array.isArray(this.simulationStatus.recentEvents)
                ? this.simulationStatus.recentEvents
                : [];
        },
        redTeamHpSummary() {
            return this.buildTeamHpSummary('red');
        },
        blueTeamHpSummary() {
            return this.buildTeamHpSummary('blue');
        },
        filteredLogs() {
            return this.logs.filter(log => {
                if (log.type === 'info' && !this.logFilters.info) return false;
                if (log.type === 'command' && !this.logFilters.command) return false;
                if (log.type === 'error' && !this.logFilters.error) return false;
                if (log.type === 'video' && !this.logFilters.video) return false;
                return true;
            });
        }
    }
    });
}
