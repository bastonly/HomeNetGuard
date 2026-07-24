<template>
  <div style="padding:20px">
    <h2>家庭网络管理系统</h2>
    <el-card style="margin-top:20px">
      <p>总设备数: {{ devices.length }}</p>
      <p>在线设备: {{ onlineCount }}</p>
      <p>离线设备: {{ offlineCount }}</p>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted, computed } from 'vue';
import { listDevices } from '@/api/device';

const devices = ref([]);

const fetchDevices = async () => {
  const res = await listDevices();
  devices.value = Array.isArray(res) ? res : (res.data || []);
}

const onlineCount = computed(() => devices.value.filter(d => d.status === '在线').length);
const offlineCount = computed(() => devices.value.filter(d => d.status !== '在线').length);

onMounted(fetchDevices);
</script>

