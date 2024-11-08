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

