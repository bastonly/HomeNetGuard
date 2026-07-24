import http from '@/utils/http'

/**
 * 获取临时授权列表
 * @param {Object} params { groups_id: number }
 */
export const getTemporaryAuths = (params) => http.get('/api/temporary', { params })

/**
 * 新增临时授权
 */
export const addTemporaryAuth = (data) => http.post('/api/temporary', data)

/**
 * 更新临时授权
 */
export const updateTemporaryAuth = (id, data) => http.put(`/api/temporary/${id}`, data)

/**
 * 删除临时授权
 */
export const deleteTemporaryAuth = (id) => http.delete(`/api/temporary/${id}`)
