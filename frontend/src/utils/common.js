/**
 * 全局公共工具类
 */

import {
    Iphone, Monitor, Tickets, VideoCamera, Platform, Watch,
    Connection, Reading, Lollipop, Odometer, Cpu, Sunrise
} from '@element-plus/icons-vue'

// --- 时区相关 ---
const ET_TIMEZONE = 'America/New_York';

/**
 * 将任意日期/时间戳转换为美东日期字符串 (YYYY-MM-DD)
 * @param {Date|Number|String} date
 */
export const formatToETDate = (date) => {
    if (!date) return '';
    const d = new Date(date);
    return new Intl.DateTimeFormat('en-CA', {
        timeZone: ET_TIMEZONE,
        year: 'numeric',
        month: '2-digit',
        day: '2-digit'
    }).format(d);
};

/**
 * 获取当前的美东日期字符串
 */
export const getCurrentETDate = () => {
    return formatToETDate(new Date());
};

// --- 后续可以扩展其他公共方法 ---

/**
 * 示例：简单的 MAC 地址美化
 */
export const formatMacAddress = (mac) => {
    return mac ? mac.toUpperCase().replace(/-/g, ':') : '';
};

/**
 * 设备类型定义
 * 0 未知
 * 1 手机
 * 2 电脑
 * 3 平板
 * 4 电视
 * 5 摄像头
 */
export const DEVICE_TYPE_MAP = Object.freeze({
    0: { label: 'device.type.unknown', icon: Tickets, color: '#94a3b8' },
    1: { label: 'device.type.phone', icon: Iphone, color: '#3b82f6' },
    2: { label: 'device.type.pc', icon: Platform, color: '#64748B' },
    3: { label: 'device.type.tablet', icon: Tickets, color: '#7c3aed' },
    4: { label: 'device.type.tv', icon: Monitor, color: '#06b6d4' },
    5: { label: 'device.type.camera', icon: VideoCamera, color: '#6366F1' },
    6: { label: 'device.type.watch', icon: Watch, color: '#FF5733' },
    7: { label: 'device.type.router', icon: Connection, color: '#10B981' },
    8: { label: 'device.type.fan', icon: Reading, color: '#3B82F6' },
    9: { label: 'device.type.light', icon: Lollipop, color: '#F59E0B' },
    10: { label: 'device.type.ac', icon: Odometer, color: '#06B6D4' },
    11: { label: 'device.type.cleaner', icon: Cpu, color: '#8B5CF6' },
    12: { label: 'device.type.socket', icon: Connection, color: '#64748B' },
    13: { label: 'device.type.thermometer', icon: Sunrise, color: '#EC4899' },
})

export const normalizeDeviceType = (v) => {
    const n = Number(v)
    if (!Number.isFinite(n)) return 0
    return DEVICE_TYPE_MAP[n] ? n : 0
}

export const getDeviceTypeMeta = (type) => {
    const t = normalizeDeviceType(type)
    return DEVICE_TYPE_MAP[t] || DEVICE_TYPE_MAP[0]
}

export const getDeviceTypeLabel = (type) => getDeviceTypeMeta(type).label
export const getDeviceTypeIcon = (type) => getDeviceTypeMeta(type).icon
export const getDeviceTypeColor = (type) => getDeviceTypeMeta(type).color

export const DEVICE_TYPE_OPTIONS = Object.freeze(
    Object.keys(DEVICE_TYPE_MAP).map((k) => {
        const key = Number(k)
        const m = DEVICE_TYPE_MAP[key]
        return { value: key, label: m.label, icon: m.icon, color: m.color }
    })
)
// --- 授权与设备状态相关 ---

export const LICENSE_STATUS_MAP = Object.freeze({
    'active': { label: 'license.status.active', class: 'status-active' },
    'inactive': { label: 'license.status.inactive', class: 'status-inactive' },
    'expired': { label: 'license.status.expired', class: 'status-expired' },
    'suspended': { label: 'license.status.suspended', class: 'status-warning' },
    'revoked': { label: 'license.status.revoked', class: 'status-error' },
    'pending': { label: 'license.status.pending', class: 'status-warning' },
    'unknown': { label: 'license.status.unknown', class: 'status-info' }
})

