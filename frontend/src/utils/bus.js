import { reactive } from 'vue'

class Bus {
    constructor() {
        this.list = {}
    }

    emit(name, ...args) {
        const eventName = name.toString()
        this.list[eventName] && this.list[eventName].forEach(fn => fn(...args))
    }

    on(name, fn) {
        const eventName = name.toString()
        this.list[eventName] = this.list[eventName] || []
        this.list[eventName].push(fn)
    }

    off(name, fn) {
        const eventName = name.toString()
        if (this.list[eventName]) {
            const index = this.list[eventName].indexOf(fn)
            if (index > -1) {
                this.list[eventName].splice(index, 1)
            }
        }
    }
}

export default new Bus()
