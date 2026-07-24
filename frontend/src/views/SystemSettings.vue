<template>
    <div class="page-container">
        <!-- Header -->
        <div class="header-row">
            <div class="header-left">
                <h2 class="title">{{ $t('settings.title') }}</h2>
                <div class="sub">{{ $t('settings.sub') }}</div>
            </div>
            <!-- Global action removed as per request -->
        </div>

        <!-- Content -->
        <div class="settings-content">
            <el-card shadow="never" class="modern-card">
                <el-form :model="form" label-width="140px" v-loading="loading" label-position="left">
                    
                    <!-- 0. 基本设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Setting /></el-icon>
                                <span>{{ $t('settings.basic_settings') }}</span>
                            </div>
                            <el-button type="primary" size="small" @click="saveSection('SYSTEM')" :loading="saving.SYSTEM">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.timezone')">
                                        <el-select v-model="form.SYSTEM.TIMEZONE" :placeholder="$t('settings.timezone_placeholder')" style="width: 100%">
                                            <el-option
                                                v-for="item in timezoneOptions"
                                                :key="item.value"
                                                :label="item.label"
                                                :value="item.value"
                                            />
                                        </el-select>
                                        <p class="form-hint">{{ $t('settings.timezone_hint') }}</p>
                                    </el-form-item>
                                </el-col>

                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.theme')">
                                        <el-radio-group v-model="themeMode" @change="handleThemeChange" style="width: 100%">
                                            <el-radio-button value="light">{{ $t('settings.theme_light') }}</el-radio-button>
                                            <el-radio-button value="dark">{{ $t('settings.theme_dark') }}</el-radio-button>
                                            <el-radio-button value="auto">{{ $t('settings.theme_auto') }}</el-radio-button>
                                        </el-radio-group>
                                        <div class="form-hint" style="margin-top: 4px; width: 100%;">{{ $t('settings.theme_hint') }}</div>
                                    </el-form-item>
                                </el-col>
                            </el-row>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 1. 系统用户设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><User /></el-icon>
                                <span>{{ $t('settings.system_user') }}</span>
                            </div>
                            <el-button type="primary" size="small" @click="saveSection('SYSTEM_USER')" :loading="saving.SYSTEM_USER">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.username')">
                                        <el-input v-model="form.SYSTEM_USER.USER" :placeholder="$t('settings.username_placeholder')" />
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.password')">
                                        <el-input v-model="form.SYSTEM_USER.PASSWORD" type="password" show-password :placeholder="$t('settings.password_placeholder')" />
                                    </el-form-item>
                                </el-col>
                            </el-row>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 2. JWT 设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Key /></el-icon>
                                <span>{{ $t('settings.jwt_security') }}</span>
                            </div>
                             <el-button type="primary" size="small" @click="saveSection('JWT')" :loading="saving.JWT">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-form-item label="Secret Key">
                                <el-input v-model="form.JWT.JWT_SECRET_KEY" />
                            </el-form-item>
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.token_expires')">
                                        <el-input-number v-model="form.JWT.JWT_ACCESS_TOKEN_EXPIRES_MINUTES" :min="1" controls-position="right" style="width: 50%"/>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.refresh_expires')">
                                        <el-input-number v-model="form.JWT.JWT_REFRESH_TOKEN_EXPIRES" :min="1" controls-position="right" style="width: 50%"/>
                                    </el-form-item>
                                </el-col>
                            </el-row>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 3. 服务器相关设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Connection /></el-icon>
                                <span>{{ $t('settings.server_config') }}</span>
                            </div>
                            <div style="display: flex; gap: 8px;">
                                <el-button 
                                    v-if="form.SERVER.LICENSE_STATUS == 1 || form.SERVER.LICENSE_STATUS === 'active' || form.SERVER.LICENSE_STATUS === 'suspended'"
                                    type="primary" 
                                    size="small" 
                                    plain 
                                    @click="reverifyLicense" 
                                    :loading="reverifying"
                                >
                                    Check In
                                </el-button>
                                <el-button 
                                    v-else
                                    type="warning" 
                                    size="small" 
                                    icon="Key"
                                    @click="openActivateDialog"
                                    :loading="activating"
                                >
                                    {{ $t('settings.activate_device') }}
                                </el-button>
                                <el-button type="primary" size="small" @click="saveSection('SERVER')" :loading="saving.SERVER">{{ $t('common.save') }}</el-button>
                            </div>
                        </div>
                        <div class="section-body">
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.server_url')">
                                        <el-input v-model="form.SERVER.URL" disabled />
                                    </el-form-item>
                                </el-col>

                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.license_id')">
                                        <el-input v-model="form.SERVER.LICENSE_ID" disabled />
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.api_token')">
                                       <el-input v-model="form.SERVER.API_Token" :placeholder="$t('settings.api_token')" />
                                    </el-form-item>
                                </el-col>
                                 <el-col :xs="24" :sm="12">
                                     <el-form-item :label="$t('settings.current_version')">
                                        <div style="display: flex; align-items: center; gap: 8px; width: 100%;">
                                            <el-input v-model="form.SERVER.CURRENT_API_VERSION" disabled />
                                            <el-button 
                                                v-if="hasNewVersion" 
                                                type="danger" 
                                                size="small" 
                                                @click="handleUpgrade"
                                                :loading="upgrading"
                                            >
                                                {{ $t('settings.auto_upgrade') }}
                                            </el-button>
                                        </div>
                                    </el-form-item>
                                 </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.latest_version')">
                                        <el-input v-model="form.SERVER.LASTEST_API_VERSION" disabled :placeholder="$t('settings.latest_none')" />
                                    </el-form-item>
                                </el-col>
                                 <el-col :xs="24" :sm="12">
                                     <el-form-item :label="$t('settings.expiry_date')">
                                         <el-input :model-value="form.SERVER.EXPIRED_DATE ? form.SERVER.EXPIRED_DATE.split(' ')[0] : ''" disabled :placeholder="$t('settings.expiry_forever')" />
                                     </el-form-item>
                                 </el-col>
                                 <el-col :xs="24" :sm="12">
                                     <el-form-item :label="$t('settings.license_status')">
                                         <div class="status-display">
                                             <span :class="getLicenseStatusMeta(form.SERVER.LICENSE_STATUS, checkIsOffline(form.SERVER.LAST_CHECKIN_TIME)).class">
                                                 {{ $t(getLicenseStatusMeta(form.SERVER.LICENSE_STATUS, checkIsOffline(form.SERVER.LAST_CHECKIN_TIME)).label) }}
                                             </span>
                                         </div>
                                     </el-form-item>
                                 </el-col>
                                 <el-col :xs="24" :sm="12">
                                     <el-form-item :label="$t('settings.device_status')">
                                         <div class="status-display">
                                             <span :class="getDeviceStatusMeta(form.SERVER.DEVICE_STATUS).class">
                                                 {{ $t(getDeviceStatusMeta(form.SERVER.DEVICE_STATUS).label) }}
                                             </span>
                                         </div>
                                     </el-form-item>
                                 </el-col>
                            </el-row>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 4. AdGuard 设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Lock /></el-icon>
                                <span>{{ $t('settings.adguard_integration') }}</span>
                            </div>
                             <el-button type="primary" size="small" @click="saveSection('ADGUARD')" :loading="saving.ADGUARD">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-form-item :label="$t('settings.adguard_url')">
                                <el-input v-model="form.ADGUARD.SERVER.URL" />
                            </el-form-item>
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.adguard_user')">
                                        <el-input v-model="form.ADGUARD.SERVER.USER" />
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.adguard_password')">
                                        <el-input v-model="form.ADGUARD.SERVER.PASSWORD" type="password" show-password :placeholder="$t('settings.password_placeholder')" />
                                    </el-form-item>
                                </el-col>
                            </el-row>
                            <el-form-item :label="$t('settings.template_config')" class="mt-2">
                                <el-input 
                                    type="textarea" 
                                    :rows="15"
                                    :model-value="JSON.stringify(form.ADGUARD.TEMPLATE, null, 2)"
                                    disabled
                                    :placeholder="$t('settings.template_readonly')"
                                    class="code-font"
                                />
                            </el-form-item>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 5. 邮件通知设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Message /></el-icon>
                                <span>{{ $t('settings.email_notification') || '邮件通知设置' }}</span>
                            </div>
                            <el-button type="primary" size="small" @click="saveSection('EMAIL')" :loading="saving.EMAIL">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-form-item :label="$t('settings.email_enabled') || '开启通知'">
                                <el-switch v-model="form.EMAIL.ENABLED" />
                            </el-form-item>
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.smtp_server') || 'SMTP服务器'">
                                        <el-input v-model="form.EMAIL.SMTP_SERVER" placeholder="smtp.qq.com" />
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.smtp_port') || 'SMTP端口'">
                                        <el-input-number v-model="form.EMAIL.SMTP_PORT" :min="1" :max="65535" controls-position="right" style="width: 100%"/>
                                    </el-form-item>
                                </el-col>
                            </el-row>
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.email_user') || '发件人邮箱'">
                                        <el-input v-model="form.EMAIL.USER" placeholder="your_email@qq.com" />
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.email_password') || '授权码/密码'">
                                        <el-input v-model="form.EMAIL.PASSWORD" type="password" show-password :placeholder="$t('settings.password_placeholder')" />
                                    </el-form-item>
                                </el-col>
                            </el-row>
                            <el-form-item :label="$t('settings.email_receivers') || '收件人列表'">
                                <el-select 
                                    v-model="form.EMAIL.RECEIVERS" 
                                    multiple 
                                    filterable 
                                    allow-create 
                                    default-first-option 
                                    :placeholder="$t('settings.email_receivers_placeholder') || '输入邮箱并回车'" 
                                    style="width: 100%"
                                >
                                    <el-option v-for="item in form.EMAIL.RECEIVERS" :key="item" :label="item" :value="item" />
                                </el-select>
                                <p class="form-hint">{{ $t('settings.email_receivers_hint') || '支持多个收件人，输入后按回车确认' }}</p>
                            </el-form-item>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 5. 计划任务设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><AlarmClock /></el-icon>
                                <span>{{ $t('settings.scheduler_tasks') }}</span>
                            </div>
                             <el-button type="primary" size="small" @click="saveSection('SCHEDULER')" :loading="saving.SCHEDULER">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.policy_sync_interval')">
                                        <el-input-number v-model="form.SCHEDULER.SYNC_NFT_INTERVAL" :min="30" :max="3600" controls-position="right" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.policy_sync_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.status_update_interval')">
                                        <el-input-number v-model="form.SCHEDULER.DEVICE_UPDATE_INTERVAL" :min="30" :max="600" controls-position="right" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.status_update_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.entertainment_analysis')">
                                        <el-time-picker v-model="form.SCHEDULER.ENTERTAINMENT_COLLECT_TIME" value-format="HH:mm" format="HH:mm" :placeholder="$t('common.select_time')" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.entertainment_analysis_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.adguard_analysis')">
                                        <el-time-picker v-model="form.SCHEDULER.ADGUARD_ANALYSIS_TIME" value-format="HH:mm" format="HH:mm" :placeholder="$t('common.select_time')" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.adguard_analysis_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="12">
                                    <el-form-item :label="$t('settings.cleanup_time')">
                                        <el-time-picker v-model="form.SCHEDULER.CLEANUP_TIME" value-format="HH:mm" format="HH:mm" :placeholder="$t('common.select_time')" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.cleanup_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                            </el-row>
                        </div>
                    </div>

                    <el-divider />

                    <!-- 6. 日志保留设置 -->
                    <div class="section-block">
                        <div class="section-head">
                            <div class="head-title">
                                <el-icon><Calendar /></el-icon>
                                <span>{{ $t('settings.retention_policy') }}</span>
                            </div>
                             <el-button type="primary" size="small" @click="saveSection('RETENTION')" :loading="saving.RETENTION">{{ $t('common.save') }}</el-button>
                        </div>
                        <div class="section-body">
                            <el-alert
                                :title="$t('settings.retention_warning')"
                                type="warning"
                                :closable="false"
                                show-icon
                                class="mb-4"
                            />
                            <el-row :gutter="24">
                                <el-col :xs="24" :sm="8">
                                    <el-form-item :label="$t('settings.retention_adguard_summary')">
                                        <el-input-number v-model="form.RETENTION.ADGUARD_SUMMARY" :min="1" :max="30" controls-position="right" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.retention_adguard_summary_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="8">
                                    <el-form-item :label="$t('settings.retention_adguard_detail')">
                                        <el-input-number v-model="form.RETENTION.ADGUARD_DETAIL" :min="1" :max="10" controls-position="right" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.retention_adguard_detail_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                                <el-col :xs="24" :sm="8">
                                    <el-form-item :label="$t('settings.retention_sys_messages')">
                                        <el-input-number v-model="form.RETENTION.SYS_MESSAGES" :min="1" :max="30" controls-position="right" style="width: 100%"/>
                                        <p class="form-hint">{{ $t('settings.retention_sys_messages_hint') }}</p>
                                    </el-form-item>
                                </el-col>
                            </el-row>
                        </div>
                    </div>

                </el-form>
            </el-card>

            <!-- Activation Dialog -->
            <el-dialog v-model="activateDialogVisible" :title="$t('settings.device_activation')" width="500px">
                <el-form :model="activateForm" label-width="100px">
                    <el-form-item :label="$t('settings.server_url')" required>
                        <el-input v-model="activateForm.server_url" placeholder="https://verify.example.com" />
                    </el-form-item>
                    <el-form-item :label="$t('settings.api_token')" required>
                        <el-input v-model="activateForm.api_token" :placeholder="$t('settings.api_token')" />
                    </el-form-item>
                    <el-form-item :label="$t('settings.activation_code')" required>
                        <el-input v-model="activateForm.activation_code" :placeholder="$t('settings.activation_code_placeholder')" />
                    </el-form-item>
                    <el-form-item :label="$t('settings.device_model')">
                        <el-input v-model="activateForm.model" disabled />
                    </el-form-item>
                    <el-form-item :label="$t('settings.os_version')">
                        <el-input v-model="activateForm.os_version" disabled />
                    </el-form-item>
                    <el-form-item :label="$t('settings.client_version')">
                        <el-input v-model="form.SERVER.CURRENT_API_VERSION" disabled />
                    </el-form-item>
                </el-form>
                <template #footer>
                    <span class="dialog-footer">
                        <el-button @click="activateDialogVisible = false">{{ $t('common.cancel') }}</el-button>
                        <el-button type="primary" @click="submitActivation" :loading="activating">{{ $t('settings.submit_activation') }}</el-button>
                    </span>
                </template>
            </el-dialog>
            <!-- End Dialog -->
        </div>
    </div>
