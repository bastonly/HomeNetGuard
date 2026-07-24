<template>
    <div class="page">
        <!-- Header -->
        <div class="header">
            <div class="header-left">
                <div class="title-row">
                    <h2 class="title">{{ $t('temp_auth.title') }}</h2>
                    <el-button
                            v-if="isMobile"
                            class="refresh-circle"
                            circle
                            :loading="loadingList"
                            @click="loadList"
                            :disabled="!selectedGroupId"
                    >
                        <el-icon><Refresh /></el-icon>
                    </el-button>
                </div>
                <div class="sub">{{ $t('temp_auth.sub') }}</div>
            </div>

            <div class="header-right">
                <el-select
                        v-model="selectedGroupId"
                        class="group-select"
                        :placeholder="$t('temp_auth.select_group_placeholder')"
                        :loading="loadingGroups"
                        @change="loadList"
                        :size="'default'"
                >
                    <el-option v-for="g in allowedGroups" :key="g.id" :label="g.label" :value="g.id" />
                </el-select>

                <div class="btns">
                    <el-button v-if="!isMobile" :loading="loadingList" @click="loadList" :disabled="!selectedGroupId">
                        {{ $t('temp_auth.refresh') }}
                    </el-button>

                    <el-button type="primary" @click="openAdd" :disabled="!selectedGroupId" :size="'default'">
                        {{ $t('temp_auth.add_auth') }}
                    </el-button>
                </div>
            </div>
        </div>

        <!-- ✅ PC：表格 -->
        <el-table
            v-if="!isMobile"
            :data="pagedRows"
            style="width: 100%"
            v-loading="loadingList"
            :empty-text="selectedGroupId ? $t('temp_auth.empty_records') : $t('temp_auth.select_group_first')"
        >
            <el-table-column :label="$t('temp_auth.group')" min-width="140">
                <template #default="scope">
                    <el-tag type="info">{{ groupNameById(scope.row.groups_id) }}</el-tag>
                </template>
            </el-table-column>

            <el-table-column :label="$t('temp_auth.start_time')" min-width="180">
                <template #default="scope">{{ fmtTs(scope.row.from_at) }}</template>
            </el-table-column>

            <el-table-column :label="$t('temp_auth.end_time')" min-width="180">
                <template #default="scope">{{ fmtTs(scope.row.end_at) }}</template>
            </el-table-column>

            <el-table-column :label="$t('temp_auth.remain')" min-width="140">
                <template #default="scope">
                    <span class="remain-pill" :class="scope.row._expired ? 'is-expired' : 'is-ok'">
                      {{ scope.row._expired ? $t('temp_auth.expired') : scope.row._remainText }}
                    </span>
                </template>
            </el-table-column>

            <el-table-column :label="$t('home.status')" width="110">
                <template #default="scope">
                    <el-tag :type="scope.row._expired ? 'info' : 'success'" size="small">
                        {{ scope.row._expired ? $t('temp_auth.expired') : $t('temp_auth.active') }}
                    </el-tag>
                </template>
            </el-table-column>

            <el-table-column :label="$t('common.actions')" width="140" fixed="right">
                <template #default="scope">
                    <template v-if="!scope.row._expired">
                        <el-dropdown trigger="click">
                            <el-button link type="primary">{{ $t('common.actions') }}</el-button>
                            <template #dropdown>
                                <el-dropdown-menu>
                                    <el-dropdown-item @click="openEdit(scope.row)">{{ $t('temp_auth.modify_duration') }}</el-dropdown-item>
                                    <el-dropdown-item divided @click="confirmDelete(scope.row)">
                                        <span class="danger-text">{{ $t('temp_auth.delete_block') }}</span>
                                    </el-dropdown-item>
                                </el-dropdown-menu>
                            </template>
                        </el-dropdown>
                    </template>
                    <template v-else>
                        <el-tag type="info" size="small">{{ $t('temp_auth.expired') }}</el-tag>
                    </template>
                </template>
            </el-table-column>
        </el-table>

        <!-- ✅ 手机：左滑列表（卡片化） -->
        <div v-else class="mobile-list" v-loading="loadingList">
            <div v-if="!loadingList && rowsView.length === 0" class="empty">
                {{ selectedGroupId ? $t('temp_auth.empty_records') : $t('temp_auth.select_group_first') }}
            </div>

            <!-- ✅ 关键修正：间距放到 wrapper，SwipeRow 自己不带 margin -->
            <div v-for="row in pagedRows" :key="row.id" class="swipe-wrap">
                <SwipeRow
                    class="swipe-shell"
                    :actions="['edit','delete']"
                    :disabled="row._expired"
                    @edit="openEdit(row)"
                    @delete="confirmDelete(row)"
                >
                    <div class="m-item">
                        <div class="m-top">
                            <el-tag type="info" size="small">{{ groupNameById(row.groups_id) }}</el-tag>
                            <el-tag :type="row._expired ? 'info' : 'success'" size="small">
                                {{ row._expired ? $t('temp_auth.expired') : $t('temp_auth.active') }}
                            </el-tag>
                        </div>

                        <div class="m-line">
                            <span class="k">{{ $t('temp_auth.start_time') }}</span>
                            <span class="v">{{ fmtTs(row.from_at) }}</span>
                        </div>
                        <div class="m-line">
                            <span class="k">{{ $t('temp_auth.end_time') }}</span>
                            <span class="v">{{ fmtTs(row.end_at) }}</span>
                        </div>

                        <div class="m-bottom">
                            <span class="remain-pill" :class="row._expired ? 'is-expired' : 'is-ok'">
                              {{ row._expired ? $t('temp_auth.expired') : row._remainText }}
                            </span>
                        </div>
                    </div>
                    <template #editIcon>⏱</template>
                    <template #deleteIcon>🛑</template>
                </SwipeRow>
            </div>
        </div>
        
        <PaginationBar
            v-if="rowsView.length > 0"
            v-model:page="page"
            v-model:pageSize="pageSize"
            :total="rowsView.length"
            :size="isMobile ? 'small' : 'default'"
            layout="prev, pager, next"
        />
        <div class="tip">
            <el-alert
                :title="$t('temp_auth.tip_title')"
                type="info"
                show-icon
                :closable="false"
                :description="$t('temp_auth.tip_desc')"
            />
        </div>
        <!-- ✅ 新增弹窗 -->
        <el-dialog
            v-model="addVisible"
            :title="$t('temp_auth.add_title')"
            :width="dialogWidth"
            :fullscreen="isMobile"
            :close-on-click-modal="false"
            class="adaptive-dialog"
        >
            <el-form :model="addForm" label-width="90px">
                <el-form-item :label="$t('temp_auth.group')" required>
                    <el-select v-model="addForm.groups_id" style="width: 100%">
                        <el-option v-for="g in allowedGroups" :key="g.id" :label="g.label" :value="g.id" />
                    </el-select>
                </el-form-item>

                <el-form-item :label="$t('temp_auth.duration')" required>
                    <el-select v-model="addForm.duration_min" :placeholder="$t('temp_auth.duration')" style="width: 100%">
                        <el-option v-for="d in DURATIONS" :key="d.value" :label="d.label" :value="d.value" />
                    </el-select>
                </el-form-item>

                <el-form-item :label="$t('temp_auth.preview')">
                    <div class="preview">
                        <div><span class="k">{{ $t('temp_auth.start_time') }}</span><span class="v">{{ fmtTs(addStartAt) }}</span></div>
                        <div><span class="k">{{ $t('temp_auth.end_time') }}</span><span class="v">{{ fmtTs(addEndAtPreview) }}</span></div>
                    </div>
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="addVisible = false">{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submitAdd">{{ $t('temp_auth.save_and_enable') }}</el-button>
                </div>
            </template>
        </el-dialog>

        <!-- ✅ 修改弹窗 -->
        <el-dialog
            v-model="editVisible"
            :title="$t('temp_auth.edit_title')"
            :width="dialogWidth"
            :fullscreen="isMobile"
            :close-on-click-modal="false"
            class="adaptive-dialog"
        >
            <el-form :model="editForm" label-width="90px">
                <el-form-item :label="$t('temp_auth.group')">
                    <el-input :value="groupNameById(editForm.groups_id)" disabled />
                </el-form-item>

                <el-form-item :label="$t('temp_auth.start_time')">
                    <el-input :value="fmtTs(editForm.from_at)" disabled />
                </el-form-item>

                <el-form-item :label="$t('temp_auth.end_time')">
                    <el-input :value="fmtTs(editForm.end_at)" disabled />
                </el-form-item>

                <el-form-item :label="$t('temp_auth.new_duration')" required>
                    <el-select v-model="editForm.duration_min" :placeholder="$t('temp_auth.duration')" style="width: 100%">
                        <el-option v-for="d in DURATIONS" :key="d.value" :label="d.label" :value="d.value" />
                    </el-select>
                    <div class="hint2">{{ $t('temp_auth.edit_hint') }}</div>
                </el-form-item>

                <el-form-item :label="$t('temp_auth.new_end')">
                    <el-tag type="info">{{ fmtTs(editEndAtPreview) }}</el-tag>
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="editVisible = false">{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submitEdit">{{ $t('temp_auth.save_and_update') }}</el-button>
                </div>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import { ref, computed, onMounted, onUnmounted } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import { getTemporaryAuths, addTemporaryAuth, updateTemporaryAuth, deleteTemporaryAuth } from '@/api/temporary'
    import { getGroups } from '@/api/group'
    import { Refresh } from '@element-plus/icons-vue'
    import SwipeRow from '@/components/SwipeRow.vue'
    import PaginationBar from '@/components/PaginationBar.vue'
    import { formatGroupName } from '@/utils/common.js'
    
    const { t } = useI18n()

    /**
     * DB: temporary(id, groups_id, from_at(INT秒), end_at(INT秒))
     * 前端：duration 用【分钟】
     */
    const DURATIONS = computed(() => [
        { label: t('temp_auth.duration_list.m10'), value: 10 },
        { label: t('temp_auth.duration_list.m20'), value: 20 },
        { label: t('temp_auth.duration_list.m30'), value: 30 },
        { label: t('temp_auth.duration_list.h1'), value: 60 },
        { label: t('temp_auth.duration_list.h2'), value: 120 },
        { label: t('temp_auth.duration_list.h3'), value: 180 }
    ])

    const groups = ref([])
    const list = ref([])
    
    // 分页
    const page = ref(1)
    const pageSize = ref(20)

    const selectedGroupId = ref(null)

    const loadingGroups = ref(false)
    const loadingList = ref(false)
    const saving = ref(false)

    const addVisible = ref(false)
    const editVisible = ref(false)

    /* ✅ 移动端判定 + 弹窗宽度 */
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)
    const dialogWidth = computed(() => (isMobile.value ? '100%' : '560px'))

    // 新增：冻结一个开始时间，保证预览与提交一致
    const addStartAt = ref(0)

    const addForm = ref({ groups_id: null, duration_min: 10 })
    const editForm = ref({ id: null, groups_id: null, from_at: 0, end_at: 0, duration_min: 10 })

    const nowSec = ref(Math.floor(Date.now() / 1000))
    let ticker = null

    const minToSec = (min) => Number(min || 0) * 60

    const fmtTs = (sec) => {
        if (!sec) return '-'
        const d = new Date(Number(sec) * 1000)
        return isNaN(d.getTime()) ? String(sec) : d.toLocaleString()
    }

    const remainText = (endAt) => {
        const diff = Number(endAt || 0) - nowSec.value
        if (diff <= 0) return '00:00:00'
        const h = String(Math.floor(diff / 3600)).padStart(2, '0')
        const m = String(Math.floor((diff % 3600) / 60)).padStart(2, '0')
        const s = String(diff % 60).padStart(2, '0')
        return `${h}:${m}:${s}`
    }
    

    const groupNameById = (id) => {
        const g = groups.value.find(g => g.id === id)
        if (!g) return String(id ?? '-')
        return formatGroupName(g, t)
    }

    const allowedGroups = computed(() => {
        const deny = new Set(['open', 'temp'])
        return groups.value
            .filter(g => !deny.has(String(g.name).toLowerCase()))
            .map(g => ({
                ...g,
                label: formatGroupName(g, t)
            }))
    })

    const rowsView = computed(() =>
        (list.value || []).map(r => {
            const expired = Number(r.end_at || 0) <= nowSec.value
            return {
                ...r,
                _expired: expired,
                _remainText: expired ? t('temp_auth.expired') : remainText(r.end_at)
            }
        })
    )
    const pagedRows = computed(() => {
        const p = Number(page.value || 1)
        const s = Number(pageSize.value || 20)
        const start = (p - 1) * s
        // rowsView 是所有数据，这里基于它做分页
        return rowsView.value.slice(start, start + s)
    })

    const addEndAtPreview = computed(() => {
        const start = Number(addStartAt.value || 0)
        const dur = minToSec(addForm.value.duration_min)
        return start ? (start + dur) : 0
    })

    const editEndAtPreview = computed(() => {
        const start = Number(editForm.value.from_at || 0)
        const dur = minToSec(editForm.value.duration_min)
        return start ? (start + dur) : 0
    })

    /* ============ API ============ */
    const loadGroups = async () => {
        loadingGroups.value = true
        try {
            const res = await getGroups()
            groups.value = Array.isArray(res) ? res : (res?.data || [])
            if (!selectedGroupId.value && allowedGroups.value.length) {
                selectedGroupId.value = allowedGroups.value[0].id
            }
        } finally {
            loadingGroups.value = false
        }
    }

    const loadList = async () => {
        if (!selectedGroupId.value) return
        loadingList.value = true
        try {
            const res = await getTemporaryAuths({ groups_id: selectedGroupId.value })
            list.value = Array.isArray(res) ? res : (res?.data || [])
            page.value = 1
        } finally {
            loadingList.value = false
        }
    }

    /* ============ 新增 ============ */
    const openAdd = () => {
        if (!selectedGroupId.value) return ElMessage.warning(t('temp_auth.select_group_first'))
        addStartAt.value = nowSec.value
        addForm.value = { groups_id: selectedGroupId.value, duration_min: DURATIONS.value[0].value }
        addVisible.value = true
    }

    const submitAdd = async () => {
        const { groups_id, duration_min } = addForm.value
        if (!groups_id) return ElMessage.warning(t('temp_auth.select_group_first'))
        if (!duration_min) return ElMessage.warning(t('temp_auth.duration'))

        saving.value = true
        try {
            const fromAt = Number(addStartAt.value || nowSec.value)
            const endAt = fromAt + minToSec(duration_min)

            await addTemporaryAuth({ groups_id, from_at: fromAt, end_at: endAt })
            ElMessage.success(t('temp_auth.add_success'))
            addVisible.value = false
            await loadList()
        } finally {
            saving.value = false
        }
    }

    /* ============ 修改 ============ */
    const openEdit = (row) => {
        if (row._expired) return
        const curMin = Math.round((Number(row.end_at) - Number(row.from_at)) / 60)
        const exists = DURATIONS.value.some(d => d.value === curMin)

        editForm.value = {
            id: row.id,
            groups_id: row.groups_id,
            from_at: Number(row.from_at || 0),
            end_at: Number(row.end_at || 0),
            duration_min: exists ? curMin : DURATIONS.value[0].value
        }
        editVisible.value = true
    }

    const submitEdit = async () => {
        const { id, from_at, duration_min } = editForm.value
        if (!id) return
        if (!duration_min) return ElMessage.warning(t('temp_auth.duration'))

        saving.value = true
        try {
            const newEndAt = Number(from_at) + minToSec(duration_min)
            await updateTemporaryAuth(id, { end_at: newEndAt })
            ElMessage.success(t('temp_auth.update_success'))
            editVisible.value = false
            await loadList()
        } finally {
            saving.value = false
        }
    }

    /* ============ 删除 ============ */
    const confirmDelete = async (row) => {
        if (row._expired) return

        try {
            await ElMessageBox.confirm(
                t('temp_auth.delete_confirm', { group: groupNameById(row.groups_id), range: `${fmtTs(row.from_at)} ~ ${fmtTs(row.end_at)}` }),
                t('home.op_confirm'),
                { type: 'warning', confirmButtonText: t('temp_auth.delete_block'), cancelButtonText: t('common.cancel') }
            )
        } catch {
            return
        }

        saving.value = true
        try {
            await deleteTemporaryAuth(row.id)
            ElMessage.success(t('temp_auth.delete_success'))
            await loadList()
        } finally {
            saving.value = false
        }
    }

    /* ============ 生命周期 ============ */
    onMounted(async () => {
        window.addEventListener('resize', onResize)
        ticker = setInterval(() => { nowSec.value = Math.floor(Date.now() / 1000) }, 1000)
        await loadGroups()
        if (selectedGroupId.value) await loadList()
    })

    onUnmounted(() => {
        if (ticker) clearInterval(ticker)
        window.removeEventListener('resize', onResize)
    })
</script>

<style scoped>
    /* ============ Layout ============ */
    /* .page, .page-container moved to layout.css */

    /* ============ Header ============ */
    /* .header, .title, .sub, .header-right, .btns moved to layout.css */

    .group-select { width: 260px; }
    .btns { display: flex; gap: 10px; }

    /* ============ Card ============ */
    /* .card moved to layout.css */

    /* ============ Table (PC) ============ */
    /* Overrides moved to overrides.css */
    /* Using generic ones from overrides.css allows removing this block */
    :deep(.el-tag--info) {
        background-color: var(--divider-color);
        border-color: var(--border-light);
        color: var(--text-primary);
        font-size: 12px;
        padding: 0 10px;
        border-radius: 999px;
    }
    .danger-text { color: #ef4444; }

    /* 剩余时间胶囊 */
    .remain-pill {
        display: inline-flex;
        align-items: center;
        padding: 4px 10px;
        border-radius: 999px;
        border: 1px solid var(--border-light);
        font-size: 12px;
        font-weight: 700;
        font-variant-numeric: tabular-nums;
        letter-spacing: 0.2px;
    }
    .remain-pill.is-ok {
        color: #10b981;
        background: rgba(16, 185, 129, 0.1);
        border-color: rgba(16, 185, 129, 0.2);
    }
    .remain-pill.is-expired {
        color: var(--text-regular);
        background: var(--divider-color);
        border-color: var(--border-light);
    }

    /* ============ Mobile list ============ */
    /* .mobile-list, .empty, .swipe-wrap, .swipe-shell moved to components.css layout.css etc. */
    /* .m-item, .m-top, .m-line, .m-bottom, .hint moved to components.css layout.css etc. */

    /* ============ Preview / Dialog ============ */
    .preview {
        width: 100%;
        border: 1px solid var(--border-light);
        border-radius: 12px;
        padding: 10px 12px;
        background: var(--bg-page);
    }
    .preview .k {
        display: inline-block;
        width: 44px;
        color: var(--text-regular);
        font-size: 12px;
    }
    .preview .v {
        color: var(--text-primary);
        font-size: 12px;
        font-variant-numeric: tabular-nums;
    }

    :deep(.adaptive-dialog .el-dialog__body) {
        padding: 16px;
        max-height: calc(100vh - 170px);
        overflow: auto;
    }
    .dlg-footer { display: flex; justify-content: flex-end; gap: 10px; }
    @media (max-width: 768px) {
        .header-right {
            flex-direction: row !important;
            flex-wrap: nowrap !important;
            align-items: center !important;
            gap: 8px !important;
        }
        .group-select {
            flex: 1 1 50% !important;
            width: 50% !important;
            max-width: 50% !important;
            min-width: 0 !important;
        }
        .btns {
            flex: 1 1 50% !important;
            width: 50% !important;
            max-width: 50% !important;
        }
        .btns :deep(.el-button) {
            width: 100%;
        }

        .dlg-footer { justify-content: stretch; }
        .dlg-footer :deep(.el-button) { flex: 1; }
    }
    .hint2 { margin-top: 6px; font-size: 12px; color: #94a3b8; }

    /* ============ Tip / Alert ============ */
    .tip {
        padding: 12px 12px 14px;
        border-top: 1px solid var(--divider-color);
        font-size: 12px;
    }
    :deep(.el-alert) { border-radius: 12px; }
</style>
