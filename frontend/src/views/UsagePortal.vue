<template>
    <div class="usage-portal-page" :class="{ 'is-mobile': isMobile }">
        <div v-if="loading" class="loading-state" v-loading="loading"></div>
        
        <div v-else-if="!isRestricted" class="error-container">
            <el-empty :description="$t('usage.redirect_admin')">
                <el-button type="primary" @click="goToLogin">{{ $t('login.login_btn') }}</el-button>
            </el-empty>
        </div>

        <div v-else class="portal-container">
            <!-- 头部信息 -->
            <div class="portal-header">
                <div class="header-left">
                    <h1 class="portal-title">{{ $t('usage.title') }}</h1>
                    <div class="portal-subtitle">
                        <span class="device-tag" v-if="usageData.device_name && usageData.device_name !== 'Administrator Panel'">
                            {{ usageData.device_name }} ({{ usageData.mac }})
                        </span>
                        <span class="device-tag admin" v-else>
                            {{ $t('groups.name_list.Admin') }} - {{ formatGroupName({ name: usageData.group_name, remark: usageData.remark }, t) }}
                        </span>
                    </div>
                </div>
                <div class="header-right">
                    <el-button type="primary" :icon="Refresh" circle @click="loadData" />
                </div>
            </div>

            <div class="portal-body">
                <!-- 第一列：圆环图 -->
                <div class="portal-card chart-card">
                    <div class="card-title">
                        {{ formatGroupName({ name: usageData.group_name, remark: usageData.remark }, t) }} 
                        <el-tag :type="usageData.limit_type === 'duration' ? 'success' : 'warning'" size="small" effect="dark">
                            {{ usageData.limit_type === 'duration' ? $t('usage.duration_mode') : $t('usage.time_slot_mode') }}
                        </el-tag>
                    </div>
                    
                    <div class="chart-wrapper">
                        <!-- ECharts 圆环图容器 -->
                        <div ref="chartRef" class="chart-container"></div>
                        <div class="chart-center-text">
                            <span class="center-percent">{{ usedPercent }}%</span>
                            <span class="center-label">{{ $t('usage.time_used') }}</span>
                        </div>
                    </div>

                    <div class="time-stats">
                        <div class="stat-item">
                            <span class="label">{{ $t('usage.time_used') }}</span>
                            <span class="value">{{ formatSeconds(usageData.used_duration) }}</span>
                        </div>
                        <div class="stat-divider"></div>
                        <div class="stat-item">
                            <span class="label">{{ $t('usage.time_remaining') }}</span>
                            <span class="value highlight">{{ formatSeconds(usageData.remaining_duration) }}</span>
                        </div>
                    </div>
                </div>

                <!-- 第二列：状态与控制卡片 -->
                <div class="portal-card control-card">
                    <div class="card-title">{{ $t('usage.status_label') }}</div>
                    
                    <!-- 一键禁网警示 -->
                    <div v-if="usageData.is_blocked_manual" class="status-alert danger">
                        <el-icon><Lock /></el-icon>
                        <span class="alert-text">{{ $t('usage.err_manual_blocked') }}</span>
                    </div>

                    <!-- 时长模式：展示控制按钮 -->
                    <div v-else-if="usageData.limit_type === 'duration'" class="control-duration-panel">
                        <div class="status-indicator-wrapper">
                            <span class="pulse-dot" :class="{ 'dot-active': usageData.status === 'running', 'dot-stopped': usageData.status !== 'running' }"></span>
                            <span class="status-text" :class="{ 'active': usageData.status === 'running' }">
                                {{ usageData.status === 'running' ? $t('usage.status_running') : $t('usage.status_stopped') }}
                            </span>
                        </div>

                        <!-- 倒计时数字显示 -->
                        <div class="countdown-clock" :class="{ 'active': usageData.status === 'running' }">
                            {{ formattedCountdown }}
                        </div>

                        <!-- 操作按钮 -->
                        <div class="control-actions">
                            <el-button 
                                type="success" 
                                size="large" 
                                class="action-btn"
                                :disabled="usageData.status === 'running' || usageData.remaining_duration <= 0" 
                                @click="handleStart"
                            >
                                {{ $t('usage.start_internet') }}
                            </el-button>
                            <el-button 
                                type="danger" 
                                size="large" 
                                class="action-btn"
                                :disabled="usageData.status !== 'running'" 
                                @click="handleStop"
                            >
                                {{ $t('usage.stop_internet') }}
                            </el-button>
                        </div>
                        
                        <div v-if="usageData.remaining_duration <= 0" class="status-alert info">
                            <el-icon><CircleClose /></el-icon>
                            <span class="alert-text">{{ $t('usage.err_exhausted') }}</span>
                        </div>
                    </div>

                    <!-- 时间段模式：展示今日时间段 -->
                    <div v-else class="control-slots-panel">
                        <div class="status-indicator-wrapper">
                            <span class="pulse-dot" :class="{ 'dot-active': usageData.status === 'running', 'dot-stopped': usageData.status !== 'running' }"></span>
                            <span class="status-text" :class="{ 'active': usageData.status === 'running' }">
                                {{ usageData.status === 'running' ? $t('usage.status_running') : $t('usage.status_stopped') }}
                            </span>
                        </div>

                        <div class="slots-timeline">
                            <div class="timeline-title">{{ $t('usage.allowed_slots') }}</div>
                            <div v-if="usageData.slots && usageData.slots.length > 0" class="slots-list">
                                <div 
                                    v-for="(slot, index) in usageData.slots" 
                                    :key="index" 
                                    class="slot-card"
                                    :class="slot.status"
                                >
                                    <div class="slot-time">{{ slot.from_at }} - {{ slot.end_at }}</div>
                                    <el-tag :type="getSlotTagType(slot.status)" size="small" effect="dark">
                                        {{ $t('usage.slot_' + slot.status) }}
                                    </el-tag>
                                </div>
                            </div>
                            <div v-else class="empty-slots">
                                {{ $t('policy.empty_slot_hint') }}
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- 下半部分：上网历史记录 -->
            <div class="portal-card history-card" v-if="usageData.limit_type === 'duration'">
                <div class="card-title">{{ $t('usage.session_history') }}</div>
                
                <el-table :data="history" class="modern-table" style="width: 100%" v-loading="loadingHistory">
                    <el-table-column prop="start_time" :label="$t('usage.start_time')" min-width="120" />
                    <el-table-column prop="end_time" :label="$t('usage.end_time')" min-width="120">
                        <template #default="scope">
                            {{ scope.row.end_time || '-' }}
                        </template>
                    </el-table-column>
                    <el-table-column prop="duration" :label="$t('usage.duration')" min-width="120" />
                    <el-table-column prop="trigger_reason" :label="$t('usage.trigger_reason')" min-width="120" />
                </el-table>
            </div>
        </div>
    </div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, computed, watch, nextTick } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { Refresh, Lock, CircleClose } from '@element-plus/icons-vue'
