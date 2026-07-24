<template>
    <div class="page admin-usage-page" :class="{ 'is-mobile': isMobile }">
        <!-- 面板标题 -->
        <div class="panel">
            <div class="panel-head">
                <div class="header-left">
                    <div class="title-row">
                        <h2 class="title">{{ $t('usage.title') }}</h2>
                        <el-button
                            v-if="isMobile"
                            class="refresh-circle"
                            circle
                            :loading="loading"
                            @click="loadActiveGroup(true)"
                        >
                            <el-icon><Refresh /></el-icon>
                        </el-button>
                    </div>
                    <div class="sub">{{ $t('usage.sub') }}</div>
                </div>
                <div class="header-right" v-if="!isMobile">
                    <el-button 
                        type="primary" 
                        :icon="Refresh" 
                        :loading="loading" 
                        @click="loadActiveGroup(true)"
                        round
                    >
                        {{ $t('common.refresh') }}
                    </el-button>
                </div>
            </div>
        </div>

        <!-- 分组 Tabs 切换 -->
        <div class="panel mobile-naked tabs-panel" v-loading="loadingGroups">
            <div class="tabs-head">
                <el-tabs v-model="activeGroupId" class="tabs" @tab-change="handleGroupChange">
                    <el-tab-pane 
                        v-for="g in restrictedGroups" 
                        :key="g.id" 
                        :label="formatGroupName(g, t)" 
                        :name="g.id" 
                    />
                </el-tabs>
            </div>

            <div class="panel-body" v-if="activeGroupId && !loading">
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

                            <div class="countdown-clock" :class="{ 'active': usageData.status === 'running' }">
                                {{ formattedCountdown }}
                            </div>

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

                <!-- 历史会话 -->
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
    </div>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, computed, nextTick } from 'vue'
import { Refresh, Lock, CircleClose } from '@element-plus/icons-vue'
import { ElMessage } from 'element-plus'
import * as echarts from 'echarts'
import { useI18n } from 'vue-i18n'
import { formatGroupName } from '@/utils/common'
import { getGroups } from '@/api/group'
import { detectUsage, startInternet, stopInternet, getUsageHistory } from '@/api/usage'

const { t } = useI18n()

const loadingGroups = ref(true)
const loading = ref(false)
const restrictedGroups = ref([])
const activeGroupId = ref(null)

const usageData = ref({})
const history = ref([])
const loadingHistory = ref(false)

const chartRef = ref(null)
let chartInstance = null
let countdownTimer = null
let autoSyncTimer = null

// 移动端自适应检测
const isMobile = ref(false)
const checkMobile = () => {
    isMobile.value = window.innerWidth <= 768
}

const usedPercent = computed(() => {
    const total = usageData.value.total_duration || 0
    const used = usageData.value.used_duration || 0
    if (total <= 0) return 0
    return Math.min(100, Math.round((used / total) * 100))
})

const formattedCountdown = computed(() => {
    const rem = usageData.value.remaining_duration || 0
    const h = Math.floor(rem / 3600)
    const m = Math.floor((rem % 3600) / 60)
    const s = rem % 60
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
})

const getSlotTagType = (status) => {
    if (status === 'active') return 'success'
    if (status === 'passed') return 'info'
    return 'primary'
}

const formatSeconds = (sec) => {
    const mins = (!sec || sec <= 0) ? 0 : Math.ceil(sec / 60)
    return t('usage.minutes_val', { n: mins })
}

// 载入受限组列表
const loadGroups = async () => {
    loadingGroups.value = true
    try {
        // kind = 2 对应受限设备组 (跟 GroupManagement 一致)
        const res = await getGroups({ kind: 2 })
        restrictedGroups.value = res || []
        if (restrictedGroups.value.length > 0) {
            activeGroupId.value = restrictedGroups.value[0].id
            await loadActiveGroup()
        }
    } catch (err) {
        console.error('Failed to load restricted groups:', err)
    } finally {
        loadingGroups.value = false
    }
}

// 载入当前组上网时长状态
const loadActiveGroup = async (silent = false) => {
    if (!activeGroupId.value) return
    if (!silent) loading.value = true
    try {
        const res = await detectUsage({ groups_id: activeGroupId.value })
        usageData.value = res || {}
        
        nextTick(() => {
            renderChart()
        })

        if (res.limit_type === 'duration' && !silent) {
            loadHistory()
        }
    } catch (err) {
        console.error('Failed to load group usage:', err)
    } finally {
        loading.value = false
    }
}

const loadHistory = async () => {
    if (!activeGroupId.value) return
    loadingHistory.value = true
    try {
        const res = await getUsageHistory({ groups_id: activeGroupId.value })
        history.value = res || []
    } catch (err) {
        console.error('Failed to load group session history:', err)
    } finally {
        loadingHistory.value = false
    }
}

// 渲染图表
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
        tooltip: { show: false },
        legend: { show: false },
        series: [
            {
                name: 'Time Limit',
                type: 'pie',
                radius: ['70%', '88%'],
                avoidLabelOverlap: false,
                label: { show: false },
                emphasis: { disabled: true },
                data: data
            }
        ]
    }
    
    chartInstance.setOption(option)
}

