<template>
    <div class="pager" v-if="total > 0">
        <div class="left">
            <span class="hint">{{ $t('common.total_items', { total }) }}</span>
        </div>

        <el-pagination
                v-model:current-page="innerPage"
                v-model:page-size="innerSize"
                :total="total"
                :page-sizes="pageSizes"
                :layout="layout"
                :size="size"
                :background="background"
                @size-change="emitChange"
                @current-change="emitChange"
        />
    </div>
</template>

<script setup>
    import { computed } from 'vue'
    import { useI18n } from 'vue-i18n'

    const { t } = useI18n()

    const props = defineProps({
        total: { type: Number, default: 0 },
        page: { type: Number, default: 1 },         // v-model:page
        pageSize: { type: Number, default: 20 },    // v-model:pageSize
        pageSizes: { type: Array, default: () => [10, 20, 50, 100] },
        size: { type: String, default: 'default' },
        background: { type: Boolean, default: true },
        layout: {
            type: String,
            default: 'prev, pager, next, sizes, jumper'
        }
    })

    const emit = defineEmits([
        'update:page',
        'update:pageSize',
        'change'
    ])

    const innerPage = computed({
        get: () => props.page,
        set: (v) => emit('update:page', v)
    })

    const innerSize = computed({
        get: () => props.pageSize,
        set: (v) => emit('update:pageSize', v)
    })

    const emitChange = () => {
        emit('change', { page: props.page, pageSize: props.pageSize, total: props.total })
    }
</script>

<style scoped>
    .pager {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        padding-top: 10px;
        flex-wrap: wrap;
    }
    .hint {
        color: #999;
        font-size: 12px;
    }
    @media (max-width: 768px) {
        .pager {
            justify-content: center;
        }
    }
</style>