import { ElMessage } from 'element-plus'
import * as echarts from 'echarts'
import { useI18n } from 'vue-i18n'
import { formatGroupName } from '@/utils/common'
import { detectUsage, startInternet, stopInternet, getUsageHistory } from '@/api/usage'

const { t } = useI18n()

const router = useRouter()
const route = useRoute()

const loading = ref(true)
const isRestricted = ref(false)
const usageData = ref({})
const history = ref([])
const loadingHistory = ref(false)

const chartRef = ref(null)
let chartInstance = null
let countdownTimer = null
let autoSyncTimer = null

// 响应式屏幕检测
const isMobile = ref(false)
const checkMobile = () => {
    isMobile.value = window.innerWidth <= 768
}

// 获取 URL 参数以支持家长管理模式和本地调试
const getRequestParams = () => {
    const params = {}
    if (route.query.groups_id) {
        params.groups_id = route.query.groups_id
    }
    if (route.query.debug_ip) {
        params.debug_ip = route.query.debug_ip
    }
    return params
}

// 动态计算使用比例
const usedPercent = computed(() => {
    const total = usageData.value.total_duration || 0
    const used = usageData.value.used_duration || 0
    if (total <= 0) return 0
    const pct = Math.round((used / total) * 100)
    return Math.min(100, pct)
})

// 动态计算倒计时字串
const formattedCountdown = computed(() => {
    const rem = usageData.value.remaining_duration || 0
    const h = Math.floor(rem / 3600)
    const m = Math.floor((rem % 3600) / 60)
    const s = rem % 60
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
})

