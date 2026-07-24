<template>
    <div class="page">
        <div class="panel">
            <div class="panel-head">
                <div class="header-left">
                    <div class="title-row">
                        <h2 class="title">{{ $t('groups_manage.title') }}</h2>
                        <el-button
                                v-if="isMobile"
                                class="refresh-circle"
                                circle
                                :loading="loading"
                                @click="loadGroups"
                                :aria-label="$t('common.refresh')"
                        >
                            <el-icon><Refresh/></el-icon>
                        </el-button>
                    </div>
                    <div class="sub">{{ $t('groups_manage.sub') }}</div>
                </div>

                <div class="header-right" v-if="!isMobile">
                    <el-button
                            class="btn-refresh"
                            :loading="loading"
                            :icon="Refresh"
                            @click="loadGroups"
                            round
                    >
                        {{ $t('common.refresh') }}
                    </el-button>
                </div>
            </div>
        </div>

        <div v-if="!isMobile" class="table-shell mobile-naked" v-loading="loading">
            <el-table
                    :data="groupsView"
                    style="width: 100%"
                    class="modern-table"
                    :empty-text="loading ? $t('groups_manage.loading') : $t('groups_manage.no_data')"
            >
                <el-table-column :label="$t('groups_manage.key_column')" width="110">
                    <template #default="scope">
                        <span class="key-pill">{{ scope.row.name }}</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.name_column')" min-width="140">
                    <template #default="scope">
                        <div class="pc-name-cell">
                            <el-icon class="group-icon">
                                <component :is="getGroupIcon(scope.row.name, scope.row.remark)"/>
                            </el-icon>
                            <span class="pc-remark">{{ formatGroupNameLocal(scope.row) }}</span>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.type_column')" width="100">
                    <template #default="scope">
                        <el-tag
                                v-if="scope.row.kind != null"
                                size="small"
                                effect="light"
                                round
                                :type="kindTagType(scope.row.kind)"
                        >
                            {{ kindLabel(scope.row.kind) }}
                        </el-tag>
                        <span v-else class="none-text">-</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.vpn_column')" width="95">
                    <template #default="scope">
                        <el-switch
                                v-model="scope.row.allow_vpn"
                                :active-value="1"
                                :inactive-value="0"
                                inline-prompt
                                :active-text="$t('groups_manage.allow')"
                                :inactive-text="$t('groups_manage.deny')"
                                style="--el-switch-on-color: #13ce66; --el-switch-off-color: #ff4949"
                                @change="handleQuickUpdate(scope.row, 'allow_vpn', scope.row.allow_vpn)"
                        />
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.video_detect_column')" width="100">
                    <template #default="scope">
                        <el-switch
                                v-model="scope.row.enable_video_detection"
                                :active-value="1"
                                :inactive-value="0"
                                inline-prompt
                                :active-text="$t('groups_manage.on')"
                                :inactive-text="$t('groups_manage.off')"
                                style="--el-switch-on-color: #409eff; --el-switch-off-color: #dcdfe6"
                                @change="handleQuickUpdate(scope.row, 'enable_video_detection', scope.row.enable_video_detection)"
                        />
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.gaming_detect_column')" width="100">
                    <template #default="scope">
                        <el-switch
                                v-model="scope.row.enable_gaming_detection"
                                :active-value="1"
                                :inactive-value="0"
                                inline-prompt
                                :active-text="$t('groups_manage.on')"
                                :inactive-text="$t('groups_manage.off')"
                                style="--el-switch-on-color: #409eff; --el-switch-off-color: #dcdfe6"
                                @change="handleQuickUpdate(scope.row, 'enable_gaming_detection', scope.row.enable_gaming_detection)"
                        />
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.block_column')" width="110">
                    <template #default="scope">
                        <el-switch
                                v-if="scope.row.kind === 2"
                                v-model="scope.row.is_blocked"
                                :active-value="1"
                                :inactive-value="0"
                                inline-prompt
                                :active-text="$t('groups_manage.intercept')"
                                :inactive-text="$t('groups_manage.normal')"
                                style="--el-switch-on-color: #f56c6c; --el-switch-off-color: #67c23a"
                                @change="handleQuickUpdate(scope.row, 'is_blocked', scope.row.is_blocked)"
                        />
                        <span v-else class="none-text">-</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups_manage.adguard_key_column')" min-width="180">
                    <template #default="scope">
                        <code v-if="scope.row.adguard_client_key" class="adguard-code mono">
                            {{ scope.row.adguard_client_key }}
                        </code>
                        <span v-else class="none-text">{{ $t('groups_manage.not_bound') }}</span>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('common.actions')" width="95" fixed="right" align="right">
                    <template #default="scope">
                        <el-button type="primary" link :icon="Edit" @click="openEdit(scope.row)">
                            {{ $t('common.edit') }}
                        </el-button>
                    </template>
                </el-table-column>
            </el-table>
        </div>

        <div v-else class="mobile-container" v-loading="loading">
            <div v-if="!loading && groupsView.length === 0" class="empty-state">{{ $t('groups_manage.no_data') }}</div>

            <SwipeRow
                    v-for="g in groupsView"
                    :key="g.id"
                    :actions="['edit']"
                    @edit="openEdit(g)"
                    @delete="confirmDelete(g)"
            >
                <div class="m-group-card">
                    <div class="m-top">
                        <div class="m-name-block">
                            <span class="m-remark">{{ formatGroupNameLocal(g) }}</span>
                            <span class="key-pill mini">{{ g.name }}</span>
                        </div>
                    </div>

                    <div class="m-body">
                        <div class="m-line">
                            <span class="k">{{ $t('groups_manage.type_column') }}</span>
                            <span class="v">
                <el-tag v-if="g.kind != null" size="small" effect="light" round :type="kindTagType(g.kind)">
                  {{ kindLabel(g.kind) }}
                </el-tag>
              </span>
                        </div>

                        <!-- Row 1: VPN + Blocked (only if kind=2) -->
                        <div class="m-row" :class="{ 'no-grid': g.kind !== 2 }">
                            <div class="m-line">
                                <span class="k">{{ $t('groups_manage.vpn_column') }}</span>
                                <span class="v">
                                <el-switch
                                        v-model="g.allow_vpn"
                                        :active-value="1"
                                        :inactive-value="0"
                                        size="small"
                                        @change="handleQuickUpdate(g, 'allow_vpn', g.allow_vpn)"
                                />
                              </span>
                            </div>

                            <div class="m-line" v-if="g.kind === 2">
                                <span class="k">{{ $t('groups_manage.block_column') }}</span>
                                <span class="v">
                                <el-switch
                                        v-model="g.is_blocked"
                                        :active-value="1"
                                        :inactive-value="0"
                                        size="small"
                                        @change="handleQuickUpdate(g, 'is_blocked', g.is_blocked)"
                                />
                              </span>
                            </div>
                        </div>

                        <!-- Row 2: Video + Gaming -->
                        <div class="m-row">
                            <div class="m-line">
                                <span class="k">{{ $t('groups_manage.video_detect_column') }}</span>
                                <span class="v">
                                <el-switch
                                        v-model="g.enable_video_detection"
                                        :active-value="1"
                                        :inactive-value="0"
                                        size="small"
                                        @change="handleQuickUpdate(g, 'enable_video_detection', g.enable_video_detection)"
                                />
                              </span>
                            </div>

                            <div class="m-line">
                                <span class="k">{{ $t('groups_manage.gaming_detect_column') }}</span>
                                <span class="v">
                                <el-switch
                                        v-model="g.enable_gaming_detection"
                                        :active-value="1"
                                        :inactive-value="0"
                                        size="small"
                                        @change="handleQuickUpdate(g, 'enable_gaming_detection', g.enable_gaming_detection)"
                                />
                              </span>
                            </div>
                        </div>

                        <div class="m-line">
                            <span class="k">{{ $t('groups_manage.adguard_key_column') }}</span>
                            <span class="v mono">{{ g.adguard_client_key || $t('groups_manage.not_bound') }}</span>
                        </div>
                    </div>
                </div>
            </SwipeRow>
        </div>

        <el-dialog
                v-model="editVisible"
                :title="$t('groups_manage.edit_title')"
                :width="isMobile ? '100%' : '520px'"
                :fullscreen="isMobile"
                destroy-on-close
                class="modern-dialog"
        >
            <el-form :model="editForm" label-position="top">
                <el-form-item :label="$t('groups_manage.core_key')">
                    <el-input v-model="editForm.name" disabled/>
                    <p class="form-hint">{{ $t('groups_manage.core_key_hint') }}</p>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.display_name')" required>
                    <el-input v-model="editForm.remark" :placeholder="$t('groups_manage.display_name_placeholder')"/>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.type_column')" required>
                    <el-radio-group v-model="editForm.kind" class="kind-radio">
                        <el-radio :value="1">{{ $t('groups_manage.kind_1') }}</el-radio>
                        <el-radio :value="2">{{ $t('groups_manage.kind_2') }}</el-radio>
                        <el-radio :value="3">{{ $t('groups_manage.kind_3') }}</el-radio>
                    </el-radio-group>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.vpn_allow_status')">
                    <el-radio-group v-model="editForm.allow_vpn">
                        <el-radio :value="1">{{ $t('groups_manage.vpn_allow_yes') }}</el-radio>
                        <el-radio :value="0">{{ $t('groups_manage.vpn_allow_no') }}</el-radio>
                    </el-radio-group>
                    <p class="form-hint">{{ $t('groups_manage.vpn_hint') }}</p>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.video_detect_column')">
                    <el-radio-group v-model="editForm.enable_video_detection">
                        <el-radio :value="1">{{ $t('groups_manage.on') }}</el-radio>
                        <el-radio :value="0">{{ $t('groups_manage.off') }} ({{ $t('common.default') }})</el-radio>
                    </el-radio-group>
                    <p class="form-hint">{{ $t('groups_manage.video_hint') }}</p>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.gaming_detect_column')">
                    <el-radio-group v-model="editForm.enable_gaming_detection">
                        <el-radio :value="1">{{ $t('groups_manage.on') }}</el-radio>
                        <el-radio :value="0">{{ $t('groups_manage.off') }} ({{ $t('common.default') }})</el-radio>
                    </el-radio-group>
                    <p class="form-hint">{{ $t('groups_manage.gaming_hint') }}</p>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.block_column')" v-if="editForm.kind === 2">
                    <el-radio-group v-model="editForm.is_blocked">
                        <el-radio :value="1">{{ $t('groups_manage.block_yes') }}</el-radio>
                        <el-radio :value="0">{{ $t('groups_manage.block_no') }} ({{ $t('common.default') }})</el-radio>
                    </el-radio-group>
                    <p class="form-hint">{{ $t('groups_manage.block_hint') }}</p>
                </el-form-item>

                <el-form-item :label="$t('groups_manage.adguard_key_column')">
                    <el-input
                            v-model="editForm.adguard_client_key"
                            :placeholder="$t('groups_manage.adguard_client_key_tip')"
                            clearable
                    />
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="editVisible = false" round>{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submitEdit" round>{{ $t('groups_manage.save_btn') }}</el-button>
                </div>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import {ref, computed, onMounted, onUnmounted} from 'vue'
    import { useI18n } from 'vue-i18n'
    import {ElMessage, ElMessageBox} from 'element-plus'
    import { 
        Refresh, Edit, Unlock, School, Timer, 
        AlarmClock, CollectionTag, VideoPlay 
    } from '@element-plus/icons-vue'
    import { getGroups, updateGroup, deleteGroup } from '@/api/group'
    import SwipeRow from '@/components/SwipeRow.vue'
    import { getDeviceTypeIcon, getDeviceTypeColor, formatGroupName } from '@/utils/common.js'

    const { t } = useI18n()
    const loading = ref(false)
    const saving = ref(false)
    const groups = ref([])

    const editVisible = ref(false)
    const editForm = ref({
        id: null,
        name: '',
        remark: '',
        kind: 2,
        allow_vpn: 1,
        enable_video_detection: 0,
        enable_gaming_detection: 0,
        is_blocked: 0,
        adguard_client_key: ''
    })

    const groupsView = computed(() =>
        [...(groups.value || [])].sort((a, b) => String(a.name).localeCompare(String(b.name)))
    )

    const kindLabel = (k) => {
        const n = Number(k)
        if (n === 1) return t('groups_manage.kind_1');
        if (n === 2) return t('groups_manage.kind_2');
        if (n === 3) return t('groups_manage.kind_3')
        return '-'
    }
    const kindTagType = (k) => {
        const n = Number(k)
        if (n === 1) return 'info';
        if (n === 2) return 'warning';
        if (n === 3) return 'success'
        return 'info'
    }

    const getGroupIcon = (name, remark) => {
        const t = `${name || ''} ${remark || ''}`.toLowerCase()
        if (t.includes('fun') || t.includes('entertainment') || t.includes('娱乐')) return VideoPlay
        if (t.includes('open') || t.includes('开放')) return Unlock
        if (t.includes('study') || t.includes('学习')) return School
        if (t.includes('temp') || t.includes('临时')) return Timer
        if (t.includes('timed') || t.includes('限时')) return AlarmClock
        return CollectionTag
    }

    const formatGroupNameLocal = (group) => {
        return formatGroupName(group, t)
    }

    const loadGroups = async () => {
        loading.value = true
        try {
            const res = await getGroups()
            const data = Array.isArray(res) ? res : (res?.data || [])
            // 初始化数据，确保每一行都有完整状态
            groups.value = data.map(item => ({
                ...item,
                allow_vpn: item.allow_vpn ?? 1,
                enable_video_detection: item.enable_video_detection ?? 0,
                enable_gaming_detection: item.enable_gaming_detection ?? 0,
                is_blocked: item.is_blocked ?? 0
            }))
        } catch (e) {
            ElMessage.error(t('common.load_failed'))
        } finally {
            loading.value = false
        }
    }

    /** 快捷更新状态（Switch触发） */
    const handleQuickUpdate = async (row, key, val) => {
        try {
            // 提交当前行的完整状态
            await updateGroup(row.id, {
                remark: row.remark,
                kind: row.kind,
                allow_vpn: row.allow_vpn,
                enable_video_detection: row.enable_video_detection,
                enable_gaming_detection: row.enable_gaming_detection,
                is_blocked: row.is_blocked,
                adguard_client_key: row.adguard_client_key
            })
            ElMessage.success(t('groups_manage.sync_success'))
        } catch (e) {
            // 失败则回滚 UI 状态
            row[key] = val === 1 ? 0 : 1
            ElMessage.error(t('common.operation_failed'))
        }
    }

    const openEdit = (row) => {
        editForm.value = {
            id: row.id,
            name: row.name || '',
            remark: row.remark ?? '',
            kind: row.kind != null ? Number(row.kind) : 2,
            allow_vpn: row.allow_vpn ?? 1,
            enable_video_detection: row.enable_video_detection ?? 0,
            enable_gaming_detection: row.enable_gaming_detection ?? 0,
            is_blocked: row.is_blocked ?? 0,
            adguard_client_key: row.adguard_client_key ?? ''
        }
        editVisible.value = true
    }

    const submitEdit = async () => {
        const remark = String(editForm.value.remark || '').trim()
        if (!remark) return ElMessage.warning(t('groups_manage.empty_name'))

        saving.value = true
        try {
            await updateGroup(editForm.value.id, {
                remark,
                kind: Number(editForm.value.kind),
                allow_vpn: editForm.value.allow_vpn,
                enable_video_detection: editForm.value.enable_video_detection,
                enable_gaming_detection: editForm.value.enable_gaming_detection,
                is_blocked: editForm.value.is_blocked,
                adguard_client_key: String(editForm.value.adguard_client_key || '').trim()
            })
            ElMessage.success(t('groups_manage.update_success'))
            editVisible.value = false
            await loadGroups()
        } catch (e) {
            ElMessage.error(t('common.save_failed'))
        } finally {
            saving.value = false
        }
    }

    const confirmDelete = async (row) => {
        try {
            await ElMessageBox.confirm(
                t('groups_manage.delete_confirm', { name: formatGroupNameLocal(row) }),
                t('groups_manage.delete_title'),
                { type: 'warning' }
            )
            await deleteGroup(row.id)
            ElMessage.success(t('groups_manage.delete_success'))
            await loadGroups()
        } catch (e) {
            if (e !== 'cancel') ElMessage.error(t('common.delete_failed'))
        }
    }

    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)
    onMounted(() => {
        window.addEventListener('resize', onResize);
        loadGroups()
    })
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    /* 保持您原始的 CSS 逻辑 */
    /* .page-container moved to layout.css */

    /* .header-card, .title, .sub moved to layout.css */

    .btn-refresh {
        border-radius: 10px;
    }

    .refresh-circle {
        width: 32px !important;
        height: 32px !important;
        min-width: 32px !important;
        padding: 0 !important;
        border-radius: 999px !important;
        display: inline-flex;
        align-items: center;
        justify-content: center;
    }

    /* Table Overrides moved to overrides.css */

    .table-shell {
        width: 100%;
        max-width: 100%;
        background: var(--bg-card);
        border: 1px solid var(--border-light);
        border-radius: 18px;
        box-shadow: 0 1px 3px rgba(15, 23, 42, 0.06);
        overflow: hidden;
    }
    /*
    ### 📱 Mobile Experience & UI Optimization
    - **Global Scroll Fix**: Removed overflow: hidden from .layout container on mobile, restoring natural body scroll, and set top nav to sticky.
    - **Group Management Optimization**: Parallel grouped the four switches (VPN+Block, Video+Gaming), significantly reducing card height on mobile.
    - **Page Adaptation**: Adjusted min-height across multiple pages to ensure smooth swipe gestures.
    - **Style Simplification**: Compressed spacing in search areas, list cards, and buttons for better information density on phones.
    */

    .key-pill {
        display: inline-flex;
        align-items: center;
        padding: 6px 14px;
        border-radius: 999px;
        background: var(--divider-color);
        color: var(--text-regular);
        font-weight: 700;
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }

    .pc-name-cell {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
    }

    .group-icon {
        color: var(--text-regular);
        font-size: 16px;
        flex: 0 0 auto;
    }

    .pc-remark {
        font-size: 15px;
        color: var(--text-primary);
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .adguard-code {
        color: var(--primary-color);
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
        font-size: 14px;
    }

    .none-text {
        color: var(--text-secondary);
    }

    /* .mobile-container, .m-group-card, .m-title, .m-remark, .m-body, .m-line, .k, .v moved to components.css/layout.css */
    /* Only keeping specific overrides if any */
    
    .m-group-card {
        border-radius: 18px; /* Override standard 14px */
    }

    .k {
        color: var(--text-secondary);
    }

    .v {
        color: var(--text-primary);
        text-align: right;
    }

    .m-name-block {
        display: flex;
        align-items: center;
        gap: 8px;
        min-width: 0;
    }

    .m-remark {
        font-weight: 800;
        font-size: 15px;
        color: var(--text-primary);
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .key-pill.mini {
        padding: 2px 8px;
        font-size: 10px;
        font-weight: 600;
        background: var(--divider-color);
        color: var(--text-regular);
        border: 1px solid var(--border-light);
    }

    /* Mobile Row Grid */
    .m-top {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: 12px;
    }

    .m-row {
        display: flex;
        gap: 16px;
        border-bottom: 1px dashed var(--divider-color);
    }
    .m-row .m-line {
        flex: 1;
        border-bottom: none;
        padding: 8px 0;
    }
    .m-row.no-grid {
        display: block;
    }
    .m-row.no-grid .m-line {
        border-bottom: none; /* Still none because parent is row */
    }
    /* Re-add border-bottom for non-grid row if needed, but actually the row itself handles the separator */

    :deep(.el-switch--small) {
        height: 20px;
    }
</style>
