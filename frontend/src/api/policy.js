import http from '@/utils/http'

/**
 * 获取上网策略列表
 * @param {Object} params { groups_id: number }
 */
export const getPolicies = (params) => http.get('/api/policies', { params })

/**
 * 新增上网策略
 */
export const addPolicy = (data) => http.post('/api/policies', data)

/**
 * 更新上网策略
 */
export const updatePolicy = (id, data) => http.put(`/api/policies/${id}`, data)

/**
 * 删除上网策略
 */
export const deletePolicy = (id) => http.delete(`/api/policies/${id}`)

/**
 * 获取设备组的节假日控制模式
 */
export const getGroupHolidayMode = (groupsId) => http.get(`/api/policies/groups/${groupsId}/holiday-mode`)

/**
 * 更新设备组的节假日控制模式
 */
export const updateGroupHolidayMode = (groupsId, holidayMode) => http.put(`/api/policies/groups/${groupsId}/holiday-mode`, { holiday_mode: holidayMode })