// 时间段 Tag 样式
const getSlotTagType = (status) => {
    if (status === 'active') return 'success'
    if (status === 'passed') return 'info'
    return 'primary'
}

// 格式化秒数为分钟
const formatSeconds = (sec) => {
    const mins = (!sec || sec <= 0) ? 0 : Math.ceil(sec / 60)
    return t('usage.minutes_val', { n: mins })
}

// 载入数据
const loadData = async (silent = false) => {
    if (!silent) loading.value = true
    try {
        const params = getRequestParams()
        const res = await detectUsage(params)
        
        if (res && res.is_restricted) {
            isRestricted.value = true
            usageData.value = res
            
            // 渲染饼图
            nextTick(() => {
                renderChart()
            })

            // 时长控制模式下载入上网记录（心跳静默刷新时不刷新历史记录列表，避免重复重绘）
            if (res.limit_type === 'duration' && !silent) {
                loadHistory()
            }
        } else {
            isRestricted.value = false
            // 若不是限制组设备，自动跳转管理员登录页
            goToLogin()
        }
    } catch (err) {
        console.error('Failed to detect device status:', err)
        isRestricted.value = false
    } finally {
        loading.value = false
    }
}

// 载入会话历史
const loadHistory = async () => {
    loadingHistory.value = true
    try {
        const params = getRequestParams()
        const res = await getUsageHistory(params)
        history.value = res || []
    } catch (err) {
        console.error('Failed to load usage history:', err)
    } finally {
        loadingHistory.value = false
    }
}

// 渲染 ECharts 饼图
const renderChart = () => {
    if (!chartRef.value) return
    
    if (chartInstance) {
        if (chartInstance.getDom() !== chartRef.value) {
            chartInstance.dispose()
            chartInstance = echarts.init(chartRef.value)
        }
    } else {
        chartInstance = echarts.init(chartRef.value)
    }
    
    const used = usageData.value.used_duration || 0
    const rem = usageData.value.remaining_duration || 0
    const total = usageData.value.total_duration || 0

    // 计算占比
    const data = [
        { 
            value: used, 
            name: t('usage.time_used'),
            itemStyle: {
                color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                    { offset: 0, color: '#ff2a85' },
                    { offset: 1, color: '#a012f0' }
                ])
            }
        },
        { 
            value: total <= 0 ? 1 : rem, 
            name: t('usage.time_remaining'),
            itemStyle: {
                color: rem <= 0 ? '#222530' : new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                    { offset: 0, color: '#00f6ff' },
                    { offset: 1, color: '#0066ff' }
                ])
            }
        }
    ]

    const option = {
        tooltip: {
            show: false
        },
        legend: {
            show: false
        },
        series: [
            {
                name: 'Time Limit',
                type: 'pie',
                radius: ['70%', '88%'],
                avoidLabelOverlap: false,
                label: {
                    show: false
                },
                emphasis: {
                    disabled: true
                },
                data: data
            }
        ]
    }
    
    chartInstance.setOption(option)
}

// 开始上网
const handleStart = async () => {
    try {
        const params = getRequestParams()
        const res = await startInternet(params)
        if (res && res.ok) {
            ElMessage.success('上网已开始，网络已接通')
            loadData(true)
            loadHistory()
        }
    } catch (err) {
        ElMessage.error(err.response?.data?.msg || '操作失败')
    }
}

// 结束上网
const handleStop = async () => {
    try {
        const params = getRequestParams()
        const res = await stopInternet(params)
        if (res && res.ok) {
            ElMessage.success('上网已暂停，网络已断开')
            loadData(true)
            loadHistory()
        }
    } catch (err) {
        ElMessage.error(err.response?.data?.msg || '操作失败')
    }
}

// 重定向到登录页
const goToLogin = () => {
    router.push('/login')
}

// 前端倒计时本地减秒数（保证表盘动态流动）
const startLocalCountdown = () => {
    countdownTimer = setInterval(() => {
        if (usageData.value && usageData.value.status === 'running' && usageData.value.limit_type === 'duration') {
            if (usageData.value.remaining_duration > 0) {
                usageData.value.remaining_duration--
                usageData.value.used_duration++
                renderChart()
            } else {
                usageData.value.status = 'stopped'
                loadData(true)
            }
        }
    }, 1000)
}

