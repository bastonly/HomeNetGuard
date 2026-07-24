import http from '@/utils/http'

/**
 * 获取节假日列表
 */
export const getHolidays = () => http.get('/api/holidays')

/**
 * 新增节假日
 */
export const addHoliday = (data) => http.post('/api/holidays', data)

/**
 * 更新节假日
 */
export const updateHoliday = (id, data) => http.put(`/api/holidays/${id}`, data)

/**
 * 删除节假日
 */
export const deleteHoliday = (id) => http.delete(`/api/holidays/${id}`)