</template>

<script setup>
import { ref, reactive, onMounted, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import http from '@/utils/http'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Check, User, Connection, Key, Lock, AlarmClock, Setting, Calendar, Message } from '@element-plus/icons-vue'
import { getLicenseStatusMeta, getDeviceStatusMeta, checkIsOffline } from '@/utils/common'
import { getThemeMode, applyTheme } from '@/utils/themeManager'

const { t } = useI18n()
const loading = ref(false)
const saving = reactive({
    SYSTEM: false,
    SYSTEM_USER: false,
    SERVER: false,
    JWT: false,
    ADGUARD: false,
    EMAIL: false,
    SCHEDULER: false,
    RETENTION: false
})
const upgrading = ref(false)

const hasNewVersion = computed(() => {
    if (!form.SERVER.CURRENT_API_VERSION || !form.SERVER.LASTEST_API_VERSION) return false
    return form.SERVER.CURRENT_API_VERSION !== form.SERVER.LASTEST_API_VERSION
})

const timezoneOptions = computed(() => [
    // ===== America =====
    { label: `America/New_York (ET)`, value: 'America/New_York' },
    { label: `America/Chicago (CT)`, value: 'America/Chicago' },
    { label: `America/Denver (MT)`, value: 'America/Denver' },
    { label: `America/Los_Angeles (PT)`, value: 'America/Los_Angeles' },
    { label: `America/Phoenix (Arizona)`, value: 'America/Phoenix' },
    { label: `America/Anchorage (Alaska)`, value: 'America/Anchorage' },
    { label: `Pacific/Honolulu (Hawaii)`, value: 'Pacific/Honolulu' },

    // ===== Canada / North America =====
    { label: `America/Toronto`, value: 'America/Toronto' },
    { label: `America/Vancouver`, value: 'America/Vancouver' },
    { label: `America/Mexico_City`, value: 'America/Mexico_City' },

    // ===== South America =====
    { label: `America/Sao_Paulo`, value: 'America/Sao_Paulo' },
    { label: `America/Buenos_Aires`, value: 'America/Buenos_Aires' },
    { label: `America/Santiago`, value: 'America/Santiago' },

    // ===== Europe =====
    { label: `Europe/London (UK)`, value: 'Europe/London' },
    { label: `Europe/Paris (FR)`, value: 'Europe/Paris' },
    { label: `Europe/Berlin (DE)`, value: 'Europe/Berlin' },
    { label: `Europe/Madrid (ES)`, value: 'Europe/Madrid' },
    { label: `Europe/Rome (IT)`, value: 'Europe/Rome' },
    { label: `Europe/Amsterdam (NL)`, value: 'Europe/Amsterdam' },
    { label: `Europe/Zurich (CH)`, value: 'Europe/Zurich' },
    { label: `Europe/Moscow (RU)`, value: 'Europe/Moscow' },

    // ===== East Asia =====
    { label: `Asia/Shanghai (${t('common.beijing_time')})`, value: 'Asia/Shanghai' },
    { label: `Asia/Hong_Kong`, value: 'Asia/Hong_Kong' },
    { label: `Asia/Taipei`, value: 'Asia/Taipei' },
    { label: `Asia/Tokyo`, value: 'Asia/Tokyo' },
    { label: `Asia/Seoul`, value: 'Asia/Seoul' },

    // ===== Southeast Asia =====
    { label: `Asia/Singapore`, value: 'Asia/Singapore' },
    { label: `Asia/Bangkok`, value: 'Asia/Bangkok' },
    { label: `Asia/Kuala_Lumpur`, value: 'Asia/Kuala_Lumpur' },
    { label: `Asia/Jakarta`, value: 'Asia/Jakarta' },
    { label: `Asia/Manila`, value: 'Asia/Manila' },

    // ===== South Asia / Mid East =====
    { label: `Asia/Kolkata`, value: 'Asia/Kolkata' },
    { label: `Asia/Dubai`, value: 'Asia/Dubai' },
    { label: `Asia/Riyadh`, value: 'Asia/Riyadh' },
    { label: `Asia/Jerusalem`, value: 'Asia/Jerusalem' },

    // ===== Africa =====
    { label: `Africa/Cairo`, value: 'Africa/Cairo' },
    { label: `Africa/Johannesburg`, value: 'Africa/Johannesburg' },
    { label: `Africa/Nairobi`, value: 'Africa/Nairobi' },

    // ===== Oceania =====
    { label: `Australia/Sydney`, value: 'Australia/Sydney' },
    { label: `Australia/Melbourne`, value: 'Australia/Melbourne' },
    { label: `Australia/Perth`, value: 'Australia/Perth' },
    { label: `Pacific/Auckland`, value: 'Pacific/Auckland' },

    // ===== General =====
    { label: `UTC`, value: 'UTC' }
])

