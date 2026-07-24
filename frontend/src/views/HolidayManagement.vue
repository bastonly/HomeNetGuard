<template>
    <div class="page">
        <!-- ===================== 方块 1：标题 + 说明 + 操作 ===================== -->
        <div class="panel">
            <div class="panel-head">
                <div class="header-left">
                    <div class="title-row">
                        <h2 class="title">{{ $t('holiday.title') }}</h2>
                        <el-button
                            v-if="isMobile"
                            class="refresh-circle"
                            circle
                            :loading="loading"
                            @click="loadAll"
                        >
                            <el-icon><Refresh /></el-icon>
                        </el-button>
                    </div>
                    <div class="sub">
                        <div>{{ $t('holiday.sub_1') }}</div>
                        <div>{{ $t('holiday.sub_2') }}</div>
                    </div>
                </div>

                <div class="header-right">
                    <el-button v-if="!isMobile" :loading="loading" @click="loadAll" :icon="Refresh">
                        {{ $t('holiday.refresh') }}
                    </el-button>
                    <el-button type="primary" @click="openAdd" :icon="Plus">
                        {{ $t('holiday.add_holiday') }}
                    </el-button>
                </div>
            </div>
        </div>

        <!-- ===================== 方块 2：表格 / 列表 ===================== -->
        <!--<div class="panel">-->
            <div class="panel-body">
                <!-- PC：表格 -->
                <el-table
                        v-if="!isMobile"
                        :data="rowsSorted"
                        v-loading="loading"
                        class="modern-table"
                        :empty-text="loading ? $t('holiday.loading') : $t('holiday.empty_text')"
                        style="width: 100%"
                >
                    <el-table-column :label="$t('holiday.date_range') || '日期范围'" min-width="200">
                        <template #default="scope">
                            <div class="date-range-col">
                                <span class="date-mono">{{ scope.row.start_date }}</span>
                                <span class="date-sep">~</span>
                                <span class="date-mono">{{ scope.row.end_date }}</span>
                            </div>
                        </template>
                    </el-table-column>

                    <el-table-column :label="$t('holiday.name')" min-width="180" show-overflow-tooltip>
                        <template #default="scope">
                            <span>{{ scope.row.name || '-' }}</span>
                        </template>
                    </el-table-column>

                    <el-table-column :label="$t('holiday.actions')" width="200" fixed="right" align="right">
                        <template #default="scope">
                            <div class="op">
                                <el-button type="primary" link :icon="Edit" @click="openEdit(scope.row)">
                                    {{ $t('holiday.edit') }}
                                </el-button>
                                <el-button type="danger" link :icon="Delete" @click="confirmDelete(scope.row)">
                                    {{ $t('holiday.delete') }}
                                </el-button>
                            </div>
                        </template>
                    </el-table-column>
                </el-table>

                <!-- Mobile：SwipeRow（左滑出现动作） -->
                <div v-else class="mobile-wrap" v-loading="loading">
                    <div v-if="!loading && rowsSorted.length === 0" class="empty">{{ $t('holiday.empty_text') }}</div>
                    <div v-else class="mobile-list">
                        <SwipeRow
                            v-for="h in rowsSorted"
                            :key="h.id"
                            :actions="['edit', 'delete']"
                            @edit="openEdit(h)"
                            @delete="confirmDelete(h)"
                    >
                            <div class="m-item">
                                <div class="m-top">
                                    <div class="m-date">{{ h.start_date }} ~ {{ h.end_date }}</div>
                                </div>

                                <div class="m-sub">
                                    <span class="k">{{ $t('holiday.name') }}</span>
                                    <span class="v ell">{{ h.name || '-' }}</span>
                                </div>
                            </div>

                            <template #editIcon>
                                <el-icon><Edit /></el-icon>
                            </template>
                            <template #deleteIcon>
                                <el-icon><Delete /></el-icon>
                            </template>
                        </SwipeRow>
                    </div>
                </div>
            </div>
        <!--</div>-->

        <!-- ===================== 新增/编辑弹窗 ===================== -->
        <el-dialog
                v-model="editVisible"
                :title="mode === 'add' ? $t('holiday.add_title') : $t('holiday.edit_title')"
                :width="isMobile ? '100%' : '520px'"
                :fullscreen="isMobile"
                :close-on-click-modal="false"
                class="adaptive-dialog"
                destroy-on-close
        >
            <el-form :model="form" :label-position="isMobile ? 'top' : 'right'" :label-width="isMobile ? 'auto' : '90px'">
                <el-form-item :label="$t('holiday.date_range') || '日期范围'" required>
                    <!-- Mobile -->
                    <div v-if="isMobile" class="mobile-date-picker-wrap">
                        <el-date-picker
                                v-model="form.start_date"
                                type="date"
                                value-format="YYYY-MM-DD"
                                format="YYYY-MM-DD"
                                :placeholder="$t('holiday.start_date') || '开始日期'"
                                style="width: 100%"
                        />
                        <span class="range-sep">~</span>
                        <el-date-picker
                                v-model="form.end_date"
                                type="date"
                                value-format="YYYY-MM-DD"
                                format="YYYY-MM-DD"
                                :placeholder="$t('holiday.end_date') || '结束日期'"
                                style="width: 100%"
                        />
                    </div>
                    <!-- PC -->
                    <el-date-picker
                            v-else
                            v-model="form.date_range"
                            type="daterange"
                            range-separator="~"
                            value-format="YYYY-MM-DD"
                            format="YYYY-MM-DD"
                            :start-placeholder="$t('holiday.start_date') || '开始日期'"
                            :end-placeholder="$t('holiday.end_date') || '结束日期'"
                            style="width: 100%"
                    />
                    <div class="hint">{{ $t('holiday.date_range_hint') || '选择假期的开始和结束日期，系统会自动覆盖该区间内的所有日期。' }}</div>
                </el-form-item>

                <el-form-item :label="$t('holiday.name')">
                    <el-input
                            v-model="form.name"
                            :placeholder="$t('holiday.name_placeholder')"
                            maxlength="50"
                            show-word-limit
                    />
                    <div class="hint">{{ $t('holiday.name_hint') }}</div>
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="editVisible = false">{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submit">
                        {{ $t('common.save') }}
                    </el-button>
                </div>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import { ref, computed, onMounted, onUnmounted } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import { Edit, Delete, Refresh, Plus } from '@element-plus/icons-vue'
    import SwipeRow from '@/components/SwipeRow.vue'
    import { getHolidays, addHoliday, updateHoliday, deleteHoliday } from '@/api/holiday'
    
    const { t } = useI18n()

    const loading = ref(false)
    const saving = ref(false)
    const holidays = ref([]) // [{id,start_date,end_date,name}]

    const editVisible = ref(false)
    const mode = ref('add') // add | edit
    const form = ref({ id: null, date_range: [], start_date: '', end_date: '', name: '' })

    /* 响应式：手机识别 */
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)

    const rowsSorted = computed(() => {
        return [...(holidays.value || [])].sort((a, b) =>
            String(b.start_date).localeCompare(String(a.start_date))
        )
    })

    const loadAll = async () => {
        loading.value = true
        try {
            const res = await getHolidays()
            holidays.value = Array.isArray(res) ? res : (res?.data || [])
        } finally {
            loading.value = false
        }
    }

    const openAdd = () => {
        mode.value = 'add'
        form.value = { id: null, date_range: [], start_date: '', end_date: '', name: '' }
        editVisible.value = true
    }

    const openEdit = (row) => {
        mode.value = 'edit'
        form.value = { 
            id: row.id, 
            date_range: [row.start_date, row.end_date], 
            start_date: row.start_date,
            end_date: row.end_date,
            name: row.name || '' 
        }
        editVisible.value = true
    }

    const submit = async () => {
        let start_date = ''
        let end_date = ''

        if (isMobile.value) {
            start_date = form.value.start_date
            end_date = form.value.end_date
        } else {
            const range = form.value.date_range
            if (range && range.length === 2) {
                start_date = range[0]
                end_date = range[1]
            }
        }

        if (!start_date || !end_date) {
            return ElMessage.warning(t('holiday.select_date_warning') || '请选择日期范围')
        }

        if (start_date > end_date) {
            return ElMessage.warning(t('holiday.date_order_warning') || '开始日期不能晚于结束日期')
        }

        const name = String(form.value.name || '').trim()

        saving.value = true
        try {
            const payload = {
                start_date: start_date,
                end_date: end_date,
                name: name
            }

            if (mode.value === 'add') {
                await addHoliday(payload)
                ElMessage.success(t('common.save_success'))
            } else {
                await updateHoliday(form.value.id, payload)
                ElMessage.success(t('common.save_success'))
            }

            editVisible.value = false
            await loadAll()
        } catch (e) {
            console.error(e)
        } finally {
            saving.value = false
        }
    }

    const confirmDelete = async (row) => {
        const displayDate = row.start_date === row.end_date 
            ? row.start_date 
            : `${row.start_date} ~ ${row.end_date}`
            
        try {
            await ElMessageBox.confirm(
                t('holiday.delete_confirm', { date: displayDate, name: row.name ? `(${row.name})` : '' }),
                t('home.op_confirm'),
                {
                    type: 'warning',
                    confirmButtonText: t('holiday.delete'),
                    cancelButtonText: t('common.cancel'),
                    confirmButtonClass: 'el-button--danger'
                }
            )
        } catch {
            return
        }

        saving.value = true
        try {
            await deleteHoliday(row.id)
            ElMessage.success(t('common.operation_success'))
            await loadAll()
        } finally {
            saving.value = false
        }
    }

    onMounted(async () => {
        window.addEventListener('resize', onResize)
        await loadAll()
    })
    onUnmounted(() => {
        window.removeEventListener('resize', onResize)
    })
