<template>
    <div class="page entertainment-page">
        <!-- Header -->
        <div class="header-row">
            <div class="header-left">
                <div class="title-row">
                    <h2 class="title">{{ $t('entertainment.title') }}</h2>
                    <div style="display: flex; gap: 8px;" v-if="isMobile">
                        <el-button
                            class="refresh-circle"
                            :loading="detecting"
                            @click="handleManualDetect"
                            :icon="Refresh"
                            type="success"
                            plain
                            circle
                        />
                        <el-button
                            class="refresh-circle"
                            :loading="loading"
                            @click="loadData"
                            :icon="Refresh"
                            circle
                        />
                    </div>
                </div>
                <div class="sub">{{ $t('entertainment.sub') }}</div>
            </div>

            <div class="header-right" v-if="!isMobile">
                <el-button 
                    type="success" 
                    :loading="detecting" 
                    @click="handleManualDetect" 
                    :icon="Refresh"
                    plain
                >
                    {{ $t('entertainment.sync_now') }}
                </el-button>
                <el-button type="primary" :loading="loading" @click="loadData" :icon="Refresh">
                    {{ $t('common.refresh') }}
                </el-button>
            </div>
        </div>

        <!-- KPI -->
        <div class="kpi-grid">
            <div class="kpi-card kpi-video">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('entertainment.video_visits') }}</div>
                    <div class="kpi-value">{{ stats.videoCount || 0 }} <span class="unit">{{ $t('audit.occurrence_unit') }}</span></div>
                </div>
                <el-icon class="kpi-icon"><VideoPlay /></el-icon>
            </div>

            <div class="kpi-card kpi-gaming">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('entertainment.gaming_visits') }}</div>
                    <div class="kpi-value">{{ stats.gamingCount || 0 }} <span class="unit">{{ $t('audit.occurrence_unit') }}</span></div>
                </div>
                <el-icon class="kpi-icon"><Cpu /></el-icon>
            </div>

            <div class="kpi-card kpi-device">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('entertainment.active_devices') }}</div>
                    <div class="kpi-value">{{ stats.deviceCount || 0 }} <span class="unit">{{ $t('device.unit_set') }}</span></div>
                </div>
                <el-icon class="kpi-icon"><Monitor /></el-icon>
            </div>
        </div>

        <!-- Filters -->
        <el-card shadow="never" class="card filter-card">
            <el-form :inline="!isMobile" :model="filters" class="filter-form" :label-position="isMobile ? 'left' : 'right'" label-suffix=":">
                <!-- Mobile: Stacked Layout -->
                <div class="filter-col-mobile" v-if="isMobile">
                    <div class="filter-row">
                        <el-form-item :label="$t('entertainment.device')" class="filter-item-full">
                            <el-select v-model="filters.device_id" :placeholder="$t('entertainment.all_devices')" clearable size="small">
                                <el-option v-for="d in devices" :key="d.id" :label="d.name" :value="d.id" />
                            </el-select>
                        </el-form-item>
                    </div>

                    <div class="filter-row">
                        <el-form-item :label="$t('entertainment.type')" class="filter-item-full">
                            <el-radio-group v-model="filters.category" size="small" class="fill-width-radio">
                                <el-radio-button label="" value="">{{ $t('entertainment.category.all') }}</el-radio-button>
                                <el-radio-button label="video" value="video">{{ $t('entertainment.category.video') }}</el-radio-button>
                                <el-radio-button label="gaming" value="gaming">{{ $t('entertainment.category.gaming') }}</el-radio-button>
                            </el-radio-group>
                        </el-form-item>
                    </div>

                    <div class="filter-row filter-row-last date-row-mobile">
                        <el-form-item class="date-item-mobile">
                            <el-date-picker
                                v-model="filters.startDate"
                                type="date"
                                :placeholder="$t('entertainment.start')"
                                value-format="YYYY-MM-DD"
                                size="small"
                                @change="updateDateRange"
                            />
                        </el-form-item>
                        <el-form-item class="date-item-mobile">
                            <el-date-picker
                                v-model="filters.endDate"
                                type="date"
                                :placeholder="$t('entertainment.end')"
                                value-format="YYYY-MM-DD"
                                size="small"
                                @change="updateDateRange"
                            />
                        </el-form-item>
                        <div class="filter-actions-mobile">
                            <el-button type="primary" :icon="Search" @click="loadDetections" size="small" />
                            <el-button :icon="Refresh" @click="resetFilters" size="small" />
                        </div>
                    </div>
                </div>

                <!-- PC Layout (keeps original or similar for consistency) -->
                <template v-if="!isMobile">
                    <el-form-item :label="$t('entertainment.device')" class="filter-item-device">
                        <el-select v-model="filters.device_id" :placeholder="$t('entertainment.all_devices')" clearable>
                            <el-option v-for="d in devices" :key="d.id" :label="d.name" :value="d.id" />
                        </el-select>
                    </el-form-item>
                    <el-form-item :label="$t('entertainment.type')" class="filter-item-type">
                        <el-radio-group v-model="filters.category">
                            <el-radio-button value="">{{ $t('entertainment.category.all') }}</el-radio-button>
                            <el-radio-button value="video">{{ $t('entertainment.category.video') }}</el-radio-button>
                            <el-radio-button value="gaming">{{ $t('entertainment.category.gaming') }}</el-radio-button>
                        </el-radio-group>
                    </el-form-item>
                    <el-form-item :label="$t('entertainment.date')" class="filter-item-date">
                        <el-date-picker
                            v-model="filters.dateRange"
                            type="daterange"
                            range-separator="-"
                            :start-placeholder="$t('entertainment.start')"
                            :end-placeholder="$t('entertainment.end')"
                            value-format="YYYY-MM-DD"
                        />
                    </el-form-item>
                    <el-form-item class="filter-actions-pc">
                        <el-button type="primary" @click="loadDetections" round>{{ $t('audit.query') }}</el-button>
                        <el-button @click="resetFilters" round>{{ $t('common.reset') }}</el-button>
                    </el-form-item>
                </template>
            </el-form>
        </el-card>

        <!-- Data Table -->
        <el-card shadow="never" :class="['card', 'table-card', { 'mobile-naked': isMobile }]">
            <el-table
                v-if="!isMobile"
                :data="pagedDetections"
                v-loading="loading"
                class="modern-table"
                style="width: 100%"
            >
                <el-table-column :label="$t('entertainment.device')" min-width="200">
                    <template #default="scope">
                        <div class="device-cell">
                            <el-icon class="device-type-icon" :style="{ color: getDeviceTypeColor(scope.row.device_type) }">
                                <component :is="getDeviceTypeIcon(scope.row.device_type)"/>
                            </el-icon>
                            <div class="device-info">
                                <span class="device-name">{{ scope.row.device_name }}</span>
                                <el-tag 
                                    v-if="scope.row.group_name" 
                                    :style="getGroupStyle(scope.row.group_id)" 
                                    class="group-tag" 
                                    effect="light" 
                                    size="small" 
                                    round
                                >
                                    {{ groupLabelById(scope.row.group_id) }}
                                </el-tag>
                            </div>
                        </div>
                    </template>
                </el-table-column>
                <el-table-column prop="category" :label="$t('entertainment.type')" width="80">
                    <template #default="scope">
                        <el-tag :type="scope.row.category === 'video' ? 'danger' : 'warning'" size="small" effect="plain">
                            {{ scope.row.category === 'video' ? $t('entertainment.category.video') : $t('entertainment.category.gaming') }}
                        </el-tag>
                    </template>
                </el-table-column>
                <el-table-column prop="detection_date" :label="$t('entertainment.date')" width="110" sortable />
                <el-table-column :label="$t('entertainment.visit_target')" min-width="240">
                    <template #default="scope">
                        <div class="site-cell">
                            <span class="site-name">{{ scope.row.site_name || scope.row.domain }}</span>
                            <code class="site-domain" v-if="scope.row.site_name">{{ scope.row.domain }}</code>
                        </div>
                    </template>
                </el-table-column>
                <el-table-column prop="visit_count" :label="$t('entertainment.visit_count')" width="80" align="center" sortable />
                <el-table-column :label="$t('entertainment.visit_period')" width="220">
                    <template #default="scope">
                        <div class="time-range-pc">
                            <div class="time-row">
                                <span class="time-label"></span>
                                <span class="time-text">{{ formatTime(scope.row.first_seen) }}</span>
                            </div>
                            <div class="time-row">
                                <span class="time-label"></span>
                                <span class="time-text">{{ formatTime(scope.row.last_seen) }}</span>
                            </div>
                        </div>
                    </template>
                </el-table-column>
                <el-table-column :label="$t('common.actions')" width="100" align="center">
                    <template #default="scope">
                        <el-button 
                            :type="scope.row.is_blocked ? 'info' : 'danger'" 
                            size="small" 
                            :plain="!scope.row.is_blocked" 
                            @click="toggleBlock(scope.row)"
                        >
                            {{ scope.row.is_blocked ? $t('entertainment.blocked') : $t('entertainment.block') }}
                        </el-button>
                    </template>
                </el-table-column>
            </el-table>

            <!-- Mobile List -->
            <div v-else class="mobile-list" v-loading="loading">
                <div v-for="(r, idx) in pagedDetections" :key="idx" class="m-card">
                    <div class="m-card-header">
                        <div class="m-device">
                            <el-icon class="m-type-icon" :style="{ color: getDeviceTypeColor(r.device_type) }">
                                <component :is="getDeviceTypeIcon(r.device_type)"/>
                            </el-icon>
                            <span class="m-device-name">{{ r.device_name }}</span>
                            <el-tag v-if="r.group_name && r.group_name !== 'N/A'"
                                    :style="getGroupStyle(r.group_id)"
                                    size="small"
                                    round>
                                {{ groupLabelById(r.group_id) }}
                            </el-tag>
                        </div>
                        <div class="m-card-header-right">
                            <el-tag :type="r.category === 'video' ? 'danger' : 'warning'" size="small" effect="light" round>
                                {{ r.category === 'video' ? $t('entertainment.category.video') : $t('entertainment.category.gaming') }}
                            </el-tag>
                            <el-button 
                                :type="r.is_blocked ? 'info' : 'danger'" 
                                size="small" 
                                round
                                class="m-block-btn"
                                @click="toggleBlock(r)"
                            >
                                {{ r.is_blocked ? $t('entertainment.blocked') : $t('entertainment.block') }}
                            </el-button>
                        </div>
                    </div>
                    
                    <div class="m-card-body">
                        <div class="m-site-info">
                            <div class="m-site-name">{{ r.site_name || r.domain }}</div>
                            <code class="m-site-domain">{{ r.domain }}</code>
                        </div>
                        
                        <div class="m-meta">
                            <div class="m-meta-item">
                                <span class="m-meta-label">{{ $t('entertainment.last_visit') }}</span>
                                <span class="m-meta-value">{{ formatTime(r.last_seen) }}</span>
                            </div>
                            <div class="m-meta-item">
                                <span class="m-meta-label">{{ $t('entertainment.visit_count') }}</span>
                                <span class="m-meta-value highlight">{{ r.visit_count }} {{ $t('audit.occurrence_unit') }}</span>
                            </div>
                        </div>
                    </div>

                </div>
                <div v-if="!loading && detections.length === 0" class="m-empty">
                    <el-empty :description="$t('entertainment.no_records')" :image-size="60" />
                </div>
            </div>

            <div class="pagination-box" v-if="detections.length > 0">
                <el-pagination
                    v-model:current-page="page"
                    v-model:page-size="pageSize"
                    :total="total"
                    :size="isMobile ? 'small' : 'default'"
                    :pager-count="isMobile ? 5 : 7"
                    :page-sizes="[10, 20, 50, 100]"
                    :layout="isMobile ? 'total, prev, pager, next' : 'total, sizes, prev, pager, next, jumper'"
                    @current-change="(val) => page = val"
                    @size-change="(val) => { pageSize = val; page = 1 }"
                />
            </div>
        </el-card>
    </div>
