import { createApp } from 'vue'
import App from './App.vue'
import router from './router'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import 'element-plus/theme-chalk/dark/css-vars.css'
import './style.css'
import './assets/styles/layout.css'
import './assets/styles/theme.css'
import './assets/styles/components.css'
import './assets/styles/overrides.css'
import { initTheme } from './utils/themeManager'

import i18n from './i18n'

const app = createApp(App)
app.use(router)
app.use(i18n)
app.use(ElementPlus)

// Initialize Theme
initTheme()

app.mount('#app')