export const DEVICE_STATUS_MAP = Object.freeze({
    'active': { label: 'device.status.normal', class: 'status-active' },
    'unbound': { label: 'device.status.unbound', class: 'status-warning' },
    'revoked': { label: 'device.status.revoked', class: 'status-error' },
    'fail': { label: 'device.status.fail', class: 'status-error' },
    'unknown': { label: 'device.status.unknown', class: 'status-info' }
})

export const getLicenseStatusMeta = (status, isOffline = false) => {
    if (status === 'active' && isOffline) {
        return { label: 'license.status.active_offline', class: 'status-warning' }
    }
    return LICENSE_STATUS_MAP[status] || LICENSE_STATUS_MAP['unknown']
}

export const getDeviceStatusMeta = (status) => {
    return DEVICE_STATUS_MAP[status] || DEVICE_STATUS_MAP['unknown']
}

export const getLicenseStatusLabel = (status, isOffline) => getLicenseStatusMeta(status, isOffline).label
export const getLicenseStatusClass = (status, isOffline) => getLicenseStatusMeta(status, isOffline).class

export const getDeviceStatusLabel = (status) => getDeviceStatusMeta(status).label
export const getDeviceStatusClass = (status) => getDeviceStatusMeta(status).class

/**
 * 通用日期解析：支持 UNIX 时间戳 (秒或毫秒)、ISO 字符串、YYYY-MM-DD HH:MM:SS 等
 * @param {Date|Number|String} val 
 * @returns {Date|null}
 */
export const parseToDate = (val) => {
    if (!val) return null
    if (val instanceof Date) return val

    // 如果是数字
    if (typeof val === 'number') {
        // 如果数字小于 10^11，认为是秒级时间戳 (UNIX)
        return val < 100000000000 ? new Date(val * 1000) : new Date(val)
    }

    // 如果是纯数字字符串
    if (typeof val === 'string' && /^\d+$/.test(val)) {
        const n = Number(val)
        return n < 100000000000 ? new Date(n * 1000) : new Date(n)
    }

    try {
        // 处理 T 或 空格分隔的格式
        let s = val
        if (typeof s === 'string' && !s.includes('T')) {
            s = s.replace(/-/g, '/')
        }
        const d = new Date(s)
        return isNaN(d.getTime()) ? null : d
    } catch (e) {
        return null
    }
}

/**
 * 检查是否超过3天未签到 (离线)
 * @param {Date|Number|String} lastCheckin 
 * @returns {Boolean}
 */
export const checkIsOffline = (lastCheckin) => {
    const lastDate = parseToDate(lastCheckin)
    if (!lastDate) return false
    const now = new Date()
    return (now - lastDate) > 259200000 // 3天 (ms)
}

/**
 * 格式化分组名称（支持 i18n 翻译）
 * @param {Object} group 分组对象 { name, remark }
 * @param {Function} t i18n 翻译函数
 * @returns {String}
 */
export const formatGroupName = (group, t) => {
    if (!group) return '--'
    const name = group.name || ''
    const remark = group.remark || ''

    // 优先尝试 i18n
    if (name) {
        const key = `groups.name_list.${name.toLowerCase()}`
        const translated = t(key)
        if (translated !== key) return translated
    }

    // 回退到备注或原始名称
    return remark || name || '--'
}
/**
 * 格式化数字，添加千分位
 * @param {Number|String} num 
 * @returns {String}
 */
export const formatNumber = (num) => {
    const n = Number(num);
    return isNaN(n) ? '0' : n.toLocaleString();
};

/**
 * 检查授权是否已彻底过期（基于后端直接给出的数据）
 * @param {Object} serverData 
 * @returns {Boolean}
 */
export const checkIsLicenseExpired = (serverData) => {
    if (!serverData) return false
    
    // 我们已经在后端 C++ 中处理过：过期时 GRACE_REMAINING_DAYS 会是 -1
    if (serverData.GRACE_REMAINING_DAYS !== undefined) {
        return parseInt(serverData.GRACE_REMAINING_DAYS) < 0
    }
    
    // 如果后端明确返回状态为 expired 或 revoked
    const status = serverData.LICENSE_STATUS
    return status === 'expired' || status === 'revoked'
}