</template>

<script setup>
import { ref, onMounted, reactive, onUnmounted, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { 
    getDetections, 
    triggerDetection,
    getEntertainmentStats,
    blockDomain, 
    unblockDomain 
} from '@/api/entertainment'
import { getGroups } from '@/api/group'
import { listDevices } from '@/api/device'
import http from '@/utils/http'
import { getDeviceTypeIcon, getDeviceTypeColor, formatGroupName, checkIsLicenseExpired } from '@/utils/common.js'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Refresh, VideoPlay, Cpu, Monitor, Search } from '@element-plus/icons-vue'

/* ================= state ================= */
const { t } = useI18n()
const loading = ref(false)
const detecting = ref(false)
const devices = ref([])
const detections = ref([])
const total = ref(0)
const page = ref(1)
const pageSize = ref(20)
const groupsAll = ref([])
const serverData = ref(null)

const pagedDetections = computed(() => {
    const start = (page.value - 1) * pageSize.value
    return detections.value.slice(start, start + pageSize.value)
})

const stats = reactive({
    videoCount: 0,
    gamingCount: 0,
    deviceCount: 0
})

const filters = reactive({
    device_id: null,
    category: '',
    dateRange: [],
    // 移动端专用独立绑定
    startDate: '',
    endDate: ''
})

