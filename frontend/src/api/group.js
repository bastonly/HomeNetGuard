import http from '@/utils/http'

/**
 * 获取设备组列表
 * @param {Object} params { kind: number }
 */
export const getGroups = (params) => http.get('/api/groups', { params })

/**
 * 获取带状态的设备组列表
 */
export const getGroupsWithStatus = () => http.get('/api/groups/status')

/**
 * 修改分组信息
 */
export const updateGroup = (id, data) => http.put(`/api/groups/${id}`, data)

/**
 * 删除分组
 */
export const deleteGroup = (id) => http.delete(`/api/groups/${id}`)