const form = reactive({
    SYSTEM: { TIMEZONE: 'US/Eastern', THEME: 'auto' },
    SYSTEM_USER: { USER: '', PASSWORD: '' },
    SERVER: { URL: '', API_Token: '', LICENSE_ID: '', CURRENT_API_VERSION: '', LASTEST_API_VERSION: '', EXPIRED_DATE: '', LICENSE_STATUS: 'inactive', DEVICE_STATUS: 'unknown' },
    JWT: { JWT_SECRET_KEY: '', JWT_ACCESS_TOKEN_EXPIRES_MINUTES: 30, JWT_REFRESH_TOKEN_EXPIRES: 7 },
    ADGUARD: { 
        SERVER: { URL: '', USER: '', PASSWORD: '' },
        TEMPLATE: {}
    },
    EMAIL: { 
        ENABLED: false, 
        SMTP_SERVER: '', 
        SMTP_PORT: 465, 
        USE_SSL: true, 
        USER: '', 
        PASSWORD: '', 
        RECEIVERS: [] 
    },
    SCHEDULER: {
        SYNC_NFT_INTERVAL: 60,
        DEVICE_UPDATE_INTERVAL: 30,
        ADGUARD_ANALYSIS_TIME: '00:05',
        ENTERTAINMENT_COLLECT_TIME: '00:10',
        CLEANUP_TIME: '00:20'
    },
    RETENTION: {
        ADGUARD_SUMMARY: 30,
        ADGUARD_DETAIL: 5,
        SYS_MESSAGES: 15
    }
})

