import http from '@/utils/http'

/**
 * 探测设备上网使用状态 (免密码，依据客户端IP)
 * @param {Object} params { debug_ip }
 */
export const detectUsage = (params) => http.get('/api/usage/detect', { params })

/**
 * 开始连网上网 (仅限时长控制模式)
 * @param {Object} params { debug_ip }
 */
export const startInternet = (params) => http.post('/api/usage/start', null, { params })

/**
 * 暂停/结束连网上网 (仅限时长控制模式)
 * @param {Object} params { debug_ip }
 */
export const stopInternet = (params) => http.post('/api/usage/stop', null, { params })

/**
 * 获取今日连网历史明细
 * @param {Object} params { debug_ip }
 */
export const getUsageHistory = (params) => http.get('/api/usage/history', { params })
