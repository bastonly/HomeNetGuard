import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import path from 'path'

export default defineConfig({
  plugins: [vue()],
  base: '/',
  resolve: {
    alias: { '@': path.resolve(__dirname, 'src') }
  },
  server: {
    proxy: {
      '/api': {
        target: 'http://192.168.0.8:5000',
        // target: 'http://96.28.83.79:1974',
        changeOrigin: true
      }
    }
  }
})
