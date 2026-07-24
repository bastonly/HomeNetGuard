import http from '@/utils/http'

/**
 * 获取设备列表 (带组名详情)
 */
export const getDeviceList = () => http.get('/api/devices/list')

/**
 * 获取基础设备列表
 */
export const listDevices = () => http.get('/api/devices')

/**
 * 新增设备
 */
export const addDevice = (data) => http.post('/api/devices', data)

/**
 * 更新设备
 */
export const updateDevice = (id, data) => http.put(`/api/devices/${id}`, data)

/**
 * 删除设备
 */
export const deleteDevice = (id) => http.delete(`/api/devices/${id}`)
