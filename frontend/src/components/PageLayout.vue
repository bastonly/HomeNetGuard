<template>
    <div class="page-container">
        <!-- 顶部面板：标题/副标题/右侧操作 -->
        <div class="panel">
            <div class="panel-head">
                <div class="title-block">
                    <h2 class="title">{{ title }}</h2>
                    <div v-if="subtitle" class="sub">{{ subtitle }}</div>
                    <!-- 允许页面自己扩展说明/小提示 -->
                    <slot name="subtitleExtra" />
                </div>

                <div class="actions-ui">
                    <slot name="actions" />
                </div>
            </div>

            <!-- 如果页面想在 header 下加一条横线/分割线或其他内容 -->
            <slot name="headerBottom" />
        </div>

        <!-- 主内容（可以是一个 panel，也可以是多个 panel） -->
        <slot />
    </div>
</template>

<script setup>
    defineProps({
        title: { type: String, required: true },
        subtitle: { type: String, default: '' }
    })
</script>

<style scoped>
    /* 这里不写任何“设计系统”CSS，全部交给你的 3 个全局 css（ui-base/ui-tokens/ui-elementplus） */
    /* 只保留与组件结构强绑定的最小样式（也可以为空） */
</style>
