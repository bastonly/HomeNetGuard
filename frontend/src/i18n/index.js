import { createI18n } from 'vue-i18n'
import axios from 'axios'
import zhLocal from './locales/zh.json'
import enLocal from './locales/en.json'

// 获取本地存储的语言，默认为中文
const savedLanguage = localStorage.getItem('language') || 'zh'

const i18n = createI18n({
    legacy: false,
    locale: savedLanguage,
    fallbackLocale: 'zh',
    messages: {
        zh: zhLocal,
        en: enLocal
    }
})

// Track loaded languages to prevent redundant API calls
const loadedLanguages = new Set()

/**
 * Loads translations from Redis via API and updates i18n instance.
 */
export async function loadExternalTranslations(lang) {
    if (loadedLanguages.has(lang)) return
    
    try {
        const response = await axios.get(`/api/i18n/${lang}`)
        if (response.data && response.data.ok) {
            i18n.global.setLocaleMessage(lang, response.data.data)
            loadedLanguages.add(lang)
            console.log(`[i18n] Successfully loaded ${lang} from Redis`)
        }
    } catch (error) {
        console.warn(`[i18n] Failed to load ${lang} from Redis, using local fallback.`, error)
    }
}

// Initial load for the active language
loadExternalTranslations(savedLanguage)

export default i18n
