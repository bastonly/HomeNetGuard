<template>
    <div class="side-menu-wrapper">
        <div class="user-box">
            <div class="user-main">
                <el-avatar :size="44" class="avatar" :src="logoImg" />

                <div class="user-info">
                    <div class="username">{{ username }}</div>
                    <div class="role">{{ $t('user.logged_in') }}</div>
                </div>
            </div>

            <!-- 系统时间展示（手机端集成） -->
            <div class="time-info mobile-only" v-if="systemTime">
                <div class="time-item">
                    <el-icon><Calendar /></el-icon>
                    <span>{{ currentTimeDate }}</span>
                </div>
                <div class="time-item">
                    <el-icon><Clock /></el-icon>
                    <span class="clock-bold">{{ currentTimeClock }}</span>
                </div>
            </div>
        </div>

        <!-- 系统时间展示（PC端原有样式） -->
        <div class="time-bar pc-only" v-if="systemTime">
            <div class="time-item">
                <el-icon><Calendar /></el-icon>
                <span>{{ currentTimeDate }}</span>
            </div>
            <div class="time-item">
                <el-icon><Clock /></el-icon>
                <span class="clock-bold">{{ currentTimeClock }}</span>
            </div>
        </div>
        <div class="expire-bar" :class="expireClass" v-if="expireWarning">
            {{ typeof expireWarning === 'object' ? $t(expireWarning.key, expireWarning.params) : $t(expireWarning) }}
            <el-tooltip v-if="isInGrace" :content="$t(graceReason)" placement="top">
                <el-icon class="grace-help-icon"><QuestionFilled /></el-icon>
            </el-tooltip>
        </div>

        <el-menu
            class="side-menu"
            router
            :default-active="route.path"
            @select="$emit('select')"
    >
        <el-menu-item index="/">
            <el-icon><HomeFilled /></el-icon>
            <span>{{ $t('menu.home') }}</span>
        </el-menu-item>

        <el-menu-item index="/devices">
            <el-icon><Monitor /></el-icon>
            <span>{{ $t('menu.devices') }}</span>
        </el-menu-item>

        <el-menu-item index="/groups">
            <el-icon><Collection /></el-icon>
            <span>{{ $t('menu.groups') }}</span>
        </el-menu-item>

        <el-menu-item index="/groups/usage">
            <el-icon><Timer /></el-icon>
            <span>{{ $t('menu.groups_usage') }}</span>
        </el-menu-item>

        <el-menu-item index="/policy">
            <el-icon><Timer /></el-icon>
            <span>{{ $t('menu.policy') }}</span>
        </el-menu-item>

        <el-menu-item index="/temp">
            <el-icon><Clock /></el-icon>
            <span>{{ $t('menu.temp') }}</span>
        </el-menu-item>

        <el-menu-item index="/holiday">
            <el-icon><Calendar /></el-icon>
            <span>{{ $t('menu.holiday') }}</span>
        </el-menu-item>

        <el-menu-item index="/messages" class="menu-item-audit">
            <el-icon><Warning /></el-icon>
            <span class="menu-label">
                {{ $t('menu.audit') }}
                <el-badge v-if="unreadCount > 0" :value="unreadCount" :max="99" class="unread-badge" />
            </span>
        </el-menu-item>

        <el-menu-item index="/stats">
            <el-icon><DataAnalysis /></el-icon>
            <span>{{ $t('menu.stats') }}</span>
        </el-menu-item>

        <el-menu-item index="/entertainment">
            <el-icon><PieChart /></el-icon>
            <span>{{ $t('menu.entertainment') }}</span>
        </el-menu-item>

        <el-menu-item index="/settings">
            <el-icon><Setting /></el-icon>
            <span>{{ $t('menu.settings') }}</span>
        </el-menu-item>

        <el-menu-item index="/logout" @click="logout" class="logout-menu-item">
            <el-icon><SwitchButton /></el-icon>
            <span>{{ $t('menu.logout') }}</span>
        </el-menu-item>
    </el-menu>
        
        <!-- ===== 底部版本与版权 ===== -->
        <div class="menu-footer">
            <div class="footer-top">
                <div class="lang-group">
                    <span :class="['lang-item', { 'is-active': locale === 'zh' }]" @click="setLanguage('zh')">中文</span>
                    <span class="lang-divider">/</span>
                    <span :class="['lang-item', { 'is-active': locale === 'en' }]" @click="setLanguage('en')">English</span>
                </div>
            </div>
            
            <div class="footer-bottom">
                <div class="footer-info">
                    <div class="version">
                        v{{ apiVersion }}
                        <div class="status-badges">
                            <span class="license-status" :class="licenseClass" v-if="licenseStatus">
                                {{ $t(licenseText) }}
                            </span>
                            <!-- <span class="license-status" :class="deviceClass" v-if="deviceStatus">
                                {{ $t(deviceText) }}
                            </span> -->
                        </div>
                    </div>
                    <div class="copyright">
                        {{ $t('footer.copyright') }}
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script setup>
    import { useRoute, useRouter } from 'vue-router'
    import logoImg from '@/assets/logo.png'
    import http from '@/utils/http'
    import bus from '@/utils/bus'
    import { useI18n } from 'vue-i18n'
    import { ref, computed, onMounted, onUnmounted } from 'vue'
    import { ElMessageBox, ElMessage } from 'element-plus'
    import { 
        Warning, HomeFilled, Monitor, Collection, Timer, Clock, 
        Calendar, DataAnalysis, SwitchButton, House, PieChart, 
        Setting, QuestionFilled 
    } from '@element-plus/icons-vue'
    import { getLicenseStatusMeta, getDeviceStatusMeta, checkIsOffline, parseToDate } from '@/utils/common'

    const route = useRoute()
    const router = useRouter()
    const { locale } = useI18n()
    const username = ref('...')
    const unreadCount = ref(0)
    const licenseStatus = ref('')
    const licenseClass = ref('')
    const licenseText = ref('')
    
    const deviceStatus = ref('')
    const deviceClass = ref('')
    const deviceText = ref('')
    const apiVersion = ref(localStorage.getItem('api_version') || '...')
    const systemTime = ref('')
    const currentTimeDate = computed(() => {
        if (!systemTime.value) return ''
        return systemTime.value.split(' ')[0]
    })
    const currentTimeClock = computed(() => {
        if (!systemTime.value) return ''
        return systemTime.value.split(' ')[1]
    })
    const expireWarning = ref('')
    const expireClass = ref('') // 用于控制 expire-bar 样式的响应式变量
    const graceReason = ref('')
    const isInGrace = ref(false)
    let poller = null
    let clockTimer = null
    // 全局单例状态备份 (处理组件重挂载)
    const cachedLicenseData = ref(null)
    const lastLoadTime = ref(0)

    onMounted(() => {
        // 增加一个延时或重試機制，確保能讀到
        const name = localStorage.getItem('username')
        username.value = name || '管理员'
        
        loadUnreadCount()
        loadLicenseStatus()
        startClock()
        bus.on('refresh-unread', loadUnreadCount)
    })
    
    // 清理定时器
    onUnmounted(() => {
        if (poller) clearInterval(poller)
        if (clockTimer) clearInterval(clockTimer)
        bus.off('refresh-unread', loadUnreadCount)
    })

    const loadUnreadCount = async () => {
        // 频繁切换页面时避免并发
        if (unreadCount.loading) return
        unreadCount.loading = true
        try {
            const res = await http.get('/api/messages/security/unread_count')
            if (res && res.ok) {
                unreadCount.value = res.count
            }
        } catch (e) {
            console.error('Fetch unread count failed', e)
        } finally {
            unreadCount.loading = false
        }
    }

    const loadLicenseStatus = async () => {
        // 缓存 30 秒内的请求
        const now = Date.now()
        if (cachedLicenseData.value && (now - lastLoadTime.value < 30000)) {
            updateStatusUI(cachedLicenseData.value)
            return
        }

        try {
            const res = await http.get('/api/settings')
            if (res && res.ok && res.data && res.data.SERVER) {
                const srv = res.data.SERVER
                cachedLicenseData.value = srv
                lastLoadTime.value = now
                updateStatusUI(srv)
                
                if (srv.CURRENT_API_VERSION) {
                    apiVersion.value = srv.CURRENT_API_VERSION
                    localStorage.setItem('api_version', srv.CURRENT_API_VERSION)
                }
            }
        } catch (e) {
            console.error('Fetch license status failed', e)
        }
    }

    const startClock = () => {
        updateClock()
        // 每 10 分钟刷新一次系统时间
        clockTimer = setInterval(updateClock, 600000)
    }

    const updateClock = async () => {
        try {
            const res = await http.get('/api/settings')
            if (res && res.ok && res.data) {
                if (res.data.SYSTEM_TIME) systemTime.value = res.data.SYSTEM_TIME
                if (res.data.SERVER && res.data.SERVER.CURRENT_API_VERSION) {
                    apiVersion.value = res.data.SERVER.CURRENT_API_VERSION
                    localStorage.setItem('api_version', res.data.SERVER.CURRENT_API_VERSION)
                }
            }
        } catch (e) {
            console.error('Update clock failed', e)
        }
    }

    const updateStatusUI = (serverData) => {
        const status = serverData.LICENSE_STATUS
        licenseStatus.value = status
        
        const isOffline = checkIsOffline(serverData.LAST_CHECKIN_TIME)
        
        expireWarning.value = ''
        expireClass.value = '' 

        const deviceStatus = serverData.DEVICE_STATUS || 'unknown'
        const isFullyActive = status === 'active' && deviceStatus === 'active'

        if (serverData.GRACE_START_TIME && !isFullyActive && deviceStatus !== 'unbound') {
            try {
                const graceMax = serverData.GRACE_PERIOD_MAX || 30
                let remainingDays = 0
                
                // 优先使用后端预计算好的剩余天数
                if (serverData.GRACE_REMAINING_DAYS !== undefined) {
                    remainingDays = parseInt(serverData.GRACE_REMAINING_DAYS)
                } else if (serverData.GRACE_START_TS) {
                    const startTs = parseInt(serverData.GRACE_START_TS)
                    if (startTs > 0) {
                        const now = Math.floor(Date.now() / 1000)
                        const diffSec = now - startTs
                        const graceMaxSec = graceMax * 24 * 3600 // Use graceMax from serverData
                        if (diffSec >= graceMaxSec) {
                            remainingDays = -1
                        } else {
                            remainingDays = Math.floor((graceMaxSec - diffSec) / 86400)
                        }
                    }
                }
                
                isInGrace.value = true
                const reason = serverData.GRACE_REASON || 'cpp.GRACE_REASON_INACTIVE'
                if (remainingDays < 0) {
                    expireWarning.value = 'license.grace_expired'
                } else if (remainingDays === 0) {
                    expireWarning.value = 'license.grace_today'
                } else {
                    expireWarning.value = { key: 'license.grace_remaining', params: { n: remainingDays } }
                }
                graceReason.value = reason

                // 统一颜色逻辑：<=10红，<=20黄，<=graceMax 绿
                if (remainingDays <= 10) {
                    expireClass.value = 'expire-danger'
                } else if (remainingDays <= 20) {
                    expireClass.value = 'expire-warning'
                } else {
                    expireClass.value = 'expire-success'
                }
            } catch (e) {
                console.error('Calculate grace period failed', e)
            }
        } else {
            isInGrace.value = false
            graceReason.value = ''
        }
        
        // 2. 如果没有宽限期提醒，再检查普通的过期提醒
        if (!expireWarning.value && status === 'active' && serverData.EXPIRED_DATE) {
            try {
                // Use server-synced time if available to prevent local clock tampering
                const serverTimeStr = serverData.SERVER_TIME ? serverData.SERVER_TIME.split(' ')[0] : null
                const expDateStr = serverData.EXPIRED_DATE.split(' ')[0]
                
                const expTime = parseToDate(expDateStr)
                const now = serverTimeStr ? parseToDate(serverTimeStr) : new Date()

                if (expTime && now) {
                    // Normalize both to midnight
                    expTime.setHours(0, 0, 0, 0)
                    now.setHours(0, 0, 0, 0)

                    const diffDays = Math.round((expTime - now) / (1000 * 60 * 60 * 24))
                    
                    // 只要剩余天数 <= 30，就显示提醒并按颜色区分
                    if (diffDays <= 30) {
                        if (diffDays > 0) {
                            expireWarning.value = { key: 'license.expiry_remaining', params: { n: diffDays } }
                            // 到期提醒颜色逻辑也保持一致: 30(绿), 20(黄), 10(红)
                            if (diffDays <= 10) {
                                expireClass.value = 'expire-danger'
                            } else if (diffDays <= 20) {
                                expireClass.value = 'expire-warning'
                            } else {
                                expireClass.value = 'expire-success'
                            }
                        } else if (diffDays === 0) {
                            expireWarning.value = 'license.expiry_today'
                            expireClass.value = 'expire-danger'
                        } else {
                            expireWarning.value = 'license.license_expired'
                            expireClass.value = 'expire-danger'
                        }
                    }
                }
            } catch (e) {
                console.error('Expire date parsing error:', e)
            }
        }

        // 3. 使用公共工具类映射 授权与设备 状态文字和样式
        const lMeta = getLicenseStatusMeta(status, isOffline)
        licenseText.value = lMeta.label
        licenseClass.value = lMeta.class

        const dStatus = serverData.DEVICE_STATUS || 'unknown'
        deviceStatus.value = dStatus
        const dMeta = getDeviceStatusMeta(dStatus)
        deviceText.value = dMeta.label
        deviceClass.value = dMeta.class

        if (serverData.CURRENT_API_VERSION) {
            apiVersion.value = serverData.CURRENT_API_VERSION
        }
    }

    const logout = async () => {
        try {
            await ElMessageBox.confirm(
                locale.value === 'zh' ? '确定要退出登录吗？' : 'Are you sure you want to logout?',
                locale.value === 'zh' ? '确认退出' : 'Logout',
                {
                    confirmButtonText: locale.value === 'zh' ? '退出' : 'Logout',
                    cancelButtonText: locale.value === 'zh' ? '取消' : 'Cancel',
                    type: 'warning',
                }
            )
        } catch {
            return
        }

        try {
            await http.post('/api/logout')
        } catch (e) {
            // 忽略后端异常，继续清理
        }
        // 清理前端缓存
        localStorage.clear()
        sessionStorage.clear()
        
        ElMessage.success(locale.value === 'zh' ? '已退出' : 'Logged out')
        router.replace('/login')
    }

    const setLanguage = (lang) => {
        if (locale.value === lang) return
        locale.value = lang
        localStorage.setItem('language', lang)
        ElMessage.success(lang === 'zh' ? '已切换至中文' : 'Language switched to English')
    }

