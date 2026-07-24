<template>
    <div class="page access-stats-page">
        <!-- Header -->
        <div class="header-row">
            <div class="header-left">
                <!-- ✅ Mobile：标题同行右侧放“普通刷新”按钮 -->
                <div class="title-row">
                    <h2 class="title">{{ $t('stats.title') }}</h2>
                    <el-button
                            v-if="isMobile"
                            class="refresh-circle"
                            :loading="loading"
                            @click="load"
                            :icon="Refresh"
                            circle
                    />
                </div>

                <div class="sub">{{ $t('stats.sub') }}</div>
            </div>

            <div class="actions">
                <el-button-group class="hidden-xs">
                    <el-button :loading="fetching" @click="fetchAdGuardConfig" :icon="Download">{{ $t('stats.get_adguard_config') }}</el-button>
                    <el-button :loading="syncing" @click="syncAdGuardConfig" :icon="Connection">{{ $t('stats.sync_adguard') }}</el-button>
                </el-button-group>

                <!-- ✅ PC：仍保留原来的“蓝色主按钮”刷新 -->
                <el-button v-if="!isMobile" type="primary" :loading="loading" @click="load" :icon="Refresh">
                    {{ $t('stats.refresh_data') }}
                </el-button>
            </div>
        </div>

        <!-- KPI -->
        <div class="kpi-grid">
            <div class="kpi-card kpi-total">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.today_queries') }}</div>
                    <div class="kpi-value">{{ formatNumber(summary.total) }}</div>
                </div>
                <el-icon class="kpi-icon"><Memo /></el-icon>
            </div>

            <div class="kpi-card kpi-blocked">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.today_blocked') }}</div>
                    <div class="kpi-value">{{ formatNumber(summary.blocked) }}</div>
                </div>
                <el-icon class="kpi-icon"><CircleClose /></el-icon>
            </div>

            <div class="kpi-card kpi-rate">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.interception_rate') }}</div>
                    <div class="kpi-value">{{ calculateRate }}%</div>
                </div>
                <el-icon class="kpi-icon"><PieChart /></el-icon>
            </div>
        </div>

        <!-- Charts -->
        <div class="chart-stack-layout">
            <el-card shadow="never" class="card">
                <div class="card-title">{{ $t('stats.traffic_share') }}</div>
                <div ref="pieRef" class="chart"></div>
            </el-card>

            <el-card shadow="never" class="card">
                <div class="card-title">{{ $t('stats.blocked_ranking') }}</div>
                <div ref="barRef" class="chart"></div>
            </el-card>
        </div>

        <!-- Tabs -->
        <el-card shadow="never" class="card tabs-card">
            <el-tabs v-model="activeTab" class="modern-tabs">
                <!-- Devices -->
                <el-tab-pane name="devices">
                    <template #label>
                        <span class="tab-label"><el-icon><Monitor /></el-icon> {{ $t('stats.device_traffic_top') }}</span>
                    </template>

                    <el-table
                            v-if="!isMobile"
                            :data="deviceTop20"
                            v-loading="loading"
                            class="modern-table"
                            style="width: 100%"
                    >
                        <el-table-column :label="$t('device.device_name')" min-width="180">
                            <template #default="s">
                                <span class="device-name">
                                  <el-icon
                                          class="dev-icon"
                                          :style="{ color: getDeviceTypeColor(getType(s.row)) }"
                                  >
                                    <component :is="getDeviceTypeIcon(getType(s.row))" />
                                  </el-icon>
                                  <span class="dev-text">{{ s.row.name || $t('stats.unknown_device') }}</span>
                                </span>
                            </template>
                        </el-table-column>

                        <el-table-column :label="$t('groups.title')" min-width="140">
                            <template #default="s">
                                <span
                                        v-if="s.row.group_key"
                                        class="group-tag"
                                        :style="getGroupTagStyle(s.row.group_name)"
                                >
                                  {{ formatGroupName({ name: s.row.group_key, remark: s.row.group_name }, t) }}
                                </span>
                                <span v-else class="none-text">{{ $t('stats.unclassified') }}</span>
                            </template>
                        </el-table-column>

                        <el-table-column :label="$t('device.network_address')" min-width="160">
                            <template #default="s">
                                <code class="ip-tag">{{ s.row.ip || '-' }}</code>
                            </template>
                        </el-table-column>

                        <el-table-column prop="query_count" :label="$t('home.today_queries')" width="120" sortable />

                        <el-table-column :label="$t('stats.analyze')" width="90" fixed="right">
                            <template #default="s">
                                <el-button type="primary" link @click="goDevice(s.row)" :disabled="!s.row.device_id">
                                    {{ $t('stats.analyze') }}
                                </el-button>
                            </template>
                        </el-table-column>
                    </el-table>

                    <!-- Mobile list -->
                    <div v-else class="mobile-list" v-loading="loading">
                        <div
                                v-for="(r, idx) in deviceTop20"
                                :key="idx"
                                class="m-row"
                                @click="goDevice(r)"
                        >
                            <div class="m-top">
                                    <span class="m-name ell">{{ r.name || $t('stats.unknown_device') }}</span>
                                <span
                                        v-if="r.group_key"
                                        class="group-tag mini"
                                        :style="getGroupTagStyle(r.group_name)"
                                >
                                  {{ formatGroupName({ name: r.group_key, remark: r.group_name }, t) }}
                                </span>
                                <span v-else class="none-text">{{ $t('stats.unclassified') }}</span>
                            </div>

                            <div class="m-sub">
                                <code class="m-ip">{{ r.ip || '-' }}</code>
                                <span class="m-count"><b>{{ r.query_count ?? 0 }}</b> {{ $t('audit.occurrence_unit') }}</span>
                            </div>
                        </div>
                    </div>
                </el-tab-pane>

                <!-- Blocked -->
                <el-tab-pane name="blocked">
                    <template #label>
                        <span class="tab-label"><el-icon><CircleClose /></el-icon> {{ $t('stats.blocked_top') }}</span>
                    </template>

                    <el-table
                            v-if="!isMobile"
                            :data="blockedTop20"
                            v-loading="loading"
                            class="modern-table"
                            style="width: 100%"
                    >
                        <el-table-column prop="domain" :label="$t('stats.domain_address')" min-width="320" show-overflow-tooltip />
                        <el-table-column :label="$t('stats.intercept_frequency')" width="150">
                            <template #default="s">
                                <span class="blocked-count-text">{{ s.row.count ?? 0 }}</span>
                            </template>
                        </el-table-column>
                    </el-table>

                    <div v-else class="mobile-list" v-loading="loading">
                        <div v-for="(r, idx) in blockedTop20" :key="idx" class="m-blocked">
                            <div class="m-domain" :title="r.domain">{{ r.domain }}</div>
                            <div class="m-badge">{{ r.count ?? 0 }}</div>
                        </div>
                    </div>
                </el-tab-pane>
            </el-tabs>
        </el-card>
    </div>