const updateDateRange = () => {
    filters.dateRange = [filters.startDate || '', filters.endDate || '']
}

/* ================= responsive ================= */
const isMobile = ref(window.innerWidth < 768)
const onResize = () => {
    isMobile.value = window.innerWidth < 768
}

/* ================= lifecycle ================= */
onMounted(() => {
    window.addEventListener('resize', onResize)
    loadServerData()
    loadGroups()
    loadDevices()
    loadStats()
    loadDetections()
})

onUnmounted(() => {
    window.removeEventListener('resize', onResize)
})

/* ================= methods ================= */
const loadServerData = async () => {
    try {
        const res = await http.get('/api/settings')
        if (res && res.data && res.data.SERVER) {
            serverData.value = res.data.SERVER
        }
    } catch (e) {
        console.error('Failed to fetch server data', e)
    }
}

const loadGroups = async () => {
    try {
        const res = await getGroups()
        const arr = Array.isArray(res) ? res : (res.data || [])
        groupsAll.value = arr.map(item => {
            return { id: item.id, label: formatGroupName(item, t) }
        })
    } catch (e) {
        console.error('Failed to load groups', e)
    }
}

const groupLabelById = (gid) => {
    return groupsAll.value.find(x => x.id === gid)?.label || String(gid || '')
}