// Theme Logic
const themeMode = ref(getThemeMode())
const handleThemeChange = async (val) => {
    applyTheme(val)
    form.SYSTEM.THEME = val
    await saveSection('SYSTEM')
}

// Activation Logic
const activateDialogVisible = ref(false)
const activating = ref(false)
const systemInfo = reactive({
    model: '',
    os_version: '',
    device_fingerprint: ''
})
const activateForm = reactive({
    server_url: '',
    api_token: '',
    activation_code: '',
    model: '',
    os_version: ''
})

const openActivateDialog = () => {
    // Pre-fill with current settings
    activateForm.server_url = form.SERVER.URL
    activateForm.api_token = form.SERVER.API_Token
    // Pre-fill system info for submission
    activateForm.model = systemInfo.model
    activateForm.os_version = systemInfo.os_version
    activateDialogVisible.value = true
}

const submitActivation = async () => {
    if (!activateForm.server_url || !activateForm.api_token || !activateForm.activation_code) {
        ElMessage.warning(t('settings.fill_full_info'))
        return
    }
    
    activating.value = true
    try {
        const res = await http.post('/api/activate', {
            server_url: activateForm.server_url,
            api_token: activateForm.api_token,
            activation_code: activateForm.activation_code,
            model: activateForm.model,
            os_version: activateForm.os_version
        })
        
        if (res.ok) {
            ElMessage.success(t('settings.activation_success'))
            activateDialogVisible.value = false
            // 刷新页面以同步全局状态（如侧边栏状态）
            setTimeout(() => {
                window.location.reload()
            }, 1000)
        } else {
             ElMessage.error(res.msg || t('settings.activation_failed'))
        }
    } catch (e) {
        ElMessage.error(t('settings.network_error'))
    } finally {
        activating.value = false
    }
}

