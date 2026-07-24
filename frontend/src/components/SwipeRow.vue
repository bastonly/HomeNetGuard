<template>
    <div
            class="swipe-row"
            ref="root"
            @touchstart.passive="onStart"
            @touchmove="onMove"
            @touchend="onEnd"
            @touchcancel="onEnd"
    >
        <!-- 主内容（可滑动） -->
        <div
                class="swipe-content"
                :style="{ transform: `translateX(${-offset}px)` }"
                :class="{ anim: animating }"
        >
            <slot />
        </div>

        <!-- 右侧动作区（隐藏在后面） -->
        <div class="swipe-actions" :style="{ width: `${effectiveActionsWidth}px` }">
            <button v-if="showEdit" class="btn edit" @click.stop="emitEdit" aria-label="edit">
                <slot name="editIcon">✎</slot>
            </button>

            <button v-if="showDelete" class="btn del" @click.stop="emitDelete" aria-label="delete">
                <slot name="deleteIcon">🗑</slot>
            </button>
        </div>
    </div>
</template>

<script setup>
    import { ref, computed } from 'vue'

    const props = defineProps({
        actionsWidth: { type: Number, default: 96 },
        actions: { type: Array, default: undefined }, // ['edit','delete']
        showEdit: { type: Boolean, default: undefined },
        showDelete: { type: Boolean, default: undefined },
        disabled: { type: Boolean, default: false }
    })

    const emit = defineEmits(['edit', 'delete', 'open'])
    const root = ref(null)

    const startX = ref(0)
    const startY = ref(0)
    const dragging = ref(false)
    const offset = ref(0)
    const animating = ref(false)

    let opened = false
    let dirLock = null // null | 'h' | 'v'

    const hasExplicitControls = computed(() => {
        return props.actions !== undefined || props.showEdit !== undefined || props.showDelete !== undefined
    })

    const showEdit = computed(() => {
        if (!hasExplicitControls.value) return true
        if (props.actions !== undefined) return props.actions.includes('edit')
        return !!props.showEdit
    })

    const showDelete = computed(() => {
        if (!hasExplicitControls.value) return true
        if (props.actions !== undefined) return props.actions.includes('delete')
        return !!props.showDelete
    })

    const buttonsCount = computed(() => (showEdit.value ? 1 : 0) + (showDelete.value ? 1 : 0))

    const effectiveActionsWidth = computed(() => {
        if (!hasExplicitControls.value) return props.actionsWidth
        return buttonsCount.value * 56 // ✅ 稍微大一点更好点按（你也可以改回 48）
    })

    const close = () => {
        opened = false
        animating.value = true
        offset.value = 0
        setTimeout(() => (animating.value = false), 180)
    }

    const open = () => {
        if (effectiveActionsWidth.value <= 0) return
        opened = true
        animating.value = true
        offset.value = effectiveActionsWidth.value
        setTimeout(() => (animating.value = false), 180)
        emit('open')
    }

    const onStart = (e) => {
        if (props.disabled) return
        if (effectiveActionsWidth.value <= 0) return

        const t = e.touches[0]
        startX.value = t.clientX
        startY.value = t.clientY
        dragging.value = true
        dirLock = null
    }

    const onMove = (e) => {
        if (props.disabled) return
        if (!dragging.value) return
        if (effectiveActionsWidth.value <= 0) return

        const t = e.touches[0]
        const dx = t.clientX - startX.value
        const dy = t.clientY - startY.value

        // ✅ 先锁方向：避免上下滚动卡住
        if (!dirLock) {
            if (Math.abs(dy) > Math.abs(dx) + 6) dirLock = 'v'
            else if (Math.abs(dx) > Math.abs(dy) + 6) dirLock = 'h'
            else return
        }

        if (dirLock === 'v') return // ✅ 纵向滚动完全放行

        // ✅ 横向滑动才 preventDefault（否则会出现你控制台那种提示 + 卡顿）
        if (e.cancelable) e.preventDefault()

        // dx<0 左滑打开；dx>0 右滑关闭
        let next = (opened ? effectiveActionsWidth.value : 0) - dx
        if (next < 0) next = 0
        if (next > effectiveActionsWidth.value) next = effectiveActionsWidth.value
        offset.value = next
    }

    const onEnd = () => {
        if (props.disabled) return
        if (!dragging.value) return
        dragging.value = false

        if (effectiveActionsWidth.value <= 0) {
            close()
            return
        }

        // 超过 1/3 就打开
        if (offset.value > effectiveActionsWidth.value / 3) open()
        else close()
    }

    const emitEdit = () => {
        close()
        emit('edit')
    }
    const emitDelete = () => {
        close()
        emit('delete')
    }

    defineExpose({ close, open })
</script>

<style scoped>
    .swipe-row {
        position: relative;
        overflow: hidden;
        width: 100%;
    }

    /* ✅ 必须撑满，否则 actions 会“透出来” */
    .swipe-content {
        position: relative;
        z-index: 2;
        width: 100%;
        min-width: 100%;
        will-change: transform;
        background: var(--bg-card, #fff); /* ✅ 避免透底，使用主题变量 */
    }

    .swipe-content.anim {
        transition: transform 0.18s ease;
    }

    .swipe-actions {
        position: absolute;
        right: 0;
        top: 0;
        bottom: 0;
        display: flex;
        z-index: 1;
    }

    .btn {
        width: 56px; /* ✅ 触控更友好 */
        border: none;
        outline: none;
        color: #fff;
        font-size: 18px;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 0;
    }

    .btn.edit { background: #409eff; }
    .btn.del { background: #f56c6c; }
</style>