const loadDevices = async () => {
    try {
        const res = await listDevices()
        if (Array.isArray(res)) {
            devices.value = res
        } else if (res && res.data) {
            devices.value = res.data
        }
    } catch (e) {
        console.error('Failed to load devices', e)
    }
}

const loadData = async () => {
    await Promise.all([
        loadServerData(),
        loadStats(),
        loadDetections()
    ])
}

const loadStats = async () => {
    try {
        const res = await getEntertainmentStats()
        if (res && res.ok) {
            const data = res.data
            stats.videoCount = data.category_stats?.find(c => c.category === 'video')?.total_visits || 0
            stats.gamingCount = data.category_stats?.find(c => c.category === 'gaming')?.total_visits || 0
            stats.deviceCount = data.device_stats?.length || 0
        }
    } catch (e) {
        console.error('Failed to load stats', e)
    }
}

const loadDetections = async (resetPage = true) => {
    loading.value = true
    if (resetPage) page.value = 1
    try {
        const params = {
            device_id: filters.device_id,
            category: filters.category,
            date_from: filters.dateRange?.[0],
            date_to: filters.dateRange?.[1],
            // 后端现在返回全部数据（前500条），前端不再传分页参数给后端以保持简单
        }
        const res = await getDetections(params)
        if (res && res.ok) {
            detections.value = res.data || []
            total.value = res.total || 0
        }
    } catch (e) {
        ElMessage.error(t('entertainment.fetch_failed'))
    } finally {
        loading.value = false
    }
}