const fetchData = async () => {
    loading.value = true
    try {
        const res = await http.get('/api/settings')
        if (res.ok && res.data) {
            // Merge data into form
            if (res.data.SYSTEM) {
                Object.assign(form.SYSTEM, res.data.SYSTEM)
                // Sync local theme manager with backend setting
                if (res.data.SYSTEM.THEME) {
                    themeMode.value = res.data.SYSTEM.THEME
                    applyTheme(res.data.SYSTEM.THEME)
                }
            }
            Object.assign(form.SYSTEM_USER, res.data.SYSTEM_USER || {})
            Object.assign(form.SERVER, res.data.SERVER || {})
            Object.assign(form.JWT, res.data.JWT || {})
            Object.assign(form.SCHEDULER, res.data.SCHEDULER || {})
            if (res.data.ADGUARD) {
                Object.assign(form.ADGUARD.SERVER, res.data.ADGUARD.SERVER || {})
                form.ADGUARD.TEMPLATE = res.data.ADGUARD.TEMPLATE || {}
            }
            if (res.data.EMAIL) {
                Object.assign(form.EMAIL, res.data.EMAIL)
            }
            if (res.data.RETENTION) {
                Object.assign(form.RETENTION, res.data.RETENTION)
            }
            
            // Populate System Info
            if (res.data.SYSTEM_INFO) {
                Object.assign(systemInfo, res.data.SYSTEM_INFO)
            }
            
            // UX优化：清除后端返回的 "******" 掩码，避免用户点击"显示密码"时困惑
            // 如果后端返回了掩码，说明已设置密码，前端显示为空，placeholder提示
            form.SYSTEM_USER.PASSWORD = ''
            if (form.ADGUARD.SERVER.PASSWORD === '******') {
                form.ADGUARD.SERVER.PASSWORD = ''
            }
            if (form.EMAIL.PASSWORD === '******') {
                form.EMAIL.PASSWORD = ''
            }
        } else {
             ElMessage.error(res.msg || t('settings.fetch_failed'))
        }
    } catch (e) {
        ElMessage.error(t('settings.network_error'))
        console.error(e)
    } finally {
        loading.value = false
    }
}

