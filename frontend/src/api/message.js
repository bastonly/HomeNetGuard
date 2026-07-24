import http from '@/utils/http'

/**
 * 获取安全审计日志
 * @param {Object} params { limit: number }
 */
export const getSecurityLogs = (params) => http.get('/api/messages/security', { params })

/**
 * 标记单条日志为已读
 */
export const markLogAsRead = (id) => http.post(`/api/messages/security/${id}/read`)

/**
 * 标记所有审计日志为已读
 */
export const markAllLogsAsRead = () => http.post('/api/messages/security/mark-all-read')

/**
 * 获取未读数量
 */
export const getUnreadCount = () => http.get('/api/messages/security/unread_count')

/**
 * 清空安全日志
 */
export const clearSecurityLogs = () => http.get('/api/messages/security/clear')

/**
 * 获取安全日志详情（关联域名等）
 */
export const getSecurityLogDetails = (id) => http.get(`/api/messages/security/${id}/details`)
