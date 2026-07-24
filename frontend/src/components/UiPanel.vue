<template>
    <div class="panel">
        <div v-if="$slots.head" class="panel-head">
            <slot name="head" />
        </div>
        <div class="panel-body">
            <slot />
        </div>
    </div>
</template>

<script setup></script>
