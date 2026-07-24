<template>
    <div class="login-wrap">
        <div class="lang-switch">
            <span :class="['lang-item', { 'is-active': locale === 'zh' }]" @click="setLanguage('zh')">中文</span>
            <span class="lang-divider">/</span>
            <span :class="['lang-item', { 'is-active': locale === 'en' }]" @click="setLanguage('en')">EN</span>
        </div>
        <el-card class="login-card" shadow="hover">
            <div class="login-header">
                <img :src="logoImg" class="login-logo" alt="HomeNetGuard Logo" />
                <h2 class="title">{{ $t('login.title') }}</h2>
            </div>

            <el-form :model="form" @submit.prevent="handleLogin">
                <!-- 用户名 -->
                <el-form-item>
                    <el-input v-model="form.username" :placeholder="$t('login.username')">
                        <template #prefix>
                            <el-icon><User /></el-icon>
                        </template>
                    </el-input>
                </el-form-item>

                <!-- 密码 -->
                <el-form-item>
                    <el-input v-model="form.password" type="password" :placeholder="$t('login.password')" show-password>
                        <template #prefix>
                            <el-icon><Lock /></el-icon>
                        </template>
                    </el-input>
                </el-form-item>

                <!-- 验证码 -->
                <el-form-item>
                    <el-input v-model="form.captcha" :placeholder="$t('login.captcha')">
                        <template #prefix>
                            <el-icon><Picture /></el-icon>
                        </template>
                        <template #append>
                            <img
                                :src="captchaImg"
                                @click="refreshCaptcha"
                                class="captcha-img"
                                alt="captcha"
                                :title="$t('login.captcha_tip')"
                            />
                        </template>
                    </el-input>
                </el-form-item>

                <!-- 登录按钮 -->
                <el-form-item>
                    <el-button type="primary" native-type="submit" :loading="loading">
                        {{ $t('login.login_btn') }}
                    </el-button>
                </el-form-item>
                <!-- 错误提示 -->
                <div class="error" v-if="error">{{ error }}</div>
            </el-form>
        </el-card>
    </div>
</template>

<script setup>
    import { ref, onMounted } from 'vue'
    import { useI18n } from 'vue-i18n'
    import { useRouter } from 'vue-router'
    import { getCaptcha, login } from '@/api/auth'
    import logoImg from '@/assets/logo.png'
    import { isNavigationFailure } from 'vue-router'
    import { User, Lock, Picture } from '@element-plus/icons-vue'

    const router = useRouter()
    const { t, locale } = useI18n()

    const setLanguage = (lang) => {
        locale.value = lang
        localStorage.setItem('language', lang)
    }
    const form = ref({ username: '', password: '', captcha: '' })
    const error = ref('')
    const captchaImg = ref('')
    const captchaId = ref('')
    const loading = ref(false)

    const refreshCaptcha = async() => {
        try {
            const res = await getCaptcha()
            if (res.ok) {
                captchaImg.value = res.captcha_img
                captchaId.value = res.captcha_id // 保存这个 ID
            }
        } catch (e) {
            console.error(t('login.loading_captcha_failed'), e)
        }
    }

    const handleLogin = async () => {
        loading.value = true;
        try {
            const res = await login({
                ...form.value,
                captcha_id: captchaId.value
            });

            // 打印一下，看看前端收到的 res 对象长什么样
            if (res.ok) { // 假设后端返回 access_token 和 refresh_token
                // 1. 存储 Token
                localStorage.setItem('access_token', res.access_token);
                localStorage.setItem('refresh_token', res.refresh_token);
                localStorage.setItem('username', res.username || form.value.username);
                await router.push('/');
                // router.push('/');
            } else {
                error.value = res?.msg || t('login.error_default');
                refreshCaptcha()
            }
        } catch (e) {
            error.value = e.message || t('login.error_network');
            console.error('登陆过程出错', e)
            refreshCaptcha()
        } finally {
            loading.value = false;
        }
    }

    onMounted(refreshCaptcha)

    // const login = async () => {
    //     try {
    //         const res = await http.post('/api/login', form.value)
    //         if (res?.ok) {
    //             // 重点：确保从正确的位置取 username
    //             const name = res.username || form.value.username
    //             localStorage.setItem('username', name)
    //
    //             // 兜底：如果仍停留在 /login，强制跳
    //             if (router.currentRoute.value.path === '/login') {
    //                 window.location.replace('/')
    //             }
    //             return
    //         }
    //         error.value = res?.msg || '登录失败'
    //         refreshCaptcha()
    //     } catch (e) {
    //         error.value = '网络或服务器错误'
    //         refreshCaptcha()
    //     }
    // }
