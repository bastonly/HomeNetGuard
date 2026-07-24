<template>
    <div class="page-container">
        <!-- Header -->
        <div class="header-row">
            <div class="header-left">
                <div class="title-row">
                    <h2 class="title">{{ $t('device_mgmt.title') }}</h2>
                    <el-button
                        v-if="isMobile"
                        circle
                        :loading="loading"
                        @click="loadAll"
                        :icon="Refresh"
                        :aria-label="$t('home.refresh')"
                        class="refresh-circle"
                    />
                </div>
                <div class="sub">{{ $t('device_mgmt.sub') }}</div>
            </div>

            <div class="actions" :class="{ mobile: isMobile }">
                <el-select
                        v-model="filterGroupId"
                        clearable
                        :placeholder="$t('device_mgmt.filter_placeholder')"
                        class="group-select"
                        @change="applyFilter"
                >
                    <el-option v-for="g in groupsAll" :key="g.id" :label="g.label" :value="g.id"/>
                </el-select>

                <el-button v-if="!isMobile" :loading="loading" @click="loadAll" :icon="Refresh">
                    {{ $t('home.refresh') }}
                </el-button>
            </div>
        </div>

        <div class="table-shell mobile-naked">
            <!-- PC -->
            <el-table
                    v-if="!isMobile"
                    :data="devicesView"
                    style="width: 100%"
                    v-loading="loading"
                    class="modern-table"
                    :empty-text="loading ? $t('device_mgmt.loading') : $t('home.no_device')"
            >
                <el-table-column :label="$t('home.device')" min-width="240">
                    <template #default="scope">
                        <div class="device-cell">
                            <!-- 类型图标：只显示 icon -->
                            <el-icon class="type-icon" :style="{ color: getDeviceTypeColor(scope.row.type) }">
                                <component :is="getDeviceTypeIcon(scope.row.type)"/>
                            </el-icon>

                            <div class="device-meta">
                                <div class="device-name">{{ scope.row.name || $t('home.unknown_device') }}</div>
                                <div class="device-remark">{{ scope.row.remark || '-' }}</div>
                            </div>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('home.network_info')" min-width="260">
                    <template #default="scope">
                        <div class="net-block">
                            <div class="net-line">
                                <span class="k">{{ $t('home.ip') }}</span>
                                <code class="ip">{{ scope.row.ip || $t('home.offline_status') }}</code>
                            </div>
                            <div class="net-line">
                                <span class="k">{{ $t('home.mac') }}</span>
                                <code class="mac">{{ scope.row.mac }}</code>
                            </div>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('groups.title')" min-width="240">
                    <template #default="scope">
                        <div class="group-inline">
                            <el-tag
                                    v-if="scope.row.groups_id != null"
                                    :style="tagStyle(scope.row.groups_id)"
                                    class="group-tag"
                                    effect="light"
                                    round
                            >
                                {{ groupLabelById(scope.row.groups_id) }}
                            </el-tag>
                            <span v-else class="none-text">-</span>
                        </div>
                    </template>
                </el-table-column>

                <!-- ✅ 热点共享列（替换原“设备类型”列） -->
                <el-table-column :label="$t('device_mgmt.hotpot')" width="140" align="center">
                    <template #default="scope">
                        <div class="hp-wrap">
                            <!--<span class="hp-badge">热点</span>-->
                            <el-tooltip :content="scope.row.hotpot === 1 ? $t('device_mgmt.hotpot_on') : $t('device_mgmt.hotpot_off')" placement="top"
                                        :show-arrow="false">
                                <el-icon class="hp-icon pc" :class="{ on: scope.row.hotpot === 1 }">
                                    <Share/>
                                </el-icon>
                            </el-tooltip>
                        </div>
                    </template>
                </el-table-column>

                <el-table-column :label="$t('common.actions')" width="170" fixed="right" align="right">
                    <template #default="scope">
                        <div class="op-actions">
                            <el-tooltip :content="$t('common.edit')" placement="top" :show-arrow="false">
                                <el-button type="primary" link :icon="Edit" @click="openEdit(scope.row)">
                                    {{ $t('common.edit') }}
                                </el-button>
                            </el-tooltip>

                            <el-tooltip :content="$t('common.delete')" placement="top" :show-arrow="false">
                                <el-button type="danger" link :icon="Delete" @click="confirmDelete(scope.row)">
                                    {{ $t('common.delete') }}
                                </el-button>
                            </el-tooltip>
                        </div>
                    </template>
                </el-table-column>
            </el-table>

            <!-- Mobile -->
            <div v-else class="mobile-container" v-loading="loading">
                <div v-if="!loading && devicesView.length === 0" class="empty-state">{{ $t('home.no_device') }}</div>

                <!-- ✅ 关键：actions 显式传入 edit/delete，恢复左滑动作 -->
                <SwipeRow
                        v-for="row in devicesView"
                        :key="row.id"
                        :actions="['edit', 'delete']"
                        @edit="openEdit(row)"
                        @delete="confirmDelete(row)"
                >
                    <div class="m-card">
                        <div class="m-top">
                            <div class="m-name">
                                <!-- 类型 icon 放设备名前 -->
                                <el-icon class="type-icon mobile" :style="{ color: getDeviceTypeColor(row.type) }">
                                    <component :is="getDeviceTypeIcon(row.type)"/>
                                </el-icon>

                                <div class="device-meta">
                                    <div class="device-name">{{ row.name || $t('home.unnamed_device') }}</div>
                                    <div class="device-remark ell">{{ row.remark || '-' }}</div>
                                </div>
                            </div>

                            <el-tag v-if="row.groups_id != null" :style="tagStyle(row.groups_id)" size="small" round>
                                {{ groupLabelById(row.groups_id) }}
                            </el-tag>
                        </div>

                        <!-- IP/MAC 同一行，末尾追加热点共享 icon -->
                        <div class="m-inline-net">
                          <span class="mini">
                            <span class="mk">{{ $t('home.ip') }}</span>
                            <code class="mini-code ip">{{ row.ip || $t('home.offline_status') }}</code>
                          </span>
                            <span class="sep">•</span>
                            <span class="mini">
                                <span class="mk">{{ $t('home.mac') }}</span>
                                <code class="mini-code mac">{{ row.mac }}</code>
                            </span>
                            <div class="hp-wrap">
                                <span class="hp-badge mini">{{ $t('device_mgmt.hotpot') }}</span>
                                <el-tooltip :content="row.hotpot === 1 ? $t('device_mgmt.hotpot_on') : $t('device_mgmt.hotpot_off')" placement="top"
                                            :show-arrow="false">
                                    <el-icon class="hp-icon" :class="{ on: row.hotpot === 1 }">
                                        <Share/>
                                    </el-icon>
                                </el-tooltip>
                            </div>
                        </div>
                    </div>

                    <template #editIcon>
                        <el-icon>
                            <Edit/>
                        </el-icon>
                    </template>
                    <template #deleteIcon>
                        <el-icon>
                            <Delete/>
                        </el-icon>
                    </template>
                </SwipeRow>
            </div>
        </div>

        <!-- Dialog -->
        <el-dialog
                v-model="editVisible"
                :title="$t('device_mgmt.edit_title')"
                :width="dialogWidth"
                :fullscreen="isMobile"
                destroy-on-close
                class="modern-dialog"
        >
            <el-form :model="editForm" label-position="top">
                <el-form-item :label="$t('home.device_name')" required>
                    <el-input v-model="editForm.name" :placeholder="$t('device_mgmt.name_placeholder')"/>
                </el-form-item>

                <el-form-item :label="$t('device_mgmt.mac_address')">
                    <el-input v-model="editForm.mac" disabled/>
                </el-form-item>

                <el-form-item :label="$t('groups.title')" required>
                    <el-select v-model="editForm.groups_id" :placeholder="$t('device_mgmt.group_placeholder')" style="width: 100%">
                        <el-option v-for="g in groupsAll" :key="g.id" :label="g.label" :value="g.id"/>
                    </el-select>
                </el-form-item>

                <!-- 设备类型：可选，默认 0 -->
                <el-form-item :label="$t('device_mgmt.type_placeholder')">
                    <el-select v-model="editForm.type" :placeholder="$t('device_mgmt.type_placeholder')" clearable style="width: 100%">
                        <el-option
                                v-for="opt in DEVICE_TYPE_OPTIONS"
                                :key="opt.value"
                                :label="$t(opt.label)"
                                :value="opt.value"
                        >
                            <div class="type-opt">
                                <el-icon class="type-icon" :style="{ color: opt.color }">
                                    <component :is="opt.icon"/>
                                </el-icon>
                                <span class="type-opt-label">{{ $t(opt.label) }}</span>
                            </div>
                        </el-option>
                    </el-select>
                    <div class="hint">{{ $t('device_mgmt.type_hint') }}</div>
                </el-form-item>

                <!-- ✅ 热点共享：单选 是/否 -->
                <el-form-item :label="$t('device_mgmt.hotpot')">
                    <el-radio-group v-model="editForm.hotpot" class="hp-radio">
                        <el-radio :value="1">{{ $t('device_mgmt.yes') }}</el-radio>
                        <el-radio :value="0">{{ $t('device_mgmt.no') }}</el-radio>
                    </el-radio-group>
                    <div class="hint">{{ $t('device_mgmt.hotpot_hint') }}</div>
                </el-form-item>
            </el-form>

            <template #footer>
                <div class="dlg-footer">
                    <el-button @click="editVisible = false" round>{{ $t('common.cancel') }}</el-button>
                    <el-button type="primary" :loading="saving" @click="submitEdit" round>{{ $t('device_mgmt.save') }}</el-button>
                </div>
            </template>
        </el-dialog>
    </div>
