<template>
    <div class="page">
        <!-- A. 顶部：设备状态卡片 -->
        <el-card shadow="never" class="card top-card" v-loading="loading">
        <div v-if="isMobile" class="top-mobile">
            <!-- Row 1: Device Name & Back Btn -->
            <div class="m-title-row">
                <div class="m-title-left">
                    <el-icon
                        class="dev-icon"
                        :style="{ color: getDeviceTypeColor(normalizeDeviceType(info.type)), fontSize: '18px' }"
                    >
                        <component :is="getDeviceTypeIcon(normalizeDeviceType(info.type))"/>
                    </el-icon>
                    <span class="m-dev-name">{{ info.name || $t('device_stats.unnamed') }}</span>
                </div>
                <el-button size="small" @click="goBack" plain class="m-back-btn">
                    {{ $t('device_stats.back') }}
                </el-button>
            </div>

            <!-- Row 2: Network IDs -->
            <div class="m-ids mono">
                <span class="m-id"><span class="k">IP</span> {{ info.ip || '-' }}</span>
                <span class="dot">•</span>
                <span class="m-id"><span class="k">MAC</span> {{ info.mac || '-' }}</span>
            </div>

            <!-- Row 3: Action Buttons & Tags -->
            <div class="m-action-row">
                <div class="m-tags">
                    <el-tag size="small" type="info" class="ell" style="max-width: 120px;">
                        {{ formatGroupName({ name: info.group_key, remark: info.group_name }, t) }}
                    </el-tag>
                    <el-tag size="small" :type="sourceTagType">{{ sourceText }}</el-tag>
                </div>
                <div class="m-btns">
                    <el-button 
                        size="small" 
                        :loading="syncing" 
                        @click="syncAdGuardConfig" 
                        :icon="RefreshRight"
                        circle
                        style="border: none; background: var(--bg-subtle); color: var(--text-primary);"
                    />
                </div>
            </div>

            <!-- Date List -->
            <div class="m-date-list">
                <div 
                    v-for="d in availableDates" 
                    :key="d" 
                    class="date-item"
                    :class="{ active: selectedDate === d, today: d === todayStr() }"
                    @click="changeDate(d)"
                >
                    {{ d === todayStr() ? $t('device_stats.today') : d.split('-').slice(1).join('-') }}
                </div>
            </div>
        </div>

            <!-- ✅ PC 顶部：保持你原来的布局 -->
            <div v-else class="top-wrap">
                <!-- 左侧：名称 + 分组 + source -->
                <div class="left">
                    <div class="name-line">
                        <!-- ✅ PC：也用 info.type 的图标（替换原来 Monitor 固定图标） -->
                        <el-icon
                            class="dev-icon"
                            :style="{ color: getDeviceTypeColor(normalizeDeviceType(info.type)) }"
                        >
                            <component :is="getDeviceTypeIcon(normalizeDeviceType(info.type))"/>
                        </el-icon>

                        <div class="name-block">
                            <!-- 名称 + IP/MAC 同行 -->
                            <div class="name-row">
                                <div class="dev-name">{{ info.name || $t('device_stats.unnamed') }}</div>

                                <div class="id-line">
                                <span class="id-item">
                                    <span class="id-k">IP</span>
                                    <span class="id-v mono">{{ info.ip || '-' }}</span>
                                </span>
                                <span class="id-dot">•</span>
                                <span class="id-item">
                                    <span class="id-k">MAC</span>
                                    <span class="id-v mono">{{ info.mac || '-' }}</span>
                                </span>
                            </div>
                        </div>
                        <div class="sub">
                            <el-tag size="small" type="info">
                                {{ formatGroupName({ name: info.group_key, remark: info.group_name }, t) }}
                            </el-tag>
                            <el-tag size="small" :type="sourceTagType">{{ sourceText }}</el-tag>
                        </div>
                        </div>
                    </div>
                </div>

                <!-- 右侧：返回 + 日期列表 -->
                <div class="right-tools">
                    <div class="pc-date-list">
                        <div 
                            v-for="d in availableDates" 
                            :key="d" 
                            class="date-item"
                            :class="{ active: selectedDate === d, today: d === todayStr() }"
                            @click="changeDate(d)"
                        >
                            {{ d === todayStr() ? $t('device_stats.today') : d }}
                        </div>
                    </div>
                    <el-button @click="goBack" style="margin-left: 10px">{{ $t('device_stats.back') }}</el-button>
                </div>
            </div>

        </el-card>

        <!-- B. 中间：数据概览 -->
        <el-card shadow="never" class="card" v-loading="loading">
            <div class="stats">
                <div class="stat">
                    <div class="label">{{ $t('device_stats.total_queries') }}</div>
                    <!-- 日总量：固定值，不随查询改变 (如 19443) -->
                    <div class="value">{{ summary.total }}</div>
                </div>
                <div class="stat">
                    <div class="label">{{ $t('device_stats.total_blocked') }}</div>
                    <!-- 日拦截：固定值 (如 18363) -->
                    <div class="value danger">{{ summary.blocked }}</div>
                </div>
                <div class="stat">
                    <div class="label">{{ $t('device_stats.blocked_rate') }}</div>
                    <!-- 日拦截率：基于固定日总量计算 -->
                    <div class="value">{{ blockedRate }}%</div>
                </div>
            </div>
        </el-card>

        <!-- C. 底部：详细访问流水 -->
        <el-card shadow="never" class="card" v-loading="loading">
            <div class="section-title">
                <div class="title-left">{{ $t('device_stats.visit_flow') }}</div>
                <div class="title-right">
                    <span class="q-label" v-if="!isMobile">{{ $t('device_stats.time_label') }}</span>
                    <el-select
                        v-model="timeFrom"
                        :placeholder="$t('device_stats.start_hour')"
                        :size="isMobile ? 'small' : 'default'"
                        :style="{ width: isMobile ? '70px' : '100px' }"
                        clearable
                    >
                        <el-option
                            v-for="h in 24"
                            :key="h-1"
                            :label="`${String(h-1).padStart(2, '0')}${$t('device_stats.hour_suffix')}`"
                            :value="String(h-1).padStart(2, '0')"
                        />
                    </el-select>
                    <span class="q-sep">-</span>
                    <el-select
                        v-model="timeTo"
                        :placeholder="$t('device_stats.end_hour')"
                        :size="isMobile ? 'small' : 'default'"
                        :style="{ width: isMobile ? '70px' : '100px' }"
                        clearable
                    >
                        <el-option
                            v-for="h in 24"
                            :key="h-1"
                            :label="`${String(h-1).padStart(2, '0')}${$t('device_stats.hour_suffix')}`"
                            :value="String(h-1).padStart(2, '0')"
                        />
                    </el-select>
                    <el-button 
                        type="primary" 
                        :icon="Search" 
                        :size="isMobile ? 'small' : 'default'"
                        @click="handleSearch"
                        style="margin-left: 4px"
                    >
                        {{ $t('device_stats.query_btn') }}
                    </el-button>
                </div>
            </div>

            <el-tabs v-model="activeTab" class="tabs">
                <el-tab-pane :label="`${$t('device_stats.tab_all')} (${totalCount})`" name="all"/>
                <el-tab-pane :label="`${$t('device_stats.tab_pass')} (${passCount})`" name="pass"/>
                <el-tab-pane :label="`${$t('device_stats.tab_blocked')} (${blockedCount})`" name="blocked"/>
            </el-tabs>

            <div v-if="!loading && filteredLogs.length === 0" class="empty">
                {{
                activeTab === 'blocked'
                ? $t('device_stats.empty_blocked')
                : activeTab === 'pass'
                ? $t('device_stats.empty_pass')
                : $t('device_stats.empty_all')
                }}
            </div>

            <!-- PC：表格 -->
            <el-table
                    v-if="!isMobile && filteredLogs.length"
                    :data="pagedLogs"
                    style="width: 100%"
                    :row-class-name="rowClass"
            >
                <!-- 加宽了时间列表现宽度以防裁切 -->
                <el-table-column :label="$t('audit.time')" width="140">
                    <template #default="{ row }">
                        <span class="mono">{{ fmtTimeOnly(row.time) }}</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('entertainment.type')" width="92">
                    <template #default="{ row }">
                        <el-tag size="small" :type="qtypeTagType(row.query_type)" effect="light">
                            {{ row.query_type || '-' }}
                        </el-tag>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('stats.domain_address')" min-width="220">
                    <template #default="{ row }">
                        <div class="domain-cell">
                            <span class="domain">{{ row.domain || '-' }}</span>
                            <el-badge v-if="row.count > 1" :value="`${row.count}${$t('device_stats.times_unit')}`" class="count-badge" type="info" />
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.status')" min-width="220">
                    <template #default="{ row }">
                        <div class="status">
                            <el-tag size="small" :type="blockedTagType(row)" effect="light">
                                {{ rowBlockedText(row) }}
                            </el-tag>
                        </div>
                    </template>
                </el-table-column>
            </el-table>

            <!-- 手机：卡片列表 -->
            <div v-else-if="isMobile && filteredLogs.length" class="mobile-list">
                <el-card
                        v-for="r in pagedLogs"
                        :key="r._k"
                        shadow="never"
                        class="m-card"
                        :class="mobileRowClass(r)"
                >
                    <div class="m-top">
                        <div class="m-left">
                            <div class="m-time mono">{{ fmtTimeOnly(r.time) }}</div>
                            <el-tag size="small" :type="qtypeTagType(r.query_type)" effect="light">
                                {{ r.query_type || '-' }}
                            </el-tag>
                        </div>
                        <el-tag size="small" :type="blockedTagType(r)" effect="light">
                            {{ rowBlockedText(r) }}
                        </el-tag>
                    </div>

                    <div class="m-domain">
                        {{ r.domain || '-' }}
                        <el-tag v-if="r.count > 1" size="small" type="info" effect="plain" class="m-count-tag">
                            {{ r.count }}{{$t('device_stats.times_unit')}}
                        </el-tag>
                    </div>
                </el-card>
            </div>

            <div class="pagination-box" v-if="filteredLogs.length > 0">
                <el-pagination
                    v-model:current-page="page"
                    v-model:page-size="pageSize"
                    :total="filteredLogs.length"
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
    import {ref, computed, onMounted, onUnmounted, watch} from 'vue'
    import {useI18n} from 'vue-i18n'
    import {useRoute, useRouter} from 'vue-router'
    import { getDeviceStats, syncAllStats } from '@/api/stats'
    import {Search, RefreshRight, Connection} from '@element-plus/icons-vue'
    import { ElMessage } from 'element-plus'

    // ✅ 复用 common.js 的设备类型图标/颜色（跟首页一致）
    import { getDeviceTypeIcon, getDeviceTypeColor, normalizeDeviceType, formatGroupName } from '@/utils/common.js'

    const route = useRoute()
    const router = useRouter()
    const {t} = useI18n()
    const deviceId = computed(() => route.params.id)

    const loading = ref(false)
    const syncing = ref(false)
    const info = ref({id: null, name: '', ip: '', mac: '', group_name: '', type: 0})
    const summary = ref({total: 0, blocked: 0})
    const logs = ref([])
    const source = ref('')
    const availableDates = ref([])

    // 手机适配
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)

    // 日期
    const todayStr = () => {
        const d = new Date()
        const y = d.getFullYear()
        const m = String(d.getMonth() + 1).padStart(2, '0')
        const dd = String(d.getDate()).padStart(2, '0')
        return `${y}-${m}-${dd}`
    }
    const selectedDate = ref(todayStr())
    const timeFrom = ref('')
    const timeTo = ref('')
    
    // 用于真实生效的查询条件，点击“查询”后才赋值
    const activeTimeFrom = ref('')
    const activeTimeTo = ref('')

    const isTodaySelected = computed(() => selectedDate.value === todayStr())

    // Tabs + 分页
    const activeTab = ref('all')
    const page = ref(1)
    const pageSize = ref(20)
    watch([activeTab, pageSize], () => {
        page.value = 1
    })

    // source 提示
    const sourceText = computed(() => {
        if (source.value === 'agh_realtime') return t('device_stats.source_realtime')
        if (source.value === 'local_db') return t('device_stats.source_history')
        return String(source.value || '-')
    })
    
    // 是否为主动查询模式（只有真正提交了时间才算）
    const isQueryMode = computed(() => !!(activeTimeFrom.value || activeTimeTo.value))

    const sourceTagType = computed(() => {
        if (source.value === 'redis_live') return 'primary'
        if (source.value === 'local_db') return 'warning'
        return 'info'
    })

    // 分类统计 (基于当前 logs 列表，用于 Tabs 标签)
    // 逻辑：
    // 非查询模式 (isQueryMode false)：直接获取行数，严格维持列表可见行数 (至多 500)
    // 查询模式 (isQueryMode true)：获取实际发生频次，累加日志后折叠的 count 权重
    const logsTabStats = computed(() => {
        const total = logs.value.length
        const blocked = logs.value.filter(r => Number(r.is_blocked) === 1).length
        return {
            total,
            blocked,
            pass: Math.max(0, total - blocked)
        }
    })

    const totalCount = computed(() => logsTabStats.value.total)
    const blockedCount = computed(() => logsTabStats.value.blocked)
    const passCount = computed(() => logsTabStats.value.pass)
    const blockedRate = computed(() => {
        const rawTotal = summary.value.total
        if (!rawTotal) return 0
        return Math.round((summary.value.blocked / rawTotal) * 1000) / 10
    })

    // 过滤 + 分页
    const filteredLogs = computed(() => {
        if (activeTab.value === 'blocked') return logs.value.filter(r => Number(r.is_blocked) === 1)
        if (activeTab.value === 'pass') return logs.value.filter(r => Number(r.is_blocked) !== 1)
        return logs.value
    })
    const pagedLogs = computed(() => {
        const p = Number(page.value || 1)
        const s = Number(pageSize.value || 20)
        const start = (p - 1) * s
        return filteredLogs.value.slice(start, start + s)
    })

    // 日期禁用：只允许 availableDates
    const disabledDate = (dateObj) => {
        if (!dateObj) return false
        const y = dateObj.getFullYear()
        const m = String(dateObj.getMonth() + 1).padStart(2, '0')
        const d = String(dateObj.getDate()).padStart(2, '0')
        const key = `${y}-${m}-${d}`

        if (!availableDates.value || availableDates.value.length === 0) {
            return key !== todayStr()
        }
        return !availableDates.value.includes(key)
    }

    // 工具：时间
    const fmtTimeOnly = (isoLike) => {
        if (!isoLike) return '-'
        const d = new Date(isoLike)
        if (isNaN(d.getTime())) return String(isoLike).slice(11, 19) || String(isoLike)
        return d.toLocaleTimeString([], {hour12: false})
    }
    const isLateNight = (isoLike) => {
        const d = new Date(isoLike)
        if (isNaN(d.getTime())) return false
        const h = d.getHours()
        return h >= 0 && h < 6
    }

    // query_type 上色
    const qtypeTagType = (t) => {
        const x = String(t || '').toUpperCase()
        if (x === 'A') return 'primary'
        if (x === 'AAAA') return 'warning'
        if (x === 'HTTPS' || x === 'SVCB') return 'warning'
        if (x === 'PTR') return 'info'
        return 'info'
    }
    const PALETTE = [
        {bg: 'var(--tag-blue-bg)', fg: 'var(--tag-blue-fg)', bd: 'var(--tag-blue-bd)'},
        {bg: 'var(--tag-green-bg)', fg: 'var(--tag-green-fg)', bd: 'var(--tag-green-bd)'},
        {bg: 'var(--tag-orange-bg)', fg: 'var(--tag-orange-fg)', bd: 'var(--tag-orange-bd)'},
        {bg: 'var(--tag-purple-bg)', fg: 'var(--tag-purple-fg)', bd: 'var(--tag-purple-bd)'}
    ]
    const tagStyle = (gid) => {
        const c = PALETTE[Number(gid || 0) % PALETTE.length]
        return {backgroundColor: c.bg, color: c.fg, borderColor: c.bd, borderWidth: '1px', borderStyle: 'solid'}
    }

    // row-level status text
    const rowBlockedText = (row) => {
        const s = statusText(row?.status)
        if (s && s !== '-' && s !== t('device_stats.tab_blocked')) return s

        const isBlocked = Number(row?.is_blocked) === 1
        return isBlocked ? (s || t('device_stats.tab_blocked')) : t('device_stats.tab_pass')
    }
    const blockedTagType = (row) => (Number(row?.is_blocked) === 1 ? 'danger' : 'success')

    // status 文本
    const statusText = (s) => {
        const v = String(s || '')
        if (!v || v === '-' || v === 'Blocked' || v === 'NotFilteredNotFound') return ''
        
        if (v === 'Allowed') return t('device_stats.tab_pass')
        
        // Detailed reasons mapping from i18n
        const reasonKeys = [
            'FilteredBlockList',
            'FilteredBlockedService',
            'FilteredSafeBrowsing',
            'FilteredSafeSearch',
            'FilteredParental',
            'Rewrite',
            'AllowList'
        ]

        for (const key of reasonKeys) {
            if (v.startsWith(key)) {
                const label = t(`device_stats.reasons.${key}`)
                const rest = v.slice(key.length).trim()
                return `${label}${rest ? ': ' + rest : ''}`
            }
        }

        if (v.includes('Filtered') || v.includes('BlackList') || v.includes('FilterList')) {
            if (v.includes('(')) return v
            return t('device_stats.rule_intercept', { rule: v })
        }
        if (v.includes('Services')) {
            return t('device_stats.service_block', { service: v })
        }
        
        return v
    }

    // 行样式
    const rowClass = ({row}) => {
        const blocked = Number(row?.is_blocked) === 1
        const late = isLateNight(row?.time)
        if (late) return 'row-late'
        if (blocked) return 'row-blocked'
        return ''
    }
    const mobileRowClass = (row) => {
        const blocked = Number(row?.is_blocked) === 1
        const late = isLateNight(row?.time)
        return late ? 'm-late' : (blocked ? 'm-blocked' : '')
    }

    const onPagerChange = () => {
    }

    const handleSearch = () => {
        // 整点极简模式：自动补齐分钟区间。例如选了 01 到 10，则查 01:00 至 10:59 的所有日志
        activeTimeFrom.value = timeFrom.value ? `${timeFrom.value}:00` : ''
        activeTimeTo.value = timeTo.value ? `${timeTo.value}:59` : ''
        page.value = 1
        load()
    }

    const changeDate = (d) => {
        selectedDate.value = d
        page.value = 1
        load()
    }

    const onDateChange = async () => {
        // 就地刷新，也可不调用 load，由用户点查询按钮决定
        // 为了体验一致，这里也可以什么都不做，或者只重置页码
        page.value = 1
    }

    const normalizeAvailableDates = (data) => {
        const arr = data?.availableDates || data?.available_dates || data?.available_dates_list || []
        return Array.isArray(arr) ? arr.map(x => String(x)) : []
    }

    const load = async () => {
        if (!deviceId.value) return
        loading.value = true
        try {
            const params = { date: selectedDate.value }
            if (activeTimeFrom.value) params.time_from = activeTimeFrom.value
            if (activeTimeTo.value) params.time_to = activeTimeTo.value
            
            const res = await getDeviceStats(deviceId.value, params)
            const data = res?.data || res

            info.value = data?.info || {}
            summary.value = data?.summary || {total: 0, blocked: 0}
            source.value = data?.source || ''
            availableDates.value = normalizeAvailableDates(data)

            if (availableDates.value.length && !availableDates.value.includes(selectedDate.value)) {
                // 如果当前选中的日期不在可选列表中，默认选第一个（最新）
                selectedDate.value = availableDates.value[0]
                // 递归载入一次
                await load()
                return
            }

            const arr = Array.isArray(data?.logs) ? data.logs : []
            logs.value = arr.map((x, idx) => ({...x, _k: `${x.time || ''}-${idx}`}))
        } finally {
            loading.value = false
        }
    }

    const syncAdGuardConfig = async () => {
        syncing.value = true
        try {
            await syncAllStats()
            ElMessage.success(t('device_stats.sync_success'))
            await load()
        } catch (e) {
            console.error('Sync failed', e)
        } finally {
            syncing.value = false
        }
    }

    const goBack = () => router.back()

    onMounted(async () => {
        window.addEventListener('resize', onResize)
        await load()
    })
    onUnmounted(() => {
        window.removeEventListener('resize', onResize)
    })