const handleSizeChange = (val) => {
    pageSize.value = val
    page.value = 1
    // 不用重新 load，本地 slice 即可
}

const resetFilters = () => {
    filters.device_id = null
    filters.category = ''
    filters.dateRange = []
    filters.startDate = ''
    filters.endDate = ''
    loadDetections()
}

const handleManualDetect = async () => {
    if (checkIsLicenseExpired(serverData.value)) {
        ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
        return
    }
    detecting.value = true
    try {
        const res = await triggerDetection()
        if (res && res.ok) {
            ElMessage.success(res.msg || t('entertainment.sync_success'))
            loadStats()
            loadDetections()
        } else {
            ElMessage.error(res.msg || t('entertainment.sync_failed'))
        }
    } catch (e) {
        ElMessage.error(t('entertainment.sync_failed'))
    } finally {
        detecting.value = false
    }
}


const toggleBlock = (row) => {
    if (row.is_blocked) {
        handleUnblockDomain(row.domain)
    } else {
        handleBlockDomain(row.domain)
    }
}

const handleBlockDomain = async (domain) => {
    if (checkIsLicenseExpired(serverData.value)) {
        ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
        return
    }
    try {
        await ElMessageBox.confirm(
            t('entertainment.confirm_block', { domain }),
            t('entertainment.block_risk'),
            { 
                confirmButtonText: t('entertainment.block'), 
                cancelButtonText: t('common.cancel'), 
                type: 'warning',
                confirmButtonClass: 'el-button--danger'
            }
        )
        
        const res = await blockDomain(domain)
        if (res && res.ok) {
            ElMessage.success(res.msg || t('common.save_success'))
            // 实时更新状态
            loadDetections()
        } else {
            ElMessage.error(res.msg || t('common.operation_failed'))
        }
    } catch (e) {
        if (e !== 'cancel') {
            console.error('Blocking failed', e)
        }
    }
}

const handleUnblockDomain = async (domain) => {
    if (checkIsLicenseExpired(serverData.value)) {
        ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
        return
    }
    try {
        await ElMessageBox.confirm(
            t('entertainment.confirm_unblock', { domain }),
            t('entertainment.unblock_confirm'),
            { 
                confirmButtonText: t('common.confirm'), 
                cancelButtonText: t('common.close'), 
                type: 'info'
            }
        )
        
        const res = await unblockDomain(domain)
        if (res && res.ok) {
            ElMessage.success(res.msg || t('common.save_success'))
            loadDetections()
        }
    } catch (e) {
        if (e !== 'cancel') {
            console.error('Unblocking failed', e)
        }
    }
}

const formatTime = (timeStr) => {
    if (!timeStr) return '-'
    // 假设 ISO 格式: 2026-01-26T21:14:00...
    if (timeStr.includes('T')) {
        const datePart = timeStr.substring(5, 10) // 01-26
        const timePart = timeStr.substring(11, 19) // 21:14:00
        return `${datePart} ${timePart}`
    }
    return timeStr
}

// Consistent hashed colors for groups (Matching DeviceManagement.vue)
const groupPalettes = [
    {bg: 'var(--tag-blue-bg)', fg: 'var(--tag-blue-fg)', bd: 'var(--tag-blue-bd)'},
    {bg: 'var(--tag-green-bg)', fg: 'var(--tag-green-fg)', bd: 'var(--tag-green-bd)'},
    {bg: 'var(--tag-orange-bg)', fg: 'var(--tag-orange-fg)', bd: 'var(--tag-orange-bd)'},
    {bg: 'var(--tag-purple-bg)', fg: 'var(--tag-purple-fg)', bd: 'var(--tag-purple-bd)'}
]

