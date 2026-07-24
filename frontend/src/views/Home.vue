<template>
    <div class="page-container">
        <div class="overview-card">
            <div class="overview-head">
                <div class="header-left">
                    <h2 class="title">{{ $t('home.group_connectivity') }}</h2>
                    <div class="sub">{{ $t('home.group_sub') }}</div>
                </div>
                <div class="header-right">
                    <div class="stat">
                        {{ $t('home.online') }} <b>{{ groupOnlineCount }}</b> / {{ groupTotalCount }}
                    </div>

                    <el-button v-if="!isMobile" :loading="loading || loadingGroups" @click="load" :icon="Refresh">
                        {{ $t('home.refresh') }}
                    </el-button>
                    <el-button
                        v-else
                        circle
                        :loading="loading || loadingGroups"
                        @click="load"
                        :icon="Refresh"
                        :aria-label="$t('home.refresh')"
                        class="refresh-btn"
                    />
                </div>
            </div>

            <div class="divider"/>
            <div class="overview-body" v-loading="loadingGroups">
                <div v-if="!loadingGroups && groupStatuses.length === 0" class="empty-state">
                    {{ $t('home.no_data') }}
                </div>
                 <div
                    v-for="g in groupStatuses"
                    :key="g.id"
                    class="group-stat-box"
                    :class="[
                        g.is_blocked ? 'status-blocked' : (g.status === 'online' ? 'status-online' : 'status-offline')
                    ]"
                  >
                      <div class="icon-wrapper">
                          <el-icon :size="20">
                              <component :is="getGroupIcon(g.name)"/>
                          </el-icon>
                      </div>
  
                      <div class="info-wrapper">
                          <div class="group-name-text">{{ formatGroupNameLocal(g) }}</div>
  
                          <div class="status-indicator">
                              <template v-if="g.is_blocked && g.kind === 2">
                                  <el-icon class="block-icon"><Lock /></el-icon>
                                  <span class="status-text blocked">{{ $t('home.manual_block') }}</span>
                              </template>
                              <template v-else>
                                  <span class="pulse-dot" :class="g.status === 'online' ? 'dot-online' : 'dot-offline'"/>
                                  <span class="status-text">{{ g.status === 'online' ? $t('home.online_status') : $t('home.offline_status') }}</span>
                              </template>
                          </div>
                      </div>
  
                      <!-- 一键断网/恢复 动作按钮 (仅针对受限组 kind=2) -->
                      <div class="action-wrapper" v-if="g.kind === 2">
                          <!-- 开启断网：仅在未手动拦截且当前状态为联机时显示 -->
                          <el-button 
                              v-if="!g.is_blocked && g.status === 'online'"
                              type="danger" 
                              :icon="CircleClose" 
                              circle 
                              size="small"
                              :title="$t('home.one_click_block')"
                              @click="toggleGroupBlock(g, 1)"
                          />
                          <!-- 恢复上网：仅在手动拦截状态下显示 -->
                          <el-button 
                              v-else-if="g.is_blocked"
                              type="success" 
                              :icon="CircleCheck" 
                              circle 
                              size="small"
                              :title="$t('home.click_to_restore')"
                              @click="toggleGroupBlock(g, 0)"
                          />
                      </div>
                  </div>
            </div>
        </div>

        <div class="kpi-grid">
            <div class="kpi-card kpi-total">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.today_total') }}</div>
                    <div class="kpi-value">{{ formatNumber(summary.total || 0) }}</div>
                </div>
                <el-icon class="kpi-icon"><Memo /></el-icon>
            </div>

            <div class="kpi-card kpi-blocked">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.today_blocked') }}</div>
                    <div class="kpi-value">{{ formatNumber(summary.blocked || 0) }}</div>
                </div>
                <el-icon class="kpi-icon"><CircleClose /></el-icon>
            </div>

            <div class="kpi-card kpi-rate">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.blocked_rate') }}</div>
                    <div class="kpi-value">{{ calculateRate }}%</div>
                </div>
                <el-icon class="kpi-icon"><PieChart /></el-icon>
            </div>

            <!-- AdGuard Home Status Card -->
            <div class="kpi-card kpi-agh" :class="getAghStatusClass">
                <div class="kpi-main">
                    <div class="kpi-label">{{ $t('home.agh_status') }}</div>
                    <div class="kpi-value kpi-status-text">{{ getAghStatusText }}</div>
                </div>

                <div class="kpi-icon-action">
                    <!-- If running and protection enabled: Show Pause Dropdown (Yellow Pause) -->
                    <el-dropdown v-if="aghStatus.running && aghStatus.protection_enabled" trigger="click" @command="(d) => handleAghAction('pause', d)">
                        <div class="kpi-icon-wrapper active-running">
                            <el-icon class="kpi-icon"><VideoPause /></el-icon>
                        </div>
                        <template #dropdown>
                            <el-dropdown-menu>
                                <div class="dropdown-header">{{ $t('home.agh_pause_title') }}</div>
                                <el-dropdown-item command="10m">{{ $t('home.agh_pause_10m') }}</el-dropdown-item>
                                <el-dropdown-item command="30m">{{ $t('home.agh_pause_30m') }}</el-dropdown-item>
                                <el-dropdown-item command="60m">{{ $t('home.agh_pause_1h') }}</el-dropdown-item>
                                <el-dropdown-item command="tomorrow">{{ $t('home.agh_pause_tomorrow') }}</el-dropdown-item>
                                <el-dropdown-item command="" divided>{{ $t('home.agh_pause_forever') }}</el-dropdown-item>
                            </el-dropdown-menu>
                        </template>
                    </el-dropdown>

                    <!-- Else: Show Play Button to start/resume (Blue Play) -->
                    <div v-else 
                         class="kpi-icon-wrapper active-paused pointer" 
                         @click="handleAghAction(!aghStatus.running ? 'start' : 'resume')"
                    >
                        <el-icon class="kpi-icon"><VideoPlay /></el-icon>
                    </div>
                </div>
            </div>
        </div>

        <el-card v-if="!isMobile" shadow="never" class="table-card-container">
            <el-table :data="devices" v-loading="loading" style="width: 100%" class="modern-table">
                <el-table-column :label="$t('home.device_name')" min-width="240">
                    <template #default="scope">
                        <div class="device-name-cell">
                            <el-icon class="type-icon" :style="{ color: getDeviceTypeColor(scope.row.type) }">
                                <component :is="getDeviceTypeIcon(scope.row.type)"/>
                            </el-icon>

                            <span class="device-name">{{ scope.row.name || $t('home.unknown_device') }}</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.network_info')" min-width="260">
                    <template #default="scope">
                        <div class="network-details">
                            <code class="ip-tag">{{ scope.row.ip || '-' }}</code>
                            <span class="mac-tag mono">{{ scope.row.mac || '-' }}</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.status')" width="100">
                    <template #default="s">
                        <el-tag :type="s.row.status === 'online' ? 'success' : 'info'" effect="light" round>
                            {{ $t('device.status.' + s.row.status) }}
                        </el-tag>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups.title')" min-width="150">
                    <template #default="s">
                        <el-tag
                            v-if="s.row.groups_id != null"
                            :style="tagStyle(s.row.groups_id)"
                            size="small"
                            round
                            effect="light"
                        >
                            {{ formatGroupNameLocal({ name: s.row.group_name, remark: s.row.group_remark }) }}
                        </el-tag>
                        <span v-else class="m-group-name">-</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.remark')" min-width="180">
                    <template #default="s">
                        <span class="remark-text">{{ s.row.remark || '-' }}</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.last_seen')" min-width="200">
                    <template #default="s">
                        <span class="time-display">{{ formatTime(s.row.last_seen) }}</span>
                    </template>
                </el-table-column>
            </el-table>
        </el-card>

        <div v-else class="mobile-container" v-loading="loading">
            <div v-for="d in devices" :key="d.mac || d.id" class="m-card">
                <div class="m-header">
                    <div class="m-name">
                        <el-icon class="type-icon mobile" :style="{ color: getDeviceTypeColor(d.type) }">
                            <component :is="getDeviceTypeIcon(d.type)"/>
                        </el-icon>

                        <span class="m-title">{{ d.name || $t('home.unnamed_device') }}</span>
                    </div>

                    <el-tag size="small" :type="d.status === 'online' ? 'success' : 'info'" round>
                        {{ $t('device.status.' + d.status) }}
                    </el-tag>
                </div>

                <div class="m-content">
                    <div class="m-line">
                        <label>{{ $t('home.network_info') }}</label>
                        <span class="v mono">{{ d.ip || '-' }} / {{ d.mac || '-' }}</span>
                    </div>
                    <div class="m-line">
                        <label>{{ $t('home.active') }}</label>
                        <span class="v">{{ formatTime(d.last_seen) }}</span>
                    </div>
                </div>
            </div>

            <div v-if="!loading && devices.length === 0" class="empty-state">
                {{ $t('home.no_device') }}
            </div>
        </div>
    </div>
