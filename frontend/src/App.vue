<template>
    <el-config-provider :locale="locale">
        <router-view />
    </el-config-provider>
</template>

<script setup>
    import { computed } from 'vue'
    import { useI18n } from 'vue-i18n'
    import zhCn from 'element-plus/dist/locale/zh-cn.mjs'
    import en from 'element-plus/dist/locale/en.mjs'

    const { locale: i18nLocale } = useI18n()

    const locale = computed(() => {
        return i18nLocale.value === 'zh' ? zhCn : en
    })
</script>