const getGroupStyle = (groupId) => {
    if (groupId == null) return {}
    // Use ID directly for consistent color mapping with DeviceManagement
    const index = Number(groupId) % groupPalettes.length
    const c = groupPalettes[index]
    return {
        backgroundColor: c.bg, 
        color: c.fg, 
        borderColor: c.bd, 
        borderWidth: '1px', 
        borderStyle: 'solid'
    }
}

</script>

<style scoped>
/* ========================= Base (Follow AccessStatistics Style) ========================= */
.page {
    padding: 16px;
    background-color: var(--bg-page);
    display: flex;
    flex-direction: column;
    gap: 14px;
    min-height: 100vh;
    overflow-x: hidden;
}

@media (max-width: 768px) {
    .page { 
        padding: 12px; 
        gap: 12px; 
        min-height: auto; /* Allow natural height on mobile */
        overflow: visible; /* Ensure scrolling is not trapped */
    }
}


.title {
    grid-area: title;
    margin: 0;
    font-size: 16px;
    line-height: 1.2;
    font-weight: 700;
    color: var(--text-primary);
}

@media (max-width: 768px) {
    .title {
        font-size: 14px;
    }
}

.sub {
    grid-area: sub;
    font-size: 12px;
    color: var(--text-regular);
}

@media (max-width: 768px) {
    .sub {
        font-size: 10px;
        opacity: 0.8;
    }
}

.header-right {
    grid-area: action;
    display: flex;
    justify-content: flex-end;
}

.sync-btn {
    border-radius: 8px; /* Slightly tighter radius */
    font-weight: 600;
}

@media (max-width: 768px) {
    .sync-btn {
        font-size: 10px; /* Reduced font size */
        padding: 4px 8px;
        height: 26px;
        flex-shrink: 0;
    }
}

/* ========================= KPI ========================= */
.kpi-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 12px;
}

.kpi-card {
    border-radius: 12px;
    border: 1px solid var(--border-light);
    background: var(--bg-card);
    box-shadow: 0 1px 2px rgba(0,0,0,0.05);
    padding: 10px 14px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    min-height: 52px;
    position: relative; /* Fixed for absolute positioning of icons on mobile */
}

