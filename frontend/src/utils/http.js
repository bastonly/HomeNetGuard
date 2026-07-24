import axios from 'axios'
import { ElMessageBox, ElMessage } from 'element-plus'
import router from '@/router'
import i18n from '@/i18n'

const isDev = import.meta.env.DEV
const API_BASE = isDev ? (import.meta.env.VITE_API_BASE || '') : ''

const http = axios.create({
    baseURL: API_BASE,
    withCredentials: true,
    timeout: 8000
})

// --- 无感刷新相关变量 ---
let isRefreshing = false      // 标记是否正在刷新中
let requestsQueue = []        // 存储因 401 被挂起的请求回调

// 将请求加入队列
const addSubscriber = (callback) => {
    requestsQueue.push(callback)
}

// 刷新完成后，执行队列中的所有请求
const onAccessTokenFetched = (newToken) => {
    requestsQueue.forEach((callback) => callback(newToken))
    requestsQueue = []
}

// 防止重复弹窗
let showing = false
const showErrorOnce = async (msg) => {
    if (showing) return
    showing = true
    try {
        await ElMessageBox.alert(msg || '请求失败', '提示', {
            type: 'error',
            confirmButtonText: '确定'
        })
    } finally {
        showing = false
    }
}

// 1. 请求拦截器：每次请求都带上最新的 Access Token
http.interceptors.request.use(config => {
    const token = localStorage.getItem('access_token')
    if (token) {
        config.headers.Authorization = `Bearer ${token}`
    }
    return config
})

// 2. 响应拦截器：核心优化点
http.interceptors.response.use(
    (resp) => {
        const data = resp.data
        // 兼容你原有的 ok 字段判断
        if (data && typeof data === 'object' && 'ok' in data) {
            if (data.ok === false) {
                // 如果是后端的标准错误码并需要翻译
                let errorMsg = data.msg || '操作失败'
                if (data.msg && data.msg.startsWith('api.')) {
                    errorMsg = i18n.global.t(data.msg)
                }
                
                // 如果后端在 ok:false 的同时也给 401 状态码，会被下面的 err 捕获
                // 这里处理业务逻辑上的错误
                showErrorOnce(errorMsg)
                return Promise.reject(new Error(errorMsg))
            }
        }
        return data
    },
    async (err) => {
        const { config, response } = err
        const status = response?.status

        // --- 核心：处理 401 无感刷新 ---
        if (status === 401) {
            // 如果已经在登录页了，直接报错不要刷新
            if (router.currentRoute.value.path === '/login') {
                return Promise.reject(err)
            }

            // A. 如果已经在刷新中了，把当前请求挂起放入队列
            if (isRefreshing) {
                return new Promise((resolve) => {
                    addSubscriber((newToken) => {
                        config.headers.Authorization = `Bearer ${newToken}`
                        resolve(http(config)) // 重新发起请求
                    })
                })
            }

            // B. 开启刷新流程
            const refreshToken = localStorage.getItem('refresh_token')
            if (!refreshToken) {
                // 连 refresh_token 都没有，只能去登录
                handleLogout()
                return Promise.reject(err)
            }

            isRefreshing = true

            try {
                // 注意：这里使用原生 axios 发起请求，避免进入死循环
                const res = await axios.post(`${API_BASE}/api/refresh`, {}, {
                    headers: { 'Authorization': `Bearer ${refreshToken}` }
                })

                if (res.data && res.data.access_token) {
                    const newToken = res.data.access_token
                    localStorage.setItem('access_token', newToken)

                    // 通知队列里的请求可以继续了
                    onAccessTokenFetched(newToken)

                    // 重试当前的这个请求
                    config.headers.Authorization = `Bearer ${newToken}`
                    return http(config)
                }
            } catch (refreshErr) {
                // Refresh Token 也过期或失效了
                handleLogout()
                return Promise.reject(refreshErr)
            } finally {
                isRefreshing = false
            }
        }

        // 其他非 401 错误处理
        const data = response?.data
        const msg = data?.msg || data?.message || err?.message || '网络或服务器错误'
        await showErrorOnce(msg)
        return Promise.reject(err)
    }
)

// 退出登录逻辑封装
function handleLogout() {
    localStorage.removeItem('access_token')
    localStorage.removeItem('refresh_token')
    localStorage.removeItem('username')
    ElMessage.warning('登录已过期，请重新登录')
    router.replace('/login')
}

export default http
