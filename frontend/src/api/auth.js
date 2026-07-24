import http from '@/utils/http'

/**
 * 获取验证码
 */
export const getCaptcha = () => http.get('/api/captcha')

/**
 * 登录
 */
export const login = (data) => http.post('/api/login', data)

/**
 * 检查会话是否有效
 */
export const checkSession = () => http.get('/api/session')

/**
 * 退出登录
 */
export const logout = () => http.post('/api/logout')

/**
 * 刷新 Token
 */
export const refreshToken = () => http.post('/api/refresh')
