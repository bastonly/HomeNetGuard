<template>
    <div class="page">
        <div class="header-row">
            <div class="header-left">
                <div class="title-row">
                    <div class="title-wrapper">
                        <el-icon class="page-main-icon"><Warning /></el-icon>
                        <h2 class="title">{{ $t('audit.title') }}</h2>
                    </div>
                    
                    <el-button
                        v-if="isMobile"
                        type="primary"
                        plain
                        size="small"
                        round
                        @click="handleMarkAllAsRead"
                        :loading="markingAllRead"
                    >
                        {{ $t('audit.mark_read_all') }}
                    </el-button>
                </div>
                <div class="sub">{{ $t('audit.sub') }}</div>
            </div>

            <div class="actions" :class="{ mobile: isMobile }">
                <div class="filter-group" :class="{ mobile: isMobile }">
                    <!-- 第一行：级别 (手机端独占一行) -->
                    <div class="level-row" v-if="isMobile">
                        <el-radio-group v-model="filterLevel" size="default" @change="loadLogs" class="level-filter-mobile">
                            <el-radio-button value="">{{ $t('audit.all') }}</el-radio-button>
                            <el-radio-button value="critical">{{ $t('audit.level.critical') }}</el-radio-button>
                            <el-radio-button value="error">{{ $t('audit.level.error') }}</el-radio-button>
                            <el-radio-button value="warning">{{ $t('audit.level.warning') }}</el-radio-button>
                            <el-radio-button value="info">{{ $t('audit.level.info') }}</el-radio-button>
                        </el-radio-group>
                    </div>

                    <!-- 第二行：时间 + 搜索 (PC端合并显示) -->
                    <div class="search-row">
                        <el-date-picker
                            v-model="targetDate"
                            type="date"
                            :placeholder="$t('audit.select_date')"
                            format="YYYY-MM-DD"
                            value-format="YYYY-MM-DD"
                            @change="loadLogs"
                            :size="isMobile ? 'small' : 'default'"
                            :style="{ width: isMobile ? '110px' : '150px' }"
                        />
                        
                        <el-radio-group v-if="!isMobile" v-model="filterLevel" size="default" @change="loadLogs" class="level-filter">
                            <el-radio-button value="">{{ $t('audit.all') }}</el-radio-button>
                            <el-radio-button value="critical">{{ $t('audit.level.critical') }}</el-radio-button>
                            <el-radio-button value="error">{{ $t('audit.level.error') }}</el-radio-button>
                            <el-radio-button value="warning">{{ $t('audit.level.warning') }}</el-radio-button>
                            <el-radio-button value="info">{{ $t('audit.level.info') }}</el-radio-button>
                        </el-radio-group>

                        <el-input
                            v-model="searchKeyword"
                            :placeholder="$t('audit.search_placeholder')"
                            :prefix-icon="SearchIcon"
                            clearable
                            @keyup.enter="loadLogs"
                            :size="isMobile ? 'small' : 'default'"
                            :style="{ width: isMobile ? '120px' : '180px' }"
                        />
                        
                        <el-button 
                            type="primary" 
                            :icon="SearchIcon" 
                            @click="loadLogs" 
                            :loading="loading"
                            :size="isMobile ? 'small' : 'default'"
                            class="search-btn"
                        >
                            {{ isMobile ? '' : $t('audit.query') }}
                        </el-button>
                    </div>
                </div>

                <div class="action-btns" v-if="!isMobile">
                    <el-button @click="handleMarkAllAsRead" :loading="markingAllRead" type="primary" plain>
                        {{ $t('audit.mark_read_all') }}
                    </el-button>
                    <el-button :loading="loading" @click="loadLogs" :icon="Refresh">
                        {{ $t('home.refresh') }}
                    </el-button>
                    <el-button type="danger" plain :icon="Delete" @click="confirmClearAll">
                        {{ $t('audit.clear_logs') }}
                    </el-button>
                </div>
            </div>
        </div>

        <div class="table-shell mobile-naked">
            <el-table
                    v-if="!isMobile"
                    :data="pagedLogs"
                    v-loading="loading"
                    class="modern-table"
                    style="width: 100%"
                    :empty-text="loading ? $t('device_mgmt.loading') : $t('audit.no_logs')"
            >
                <el-table-column :label="$t('audit.time')" width="200">
                    <template #default="{row}">
                        <div class="time-cell">
                            <span class="date-text">{{ row.updated_at.split(' ')[0] }}</span>
                            <span class="time-text">{{ row.updated_at.split(' ')[1] }}</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('audit.level_label')" width="100">
                    <template #default="{row}">
                        <div class="level-cell">
                            <div class="read-dot" v-if="row.is_read === 0" :title="$t('audit.unread')"></div>
                            <el-tag :type="getLevelTagType(row.level)" size="small" effect="dark" round>
                                {{ getLevelLabel(row.level) }}
                            </el-tag>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('audit.source')" min-width="200">
                    <template #default="{row}">
                        <div class="device-info">
                            <code class="ip">{{ row.source_ip }}</code>
                            <el-tag v-if="row.group_name && row.group_name !== 'N/A'"
                                    :style="tagStyle(row.group_name)"
                                    size="small"
                                    round>
                                {{ formatGroupName({ name: row.group_name }, $t) }}
                            </el-tag>
                            <span v-else class="mac-hint">{{ row.source_mac }}</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('audit.operator')" width="120">
                    <template #default="{row}">
                        <div class="user-cell">
                            <el-tag v-if="row.source_user" type="info" size="small" effect="plain" round>
                                {{ row.source_user }}
                            </el-tag>
                            <span v-else class="none-text">-</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('audit.details')" min-width="320">
                    <template #default="{row}">
                        <div class="content-cell">
                             <span class="main-msg">{{ $t(maskSensitiveInfo(row.content), { user: row.source_user, group: formatGroupName({ name: row.group_name }, $t) }) }}</span>
                            <el-badge
                                    v-if="row.occurrence_count > 1"
                                    :value="row.occurrence_count"
                                    :max="999"
                                    class="count-badge"
                                    type="danger"
                            />
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('common.actions')" width="120" align="right" fixed="right">
                    <template #default="{row}">
                        <el-button type="primary" link @click="viewDetail(row)">{{ $t('audit.view') }}</el-button>
                    </template>
                </el-table-column>
            </el-table>

            <div v-else class="mobile-container" v-loading="loading">
            <div v-if="!loading && pagedLogs.length === 0" class="empty-state">{{ $t('audit.no_logs') }}</div>

                <div v-for="row in pagedLogs" :key="row.id" class="m-log-card" :class="{ unread: row.is_read === 0 }" @click="viewDetail(row)">
                    <div class="m-log-top">
                        <div class="m-top-left">
                            <div class="m-unread-dot" v-if="row.is_read === 0"></div>
                            <span class="m-time">{{ row.updated_at.split(' ')[1] }}</span>
                        </div>
                        <div class="m-tags">
                            <el-tag v-if="row.group_name && row.group_name !== 'N/A'"
                                    :style="tagStyle(row.group_name)"
                                    size="small"
                                    round>
                                {{ formatGroupName({ name: row.group_name }, $t) }}
                            </el-tag>
                            <el-tag :type="getLevelTagType(row.level)" size="small" round>
                                {{ getLevelLabel(row.level) }}
                            </el-tag>
                        </div>
                    </div>
                    <div class="m-log-body">
                        <div class="m-msg-row">
                            <span class="m-msg">{{ $t(maskSensitiveInfo(row.content), { user: row.source_user, group: formatGroupName({ name: row.group_name }, $t) }) }}</span>
                            <span class="m-count" v-if="row.occurrence_count > 1">x{{ row.occurrence_count }}</span>
                        </div>
                        <div class="m-meta">
                            <code class="mini-ip">{{ row.source_ip }}</code>
                            <span class="m-user" v-if="row.source_user">@{{ row.source_user }}</span>
                            <span class="mini-mac">{{ row.source_mac }}</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <PaginationBar
            v-if="logsView.length > 0"
            v-model:page="page"
            v-model:pageSize="pageSize"
            :total="logsView.length"
            :size="isMobile ? 'small' : 'default'"
            layout="prev, pager, next, sizes"
        />

        <el-dialog v-model="detailVisible" :title="$t('audit.dialog_title')" :width="isMobile ? '95%' : '550px'" round>
            <div class="detail-box" v-if="activeRow">
                <div class="d-grid">
                    <div class="d-item"><span class="dk">{{ $t('audit.source_mac') }}</span> <code>{{ activeRow.source_mac }}</code></div>
                    <div class="d-item"><span class="dk">{{ $t('audit.operator_label') }}</span> <b>{{ activeRow.source_user || '-' }}</b></div>
                    <div class="d-item"><span class="dk">{{ $t('audit.event_tag') }}</span> <el-tag size="small" round>{{ activeRow.event_tag || 'Audit' }}</el-tag></div>
                    <div class="d-item"><span class="dk">{{ $t('audit.occurrence_count') }}</span> <b style="color: #ef4444;">{{ activeRow.occurrence_count }} {{ $t('audit.occurrence_unit') }}</b></div>
                </div>
                <div class="d-item"><span class="dk">{{ $t('audit.last_update') }}</span> {{ activeRow.updated_at }}</div>
                
                <el-tabs v-model="activeTab" class="detail-tabs">
                    <el-tab-pane :label="$t('audit.raw_payload')" name="payload">
                        <div class="payload-section-inner">
                            <div class="d-item-hint"><span class="dk">{{ $t('audit.payload_hint') }}</span></div>
                            <pre class="raw-code">{{ formatJson(activeRow.payload) }}</pre>
                        </div>
                    </el-tab-pane>

                    <el-tab-pane :label="$t('audit.related_domains')" name="domains" v-if="activeRow.event_tag === 'TIMEOUT' || activeRow.content.includes('尝试上网')">
                        <div class="domains-section-inner">
                            <div class="d-item-hint"><span class="dk">{{ $t('audit.dns_hint') }}</span></div>
                            <div v-loading="loadingDetails" class="domain-list">
                                <template v-if="relatedDomains.length > 0">
                                    <div v-for="(d, idx) in relatedDomains" :key="idx" class="domain-tag-item">
                                        <span class="d-time">{{ d.access_time.split(' ')[1].split('.')[0] }}</span>
                                        <span class="d-name">{{ d.domain }}</span>
                                        <el-tag v-if="d.is_blocked" size="small" type="danger" effect="plain" round>{{ $t('audit.blocked') }}</el-tag>
                                    </div>
                                </template>
                                <div v-else-if="!loadingDetails" class="empty-mini">{{ $t('audit.no_related') }}</div>
                            </div>
                        </div>
                    </el-tab-pane>
                </el-tabs>
            </div>
            <template #footer>
                <el-button @click="detailVisible = false" round block style="width: 100%">{{ $t('audit.close') }}</el-button>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import { ref, onMounted, onUnmounted, computed } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { Refresh, Delete, Warning, Connection, Search as SearchIcon } from '@element-plus/icons-vue'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import { getSecurityLogs, markLogAsRead, clearSecurityLogs, getSecurityLogDetails, markAllLogsAsRead } from '@/api/message'
    import bus from '@/utils/bus'
    import { formatGroupName } from '@/utils/common.js'
    import PaginationBar from '@/components/PaginationBar.vue'

    const { t } = useI18n()
    const loading = ref(false)
    const markingAllRead = ref(false)
    const logsAll = ref([])
    const logsView = ref([])
    const filterLevel = ref('')
    const activeRow = ref(null)
    const relatedDomains = ref([])
    const loadingDetails = ref(false)
    const detailVisible = ref(false)
    const getLocalDate = () => {
        const d = new Date()
        const year = d.getFullYear()
        const month = String(d.getMonth() + 1).padStart(2, '0')
        const day = String(d.getDate()).padStart(2, '0')
        return `${year}-${month}-${day}`
    }
    // Set default to today's local date. If user wants it empty, change to ref('')
    const targetDate = ref(getLocalDate())
    const activeTab = ref('payload')
    const searchKeyword = ref('')

    // 分页
    const page = ref(1)
    const pageSize = ref(20)

    const pagedLogs = computed(() => {
        const p = Number(page.value || 1)
        const s = Number(pageSize.value || 20)
        const start = (p - 1) * s
        return logsView.value.slice(start, start + s)
    })

    // 载入数据
    const loadLogs = async () => {
        loading.value = true
        try {
            const params = {
                level: filterLevel.value || undefined,
                date_from: targetDate.value || undefined,
                date_to: targetDate.value || undefined,
                keyword: searchKeyword.value || undefined
            }
            const res = await getSecurityLogs(params)
            logsAll.value = Array.isArray(res) ? res : (res.data || [])
            logsView.value = [...logsAll.value]
            page.value = 1
        } catch (err) {
            ElMessage.error(t('audit.fetch_error'))
        } finally {
            loading.value = false
        }
    }

    const getLevelTagType = (level) => {
        const map = {
            'critical': 'danger',
            'error': 'danger',
            'warning': 'warning',
            'info': 'info',
            'intercept': 'warning'
        }
        return map[level] || 'info'
    }

    const getLevelLabel = (level) => {
        const map = {
            'critical': t('audit.level.critical'),
            'error': t('audit.level.error'),
            'warning': t('audit.level.warning'),
            'info': t('audit.level.info'),
            'intercept': t('audit.level.intercept')
        }
        return map[level] || level
    }

    // 复用你的 PALETTE 逻辑
    const PALETTE = [
        {bg: 'var(--tag-blue-bg)', fg: 'var(--tag-blue-fg)', bd: 'var(--tag-blue-bd)'},
        {bg: 'var(--tag-green-bg)', fg: 'var(--tag-green-fg)', bd: 'var(--tag-green-bd)'},
        {bg: 'var(--tag-orange-bg)', fg: 'var(--tag-orange-fg)', bd: 'var(--tag-orange-bd)'},
        {bg: 'var(--tag-purple-bg)', fg: 'var(--tag-purple-fg)', bd: 'var(--tag-purple-bd)'}
    ]
    const tagStyle = (name) => {
        // 根据名称哈希选择颜色，保持同一个组颜色一致
        const hash = (name || '').split('').reduce((acc, char) => acc + char.charCodeAt(0), 0)
        const c = PALETTE[hash % PALETTE.length]
        return { backgroundColor: c.bg, color: c.fg, borderColor: c.bd, borderWidth: '1px', borderStyle: 'solid' }
    }

    const maskSensitiveInfo = (str) => {
        if (!str) return ''
        // 匹配 device_fingerprint 或 fingerprint，支持 JSON 格式和普通文本格式
        // 例子: "device_fingerprint": "xyz...", device_fingerprint=xyz...
        return str.replace(/((?:device_)?fingerprint"?\s*[:=]\s*"?)([\w\-]+)("?)/gi, (match, prefix, val, suffix) => {
            if (val.length <= 6) return match
            return prefix + '...' + val.slice(-6) + suffix
        })
    }

    const formatJson = (str) => {
        try { 
            const jsonStr = JSON.stringify(JSON.parse(str), null, 2); 
            return maskSensitiveInfo(jsonStr);
        }
        catch { return maskSensitiveInfo(str); }
    }

    const viewDetail = async (row) => {
        activeRow.value = row
        detailVisible.value = true
        relatedDomains.value = []
        activeTab.value = 'payload'
        
        // 如果是拦截事件，尝试加载详情（异步加载，不阻塞页签显示）
        if (row.event_tag === 'TIMEOUT' || row.content === 'cpp.GROUP_TIME_BLOCK' || row.content.includes('禁网时段')) {
            loadingDetails.value = true
            try {
                const res = await getSecurityLogDetails(row.id)
                relatedDomains.value = Array.isArray(res) ? res : (res.data || [])
            } catch (e) {
                console.error('Failed to load domain details', e)
            } finally {
                loadingDetails.value = false
            }
        }
        
        // 如果是未读，调用接口标记为已读，并更新本地状态
        if (row.is_read === 0) {
            try {
                await markLogAsRead(row.id)
                row.is_read = 1
                bus.emit('refresh-unread')
            } catch (e) {
                console.error(e)
            }
        }
    }

    const handleMarkAllAsRead = async () => {
        markingAllRead.value = true
        try {
            await markAllLogsAsRead()
            ElMessage.success(t('audit.mark_read_success'))
            // 更新本地已读状态
            logsAll.value.forEach(row => { row.is_read = 1 })
            logsView.value.forEach(row => { row.is_read = 1 })
            bus.emit('refresh-unread')
        } catch (e) {
            ElMessage.error(t('common.operation_failed'))
        } finally {
            markingAllRead.value = false
        }
    }

    const confirmClearAll = async () => {
        try {
            await ElMessageBox.confirm(t('audit.confirm_clear'), t('audit.high_risk'), {
                confirmButtonText: t('audit.clear_now'),
                cancelButtonText: t('common.cancel'),
                confirmButtonClass: 'el-button--danger',
                type: 'error',
                roundButton: true
            })
            await clearSecurityLogs()
            ElMessage.success(t('audit.reset_success'))
            loadLogs()
        } catch {}
    }

    /* 响应式适配 */
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)

    onMounted(() => {
        window.addEventListener('resize', onResize)
        loadLogs()
    })
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    .page {
        padding: 16px;
        background-color: var(--bg-page);
        display: flex;
        flex-direction: column;
        gap: 14px;
        min-height: 100vh;
        overflow-x: hidden;
    }


    .filter-group {
        display: flex;
        align-items: center;
        gap: 12px;
        flex-wrap: wrap;
    }

    .filter-group.mobile {
        flex-direction: column;
        align-items: stretch;
        width: 100%;
        gap: 10px;
    }

    .level-row {
        width: 100%;
        display: flex;
        justify-content: flex-start;
    }

    .search-row {
        display: flex;
        align-items: center;
        gap: 8px;
        flex-wrap: nowrap;
    }

    .title-wrapper {
        display: flex;
        align-items: center;
        gap: 8px;
        flex: 1; /* Allow title to push button to the right */
    }

    .title-actions {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .page-main-icon {
        font-size: 20px;
        color: #3b82f6;
    }

    .title {
        margin: 0;
        font-size: 16px;
        font-weight: 800;
        color: var(--text-primary);
    }

    .sub {
        font-size: 12px;
        color: var(--text-regular);
        margin-top: 4px;
    }

    .actions {
        display: flex;
        align-items: center;
        gap: 10px;
    }

    /* Table */
    .modern-table {
        border-radius: 14px;
        overflow: hidden;
        box-shadow: 0 1px 3px rgba(0,0,0,0.02);
        border: 1px solid var(--border-light);
        background: var(--bg-card);
    }

    .time-cell {
        display: flex;
        flex-direction: column;
        font-size: 12px;
    }

    .date-text { color: var(--text-secondary); }
    .time-text { color: var(--text-primary); font-weight: 600; }

    .device-info {
        display: flex;
        align-items: center;
        gap: 10px;
    }

    .ip {
        font-family: monospace;
        color: var(--primary-color);
        background: var(--divider-color);
        padding: 2px 6px;
        border-radius: 4px;
    }

    .mac-hint {
        font-size: 11px;
        color: var(--text-secondary);
        font-family: monospace;
    }

    .content-cell {
        display: flex;
        align-items: center;
        gap: 12px;
    }

    .main-msg {
        font-size: 13px;
        color: var(--text-primary);
    }

    /* Mobile List */
    .mobile-container {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .m-log-card {
        background: var(--bg-card);
        border: 1px solid var(--border-light);
        border-radius: 14px;
        padding: 14px;
        box-shadow: 0 1px 2px rgba(0,0,0,0.02);
    }

    .m-log-top {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 8px;
    }

    .m-time { font-size: 12px; color: var(--text-secondary); font-weight: 600; }

    .m-top-left {
        display: flex;
        align-items: center;
    }

    .m-tags { display: flex; gap: 6px; }

    .m-msg-row {
        display: flex;
        justify-content: space-between;
        align-items: flex-start;
        gap: 8px;
    }

    .m-msg {
        font-size: 14px;
        font-weight: 700;
        color: var(--text-primary);
        line-height: 1.4;
    }

    .m-count {
        background: #fee2e2;
        color: #ef4444;
        padding: 1px 6px;
        border-radius: 6px;
        font-size: 11px;
        font-weight: 800;
    }

    .m-meta {
        margin-top: 10px;
        display: flex;
        justify-content: space-between;
        font-size: 11px;
        border-top: 1px dashed var(--divider-color);
        padding-top: 8px;
    }

    .mini-ip { color: var(--primary-color); }
    .m-user { color: var(--text-regular); font-size: 11px; }
    .mini-mac { color: var(--text-secondary); font-family: monospace; }

    /* Dialog */
    .detail-box { display: flex; flex-direction: column; gap: 14px; }
    .d-grid {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 12px;
        padding-bottom: 8px;
        border-bottom: 1px dashed var(--divider-color);
    }
    .user-cell { font-size: 13px; }
    .none-text { color: var(--text-secondary); }
    .dk { font-weight: 600; color: var(--text-regular); font-size: 13px; }
    .raw-code {
        background: #0f172a;
        color: #38bdf8;
        padding: 16px;
        border-radius: 10px;
        font-family: 'Fira Code', monospace;
        font-size: 12px;
        font-family: 'Fira Code', monospace;
        font-size: 12px;
        overflow-x: auto;
        line-height: 1.5;
        white-space: pre-wrap;       /* Allow wrapping */
        word-break: break-all;       /* Break long words */
    }

    .domains-section-inner, .payload-section-inner {
        margin-top: 10px;
    }

    .detail-tabs {
        margin-top: 14px;
    }

    .d-item-hint {
        margin-bottom: 8px;
    }

    .domain-list {
        max-height: 250px;
        overflow-y: auto;
        -webkit-overflow-scrolling: touch; /* Enable smooth scrolling on iOS */
        background: var(--divider-color);
        padding: 12px;
        border-radius: 10px;
        border: 1px solid var(--border-light);
    }

    .domain-tag-item {
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 4px 0;
        font-family: monospace;
        font-size: 13px;
        border-bottom: 1px solid var(--divider-color);
    }

    .d-time { color: var(--text-secondary); font-size: 11px; }
    .d-name { color: var(--text-primary); flex: 1; word-break: break-all; }

    .empty-mini {
        font-size: 12px;
        color: var(--text-secondary);
        padding: 10px 0;
        text-align: center;
    }

    /* Unread Indicator */
    .level-cell {
        display: flex;
        align-items: center;
        gap: 6px;
    }
    .read-dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background-color: #ef4444; /* red dot */
        flex: 0 0 auto;
    }
    
    /* Mobile Unread Highlight */
    .m-unread-dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background-color: #ef4444;
        margin-right: 8px;
        flex-shrink: 0;
    }

    .empty-state {
        text-align: center;
        padding: 40px;
        color: var(--text-secondary);
    }

    @media (max-width: 768px) {

        .actions.mobile {
            width: 100%;
            flex-direction: column;
            gap: 10px;
        }

        .level-filter-mobile {
            display: flex;
            width: 100%;
            justify-content: flex-start;
        }

        .level-filter-mobile :deep(.el-radio-button__inner) {
            padding: 8px 12px;
            font-size: 12px;
        }

        .search-row {
            width: 100%;
            display: flex;
            gap: 8px;
        }

        .search-row :deep(.el-date-editor),
        .search-row :deep(.el-input) {
            flex: 1;
            width: auto !important;
        }

        .search-btn {
            flex: 0 0 auto;
        }
    }
</style>