.kpi-video { border-left: 4px solid #f56c6c; }
.kpi-gaming { border-left: 4px solid #e6a23c; }
.kpi-device { border-left: 4px solid #409eff; }

.kpi-main { display: flex; flex-direction: column; gap: 6px; }
.kpi-label { font-size: 12px; color: var(--text-regular); }
.kpi-value { font-size: 20px; font-weight: 800; color: var(--text-primary); line-height: 1; }
.kpi-value .unit { font-size: 12px; font-weight: 400; color: var(--text-secondary); margin-left: 2px; }
.kpi-icon { font-size: 28px; color: rgba(15, 23, 42, 0.08); }

@media (max-width: 768px) {
    .kpi-grid { gap: 10px; }
    .kpi-card { padding: 12px; display: flex; flex-direction: column; align-items: flex-start; gap: 4px; }
    .kpi-icon { position: absolute; right: 8px; top: 8px; font-size: 20px; }
    .kpi-value { font-size: 16px; }
}

/* ========================= Filter Card ========================= */
.card {
    border-radius: 12px;
    border: 1px solid var(--border-light);
    background: var(--bg-card);
}

.filter-card { padding: 0px; } /* Ensure no extra padding on the card itself */
.filter-form :deep(.el-form-item) { margin-bottom: 0px; margin-right: 12px; vertical-align: middle; }

/* PC 端的固定宽度 - 加宽设备选择器 */
.filter-item-device { width: 300px; }
.filter-item-type { margin-right: 16px; }
.filter-item-date { width: 320px; }

@media (max-width: 768px) {
    .filter-card { 
        padding: 0; 
        background: transparent;
        border: none;
    }
    .filter-col-mobile { 
        display: flex;
        flex-direction: column;
        gap: 8px; /* Reduced gap for a tighter look */
    }
    
    /* Elegant Mobile Form Rows */
    .filter-row {
        background: var(--bg-card);
        border-radius: 10px;
        padding: 6px 12px;
        border: 1px solid var(--divider-color);
        display: flex;
        align-items: center;
        min-height: 44px;
    }
    
    .filter-form :deep(.el-form-item) {
        margin-right: 0;
        width: 100%;
        display: flex;
        align-items: center;
    }
    
    .filter-form :deep(.el-form-item__label) {
        flex-shrink: 0;
        width: 44px !important;
        font-size: 13px;
        color: var(--text-regular);
        font-weight: 500;
        padding-right: 8px !important;
    }
    
    .filter-form :deep(.el-input__wrapper),
    .filter-form :deep(.el-select .el-input__wrapper) {
        box-shadow: none !important;
        background: transparent !important;
        padding-left: 0 !important;
    }
}

/* ========================= Modern Table ========================= */
.table-card { padding: 4px; }
.modern-table :deep(.el-table__header th) {
    background-color: var(--bg-page) !important;
    color: var(--text-regular) !important;
    font-weight: 600;
    font-size: 13px;
    height: 50px;
}

.modern-table :deep(.el-table__row) {
    transition: background-color 0.2s;
}

.modern-table :deep(.el-table__row:hover) {
    background-color: var(--divider-color) !important;
}

/* --- Device Cell Styles (PC) --- */
.device-cell {
    display: flex;
    align-items: center; /* Center icon vertically */
    gap: 12px;
}

.device-type-icon {
    font-size: 24px; /* Slightly larger icon */
    flex-shrink: 0;
}

.device-info {
    display: flex;
    flex-direction: column;
    gap: 4px; /* Spacing between name and tag */
    align-items: flex-start;
}

.device-name { 
    font-weight: 700; 
    color: var(--text-primary); 
    font-size: 14px; 
    line-height: 1.2;
}


/* --- Site Cell Styles (PC) --- */
.site-cell {
    display: flex;
    align-items: center;
    flex-wrap: wrap; /* Allow wrapping if domain is long */
}

.site-name {
    font-weight: 700;
    margin-right: 8px;
    color: var(--text-primary);
    font-size: 14px;
}

.site-domain {
    font-size: 12px;
    color: var(--text-regular);
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    background: var(--bg-page);
    padding: 2px 6px;
    border-radius: 4px;
}

/* --- Time Range Styles --- */
.time-range-pc {
    display: flex;
    flex-direction: column;
    gap: 4px;
}

.time-row {
    display: flex;
    align-items: center;
    white-space: nowrap;
    font-size: 12px;
    color: var(--text-regular);
}

.time-label {
    font-weight: 600;
    color: var(--text-secondary);
    margin-right: 4px;
}

.time-text {
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
}

/* ========================= Mobile Card Layout ========================= */
.mobile-list.page {
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 16px;
    background-color: var(--bg-page);
    min-height: 100vh;
}

.m-card {
    background: var(--bg-card);
    border-radius: 18px;
    border: 1px solid var(--divider-color);
    box-shadow: 0 2px 6px rgba(15, 23, 42, 0.04);
    overflow: hidden;
    display: flex;
    flex-direction: column;
    transition: transform 0.2s;
}

/* Removed :active transform to prevent interference with mobile scrolling */

.m-card-header {
    padding: 8px 14px; /* Tighter header */
    background: var(--bg-card);
    border-bottom: 1px solid var(--divider-color);
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.m-card-header-right {
    display: flex;
    align-items: center;
    gap: 8px;
}

.m-device {
    display: flex;
    align-items: center;
    gap: 8px;
}

.m-type-icon { font-size: 18px; }
.m-device-name { font-weight: 700; color: var(--text-primary); font-size: 14px; }

.m-card-body {
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 12px;
}

.m-site-info { display: flex; flex-direction: column; gap: 4px; }
.m-site-name { font-weight: 700; color: var(--text-primary); font-size: 15px; line-height: 1.4; }
.m-site-domain { font-size: 12px; color: var(--text-regular); font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; background: var(--bg-page); padding: 2px 6px; border-radius: 4px; width: fit-content; }

.m-meta {
    display: flex;
    gap: 20px;
}

.m-meta-item { display: flex; flex-direction: column; gap: 2px; }
.m-meta-label { font-size: 11px; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 0.5px; }
.m-meta-value { font-size: 10px; color: var(--text-regular); }
.m-meta-value.highlight { color: var(--primary-color); }

.m-card-footer {
    padding: 12px 16px;
    background: var(--bg-card);
    border-top: 1px dashed var(--divider-color);
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.m-time-box { display: flex; flex-direction: column; }
.m-time-label { font-size: 11px; color: var(--text-secondary); }
.m-time-value { font-size: 12px; color: var(--text-regular); font-family: monospace; }

.m-block-btn {
    min-width: 80px;
    font-weight: 600;
    font-size: 13px;
    height: 36px;
}

.m-empty { padding: 40px 0; }

.pagination-box {
    margin-top: 14px;
    display: flex;
    justify-content: flex-end;
    padding: 0 14px 14px 14px;
}

/* ========================= Mobile Specific Overrides ========================= */
@media (max-width: 768px) {
    .table-card {
        padding: 0 !important;
        margin: 0 !important;
        border: none !important;
        background: transparent !important;
    }
    
    .filter-col-mobile {
        display: flex;
        flex-direction: column;
        gap: 8px; /* Consistent with previous change */
    }
    .date-row-mobile {
        justify-content: flex-start;
        padding-right: 8px;
    }
    .date-item-mobile {
        margin-right: 0 !important;
        flex: 1;
        min-width: 0;
    }
    .date-item-mobile :deep(.el-input__wrapper) {
        padding: 0 2px !important;
        font-size: 12px;
    }
    .date-item-mobile :deep(.el-form-item__label) {
        width: 24px !important;
        padding: 0 !important;
        font-size: 12px;
        text-align: center;
    }
    .filter-actions-mobile {
        display: flex;
        gap: 6px;
        flex-shrink: 0;
        padding-left: 8px;
        border-left: 1px solid var(--divider-color);
        margin-left: 4px;
    }
    .filter-actions-mobile .el-button {
        padding: 4px 10px; 
        height: 28px;
        margin: 0;
    }

    .m-card {
        padding: 5px !important;
        border-radius: 12px;
        margin-bottom: 8px;
        box-shadow: 0 2px 4px rgba(15, 23, 42, 0.03);
    }
    .m-group-card, .m-item {
        padding: 5px !important;
    }
    .m-card-header {
        padding: 8px 12px; 
    }
    .m-card-body {
        padding: 4px 12px; /* Further tightened body padding */
        gap: 0px; /* Minimal internal gap */
    }
    .m-site-info {
        gap: 0px;
        margin-bottom: 2px;
    }
    .m-site-name {
        font-size: 14px;
        line-height: 1.1;
    }
    .m-site-domain {
        font-size: 11px;
        padding: 1px 4px;
    }
    .m-meta {
        gap: 10px;
    }
    .m-meta-item {
        gap: 0px;
    }
    .m-card-footer {
        padding: 1px 12px 3px 12px; /* Extremely tight footer */
    }
    .m-block-btn {
        min-width: 60px;
        font-weight: 600;
        font-size: 11px;
        height: 22px; /* Smallest action button */
        padding: 0 6px;
    }
}
</style>

<style>
/* 强制覆盖 Element Plus 卡片 padding (与访问统计页面一致) */
.entertainment-page .el-card__body {
    padding: 5px !important;
}
</style>