// 后端定时同步校准，防时钟漂移 (每10秒同步)
const startAutoSync = () => {
    autoSyncTimer = setInterval(() => {
        loadData(true)
    }, 10000)
}

// 监听窗口尺寸变化重置饼图
const handleResize = () => {
    checkMobile()
    if (chartInstance) {
        chartInstance.resize()
    }
}

onMounted(() => {
    checkMobile()
    window.addEventListener('resize', handleResize)
    loadData()
    startLocalCountdown()
    startAutoSync()
})

onBeforeUnmount(() => {
    window.removeEventListener('resize', handleResize)
    if (countdownTimer) clearInterval(countdownTimer)
    if (autoSyncTimer) clearInterval(autoSyncTimer)
    if (chartInstance) {
        chartInstance.dispose()
    }
})

// 监听路由变化（比如家长在 Tab 切换切换组）
watch(() => route.fullPath, () => {
    loadData()
})
</script>

<style scoped>
/* ==============================================================================
   磨砂玻璃科技风深色面板样式设计 (Glassmorphism Dark Tech Theme)
   ============================================================================== */
.usage-portal-page {
    min-height: 100vh;
    background: radial-gradient(circle at top left, #1a1b2e 0%, #0d0e15 100%);
    color: #e2e8f0;
    padding: 24px;
    box-sizing: border-box;
    display: flex;
    justify-content: center;
    align-items: flex-start;
}

.portal-container {
    width: 100%;
    max-width: 1200px;
    display: flex;
    flex-direction: column;
    gap: 24px;
}

/* 头部 */
.portal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16px 24px;
    background: rgba(255, 255, 255, 0.03);
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 16px;
    backdrop-filter: blur(12px);
}