onMounted(() => {
    fetchData()
})

const saveSection = async (section) => {
    if (!section || !form[section]) return
    
    saving[section] = true
    try {
        // Construct payload with only the specific section
        const payload = {}
        payload[section] = JSON.parse(JSON.stringify(form[section]))
        
        // For AdGuard and Email, we need to wrap it properly since form structure matches backend but partial update needs careful handling
        if (section === 'ADGUARD' || section === 'EMAIL') {
             // Backend expects { "ADGUARD": { "SERVER": ... } } or { "EMAIL": { ... } }
        }

        const res = await http.post('/api/settings', payload)
        if (res.ok) {
            ElMessage.success(t('settings.save_success'))
        } else {
            ElMessage.error(res.msg || t('settings.save_failed'))
        }
    } catch (e) {
        ElMessage.error(t('settings.network_error'))
    } finally {
        saving[section] = false
    }
}

const reverifying = ref(false)
const reverifyLicense = async () => {
    reverifying.value = true
    try {
        const res = await http.post('/api/reverify_license')
        if (res.ok) {
            ElMessage.success(t('settings.reverify_success'))
            // 刷新页面以同步全局状态
            setTimeout(() => {
                window.location.reload()
            }, 1000)
        } else {
             ElMessage.error(res.msg || t('settings.reverify_failed'))
        }
    } catch (e) {
        ElMessage.error(t('settings.network_error'))
    } finally {
        reverifying.value = false
    }
}