const handleGroupChange = () => {
    loadActiveGroup()
}

const handleStart = async () => {
    try {
        const res = await startInternet({ groups_id: activeGroupId.value })
        if (res && res.ok) {
            ElMessage.success('操作成功，网络已接通')
            loadActiveGroup(true)
            loadHistory()
        }
    } catch (err) {
        ElMessage.error(err.response?.data?.msg || '操作失败')
    }
}

const handleStop = async () => {
    try {
        const res = await stopInternet({ groups_id: activeGroupId.value })
        if (res && res.ok) {
            ElMessage.success('操作成功，网络已断开')
            loadActiveGroup(true)
            loadHistory()
        }
    } catch (err) {
        ElMessage.error(err.response?.data?.msg || '操作失败')
    }
}

const startLocalCountdown = () => {
    countdownTimer = setInterval(() => {
        if (usageData.value && usageData.value.status === 'running' && usageData.value.limit_type === 'duration') {
            if (usageData.value.remaining_duration > 0) {
                usageData.value.remaining_duration--
                usageData.value.used_duration++
                renderChart()
            } else {
                usageData.value.status = 'stopped'
                loadActiveGroup(true)
            }
        }
    }, 1000)
}

const startAutoSync = () => {
    autoSyncTimer = setInterval(() => {
        loadActiveGroup(true)
    }, 10000)
}

const handleResize = () => {
    checkMobile()
    if (chartInstance) {
        chartInstance.resize()
    }
}

onMounted(() => {
    checkMobile()
    window.addEventListener('resize', handleResize)
    loadGroups()
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
</script>

<style scoped>
.panel-body {
    padding: 0 24px 24px 24px;
}

.tabs-head {
    padding: 12px 24px 0 24px;
}

@media (max-width: 768px) {
    .panel-body {
        padding: 0 12px 12px 12px;
    }
    .tabs-head {
        padding: 8px 12px 0 12px;
    }
}

/* 后台端在 Layout 结构内渲染，因此背景采用后台的统一卡片容器样式 */
.admin-usage-page {
    width: 100%;
}

.tabs-panel {
    margin-top: 20px;
}

.portal-body {
    display: grid;
    grid-template-columns: 1fr 1.2fr;
    gap: 24px;
    margin-top: 20px;
}

.portal-card {
    background: #1e2235;
    border: 1px solid #2e354f;
    border-radius: 16px;
    padding: 24px;
}

.card-title {
    font-size: 16px;
    font-weight: 600;
    margin-bottom: 20px;
    color: #cbd5e1;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

/* 环图 */
.chart-wrapper {
    position: relative;
    width: 220px;
    height: 220px;
    margin: 10px auto 20px auto;
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
    font-size: 32px;
    font-weight: 800;
    color: #00f6ff;
}

.center-label {
    font-size: 11px;
    color: #64748b;
    margin-top: 2px;
    text-transform: uppercase;
}

.time-stats {
    display: flex;
    justify-content: space-around;
    align-items: center;
    background: #141724;
    border: 1px solid #2e354f;
    border-radius: 12px;
    padding: 16px;
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
    font-size: 16px;
    font-weight: 700;
    color: #cbd5e1;
}

.stat-item .value.highlight {
    color: #00f6ff;
}

.stat-divider {
    width: 1px;
    height: 30px;
    background: #2e354f;
}

/* 状态 */
.status-indicator-wrapper {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 10px 16px;
    background: #141724;
    border-radius: 8px;
    align-self: flex-start;
    margin-bottom: 24px;
}

.pulse-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
}

.dot-active {
    background: #00ff66;
    box-shadow: 0 0 10px #00ff66;
}

.dot-stopped {
    background: #ef4444;
    box-shadow: 0 0 10px #ef4444;
}

.status-text {
    font-size: 13px;
    font-weight: 500;
    color: #94a3b8;
}

.status-text.active {
    color: #00ff66;
}

.countdown-clock {
    font-family: monospace;
    font-size: 48px;
    font-weight: 800;
    color: #475569;
    text-align: center;
    margin: 20px 0;
    letter-spacing: 1px;
}

.countdown-clock.active {
    color: #00f6ff;
    text-shadow: 0 0 15px rgba(0, 246, 255, 0.3);
}

.control-actions {
    display: flex;
    gap: 16px;
    justify-content: center;
}

.action-btn {
    flex: 1;
    height: 50px;
    font-size: 15px;
    font-weight: 600;
}

/* 时间段列表 */
.slots-timeline {
    display: flex;
    flex-direction: column;
    gap: 16px;
}

.timeline-title {
    font-size: 14px;
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
    padding: 14px 16px;
    border-radius: 10px;
    border: 1px solid #2e354f;
    background: #141724;
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
    opacity: 0.5;
}

.slot-time {
    font-size: 15px;
    font-weight: 600;
}

/* 报警框 */
.status-alert {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 16px;
    border-radius: 10px;
    margin-top: 20px;
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
    font-size: 13px;
}

.history-card {
    margin-top: 24px;
}

/* 手机适配 */
.is-mobile .portal-body {
    grid-template-columns: 1fr;
    gap: 16px;
}

.is-mobile .countdown-clock {
    font-size: 38px;
}
</style>
