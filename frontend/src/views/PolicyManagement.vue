<template>
    <div class="page">
        <!-- ===================== 方块 1：标题 + 说明 + 筛选 + 新增 ===================== -->
        <div class="panel">
            <div class="panel-head">
                <div class="header-left">
                    <div class="title-row">
                        <h2 class="title">{{ $t('policy.title') }}</h2>
                        <el-button
                            v-if="isMobile"
                            class="refresh-circle"
                            circle
                            :loading="loadingPolicies"
                            @click="loadPolicies"
                            :disabled="!selectedGroupId"
                        >
                            <el-icon><Refresh /></el-icon>
                        </el-button>
                    </div>
                    <div class="sub">
                        <div>{{ $t('policy.rule_1') }}</div>
                        <div>{{ $t('policy.rule_2') }}</div>
                    </div>
                </div>

                <div class="header-right">
                    <el-select
                            v-model="selectedGroupId"
                            :placeholder="$t('policy.select_group_placeholder')"
                            class="group-select"
                            :loading="loadingGroups"
                            @change="onGroupChange"
                    >
                        <el-option v-for="g in allowedGroups" :key="g.id" :label="g.label" :value="g.id" />
                    </el-select>

                    <el-button
                            type="primary"
                            :icon="Plus"
                            class="btn-add"
                            @click="openAdd"
                            :disabled="isAddDisabled"
                    >
                        {{ addButtonText }}
                    </el-button>
                </div>
            </div>
        </div>

        <!-- ===================== 方块 2：Tabs + 列表/表格 ===================== -->
        <div class="panel mobile-naked">
            <div class="tabs-head">
                <el-tabs v-model="activeType" class="tabs">
                    <el-tab-pane :label="$t('policy.workday')" name="workday" />
                    <el-tab-pane :label="$t('policy.weekend')" name="weekend" />
                    <el-tab-pane :label="$t('policy.holiday')" name="holiday" />
                </el-tabs>
            </div>

            <div class="panel-body">
                <!-- 节假日全局控制模式选择器 -->
                <div v-if="activeType === 'holiday' && selectedGroupId" class="holiday-mode-selector">
                    <span class="mode-label">节假日控制模式：</span>
                    <el-radio-group v-model="holidayMode" @change="handleHolidayModeChange" size="default">
                        <el-radio-button label="period">{{ $t('policy.limit_type_period') }}</el-radio-button>
                        <el-radio-button label="duration">{{ $t('policy.limit_type_duration') }}</el-radio-button>
                    </el-radio-group>
                </div>
                <!-- PC：表格 -->
                <el-table
                        v-if="!isMobile"
                        :data="currentPolicies"
                        v-loading="loadingPolicies"
                        class="modern-table"
                        :empty-text="selectedGroupId ? $t('policy.empty_slot_hint') : $t('policy.select_group_first')"
                        style="width: 100%"
                >
                    <el-table-column :label="$t('policy.time_range')" min-width="240">
                        <template #default="scope">
                            <span v-if="scope.row.limit_type === 'duration'">
                                {{ $t('policy.duration_limit') }}: <b>{{ scope.row.duration_limit }}</b> 分钟
                            </span>
                            <el-tag v-else type="info">{{ scope.row.from_at }} - {{ scope.row.end_at }}</el-tag>
                        </template>
                    </el-table-column>

                    <el-table-column :label="$t('policy.group')" min-width="180">
                        <template #default>
                            <span class="group-name">{{ selectedGroupName || '-' }}</span>
                        </template>
                    </el-table-column>

                    <el-table-column :label="$t('policy.enable_status')" width="120">
                        <template #default="scope">
                            <el-switch
                                    v-model="scope.row.is_enabled"
                                    :active-value="1"
                                    :inactive-value="0"
                                    @change="handleStatusChange(scope.row)"
                            />
                        </template>
                    </el-table-column>

                    <el-table-column :label="$t('common.actions')" width="180" fixed="right">
                        <template #default="scope">
                            <el-button type="primary" link :icon="Edit" @click="openEdit(scope.row)">{{ $t('common.edit') }}</el-button>
                            <el-button type="danger" link :icon="Delete" @click="confirmDelete(scope.row)">{{ $t('common.delete') }}</el-button>
                        </template>
                    </el-table-column>
                </el-table>

                <!-- Mobile：SwipeRow -->
                <div v-else class="mobile-wrap" v-loading="loadingPolicies">
                    <div v-if="!loadingPolicies && currentPolicies.length === 0" class="empty">
                        {{ selectedGroupId ? $t('policy.empty_slot_hint') : $t('policy.select_group_first') }}
                    </div>

                    <div v-else class="mobile-list">
                        <SwipeRow
                                v-for="row in currentPolicies"
                                :key="row.id"
                                :actions="['edit','delete']"
                                @edit="openEdit(row)"
                                @delete="confirmDelete(row)"
                        >
                            <div class="m-item">
                                <div class="m-top">
                                    <div class="m-range">
                                        <span v-if="row.limit_type === 'duration'">
                                            {{ $t('policy.duration_limit') }}: <b>{{ row.duration_limit }}</b> 分钟
                                        </span>
                                        <span v-else>{{ row.from_at }} - {{ row.end_at }}</span>
                                    </div>
                                    <div class="m-actions-switch">
                                        <el-switch
                                                v-model="row.is_enabled"
                                                :active-value="1"
                                                :inactive-value="0"
                                                size="small"
                                                @change="handleStatusChange(row)"
                                        />
                                    </div>
                                </div>
                                <div class="m-sub" style="margin-top: 4px; display: flex; justify-content: space-between; align-items: center;">
                                    <span class="v" style="font-size: 12px; color: var(--text-regular);">{{ selectedGroupName || '-' }}</span>
                                </div>
                            </div>

                            <template #editIcon><el-icon><Edit /></el-icon></template>
                            <template #deleteIcon><el-icon><Delete /></el-icon></template>
                        </SwipeRow>
                    </div>
                </div>
            </div>
        </div>

        <!-- ===================== 新增/编辑弹窗 ===================== -->
        <el-dialog
                v-model="editVisible"
                :title="editMode === 'add' ? $t('policy.add_slot') : $t('policy.edit_slot')"
                :width="dialogWidth"
                :fullscreen="isMobile"
                :close-on-click-modal="false"
                class="adaptive-dialog"
                destroy-on-close
        >
            <el-form :model="editForm" label-width="90px">
                <el-form-item :label="$t('policy.group')">
                    <el-input :value="selectedGroupName" disabled />
                </el-form-item>

                <el-form-item :label="$t('policy.type')" required>
                    <el-select v-model="editForm.type" style="width: 100%">
                        <el-option :label="$t('policy.workday')" value="workday" />
                        <el-option :label="$t('policy.weekend')" value="weekend" />
                        <el-option :label="$t('policy.holiday')" value="holiday" />
                    </el-select>
                </el-form-item>

                <!-- 移除弹窗内 limit_type 单选框，因为已由顶层控制 -->

                <template v-if="editForm.type !== 'holiday' || editForm.limit_type !== 'duration'">
                    <el-form-item :label="$t('policy.start_time')" required>
                        <el-time-picker
                                v-model="editForm.from_at"
                                format="HH:mm"
                                value-format="HH:mm"
                                :placeholder="$t('policy.select_start')"
                                style="width: 100%"
                        />
                    </el-form-item>

                    <el-form-item :label="$t('policy.end_time')" required>
                        <el-time-picker
                                v-model="editForm.end_at"
                                format="HH:mm"
                                value-format="HH:mm"
                                :placeholder="$t('policy.select_end')"
                                style="width: 100%"
                        />
                    </el-form-item>
                </template>

                <el-form-item v-if="editForm.type === 'holiday' && editForm.limit_type === 'duration'" :label="$t('policy.duration_limit')" required>
                    <el-input-number v-model="editForm.duration_limit" :min="1" :placeholder="$t('policy.duration_limit_hint')" style="width: 100%" />
                </el-form-item>

                <el-form-item label-width="0">
                    <el-alert
                            type="warning"
                            show-icon
                            :closable="false"
                            :title="$t('policy.validate_rule')"
                            :description="$t('policy.validate_hint')"
                    />
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="editVisible = false">{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submitPolicy">{{ $t('common.save') }}</el-button>
                </div>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import { ref, computed, onMounted, onUnmounted } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { ElMessage, ElMessageBox } from 'element-plus'
    import { Edit, Delete, Plus, Refresh } from '@element-plus/icons-vue'
    import { getPolicies, addPolicy, updatePolicy, deletePolicy, getGroupHolidayMode, updateGroupHolidayMode } from '@/api/policy'
    import { getGroups } from '@/api/group'
    import { formatGroupName } from '@/utils/common.js'
    import SwipeRow from '@/components/SwipeRow.vue'
    
    const { t } = useI18n()

    const loadingGroups = ref(false)
    const loadingPolicies = ref(false)
    const saving = ref(false)

    const groups = ref([])
    const selectedGroupId = ref(null)
    const activeType = ref('workday')

    const editVisible = ref(false)
    const editMode = ref('add')
    const editForm = ref({ id: null, type: 'workday', from_at: '', end_at: '', limit_type: 'period', duration_limit: 120 })
    const holidayMode = ref('period')

    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)
    const dialogWidth = computed(() => (isMobile.value ? '100%' : '560px'))

    const typeLabel = (type) => {
        if (type === 'workday') return t('policy.workday')
        if (type === 'weekend') return t('policy.weekend')
        if (type === 'holiday') return t('policy.holiday')
        return type || '-'
    }

    const toMin = (hhmm) => {
        if (!hhmm) return 0
        const s = String(hhmm)
        const h = Number(s.slice(0, 2)) || 0
        const m = Number(s.slice(3, 5)) || 0
        return h * 60 + m
    }
    const isOverlap = (aStart, aEnd, bStart, bEnd) => Math.max(aStart, bStart) < Math.min(aEnd, bEnd)

    const allowedGroups = computed(() => groups.value || [])
    const selectedGroupName = computed(() => {
        const g = (groups.value || []).find(x => x.id === selectedGroupId.value)
        return g?.label || ''
    })

    const policiesByType = ref({ workday: [], weekend: [], holiday: [] })
    const currentPolicies = computed(() => {
        const all = policiesByType.value[activeType.value] || []
        if (activeType.value === 'holiday') {
            return all.filter(p => p.limit_type === holidayMode.value)
        }
        return all
    })

    const isAddDisabled = computed(() => {
        if (!selectedGroupId.value) return true
        if (activeType.value === 'holiday' && holidayMode.value === 'duration') {
            const hasDuration = (policiesByType.value.holiday || []).some(p => p.limit_type === 'duration')
            return hasDuration
        }
        return false
    })

    const addButtonText = computed(() => {
        if (activeType.value === 'holiday' && holidayMode.value === 'duration') {
            return '设置上网时长'
        }
        return t('policy.add_slot')
    })

    const validateRange = ({ id, type, from_at, end_at, limit_type, duration_limit }) => {
        if (!type) return t('policy.type')
        
        if (type === 'holiday' && limit_type === 'duration') {
            if (!duration_limit || Number(duration_limit) <= 0) return '请输入有效的上网时长'
            return ''
        }

        if (!from_at || !end_at) return t('policy.select_start')
        const s = toMin(from_at)
        const e = toMin(end_at)
        if (s >= e) return t('policy.validate_hint')

        const arr = policiesByType.value[type] || []
        for (const p of arr) {
            if (p.id === id) continue
            if (p.limit_type === 'duration') continue
            const ps = toMin(p.from_at)
            const pe = toMin(p.end_at)
            if (isOverlap(s, e, ps, pe)) return `时间段与已有区间重叠：${p.from_at}-${p.end_at}`
        }
        return ''
    }

    const loadGroups = async () => {
        loadingGroups.value = true
        try {
            const res = await getGroups({ kind: 2 })
            const arr = Array.isArray(res) ? res : (res?.data || [])
            groups.value = arr.map(item => {
                return { ...item, label: formatGroupName(item, t) }
            })
            if (!selectedGroupId.value && allowedGroups.value.length > 0) {
                selectedGroupId.value = allowedGroups.value[0].id
            }
        } finally {
            loadingGroups.value = false
        }
    }

    const loadHolidayMode = async () => {
        if (!selectedGroupId.value) return
        try {
            const res = await getGroupHolidayMode(selectedGroupId.value)
            holidayMode.value = res.holiday_mode || 'period'
        } catch (err) {
            console.error('Failed to load holiday mode:', err)
        }
    }

    const handleHolidayModeChange = async (val) => {
        if (!selectedGroupId.value) return
        try {
            await updateGroupHolidayMode(selectedGroupId.value, val)
            ElMessage.success(t('common.save_success'))
            await loadPolicies()
        } catch (err) {
            ElMessage.error(t('common.operation_failed'))
            holidayMode.value = val === 'period' ? 'duration' : 'period'
        }
    }

    const loadPolicies = async () => {
        if (!selectedGroupId.value) return
        loadingPolicies.value = true
        try {
            await loadHolidayMode()
            const res = await getPolicies({ groups_id: selectedGroupId.value })
            const list = Array.isArray(res) ? res : (res?.data || [])

            policiesByType.value.workday.length = 0
            policiesByType.value.weekend.length = 0
            policiesByType.value.holiday.length = 0

            for (const p of list) {
                p._fromMin = toMin(p.from_at)
                const t = p.type || 'workday'
                if (policiesByType.value[t]) policiesByType.value[t].push(p)
            }

            policiesByType.value.workday.sort((a, b) => a._fromMin - b._fromMin)
            policiesByType.value.weekend.sort((a, b) => a._fromMin - b._fromMin)
            policiesByType.value.holiday.sort((a, b) => a._fromMin - b._fromMin)
        } finally {
            loadingPolicies.value = false
        }
    }

    const onGroupChange = async () => { await loadPolicies() }

    const openAdd = () => {
        if (!selectedGroupId.value) return ElMessage.warning(t('policy.select_group_first'))
        editMode.value = 'add'
        editForm.value = { id: null, type: activeType.value, from_at: '', end_at: '', limit_type: activeType.value === 'holiday' ? holidayMode.value : 'period', duration_limit: 120 }
        editVisible.value = true
    }

    const openEdit = (row) => {
        editMode.value = 'edit'
        editForm.value = { 
            id: row.id, 
            type: row.type, 
            from_at: row.from_at, 
            end_at: row.end_at, 
            limit_type: row.limit_type || 'period', 
            duration_limit: row.duration_limit || 120 
        }
        editVisible.value = true
    }

    const submitPolicy = async () => {
        if (!selectedGroupId.value) return ElMessage.warning('请先选择设备组')

        const err = validateRange(editForm.value)
        if (err) return ElMessage.warning(err)

        saving.value = true
        try {
            const payload = {
                groups_id: selectedGroupId.value,
                type: editForm.value.type,
                limit_type: editForm.value.limit_type || 'period',
                duration_limit: editForm.value.duration_limit || 0,
                from_at: editForm.value.from_at,
                end_at: editForm.value.end_at
            }

            if (editMode.value === 'add') {
                await addPolicy(payload)
                ElMessage.success(t('common.save_success'))
            } else {
                await updatePolicy(editForm.value.id, payload)
                ElMessage.success(t('common.save_success'))
            }

            editVisible.value = false
            await loadPolicies()
        } finally {
            saving.value = false
        }
    }

    const handleStatusChange = async (row) => {
        try {
            const payload = {
                groups_id: selectedGroupId.value,
                type: row.type,
                limit_type: row.limit_type || 'period',
                duration_limit: row.duration_limit || 0,
                from_at: row.from_at,
                end_at: row.end_at,
                is_enabled: row.is_enabled
            }
            await updatePolicy(row.id, payload)
            ElMessage.success(t('common.save_success'))
            await loadPolicies()
        } catch (err) {
            row.is_enabled = row.is_enabled === 1 ? 0 : 1
            ElMessage.error(err.response?.data?.msg || t('common.operation_failed'))
        }
    }

    const confirmDelete = async (row) => {
        try {
            const rangeDesc = row.limit_type === 'duration' ? `时长 ${row.duration_limit} 分钟` : `${row.from_at}-${row.end_at}`
            await ElMessageBox.confirm(
                t('policy.delete_confirm', { range: rangeDesc, type: typeLabel(row.type) }),
                t('home.op_confirm'),
                {
                    confirmButtonText: t('common.delete'),
                    cancelButtonText: t('common.cancel'),
                    type: 'warning',
                    confirmButtonClass: 'el-button--danger'
                }
            )
            saving.value = true
            await deletePolicy(row.id)
            ElMessage.success(t('common.operation_success'))
            await loadPolicies()
        } catch {
            // noop
        } finally {
            saving.value = false
        }
    }

    onMounted(async () => {
        window.addEventListener('resize', onResize)
        await loadGroups()
        if (selectedGroupId.value) await loadPolicies()
    })
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    /* .page, .page * moved to layout.css */

    /* .panel, .panel-head, .title, .sub, .head-right moved to layout.css */

    /* Keeping group specific override if unique, but looks standard */
    .group-select { width: 260px; }
    
    .holiday-mode-selector {
        display: flex;
        align-items: center;
        margin-bottom: 16px;
        padding: 12px 16px;
        background-color: var(--el-fill-color-blank);
        border: 1px dashed var(--el-border-color);
        border-radius: 8px;
        gap: 12px;
    }
    
    .mode-label {
        font-size: 14px;
        font-weight: 600;
        color: var(--el-text-color-regular);
    }

    .tabs-head {
        padding: 8px 14px 0 14px;
        /*border-bottom: 1px solid #f1f5f9;*/
    }
    :deep(.el-tabs__nav-wrap::after) { height: 1px; background-color: var(--divider-color); }

    .panel-body { padding: 6px 5px 0px 5px; }

    /* Table Overrides moved to overrides.css */
    /* Keeping row padding override if it differs from standard, otherwise remove */
    /* Policy had 20px, standard 16px. Keeping specfic override if desired or falling back to standard */
    :deep(.el-table .el-table__cell) { padding: 20px 0 !important; }
    
    /* Header specficity fix from previous turn, keeping here or relying on overrides? */
    /* overrides.css has :deep(.el-table .el-table__header .el-table__cell) { padding: 12px ... } */
    /* Policy had 8px. Let's keep the explicit override for Policy if it prefers tighter/looser */
    
    :deep(.el-table .el-table__header .el-table__cell) {
        padding: 8px 0 !important;
    }

    .group-name { font-size: 14px; color: var(--text-primary); }

    .el-tag--info {
        background-color: var(--divider-color);
        border-color: var(--border-light);
        color: var(--text-regular);
        font-weight: 400;
    }


    /* .mobile-wrap, .mobile-list, .m-item, .m-top, .m-sub, .empty, .dlg-footer moved to components.css/layout.css */
    
    .mobile-wrap { width: 100%; }
    
    /* Policy specific mobile range style */
    .m-range {
        font-size: 14px;
        color: var(--text-primary);
        font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
    }

    /* ✅ 手机端：下拉框 + 新增按钮同一行（不换行） */
    @media (max-width: 768px) {
        .page { padding: 12px; }

        .header-right {
            display: flex !important;
            flex-direction: row !important;
            flex-wrap: nowrap !important;
            align-items: center !important;
            width: 100% !important;
            gap: 10px !important;
        }

        .group-select {
            flex: 1 1 50% !important;
            width: 50% !important;
            max-width: 50% !important;
            min-width: 0 !important;
        }

        .btn-add {
            flex: 1 1 50% !important;
            width: 50% !important;
            max-width: 50% !important;
            white-space: nowrap !important;
            padding: 10px 8px !important;
        }
    }
</style>