const handleUpgrade = async () => {
    try {
        await ElMessageBox.confirm(
            t('settings.upgrade_confirm_msg'),
            t('settings.upgrade_confirm_title'),
            {
                confirmButtonText: t('common.confirm'),
                cancelButtonText: t('common.cancel'),
                type: 'warning'
            }
        )
        
        upgrading.value = true
        const res = await http.post('/api/settings/upgrade')
        if (res.ok) {
            ElMessage.success(t('settings.upgrade_triggered_msg'))
            // Optional: Redirect to message center or show a persistent notification
        } else {
            ElMessage.error(res.msg || t('settings.upgrade_failed'))
        }
    } catch (e) {
        if (e !== 'cancel') {
            ElMessage.error(t('settings.network_error'))
        }
    } finally {
        upgrading.value = false
    }
}
</script>

<style scoped>
/* Page container style handles the outer padding/layout */

.modern-card {
    border-radius: 14px;
    border: 1px solid #eef0f5;
    background: #fff;
    /* content padding handled by el-card__body default or override */
}

.status-display {
    height: 32px;
    display: flex;
    align-items: center;
}

.status-display span {
    font-size: 13px;
    padding: 2px 10px;
    border-radius: 6px;
    font-weight: 500;
}

/* 状态样式复用 SideMenu 的定义 */
.status-active {
    background-color: #f0f9eb;
    color: #67c23a;
    border: 1px solid #e1f3d8;
}

.status-warning {
    background-color: #fdf6ec;
    color: #e6a23c;
    border: 1px solid #faecd8;
}

.status-inactive {
    background-color: #fef0f0;
    color: #f56c6c;
    border: 1px solid #fde2e2;
}

.status-expired {
    background-color: #fdf6ec;
    color: #e6a23c;
    border: 1px solid #faecd8;
}

.status-error {
    background-color: #fef0f0;
    color: #f56c6c;
    border: 1px solid #fde2e2;
}

.status-info {
    background-color: #f4f4f5;
    color: #909399;
    border: 1px solid #e9e9eb;
}

:deep(.el-card__body) {
    padding: 24px 32px;
}

@media (max-width: 768px) {
    :deep(.el-card__body) {
        padding: 16px;
    }
}

.section-block {
    margin-bottom: 8px;
}

.section-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    font-size: 15px;
    font-weight: 700;
    color: #1e293b;
    margin-bottom: 20px;
    padding-left: 4px;
    padding-right: 4px;
}

.head-title {
    display: flex;
    align-items: center;
    gap: 10px;
}

.section-head .el-icon {
    font-size: 18px;
    color: #3b82f6; 
}

.section-body {
    padding-left: 4px;
}

.mt-2 {
    margin-top: 12px;
}

.code-font :deep(.el-textarea__inner) {
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    font-size: 12px;
    color: #64748b;
    background-color: #f8fafc;
}
</style>