</template>

<script setup>
    import { ref, onMounted, onUnmounted, computed, nextTick } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { useRouter } from 'vue-router'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import http from '@/utils/http'
    import {
        Refresh, Download, Connection, Monitor, CircleClose, Memo, PieChart
    } from '@element-plus/icons-vue'
    import * as echarts from 'echarts'
    import { getDeviceTypeIcon, getDeviceTypeColor, normalizeDeviceType, formatGroupName, formatNumber, checkIsLicenseExpired } from '@/utils/common.js'
    import { getStatsSummary, fetchRemoteConfig, syncAllStats } from '@/api/stats'

    const router = useRouter()
    const { t } = useI18n()

    /* ================= state ================= */
    const loading = ref(false)
    const fetching = ref(false)
    const syncing = ref(false)
    const serverData = ref(null)
    const activeTab = ref('devices')

    const summary = ref({ total: 0, blocked: 0 })
    const trafficPie = ref([])
    const blockedBar = ref([])
    const deviceRanking = ref([])
    const blockedRanking = ref([])

    const deviceTop20 = computed(() => (deviceRanking.value || []).slice(0, 20))
    const blockedTop20 = computed(() => (blockedRanking.value || []).slice(0, 20))

    const calculateRate = computed(() => {
        const t = Number(summary.value.total || 0)
        const b = Number(summary.value.blocked || 0)
        if (!t) return 0
        return ((b / t) * 100).toFixed(1)
    })

    /* ================= responsive ================= */
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => {
        isMobile.value = window.innerWidth < 768
        pieChart?.resize()
        barChart?.resize()
    }

    /* ================= group tag color (soft) ================= */
    const GROUP_PALETTE = [
        { bg: '#f8fafc', text: '#334155', border: '#e5e7eb' },
        { bg: '#f0f9ff', text: '#075985', border: '#e0f2fe' },
        { bg: '#f0fdf4', text: '#166534', border: '#dcfce7' },
        { bg: '#fffbeb', text: '#92400e', border: '#fef3c7' },
        { bg: '#faf5ff', text: '#6b21a8', border: '#ede9fe' }
    ]

    const getGroupTagStyle = (name) => {
        if (!name) return {}
        let hash = 0
        for (let i = 0; i < name.length; i++) hash = name.charCodeAt(i) + ((hash << 5) - hash)
        const c = GROUP_PALETTE[Math.abs(hash) % GROUP_PALETTE.length]
        return { backgroundColor: c.bg, color: c.text, borderColor: c.border }
    }

    /* ================= charts ================= */
    const pieRef = ref(null)
    const barRef = ref(null)
    let pieChart = null
    let barChart = null

    const ensureCharts = () => {
        if (pieRef.value && !pieChart) pieChart = echarts.init(pieRef.value)
        if (barRef.value && !barChart) barChart = echarts.init(barRef.value)
    }

    const setPieOption = () => {
        if (!pieChart) return
        pieChart.setOption(
            {
                tooltip: { trigger: 'item' },
                legend: {
                    bottom: 0,
                    icon: 'circle',
                    textStyle: { fontSize: 11 },
                    itemWidth: 10,
                    itemHeight: 10
                },
                series: [{
                    type: 'pie',
                    radius: ['55%', '85%'], // ✅ Increase radius to fill whitespace
                    center: ['50%', '45%'], // ✅ Adjust center
                    itemStyle: { borderRadius: 8, borderColor: 'transparent', borderWidth: 0 },
                    label: { show: false },
                    data: trafficPie.value || []
                }]
            },
            { notMerge: true }
        )
    }

    const setBarOption = () => {
        if (!barChart) return

        const isM = isMobile.value
        const names = (blockedBar.value || []).map(x => x.name)
        const values = (blockedBar.value || []).map(x => x.value)

        barChart.setOption(
            {
                grid: {
                    top: 8,
                    bottom: 16,
                    left: isM ? 8 : 10,
                    right: isM ? 10 : 24,
                    containLabel: true
                },
                tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
                xAxis: {
                    type: 'value',
                    splitLine: { lineStyle: { type: 'dashed' } }
                },
                yAxis: {
                    type: 'category',
                    data: names,
                    inverse: true,
                    axisLabel: {
                        fontSize: 11,
                        width: isM ? 110 : 180,
                        overflow: 'truncate',
                        ellipsis: '…',
                        formatter: (v) => {
                            const max = isM ? 14 : 24
                            return v && v.length > max ? v.slice(0, max) + '…' : v
                        }
                    }
                },
                series: [{
                    type: 'bar',
                    data: values,
                    barWidth: isM ? 12 : 14,
                    itemStyle: { color: '#ef4444', borderRadius: [0, 6, 6, 0] }
                }]
            },
            { notMerge: true }
        )
    }

    const renderCharts = async () => {
        await nextTick()
        ensureCharts()
        setPieOption()
        setBarOption()
        pieChart?.resize()
        barChart?.resize()
    }

    /* ================= api ================= */
    const load = async () => {
        loading.value = true
        
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

        try {
            const results = await Promise.allSettled([
                getStatsSummary(),
                loadServerData()
            ])
            const statsRes = results[0]
            if (statsRes.status === 'fulfilled' && statsRes.value) {
                const res = statsRes.value
                summary.value = res?.summary || { total: 0, blocked: 0 }
                trafficPie.value = res?.charts?.traffic_pie || []
                blockedBar.value = res?.charts?.blocked_bar || []
                deviceRanking.value = res?.device_ranking || []
                blockedRanking.value = res?.blocked_ranking || []
                await renderCharts()
            }
        } finally {
            loading.value = false
        }
    }

    const goDevice = (row) => {
        if (!row?.device_id) return ElMessage.warning(t('stats.no_device_id'))
        router.push(`/stats/device/${row.device_id}`)
    }

    const fetchAdGuardConfig = async () => {
        if (checkIsLicenseExpired(serverData.value)) {
            ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
            return
        }
        fetching.value = true
        try {
            await fetchRemoteConfig()
            ElMessage.success(t('stats.config_fetched'))
        } finally {
            fetching.value = false
        }
    }

    const syncAdGuardConfig = async () => {
        if (checkIsLicenseExpired(serverData.value)) {
            ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
            return
        }
        syncing.value = true
        try {
            await syncAllStats()
            ElMessage.success(t('stats.sync_complete'))
        } finally {
            syncing.value = false
        }
    }

    const getType = (row) => normalizeDeviceType(row?.type)

    /* ================= lifecycle ================= */
    onMounted(async () => {
        window.addEventListener('resize', onResize)
        await load()
    })

    onUnmounted(() => {
        window.removeEventListener('resize', onResize)
        pieChart?.dispose()
        barChart?.dispose()
    })
