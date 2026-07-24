<script setup>
    import { onMounted } from 'vue'
    import { useRouter } from 'vue-router'
    import { ElMessage } from 'element-plus'
    import { logout } from '@/api/auth'

    const router = useRouter()

    onMounted(async () => {
        try {
            // 1. 通知后端记录退出日志
            // 后端 auth.py 的 /api/logout 加上了 @jwt_required()
            // 拦截器会自动带上当前的 access_token
            await logout()
        } catch (e) {
            console.error('后端退出记录失败:', e)
        } finally {
            // 2. 核心：必须清除 localStorage 中的所有 Token
            // 你也可以使用 localStorage.removeItem('access_token') 等精准删除
            localStorage.clear()
            sessionStorage.clear()

            // 3. 提示并跳转
            ElMessage.success('已安全退出登录')

            // 使用 replace 而不是 push，防止用户点击浏览器后退键回到退出页
            router.replace('/login')
        }
    })
</script>

<template>
    <div class="logout-loading">
        <el-icon class="is-loading"><Loading /></el-icon>
        <span>正在安全退出，请稍候...</span>
    </div>
</template>

<style scoped>
    .logout-loading {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 80vh;
        gap: 12px;
        color: #909399;
    }
</style>
