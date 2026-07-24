<template>
    <div class="card-ui">
        <div class="header-ui">
            <div class="title-block">
                <h2 class="title">{{ title }}</h2>
                <div v-if="subtitle" class="subtitle">{{ subtitle }}</div>
            </div>

            <div class="actions-ui">
                <slot name="actions" />
            </div>
        </div>

        <div class="header-divider"></div>

        <div class="body-ui">
            <slot />
        </div>
    </div>
</template>

<script setup>
    defineProps({
        title: { type: String, required: true },
        subtitle: { type: String, default: '' }
    })
</script>