.portal-title {
    font-size: 26px;
    font-weight: 700;
    margin: 0;
    background: linear-gradient(135deg, #00f6ff 0%, #a012f0 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
}

.portal-subtitle {
    margin-top: 6px;
}

.device-tag {
    display: inline-block;
    padding: 4px 10px;
    font-family: monospace;
    font-size: 13px;
    color: #00f6ff;
    background: rgba(0, 246, 255, 0.08);
    border: 1px solid rgba(0, 246, 255, 0.2);
    border-radius: 20px;
}

.device-tag.admin {
    color: #ffd04b;
    background: rgba(255, 208, 75, 0.08);
    border: 1px solid rgba(255, 208, 75, 0.2);
}

/* 布局网格 */
.portal-body {
    display: grid;
    grid-template-columns: 1fr 1.2fr;
    gap: 24px;
}

.portal-card {
    background: rgba(20, 22, 38, 0.6);
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 20px;
    padding: 24px;
    backdrop-filter: blur(20px);
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
}

.card-title {
    font-size: 18px;
    font-weight: 600;
    margin-bottom: 20px;
    color: #cbd5e1;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

/* 圆环图卡片 */
.chart-card {
    display: flex;
    flex-direction: column;
}

.chart-wrapper {
    position: relative;
    width: 260px;
    height: 260px;
    margin: 20px auto;
}

.chart-container {
    width: 100%;
    height: 100%;
}

.chart-center-text {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    display: flex;
    flex-direction: column;
    align-items: center;
    pointer-events: none;
}

.center-percent {
    font-size: 38px;
    font-weight: 800;
    color: #00f6ff;
    text-shadow: 0 0 15px rgba(0, 246, 255, 0.3);
}

.center-label {
    font-size: 12px;
    color: #94a3b8;
    margin-top: 4px;
    text-transform: uppercase;
    letter-spacing: 1px;
}

.time-stats {
    display: flex;
    justify-content: space-around;
    align-items: center;
    background: rgba(255, 255, 255, 0.02);
    border: 1px solid rgba(255, 255, 255, 0.04);
    border-radius: 14px;
    padding: 16px;
    margin-top: auto;
}

.stat-item {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
}

.stat-item .label {
    font-size: 12px;
    color: #64748b;
}

.stat-item .value {
    font-size: 18px;
    font-weight: 700;
    color: #cbd5e1;
}

.stat-item .value.highlight {
    color: #00f6ff;
    text-shadow: 0 0 10px rgba(0, 246, 255, 0.2);
}

.stat-divider {
    width: 1px;
    height: 30px;
    background: rgba(255, 255, 255, 0.08);
}

/* 控制卡片 */
.control-card {
    display: flex;
    flex-direction: column;
}

.status-indicator-wrapper {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px 16px;
    background: rgba(255, 255, 255, 0.02);
    border-radius: 10px;
    align-self: flex-start;
    margin-bottom: 24px;
}

.pulse-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    display: inline-block;
}

.dot-active {
    background: #00ff66;
    box-shadow: 0 0 10px #00ff66, 0 0 20px #00ff66;
    animation: pulse 1.5s infinite;
}

.dot-stopped {
    background: #ef4444;
    box-shadow: 0 0 10px #ef4444;
}

.status-text {
    font-size: 14px;
    font-weight: 500;
    color: #94a3b8;
}

.status-text.active {
    color: #00ff66;
}

.countdown-clock {
    font-family: monospace;
    font-size: 56px;
    font-weight: 800;
    color: #475569;
    text-align: center;
    margin: 24px 0;
    letter-spacing: 2px;
    transition: all 0.3s ease;
}

.countdown-clock.active {
    color: #00f6ff;
    text-shadow: 0 0 20px rgba(0, 246, 255, 0.4);
}

.control-actions {
    display: flex;
    gap: 16px;
    justify-content: center;
    margin-top: 16px;
}

.action-btn {
    flex: 1;
    height: 54px;
    font-size: 16px;
    font-weight: 600;
    border-radius: 12px;
}

/* 时段控制子面板 */
.slots-timeline {
    display: flex;
    flex-direction: column;
    gap: 16px;
}

.timeline-title {
    font-size: 15px;
    font-weight: 600;
    color: #64748b;
}

.slots-list {
    display: flex;
    flex-direction: column;
    gap: 12px;
}

.slot-card {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16px;
    border-radius: 12px;
    border: 1px solid rgba(255, 255, 255, 0.04);
}

.slot-card.active {
    background: rgba(0, 255, 102, 0.05);
    border: 1px solid rgba(0, 255, 102, 0.15);
}

.slot-card.scheduled {
    background: rgba(0, 246, 255, 0.04);
    border: 1px solid rgba(0, 246, 255, 0.1);
}

.slot-card.passed {
    background: rgba(255, 255, 255, 0.01);
    opacity: 0.5;
}

.slot-time {
    font-size: 16px;
    font-weight: 600;
}

/* 状态警示框 */
.status-alert {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 16px;
    border-radius: 12px;
    margin-top: 24px;
}

.status-alert.danger {
    background: rgba(239, 68, 68, 0.08);
    border: 1px solid rgba(239, 68, 68, 0.2);
    color: #fca5a5;
}

.status-alert.info {
    background: rgba(0, 246, 255, 0.05);
    border: 1px solid rgba(0, 246, 255, 0.15);
    color: #93c5fd;
}

.alert-text {
    font-size: 14px;
    font-weight: 500;
}

/* 连网历史 */
.history-card {
    margin-top: 24px;
}

/* 加载和错误页 */
.loading-state {
    width: 100%;
    max-width: 600px;
    height: 300px;
    background: rgba(20, 22, 38, 0.6);
    border-radius: 20px;
    border: 1px solid rgba(255, 255, 255, 0.06);
    backdrop-filter: blur(20px);
}

.error-container {
    padding: 60px 40px;
    background: rgba(20, 22, 38, 0.6);
    border-radius: 20px;
    border: 1px solid rgba(255, 255, 255, 0.06);
    backdrop-filter: blur(20px);
}

/* ==============================================================================
   移动端自适应适配样式 (Mobile Layout Adaptation)
   ============================================================================== */
.is-mobile {
    padding: 16px;
}

.is-mobile .portal-body {
    grid-template-columns: 1fr; /* 移动端单列排版 */
    gap: 16px;
}

.is-mobile .portal-title {
    font-size: 20px;
}

.is-mobile .countdown-clock {
    font-size: 44px;
}

.is-mobile .stat-item .value {
    font-size: 16px;
}

@keyframes pulse {
    0% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(0, 255, 102, 0.7);
    }
    70% {
        transform: scale(1);
        box-shadow: 0 0 0 10px rgba(0, 255, 102, 0);
    }
    100% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(0, 255, 102, 0);
    }
}
</style>
