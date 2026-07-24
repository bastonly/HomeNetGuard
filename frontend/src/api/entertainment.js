import http from '@/utils/http'

/**
 * 获取娱乐网站检测记录 (分页)
 */
export const getDetections = (params) => http.get('/api/entertainment/detections', { params })

/**
 * 手动触发检测 (同步昨天和今天数据)
 */
export const triggerDetection = () => http.post('/api/entertainment/detect', {}, { timeout: 30000 })

/**
 * 获取娱乐访问统计数据
 */
export const getEntertainmentStats = (params) => http.get('/api/entertainment/stats', { params })

/**
 * 将域名加入黑名单
 */
export const blockDomain = (domain) => http.post('/api/entertainment/block', { domain })

/**
 * 将域名从黑名单移除
 */
export const unblockDomain = (domain) => http.post('/api/entertainment/unblock', { domain })