</script>

<style scoped>

    /* ✅ 手机标题行右侧“普通刷新”按钮：上下居中 + 高度固定 */
    .refresh-inline{
        height: 32px;
        padding: 0 12px;
        border-radius: 10px;
        font-size: 12px;
        flex: 0 0 auto;

        display: inline-flex;        /* ✅ 图标+文字上下居中 */
        align-items: center;
        justify-content: center;
    }

    /* ========================= KPI ========================= */
    .kpi-grid {
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 12px;
    }
    @media (max-width: 768px) { .kpi-grid { gap: 10px; } }

    .kpi-card {
        position: relative;
        border-radius: 14px;
        border: 1px solid var(--border-light);
        background: var(--bg-card);
        box-shadow: 0 1px 3px rgba(15, 23, 42, 0.05);
        overflow: hidden;
        padding: 14px;
        min-height: 78px;
        display: flex;
        align-items: center;
        justify-content: space-between;
    }

    .kpi-total { background: var(--kpi-total-bg); }
    .kpi-blocked { background: var(--kpi-blocked-bg); }
    .kpi-rate { background: var(--kpi-rate-bg); }

    .kpi-main { display: flex; flex-direction: column; gap: 6px; min-width: 0; }
    .kpi-label { font-size: 12px; color: var(--text-regular); }
    .kpi-value {
        font-size: 22px;
        font-weight: 900;
        color: var(--text-primary);
        line-height: 1;
        font-variant-numeric: tabular-nums;
    }
    .kpi-icon { font-size: 30px; color: var(--text-secondary); opacity: 0.1; }

    /* KPI Colors (Global) */
    .kpi-total .kpi-value { color: #2563eb; }
    .kpi-blocked .kpi-value { color: #ef4444; }
    .kpi-rate .kpi-value { color: #67c23a; }

    @media (max-width: 768px) {
        .kpi-card { padding: 12px; min-height: 70px; }
        .kpi-value { 
            font-size: 16px; 
            font-weight: normal !important;
        }
        .kpi-icon { font-size: 26px; }
    }

    /* ========================= Cards + Charts ========================= */
    .card {
        border-radius: 14px;
        border: 1px solid var(--border-light);
        background: var(--bg-card);
        box-shadow: 0 1px 3px rgba(15, 23, 42, 0.05);
        overflow: hidden;
    }

    /* Reduce card body padding as requested */
    .card :deep(.el-card__body) {
        padding: 5px !important;
    }

    .card-title {
        padding: 10px 10px 0 10px; /* Adjust title padding to match */
        font-size: 14px;
        font-weight: 800;
        color: var(--text-primary);
        margin-bottom: 0; /* Remove bottom margin to let chart control space */
    }

    .chart-stack-layout {
        display: flex;
        flex-direction: column;
        gap: 12px;
        width: 100%;
    }

    .chart { height: 280px; padding: 4px 10px 10px 10px; }

    @media (max-width: 768px) {
        .chart { height: 260px; padding: 4px 8px 10px 8px; }
    }

    /* ========================= Tabs + Table ========================= */
    .tabs-card { padding-top: 6px; }

    .modern-tabs :deep(.el-tabs__header) { margin: 0 0 8px 12px; } /* Increased to 12px */
    .modern-tabs :deep(.el-tabs__nav-wrap::after) { height: 1px; background-color: var(--divider-color); }
    .tab-label {
        display: inline-flex;
        align-items: center;
        gap: 6px;
        font-weight: 700;
        color: var(--text-primary);
    }

    .modern-table :deep(.el-table) { border: none !important; background: transparent; }
    .modern-table :deep(.el-table__inner-wrapper::before) { height: 0 !important; }
    .modern-table :deep(.el-table__border-left-patch) { width: 0 !important; }
    .modern-table :deep(.el-table__header-wrapper),
    .modern-table :deep(.el-table__body-wrapper) {
        background: var(--bg-card);
        border-radius: 14px;
        overflow: hidden;
    }
    .modern-table :deep(.el-table__header th) {
        background-color: transparent !important;
        color: var(--text-secondary) !important;
        font-weight: 600;
        padding: 12px 0 !important;
        border-bottom: 1px solid var(--divider-color) !important;
    }
    .modern-table :deep(.el-table .el-table__cell) { padding: 16px 0 !important; }
    .modern-table :deep(.el-table__body td) { border-bottom: 1px solid var(--divider-color) !important; }

    .device-name { font-weight: 800; color: var(--text-primary); }

    .ip-tag {
        color: #2563eb;
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
        font-variant-numeric: tabular-nums;
    }

    .none-text { color: #94a3b8; }

    .blocked-count-text { color: #b91c1c; font-weight: 900; }

    /* ========================= Group tag ========================= */
    .group-tag {
        display: inline-flex;
        align-items: center;
        padding: 2px 10px;
        border-radius: 999px;
        font-size: 12px;
        font-weight: 700;
        border: 1px solid var(--border-light);
        background: var(--bg-subtle);
        color: var(--text-primary);
        white-space: nowrap;
    }
    .group-tag.mini { padding: 1px 8px; font-size: 11px; }

    /* ========================= Mobile list ========================= */
    .mobile-list {
        display: flex;
        flex-direction: column;
        border-radius: 14px;
        border: 1px solid var(--border-light);
        overflow: hidden;
        background: var(--bg-card);
    }

    .m-row { padding: 14px 14px; border-bottom: 1px solid var(--divider-color); }
    .m-row:last-child { border-bottom: none; }

    .m-top {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        margin-bottom: 8px;
    }

    .m-name {
        font-weight: 800;
        color: var(--text-primary);
        font-size: 14px;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .m-sub {
        display: flex;
        justify-content: space-between;
        align-items: center;
        gap: 10px;
    }

    .m-ip {
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
        color: var(--text-regular);
        min-width: 0;
    overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .m-count { font-size: 11px; color: var(--text-primary); white-space: nowrap; }
    .m-count b { color: #2563eb; }

    /* blocked mobile */
    .m-blocked {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        padding: 12px 14px;
        border-bottom: 1px solid var(--divider-color);
    }
    .m-blocked:last-child { border-bottom: none; }

    .m-domain {
        flex: 1 1 auto;
        min-width: 0;
        color: var(--text-primary);
        font-size: 13px;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .m-badge {
        flex: 0 0 auto;
        background: #fee2e2;
        color: #b91c1c;
        font-weight: 900;
        padding: 2px 10px;
        border-radius: 10px;
        font-size: 12px;
        font-variant-numeric: tabular-nums;
    }

    /* 设备名称：图标 + 文本 */
    .dev-icon{
        margin-right: 8px;
        font-size: 16px;
        vertical-align: -2px;
        flex: 0 0 auto;
    }
    .dev-text{
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .device-name{
        display: inline-flex;
        align-items: center;
        gap: 0;
    }
    .m-name{
        display: inline-flex;
        align-items: center;
        gap: 0;
    }
</style>

<style>
/* 强制覆盖 Element Plus 卡片 padding */
.access-stats-page .el-card__body {
    padding: 10px !important;
}

@media (max-width: 768px) {
    .access-stats-page .el-card__body {
        border: 1px solid var(--border-light) !important;
        border-radius: 14px !important;
        background-color: var(--bg-card) !important;
    }

    /* Ensure the inner tabs-card (if nesting occurred) doesn't have borders if it's already in the body */
    .access-stats-page .tabs-card.el-card {
        border: none !important;
        box-shadow: none !important;
        background: transparent !important;
    }
}
</style>