</script>

<style scoped>
    /* ✅ 避免横向滚动条 */
    .page,
    .page * {
        box-sizing: border-box;
    }

    .page {
        display: flex;
        flex-direction: column;
        gap: 14px;
        padding: 16px;
        background-color: var(--bg-page);
        min-height: 100%;
        overflow-x: hidden;
    }

    /* ✅ 两块统一风格 */
    .panel {
        border-radius: 14px;
        overflow: hidden;
    }

    /* 方块 1：上下居中 + 更紧凑 */
    .panel-head {
        display: flex;
        justify-content: space-between;
        align-items: center; /* 上下居中 */
        gap: 12px;
        padding: 14px 16px; /* 更紧凑 */
        flex-wrap: wrap;
    }

    .header-left {
        display: flex;
        flex-direction: column;
        gap: 4px; /* 更紧凑 */
        min-width: 240px;
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
        gap: 10px;
        align-items: center; /* 上下居中 */
        flex-wrap: wrap;
        justify-content: flex-end;
    }

    /* 方块 2：内容区 */
    /* .panel-body replaced by layout if needed */

    /* 表格：行距更舒服一些 */
    :deep(.el-table) {
        border: none !important;
        background: transparent;
    }
    :deep(.el-table__inner-wrapper::before) {
        height: 0 !important;
    }
    :deep(.el-table__border-left-patch) {
        width: 0 !important;
    }
    :deep(.el-table .el-table__cell) {
        padding: 18px 0 !important; /* 行距更大 */
    }
    :deep(.el-table__header th) {
        background-color: transparent !important;
        color: var(--text-regular) !important;
        font-weight: 600;
        padding: 12px 0 !important;
        border-bottom: 1px solid var(--divider-color) !important;
    }
    :deep(.el-table__body td) {
        border-bottom: 1px solid var(--divider-color) !important;
    }

    .date-mono {
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
        font-weight: 700;
        color: var(--text-primary);
    }

    /* 操作区：图标 + 文案 */
    .op {
        display: inline-flex;
        gap: 10px;
        justify-content: flex-end;
    }

    /* Mobile */
    .mobile-wrap {
        width: 100%;
    }

    .mobile-list {
        overflow: hidden;
    }

    .m-item {
        padding: 14px 12px;
        border-bottom: 1px solid var(--divider-color);
    }

    .m-top {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        margin-bottom: 8px;
    }

    .m-date {
        font-weight: 800;
        font-size: 14px;
        color: var(--text-primary);
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
    }

    .m-sub {
        font-size: 12px;
        display: flex;
        gap: 10px;
        align-items: center;
    }

    .m-sub .k {
        color: var(--text-regular);
        white-space: nowrap;
    }

    .m-sub .v {
        color: var(--text-primary);
        font-weight: 400;
        min-width: 0;
    }

    .ell {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .m-hint {
        padding: 10px 12px;
        font-size: 12px;
        color: #cbd5e1;
        text-align: center;
    }

    .empty {
        padding: 16px 12px;
        color: #94a3b8;
        text-align: center;
    }

    /* dialog */
    :deep(.adaptive-dialog .el-dialog__body) {
        max-height: calc(100vh - 160px);
        overflow: auto;
        padding-top: 10px;
    }

    .dlg-footer {
        display: flex;
        justify-content: flex-end;
        gap: 12px;
        padding-top: 10px;
    }

    .hint {
        margin-top: 6px;
        font-size: 12px;
        color: #94a3b8;
        line-height: 1.4;
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

    .mobile-date-picker-wrap {
        display: flex;
        align-items: center;
        gap: 8px;
        width: 100%;
    }
    .range-sep {
        color: var(--text-regular);
        padding: 0 4px;
    }

    @media (max-width: 768px) {
        .page {
            padding: 12px;
        }

        .header-right {
            width: 100%;
            justify-content: flex-start;
        }

        .header-right :deep(.el-button) {
            flex: 1;
        }

        .dlg-footer {
            justify-content: stretch;
        }
        .dlg-footer :deep(.el-button) {
            flex: 1;
        }

        :deep(.adaptive-dialog .el-dialog__body) {
            max-height: calc(100vh - 120px) !important;
        }
    }
</style>
