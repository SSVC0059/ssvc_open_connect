<script>
    import { onMount } from 'svelte';
    let data = { mmhg: '', tp1: '', tp2: '', relay: '', signal: '' };
    let error = null;

    onMount(() => {
        // const eventSource = new EventSource('http://127.0.0.1:3000/events');
        const eventSource = new EventSource('/events');
        eventSource.onmessage = (event) => {
            try {
                const parsedData = JSON.parse(event.data);
                console.log("Получены данные:", parsedData); // Для отладки
                if (parsedData.type === 'waiting' && parsedData.common) {
                    data = {
                        mmhg: parsedData.common.mmhg,
                        tp1: parsedData.common.tp1,
                        tp2: parsedData.common.tp2,
                        relay: parsedData.common.relay,
                        signal: parsedData.common.signal
                    };
                }
            } catch (err) {
                console.error("Ошибка при разборе данных:", err);
                error = "Ошибка при разборе данных.";
            }
        };

        eventSource.onerror = (err) => {
            console.error("Ошибка подключения к SSE:", err);
            error = "Ошибка подключения к SSE.";
        };

        return () => {
            eventSource.close();
        };
    });
</script>

<style>
    .data-container {
        padding: 1rem;
        font-family: Arial, sans-serif;
    }

    .error {
        color: red;
        font-weight: bold;
    }
</style>

<div class="data-container">
    <h1>Текущие данные</h1>
    {#if error}
        <p class="error">{error}</p>
    {:else}
        <p>Давление (mmHg): {data.mmhg}</p>
        <p>Температура 1 (tp1): {data.tp1}</p>
        <p>Температура 2 (tp2): {data.tp2}</p>
        <p>Реле: {data.relay}</p>
        <p>Сигнал: {data.signal}</p>
    {/if}
</div>
