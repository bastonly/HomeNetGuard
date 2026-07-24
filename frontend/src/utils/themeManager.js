/**
 * Theme Manager for HomeNetGuard
 * Handles Light, Dark, and Auto (Follow System) modes.
 */

const THEME_KEY = 'homenetguard_theme_mode'; // 'light' | 'dark' | 'auto'

export const initTheme = () => {
    const savedTheme = localStorage.getItem(THEME_KEY) || 'auto';
    applyTheme(savedTheme);

    // Listen for system changes if in auto mode
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {
        if (localStorage.getItem(THEME_KEY) === 'auto' || !localStorage.getItem(THEME_KEY)) {
            applyTheme('auto');
        }
    });
};

export const applyTheme = (mode) => {
    const html = document.documentElement;
    let isDark = false;

    if (mode === 'auto') {
        isDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
    } else {
        isDark = mode === 'dark';
    }

    if (isDark) {
        html.classList.add('dark');
    } else {
        html.classList.remove('dark');
    }

    // Also update localStorage
    localStorage.setItem(THEME_KEY, mode);
};

export const getThemeMode = () => {
    return localStorage.getItem(THEME_KEY) || 'auto';
};
