import http from '@/utils/http'

/**
 * 获取统计数据概览
 */
export const getStatsSummary = () => http.get('/api/stats/overview')

/**
 * 获取首页统计简报
 */
export const getHomeStats = () => http.get('/api/stats/home')

/**
 * 获取 AdGuard 远程配置
 */
export const fetchRemoteConfig = () => http.get('/api/stats/fetch-remote')

/**
 * 同步所有 AdGuard 状态
 */
export const syncAllStats = () => http.get('/api/stats/sync/all')


/**
 * 获取 AdGuard Home 运行状态
 */
export const getAghStatus = () => http.get('/api/stats/agh/status')

/**
 * 控制 AdGuard Home (启动/恢复/暂停)
 */
export const controlAgh = (data) => http.post('/api/stats/agh/control', data)

/**
 * 获取指定设备的统计详情
 */
export const getDeviceStats = (id, params) => http.get(`/api/stats/device/${id}`, { params })
