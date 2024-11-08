<!-- DataDisplay.svelte -->
<script>
    import { createEventDispatcher, onMount } from "svelte";

    const dispatch = createEventDispatcher();

    // Хранение полученных данных
    const url = 'http://127.0.0.1:3000/events'; // URL вашего EventSource

    onMount(() => {
        const eventSource = new EventSource(url);

        eventSource.onmessage = (event) => {
            const parsedData = JSON.parse(event.data);
            dispatch("dataReceived", parsedData);
        };

        return () => eventSource.close();
    });
</script>

<!--&lt;!&ndash; Отображение данных в виде JSON &ndash;&gt;-->
<!--<div>-->
<!--    <h3>Полученные данные:</h3>-->
<!--    <pre>{JSON.stringify($data, null, 2)}</pre>-->
<!--</div>-->

<!--<style>-->
<!--    /* Дополнительное стилирование */-->
<!--    div {-->
<!--        font-family: Arial, sans-serif;-->
<!--        padding: 1em;-->
<!--        border: 1px solid #ddd;-->
<!--        background-color: #f9f9f9;-->
<!--        border-radius: 4px;-->
<!--    }-->
<!--    pre {-->
<!--        overflow-x: auto;-->
<!--        white-space: pre-wrap;-->
<!--        word-wrap: break-word;-->
<!--    }-->
<!--</style>-->