</script>

<style scoped>
    .page {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .card {
        border-radius: 12px;
        background: var(--bg-card);
        border: 1px solid var(--border-light);
    }

    /* ✅ 设备名图标（复用 common.js 的 icon + color） */
    .type-icon {
        font-size: 16px;
        margin-right: 8px;
        flex: 0 0 auto;
        vertical-align: -2px;
    }

    .dev-text {
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    /* ---------------- PC 顶部布局（原逻辑） ---------------- */
    .top-wrap {
        display: flex;
        justify-content: space-between;
        gap: 12px;
        flex-wrap: wrap;
        align-items: flex-start;
    }

    .name-line {
        display: flex;
        align-items: flex-start;
        gap: 10px;
    }

    .dev-icon {
        font-size: 22px;
        margin-top: 2px;
    }

    /* 保持原尺寸，只换内容为动态 icon */
    .name-block {
        min-width: 0;
    }

    .name-row {
        display: flex;
        align-items: baseline;
        gap: 10px;
        flex-wrap: wrap;
    }

    .section-title {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 20px;
        padding-bottom: 12px;
        border-bottom: 2px solid var(--border-light);
        flex-wrap: wrap; /* 改为允许换行 */
        gap: 12px;
    }
    
    .title-left {
        font-size: 16px;
        font-weight: 800;
        color: var(--text-primary);
        white-space: nowrap;
    }

    .title-right {
        display: flex;
        align-items: center;
        gap: 8px;
        flex: 1;
        justify-content: flex-end;
    }

    .id-line {
        display: flex;
        align-items: center;
        gap: 8px;
        font-size: 12px;
        color: var(--text-regular);
        flex-wrap: wrap;
    }

    .id-item {
        display: inline-flex;
        align-items: center;
        gap: 6px;
    }

    .id-k {
        color: #999;
    }

    .id-v {
        color: var(--text-primary);
        font-weight: 700;
    }

    .id-dot {
        color: #bbb;
    }

    .sub {
        margin-top: 6px;
        display: flex;
        gap: 8px;
        flex-wrap: wrap;
        margin-right: 10px;
    }

    .right-tools {
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: wrap;
        justify-content: flex-end;
        margin-left: auto;
    }

    .dates-hint {
        margin-top: 10px;
        font-size: 12px;
        color: #999;
    }

    /* ---------------- 统计卡片 ---------------- */
    .stats {
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 10px;
    }

    .stat {
        background: var(--bg-subtle);
        border: 1px solid var(--border-light);
        border-radius: 12px;
        padding: 10px;
    }

    .stat .label {
        font-size: 12px;
        color: #888;
    }

    .stat .value {
        font-size: 18px;
        font-weight: 900;
        margin-top: 4px;
        color: var(--text-primary);
    }

    .stat .value.danger {
        color: #d93025;
    }

    /* ---------------- 流水区 ---------------- */
    .section-title {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin-bottom: 6px;
        font-weight: 900;
    }

    .tabs {
        margin-bottom: 8px;
    }

    .domain-cell {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .count-badge {
        font-size: 10px;
    }

    :deep(.count-badge .el-badge__content) {
        font-size: 10px;
        height: 16px;
        line-height: 16px;
        padding: 0 4px;
        border-radius: 8px;
        background-color: var(--bg-subtle);
        color: var(--text-regular);
        border: 1px solid var(--border-light);
    }

    .domain {
        font-weight: 800;
        color: var(--text-primary);
        word-break: break-word;
    }

    .status {
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: wrap;
    }

    .status-text {
        color: var(--text-regular);
        font-size: 12px;
    }

    .empty {
        padding: 14px;
        color: #999;
        text-align: center;
    }

    :deep(.row-blocked td) {
        background: rgba(239, 68, 68, 0.05) !important;
    }

    :deep(.row-late td) {
        background: rgba(59, 130, 246, 0.05) !important;
    }

    /* ---------------- 手机卡片流水 ---------------- */
    .mobile-list {
        display: flex;
        flex-direction: column;
        gap: 10px;
    }

    .m-card {
        border-radius: 12px;
        padding: 10px 12px; /* Reduce default 14px padding */
    }

    .m-top {
        display: flex;
        justify-content: space-between;
        align-items: center;
        gap: 10px;
        margin-bottom: 4px; /* Reduced from 8px */
    }

    .m-left {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .m-time {
        font-size: 12px;
        color: var(--text-regular);
    }

    .m-domain {
        font-weight: 900;
        font-size: 13px;
        color: var(--text-primary);
        margin-bottom: 2px; /* Reduced from 6px */
        word-break: break-all;
        display: flex;
        align-items: center;
        gap: 6px;
    }

    .m-count-tag {
        font-size: 10px;
        height: 18px;
        padding: 0 4px;
        border-radius: 4px;
        font-weight: normal;
    }

    .m-status {
        font-size: 12px;
        color: var(--text-regular);
        word-break: break-word;
    }

    .m-blocked {
        background: rgba(239, 68, 68, 0.05) !important;
    }

    .m-late {
        background: rgba(59, 130, 246, 0.05) !important;
    }

    .mono {
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
    }

    /* ---------------- 手机端：你要的布局优化点（核心） ---------------- */
    .top-mobile {
        display: flex;
        flex-direction: column;
        gap: 10px;
    }

    .m-head {
        display: flex;
        align-items: center;
        gap: 10px;
    }

    .back-btn {
        padding: 0;
    }

    .back-arrow {
        font-size: 18px;
        font-weight: 900;
        color: var(--text-primary);
    }

    .m-head-right {
        flex: 1;
        min-width: 0;
        display: flex;
        flex-direction: column;
        gap: 6px;
    }

    .m-title-row {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
        width: 100%;
    }

    .m-title-left {
        display: flex;
        align-items: center;
        gap: 8px;
        min-width: 0;
        flex: 1;
    }
    .m-dev-name {
        font-size: 16px;
        font-weight: 800;
        color: var(--text-primary);
        line-height: 1.3;
    }

    .m-tags {
        display: inline-flex;
        align-items: center;
        gap: 8px; /* 增加间距 */
        flex: 0 0 auto;
        white-space: nowrap;
    }

    .m-action-row {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        margin-top: 4px;
    }

    .m-btns {
        display: flex;
        align-items: center;
        gap: 6px;
    }

    .m-sync-btn {
        height: 28px;
        padding: 0 8px;
        border-radius: 8px;
    }

    .m-back-btn {
        height: 28px;
        padding: 0 12px;
        border-radius: 8px;
        font-size: 12px;
        font-weight: 700;
    }

    .m-ids {
        font-size: 12px;
        color: var(--text-regular);
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: wrap;
    }

    .m-ids .k {
        color: var(--text-secondary);
        margin-right: 4px;
    }

    .m-id {
        color: var(--text-primary);
        font-weight: 700;
    }

    .dot {
        color: var(--border-light);
    }

    .m-tools {
        display: flex;
        gap: 10px;
        align-items: center;
    }

    .pc-date-list, .m-date-list {
        display: flex;
        gap: 16px;
        flex-wrap: nowrap;
        overflow-x: auto;
        padding: 4px 0;
        -webkit-overflow-scrolling: touch;
    }

    .date-item {
        flex: 0 0 auto;
        font-size: 14px;
        cursor: pointer;
        color: var(--text-regular);
        transition: all 0.2s;
        font-weight: 500;
        position: relative;
    }

    .date-item:hover {
        color: var(--primary-color);
    }

    .date-item.active {
        color: var(--primary-color) !important;
        font-weight: 700;
    }
    
    .date-item.active::after {
        content: '';
        position: absolute;
        bottom: -4px;
        left: 0;
        right: 0;
        height: 2px;
        background: var(--primary-color);
        border-radius: 2px;
    }

    .date-item.today {
        color: #10b981; /* 绿色 */
    }
    
    .date-item.today.active {
        color: #10b981 !important;
    }

    .date-item.today.active::after {
        background: #10b981 !important;
    }
    
    .q-label {
        font-size: 13px;
        color: var(--text-regular);
    }
    
    .q-sep {
        color: var(--text-secondary);
        margin: 0 2px;
    }

    .pc-date-list {
        margin-right: 12px;
    }

    .m-date-list {
        margin-top: 10px;
    }
    
    /* ✅ 手机端：统计必须同一行（三列） */
    @media (max-width: 768px) {
        .stats {
            grid-template-columns: repeat(3, minmax(0, 1fr));
            gap: 8px;
        }

        .stat {
            padding: 10px 8px;
        }

        .stat .value {
            font-size: 16px;
        }

        .stat .label {
            font-size: 11px;
        }

        .section-title {
            flex-direction: row; /* 强制横向 */
            justify-content: space-between;
            align-items: center;
        }

        .title-right {
            width: auto;
            justify-content: flex-end;
            flex-wrap: nowrap; /* 强制不换行 */
            gap: 4px;
        }

        .right-tools {
            width: auto;
            justify-content: flex-end;
        }

        .ell {
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
    }

    :deep(.el-card__body) {
        padding: 5px !important;
    }

    @media (max-width: 768px) {
        :deep(.el-card__body) {
            padding: 10px !important;
        }
    }
    .pagination-box {
        margin-top: 14px;
        display: flex;
        justify-content: flex-end;
        padding: 0 14px 14px 14px;
    }

    @media (max-width: 768px) {
        .m-card {
            padding: 8px 10px;
        }
    }
</style>
