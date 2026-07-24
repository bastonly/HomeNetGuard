import { createRouter, createWebHistory } from 'vue-router'
import axios from 'axios'
import Login from '@/views/Login.vue'
import Layout from '@/layout/Layout.vue'
import Home from '@/views/Home.vue'
import DeviceManagement from '@/views/DeviceManagement.vue'
import GroupManagement from '@/views/GroupManagement.vue'
import PolicyManagement from '@/views/PolicyManagement.vue'
import TemporaryAuth from '@/views/TemporaryAuth.vue'
import AccessStatistics from '@/views/AccessStatistics.vue'
import DeviceStats from '@/views/DeviceStats.vue'
import HolidayManagement from '@/views/HolidayManagement.vue'
import Messages from '@/views/Messages.vue'
import EntertainmentReport from '@/views/EntertainmentReport.vue'
import Logout from '@/views/Logout.vue'
import SystemSettings from '@/views/SystemSettings.vue'

const routes = [
    { path: '/login', component: Login },
    { path: '/usage', component: () => import('@/views/UsagePortal.vue') },
    {
        path: '/',
        component: Layout,
        children: [
            { path: '', component: Home },
            { path: 'devices', component: DeviceManagement },
            { path: 'groups', component: GroupManagement },
            { path: 'groups/usage', component: () => import('@/views/AdminRestrictedGroup.vue') },
            { path: 'policy', component: PolicyManagement },
            { path: 'temp', component: TemporaryAuth },
            { path: 'stats', component: AccessStatistics },
            { path: 'stats/device/:id', component: DeviceStats },
            { path: 'holiday', component: HolidayManagement },
            { path: 'messages', component: Messages },
            { path: 'entertainment', component: EntertainmentReport },
            { path: 'settings', component: SystemSettings }, // 系统设置
            { path: 'logout', component: Logout }
        ]
    },

    { path: '/:pathMatch(.*)*', redirect: '/' }
]

const router = createRouter({
    history: createWebHistory(),
    routes
})

const isDev = import.meta.env.DEV
const API_BASE = isDev ? (import.meta.env.VITE_API_BASE || '') : ''

router.beforeEach(async (to) => {
    const token = localStorage.getItem('access_token')

    // 1. 基础拦截：如果是去登录页或免密上网端，直接放行
    // 关键：必须放掉，否则会死循环
    if (to.path === '/login' || to.path === '/usage') {
        // 如果已经有 token 且想去登录页，直接送回首页
        if (to.path === '/login' && token) return { path: '/' }
        return true
    }

    // 2. 权限拦截：如果没有 token，且不是去登录页，检测是否是受限设备
    if (!token) {
        try {
            const res = await axios.get(`${API_BASE}/api/usage/detect`)
            if (res.data && res.data.is_restricted) {
                return { path: '/usage' }
            }
        } catch (e) {
            console.error('Failed to detect device restriction status:', e)
        }
        return { path: '/login' }
    }

    // 3. 只要本地有 token，直接放行
    // 真正的有效性交给 http 拦截器。如果 token 过期，
    // 页面发起具体的业务请求时，拦截器收到 401 会自动踢回登录页。
    return true

})

export default router