</script>

<style scoped>
    .side-menu {
        width: 100% !important;
        flex: 1;
        overflow-y: auto;
        overflow-x: hidden;
        border-right: none;
        display: flex;
        flex-direction: column;
        padding: 12px; /* Add padding around the list */
        gap: 2px; /* Flex gap between items */
        list-style: none !important; /* Ensure no bullets */
        background-color: var(--bg-page);
    }

    :deep(.el-menu) {
        border-right: none;
        background-color: var(--bg-page);
    }

    :deep(.el-menu-item) {
        color: var(--text-regular);
    }

    :deep(.el-menu-item.is-active) {
        color: var(--primary-color);
        background-color: var(--border-light);
    }

    :deep(.el-menu-item:hover) {
        background-color: var(--border-light);
    }

    .side-menu-wrapper {
        display: flex;
        flex-direction: column;
        height: 100%;
        width: 100%;
        background-color: var(--bg-page);
        color: var(--text-primary);
    }

    /* ===== 用户区 ===== */
    .user-box {
        display: flex;
        align-items: center;
        justify-content: flex-start; /* PC端默认靠左 */
        gap: 12px;
        padding: 16px 14px;
        border-bottom: 1px solid var(--border-light);
        width: 100%;
        box-sizing: border-box;
        background-color: var(--bg-page);
        flex-shrink: 0;
    }

    .user-main {
        display: flex;
        align-items: center;
        gap: 12px;
    }

    .avatar {
        background: var(--divider-color);
        color: var(--text-primary);
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .avatar .el-icon {
        font-size: 24px;
    }

    .user-info {
        display: flex;
        flex-direction: column;
    }

    .username {
        font-weight: 700;
        font-size: 14px;
        color: var(--text-primary) !important;
    }

    .role {
        font-size: 11px;
        color: var(--text-regular);
    }

    /* ===== 系统时间展示（集成样式 - 默认隐藏） ===== */
    .time-info {
        display: none; /* PC端默认隐藏集成版 */
        flex-direction: column;
        align-items: center; /* 水平居中 */
        gap: 2px;
        transition: all 0.3s;
    }

    /* ===== 系统时间展示（PC端原有工具栏样式） ===== */
    .time-bar {
        display: flex;
        flex-direction: row;
        align-items: center;
        justify-content: center;
        gap: 12px;
        padding: 8px 14px;
        background-color: var(--bg-subtle);
        border-bottom: 1px solid var(--border-light);
        flex-shrink: 0;
        transition: all 0.3s;
    }

    .time-item {
        display: flex;
        align-items: center;
        gap: 4px;
        font-size: 11px;
        color: var(--text-regular);
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    }

    /* 手机端集成版时间数字稍微调小一点 */
    .time-info .time-item {
        font-size: 10px;
        color: var(--text-secondary);
    }

    .time-item .el-icon {
        font-size: 13px;
        color: var(--text-secondary);
    }

    .clock-bold {
        font-weight: 600;
        color: var(--text-primary);
        font-variant-numeric: tabular-nums; /* 确保数字宽度一致，防止跳动 */
        letter-spacing: 0.2px;
    }

    /* ===== 授权到期警告行 ===== */
    .expire-bar {
        font-size: 11px;
        color: var(--text-regular);
        background-color: var(--divider-color);
        text-align: center;
        padding: 4px 14px;
        border-bottom: 1px solid var(--border-light);
        font-weight: bold;
        flex-shrink: 0;
        transition: all 0.3s;
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 4px;
    }

    .grace-help-icon {
        cursor: pointer;
        font-size: 14px;
        color: inherit;
        opacity: 0.8;
    }

    /* 宽限期颜色预警 */
    .expire-success {
        color: #67c23a;
        background-color: #f0f9eb;
    }

    .expire-warning {
        color: #e6a23c;
        background-color: #fdf6ec;
    }

    .expire-danger {
        color: #f56c6c;
        background-color: #fef0f0;
    }

    /* ===== 菜单项：手机端大按钮 ===== */
    .el-menu-item {
        width: 100%; 
        box-sizing: border-box;
        height: 50px;                 /* Slightly compact height */
        line-height: normal;          /* Use flex alignment instead of line-height */
        font-size: 15px;
        margin-bottom: 2px;           /* Spacing between items */
        border-radius: 8px;           /* Rounded corners (Pill style) */
        display: flex;                /* Ensure flex layout */
        align-items: center;          /* Vertical center */
    }

    .el-menu-item .el-icon {
        font-size: 20px;              /* ⬅️ 图标变大 */
        margin-right: 10px;
    }

    /* ===== 底部区域 ===== */
    .menu-footer {
        background-color: var(--bg-page);
        padding: 4px 16px 12px;
        border-top: 1px solid var(--border-light);
        flex-shrink: 0;
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .footer-top {
        display: flex;
        justify-content: center;
        width: 100%;
        padding: 4px 0;
    }

    .logo-container {
        padding: 20px 0;
        text-align: center;
        border-bottom: 1px solid var(--border-light);
        margin-bottom: 10px;
    }

    .footer-bottom {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
    }

    .footer-info {
        display: flex;
        flex-direction: column;
        gap: 4px;
        flex: 1;
        min-width: 0;
    }

    .version {
        text-align: left;
        font-size: 12px;
        color: #606266;
        font-family: monospace;
        display: flex;
        align-items: center;
    }

    .copyright {
        text-align: left;
        font-size: 10px;
        color: #909399;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
        transform: scale(0.95);
        transform-origin: left center;
    }

    .footer-actions {
        display: flex;
        gap: 8px;
        align-items: center;
    }

    /* 语言组样式 */
    .lang-group {
        display: flex;
        align-items: center;
        gap: 8px;
        font-size: 11px;
        color: var(--text-regular);
        padding: 4px 6px;
        background: var(--divider-color);
        border-radius: 8px;
    }

    .lang-item {
        color: #409EFF; /* 默认蓝色，表示可点击 */
        cursor: pointer;
        transition: color 0.2s;
    }
    
    .lang-item:hover {
        color: #66b1ff;
    }

    /* 当前活跃语言显示灰色且不可点击 */
    .lang-item.is-active {
        color: #c0c4cc !important;
        cursor: default;
        font-weight: normal;
    }
    
    .lang-item.is-active:hover {
        color: #c0c4cc !important;
    }

    .lang-divider {
        color: #dcdfe6;
        font-size: 10px;
    }

    .logout-btn {
        width: 32px;
        height: 32px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 8px;
        color: var(--text-regular);
        cursor: pointer;
        transition: all 0.2s;
        background: var(--divider-color);
    }
    
    .logout-btn:hover {
        background: #ffe6e6;
        color: #f56c6c;
    }
    
    .logout-btn .el-icon {
        font-size: 18px;
    }

    /* ===== 手机端优化 ===== */
    @media (max-width: 768px) {
        .user-box {
            justify-content: space-between;
        }
        
        .pc-only {
            display: none !important;
        }
        
        .mobile-only {
            display: flex !important;
        }

        .el-menu-item .el-icon {
            font-size: 22px;
        }

        .time-info {
            display: flex !important;
        }

        .time-bar {
            display: none !important;
        }
    }
    
    .menu-label {
        flex: 1;
        display: flex;
        align-items: center;
        justify-content: space-between;
        line-height: normal;
        padding-right: 4px;
    }

    .unread-badge {
        display: flex;
        align-items: center; 
    }

    :deep(.unread-badge .el-badge__content) {
        border: none;
        top: 0; 
        right: 0;
        transform: none;
        position: static;
    }

    .status-badges {
        display: flex;
        gap: 4px;
        margin-left: 6px;
    }

    .license-status {
        font-size: 10px;
        padding: 1px 4px;
        border-radius: 4px;
        font-weight: normal;
        white-space: nowrap;
    }
    
    .status-active {
        background-color: #f0f9eb;
        color: #67c23a;
        border: 1px solid #e1f3d8;
    }

    .status-warning {
        background-color: #fdf6ec;
        color: #e6a23c;
        border: 1px solid #faecd8;
    }
    
    .status-inactive {
        background-color: #fef0f0;
        color: #f56c6c;
        border: 1px solid #fde2e2;
        cursor: pointer;
    }
    
    .status-expired {
        background-color: #fdf6ec;
        color: #e6a23c;
        border: 1px solid #faecd8;
    }
    
    .status-error {
        background-color: #fef0f0;
        color: #f56c6c;
        border: 1px solid #fde2e2;
    }
    
    .status-info {
        background-color: #f4f4f5;
        color: #909399;
        border: 1px solid #e9e9eb;
    }
</style>
