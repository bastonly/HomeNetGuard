<template>
    <div class="layout">
        <!-- 桌面侧边栏 -->
        <aside class="sidebar" v-show="!isMobile">
            <SideMenu />
        </aside>
        <!-- 手机顶部栏 -->
        <header class="topbar" v-show="isMobile">
            <el-button
                class="menu-btn"
                text
                @click="drawer = true"
            >
                <el-icon class="menu-icon">
                    <Menu />
                </el-icon>
            </el-button>
            <span class="title">
                {{ $t('system.title') }}
              </span>
        </header>
        <!-- 手机抽屉菜单 -->
        <el-drawer v-model="drawer" direction="ltr" :with-header="false" size="320" class="pc-drawer">
            <SideMenu @select="drawer=false"/>
        </el-drawer>

        <!-- 内容 -->
        <main class="content">
            <router-view />
        </main>
    </div>
</template>

<script setup>
    import { ref, onMounted, onUnmounted } from 'vue'
    import SideMenu from '@/layout/SideMenu.vue'
    import { Menu } from '@element-plus/icons-vue'

    const isMobile = ref(window.innerWidth < 768)
    const drawer = ref(false)
    const onResize = () => isMobile.value = window.innerWidth < 768
    onMounted(() => window.addEventListener('resize', onResize))
    onUnmounted(() => window.removeEventListener('resize', onResize))
</script>

<style scoped>
    .layout {
        height: 100vh;
        overflow: hidden;
        display: flex;
        background: var(--bg-page);
    }
    .sidebar {
        width: 200px;
        background: var(--bg-page);
        border-right: 1px solid var(--border-light);
    }

    /* 控制 Drawer 宽度：手机 50% 屏 */
    :deep(.pc-drawer .el-drawer) {
        width: 50vw !important;
        background-color: var(--bg-page) !important; /* Force drawer background */
    }

    /* Drawer 内容区域 padding 收紧一点 */
    :deep(.pc-drawer .el-drawer__body) {
        padding: 0 !important;
        background-color: var(--bg-page) !important; /* Force body background */
    }

    /* 顶部栏整体 */
    .topbar {
        height: 56px;
        display: flex;
        align-items: center;
        padding: 0 12px;
        background: var(--bg-card);
        border-bottom: 1px solid var(--border-light);
        position: relative; /* Default */
    }

    /* 菜单按钮 */
    .menu-btn {
        padding: 10px;                /* ⬅️ 扩大可点击区域 */
        margin-right: 8px;
    }

    .menu-btn:active {
        background-color: rgba(0,0,0,0.05);
        border-radius: 8px;
    }

    /* 菜单图标 */
    .menu-icon {
        font-size: 26px;              /* ⬅️ 图标变大 */
        font-weight: bold;
        color: var(--text-primary);
    }

    /* 标题 */
    .title {
        font-size: 16px;
        font-weight: 700;
        color: var(--text-primary);
        white-space: nowrap;
    }

    .content {
        flex: 1;
        /*padding: 12px;*/
        overflow-y: auto;
        overflow-x: hidden;
    }
    @media (max-width: 768px) {
        .layout {
            flex-direction: column;
            height: auto;       /* Allow natural height */
            min-height: 100vh;
            overflow: visible;  /* Allow natural scroll */
        }
        .topbar {
            position: sticky;
            top: 0;
            z-index: 1000;
            width: 100%;
            box-sizing: border-box;
        }
        .content {
            overflow: visible !important;  /* Don't trap scroll */
            height: auto;
            flex: none;         /* Don't grow in static layout */
        }
    }
</style>
