<!-- Telemetry.svelte -->
<script>
    import { getStageName } from "../scripts/messageHandler.js"
    export let data;

    $: if (data) {
        // console.log("Полученные данные для отображения в Telemetry:", data); // Проверка данных для отображения
    } else {
        console.error(" Данных для отображения в Telemetry нет"); // Проверка данных для отображения
    }
</script>

<div class="telemetry-container">
    <h3>Данные телеметрии</h3>
    <div>{getStageName(data.type)}</div>
    <pre>{JSON.stringify(data, null, 2)}</pre>
</div>

<style>
    .telemetry-container {
        font-family: Arial, sans-serif;
        background-color: #f5f5f5;
        padding: 1.5em;
        border-radius: 8px;
        max-width: 600px;
        margin: 2em auto;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
    h3 {
        color: #333;
        text-align: center;
    }
    .json-block {
        padding-left: 1em;
    }
    .json-line {
        margin: 4px 0;
    }
    .key {
        color: #d35400;
        font-weight: bold;
    }
    .number {
        color: #2980b9;
    }
    .string {
        color: #27ae60;
    }
    .boolean {
        color: #8e44ad;
    }
    .null {
        color: #95a5a6;
    }
</style>