</script>

<style scoped>
    .login-wrap {
        display: flex;
        justify-content: center;
        align-items: center;
        min-height: 100vh;
        background-color: #0f172a;
        background-image: radial-gradient(circle at 50% -20%, #1e293b, #0f172a);
        padding: 10px;
        position: relative;
        font-family: 'Inter', sans-serif;
    }

    .login-card {
        width: 100%;
        max-width: 400px;
        border-radius: 24px;
        background: #1e293b !important; /* Solid background to fight layout.css !important */
        border: 1px solid rgba(255, 255, 255, 0.1) !important;
        box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
        box-sizing: border-box;
        position: relative;
        z-index: 5;
    }

    :deep(.el-card__body) {
        padding: 40px 30px !important;
    }

    .login-header {
        display: flex;
        flex-direction: column;
        align-items: center;
        margin-bottom: 32px;
    }

    .login-logo {
        width: 72px;
        height: 72px;
        margin-bottom: 16px;
        object-fit: contain;
        filter: drop-shadow(0 0 10px rgba(59, 130, 246, 0.5));
    }

    .title {
        text-align: center;
        font-weight: 800;
        font-size: 24px;
        color: #fff;
        margin: 0;
        font-family: 'Outfit', sans-serif;
        background: linear-gradient(to right, #fff, #3b82f6);
        -webkit-background-clip: text;
        background-clip: text;
        -webkit-text-fill-color: transparent;
    }

    .lang-switch {
        position: absolute;
        top: 30px;
        right: 30px;
        color: #fff;
        font-size: 14px;
        background: rgba(255, 255, 255, 0.1);
        padding: 6px 14px;
        border-radius: 12px;
        backdrop-filter: blur(8px);
        border: 1px solid rgba(255, 255, 255, 0.1);
        z-index: 10;
    }

    .lang-item {
        cursor: pointer;
        opacity: 0.6;
        transition: all 0.3s;
    }

    .lang-item:hover, .lang-item.is-active {
        opacity: 1;
        font-weight: bold;
        color: #3b82f6;
    }

    .lang-divider {
        margin: 0 10px;
        opacity: 0.3;
    }

    /* Element Plus Overrides */
    :deep(.el-input__wrapper) {
        background-color: rgba(255, 255, 255, 0.05) !important;
        box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.1) inset !important;
        border-radius: 12px;
    }

    :deep(.el-input__inner) {
        color: #fff !important;
        caret-color: #3b82f6 !important; /* Force explicit blue cursor */
        cursor: text !important;
    }

    :deep(.el-input__prefix-icon) {
        color: #3b82f6;
    }

    :deep(.el-button--primary) {
        width: 100%;
        height: 48px;
        border-radius: 12px;
        background-color: #3b82f6;
        border: none;
        font-weight: 700;
        font-size: 16px;
        margin-top: 10px;
        box-shadow: 0 10px 15px -3px rgba(59, 130, 246, 0.4);
        transition: all 0.3s;
    }

    :deep(.el-button--primary:hover) {
        background-color: #2563eb;
        transform: translateY(-2px);
    }

    .captcha-img {
        display: block;
        height: 32px;
        width: 90px;
        cursor: pointer;
        border-radius: 8px;
        object-fit: contain;
        transition: transform 0.2s;
    }

    .captcha-img:hover {
        transform: scale(1.05);
    }

    :deep(.el-input-group__append) {
        padding: 0 8px;
        background-color: transparent;
        border: none;
        box-shadow: none;
    }

    .error {
        color: #fb7185;
        text-align: center;
        margin-top: 16px;
        font-size: 14px;
        font-weight: 600;
    }

    @media (max-width: 480px) {
        .login-card {
            padding: 30px 20px;
        }
        .title {
            font-size: 20px;
        }
    }
</style>