</template>

<script setup>
    import {ref, onMounted, onUnmounted, computed} from 'vue'
    import { useI18n } from 'vue-i18n'
    import SwipeRow from '@/components/SwipeRow.vue'
    import {ElMessage, ElMessageBox} from 'element-plus'
    import {Refresh, Edit, Delete} from '@element-plus/icons-vue'
    import {Share} from '@element-plus/icons-vue'
    import { getDeviceList, updateDevice, deleteDevice } from '@/api/device'
    import { formatGroupName } from '@/utils/common.js'
    import bus from '@/utils/bus'
    import { getGroups } from '@/api/group'

    import {
        normalizeDeviceType,
        getDeviceTypeIcon,
        getDeviceTypeColor,
        DEVICE_TYPE_OPTIONS
    } from '@/utils/common.js'

    const { t } = useI18n()
    const loading = ref(false)
    const saving = ref(false)

    const devicesAll = ref([])
    const devicesView = ref([])

    const groupsAll = ref([])
    const filterGroupId = ref(null)

    const editVisible = ref(false)
    const editForm = ref({id: null, mac: '', name: '', groups_id: null, type: 0, hotpot: 1})

    const normalizeHotpot = (v) => (Number(v) === 1 ? 1 : 0)

    const loadAll = async () => {
        loading.value = true
        try {
            const [d, g] = await Promise.all([
                getDeviceList().then(res => (Array.isArray(res) ? res : res.data)),
                getGroups().then(res => {
                    const arr = Array.isArray(res) ? res : res.data
                    return arr.map(item => {
                        // 若翻译 key 等于翻译结果（未找到翻译），则回退到备注或原始名称
                        const label = formatGroupName(item, t)
                        return { id: item.id, label }
                    })
                })
            ])

            const list = (d || []).map(x => ({
                ...x,
                status: (x.status === '在线' || x.status === 'online') ? 'online' : 'offline',
                groups_id: x.groups_id != null ? Number(x.groups_id) : null,
                type: normalizeDeviceType(x.type),
                hotpot: normalizeHotpot(x.hotpot)
            }))

            devicesAll.value = list
            groupsAll.value = g || []
            applyFilter()
        } finally {
            loading.value = false
        }
    }

    const applyFilter = () => {
        const gid = filterGroupId.value
        devicesView.value = !gid ? [...devicesAll.value] : devicesAll.value.filter(d => d.groups_id === gid)
    }

    const groupLabelById = (gid) => {
        if (gid == null) return '-'
        const id = Number(gid)
        return groupsAll.value.find(x => Number(x.id) === id)?.label || String(gid)
    }

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

    const openEdit = (row) => {
        editForm.value = {
            id: row.id,
            mac: row.mac,
            name: row.name || '',
            groups_id: row.groups_id ?? null,
            type: normalizeDeviceType(row.type),
            hotpot: normalizeHotpot(row.hotpot)
        }
        editVisible.value = true
    }

    const submitEdit = async () => {
        const name = String(editForm.value.name || '').trim()
        const gid = editForm.value.groups_id
        if (!name || !gid) return ElMessage.warning(t('device_mgmt.incomplete_info'))

        saving.value = true
        try {
            await updateDevice(editForm.value.id, {
                name,
                groups_id: gid,
                type: normalizeDeviceType(editForm.value.type),
                hotpot: normalizeHotpot(editForm.value.hotpot)
            })
            ElMessage.success(t('device_mgmt.update_success'))
            editVisible.value = false
            await loadAll()
        } finally {
            saving.value = false
        }
    }

    const confirmDelete = async (row) => {
        try {
            await ElMessageBox.confirm(
                t('device_mgmt.confirm_delete', { name: row.name || row.mac }), 
                t('device_mgmt.sys_tip'), 
                {
                    type: 'warning',
                    confirmButtonClass: 'el-button--danger',
                    roundButton: true,
                    confirmButtonText: t('common.confirm'),
                    cancelButtonText: t('common.cancel')
                }
            )
            await deleteDevice(row.id)
            ElMessage.success(t('device_mgmt.deleted'))
            await loadAll()
        } catch {
        }
    }

    /* responsive */
    const isMobile = ref(window.innerWidth < 768)
    const onResize = () => (isMobile.value = window.innerWidth < 768)
    const dialogWidth = computed(() => (isMobile.value ? '100%' : '520px'))

    onMounted(() => {
        window.addEventListener('resize', onResize);
        loadAll()
    })
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    /* .page-container moved to layout.css */

    /* Header */
    /* .header-row, .header-left, .title, .sub, .actions moved to layout.css */


    .group-select {
        width: 220px;
    }

    .refresh-btn {
        flex: 0 0 auto;
    }


    /* Table */
    /* Element Plus Table overrides moved to overrides.css */

    .table-shell {
        background: transparent;
    }

    /* Device cell */
    .device-cell {
        display: flex;
        align-items: flex-start;
        gap: 10px;
        min-width: 0;
    }

    .device-meta {
        min-width: 0;
    }

    .device-name {
        font-weight: 800;
        color: var(--text-primary);
        font-size: 14px;
        line-height: 1.2;
    }

    .device-remark {
        margin-top: 4px;
        font-size: 12px;
        color: var(--text-regular);
    }

    .type-icon {
        font-size: 16px;
        flex: 0 0 auto;
    }

    .type-icon.mobile {
        font-size: 18px;
    }

    .net-block {
        display: flex;
        flex-direction: column;
        gap: 8px;
    }

    .net-line {
        display: flex;
        align-items: center;
        gap: 10px;
        font-size: 12px;
    }

    /* Utilities */
    /* .k, .ip, .mac, .mono moved to layout/components.css where appropriate */

    .k {
        min-width: 28px;
    }

    .ip {
        color: var(--primary-color);
    }

    .group-inline {
        display: flex;
        align-items: center;
        gap: 10px;
        flex-wrap: wrap;
    }

    .none-text {
        color: var(--text-secondary);
    }

    .op-actions {
        display: inline-flex;
        align-items: center;
        justify-content: flex-end;
        gap: 10px;
        white-space: nowrap;
    }

    :deep(.op-actions .el-button.is-link) {
        padding: 0;
    }

    /* Hotpot icon */
    .hp-icon {
        font-size: 16px;
        color: var(--text-secondary); /* 不共享：灰 */
        vertical-align: -2px;
    }

    .hp-icon.on {
        color: #10b981; /* 共享：绿 */
    }

    .hp-icon.pc {
        font-size: 18px;
    }

    /* Mobile */
    .mobile-container {
        display: flex;
        flex-direction: column;
        gap: 12px;
    }

    .m-card {
        background: var(--bg-card);
        border: 1px solid var(--border-light);
        border-radius: 14px;
        padding: 14px;
    }

    .m-top {
        display: flex;
        justify-content: space-between;
        align-items: flex-start;
        gap: 10px;
        margin-bottom: 10px;
    }

    .m-name {
        display: flex;
        gap: 10px;
        min-width: 0;
        align-items: flex-start;
    }

    .m-inline-net {
        display: flex;
        align-items: center;
        gap: 8px;
        border-top: 1px dashed var(--divider-color);
        padding-top: 10px;
        color: var(--text-regular);
        font-size: 12px;
        flex-wrap: wrap;
    }

    .mini {
        display: inline-flex;
        align-items: center;
        gap: 6px;
    }

    .mk {
        color: var(--text-secondary);
    }

    .sep {
        color: var(--divider-color);
    }

    .ell {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .empty-state {
        padding: 18px 8px;
        color: #94a3b8;
        text-align: center;
    }

    /* Dialog footer */
    :deep(.modern-dialog .el-dialog__body) {
        padding-top: 10px;
    }

    .dlg-footer {
        display: flex;
        justify-content: flex-end;
        gap: 12px;
    }

    /* 类型下拉 option */
    .type-opt {
        display: inline-flex;
        align-items: center;
        gap: 10px;
    }

    .type-opt-label {
        font-size: 13px;
        color: var(--text-primary);
    }

    .hint {
        margin-top: 6px;
        font-size: 12px;
        color: #94a3b8;
        line-height: 1.4;
    }

    .hp-radio :deep(.el-radio) {
        margin-right: 16px;
    }

    /* Hotpot label + icon */
    .hp-wrap {
        display: inline-flex;
        align-items: center;
        gap: 6px;
    }

    .hp-badge {
        display: inline-flex;
        align-items: center;
        padding: 2px 8px;
        border-radius: 999px;
        border: 1px solid var(--border-light);
        background: var(--bg-page);
        color: var(--text-primary);
        font-weight: 700;
        font-size: 12px;
        line-height: 1;
        white-space: nowrap;
    }

    .hp-badge.mini {
        padding: 1px 7px;
        font-size: 11px;
        color: #cbd5e1;
    }

    /* Responsive */
</style>