</template>

<script setup>
    import {ref, computed, onMounted, onUnmounted} from 'vue'
    import { useI18n } from 'vue-i18n'
    import { getDeviceList } from '@/api/device'
    import { getGroupsWithStatus, updateGroup } from '@/api/group'
    import { getHomeStats, getAghStatus, controlAgh } from '@/api/stats'
    import {Refresh, Monitor, Reading, VideoPlay, Timer, Connection, Lock, CircleClose, CircleCheck, Memo, PieChart, Warning, ArrowDown, VideoPause} from '@element-plus/icons-vue'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import http from '@/utils/http'
    import {getDeviceTypeIcon, getDeviceTypeColor, normalizeDeviceType, formatGroupName, formatNumber, checkIsLicenseExpired} from '@/utils/common.js'

    const { t, locale } = useI18n()
    const devices = ref([])
    const groupStatuses = ref([])
    const summary = ref({ total: 0, blocked: 0 })
    const aghStatus = ref({ running: false, protection_enabled: false, protection_disabled_duration: 0 })
    const serverData = ref(null)

    const loading = ref(false)
    const loadingGroups = ref(false)

    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)

    /* 设备组图标（按 key 字符串） */
    const getGroupIcon = (name) => {
        const n = String(name || '').toLowerCase()
        if (n.includes('study')) return Reading
        if (n.includes('fun')) return VideoPlay
        if (n.includes('timed')) return Timer
        if (n.includes('open')) return Connection
        if (n.includes('lock')) return Lock
        return Monitor
    }

    const groupOnlineCount = computed(() => (groupStatuses.value || []).filter(g => g.status === 'online').length)
    const groupTotalCount = computed(() => (groupStatuses.value || []).length)

    const formatGroupNameLocal = (group) => formatGroupName(group, t)

    const getAghStatusClass = computed(() => {
        if (!aghStatus.value.running || aghStatus.value.status === 'failed') return 'agh-offline'
        if (!aghStatus.value.protection_enabled) return 'agh-paused'
        return 'agh-running'
    })

    const getAghStatusText = computed(() => {
        if (!aghStatus.value.running || aghStatus.value.status === 'failed') return 'inactive'
        if (!aghStatus.value.protection_enabled) return 'Paused'
        return 'running'
    })

    const fetchAghStatus = async () => {
        try {
            const res = await getAghStatus()
            aghStatus.value = res.data || res
        } catch (e) {
            console.error('Failed to fetch AGH status', e)
            aghStatus.value = { running: false, protection_enabled: false, status: 'failed' }
        }
    }

    const handleAghAction = async (action, duration = '') => {
        if (checkIsLicenseExpired(serverData.value)) {
            ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
            return
        }

        if (action === 'pause') {
            try {
                await ElMessageBox.confirm(
                    t('home.agh_pause_confirm'),
                    t('home.op_confirm'),
                    { type: 'warning', confirmButtonClass: 'el-button--danger' }
                )
            } catch { return }
        }

        try {
            const res = await controlAgh({ action, duration })
            if (res.data?.ok || res.ok) {
                ElMessage.success(t('common.save_success'))
            } else {
                ElMessage.error(t('common.operation_failed'))
            }
        } catch (e) {
            ElMessage.error(t('common.operation_failed'))
        } finally {
            load()
        }
    }

    const load = async () => {
        loading.value = true
        loadingGroups.value = true
        
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

        // Concurrent requests, but each handled separately so one failure doesn't block others
        const loadDevices = async () => {
            try {
                const devRes = await getDeviceList()
                const arr = Array.isArray(devRes) ? devRes : (devRes?.data || [])
                devices.value = (arr || []).map(d => ({
                    ...d,
                    status: (d.status === '在线' || d.status === 'online') ? 'online' : 'offline',
                    type: normalizeDeviceType(d.type)
                }))
            } catch (e) { console.error('Failed to load devices', e) }
        }

        const loadGroups = async () => {
            try {
                const groupRes = await getGroupsWithStatus()
                groupStatuses.value = Array.isArray(groupRes) ? groupRes : (groupRes?.data || [])
            } catch (e) { console.error('Failed to load groups', e) }
        }

        const loadStats = async () => {
            try {
                const statsRes = await getHomeStats()
                if (statsRes) summary.value = statsRes?.stats?.summary || statsRes?.summary || { total: 0, blocked: 0 }
            } catch (e) { console.error('Failed to load stats', e) }
        }

        const loadAgh = async () => {
            try {
                const aghRes = await getAghStatus()
                if (aghRes) aghStatus.value = aghRes.data || aghRes
            } catch (e) {
                console.error('Failed to fetch AGH status', e)
                aghStatus.value = { running: false, protection_enabled: false, status: 'failed' }
            }
        }

        try {
            await Promise.allSettled([
                loadServerData(),
                loadDevices(),
                loadGroups(),
                loadStats(),
                loadAgh()
            ])
        } finally {
            loading.value = false
            loadingGroups.value = false
        }
    }

    const calculateRate = computed(() => {
        const t = Number(summary.value.total || 0)
        const b = Number(summary.value.blocked || 0)
        if (!t) return 0
        return ((b / t) * 100).toFixed(1)
    })

    /* 柔和设备组色块 */
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

    const toggleGroupBlock = async (group, blockStatus) => {
        if (checkIsLicenseExpired(serverData.value)) {
            ElMessageBox.alert(t('license.expired_alert'), t('home.op_confirm'), { type: 'error' })
            return
        }

        const actionText = blockStatus === 1 ? t('home.one_click_block') : t('home.click_to_restore')
        try {
            await ElMessageBox.confirm(
                t('home.confirm_action', { name: formatGroupNameLocal(group), action: actionText }),
                t('home.op_confirm'),
                {
                    confirmButtonText: t('home.confirm'),
                    cancelButtonText: t('home.cancel'),
                    type: blockStatus === 1 ? 'warning' : 'success'
                }
            )
        } catch {
            return
        }

        try {
            await updateGroup(group.id, {
                remark: group.remark,
                kind: group.kind || 1,
                is_blocked: blockStatus
            })
            ElMessage.success(t('home.op_success', { action: actionText }))
            await load()
        } catch (err) {
            ElMessage.error(t('home.op_fail', { action: actionText }))
        }
    }

    const formatTime = (ts) => {
        if (!ts) return '-'
        return new Date(Number(ts) * 1000).toLocaleString()
    }

    onMounted(() => {
        window.addEventListener('resize', onResize)
        load()
    })
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    /* 页面整体 */
    .page-container {
        padding: 16px;
        background-color: var(--bg-page);
        display: flex;
        flex-direction: column;
        gap: 14px;
    }

    /* 顶部概览卡片 */
    .overview-card {
        border-radius: 16px;
        border: 1px solid var(--border-light);
        background: var(--bg-header);
        overflow: hidden;
    }

    .overview-head {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 14px;
        padding: 14px;
    }

    .header-left {
        min-width: 220px;
        display: flex;
        flex-direction: column;
        gap: 6px;
    }

    .title {
        margin: 0;
        font-size: 16px;
        font-weight: 800;
        color: var(--text-primary);
        line-height: 1.2;
    }

    .sub {
        font-size: 12px;
        color: var(--text-regular);
        line-height: 1.35;
    }

    .header-right {
        display: flex;
        align-items: center;
        justify-content: flex-end;
        gap: 12px;
        flex-wrap: nowrap;
    }

    .stat {
        font-size: 12px;
        color: var(--text-regular);
        white-space: nowrap;
    }

    .stat b {
        color: var(--primary-color);
    }

    .divider {
        height: 1px;
        background: var(--divider-color);
    }

    /* ===================== 设备组卡片：PC端并排 ===================== */
    .overview-body {
        padding: 14px;
        display: grid;
        grid-template-columns: repeat(2, 1fr);
        gap: 12px;
    }

    @media (min-width: 768px) {
        .overview-body {
            /* 只要空间足够，就排成一行 */
            grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
        }
    }

    .group-stat-box {
        display: flex;
        align-items: center;
        padding: 14px 12px;
        border-radius: 14px;
        border: 1px solid var(--border-light);
        background: var(--bg-card);
    }

    .icon-wrapper {
        width: 42px;
        height: 42px;
        border-radius: 14px;
        display: flex;
        align-items: center;
        justify-content: center;
        margin-right: 12px;
        flex: 0 0 auto;
    }

    .status-online {
        background: var(--status-online-bg);
        border-color: var(--status-online-border);
    }

    .status-online .icon-wrapper {
        background: #10b981;
        color: #fff;
    }

    .status-blocked {
        background: var(--status-blocked-bg);
        border-color: var(--status-blocked-border);
    }

    .status-blocked .icon-wrapper {
        background: #f43f5e;
        color: #fff;
    }

    .status-offline {
        background: var(--status-offline-bg);
        border-color: var(--status-offline-border);
    }

    .status-offline .icon-wrapper {
        background: #94a3b8;
        color: #fff;
    }

    .info-wrapper {
        min-width: 0;
        display: flex;
        flex-direction: column;
        gap: 6px;
    }

    .group-name-text {
        font-size: 13px;
        font-weight: 800;
        color: var(--text-primary);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .status-indicator {
        display: flex;
        align-items: center;
        gap: 8px;
        color: #64748b;
        font-size: 12px;
    }

    .pulse-dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
    }

    .dot-online { background: #10b981; }
    .dot-offline { background: #94a3b8; }

    .block-icon {
        color: #f43f5e;
        font-size: 14px;
    }

    .status-text.blocked {
        color: #e11d48;
        font-weight: 700;
    }

    .action-wrapper {
        margin-left: auto;
        opacity: 0.6;
        transition: opacity 0.2s;
    }

    .group-stat-box:hover .action-wrapper {
        opacity: 1;
    }

    /* KPI Grid */
    .kpi-grid {
        display: grid;
        grid-template-columns: repeat(4, minmax(0, 1fr));
        gap: 12px;
    }

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

    .kpi-total { background: var(--bg-card); }
    .kpi-blocked { background: var(--bg-card); }
    .kpi-rate { background: var(--bg-card); }

    .kpi-agh {
        background: var(--bg-card);
        transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }

    .agh-running { background: var(--status-online-bg); border-color: var(--status-online-border); }
    .agh-paused { background: var(--status-blocked-bg); border-color: var(--status-blocked-border); }
    .agh-offline { background: var(--status-offline-bg); border-color: var(--status-offline-border); }

    /* KPI 颜色修改 */
    .kpi-total .kpi-value { color: #2563eb; }
    .kpi-blocked .kpi-value { color: #ef4444; }
    .kpi-rate .kpi-value { color: #ef4444; }
    
    .agh-running .kpi-status-text { color: #2563eb; } /* Running -> Blue */
    .agh-paused .kpi-status-text { color: #eab308; } /* Paused -> Yellow */
    .agh-offline .kpi-status-text { color: #ef4444; } /* Inactive/Offline -> Red */

    .kpi-icon-action {
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .kpi-icon-wrapper {
        width: 44px;
        height: 44px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 12px;
        transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        color: rgba(15, 23, 42, 0.15); /* Default gray */
    }

    .kpi-icon-wrapper.active-running {
        color: #eab308; /* Yellow Pause Icon */
    }

    .kpi-icon-wrapper.active-paused {
        color: #2563eb; /* Blue Play Icon */
    }

    .kpi-icon-wrapper.pointer {
        cursor: pointer;
    }

    .kpi-icon-wrapper.pointer:hover {
        transform: scale(1.1);
        background: rgba(15, 23, 42, 0.05);
    }

    .kpi-icon-wrapper.pointer:active {
        transform: scale(0.95);
    }

    .kpi-icon-wrapper.active:hover {
        background: rgba(37, 99, 235, 0.15);
    }

    .kpi-icon-wrapper .el-icon {
        font-size: 32px;
        color: inherit;
    }

    .dropdown-header {
        padding: 4px 10px;
        font-size: 10px;
        color: #94a3b8;
        font-weight: 700;
        text-transform: uppercase;
        letter-spacing: 0.5px;
    }

    :deep(.el-dropdown-menu__item) {
        font-size: 12px;
        padding: 4px 12px;
        line-height: 1.4;
    }

    .kpi-main { display: flex; flex-direction: column; gap: 6px; min-width: 0; }
    .kpi-label { font-size: 12px; color: #64748b; }
    .kpi-value {
        font-size: 20px;
        font-weight: 900;
        line-height: 1;
        font-variant-numeric: tabular-nums;
    }
    .kpi-status-text {
        font-size: 16px;
        margin-top: 4px;
    }
    .kpi-icon { font-size: 30px; color: rgba(15, 23, 42, 0.10); }

    /* ===================== 设备列表：完全恢复原始风格 ===================== */
    .table-card-container {
        border-radius: 14px;
        border: 1px solid var(--border-light);
        background: var(--bg-card);
    }

    .group-text {
        color: var(--text-regular);
        font-size: 13px;
        font-weight: 500;
    }

    .device-name-cell {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
    }

    .device-name {
        font-weight: 800;
        color: var(--text-primary);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .type-icon { font-size: 16px; }
    .type-icon.mobile { font-size: 18px; }

    .network-details {
        display: flex;
        flex-direction: column;
        gap: 6px;
    }

    .ip-tag {
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
        color: #2563eb;
        font-size: 12px;
    }

    .mac-tag { font-size: 12px; color: var(--text-regular); }
    .remark-text { color: var(--text-primary); }
    .time-display { color: var(--text-regular); }
    .mono { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }

    /* Mobile Container */
    .mobile-container {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .m-device-card {
        border-radius: 14px;
        border: 1px solid var(--border-light);
        background: var(--bg-card);
    }

    .m-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
        margin-bottom: 8px;
    }

    .m-group-sep {
        margin: 0 6px;
        color: var(--border-light);
        font-size: 10px;
    }

    .m-group-name {
        font-size: 11px;
        color: var(--text-secondary);
        font-weight: 500;
    }

    .m-time {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
        font-weight: 800;
    }

    .m-name {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
        font-weight: 800;
    }

    .m-title {
        font-size: 15px;
        color: var(--text-primary);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .m-content {
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .m-line {
        display: flex;
        justify-content: space-between;
        font-size: 13px;
        padding-bottom: 8px;
        border-bottom: 1px solid var(--divider-color);
    }

    .m-line:last-child {
        border-bottom: none;
        padding-bottom: 0;
    }

    .m-line label { color: var(--text-secondary); }
    .m-line .v { color: var(--text-primary); }
    .m-line .v.mono { font-size: 11px; } /* Smaller font for IP/MAC as requested */

    .empty-state {
        padding: 16px;
        text-align: center;
        color: var(--text-secondary);
    }

    /* 响应式适配 */
    @media (max-width: 768px) {
        .page-container { padding: 12px; }
        .header-right { width: 100%; justify-content: flex-end; }
        
        /* 缩小设备组卡片 */
        .overview-body {
            padding: 10px;
            gap: 8px;
        }
        .group-stat-box {
            padding: 10px 8px;
            border-radius: 12px;
        }
        .icon-wrapper {
            width: 34px;
            height: 34px;
            margin-right: 8px;
            border-radius: 10px;
        }
        .icon-wrapper .el-icon {
            font-size: 16px;
        }
        .group-name-text {
            font-size: 12px;
        }
        .status-indicator {
            font-size: 11px;
            gap: 4px;
        }
        .pulse-dot {
            width: 6px;
            height: 6px;
        }

        .kpi-grid { 
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 10px; 
        }
        .kpi-card { padding: 12px; min-height: 70px; }
        .kpi-value { 
            font-size: 16px; 
            font-weight: normal;
        }
        .kpi-icon { font-size: 26px; }

        /* 缩小移动端设备详情字体 */
        .m-line { font-size: 12px; }
        .v.mono { font-size: 10px; }
    }
</style>
